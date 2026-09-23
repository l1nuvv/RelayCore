#pragma once

#include "relaycore/app/engine_services.h"
#include "relaycore/render/render_diagnostics.h"
#include "relaygame/game_world_runtime.h"
#include "relaygame/runtime_model.h"
#include "relaygame/world_scene_renderer.h"

#include <optional>

namespace RelayGame::detail
{
    struct PreparedLevelSession {
        GameWorldRuntime runtime;
        WorldSceneRenderer world_scene;
    };

    [[nodiscard]] auto prepare_level_session(
        const RelayCore::core::EngineConfig& config,
        LevelSession level,
        const RelayCore::render::RenderAuthoringConfig& authoring,
        RelayCore::render::ColorPipelineMode pipeline,
        RelayCore::render::StaticModelCache& models,
        const SavedGame* save = nullptr
    ) -> PreparedLevelSession;

    struct DemoGameRuntimeState {
        RelayGame::GameWorldRuntime& runtime;
        RelayGame::WorldSceneRenderer& world_scene;
        RelayCore::render::IterationDiagnostics& iteration_diagnostics;
        std::optional<RelayGame::RuntimeLevelSnapshot>& saved_runtime_snapshot;
    };

    void start_level_session(DemoGameRuntimeState state, RelayCore::EngineServices& services);

    void replace_level_session(
        DemoGameRuntimeState state,
        PreparedLevelSession prepared,
        RelayCore::EngineServices& services
    );

    auto reload_live_content(DemoGameRuntimeState state, RelayCore::EngineServices& services) -> bool;

    auto handle_debug_iteration_tools(DemoGameRuntimeState state, RelayCore::EngineServices& services) -> bool;

    void publish_iteration_diagnostics(DemoGameRuntimeState state, RelayCore::EngineServices& services);
} // namespace RelayGame::detail
