#pragma once

#include "relaygame/player_controller.h"

namespace RelayCore::core
{
    struct EngineConfig;
}

namespace RelayGame
{
    [[nodiscard]] auto build_player_controller_config(const RelayCore::core::EngineConfig& config)
        -> GroundedPlayerControllerConfig;
}
