#pragma once

#include <SDL3/SDL.h>

#include "relaycore/app/frame_timer.hpp"

namespace RelayCore
{
    class EngineServices;

    class AppDelegate {
    public:
        virtual ~AppDelegate() = default;

        virtual void on_startup(EngineServices& /*services*/) {}

        virtual void on_event(const SDL_Event& /*event*/, EngineServices& /*services*/) {}

        virtual void on_update(const FrameInfo& /*frame*/, EngineServices& /*services*/) {}

        virtual void on_render(const FrameInfo& /*frame*/, EngineServices& /*services*/) {}

        virtual void on_shutdown(EngineServices& /*services*/) noexcept {}
    };

    class NullAppDelegate final : public AppDelegate {};
} // namespace RelayCore
