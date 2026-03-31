#pragma once

#include "app/frame_timer.hpp"

#include <SDL3/SDL.h>

namespace RelayCore
{
    class Window;
    class AudioSystem;
}

namespace RelayCore::render
{
    class PerspectiveCamera;
}

namespace RelayCore::debug
{
    struct ImGuiOverlayConfig {
        bool enabled          = true;
        bool show_demo_window = false;
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

        void draw_diagnostics(const FrameInfo&                 frame,
                              const render::PerspectiveCamera& camera,
                              const AudioSystem&               audio_system);

        void render() const;

        [[nodiscard]] auto is_initialized() const noexcept -> bool;

        [[nodiscard]] auto enabled() const noexcept -> bool;

    private:
        ImGuiOverlayConfig config_{};
        bool               initialized_ = false;
    };
} // namespace RelayCore::debug
