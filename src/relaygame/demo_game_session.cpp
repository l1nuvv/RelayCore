#include "relaycore/app/engine_services.h"
#include "relaycore/audio/audio_system.h"
#include "relaycore/core/log.h"
#include "relaycore/render/renderer.h"
#include "relaygame/demo_game.h"
#include "relaygame/level_transition_diagnostics.h"

namespace RelayGame
{
    void DemoGame::change_screen(Screen screen, RelayCore::EngineServices& services)
    {
        if (screen == Screen::Playing && !window_focused_ && services.renderer().ui_available())
        {
            paused_screen_ = Screen::Playing;
            screen = Screen::Paused;
        }
        runtime_.set_paused(screen != Screen::Playing || !window_focused_, services);
        screen_ = screen;
        update_frame_context(services);
        services.renderer().player_ui().set_screen(
            screen == Screen::Playing  ? RelayCore::render::PlayerUiScreen::Hud
            : screen == Screen::Editor ? RelayCore::render::PlayerUiScreen::Hidden
            : screen == Screen::Finale ? RelayCore::render::PlayerUiScreen::Finale
                                       : RelayCore::render::PlayerUiScreen::Menu
        );
        if (screen != Screen::Editor)
            editor_.suspend_camera();
        if (screen == Screen::Finale && window_focused_)
            services.audio_system().set_paused(false);
    }

    void DemoGame::install_level_session(
        detail::PreparedLevelSession&& prepared,
        RelayCore::EngineServices& services,
        const bool allow_capture
    )
    {
        apply_player_settings(services);
        services.renderer().prepare_resources(prepared.world_scene.resource_items());
        prepared.runtime.apply_player_settings(settings_.sensitivity, settings_.camera_motion);
        prepared.runtime.prepare_level_session_activation(services, allow_capture);
        detail::replace_level_session(
            {runtime_,
             world_scene_,
             iteration_diagnostics_,
             saved_runtime_snapshot_,
             played_seconds_,
             scene_time_,
             flashlight_on_},
            std::move(prepared),
            services
        );
        update_frame_context(services);
    }

    void DemoGame::enter_editor(RelayCore::EngineServices& services)
    {
        services.renderer().reset_render_history();
        try
        {
            if (editor_return_)
            {
                editor_.resume(services);
                runtime_.shutdown(services);
                world_scene_.shutdown();
                services.camera().set_position(editor_camera_position_);
                services.camera().set_orientation(editor_camera_angles_.x, editor_camera_angles_.y);
            } else
            {
                if (!save_progress())
                    return;
                if (runtime_.level_path().empty())
                    start_level_session(services);
                editor_.enter(runtime_.level(), services);
                runtime_.set_paused(true, services);
                editor_return_.emplace(EditorReturnState{
                    {std::move(runtime_), std::move(world_scene_)},
                    played_seconds_,
                    scene_time_,
                    flashlight_on_,
                    progress_enabled_,
                    screen_,
                    std::move(visited_levels_)
                });
                runtime_ = GameWorldRuntime{};
                world_scene_ = WorldSceneRenderer{};
            }
            progress_enabled_ = false;
            saved_runtime_snapshot_.reset();
            change_screen(Screen::Editor, services);
        } catch (const std::exception& error)
        {
            progress_message_ = error.what();
            change_screen(screen_, services);
            RelayCore::log::error("Editor: {}", error.what());
        }
    }

    void DemoGame::begin_editor_preview(RelayCore::EngineServices& services)
    {
        if (screen_ != Screen::Editor || !editor_return_)
            return;
        detail::LevelTransitionDiagnostics
            transition{services, runtime_, "editor_preview", editor_.save_path().string()};
        try
        {
            auto prepared = detail::prepare_level_session(
                services.config(),
                LevelSessionBuilder{services.config().gameplay}.build(editor_.document().level(), editor_.save_path()),
                services.renderer().authoring_config(),
                services.renderer().color_pipeline_mode(),
                services.renderer().static_models()
            );
            editor_camera_position_ = services.camera().position();
            editor_camera_angles_ = {services.camera().yaw_radians(), services.camera().pitch_radians()};
            install_level_session(std::move(prepared), services, window_focused_);
            transition.succeeded();
            visited_levels_.clear();
            editor_.leave_scene();
            played_seconds_ = 0;
            scene_time_ = 0;
            flashlight_on_ = false;
            autosave_elapsed_ = 0;
            change_screen(Screen::Playing, services);
        } catch (const std::exception& error)
        {
            transition.failed(error.what());
            editor_.report_error(error.what());
        }
    }

    void DemoGame::return_from_editor(RelayCore::EngineServices& services)
    {
        if (!editor_return_)
            return;
        detail::LevelTransitionDiagnostics
            transition{services, runtime_, "editor_return", editor_return_->session.runtime.level_path()};
        const auto played = editor_return_->played_seconds;
        const auto scene_time = editor_return_->scene_time;
        const auto flashlight = editor_return_->flashlight;
        const auto progress = editor_return_->progress_enabled;
        const auto return_screen = editor_return_->return_screen;
        try
        {
            install_level_session(
                std::move(editor_return_->session),
                services,
                return_screen == Screen::Playing && window_focused_
            );
        } catch (const std::exception& error)
        {
            progress_message_ = error.what();
            transition.failed(error.what());
            editor_.report_error(progress_message_);
            RelayCore::log::error("Return from editor: {}", error.what());
            return;
        }
        transition.succeeded();
        visited_levels_ = std::move(editor_return_->visited_levels);
        editor_return_.reset();
        editor_.leave_scene();
        played_seconds_ = played;
        scene_time_ = scene_time;
        flashlight_on_ = flashlight;
        progress_enabled_ = progress;
        saved_step_ = runtime_.mission_status().current_step;
        autosave_elapsed_ = 0;
        change_screen(return_screen, services);
    }
} // namespace RelayGame
