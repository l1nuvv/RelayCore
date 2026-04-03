/**
 * @file src/relaycore/world/blockout_loader.h
 * @brief TOML loader for blockout-level data.
 */

#pragma once

#include "relaycore/world/blockout_level.h"

#include <filesystem>

namespace RelayCore
{
    /**
     * @brief Loads a blockout level from a TOML file.
     * @param path Path to the level file.
     * @return Parsed blockout level.
     */
    [[nodiscard]] BlockoutLevel load_blockout_level(const std::filesystem::path& path);
} // namespace RelayCore
