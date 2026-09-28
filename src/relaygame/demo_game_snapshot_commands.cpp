#include "relaycore/core/log.h"
#include "relaycore/core/profiling.h"
#include "relaycore/platform/window.h"
#include "relaycore/render/renderer.h"
#include "relaygame/demo_game_debug_commands_internal.h"

namespace RelayGame::detail
{
    namespace
    {
        void save_runtime_snapshot(DemoGameRuntimeState state, RelayCore::EngineServices& services)
        {
            RELAYCORE_PROFILE_ZONE_N("DemoGame::save_runtime_snapshot");
            state.saved_runtime_snapshot =
                state.runtime.capture_encounter_snapshot(state.played_seconds, state.scene_time, state.flashlight);
            update_saved_snapshot_diagnostics(state);
            publish_snapshot_result(
                state.iteration_diagnostics,
                true,
                state.iteration_diagnostics.saved_snapshot_summary
            );
            publish_iteration_diagnostics(state, services);
            RelayCore::log::info("{}", state.iteration_diagnostics.last_snapshot_summary);
        }

        void restore_runtime_snapshot(DemoGameRuntimeState state, RelayCore::EngineServices& services)
        {
            RELAYCORE_PROFILE_ZONE_N("DemoGame::restore_runtime_snapshot");
            if (!state.saved_runtime_snapshot.has_value())
            {
                publish_snapshot_result(
                    state.iteration_diagnostics,
                    false,
                    "Local encounter restore failed",
                    "No local encounter snapshot is available (F9)"
                );
                publish_iteration_diagnostics(state, services);
                RelayCore::log::warn("{}", state.iteration_diagnostics.last_snapshot_error);
                return;
            }

            try
            {
                auto restored = state.runtime;
                restored.restore_encounter_snapshot(*state.saved_runtime_snapshot);
                restored.prepare_level_session_activation(services, services.window().relative_mouse_mode());
                state.runtime = std::move(restored);
                state.played_seconds = state.saved_runtime_snapshot->game.played_seconds;
                state.scene_time = state.saved_runtime_snapshot->scene_time;
                state.flashlight = state.saved_runtime_snapshot->game.flashlight;
                state.runtime.activate_level_session(services);
                publish_snapshot_result(state.iteration_diagnostics, true, "Local encounter restored (memory only)");
            } catch (const std::exception& error)
            {
                publish_snapshot_result(
                    state.iteration_diagnostics,
                    false,
                    "Local encounter restore failed",
                    error.what()
                );
            }
            state.runtime.publish_diagnostics(services.renderer());
            publish_iteration_diagnostics(state, services);
            if (state.iteration_diagnostics.last_snapshot_succeeded)
                RelayCore::log::info("{}", state.iteration_diagnostics.last_snapshot_summary);
            else
            {
                RelayCore::log::warn(
                    "{}",
                    state.iteration_diagnostics.last_snapshot_error.empty()
                        ? state.iteration_diagnostics.last_snapshot_summary
                        : state.iteration_diagnostics.last_snapshot_error
                );
            }
        }
    } // namespace

    auto handle_runtime_snapshot_commands(
        DemoGameRuntimeState state,
        RelayCore::EngineServices& services,
        const IterationCommandRequests& requests
    ) -> bool
    {
        RELAYCORE_PROFILE_ZONE_N("DemoGame::handle_runtime_snapshot_commands");
        if (requests.runtime.save_runtime_snapshot || requests.overlay.save_runtime_snapshot)
        {
            save_runtime_snapshot(state, services);
            return true;
        }

        if (requests.runtime.restore_runtime_snapshot || requests.overlay.restore_runtime_snapshot)
        {
            restore_runtime_snapshot(state, services);
            return true;
        }

        return false;
    }
} // namespace RelayGame::detail
