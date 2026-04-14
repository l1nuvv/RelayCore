#pragma once

#include "relaycore/world/blockout_level.h"

#include <filesystem>

namespace RelayCore::detail
{
    [[nodiscard]] BlockoutLevel load_blockout_level_data(const std::filesystem::path& path);
} // namespace RelayCore::detail
