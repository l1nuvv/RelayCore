/**
 * @file src/relaycore/core/paths.cpp
 * @brief Runtime asset-path resolution implementation.
 */

#include "relaycore/core/paths.h"

#include "relaycore/core/profiling.h"

#include <SDL3/SDL_error.h>
#include <SDL3/SDL_filesystem.h>
#include <cstdlib>
#include <stdexcept>
#include <string>

namespace RelayCore::core
{
    auto executable_base_path() -> std::filesystem::path
    {
        RELAYCORE_PROFILE_FUNCTION();
        const char* base_path = SDL_GetBasePath();
        if (base_path == nullptr)
        {
            throw std::runtime_error(
                std::string("SDL_GetBasePath failed while resolving runtime assets: ") + SDL_GetError()
            );
        }

        return std::filesystem::path(base_path);
    }

    auto asset_root_path() -> std::filesystem::path
    {
        RELAYCORE_PROFILE_FUNCTION();

        std::filesystem::path asset_root;
        if (const char* override_path = std::getenv("RELAYCORE_ASSET_ROOT"); override_path != nullptr)
        {
            asset_root = std::filesystem::path(override_path);
            if (!asset_root.is_absolute())
            {
                throw std::runtime_error(
                    "RELAYCORE_ASSET_ROOT must be an absolute path: '" + asset_root.string() + "'"
                );
            }
        } else
        {
            asset_root = executable_base_path() / "assets";
        }

        std::error_code path_error;
        const bool is_directory = std::filesystem::is_directory(asset_root, path_error);
        if (path_error)
        {
            throw std::runtime_error(
                "Failed to inspect asset directory '" + asset_root.string() + "': " + path_error.message()
            );
        }
        if (!is_directory)
        {
            throw std::runtime_error(
                "Asset directory does not exist or is not a directory: '" + asset_root.string() + "'"
            );
        }

        return asset_root;
    }

    auto asset_path(const std::string_view relative_path) -> std::filesystem::path
    {
        RELAYCORE_PROFILE_FUNCTION();
        return asset_root_path() / std::filesystem::path(relative_path);
    }
} // namespace RelayCore::core
