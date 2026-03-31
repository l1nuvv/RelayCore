#include "platform/window.hpp"

#include <SDL3/SDL.h>

#include "core/log.hpp"
#include "core/sdl_error.hpp"

namespace RelayCore
{
    namespace
    {
        void set_gl_attributes()
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
        }
    } // namespace

    Window::Window(const WindowConfig& config)
    {
        set_gl_attributes();

        SDL_WindowFlags flags = SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE;
        if (config.high_dpi) { flags |= SDL_WINDOW_HIGH_PIXEL_DENSITY; }

        SDL_Window* created_window = SDL_CreateWindow(config.title.c_str(), config.width, config.height, flags);
        if (created_window == nullptr) { core::throw_sdl_error("Failed to create SDL window"); }

        SDL_GLContext created_context = SDL_GL_CreateContext(created_window);
        if (created_context == nullptr)
        {
            SDL_DestroyWindow(created_window);
            core::throw_sdl_error("Failed to create OpenGL context");
        }

        const int requested_swap_interval = config.vsync ? 1 : 0;
        if (!SDL_GL_SetSwapInterval(requested_swap_interval))
        {
            SDL_GL_DestroyContext(created_context);
            SDL_DestroyWindow(created_window);
            core::throw_sdl_error("Failed to set swap interval");
        }

        window_     = created_window;
        gl_context_ = created_context;

        log_context_info();
    }

    Window::~Window() noexcept
    {
        if (gl_context_ != nullptr) { SDL_GL_DestroyContext(gl_context_); }

        if (window_ != nullptr) { SDL_DestroyWindow(window_); }
    }

    auto Window::handle() const noexcept -> SDL_Window* { return window_; }

    auto Window::gl_context() const noexcept -> SDL_GLContext { return gl_context_; }

    auto Window::pixel_extent() const -> PixelExtent
    {
        PixelExtent extent{};
        SDL_GetWindowSizeInPixels(window_, &extent.width, &extent.height);
        return extent;
    }

    auto Window::relative_mouse_mode() const -> bool { return SDL_GetWindowRelativeMouseMode(window_); }

    void Window::set_relative_mouse_mode(const bool enabled) const
    {
        core::require_sdl(SDL_SetWindowRelativeMouseMode(window_, enabled), "Failed to set relative mouse mode");
    }

    void Window::swap_buffers() const
    {
        if (!SDL_GL_SwapWindow(window_)) { core::throw_sdl_error("Failed to swap OpenGL window buffers"); }
    }

    void Window::log_context_info() const
    {
        int major_version = 0;
        int minor_version = 0;
        int profile_mask  = 0;
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
