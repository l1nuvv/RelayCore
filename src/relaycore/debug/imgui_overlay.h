/**
 * @file src/relaycore/debug/imgui_overlay.h
 * @brief Dear ImGui debug overlay wrapper.
 */

#pragma once

#include <SDL3/SDL.h>

#include "relaycore/app/frame_timer.h"

namespace RelayCore
{
    class Window;
    class AudioSystem;
} // namespace RelayCore

namespace RelayCore::render
{
    class PerspectiveCamera;
}

namespace RelayCore::debug
{
    /**
     * @brief Configuration options for the debug ImGui overlay.
     */
    struct ImGuiOverlayConfig {
        bool enabled          = true; ///< Enables overlay initialization and rendering.
        bool show_demo_window = false; ///< Requests that the built-in ImGui demo window be shown.
    };

    /**
     * @brief Owns Dear ImGui state and renders the engine diagnostics overlay.
     */
    class ImGuiOverlay {
    public:
        /**
         * @brief Creates an overlay with the supplied configuration.
         * @param config Overlay configuration.
         */
        explicit ImGuiOverlay(const ImGuiOverlayConfig& config = ImGuiOverlayConfig{});

        /**
         * @brief Shuts down ImGui state if it was initialized.
         */
        ~ImGuiOverlay() noexcept;

        ImGuiOverlay(const ImGuiOverlay&) = delete; ///< ImGuiOverlay is not copyable.

        /**
         * @brief Disables copy assignment.
         * @return Reference to this object.
         */
        auto operator=(const ImGuiOverlay&) -> ImGuiOverlay& = delete;

        /**
         * @brief Initializes ImGui for the supplied window and GL context.
         * @param window Window that owns the active OpenGL context.
         */
        void initialize(const Window& window);

        /**
         * @brief Shuts down ImGui backends and destroys the context.
         */
        void shutdown() noexcept;

        /**
         * @brief Forwards an SDL event to ImGui.
         * @param event SDL event to process.
         */
        void process_event(const SDL_Event& event);

        /**
         * @brief Starts a new ImGui frame.
         */
        void begin_frame();

        /**
         * @brief Draws runtime diagnostics into the overlay.
         * @param frame Frame timing information.
         * @param camera Camera being used for rendering.
         * @param audio_system Audio system whose state is displayed.
         */
        void draw_diagnostics(
            const FrameInfo& frame, const render::PerspectiveCamera& camera, const AudioSystem& audio_system
        );

        /**
         * @brief Renders the accumulated ImGui draw data.
         */
        void render() const;

        /**
         * @brief Reports whether the overlay has been initialized.
         * @return True when the ImGui context and backends are live.
         */
        [[nodiscard]] auto is_initialized() const noexcept -> bool;

        /**
         * @brief Reports whether the overlay is enabled by configuration.
         * @return True when overlay rendering is allowed.
         */
        [[nodiscard]] auto enabled() const noexcept -> bool;

    private:
        ImGuiOverlayConfig config_{}; ///< Stored overlay configuration.
        bool               initialized_ = false; ///< True after successful backend initialization.
    };
} // namespace RelayCore::debug
