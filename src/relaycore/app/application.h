/**
 * @file src/relaycore/app/application.h
 * @brief High-level engine application wrapper and main-loop owner.
 */

#pragma once

#include <memory>

#include "relaycore/app/app_delegate.h"
#include "relaycore/app/engine_services.h"
#include "relaycore/app/frame_timer.h"
#include "relaycore/audio/audio_system.h"
#include "relaycore/core/engine_config.h"
#include "relaycore/platform/sdl_system.h"
#include "relaycore/platform/window.h"
#include "relaycore/render/renderer.h"

namespace RelayCore
{
    /**
     * @brief Owns the engine runtime and drives the main loop.
     *
     * Application wires together SDL, the window, renderer, audio, frame timing,
     * and the active game delegate.
     */
    class Application {
    public:
        /**
         * @brief Creates an application instance around a delegate.
         * @param app_delegate Delegate that receives lifecycle callbacks.
         */
        explicit Application(std::unique_ptr<AppDelegate> app_delegate = std::make_unique<NullAppDelegate>());

        /**
         * @brief Runs the application until shutdown is requested.
         * @return Process exit code.
         */
        [[nodiscard]] auto run() -> int;

    private:
        /**
         * @brief Polls and dispatches SDL events for the current frame.
         * @param services Runtime services exposed to the game layer.
         */
        void process_events(EngineServices& services);

        /**
         * @brief Routes one SDL event through engine and game handlers.
         * @param event Event received from SDL.
         * @param services Runtime services exposed to the game layer.
         */
        void handle_event(const SDL_Event& event, EngineServices& services);

        std::unique_ptr<AppDelegate> app_delegate_; ///< Active game delegate.
        FrameTimer                   frame_timer_{}; ///< Frame timing helper.
        SdlSystem                    sdl_system_{}; ///< SDL lifetime guard.
        core::EngineConfig           config_{}; ///< Loaded engine configuration.
        Window                       window_; ///< Native window and GL context owner.
        Renderer                     renderer_{}; ///< Rendering subsystem.
        AudioSystem                  audio_system_{}; ///< Audio subsystem.
        bool                         is_running_ = true; ///< Main-loop running flag.
    };
} // namespace RelayCore
