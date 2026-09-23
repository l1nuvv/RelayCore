#include "relaycore/core/log.h"
#include "relaycore/core/profiling.h"
#include "relaycore/render/renderer.h"
#include "relaygame/demo_game_debug_commands_internal.h"

namespace RelayGame::detail
{
    namespace
    {
        void save_runtime_snapshot(DemoGameRuntimeState state, RelayCore::EngineServices& services)
        {
            RELAYCORE_PROFILE_ZONE_N("DemoGame::save_runtime_snapshot");
            state.saved_runtime_snapshot = state.runtime.capture_runtime_snapshot();
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
                    "Snapshot restore failed",
                    "No saved runtime snapshot is available"
                );
                publish_iteration_diagnostics(state, services);
                RelayCore::log::warn("{}", state.iteration_diagnostics.last_snapshot_error);
                return;
            }

            const RelayGame::SnapshotRestoreResult restore_result =
                state.runtime.restore_runtime_snapshot(*state.saved_runtime_snapshot);
            publish_snapshot_result(
                state.iteration_diagnostics,
                restore_result.restored,
                RelayGame::build_runtime_restore_summary(restore_result),
                restore_result.error
            );
            state.runtime.publish_diagnostics(services.renderer());
            publish_iteration_diagnostics(state, services);
            if (restore_result.restored)
            {
                RelayCore::log::info("{}", state.iteration_diagnostics.last_snapshot_summary);
            } else
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
