/**
 * @file src/relaygame/demo_game.h
 * @brief Demo/game-side delegate used by the RelayCore executable.
 */

#pragma once

#include <memory>

#include "relaycore/app/app_delegate.h"
#include "relaycore/app/fly_camera_controller.h"
#include "relaycore/world/blockout_level.h"

namespace RelayGame
{
    /**
     * @brief Minimal demo delegate that loads and draws a blockout level.
     */
    class DemoGame final : public RelayCore::AppDelegate {
    public:
        /**
         * @brief Loads startup state for the demo scene.
         * @param services Runtime services exposed by the engine.
         */
        void on_startup(RelayCore::EngineServices& services) override;

        /**
         * @brief Forwards relevant SDL events to the fly camera.
         * @param event SDL event received from the application.
         * @param services Runtime services exposed by the engine.
         */
        void on_event(const SDL_Event& event, RelayCore::EngineServices& services) override;

        /**
         * @brief Updates camera motion for the current frame.
         * @param frame Current frame timing information.
         * @param services Runtime services exposed by the engine.
         */
        void on_update(const RelayCore::FrameInfo& frame, RelayCore::EngineServices& services) override;

        /**
         * @brief Draws the loaded blockout level using debug cubes.
         * @param frame Current frame timing information.
         * @param services Runtime services exposed by the engine.
         */
        void on_render(const RelayCore::FrameInfo& frame, RelayCore::EngineServices& services) override;

        /**
         * @brief Logs demo shutdown.
         * @param services Runtime services exposed by the engine.
         */
        void on_shutdown(RelayCore::EngineServices& services) noexcept override;

    private:
        RelayCore::FlyCameraController fly_camera_{}; ///< Fly-camera input controller.
        RelayCore::BlockoutLevel       level_{}; ///< Loaded blockout level data.
        float                          cube_angle_a_ = 0.0F; ///< Legacy debug cube angle retained for compatibility.
        float                          cube_angle_b_ = 0.0F; ///< Legacy debug cube angle retained for compatibility.
    };

    /**
     * @brief Creates the default demo game delegate.
     * @return Unique pointer to a demo delegate instance.
     */
    [[nodiscard]] auto make_demo_game() -> std::unique_ptr<RelayCore::AppDelegate>;
} // namespace RelayGame
