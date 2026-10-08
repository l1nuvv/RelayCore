#include "relaygame/demo_game.h"

#include "relaycore/app/engine_services.h"
#include "relaycore/audio/audio_system.h"
#include "relaycore/core/log.h"
#include "relaycore/core/paths.h"
#include "relaycore/core/profiling.h"
#include "relaycore/platform/window.h"
#include "relaycore/render/renderer.h"
#include "relaygame/demo_game_runtime_ops.h"
#include "relaygame/level_session_builder.h"

namespace RelayGame
{
    void DemoGame::start_level_session(RelayCore::EngineServices& services)
    {
        if (development_sandbox_)
        {
            install_level_session(
                detail::prepare_level_session(
                    services.config(),
                    LevelSessionBuilder{services.config().gameplay}.build(
                        RelayCore::core::asset_path("levels/development_sandbox.toml")
                    ),
                    services.renderer().authoring_config(),
                    services.renderer().color_pipeline_mode(),
                    services.renderer().static_models()
                ),
                services,
                window_focused_
            );
            return;
        }
        detail::start_level_session(
            detail::DemoGameRuntimeState{
                .runtime = runtime_,
                .world_scene = world_scene_,
                .iteration_diagnostics = iteration_diagnostics_,
                .saved_runtime_snapshot = saved_runtime_snapshot_,
                .played_seconds = played_seconds_,
                .scene_time = scene_time_,
                .flashlight = flashlight_on_,
            },
            services
        );
    }

    void DemoGame::reload_live_content(RelayCore::EngineServices& services)
    {
        detail::reload_live_content(
            detail::DemoGameRuntimeState{
                .runtime = runtime_,
                .world_scene = world_scene_,
                .iteration_diagnostics = iteration_diagnostics_,
                .saved_runtime_snapshot = saved_runtime_snapshot_,
                .played_seconds = played_seconds_,
                .scene_time = scene_time_,
                .flashlight = flashlight_on_,
            },
            services
        );
    }

    auto DemoGame::handle_debug_iteration_tools(RelayCore::EngineServices& services) -> bool
    {
        const auto result = detail::handle_debug_iteration_tools(
            detail::DemoGameRuntimeState{
                .runtime = runtime_,
                .world_scene = world_scene_,
                .iteration_diagnostics = iteration_diagnostics_,
                .saved_runtime_snapshot = saved_runtime_snapshot_,
                .played_seconds = played_seconds_,
                .scene_time = scene_time_,
                .flashlight = flashlight_on_,
            },
            services
        );
        if (result == detail::IterationToolResult::Reloaded)
        {
            played_seconds_ = 0;
            scene_time_ = 0;
            flashlight_on_ = false;
            autosave_elapsed_ = 0;
            saved_step_ = runtime_.mission_status().current_step;
            change_screen(Screen::Playing, services);
        }
        if (result == detail::IterationToolResult::Snapshot && iteration_diagnostics_.last_snapshot_succeeded)
        {
            autosave_elapsed_ = 0;
            saved_step_ = runtime_.mission_status().current_step;
        }
        return result != detail::IterationToolResult::None;
    }

    void DemoGame::publish_iteration_diagnostics(RelayCore::EngineServices& services)
    {
        detail::publish_iteration_diagnostics(
            detail::DemoGameRuntimeState{
                .runtime = runtime_,
                .world_scene = world_scene_,
                .iteration_diagnostics = iteration_diagnostics_,
                .saved_runtime_snapshot = saved_runtime_snapshot_,
                .played_seconds = played_seconds_,
                .scene_time = scene_time_,
                .flashlight = flashlight_on_,
            },
            services
        );
    }

    void DemoGame::on_startup(RelayCore::EngineServices& services)
    {
        RELAYCORE_PROFILE_ZONE_N("DemoGame::on_startup");
        initialize_player_settings(services);
        initialize_progress();
        try
        {
            start_level_session(services);
        } catch (const std::exception& error)
        {
            if (!services.renderer().ui_available())
                throw;
            progress_message_ = error.what();
        }
        change_screen(services.renderer().ui_available() ? Screen::MainMenu : Screen::Playing, services);
        services.renderer().set_diagnostics_visible(false);
    }

    void DemoGame::on_event(const SDL_Event& event, RelayCore::EngineServices& services)
    {
        RELAYCORE_PROFILE_FUNCTION();
        if (event.type == SDL_EVENT_WINDOW_FOCUS_LOST || event.type == SDL_EVENT_WINDOW_MINIMIZED)
            window_focused_ = false;
        if (event.type == SDL_EVENT_WINDOW_FOCUS_GAINED || event.type == SDL_EVENT_WINDOW_RESTORED)
            window_focused_ = true;
        if (screen_ == Screen::Finale
            && (event.type == SDL_EVENT_WINDOW_FOCUS_LOST || event.type == SDL_EVENT_WINDOW_MINIMIZED
                || event.type == SDL_EVENT_WINDOW_FOCUS_GAINED || event.type == SDL_EVENT_WINDOW_RESTORED))
            services.audio_system().set_paused(!window_focused_);
        if (event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat && event.key.scancode == SDL_SCANCODE_RETURN)
        {
            if (screen_ == Screen::MainMenu && !show_settings_ && !confirm_new_game_)
            {
                if (save_file_exists_ && !disk_save_)
                    confirm_new_game_ = true;
                else
                    begin_episode(disk_save_.has_value(), services);
            } else if (screen_ == Screen::Finale)
                finale_time_ = 6;
            else if (screen_ == Screen::Complete)
                return_to_menu(services);
            return;
        }
        if (event.type == SDL_EVENT_KEY_DOWN && event.key.scancode == SDL_SCANCODE_ESCAPE && !event.key.repeat)
        {
            if (services.renderer().ui_available())
            {
                if (show_settings_)
                    show_settings_ = false;
                else if (screen_ == Screen::Playing || screen_ == Screen::Editor || screen_ == Screen::Paused)
                    set_pause(screen_ != Screen::Paused, services);
                else if (screen_ == Screen::Finale)
                    finale_time_ = 6;
            } else
                services.request_quit();
            return;
        }
        if (event.type == SDL_EVENT_WINDOW_FOCUS_LOST || event.type == SDL_EVENT_WINDOW_MINIMIZED)
        {
            runtime_.on_event(event, services);
            if (editor_.active())
                editor_.on_event(event, services);
            if (services.renderer().ui_available() && (screen_ == Screen::Playing || screen_ == Screen::Editor))
                set_pause(true, services);
            return;
        }
        if (event.type == SDL_EVENT_KEY_DOWN && event.key.scancode == SDL_SCANCODE_F11 && !event.key.repeat
            && services.renderer().ui_available())
        {
            if (screen_ == Screen::Editor)
                editor_.request_play();
            else if (screen_ == Screen::Playing
                     || (editor_return_ && (screen_ == Screen::Finale || screen_ == Screen::Complete)))
                enter_editor(services);
            return;
        }
        if (screen_ != Screen::Playing && screen_ != Screen::Editor)
        {
            if (event.type == SDL_EVENT_WINDOW_FOCUS_GAINED || event.type == SDL_EVENT_WINDOW_RESTORED)
                runtime_.on_event(event, services);
            return;
        }
        if (editor_.active())
        {
            editor_.on_event(event, services);
            return;
        }
        if (event.type == SDL_EVENT_KEY_DOWN && event.key.scancode == SDL_SCANCODE_F && !event.key.repeat
            && services.window().relative_mouse_mode() && !services.renderer().wants_keyboard_capture()
            && runtime_.player_combat_state().health > 0)
        {
            flashlight_on_ = !flashlight_on_;
            services.audio_system().play_one_shot(RelayCore::core::asset_path("audio/player/flashlight_switch.wav"));
            return;
        }
        runtime_.on_event(event, services);
    }

    void DemoGame::on_update(const RelayCore::FrameInfo& frame, RelayCore::EngineServices& services)
    {
        RELAYCORE_PROFILE_ZONE_N("DemoGame::on_update");
        if (screen_ == Screen::Finale)
        {
            if (window_focused_)
                update_finale(static_cast<float>(frame.delta_seconds), services);
            return;
        }
        if (screen_ == Screen::Editor)
        {
            editor_.update(frame, services);
            if (editor_.exit_requested())
                return_from_editor(services);
            else if (editor_.play_requested())
                begin_editor_preview(services);
            return;
        }
        if (screen_ != Screen::Playing)
            return;
        runtime_.update(frame, services);
        scene_time_ += static_cast<float>(frame.delta_seconds);
        played_seconds_ += static_cast<float>(frame.delta_seconds);
        autosave_elapsed_ += static_cast<float>(frame.delta_seconds);
        if (!editor_return_ && handle_debug_iteration_tools(services))
        {
            runtime_.publish_diagnostics(services.renderer());
            return;
        }
        if (const auto* exit = runtime_.active_level_exit())
        {
            transition_to(*exit, services);
            return;
        }
        if (runtime_.mission_status().completed && !runtime_.has_completion_exit())
        {
            static_cast<void>(save_progress());
            begin_finale(services);
        } else if (progress_enabled_
                   && (autosave_elapsed_ >= 25 || saved_step_ != runtime_.mission_status().current_step))
            static_cast<void>(save_progress());
        runtime_.publish_diagnostics(services.renderer());
        runtime_.apply_player_settings(settings_.sensitivity, settings_.camera_motion);
        services.renderer().set_field_of_view(settings_.fov);
    }

    void DemoGame::on_render(const RelayCore::FrameInfo& frame, RelayCore::EngineServices& services)
    {
        RELAYCORE_PROFILE_ZONE_N("DemoGame::on_render");
        services.renderer().set_scene_time(scene_time_);
        services.renderer().set_spot_light(
            {.position = services.camera().position(),
             .direction = services.camera().forward(),
             .enabled = flashlight_on_ && !editor_.active() && screen_ != Screen::MainMenu
                        && runtime_.player_combat_state().health > 0}
        );
        if (editor_.active())
        {
            editor_.render(frame, services);
            return;
        }
        if (runtime_.level_path().empty())
            return;
        world_scene_.render(
            services.renderer(),
            services.camera(),
            scene_time_,
            runtime_.doors(),
            runtime_.turrets(),
            runtime_.usables(),
            runtime_.focused_usable_index(),
            services.renderer().inspector_diagnostics(),
            services.renderer().inspector_visualization_enabled(),
            runtime_.activated_target_ids(),
            screen_ == Screen::Playing || screen_ == Screen::Paused ? runtime_.scientist_pose() : ScientistPose{},
            runtime_.environment(),
            runtime_.enemies(),
            &runtime_.player_combat_state(),
            runtime_.player_pellet_traces()
        );
        if ((screen_ == Screen::Playing || screen_ == Screen::Paused) && runtime_.player_combat_state().alive)
        {
            if (runtime_.selected_weapon() == PlayerWeapon::Shotgun)
                world_scene_.render_shotgun_viewmodel(
                    services.renderer(),
                    services.camera(),
                    runtime_.shotgun_cycle_remaining_seconds()
                );
            else
                world_scene_.render_pistol_viewmodel(
                    services.renderer(),
                    services.camera(),
                    runtime_.pistol_cycle_remaining_seconds(),
                    runtime_.player_combat_state().weapon_refire_seconds
                );
        }
        services.renderer().set_world_diagnostics(world_scene_.diagnostics());
        services.renderer().finish_scene();
        signal_update_ready();
    }

    void DemoGame::on_ui(RelayCore::EngineServices& services)
    {
        if (screen_ == Screen::Finale)
            draw_finale();
        else if (screen_ == Screen::Editor)
            editor_.draw_ui(services);
        else if (screen_ != Screen::Playing)
            draw_pause_menu(services);
    }

    void DemoGame::on_shutdown(RelayCore::EngineServices& services) noexcept
    {
        RELAYCORE_PROFILE_FUNCTION();
        if (screen_ == Screen::Playing || screen_ == Screen::Paused)
            static_cast<void>(save_progress());
        runtime_.shutdown(services);
        editor_.shutdown();
        world_scene_.shutdown();
        editor_return_.reset();
        saved_runtime_snapshot_.reset();
        progress_enabled_ = false;
        screen_ = Screen::MainMenu;
        played_seconds_ = 0;
        scene_time_ = 0;
        flashlight_on_ = false;
        RelayCore::log::info("DemoGame shutdown");
    }

    auto make_demo_game() -> std::unique_ptr<RelayCore::AppDelegate>
    {
        RELAYCORE_PROFILE_FUNCTION();
        return std::make_unique<DemoGame>();
    }
} // namespace RelayGame
