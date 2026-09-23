#include "relaycore/core/profiling.h"
#include "relaygame/demo_game_debug_commands_internal.h"
#include "relaygame/demo_game_iteration_internal.h"

namespace RelayGame::detail
{
    auto handle_debug_iteration_tools(DemoGameRuntimeState state, RelayCore::EngineServices& services) -> bool
    {
        RELAYCORE_PROFILE_ZONE_N("DemoGame::handle_debug_iteration_tools");
        RelayCore::Renderer& renderer = services.renderer();
        const IterationCommandRequests requests = capture_iteration_command_requests(state, renderer);

        if (requests.runtime.reload_level)
        {
            return reload_live_content(state, services);
        }

        if (handle_runtime_snapshot_commands(state, services, requests))
        {
            return false;
        }

        if (handle_authoring_warning_commands(state, services, requests))
        {
            return false;
        }

        if (handle_runtime_registry_commands(state, services, requests))
        {
            return false;
        }

        if (handle_runtime_binding_commands(state, services, requests))
        {
            return false;
        }

        if (handle_render_authoring_commands(state, services, requests))
        {
            return false;
        }

        handle_open_inspector_source_command(state, services, requests);
        return false;
    }
} // namespace RelayGame::detail
