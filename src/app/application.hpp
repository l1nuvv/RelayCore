#pragma once

#include "app/app_delegate.hpp"
#include "app/fly_camera_controller.hpp"
#include "app/frame_timer.hpp"
#include "audio/audio_system.hpp"
#include "core/engine_config.hpp"
#include "platform/sdl_system.hpp"
#include "platform/window.hpp"
#include "render/renderer.hpp"

#include <memory>

namespace RelayCore
{
    class Application {
    public:
        explicit Application(std::unique_ptr<AppDelegate> app_delegate = std::make_unique<NullAppDelegate>());

        [[nodiscard]] auto run() -> int;

    private:
        void process_events();

        void handle_event(const SDL_Event& event);

        std::unique_ptr<AppDelegate> app_delegate_;
        FrameTimer                   frame_timer_{};
        SdlSystem                    sdl_system_{};
        core::EngineConfig           config_{};
        Window                       window_;
        Renderer                     renderer_{};
        FlyCameraController          fly_camera_controller_{};
        AudioSystem                  audio_system_{};
        bool                         is_running_ = true;
    };
} // namespace RelayCore
