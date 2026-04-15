#pragma once

#include "relaycore/app/engine_services.h"
#include "relaycore/gameplay/runtime_model.h"
#include "relaycore/render/render_diagnostics.h"
#include "relaygame/game_world_runtime.h"
#include "relaygame/world_scene_renderer.h"

#include <optional>

namespace RelayGame::detail
{
    struct DemoGameRuntimeState {
        RelayGame::GameWorldRuntime& runtime;
        RelayGame::WorldSceneRenderer& world_scene;
        RelayCore::render::IterationDiagnostics& iteration_diagnostics;
        std::optional<RelayCore::gameplay::RuntimeLevelSnapshot>& saved_runtime_snapshot;
    };

    void start_level_session(DemoGameRuntimeState state, RelayCore::EngineServices& services);

    void reload_live_content(DemoGameRuntimeState state, RelayCore::EngineServices& services);

    void handle_debug_iteration_tools(DemoGameRuntimeState state, RelayCore::EngineServices& services);

    void publish_iteration_diagnostics(DemoGameRuntimeState state, RelayCore::EngineServices& services);
} // namespace RelayGame::detail
