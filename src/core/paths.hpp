#pragma once

#include <filesystem>
#include <string_view>

namespace RelayCore::core
{
[[nodiscard]] auto executable_base_path() -> std::filesystem::path;
[[nodiscard]] auto asset_root_path() -> std::filesystem::path;
[[nodiscard]] auto asset_path(std::string_view relative_path) -> std::filesystem::path;
} // namespace RelayCore::core
