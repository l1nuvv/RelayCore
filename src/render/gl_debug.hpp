#pragma once

namespace RelayCore::render::gl_debug
{
void initialize();
[[nodiscard]] auto is_available() noexcept -> bool;
} // namespace RelayCore::render::gl_debug
