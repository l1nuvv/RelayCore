#pragma once

#include <SDL3/SDL.h>

#include "relaycore/app/frame_timer.hpp"

namespace RelayCore
{
    class EngineServices;
    class Window;
}

namespace RelayCore::render
{
    class PerspectiveCamera;
}

namespace RelayCore
{
    struct FlyCameraControllerConfig {
        float move_speed                = 3.5F;
        float sprint_multiplier         = 3.0F;
        float mouse_sensitivity_radians = 0.0022F;
        bool  capture_mouse_on_start    = true;
    };

    class FlyCameraController {
    public:
        explicit FlyCameraController(const FlyCameraControllerConfig& config = FlyCameraControllerConfig{});

        void enable_capture(Window& window) const;

        void on_event(const SDL_Event& event, Window& window, EngineServices& services);

        void update(const FrameInfo& frame, render::PerspectiveCamera& camera);

        [[nodiscard]] auto config() const noexcept -> const FlyCameraControllerConfig&;

    private:
        FlyCameraControllerConfig config_{};
        float                     pending_yaw_delta_   = 0.0F;
        float                     pending_pitch_delta_ = 0.0F;
    };
} // namespace RelayCore
