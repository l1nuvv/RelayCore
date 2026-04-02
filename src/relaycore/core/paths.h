/**
 * @file src/relaycore/core/paths.h
 * @brief Runtime path helpers for executable-relative assets.
 */

#pragma once

#include <filesystem>
#include <string_view>

namespace RelayCore::core
{
    /**
     * @brief Returns the base directory of the running executable.
     * @return Base directory reported by SDL or a build-time fallback.
     */
    [[nodiscard]] auto executable_base_path() -> std::filesystem::path;

    /**
     * @brief Returns the runtime asset root directory.
     * @return Path to the resolved asset root.
     */
    [[nodiscard]] auto asset_root_path() -> std::filesystem::path;

    /**
     * @brief Builds an asset path relative to the resolved asset root.
     * @param relative_path Relative path inside the asset root.
     * @return Absolute or executable-relative asset path.
     */
    [[nodiscard]] auto asset_path(std::string_view relative_path) -> std::filesystem::path;
} // namespace RelayCore::core
