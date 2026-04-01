#pragma once

#include <memory>

#include "relaycore/app/app_delegate.hpp"
#include "relaycore/app/engine_services.hpp"
#include "relaycore/app/frame_timer.hpp"
#include "relaycore/audio/audio_system.hpp"
#include "relaycore/core/engine_config.hpp"
#include "relaycore/platform/sdl_system.hpp"
#include "relaycore/platform/window.hpp"
#include "relaycore/render/renderer.hpp"

namespace RelayCore
{
    class Application {
    public:
        explicit Application(std::unique_ptr<AppDelegate> app_delegate = std::make_unique<NullAppDelegate>());

        [[nodiscard]] auto run() -> int;

    private:
        void process_events(EngineServices& services);

        void handle_event(const SDL_Event& event, EngineServices& services);

        std::unique_ptr<AppDelegate> app_delegate_;
        FrameTimer                   frame_timer_{};
        SdlSystem                    sdl_system_{};
        core::EngineConfig           config_{};
        Window                       window_;
        Renderer                     renderer_{};
        AudioSystem                  audio_system_{};
        bool                         is_running_ = true;
    };
} // namespace RelayCore
