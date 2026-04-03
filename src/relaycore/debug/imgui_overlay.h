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
        bool  enabled                  = true; ///< Enables overlay initialization and rendering.
        bool  show_demo_window         = false; ///< Requests that the built-in ImGui demo window be shown.
        bool  log_fps_to_console       = true; ///< Writes FPS to the engine log periodically.
        float fps_log_interval_seconds = 1.0F; ///< Interval between FPS log entries.
    };

    /**
     * @brief Owns Dear ImGui state and renders the engine diagnostics overlay.
     */
    class ImGuiOverlay {
    public:
        explicit ImGuiOverlay(const ImGuiOverlayConfig& config = ImGuiOverlayConfig{});

        ~ImGuiOverlay() noexcept;

        ImGuiOverlay(const ImGuiOverlay&) = delete;

        auto operator=(const ImGuiOverlay&) -> ImGuiOverlay& = delete;

        void initialize(const Window& window);

        void shutdown() noexcept;

        void process_event(const SDL_Event& event);

        void begin_frame();

        void draw_diagnostics(
            const FrameInfo&                 frame,
            const render::PerspectiveCamera& camera,
            const AudioSystem&               audio_system
        );

        void render() const;

        [[nodiscard]] auto is_initialized() const noexcept -> bool;

        [[nodiscard]] auto enabled() const noexcept -> bool;

    private:
        ImGuiOverlayConfig config_{};
        bool               initialized_         = false;
        float              fps_log_accumulator_ = 0.0F;
    };
} // namespace RelayCore::debug
