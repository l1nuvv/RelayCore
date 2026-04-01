#include "relaycore/core/paths.hpp"

#include <SDL3/SDL_filesystem.h>

#include "relaycore/core/log.hpp"

#ifndef RELAYCORE_ASSET_DIR
#error "RELAYCORE_ASSET_DIR must be defined by the build system"
#endif

namespace RelayCore::core
{
    auto executable_base_path() -> std::filesystem::path
    {
        const char* base_path = SDL_GetBasePath();
        if (base_path != nullptr) { return std::filesystem::path(base_path); }

        log::warn("SDL_GetBasePath failed, falling back to build-time asset path: {}", SDL_GetError());
        return std::filesystem::path(RELAYCORE_ASSET_DIR).parent_path();
    }

    auto asset_root_path() -> std::filesystem::path
    {
        const std::filesystem::path runtime_asset_root = executable_base_path() / "assets";
        if (std::filesystem::exists(runtime_asset_root)) { return runtime_asset_root; }

        return std::filesystem::path(RELAYCORE_ASSET_DIR);
    }

    auto asset_path(const std::string_view relative_path) -> std::filesystem::path
    {
        return asset_root_path() / std::filesystem::path(relative_path);
    }
} // namespace RelayCore::core
