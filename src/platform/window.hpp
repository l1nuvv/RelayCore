#pragma once

#include <SDL3/SDL.h>

#include <string>

namespace RelayCore
{
    struct WindowConfig {
        std::string title    = "RelayCore";
        int         width    = 1280;
        int         height   = 720;
        bool        high_dpi = true;
        bool        vsync    = true;
    };

    struct PixelExtent {
        int width  = 0;
        int height = 0;
    };

    class Window {
    public:
        explicit Window(const WindowConfig& config);

        ~Window() noexcept;

        Window(const Window&) = delete;

        auto operator=(const Window&) -> Window& = delete;

        Window(Window&&) = delete;

        auto operator=(Window&&) -> Window& = delete;

        [[nodiscard]] auto handle() const noexcept -> SDL_Window*;

        [[nodiscard]] auto gl_context() const noexcept -> SDL_GLContext;

        [[nodiscard]] auto pixel_extent() const -> PixelExtent;

        [[nodiscard]] auto relative_mouse_mode() const -> bool;

        void set_relative_mouse_mode(bool enabled) const;

        void swap_buffers() const;

    private:
        void log_context_info() const;

        SDL_Window*   window_     = nullptr;
        SDL_GLContext gl_context_ = nullptr;
    };
} // namespace RelayCore
