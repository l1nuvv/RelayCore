/**
 * @file src/relaycore/world/blockout_loader.cpp
 * @brief Public blockout-level TOML loader entrypoint.
 */

#include "relaycore/world/blockout_loader.h"

#include "relaycore/world/blockout_loader_detail.h"

namespace RelayCore
{
    BlockoutLevel load_blockout_level(const std::filesystem::path& path)
    {
        return detail::load_blockout_level_data(path);
    }
} // namespace RelayCore
