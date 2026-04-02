/**
 * @file src/relaycore/app/app_delegate.h
 * @brief Game-side lifecycle interface consumed by the engine runtime.
 */

#pragma once

#include <SDL3/SDL.h>

#include "relaycore/app/frame_timer.h"

namespace RelayCore
{
    class EngineServices;

    /**
     * @brief Receives lifecycle callbacks from the engine runtime.
     *
     * The engine owns the main loop and invokes an application delegate for
     * startup, input processing, per-frame updates, rendering, and shutdown.
     */
    class AppDelegate {
    public:
        /**
         * @brief Destroys the delegate.
         */
        virtual ~AppDelegate() = default;

        /**
         * @brief Called once after the runtime has initialized core systems.
         * @param services Runtime services exposed to the game layer.
         */
        virtual void on_startup(EngineServices& /*services*/) {}

        /**
         * @brief Called for every SDL event pumped by the application.
         * @param event SDL event that was received from the queue.
         * @param services Runtime services exposed to the game layer.
         */
        virtual void on_event(const SDL_Event& /*event*/, EngineServices& /*services*/) {}

        /**
         * @brief Called once per frame before rendering.
         * @param frame Frame timing and frame-index information.
         * @param services Runtime services exposed to the game layer.
         */
        virtual void on_update(const FrameInfo& /*frame*/, EngineServices& /*services*/) {}

        /**
         * @brief Called once per frame after the renderer begins the frame.
         * @param frame Frame timing and frame-index information.
         * @param services Runtime services exposed to the game layer.
         */
        virtual void on_render(const FrameInfo& /*frame*/, EngineServices& /*services*/) {}

        /**
         * @brief Called during shutdown before engine systems are torn down.
         * @param services Runtime services exposed to the game layer.
         */
        virtual void on_shutdown(EngineServices& /*services*/) noexcept {}
    };

    /**
     * @brief No-op delegate used when the caller does not supply one.
     */
    class NullAppDelegate final : public AppDelegate {};
} // namespace RelayCore
