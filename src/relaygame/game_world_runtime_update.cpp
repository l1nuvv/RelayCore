#include "relaycore/app/engine_services.h"
#include "relaycore/core/log.h"
#include "relaycore/core/profiling.h"
#include "relaycore/physics/collision.h"
#include "relaygame/game_world_runtime.h"

#include <algorithm>

namespace RelayGame
{
    void GameWorldRuntime::update(const RelayCore::FrameInfo& frame, RelayCore::EngineServices& services)
    {
        RELAYCORE_PROFILE_ZONE_N("GameWorldRuntime::update");

        const float delta_seconds = static_cast<float>(frame.delta_seconds);
        sync_imgui_capture(services);
        begin_frame_update(delta_seconds);

        runtime_state_.fixed_steps_last_frame =
            simulation_loop_.advance(frame, [this, &services](const RelayCore::FrameInfo& sim_frame) {
                simulate_fixed_step(sim_frame, frame_actions_, services);
            });

        refresh_runtime_usables_and_focus();
        handle_inspector_pin_input();
        update_trigger_overlap_count();
        refresh_active_door_count();
        finalize_frame_update(services, delta_seconds);
    }

    void GameWorldRuntime::begin_frame_update(const float delta_seconds)
    {
        RELAYCORE_PROFILE_FUNCTION();
        interaction_state_.use_pressed = false;
        interaction_state_.fired_triggers = 0;
        combat_state_.fire_pressed = false;
        combat_state_.active_hazard_contacts = 0;
        combat_state_.turret_shots_last_frame = 0;
        player_controller_.flush_pending_look();
        const FrameGameplayActions captured_actions = capture_frame_gameplay_actions();
        // Latch discrete sim actions until a fixed step consumes them.
        frame_actions_.use_pressed = frame_actions_.use_pressed || captured_actions.use_pressed;
        frame_actions_.fire_pressed = frame_actions_.fire_pressed || captured_actions.fire_pressed;
        frame_actions_.inspector_pin_pressed = captured_actions.inspector_pin_pressed;
        frame_iteration_commands_ = capture_frame_iteration_commands();
        tick_transient_hud_event(delta_seconds);
        tick_usable_feedback(delta_seconds);
    }

    auto GameWorldRuntime::capture_frame_gameplay_actions() noexcept -> FrameGameplayActions
    {
        RELAYCORE_PROFILE_FUNCTION();
        FrameGameplayActions actions{};
        actions.use_pressed = player_controller_.consume_use_pressed();
        actions.fire_pressed = player_controller_.consume_fire_pressed();
        actions.inspector_pin_pressed = player_controller_.consume_inspector_pin_pressed();
        return actions;
    }

    auto GameWorldRuntime::capture_frame_iteration_commands() noexcept -> IterationCommandRequests
    {
        RELAYCORE_PROFILE_FUNCTION();
        IterationCommandRequests commands{};
        commands.reload_level = player_controller_.consume_reload_level_pressed();
        commands.open_inspector_source = player_controller_.consume_open_inspector_source_pressed();
        commands.save_runtime_snapshot = player_controller_.consume_save_runtime_snapshot_pressed();
        commands.restore_runtime_snapshot = player_controller_.consume_restore_runtime_snapshot_pressed();
        return commands;
    }

    void GameWorldRuntime::handle_inspector_pin_input()
    {
        RELAYCORE_PROFILE_FUNCTION();
        if (!frame_actions_.inspector_pin_pressed)
        {
            return;
        }

        if (inspector_state_.pinned)
        {
            inspector_state_ = {};
            RelayCore::log::info("Inspector pin cleared");
            return;
        }

        const RelayCore::render::InspectorDiagnostics live_inspector = build_live_inspector_diagnostics();
        if (!live_inspector.has_pick)
        {
            RelayCore::log::info("Inspector pin ignored: no object under crosshair");
            return;
        }

        inspector_state_.pinned = true;
        inspector_state_.pinned_diagnostics = live_inspector;
        inspector_state_.pinned_diagnostics.pinned = true;
        RelayCore::log::info(
            "Inspector pinned {} {}",
            inspector_state_.pinned_diagnostics.object_kind.empty() ? std::string{"object"}
                                                                    : inspector_state_.pinned_diagnostics.object_kind,
            inspector_state_.pinned_diagnostics.name.empty() ? std::string{}
                                                             : ("'" + inspector_state_.pinned_diagnostics.name + "'")
        );
    }

    void GameWorldRuntime::update_trigger_overlap_count()
    {
        RELAYCORE_PROFILE_FUNCTION();
        runtime_state_.active_trigger_overlaps = 0;
        const RelayCore::physics::Aabb player_bounds = player_controller_.player_bounds();
        RelayCore::gameplay::query_trigger_candidates(trigger_spatial_index_, player_bounds, trigger_candidates_);
        for (const std::uint32_t candidate_index : trigger_candidates_)
        {
            if (candidate_index >= triggers_.size())
            {
                continue;
            }

            if (RelayCore::physics::overlaps(player_bounds, triggers_[candidate_index].bounds, 0.001F))
            {
                ++runtime_state_.active_trigger_overlaps;
            }
        }
    }

    void GameWorldRuntime::refresh_active_door_count() noexcept
    {
        interaction_state_.active_doors =
            static_cast<std::uint32_t>(std::count_if(doors_.begin(), doors_.end(), [](const auto& door) {
                return RelayCore::gameplay::is_door_active(door);
            }));
    }

    void GameWorldRuntime::update_door_runtime(const float sim_delta_seconds) noexcept
    {
        RELAYCORE_PROFILE_FUNCTION();
        interaction_state_.active_doors =
            RelayCore::gameplay::update_doors(sim_delta_seconds, player_controller_.player_bounds(), doors_);
    }

    void GameWorldRuntime::finalize_frame_update(RelayCore::EngineServices& services, const float delta_seconds)
    {
        RELAYCORE_PROFILE_FUNCTION();
        update_ambient_audio(services);
        services.audio_system().update(delta_seconds);
        refresh_audio_runtime_state(services);
        player_controller_.apply_camera(services.camera());
    }
} // namespace RelayGame
