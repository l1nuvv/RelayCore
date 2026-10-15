/**
 * @file src/relaycore/app/application.cpp
 * @brief Application main-loop implementation.
 */

#include "relaycore/app/application.h"

#include "relaycore/core/log.h"
#include "relaycore/core/paths.h"
#include "relaycore/core/profiling.h"

#include <SDL3/SDL.h>
#include <chrono>

namespace RelayCore
{
    namespace
    {
        constexpr Sint32 kNonPresentableEventWaitTimeoutMs = 20;

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
                delegate_(delegate), services_(services)
            {}

            /**
             * @brief Invokes the delegate shutdown callback.
             */
            ~AppShutdownGuard() noexcept
            {
                try
                {
                    services_.frame_diagnostics().flush();
                } catch (...)
                {}
                delegate_.on_shutdown(services_);
            }

            AppShutdownGuard(const AppShutdownGuard&) = delete;

            auto operator=(const AppShutdownGuard&) -> AppShutdownGuard& = delete;

        private:
            AppDelegate& delegate_;    ///< Delegate that owns shutdown behaviour.
            EngineServices& services_; ///< Runtime services forwarded to the delegate.
        };
    } // namespace

    Application::Application(
        std::unique_ptr<AppDelegate> app_delegate,
        std::filesystem::path config_path,
        const render::ColorPipelineMode color_pipeline_mode
    ) : app_delegate_(app_delegate ? std::move(app_delegate) : std::make_unique<NullAppDelegate>())
    {
        RELAYCORE_PROFILE_FUNCTION();
        config_.emplace(
            config_path.empty() ? core::load_default_engine_config() : core::load_engine_config(config_path)
        );
        window_.emplace(config_->window);
        renderer_.emplace(config_->renderer, color_pipeline_mode);
        audio_system_.emplace(config_->audio);

        renderer_->initialize(*window_);

        const PixelExtent extent = window_->pixel_extent();
        renderer_->on_resize(extent.width, extent.height);

        audio_system_->initialize();
        if (!config_->audio.startup_sound.empty())
        {
            audio_system_->play_one_shot(core::asset_path(config_->audio.startup_sound));
        }

        log::info("[app] Application initialized");
    }

    auto Application::run() -> int
    {
        RELAYCORE_PROFILE_ZONE_N("Application::run");

        using Clock = std::chrono::steady_clock;

        EngineServices services{*config_, *window_, *renderer_, *audio_system_, is_running_};
        AppShutdownGuard shutdown_guard(*app_delegate_, services);
        app_delegate_->on_startup(services);
        auto& diagnostics = services.frame_diagnostics();

        while (is_running_)
        {
            RELAYCORE_PROFILE_ZONE_N("Application::run_frame");
            diagnostics.begin_frame();
            const auto finish_diagnostics = [&] {
                const auto& settings = renderer_->config().imgui;
                diagnostics.end_frame(settings.log_fps_to_console, settings.fps_log_interval_seconds);
            };

            {
                RELAYCORE_PROFILE_ZONE_N("process_events");
                process_events(services, !can_present(window_->presentation_state()));
            }
            if (!is_running_)
            {
                break;
            }

            const WindowPresentationState presentation_state = window_->presentation_state();
            const FrameInfo frame = frame_timer_.begin_frame();
            const auto cpu_work_start = Clock::now();
            {
                RELAYCORE_PROFILE_ZONE_N("app_update");
                app_delegate_->on_update(frame, services);
            }
            if (!is_running_)
            {
                break;
            }

            if (!can_present(presentation_state))
            {
                diagnostics.mark_suspended();
                finish_diagnostics();
                RELAYCORE_PROFILE_FRAME();
                continue;
            }

            {
                RELAYCORE_PROFILE_ZONE_N("begin_frame");
                renderer_->begin_frame();
            }
            {
                RELAYCORE_PROFILE_ZONE_N("render_world");
                app_delegate_->on_render(frame, services);
                renderer_->finish_scene();
            }
            {
                RELAYCORE_PROFILE_ZONE_N("render_ui");
                if (renderer_->begin_ui(frame, *audio_system_, app_delegate_->has_custom_ui()))
                {
                    app_delegate_->on_ui(services);
                    renderer_->end_ui();
                }
            }
            const auto present_start = Clock::now();
            {
                RELAYCORE_PROFILE_ZONE_N("present");
                static_cast<void>(renderer_->present());
            }

            const auto present_end = Clock::now();
            [[maybe_unused]] const auto cpu_work_ms =
                std::chrono::duration<double, std::milli>(present_start - cpu_work_start).count();
            [[maybe_unused]] const auto present_ms =
                std::chrono::duration<double, std::milli>(present_end - present_start).count();

            RELAYCORE_PROFILE_PLOT("frame_time_ms", frame.wall_delta_seconds * 1000.0);
            RELAYCORE_PROFILE_PLOT("cpu_work_ms", cpu_work_ms);
            RELAYCORE_PROFILE_PLOT("present_ms", present_ms);

            finish_diagnostics();
            RELAYCORE_PROFILE_FRAME();
        }
        diagnostics.flush();

        return 0;
    }

    void Application::process_events(EngineServices& services, const bool wait_for_first_event)
    {
        RELAYCORE_PROFILE_FUNCTION();
        SDL_Event event{};
        if (wait_for_first_event && SDL_WaitEventTimeout(&event, kNonPresentableEventWaitTimeoutMs))
        {
            handle_event(event, services);
        }

        while (is_running_ && SDL_PollEvent(&event))
        {
            handle_event(event, services);
        }
    }

    void Application::handle_event(const SDL_Event& event, EngineServices& services)
    {
        RELAYCORE_PROFILE_FUNCTION();
        renderer_->process_event(event);

        switch (event.type)
        {
            case SDL_EVENT_QUIT:
            case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
                services.request_quit();
                break;

            case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
                renderer_->on_resize(event.window.data1, event.window.data2);
                RelayCore::log::debug("[app] Window pixel size changed: {}x{}", event.window.data1, event.window.data2);
                break;

            case SDL_EVENT_KEY_DOWN:
                if (!event.key.repeat && event.key.scancode == SDL_SCANCODE_F2)
                {
                    renderer_->toggle_diagnostics_visible();
                    RelayCore::log::info(
                        "[app] Diagnostics overlay {}",
                        renderer_->diagnostics_visible() ? "enabled" : "disabled"
                    );
                    RelayCore::log::flush();
                    return;
                }
                if (!event.key.repeat && event.key.scancode == SDL_SCANCODE_F6)
                {
                    renderer_->toggle_advanced_diagnostics_visible();
                    RelayCore::log::info(
                        "[app] Advanced diagnostics {}",
                        renderer_->advanced_diagnostics_visible() ? "enabled" : "disabled"
                    );
                    RelayCore::log::flush();
                    return;
                }
                break;

            default:
                break;
        }

        app_delegate_->on_event(event, services);
    }
} // namespace RelayCore
