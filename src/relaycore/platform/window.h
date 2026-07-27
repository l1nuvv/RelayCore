/**
 * @file src/relaycore/platform/window.h
 * @brief SDL window and OpenGL context ownership wrapper.
 */

#pragma once

#include "relaycore/render/color_pipeline_mode.h"

#include <SDL3/SDL.h>
#include <string>

namespace RelayCore
{
    /**
     * @brief Parameters used to create the main application window.
     */
    struct WindowConfig {
        std::string title = "RelayCore"; ///< Window title text.
        int width = 1280;                ///< Requested window width in logical units.
        int height = 720;                ///< Requested window height in logical units.
        bool high_dpi = true;            ///< Requests a high-DPI-capable window.
        bool vsync = true;               ///< Enables swap-interval synchronization.
    };

    /**
     * @brief Pixel size of a window framebuffer.
     */
    struct PixelExtent {
        int width = 0;  ///< Framebuffer width in pixels.
        int height = 0; ///< Framebuffer height in pixels.
    };

    /**
     * @brief Current window properties that determine whether OpenGL presentation is possible.
     */
    struct WindowPresentationState {
        int pixel_width = 0;    ///< Current drawable width in pixels.
        int pixel_height = 0;   ///< Current drawable height in pixels.
        bool minimized = false; ///< True while the window is minimized.
        bool hidden = false;    ///< True while the window is hidden.
    };

    /**
     * @brief Reports whether a window has a drawable surface suitable for presentation.
     */
    [[nodiscard]] constexpr auto can_present(const WindowPresentationState& state) noexcept -> bool
    {
        return state.pixel_width > 0 && state.pixel_height > 0 && !state.minimized && !state.hidden;
    }

    /**
     * @brief Owns an SDL window together with its OpenGL context.
     */
    class Window {
    public:
        /**
         * @brief Creates the SDL window and its OpenGL context.
         * @param config Window creation parameters.
         */
        explicit Window(
            const WindowConfig& config,
            render::ColorPipelineMode color_pipeline_mode = render::ColorPipelineMode::legacy_gamma
        );

        [[nodiscard]] auto framebuffer_srgb_capable() const noexcept -> int;

        /**
         * @brief Destroys the OpenGL context and window.
         */
        ~Window() noexcept;

        Window(const Window&) = delete; ///< Window is not copyable.

        /**
         * @brief Disables copy assignment.
         * @return Reference to this object.
         */
        auto operator=(const Window&) -> Window& = delete;

        Window(Window&&) = delete; ///< Window is not movable.

        /**
         * @brief Disables move assignment.
         * @return Reference to this object.
         */
        auto operator=(Window&&) -> Window& = delete;

        /**
         * @brief Returns the raw SDL window handle.
         * @return SDL window handle.
         */
        [[nodiscard]] auto handle() const noexcept -> SDL_Window*;

        /**
         * @brief Returns the raw SDL OpenGL context handle.
         * @return SDL GL context.
         */
        [[nodiscard]] auto gl_context() const noexcept -> SDL_GLContext;

        /**
         * @brief Returns the current framebuffer pixel extent.
         * @return Window framebuffer pixel size.
         */
        [[nodiscard]] auto pixel_extent() const -> PixelExtent;

        /**
         * @brief Returns the current presentation-relevant window state.
         * @return Drawable pixel extent and minimized/hidden flags.
         */
        [[nodiscard]] auto presentation_state() const -> WindowPresentationState;

        /**
         * @brief Reports whether relative mouse mode is enabled for the window.
         * @return True when SDL relative mouse mode is active.
         */
        [[nodiscard]] auto relative_mouse_mode() const -> bool;

        /**
         * @brief Returns the active display refresh rate for the window.
         * @return Refresh rate in hertz, or 0 when unavailable.
         */
        [[nodiscard]] auto refresh_rate_hz() const -> float;

        /**
         * @brief Returns the active OpenGL swap interval.
         * @return Swap interval value reported by SDL.
         */
        [[nodiscard]] auto swap_interval() const -> int;

        /**
         * @brief Reports whether swap-interval synchronization is enabled.
         * @return True when the swap interval is non-zero.
         */
        [[nodiscard]] auto vsync_enabled() const -> bool;

        [[nodiscard]] auto set_swap_interval(int interval) -> bool;

        [[nodiscard]] auto toggle_vsync() -> bool;

        /**
         * @brief Enables or disables relative mouse mode for the window.
         * @param enabled Desired relative mouse mode state.
         */
        void set_relative_mouse_mode(bool enabled) const;

        /**
         * @brief Swaps the front and back buffers of the OpenGL window.
         */
        void swap_buffers() const;

    private:
        /**
         * @brief Logs OpenGL context information after successful creation.
         */
        void log_context_info() const;

        SDL_Window* window_ = nullptr;       ///< Owned SDL window handle.
        SDL_GLContext gl_context_ = nullptr; ///< Owned OpenGL context handle.
        int framebuffer_srgb_capable_ = 0;
    };
} // namespace RelayCore
