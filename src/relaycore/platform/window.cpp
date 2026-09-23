/**
 * @file src/relaycore/platform/window.cpp
 * @brief SDL window and OpenGL context implementation.
 */

#include "relaycore/platform/window.h"

#include "relaycore/core/log.h"
#include "relaycore/core/profiling.h"
#include "relaycore/core/sdl_error.h"

#include <SDL3/SDL.h>
#include <memory>

namespace RelayCore
{
    namespace
    {
        /**
         * @brief Configures the SDL OpenGL attributes required by RelayCore.
         */
        void set_gl_attributes(const render::ColorPipelineMode color_pipeline_mode)
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
    } // namespace

    Window::Window(const WindowConfig& config, const render::ColorPipelineMode color_pipeline_mode)
    {
        set_gl_attributes(color_pipeline_mode);

        SDL_WindowFlags flags = SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE;
        if (config.high_dpi)
        {
            flags |= SDL_WINDOW_HIGH_PIXEL_DENSITY;
        }

        std::unique_ptr<SDL_Window, decltype(&SDL_DestroyWindow)> created_window{
            SDL_CreateWindow(config.title.c_str(), config.width, config.height, flags),
            SDL_DestroyWindow,
        };
        if (created_window == nullptr)
        {
            core::throw_sdl_error("Failed to create SDL window");
        }

        std::unique_ptr<SDL_GLContextState, decltype(&SDL_GL_DestroyContext)> created_context{
            SDL_GL_CreateContext(created_window.get()),
            SDL_GL_DestroyContext,
        };
        if (created_context == nullptr)
        {
            core::throw_sdl_error("Failed to create OpenGL context");
        }

        const int requested_swap_interval = config.vsync ? 1 : 0;
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
        window_ = created_window.release();
        gl_context_ = created_context.release();
    }

    Window::~Window() noexcept
    {
        if (gl_context_ != nullptr)
        {
            SDL_GL_DestroyContext(gl_context_);
        }

        if (window_ != nullptr)
        {
            SDL_DestroyWindow(window_);
        }
    }

    auto Window::handle() const noexcept -> SDL_Window*
    {
        return window_;
    }

    auto Window::gl_context() const noexcept -> SDL_GLContext
    {
        return gl_context_;
    }

    auto Window::framebuffer_srgb_capable() const noexcept -> int
    {
        return framebuffer_srgb_capable_;
    }

    auto Window::pixel_extent() const -> PixelExtent
    {
        PixelExtent extent{};
        SDL_GetWindowSizeInPixels(window_, &extent.width, &extent.height);
        return extent;
    }

    auto Window::presentation_state() const -> WindowPresentationState
    {
        const PixelExtent extent = pixel_extent();
        const SDL_WindowFlags flags = SDL_GetWindowFlags(window_);
        return WindowPresentationState{
            .pixel_width = extent.width,
            .pixel_height = extent.height,
            .minimized = (flags & SDL_WINDOW_MINIMIZED) != 0,
            .hidden = (flags & SDL_WINDOW_HIDDEN) != 0,
        };
    }

    auto Window::relative_mouse_mode() const -> bool
    {
        return SDL_GetWindowRelativeMouseMode(window_);
    }

    auto Window::refresh_rate_hz() const -> float
    {
        const SDL_DisplayID display_id = SDL_GetDisplayForWindow(window_);
        if (display_id == 0)
        {
            return 0.0F;
        }

        const SDL_DisplayMode* mode = SDL_GetCurrentDisplayMode(display_id);
        if (mode == nullptr)
        {
            return 0.0F;
        }

        return mode->refresh_rate;
    }

    auto Window::swap_interval() const -> int
    {
        int interval = 0;
        if (!SDL_GL_GetSwapInterval(&interval))
        {
            return 0;
        }
        return interval;
    }

    auto Window::vsync_enabled() const -> bool
    {
        return swap_interval() != 0;
    }

    auto Window::set_swap_interval(int interval) -> bool
    {
        if (SDL_GL_SetSwapInterval(interval))
        {
            return true;
        }

        log::warn("SDL_GL_SetSwapInterval({}) failed: {}", interval, SDL_GetError());
        return false;
    }

    auto Window::toggle_vsync() -> bool
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

    void Window::set_relative_mouse_mode(const bool enabled) const
    {
        core::require_sdl(SDL_SetWindowRelativeMouseMode(window_, enabled), "Failed to set relative mouse mode");
    }

    void Window::swap_buffers() const
    {
        RELAYCORE_PROFILE_ZONE_N("SDL_GL_SwapWindow");
        if (!SDL_GL_SwapWindow(window_))
        {
            core::throw_sdl_error("Failed to swap OpenGL window buffers");
        }
    }

    void Window::log_context_info() const
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
