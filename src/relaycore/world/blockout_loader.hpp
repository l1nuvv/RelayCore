#pragma once

#include "relaycore/world/blockout_level.hpp"

#include <filesystem>

namespace RelayCore
{
[[nodiscard]] BlockoutLevel load_blockout_level(const std::filesystem::path& path);
} // namespace RelayCore
