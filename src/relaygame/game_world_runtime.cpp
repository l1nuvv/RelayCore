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
        level_ = std::move(session.level);
        doors_ = std::move(session.doors);
        triggers_ = std::move(session.triggers);
        usables_ = std::move(session.usables);
        trigger_spatial_index_ = std::move(session.trigger_spatial_index);
        usable_spatial_index_ = std::move(session.usable_spatial_index);
        interaction_state_.use_binding_label = services.config().gameplay.controls.use.empty()
                                                   ? std::string{"E"}
                                                   : services.config().gameplay.controls.use;
        player_controller_.set_config(build_player_controller_config(services.config()));
        player_controller_.initialize(spawn_position);
        player_controller_.set_static_collision_world(level_);
        player_controller_.enable_capture(services.window());
        simulation_loop_.reset();
        player_controller_.apply_camera(services.camera());
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

        (void) simulation_loop_.advance(frame, [this](const RelayCore::FrameInfo& sim_frame) {
            simulate_fixed_step(sim_frame);
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

    void GameWorldRuntime::simulate_fixed_step(const RelayCore::FrameInfo& sim_frame)
    {
        RelayCore::gameplay::collect_blocking_doors(doors_, dynamic_blockers_);
        player_controller_.update(sim_frame, level_, dynamic_blockers_);
        refresh_runtime_usables_and_focus();

        const bool use_pressed_this_step = player_controller_.consume_use_pressed();
        interaction_state_.use_pressed = interaction_state_.use_pressed || use_pressed_this_step;
        if (use_pressed_this_step && interaction_state_.focused_usable_index >= 0)
        {
            const RelayCore::gameplay::RuntimeUsable& usable =
                usables_[static_cast<std::size_t>(interaction_state_.focused_usable_index)];
            if (RelayCore::gameplay::activate_target(usable.target, doors_))
            {
                interaction_state_.last_target = usable.target;
            }
        }

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

        interaction_state_.active_doors =
            RelayCore::gameplay::update_doors(static_cast<float>(sim_frame.delta_seconds), doors_);
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
            diagnostics.focused_label = usable.label;
        }

        diagnostics.last_target = interaction_state_.last_target;
        diagnostics.use_binding_label = interaction_state_.use_binding_label;
        return diagnostics;
    }
} // namespace RelayGame
