#include "relaycore/app/engine_services.h"
#include "relaycore/audio/audio_system.h"
#include "relaycore/core/log.h"
#include "relaycore/core/paths.h"
#include "relaycore/render/renderer.h"
#include "relaygame/demo_game.h"

#include <algorithm>
#include <cmath>
#include <glm/gtc/constants.hpp>
#include <imgui.h>

namespace RelayGame
{
    void DemoGame::initialize_progress()
    {
        disk_save_.reset();
        save_file_exists_ = false;
        progress_message_.clear();
        progress_path_ = user_data_directory_ / "progress.toml";
        try
        {
            save_file_exists_ = std::filesystem::exists(progress_path_);
            if (save_file_exists_)
            {
                auto save = load_saved_game(progress_path_);
                disk_save_ = std::move(save);
            }
        } catch (const std::exception& error)
        {
            progress_message_ = std::string{"Не удалось прочитать сохранение: "} + error.what();
        }
    }

    auto DemoGame::save_progress() noexcept -> bool
    {
        if (!progress_enabled_ || editor_return_)
            return true;
        autosave_elapsed_ = 0;
        saved_step_ = runtime_.mission_status().current_step;
        try
        {
            auto save = runtime_.capture_saved_game(played_seconds_, flashlight_on_);
            save.visited_levels = visited_levels_;
            write_saved_game(progress_path_, save);
            save_file_exists_ = true;
            disk_save_ = std::move(save);
            progress_message_.clear();
            return true;
        } catch (const std::exception& error)
        {
            progress_message_ = std::string{"Прогресс не сохранён: "} + error.what();
            RelayCore::log::error("{}", progress_message_);
            return false;
        }
    }

    void DemoGame::begin_episode(bool continuing, RelayCore::EngineServices& services)
    {
        if (screen_ != Screen::MainMenu)
            return;
        std::vector<SavedGame> initial_history;
        try
        {
            if (settings_dirty_)
            {
                save_player_settings(settings_path_, settings_);
                settings_dirty_ = false;
            }
            if (continuing && !disk_save_)
                return;
            if (continuing)
                initial_history = disk_save_->visited_levels;
            const LevelSessionBuilder builder{services.config().gameplay};
            auto prepared = detail::prepare_level_session(
                services.config(),
                continuing                ? builder.build(disk_save_->world.level_path)
                : new_game_level_.empty() ? builder.build()
                                          : builder.build(RelayCore::core::asset_path(new_game_level_)),
                services.renderer().authoring_config(),
                services.renderer().color_pipeline_mode(),
                services.renderer().static_models(),
                continuing ? &*disk_save_ : nullptr
            );
            install_level_session(std::move(prepared), services, window_focused_);
        } catch (const std::exception& error)
        {
            progress_message_ = error.what();
            RelayCore::log::error("Begin episode: {}", error.what());
            return;
        }

        visited_levels_ = std::move(initial_history);
        played_seconds_ = continuing ? disk_save_->played_seconds : 0;
        flashlight_on_ = continuing && disk_save_->flashlight;
        scene_time_ = played_seconds_;
        autosave_elapsed_ = 0;
        saved_step_ = runtime_.mission_status().current_step;
        progress_enabled_ = true;
        progress_message_.clear();
        show_settings_ = false;
        confirm_new_game_ = false;
        saved_runtime_snapshot_.reset();
        change_screen(Screen::Playing, services);
        try
        {
            runtime_.publish_diagnostics(services.renderer());
            if (runtime_.mission_status().completed && !runtime_.has_completion_exit())
                begin_finale(services);
            else if (!continuing)
                static_cast<void>(save_progress());
        } catch (const std::exception& error)
        {
            RelayCore::log::error("Episode started, but presentation update failed: {}", error.what());
        }
    }

    void DemoGame::return_to_menu(RelayCore::EngineServices& services)
    {
        if (screen_ != Screen::Complete && !save_progress())
            return;
        if (settings_dirty_)
        {
            try
            {
                save_player_settings(settings_path_, settings_);
                settings_dirty_ = false;
            } catch (const std::exception& error)
            {
                settings_message_ = error.what();
                return;
            }
        }
        if (editor_.active())
            editor_.leave(services);
        runtime_.shutdown(services);
        world_scene_.shutdown();
        editor_return_.reset();
        saved_runtime_snapshot_.reset();
        iteration_diagnostics_ = {};
        services.renderer().set_iteration_diagnostics({});
        services.renderer().set_world_diagnostics({});
        runtime_.publish_diagnostics(services.renderer());
        progress_enabled_ = false;
        played_seconds_ = 0;
        scene_time_ = 0;
        flashlight_on_ = false;
        change_screen(Screen::MainMenu, services);
        show_settings_ = false;
    }

    void DemoGame::begin_finale(RelayCore::EngineServices& services)
    {
        change_screen(runtime_.ending() ? Screen::Finale : Screen::Complete, services);
        finale_time_ = 0;
        finale_start_position_ = services.camera().position();
        finale_start_angles_ = {services.camera().yaw_radians(), services.camera().pitch_radians()};
        runtime_.begin_ending();
    }

    void DemoGame::update_finale(float dt, RelayCore::EngineServices& services)
    {
        finale_time_ += dt;
        scene_time_ += dt;
        const float t = std::clamp(finale_time_ / 6.0F, 0.0F, 1.0F);
        const float blend = t * t * (3 - 2 * t);
        const auto& ending = *runtime_.ending();
        services.camera().set_position(glm::mix(finale_start_position_, ending.camera_position, blend));
        const float yaw_delta = std::remainder(ending.view_angles.x - finale_start_angles_.x, glm::two_pi<float>());
        services.camera().set_orientation(
            finale_start_angles_.x + yaw_delta * blend,
            glm::mix(finale_start_angles_.y, ending.view_angles.y, blend)
        );
        runtime_.update_ending(dt, services);
        if (t >= 1)
        {
            // Finish the closing movement even when the cinematic was skipped.
            runtime_.update_ending(4, services);
            change_screen(Screen::Complete, services);
        }
    }

    void DemoGame::draw_finale()
    {
        const auto* viewport = ImGui::GetMainViewport();
        const ImVec2 pos = viewport->Pos, size = viewport->Size;
        auto* draw = ImGui::GetForegroundDrawList();
        draw->AddRectFilled(pos, {pos.x + size.x, pos.y + size.y * 0.12F}, IM_COL32(8, 12, 11, 255));
        draw->AddRectFilled(
            {pos.x, pos.y + size.y * 0.83F},
            {pos.x + size.x, pos.y + size.y},
            IM_COL32(8, 12, 11, 255)
        );
        const bool pressure_transfer = runtime_.level_path().ends_with("pressure_transfer.toml");
        const bool bench_relay = runtime_.level_path().ends_with("bench_relay.toml");
        const bool cooling_loop = runtime_.level_path().ends_with("cooling_loop.toml");
        const char* text = pressure_transfer  ? (finale_time_ < 2   ? "Перепуск работает. Давление выровнено."
                                                 : finale_time_ < 4 ? "Выход в технический сектор открыт."
                                                                    : "Следующий маршрут: технический сектор.")
                           : bench_relay      ? (finale_time_ < 2   ? "Реле мастерской восстановлено."
                                                 : finale_time_ < 4 ? "Служебный выход открыт."
                                                                    : "Ремонтный зал пройден.")
                           : cooling_loop     ? (finale_time_ < 2   ? "Утечка изолирована. Контур охлаждения работает."
                                                 : finale_time_ < 4 ? "Вентиляция восстановлена. Выход открыт."
                                                                    : "Охлаждающий сектор пройден.")
                           : finale_time_ < 2 ? "Питание и охлаждение в норме."
                           : finale_time_ < 4 ? "Изоляция сектора завершена. Допуск подтверждён."
                                              : "Следующий маршрут: исследовательский блок.";
        const auto extent = ImGui::CalcTextSize(text);
        draw->AddText({pos.x + (size.x - extent.x) * 0.5F, pos.y + size.y * 0.88F}, IM_COL32(225, 221, 202, 255), text);
        draw->AddText({pos.x + 24, pos.y + size.y * 0.95F}, IM_COL32(145, 155, 148, 255), "Enter / Esc — пропустить");
    }
} // namespace RelayGame
