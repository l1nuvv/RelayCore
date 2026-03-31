#pragma once

namespace RelayCore::render::gl
{
void load();
[[nodiscard]] auto is_loaded() noexcept -> bool;
[[nodiscard]] auto loaded_major_version() noexcept -> int;
[[nodiscard]] auto loaded_minor_version() noexcept -> int;
} // namespace RelayCore::render::gl
