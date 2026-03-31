#include "app/application.hpp"

#include "core/log.hpp"
#include "core/paths.hpp"
#include "core/profiling.hpp"

#include <SDL3/SDL.h>

namespace RelayCore
{
    namespace
    {
        class AppShutdownGuard {
        public:
            explicit AppShutdownGuard(AppDelegate& delegate) :
                delegate_(delegate) {}

            ~AppShutdownGuard() noexcept { delegate_.on_shutdown(); }

            AppShutdownGuard(const AppShutdownGuard&) = delete;

            auto operator=(const AppShutdownGuard&) -> AppShutdownGuard& = delete;

        private:
            AppDelegate& delegate_;
        };
    } // namespace

    Application::Application(std::unique_ptr<AppDelegate> app_delegate) :
        app_delegate_(std::move(app_delegate)),
        config_(core::load_default_engine_config()),
        window_(config_.window),
        renderer_(config_.renderer),
        fly_camera_controller_(config_.input),
        audio_system_(config_.audio)
    {
        renderer_.initialize(window_);

        const PixelExtent extent = window_.pixel_extent();
        renderer_.on_resize(extent.width, extent.height);
        fly_camera_controller_.enable_capture(window_);

        audio_system_.initialize();
        if (!config_.audio.startup_sound.empty())
        {
            audio_system_.play_one_shot(core::asset_path(config_.audio.startup_sound));
        }

        app_delegate_->on_startup();

        log::info("Application initialized");
    }

    auto Application::run() -> int
    {
        RELAYCORE_PROFILE_ZONE_N("Application::run");
        AppShutdownGuard shutdown_guard(*app_delegate_);

        while (is_running_)
        {
            const FrameInfo frame = frame_timer_.begin_frame(); {
                RELAYCORE_PROFILE_ZONE_N("process_events");
                process_events();
            } {
                RELAYCORE_PROFILE_ZONE_N("camera.update");
                fly_camera_controller_.update(frame, renderer_.camera());
            } {
                RELAYCORE_PROFILE_ZONE_N("app_delegate.on_update");
                app_delegate_->on_update(frame);
            } {
                RELAYCORE_PROFILE_ZONE_N("renderer.render_frame");
                renderer_.render_frame(frame, audio_system_);
            } {
                RELAYCORE_PROFILE_ZONE_N("swap_buffers");
                window_.swap_buffers();
            }

            RELAYCORE_PROFILE_FRAME();
        }

        return 0;
    }

    void Application::process_events()
    {
        SDL_Event event{};
        while (SDL_PollEvent(&event)) { handle_event(event); }
    }

    void Application::handle_event(const SDL_Event& event)
    {
        renderer_.process_event(event);

        switch (event.type)
        {
            case SDL_EVENT_QUIT: is_running_ = false;
                break;

            case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED: renderer_.on_resize(event.window.data1, event.window.data2);
                log::debug("Window pixel size changed: {}x{}", event.window.data1, event.window.data2);
                break;

            default: break;
        }

        fly_camera_controller_.on_event(event, window_, is_running_);
        app_delegate_->on_event(event);
    }
} // namespace RelayCore
