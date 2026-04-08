#include "relaygame/game_world_runtime.h"

#include "relaycore/app/engine_services.h"
#include "relaycore/core/log.h"
#include "relaycore/core/profiling.h"
#include "relaycore/render/renderer.h"
#include "relaygame/player_controller_config.h"

#include <algorithm>
#include <cmath>

namespace RelayGame
{
    void GameWorldRuntime::startup(RelayCore::EngineServices& services)
    {
        RELAYCORE_PROFILE_ZONE_N("GameWorldRuntime::startup");

        LevelSession session = LevelSessionBuilder{services.config().gameplay}.build();
        const glm::vec3 spawn_position = session.spawn_position;
        const auto spawn_view_angles = session.spawn_view_angles;
        level_ = std::move(session.level);
        doors_ = std::move(session.doors);
        triggers_ = std::move(session.triggers);
        usables_ = std::move(session.usables);
        inventory_item_ids_.clear();
        activated_target_ids_.clear();
        mission_runtime_.reset(RelayCore::gameplay::MissionBuilder{level_}.build());
        trigger_spatial_index_ = std::move(session.trigger_spatial_index);
        usable_spatial_index_ = std::move(session.usable_spatial_index);
        interaction_state_.use_binding_label = services.config().gameplay.controls.use.empty()
                                                   ? std::string{"E"}
                                                   : services.config().gameplay.controls.use;
        configure_audio_cues(services);
        if (const RelayCore::gameplay::MissionStatus mission = mission_runtime_.status();
            mission.active && !mission.completed)
        {
            push_mission_status_event(mission, RelayCore::render::HudEventStyle::Neutral, 2.8F);
        }
        sync_door_audio_state();
        player_controller_.set_config(build_player_controller_config(services.config()));
        player_controller_.initialize(spawn_position, spawn_view_angles);
        player_controller_.set_static_collision_world(level_);
        player_controller_.enable_capture(services.window());
        simulation_loop_.reset();
        player_controller_.apply_camera(services.camera());
        if (!audio_cues_.ambient_loop.empty())
        {
            services.audio_system().play_loop(audio_cues_.ambient_loop);
        }
    }

    void GameWorldRuntime::shutdown(RelayCore::EngineServices& services) noexcept
    {
        services.audio_system().stop_loop();
    }

    void GameWorldRuntime::on_event(const SDL_Event& event, RelayCore::EngineServices& services)
    {
        player_controller_.on_event(event, services.window(), services);
    }

    void GameWorldRuntime::update(const RelayCore::FrameInfo& frame, RelayCore::EngineServices& services)
    {
        RELAYCORE_PROFILE_ZONE_N("GameWorldRuntime::update");

        interaction_state_.use_pressed = false;
        interaction_state_.fired_triggers = 0;
        player_controller_.flush_pending_look();
        tick_transient_hud_event(static_cast<float>(frame.delta_seconds));
        tick_usable_feedback(static_cast<float>(frame.delta_seconds));

        (void) simulation_loop_.advance(frame, [this, &services](const RelayCore::FrameInfo& sim_frame) {
            simulate_fixed_step(sim_frame, services);
        });

        refresh_runtime_usables_and_focus();
        interaction_state_.active_doors =
            static_cast<std::uint32_t>(std::count_if(doors_.begin(), doors_.end(), [](const auto& door) {
                return RelayCore::gameplay::is_door_active(door);
            }));
        player_controller_.apply_camera(services.camera());
    }

    void GameWorldRuntime::publish_diagnostics(RelayCore::Renderer& renderer) const
    {
        renderer.set_player_diagnostics(build_player_diagnostics());
        renderer.set_interaction_diagnostics(build_interaction_diagnostics());
    }

    auto GameWorldRuntime::level() const noexcept -> const RelayCore::BlockoutLevel&
    {
        return level_;
    }

    auto GameWorldRuntime::doors() const noexcept -> const std::vector<RelayCore::gameplay::RuntimeDoor>&
    {
        return doors_;
    }

    auto GameWorldRuntime::triggers() const noexcept -> const std::vector<RelayCore::gameplay::RuntimeTrigger>&
    {
        return triggers_;
    }

    auto GameWorldRuntime::usables() const noexcept -> const std::vector<RelayCore::gameplay::RuntimeUsable>&
    {
        return usables_;
    }

    auto GameWorldRuntime::focused_usable_index() const noexcept -> int
    {
        return interaction_state_.focused_usable_index;
    }

    void
    GameWorldRuntime::simulate_fixed_step(const RelayCore::FrameInfo& sim_frame, RelayCore::EngineServices& services)
    {
        const RelayCore::gameplay::MissionStatus mission_before = mission_runtime_.status();
        const bool mission_was_complete = mission_before.completed;
        RelayCore::gameplay::collect_blocking_doors(doors_, player_controller_.player_bounds(), dynamic_blockers_);
        player_controller_.update(sim_frame, level_, dynamic_blockers_);
        refresh_runtime_usables_and_focus();

        const bool use_pressed_this_step = player_controller_.consume_use_pressed();
        process_use_input(use_pressed_this_step, services);

        RelayCore::gameplay::query_trigger_candidates(
            trigger_spatial_index_,
            player_controller_.player_bounds(),
            trigger_candidates_
        );
        interaction_state_.fired_triggers += RelayCore::gameplay::update_trigger_overlaps(
            player_controller_.player_bounds(),
            trigger_candidates_,
            triggers_,
            doors_,
            interaction_state_.last_target
        );
        play_door_state_change_audio(services);

        interaction_state_.active_doors = RelayCore::gameplay::update_doors(
            static_cast<float>(sim_frame.delta_seconds),
            player_controller_.player_bounds(),
            doors_
        );
        sync_door_audio_state();
        mission_runtime_.update(player_controller_.player_bounds(), doors_, inventory_item_ids_, activated_target_ids_);
        const RelayCore::gameplay::MissionStatus mission_after = mission_runtime_.status();
        if (!mission_was_complete && mission_after.completed)
        {
            push_transient_hud_event(
                mission_after.title.empty() ? std::string{"Задача выполнена"} : mission_after.title,
                RelayCore::render::HudEventStyle::Success,
                2.6F
            );
        } else if (mission_after.active && !mission_after.completed
                   && (!mission_before.active || mission_after.current_step != mission_before.current_step))
        {
            push_mission_status_event(mission_after, RelayCore::render::HudEventStyle::Neutral, 2.4F);
        }
    }

    void GameWorldRuntime::refresh_runtime_usables_and_focus()
    {
        RelayCore::gameplay::update_runtime_usables(doors_, usables_);
        RelayCore::gameplay::rebuild_usable_spatial_index(usables_, usable_spatial_index_);
        refresh_interaction_focus();
    }

    void GameWorldRuntime::refresh_interaction_focus()
    {
        RelayCore::gameplay::query_usable_candidates(
            usable_spatial_index_,
            player_controller_.eye_position_world(),
            2.25F,
            usable_candidates_
        );
        const auto focus = RelayCore::gameplay::find_focused_usable(
            player_controller_.eye_position_world(),
            player_controller_.view_forward(),
            usables_,
            usable_candidates_
        );
        interaction_state_.focused_usable_index = focus.usable_index;
        interaction_state_.focused_distance = focus.distance;
    }

    auto GameWorldRuntime::build_player_diagnostics() const -> RelayCore::render::PlayerDiagnostics
    {
        RELAYCORE_PROFILE_ZONE_N("GameWorldRuntime::player_diagnostics_build");
        RelayCore::render::PlayerDiagnostics player_diagnostics{};
        player_diagnostics.position = player_controller_.position();
        player_diagnostics.velocity = player_controller_.velocity();
        player_diagnostics.grounded = player_controller_.grounded();
        player_diagnostics.collision_enabled = (player_controller_.movement_mode() != RelayGame::MovementMode::Noclip);
        player_diagnostics.movement_mode = static_cast<std::uint32_t>(player_controller_.movement_mode());

        const auto& velocity = player_diagnostics.velocity;
        player_diagnostics.speed = std::sqrt((velocity.x * velocity.x) + (velocity.z * velocity.z));
        return player_diagnostics;
    }

    auto GameWorldRuntime::build_interaction_diagnostics() const -> RelayCore::render::InteractionDiagnostics
    {
        RELAYCORE_PROFILE_ZONE_N("GameWorldRuntime::interaction_diagnostics_build");
        RelayCore::render::InteractionDiagnostics diagnostics{};
        diagnostics.has_focused_usable = interaction_state_.focused_usable_index >= 0;
        diagnostics.focused_distance = interaction_state_.focused_distance;
        diagnostics.active_doors = interaction_state_.active_doors;
        diagnostics.fired_triggers = interaction_state_.fired_triggers;
        diagnostics.total_doors = static_cast<std::uint32_t>(doors_.size());
        diagnostics.total_triggers = static_cast<std::uint32_t>(triggers_.size());
        diagnostics.use_pressed = interaction_state_.use_pressed;

        if (interaction_state_.focused_usable_index >= 0)
        {
            const auto& usable = usables_[static_cast<std::size_t>(interaction_state_.focused_usable_index)];
            diagnostics.focused_label = build_focused_usable_label(usable);
        }

        diagnostics.last_target = interaction_state_.last_target;
        diagnostics.use_binding_label = interaction_state_.use_binding_label;

        const RelayCore::gameplay::MissionStatus mission = mission_runtime_.status();
        diagnostics.has_objective = mission.active;
        diagnostics.mission_complete = mission.completed;
        diagnostics.objective_current = mission.current_step;
        diagnostics.objective_total = mission.total_steps;
        diagnostics.objective_title = mission.title;
        diagnostics.objective_detail = mission.detail;
        diagnostics.inventory_summary = build_inventory_summary();
        diagnostics.has_inventory = !diagnostics.inventory_summary.empty();
        diagnostics.has_transient_event =
            transient_hud_event_.remaining_seconds > 0.0F && !transient_hud_event_.text.empty();
        diagnostics.transient_event_text = transient_hud_event_.text;
        diagnostics.transient_event_style = transient_hud_event_.style;
        diagnostics.transient_event_strength =
            transient_hud_event_.total_seconds > 0.001F
                ? std::clamp(transient_hud_event_.remaining_seconds / transient_hud_event_.total_seconds, 0.0F, 1.0F)
                : 0.0F;
        return diagnostics;
    }
} // namespace RelayGame
