/**
 * @file src/relaycore/app/fly_camera_controller.h
 * @brief Input-driven free-flight camera controller.
 */

#pragma once

#include <SDL3/SDL.h>

#include "relaycore/app/frame_timer.h"

namespace RelayCore
{
    class EngineServices;
    class Window;
} // namespace RelayCore

namespace RelayCore::render
{
    class PerspectiveCamera;
}

namespace RelayCore
{
    /**
     * @brief Tuning parameters for a free-flight debug camera.
     */
    struct FlyCameraControllerConfig {
        float move_speed                = 3.5F;    ///< Base movement speed in world units per second.
        float sprint_multiplier         = 3.0F;    ///< Multiplier applied while sprint is held.
        float mouse_sensitivity_radians = 0.0022F; ///< Mouse delta to radians conversion factor.
        bool  capture_mouse_on_start    = true;    ///< Enables relative mouse mode during startup.
    };

    /**
     * @brief Converts SDL mouse and keyboard input into camera motion.
     */
    class FlyCameraController {
    public:
        /**
         * @brief Creates a controller with the supplied configuration.
         * @param config Controller tuning parameters.
         */
        explicit FlyCameraController(const FlyCameraControllerConfig& config = FlyCameraControllerConfig{});

        /**
         * @brief Enables relative mouse capture when configured to do so.
         * @param window Window that owns the mouse capture state.
         */
        void enable_capture(Window& window) const;

        /**
         * @brief Consumes an SDL event relevant to camera control.
         * @param event SDL event received from the main loop.
         * @param window Window that owns the mouse capture state.
         * @param services Runtime services used for side effects such as quit requests.
         */
        void on_event(const SDL_Event& event, Window& window, EngineServices& services);

        /**
         * @brief Advances the camera using accumulated input for the frame.
         * @param frame Frame timing information.
         * @param camera Camera to update.
         */
        void update(const FrameInfo& frame, render::PerspectiveCamera& camera);

        /**
         * @brief Returns the immutable controller configuration.
         * @return Controller configuration.
         */
        [[nodiscard]] auto config() const noexcept -> const FlyCameraControllerConfig&;

    private:
        FlyCameraControllerConfig config_{};                   ///< Current controller tuning.
        float                     pending_yaw_delta_   = 0.0F; ///< Mouse yaw accumulated since the previous update.
        float                     pending_pitch_delta_ = 0.0F; ///< Mouse pitch accumulated since the previous update.
    };
} // namespace RelayCore
