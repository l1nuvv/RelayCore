/**
 * @file src/relaycore/render/gl_loader.cpp
 * @brief glad loader bootstrap implementation.
 */

#include "relaycore/render/gl_loader.h"

#include <SDL3/SDL.h>
#include <glad/gl.h>
#include <stdexcept>

#include "relaycore/core/log.h"

namespace RelayCore::render::gl
{
    namespace
    {
        bool g_loaded        = false; ///< True after glad has loaded OpenGL entry points.
        int  g_major_version = 0;     ///< Loaded OpenGL major version.
        int  g_minor_version = 0;     ///< Loaded OpenGL minor version.
    } // namespace

    void load()
    {
        if (g_loaded) { return; }

        const int version = gladLoadGL(reinterpret_cast<GLADloadfunc>(SDL_GL_GetProcAddress));
        if (version == 0) { throw std::runtime_error("Failed to initialize OpenGL loader via glad"); }

        g_major_version = GLAD_VERSION_MAJOR(version);
        g_minor_version = GLAD_VERSION_MINOR(version);
        g_loaded        = true;

        log::info("Loaded OpenGL via glad: {}.{}", g_major_version, g_minor_version);
    }

    auto is_loaded() noexcept -> bool { return g_loaded; }

    auto loaded_major_version() noexcept -> int { return g_major_version; }

    auto loaded_minor_version() noexcept -> int { return g_minor_version; }
} // namespace RelayCore::render::gl
