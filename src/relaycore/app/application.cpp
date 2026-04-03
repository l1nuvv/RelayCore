/**
 * @file src/relaycore/app/application.cpp
 * @brief Application main-loop implementation.
 */

#include "relaycore/app/application.h"

#include <SDL3/SDL.h>

#include "relaycore/core/log.h"
#include "relaycore/core/paths.h"
#include "relaycore/core/profiling.h"

namespace RelayCore
{
    namespace
    {
        /**
         * @brief Ensures the active delegate receives on_shutdown() exactly once.
         */
        class AppShutdownGuard {
        public:
            /**
             * @brief Stores the delegate and services that must be shut down together.
             * @param delegate Delegate that will receive the shutdown callback.
             * @param services Runtime services passed to the delegate.
             */
            AppShutdownGuard(AppDelegate& delegate, EngineServices& services) noexcept :
                delegate_(delegate),
                services_(services) {}

            /**
             * @brief Invokes the delegate shutdown callback.
             */
            ~AppShutdownGuard() noexcept { delegate_.on_shutdown(services_); }

            AppShutdownGuard(const AppShutdownGuard&) = delete;

            auto operator=(const AppShutdownGuard&) -> AppShutdownGuard& = delete;

        private:
            AppDelegate&    delegate_; ///< Delegate that owns shutdown behaviour.
            EngineServices& services_; ///< Runtime services forwarded to the delegate.
        };
    } // namespace

    Application::Application(std::unique_ptr<AppDelegate> app_delegate) :
        app_delegate_(app_delegate ? std::move(app_delegate) : std::make_unique<NullAppDelegate>()),
        config_(core::load_default_engine_config()),
        window_(config_.window),
        renderer_(config_.renderer),
        audio_system_(config_.audio)
    {
        renderer_.initialize(window_);

        const PixelExtent extent = window_.pixel_extent();
        renderer_.on_resize(extent.width, extent.height);

        audio_system_.initialize();
        if (!config_.audio.startup_sound.empty())
        {
            audio_system_.play_one_shot(core::asset_path(config_.audio.startup_sound));
        }

        log::info("Application initialized");
    }

    auto Application::run() -> int
    {
        RELAYCORE_PROFILE_ZONE_N("Application::run");

        EngineServices   services{config_, window_, renderer_, audio_system_, is_running_};
        AppShutdownGuard shutdown_guard(*app_delegate_, services);

        app_delegate_->on_startup(services);

        while (is_running_)
        {
            const FrameInfo frame = frame_timer_.begin_frame(); {
                RELAYCORE_PROFILE_ZONE_N("process_events");
                process_events(services);
            } {
                RELAYCORE_PROFILE_ZONE_N("app_delegate.on_update");
                app_delegate_->on_update(frame, services);
            } {
                RELAYCORE_PROFILE_ZONE_N("renderer.begin_frame");
                renderer_.begin_frame();
            } {
                RELAYCORE_PROFILE_ZONE_N("app_delegate.on_render");
                app_delegate_->on_render(frame, services);
            } {
                RELAYCORE_PROFILE_ZONE_N("renderer.draw_debug_overlay");
                renderer_.draw_debug_overlay(frame, audio_system_);
            } {
                RELAYCORE_PROFILE_ZONE_N("swap_buffers");
                window_.swap_buffers();
            }

            RELAYCORE_PROFILE_FRAME();
        }

        return 0;
    }

    void Application::process_events(EngineServices& services)
    {
        SDL_Event event{};
        while (SDL_PollEvent(&event)) { handle_event(event, services); }
    }

    void Application::handle_event(const SDL_Event& event, EngineServices& services)
    {
        renderer_.process_event(event);

        switch (event.type)
        {
            case SDL_EVENT_QUIT: services.request_quit();
                break;

            case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED: renderer_.on_resize(event.window.data1, event.window.data2);
                log::debug("Window pixel size changed: {}x{}", event.window.data1, event.window.data2);
                break;

            default: break;
        }

        app_delegate_->on_event(event, services);
    }
} // namespace RelayCore
