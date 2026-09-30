/**
 * @file src/relaycore/platform/window.cpp
 * @brief SDL window implementation.
 */

#include "relaycore/platform/window.h"

#include "relaycore/core/sdl_error.h"

#include <SDL3/SDL_vulkan.h>
#include <stdexcept>

namespace RelayCore
{
    Window::Window(
        const WindowConfig& config,
        const render::ColorPipelineMode color_pipeline_mode,
        const WindowGraphicsApi graphics_api
    )
    {
        if (graphics_api == WindowGraphicsApi::opengl)
        {
            OpenGlContext::configure_window(color_pipeline_mode);
        }

        SDL_WindowFlags flags = SDL_WINDOW_RESIZABLE;
        flags |= graphics_api == WindowGraphicsApi::opengl ? SDL_WINDOW_OPENGL : SDL_WINDOW_VULKAN;
        if (config.high_dpi)
        {
            flags |= SDL_WINDOW_HIGH_PIXEL_DENSITY;
        }

        window_.reset(SDL_CreateWindow(config.title.c_str(), config.width, config.height, flags));
        if (window_ == nullptr)
        {
            core::throw_sdl_error("Failed to create SDL window");
        }

        SDL_Rect usable_bounds{};
        core::require_sdl(
            SDL_GetDisplayUsableBounds(SDL_GetDisplayForWindow(window_.get()), &usable_bounds),
            "Failed to query window display usable bounds"
        );
        int top = 0, left = 0, bottom = 0, right = 0;
        // Border sizes may be unavailable until the window system has decorated the window.
        static_cast<void>(SDL_GetWindowBordersSize(window_.get(), &top, &left, &bottom, &right));
        if (config.width + left + right > usable_bounds.w || config.height + top + bottom > usable_bounds.h)
        {
            core::require_sdl(SDL_MaximizeWindow(window_.get()), "Failed to fit window to display");
            core::require_sdl(SDL_SyncWindow(window_.get()), "Failed to synchronize fitted window");
        }

        if (graphics_api == WindowGraphicsApi::opengl)
        {
            gl_context_.emplace(window_.get(), config.vsync, color_pipeline_mode);
        }
    }

    Window::~Window() noexcept = default;

    auto Window::handle() const noexcept -> SDL_Window*
    {
        return window_.get();
    }

    auto Window::gl_context() const noexcept -> SDL_GLContext
    {
        return gl_context_ ? gl_context_->handle() : nullptr;
    }

    auto Window::require_gl_context() const -> const OpenGlContext&
    {
        if (!gl_context_)
        {
            throw std::logic_error("OpenGL presentation requested for a Vulkan window");
        }
        return *gl_context_;
    }

    auto Window::framebuffer_srgb_capable() const -> int
    {
        return require_gl_context().framebuffer_srgb_capable();
    }

    auto Window::vulkan_instance_extensions() const -> std::span<const char* const>
    {
        if ((SDL_GetWindowFlags(window_.get()) & SDL_WINDOW_VULKAN) == 0)
        {
            throw std::logic_error("Vulkan instance extensions requested for an OpenGL window");
        }
        Uint32 count = 0;
        const auto extensions = SDL_Vulkan_GetInstanceExtensions(&count);
        if (extensions == nullptr)
        {
            core::throw_sdl_error("Failed to query SDL Vulkan instance extensions");
        }
        return {extensions, count};
    }

    auto Window::pixel_extent() const -> PixelExtent
    {
        PixelExtent extent{};
        SDL_GetWindowSizeInPixels(window_.get(), &extent.width, &extent.height);
        return extent;
    }

    auto Window::presentation_state() const -> WindowPresentationState
    {
        const PixelExtent extent = pixel_extent();
        const SDL_WindowFlags flags = SDL_GetWindowFlags(window_.get());
        return WindowPresentationState{
            .pixel_width = extent.width,
            .pixel_height = extent.height,
            .minimized = (flags & SDL_WINDOW_MINIMIZED) != 0,
            .hidden = (flags & SDL_WINDOW_HIDDEN) != 0,
        };
    }

    auto Window::relative_mouse_mode() const -> bool
    {
        return SDL_GetWindowRelativeMouseMode(window_.get());
    }

    auto Window::refresh_rate_hz() const -> float
    {
        const SDL_DisplayID display_id = SDL_GetDisplayForWindow(window_.get());
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
        return require_gl_context().swap_interval();
    }

    auto Window::vsync_enabled() const -> bool
    {
        return swap_interval() != 0;
    }

    auto Window::set_swap_interval(const int interval) -> bool
    {
        return require_gl_context().set_swap_interval(interval);
    }

    auto Window::toggle_vsync() -> bool
    {
        return require_gl_context().toggle_vsync();
    }

    void Window::set_relative_mouse_mode(const bool enabled) const
    {
        core::require_sdl(SDL_SetWindowRelativeMouseMode(window_.get(), enabled), "Failed to set relative mouse mode");
    }

    void Window::swap_buffers() const
    {
        require_gl_context().swap_buffers();
    }
} // namespace RelayCore
