#include "relaycore/app/engine_services.h"
#include "relaycore/audio/audio_system.h"
#include "relaycore/core/log.h"
#include "relaycore/platform/window.h"
#include "relaycore/render/renderer.h"
#include "relaygame/demo_game.h"
#include "relaygame/game_version.h"

#include <SDL3/SDL_filesystem.h>
#include <algorithm>
#include <glm/trigonometric.hpp>
#include <imgui.h>
#include <stdexcept>

namespace RelayGame
{
    void DemoGame::initialize_player_settings(RelayCore::EngineServices& services)
    {
        update_notification_ = SDL_getenv("RELAYCORE_UPDATE_READY_EVENT") != nullptr;
        if (development_sandbox_)
            new_game_level_ = "levels/development_sandbox.toml";
        settings_.volume = services.config().audio.master_volume;
        settings_.sensitivity = services.config().input.mouse_sensitivity_radians;
        settings_.fov = glm::degrees(services.config().renderer.camera.vertical_fov_radians);
        settings_.vsync = services.window().vsync_enabled();
        if (user_data_directory_.empty())
        {
            const std::unique_ptr<char, decltype(&SDL_free)> directory{
                SDL_GetPrefPath("RelayCore", development_sandbox_ ? "Sandbox" : "RelayCore"),
                SDL_free
            };
            if (!directory)
                throw std::runtime_error("Cannot resolve player settings directory");
            user_data_directory_ = directory.get();
        }
        settings_path_ = user_data_directory_ / "player_settings.toml";
        try
        {
            settings_ = load_player_settings(settings_path_, settings_);
        } catch (const std::exception& error)
        {
            settings_message_ = error.what();
            RelayCore::log::warn("Player settings: {}", error.what());
        }
        apply_player_settings(services);
    }

    void DemoGame::apply_player_settings(RelayCore::EngineServices& services)
    {
        runtime_.apply_player_settings(settings_.sensitivity, settings_.camera_motion);
        services.audio_system().set_master_volume(settings_.volume);
        services.renderer().set_field_of_view(settings_.fov);
        if (services.window().vsync_enabled() != settings_.vsync)
        {
            static_cast<void>(services.window().toggle_vsync());
            settings_.vsync = services.window().vsync_enabled();
        }
    }

    void DemoGame::set_pause(bool paused, RelayCore::EngineServices& services)
    {
        if (!paused && settings_dirty_)
        {
            try
            {
                save_player_settings(settings_path_, settings_);
                settings_dirty_ = false;
                settings_message_.clear();
            } catch (const std::exception& error)
            {
                settings_message_ = error.what();
                return;
            }
        }
        if (paused)
        {
            paused_screen_ = screen_;
            static_cast<void>(save_progress());
            editor_.suspend_camera();
            settings_.vsync = services.window().vsync_enabled();
        }
        change_screen(paused ? Screen::Paused : paused_screen_, services);
    }

    void DemoGame::draw_pause_menu(RelayCore::EngineServices& services)
    {
        const auto* viewport = ImGui::GetMainViewport();
        const float height = show_settings_                ? 660.0F
                             : confirm_new_game_           ? 550.0F
                             : screen_ == Screen::Complete ? 350.0F
                             : screen_ == Screen::MainMenu ? 550.0F
                                                           : 470.0F;
        ImGui::SetNextWindowPos(viewport->GetCenter(), ImGuiCond_Always, {0.5F, 0.5F});
        ImGui::SetNextWindowSize(
            {std::min(580.0F, viewport->WorkSize.x - 24.0F), std::min(height, viewport->WorkSize.y - 24.0F)}
        );
        ImGui::PushStyleColor(ImGuiCol_WindowBg, {0.065F, 0.085F, 0.08F, 0.97F});
        ImGui::PushStyleColor(ImGuiCol_Button, {0.19F, 0.28F, 0.24F, 1.0F});
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, {0.28F, 0.39F, 0.32F, 1.0F});
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, {0.35F, 0.44F, 0.35F, 1.0F});
        ImGui::PushStyleColor(ImGuiCol_FrameBg, {0.14F, 0.20F, 0.17F, 1.0F});
        ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, {0.22F, 0.30F, 0.24F, 1.0F});
        ImGui::PushStyleColor(ImGuiCol_FrameBgActive, {0.25F, 0.34F, 0.27F, 1.0F});
        ImGui::PushStyleColor(ImGuiCol_SliderGrab, {0.68F, 0.58F, 0.34F, 1.0F});
        ImGui::PushStyleColor(ImGuiCol_SliderGrabActive, {0.90F, 0.70F, 0.36F, 1.0F});
        ImGui::PushStyleColor(ImGuiCol_CheckMark, {0.90F, 0.70F, 0.36F, 1.0F});
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {24, 20});
        ImGui::Begin(
            "Пауза##player_pause",
            nullptr,
            ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove
                | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoTitleBar
        );
        ImGui::PushFont(nullptr, 24.0F);
        ImGui::TextColored({0.9F, 0.7F, 0.36F, 1}, "RELAYCORE");
        ImGui::PopFont();
        ImGui::SameLine();
        ImGui::TextDisabled("/ %s", RELAYCORE_GAME_VERSION);
        const bool pressure_transfer = screen_ == Screen::MainMenu
                                           ? new_game_level_ == "levels/pressure_transfer.toml"
                                           : runtime_.level_path().ends_with("pressure_transfer.toml");
        ImGui::SeparatorText(
            development_sandbox_ ? "ИСПЫТАТЕЛЬНЫЙ ПОЛИГОН"
            : pressure_transfer  ? "ПЕРЕПУСК ДАВЛЕНИЯ"
                                 : "КОНТУР 03 / ВОССТАНОВЛЕНИЕ"
        );
        if (screen_ == Screen::MainMenu)
        {
            if (update_notification_)
            {
                ImGui::TextColored({0.65F, 0.85F, 0.68F, 1}, "Игра обновлена до версии %s", RELAYCORE_GAME_VERSION);
                ImGui::SameLine();
                if (ImGui::SmallButton("Понятно##update_notice"))
                    update_notification_ = false;
            } else
                ImGui::TextWrapped(
                    development_sandbox_
                        ? "Движение, устройства, стрельбище и арена. F7 — сброс карты; F9/F10 — повтор встречи."
                    : new_game_level_.empty()
                        ? "Обесточенный сектор. Запасной предохранитель. Один путь к исследовательскому блоку."
                    : new_game_level_ == "levels/pressure_transfer.toml"
                        ? "Перепускной сектор. Два оружия, служебный зал и длинная галерея. Запустите перепуск и "
                          "откройте выход."
                        : "Сервисная мастерская и насосная галерея. Найдите ключ и откройте путь через оба сектора."
                );
            ImGui::Spacing();
            ImGui::BeginDisabled(!disk_save_);
            if (ImGui::Button("Продолжить [Enter]", {-1, 44}))
                begin_episode(true, services);
            ImGui::EndDisabled();
            if (development_sandbox_ && disk_save_)
                ImGui::Text(
                    "Сохранено: полигон · %02d:%02d",
                    static_cast<int>(disk_save_->played_seconds) / 60,
                    static_cast<int>(disk_save_->played_seconds) % 60
                );
            else if (disk_save_ && !disk_save_->world.mission.completed)
                ImGui::Text(
                    "Сохранено: этап %u · %02d:%02d",
                    disk_save_->world.mission.current_step + 1,
                    static_cast<int>(disk_save_->played_seconds) / 60,
                    static_cast<int>(disk_save_->played_seconds) % 60
                );
            if (!development_sandbox_
                && ImGui::BeginCombo(
                    "Сценарий новой игры",
                    new_game_level_.empty()                              ? "Восстановление питания"
                    : new_game_level_ == "levels/pressure_transfer.toml" ? "Перепуск давления"
                                                                         : "Два сектора"
                ))
            {
                if (ImGui::Selectable("Восстановление питания", new_game_level_.empty()))
                    new_game_level_.clear();
                if (ImGui::Selectable("Два сектора", new_game_level_ == "levels/foundation_intake.toml"))
                    new_game_level_ = "levels/foundation_intake.toml";
                if (ImGui::Selectable("Перепуск давления", new_game_level_ == "levels/pressure_transfer.toml"))
                    new_game_level_ = "levels/pressure_transfer.toml";
                ImGui::EndCombo();
            }
            if (ImGui::Button(development_sandbox_ ? "Начать испытания" : "Новый эпизод", {-1, 40}))
            {
                if (save_file_exists_)
                    confirm_new_game_ = true;
                else
                    begin_episode(false, services);
            }
            if (confirm_new_game_)
            {
                ImGui::TextWrapped(
                    development_sandbox_ ? "Сбросить полигон? Предыдущее состояние испытаний будет заменено."
                                         : "Начать заново? Предыдущее сохранение будет заменено."
                );
                if (ImGui::Button("Начать заново", {230, 34}))
                    begin_episode(false, services);
                ImGui::SameLine();
                if (ImGui::Button("Отмена", {-1, 34}))
                    confirm_new_game_ = false;
            }
            if (ImGui::Button("Редактор карты", {-1, 34}))
                enter_editor(services);
        } else if (screen_ == Screen::Complete)
        {
            const auto mission = runtime_.mission_status();
            ImGui::TextColored({0.65F, 0.85F, 0.68F, 1}, "%s", mission.title.c_str());
            ImGui::TextWrapped("%s", mission.detail.c_str());
            ImGui::Text(
                "Время прохождения: %02d:%02d",
                static_cast<int>(played_seconds_) / 60,
                static_cast<int>(played_seconds_) % 60
            );
            if (ImGui::Button("В главное меню [Enter]", {-1, 42}))
                return_to_menu(services);
            if (editor_return_ && ImGui::Button("Вернуться в редактор [F11]", {-1, 34}))
                enter_editor(services);
        } else
        {
            ImGui::TextUnformatted(editor_.active() ? "Редактор приостановлен" : "Игра приостановлена");
            if (ImGui::Button("Продолжить [Esc]", {-1, 42}))
                set_pause(false, services);
            if (ImGui::Button(editor_.active() ? "В главное меню" : "Сохранить и в меню", {-1, 36}))
                return_to_menu(services);
            if (editor_return_)
            {
                if (ImGui::Button("Вернуться в игру", {-1, 34}))
                    return_from_editor(services);
                if (!editor_.active() && ImGui::Button("Вернуться в редактор", {-1, 34}))
                    enter_editor(services);
            }
        }
        if (!progress_message_.empty())
            ImGui::TextWrapped("%s", progress_message_.c_str());
        ImGui::Spacing();
        if (ImGui::Button(show_settings_ ? "Скрыть настройки" : "Настройки", {-1, 34}))
            show_settings_ = !show_settings_;
        if (show_settings_)
        {
            ImGui::SeparatorText("НАСТРОЙКИ");
            bool changed = ImGui::SliderFloat("Громкость", &settings_.volume, 0, 1, "%.2f");
            float sensitivity = settings_.sensitivity * 1000.0F;
            if (ImGui::SliderFloat("Мышь", &sensitivity, 0.3F, 8, "%.2f"))
            {
                settings_.sensitivity = sensitivity / 1000.0F;
                changed = true;
            }
            changed |= ImGui::SliderFloat("Поле зрения", &settings_.fov, 50, 100, "%.0f");
            changed |= ImGui::SliderFloat("Движение камеры", &settings_.camera_motion, 0, 1, "%.2f");
            changed |= ImGui::Checkbox("Вертикальная синхронизация", &settings_.vsync);
            if (changed)
            {
                settings_dirty_ = true;
                try
                {
                    apply_player_settings(services);
                } catch (const std::exception& error)
                {
                    settings_message_ = error.what();
                }
            }
            ImGui::TextWrapped("Ноль отключает покачивание и проседание камеры при приземлении.");
            if (ImGui::Button("Сохранить настройки", {-1, 34}))
            {
                try
                {
                    save_player_settings(settings_path_, settings_);
                    settings_dirty_ = false;
                    settings_message_ = "Сохранено";
                } catch (const std::exception& error)
                {
                    settings_message_ = error.what();
                }
            }
            if (!settings_message_.empty())
                ImGui::TextWrapped("%s", settings_message_.c_str());
        }
        if (screen_ != Screen::Complete)
        {
            ImGui::SeparatorText("УПРАВЛЕНИЕ");
            ImGui::TextWrapped(
                "WASD — движение · Shift — бег · Space — прыжок\nE — взаимодействие · F — фонарик\nF11 — "
                "редактор · F2 — диагностика"
            );
        }
        ImGui::Spacing();
        if (screen_ == Screen::MainMenu && !development_sandbox_)
        {
            if (update_check_.valid() && update_check_.wait_for(std::chrono::seconds{0}) == std::future_status::ready)
            {
                try
                {
                    update_release_ = update_check_.get();
                    update_message_ = update_release_->available ? "Доступна версия " + update_release_->version
                                                                 : "Доступных обновлений нет.";
                } catch (const std::exception& error)
                {
                    update_message_ = error.what();
                }
            }
            ImGui::BeginDisabled(update_check_.valid() || !can_update_game());
            if (ImGui::Button(update_check_.valid() ? "Проверяем обновления…" : "Проверить обновления", {-1, 34}))
            {
                update_release_.reset();
                update_message_.clear();
                update_check_ = std::async(std::launch::async, check_game_update);
            }
            ImGui::EndDisabled();
            if (!can_update_game())
                ImGui::TextDisabled("Обновления доступны в упакованной сборке.");
            if (!update_message_.empty())
                ImGui::TextWrapped("%s", update_message_.c_str());
            if (update_release_ && update_release_->available)
            {
                ImGui::Text("Загрузка: %.1f МБ", static_cast<double>(update_release_->bytes) / 1048576.0);
                ImGui::TextWrapped("%s", update_release_->notes.c_str());
                ImGui::TextWrapped("Игра закроется на время обновления. Сохранения останутся.");
                if (ImGui::Button("Обновить и перезапустить", {-1, 34}))
                {
                    try
                    {
                        if (settings_dirty_)
                        {
                            save_player_settings(settings_path_, settings_);
                            settings_dirty_ = false;
                        }
                        install_game_update(update_release_->version);
                        services.request_quit();
                    } catch (const std::exception& error)
                    {
                        update_message_ = error.what();
                    }
                }
            }
        }
        if (ImGui::Button("Выйти из игры", {-1, 34}))
        {
            if (settings_dirty_)
            {
                try
                {
                    save_player_settings(settings_path_, settings_);
                    settings_dirty_ = false;
                } catch (const std::exception& error)
                {
                    settings_message_ = error.what();
                    show_settings_ = true;
                }
            }
            if (!settings_dirty_ && (screen_ == Screen::MainMenu || screen_ == Screen::Complete || save_progress()))
                services.request_quit();
        }
        ImGui::End();
        ImGui::PopStyleVar();
        ImGui::PopStyleColor(10);
    }
} // namespace RelayGame
