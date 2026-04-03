/**
 * @file src/relaycore/render/gl_debug.h
 * @brief OpenGL debug-output initialization helpers.
 */

#pragma once

namespace RelayCore::render::gl_debug
{
    /**
     * @brief Initializes OpenGL debug output for the active context.
     */
    void initialize();

    /**
     * @brief Reports whether OpenGL debug output is active.
     * @return True when the debug callback has been installed.
     */
    [[nodiscard]] auto is_available() noexcept -> bool;
} // namespace RelayCore::render::gl_debug
