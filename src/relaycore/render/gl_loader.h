/**
 * @file src/relaycore/render/gl_loader.h
 * @brief glad-based OpenGL loader helpers.
 */

#pragma once

namespace RelayCore::render::gl
{
    /**
     * @brief Loads OpenGL entry points through SDL and glad.
     */
    void load();

    /**
     * @brief Reports whether OpenGL entry points have been loaded.
     * @return True when glad initialization completed successfully.
     */
    [[nodiscard]] auto is_loaded() noexcept -> bool;

    /**
     * @brief Returns the loaded OpenGL major version.
     * @return Loaded OpenGL major version.
     */
    [[nodiscard]] auto loaded_major_version() noexcept -> int;

    /**
     * @brief Returns the loaded OpenGL minor version.
     * @return Loaded OpenGL minor version.
     */
    [[nodiscard]] auto loaded_minor_version() noexcept -> int;
} // namespace RelayCore::render::gl
