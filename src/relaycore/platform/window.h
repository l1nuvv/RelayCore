/**
 * @file src/relaycore/platform/window.h
 * @brief SDL window ownership for Vulkan presentation.
 */

#pragma once

#include <SDL3/SDL.h>
#include <memory>
#include <span>
#include <string>

namespace RelayCore
{

    /**
     * @brief Parameters used to create the main application window.
     */
    struct WindowConfig {
        std::string title = "RelayCore"; // Window title text.
        int width = 1280;                // Requested window width in logical units.
        int height = 720;                // Requested window height in logical units.
        bool high_dpi = true;            // Requests a high-DPI-capable window.
        bool vsync = true;               // Requests synchronized presentation.
    };

    /**
     * @brief Pixel size of a window framebuffer.
     */
    struct PixelExtent {
        int width = 0;  // Framebuffer width in pixels.
        int height = 0; // Framebuffer height in pixels.
    };

    /**
     * @brief Current window properties that determine whether presentation is possible.
     */
    struct WindowPresentationState {
        int pixel_width = 0;    // Current drawable width in pixels.
        int pixel_height = 0;   // Current drawable height in pixels.
        bool minimized = false; // True while the window is minimized.
        bool hidden = false;    // True while the window is hidden.
    };

    /**
     * @brief Reports whether a window has a drawable surface suitable for presentation.
     */
    [[nodiscard]] constexpr auto can_present(const WindowPresentationState& state) noexcept -> bool
    {
        return state.pixel_width > 0 && state.pixel_height > 0 && !state.minimized && !state.hidden;
    }

    // SDL window and its Vulkan surface requirements; renderer owns presentation.
    class Window {
    public:
        explicit Window(const WindowConfig& config);

        // SDL owns the returned names; consume them while this Vulkan window is alive.
        [[nodiscard]] auto vulkan_instance_extensions() const -> std::span<const char* const>;

        ~Window() noexcept;

        Window(const Window&) = delete; // Window is not copyable.

        auto operator=(const Window&) -> Window& = delete;

        Window(Window&&) = delete; // Window is not movable.

        auto operator=(Window&&) -> Window& = delete;

        [[nodiscard]] auto handle() const noexcept -> SDL_Window*;

        [[nodiscard]] auto pixel_extent() const -> PixelExtent;

        [[nodiscard]] auto presentation_state() const -> WindowPresentationState;

        [[nodiscard]] auto relative_mouse_mode() const -> bool;

        [[nodiscard]] auto refresh_rate_hz() const -> float;

        [[nodiscard]] auto vsync_enabled() const -> bool;

        [[nodiscard]] auto toggle_vsync() -> bool;

        void set_relative_mouse_mode(bool enabled) const;

    private:
        std::unique_ptr<SDL_Window, decltype(&SDL_DestroyWindow)> window_{nullptr, SDL_DestroyWindow};
        bool vsync_requested_ = true;
    };
} // namespace RelayCore
