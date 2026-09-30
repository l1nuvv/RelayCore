#include "relaycore/core/profiling.h"
#include "relaycore/render/renderer.h"
#include "relaygame/demo_game_iteration_internal.h"

namespace RelayGame::detail
{
    void publish_snapshot_result(
        RelayCore::render::IterationDiagnostics& iteration_diagnostics,
        const bool succeeded,
        std::string summary,
        std::string error
    )
    {
        iteration_diagnostics.has_last_snapshot_result = true;
        iteration_diagnostics.last_snapshot_succeeded = succeeded;
        iteration_diagnostics.last_snapshot_summary = std::move(summary);
        iteration_diagnostics.last_snapshot_error = std::move(error);
    }

    void publish_command_result(
        RelayCore::render::IterationDiagnostics& iteration_diagnostics,
        const bool succeeded,
        std::string summary,
        std::string error
    )
    {
        iteration_diagnostics.has_last_command_result = true;
        iteration_diagnostics.last_command_succeeded = succeeded;
        iteration_diagnostics.last_command_summary = std::move(summary);
        iteration_diagnostics.last_command_error = std::move(error);
    }

    void update_saved_snapshot_diagnostics(DemoGameRuntimeState state)
    {
        state.iteration_diagnostics.has_saved_runtime_snapshot = state.saved_runtime_snapshot.has_value();
        state.iteration_diagnostics.saved_snapshot_summary =
            state.saved_runtime_snapshot.has_value()
                ? "Local encounter (player, enemies, world): "
                      + RelayGame::build_runtime_snapshot_summary(state.saved_runtime_snapshot->game.world)
                : std::string{};
    }

    void push_iteration_diagnostics(DemoGameRuntimeState state, RelayCore::EngineServices& services)
    {
        services.renderer().set_iteration_diagnostics(state.iteration_diagnostics);
    }

    void publish_iteration_diagnostics(DemoGameRuntimeState state, RelayCore::EngineServices& services)
    {
        RELAYCORE_PROFILE_FUNCTION();
        state.iteration_diagnostics.authoring_warnings = state.runtime.authoring_warnings();
        state.iteration_diagnostics.authoring_warnings.insert(
            state.iteration_diagnostics.authoring_warnings.end(),
            state.world_scene.authoring_warnings().begin(),
            state.world_scene.authoring_warnings().end()
        );
        state.iteration_diagnostics.runtime_registry = state.runtime.runtime_registry_diagnostics();
        state.iteration_diagnostics.runtime_bindings = state.runtime.runtime_binding_diagnostics();
        update_saved_snapshot_diagnostics(state);
        push_iteration_diagnostics(state, services);
    }
} // namespace RelayGame::detail
