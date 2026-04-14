/**
 * @file src/relaycore/render/gl_debug.cpp
 * @brief OpenGL debug callback and setup implementation.
 */

#include "relaycore/render/gl_debug.h"

#include "relaycore/core/log.h"
#include "relaycore/core/profiling.h"

#include <SDL3/SDL.h>
#include <glad/gl.h>

namespace RelayCore::render::gl_debug
{
    namespace
    {
        bool g_available = false; ///< True after the debug callback has been installed.

#ifndef APIENTRY
#define APIENTRY __stdcall
#endif

        /**
         * @brief Converts an OpenGL debug severity enum to text.
         * @param severity OpenGL debug severity enum.
         * @return String representation of the severity.
         */
        [[nodiscard]] auto to_text(const GLenum severity) -> const char*
        {
            RELAYCORE_PROFILE_FUNCTION();
            switch (severity)
            {
                case GL_DEBUG_SEVERITY_HIGH:
                    return "high";
                case GL_DEBUG_SEVERITY_MEDIUM:
                    return "medium";
                case GL_DEBUG_SEVERITY_LOW:
                    return "low";
                case GL_DEBUG_SEVERITY_NOTIFICATION:
                    return "notification";
                default:
                    return "unknown";
            }
        }

        /**
         * @brief Receives OpenGL debug messages and routes them into the logger.
         * @param source OpenGL debug source enum.
         * @param type OpenGL debug message type enum.
         * @param id Driver-supplied message identifier.
         * @param severity OpenGL debug severity enum.
         * @param length Length of the message string.
         * @param message Null-terminated debug message text.
         * @param user_param User data pointer supplied when installing the callback.
         */
        void APIENTRY debug_callback(
            GLenum /*source*/,
            GLenum type,
            GLuint id,
            GLenum severity,
            GLsizei /*length*/,
            const GLchar* message,
            const void* /*user_param*/
        )
        {
            RELAYCORE_PROFILE_FUNCTION();
            if (type == GL_DEBUG_TYPE_ERROR || severity == GL_DEBUG_SEVERITY_HIGH)
            {
                log::error("OpenGL debug [{}] id={} | {}", to_text(severity), id, message);
                return;
            }

            if (severity == GL_DEBUG_SEVERITY_MEDIUM)
            {
                log::warn("OpenGL debug [{}] id={} | {}", to_text(severity), id, message);
                return;
            }

            log::debug("OpenGL debug [{}] id={} | {}", to_text(severity), id, message);
        }
    } // namespace

    void initialize()
    {
        RELAYCORE_PROFILE_FUNCTION();
        if (g_available)
        {
            return;
        }

        int context_flags = 0;
        if (!SDL_GL_GetAttribute(SDL_GL_CONTEXT_FLAGS, &context_flags))
        {
            log::warn("Failed to query GL context flags: {}", SDL_GetError());
            return;
        }

        if ((context_flags & SDL_GL_CONTEXT_DEBUG_FLAG) == 0)
        {
            log::info("OpenGL debug context is disabled");
            return;
        }

        if (glDebugMessageCallback == nullptr || glDebugMessageControl == nullptr)
        {
            log::warn("OpenGL debug output is unavailable for this context");
            return;
        }

        glEnable(GL_DEBUG_OUTPUT);
        glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
        glDebugMessageCallback(debug_callback, nullptr);
        glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DEBUG_SEVERITY_NOTIFICATION, 0, nullptr, GL_FALSE);
        g_available = true;
        log::info("OpenGL debug output enabled");
    }

    auto is_available() noexcept -> bool
    {
        return g_available;
    }
} // namespace RelayCore::render::gl_debug
