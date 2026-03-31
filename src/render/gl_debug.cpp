#include "render/gl_debug.hpp"

#include <SDL3/SDL.h>
#include <glad/gl.h>

#include "core/log.hpp"

namespace RelayCore::render::gl_debug
{
    namespace
    {
        bool g_available = false;

#ifndef APIENTRY
#define APIENTRY __stdcall
#endif

        [[nodiscard]] auto to_text(const GLenum severity) -> const char*
        {
            switch (severity)
            {
                case GL_DEBUG_SEVERITY_HIGH: return "high";
                case GL_DEBUG_SEVERITY_MEDIUM: return "medium";
                case GL_DEBUG_SEVERITY_LOW: return "low";
                case GL_DEBUG_SEVERITY_NOTIFICATION: return "notification";
                default: return "unknown";
            }
        }

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
        if (g_available) { return; }

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

    auto is_available() noexcept -> bool { return g_available; }
} // namespace RelayCore::render::gl_debug
