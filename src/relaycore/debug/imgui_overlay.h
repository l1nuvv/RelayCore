/**
 * @file src/relaycore/debug/imgui_overlay.h
 * @brief Dear ImGui debug overlay wrapper.
 */

#pragma once

#include <SDL3/SDL.h>
#include <array>
#include <cstddef>

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
    struct ImGuiOverlayConfig {
        bool  enabled                  = true;
        bool  show_demo_window         = false;
        bool  log_fps_to_console       = true;
        float fps_log_interval_seconds = 1.0F;
    };

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
            const FrameInfo& frame, const render::PerspectiveCamera& camera, const AudioSystem& audio_system
        );

        void render() const;

        [[nodiscard]] auto is_initialized() const noexcept -> bool;

        [[nodiscard]] auto enabled() const noexcept -> bool;

    private:
        static constexpr std::size_t kFrameTimeSampleCount = 120;

        void push_frame_time_sample(float frame_time_ms) noexcept;

        [[nodiscard]] auto average_frame_time_ms() const noexcept -> float;

        [[nodiscard]] static auto vsync_label(int swap_interval) -> const char*;

        ImGuiOverlayConfig                       config_{};
        const Window*                            window_              = nullptr;
        bool                                     initialized_         = false;
        float                                    fps_log_accumulator_ = 0.0F;
        std::array<float, kFrameTimeSampleCount> frame_time_samples_{};
        std::size_t                              frame_time_sample_count_  = 0;
        std::size_t                              frame_time_sample_cursor_ = 0;
    };
} // namespace RelayCore::debug
