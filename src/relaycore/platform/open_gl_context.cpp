#include "relaycore/platform/open_gl_context.h"

#include "relaycore/core/log.h"
#include "relaycore/core/profiling.h"
#include "relaycore/core/sdl_error.h"

namespace RelayCore
{
    void OpenGlContext::configure_window(const render::ColorPipelineMode color_pipeline_mode)
    {
        core::require_sdl(SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4), "Failed to set GL major version");
        core::require_sdl(SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 6), "Failed to set GL minor version");
        core::require_sdl(
            SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE),
            "Failed to set GL profile mask"
        );

#ifndef NDEBUG
        core::require_sdl(
            SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_DEBUG_FLAG),
            "Failed to enable GL debug context"
        );
#else
        core::require_sdl(SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0), "Failed to clear GL context flags");
#endif

        core::require_sdl(SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1), "Failed to enable GL double buffering");
        core::require_sdl(SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24), "Failed to set GL depth buffer size");
        core::require_sdl(SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8), "Failed to set GL stencil buffer size");
        if (color_pipeline_mode == render::ColorPipelineMode::hardware_srgb)
        {
            core::require_sdl(
                SDL_GL_SetAttribute(SDL_GL_FRAMEBUFFER_SRGB_CAPABLE, 1),
                "Failed to request an sRGB-capable default framebuffer"
            );
        }
    }

    OpenGlContext::OpenGlContext(
        SDL_Window* window,
        const bool vsync,
        const render::ColorPipelineMode color_pipeline_mode
    ) : window_(window)
    {
        context_.reset(SDL_GL_CreateContext(window_));
        if (context_ == nullptr)
        {
            core::throw_sdl_error("Failed to create OpenGL context");
        }

        const int requested_swap_interval = vsync ? 1 : 0;
        if (!SDL_GL_SetSwapInterval(requested_swap_interval))
        {
            core::throw_sdl_error("Failed to set swap interval");
        }

        if (color_pipeline_mode == render::ColorPipelineMode::hardware_srgb
            && !SDL_GL_GetAttribute(SDL_GL_FRAMEBUFFER_SRGB_CAPABLE, &framebuffer_srgb_capable_))
        {
            core::throw_sdl_error("Failed to query sRGB default framebuffer capability");
        }

        log_context_info();
    }

    auto OpenGlContext::handle() const noexcept -> SDL_GLContext
    {
        return context_.get();
    }

    auto OpenGlContext::framebuffer_srgb_capable() const noexcept -> int
    {
        return framebuffer_srgb_capable_;
    }

    auto OpenGlContext::swap_interval() const -> int
    {
        int interval = 0;
        if (!SDL_GL_GetSwapInterval(&interval))
        {
            return 0;
        }
        return interval;
    }

    auto OpenGlContext::set_swap_interval(int interval) const -> bool
    {
        if (SDL_GL_SetSwapInterval(interval))
        {
            return true;
        }

        log::warn("SDL_GL_SetSwapInterval({}) failed: {}", interval, SDL_GetError());
        return false;
    }

    auto OpenGlContext::toggle_vsync() const -> bool
    {
        const int current = swap_interval();
        const int next = (current == 0) ? 1 : 0;

        if (!set_swap_interval(next))
        {
            return false;
        }

        log::info("VSync: {}", next == 0 ? "off" : "on");
        return true;
    }

    void OpenGlContext::swap_buffers() const
    {
        RELAYCORE_PROFILE_ZONE_N("SDL_GL_SwapWindow");
        if (!SDL_GL_SwapWindow(window_))
        {
            core::throw_sdl_error("Failed to swap OpenGL window buffers");
        }
    }

    void OpenGlContext::log_context_info() const
    {
        int major_version = 0;
        int minor_version = 0;
        int profile_mask = 0;
        int context_flags = 0;

        if (!SDL_GL_GetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, &major_version)
            || !SDL_GL_GetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, &minor_version)
            || !SDL_GL_GetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, &profile_mask)
            || !SDL_GL_GetAttribute(SDL_GL_CONTEXT_FLAGS, &context_flags))
        {
            core::throw_sdl_error("Failed to query OpenGL context attributes");
        }

        const char* profile = "unknown";
        if (profile_mask == SDL_GL_CONTEXT_PROFILE_CORE)
        {
            profile = "core";
        } else if (profile_mask == SDL_GL_CONTEXT_PROFILE_COMPATIBILITY)
        {
            profile = "compatibility";
        } else if (profile_mask == SDL_GL_CONTEXT_PROFILE_ES)
        {
            profile = "es";
        }

        log::info(
            "OpenGL context created: {}.{} | profile={} | flags=0x{:X}",
            major_version,
            minor_version,
            profile,
            context_flags
        );
    }
} // namespace RelayCore
