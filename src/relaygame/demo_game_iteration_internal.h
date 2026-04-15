#pragma once

#include "relaygame/demo_game_runtime_ops.h"

#include <string>

namespace RelayGame::detail
{
    void publish_snapshot_result(
        RelayCore::render::IterationDiagnostics& iteration_diagnostics,
        bool succeeded,
        std::string summary,
        std::string error = {}
    );

    void publish_command_result(
        RelayCore::render::IterationDiagnostics& iteration_diagnostics,
        bool succeeded,
        std::string summary,
        std::string error = {}
    );

    void update_saved_snapshot_diagnostics(DemoGameRuntimeState state);

    void push_iteration_diagnostics(DemoGameRuntimeState state, RelayCore::EngineServices& services);
} // namespace RelayGame::detail
