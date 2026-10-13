#include "relaycore/app/engine_services.h"
#include "relaycore/audio/audio_system.h"
#include "relaycore/core/log.h"
#include "relaycore/platform/window.h"
#include "relaycore/render/renderer.h"
#include "relaygame/demo_game.h"
#include "relaygame/game_version.h"

#include <SDL3/SDL_filesystem.h>
#include <algorithm>
#include <cmath>
#include <format>
#include <glm/trigonometric.hpp>
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
        settings_.hdr_enabled = services.renderer().config().hdr_enabled;
        settings_.bloom_enabled = services.renderer().config().bloom_enabled;
        settings_.ssao_enabled = services.renderer().config().ssao_enabled;
        settings_.ssr_enabled = services.renderer().config().ssr_enabled;
        settings_.volumetrics_enabled = services.renderer().config().volumetrics_enabled;
        settings_.ssgi_quality = services.renderer().config().ssgi_quality;
        settings_.reflection_quality = services.renderer().config().reflection_quality;
        settings_.dof_quality = services.renderer().config().dof_quality;
        settings_.motion_blur_quality = services.renderer().config().motion_blur_quality;
        settings_.taa_enabled = services.renderer().config().taa_enabled;
        settings_.render_scale = services.renderer().config().render_scale;
        settings_.dynamic_resolution_enabled = services.renderer().config().dynamic_resolution_enabled;
        sdr_color_pipeline_ = services.renderer().color_pipeline_mode();
        if (user_data_directory_.empty())
        {
            if (const char* directory = SDL_getenv_unsafe("RELAYCORE_USER_DATA_ROOT"); directory != nullptr)
            {
                const std::filesystem::path path{directory};
                if (!path.is_absolute())
                    throw std::runtime_error("RELAYCORE_USER_DATA_ROOT must be an absolute path");
                user_data_directory_ = path;
            }
        }
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
        try
        {
            apply_player_settings(services);
        } catch (const std::exception& error)
        {
            settings_.hdr_enabled = services.renderer().config().hdr_enabled;
            settings_.bloom_enabled = services.renderer().config().bloom_enabled;
            settings_.ssao_enabled = services.renderer().config().ssao_enabled;
            settings_.ssr_enabled = services.renderer().config().ssr_enabled;
            settings_.volumetrics_enabled = services.renderer().config().volumetrics_enabled;
            settings_.ssgi_quality = services.renderer().config().ssgi_quality;
            settings_.reflection_quality = services.renderer().config().reflection_quality;
            settings_.dof_quality = services.renderer().config().dof_quality;
            settings_.motion_blur_quality = services.renderer().config().motion_blur_quality;
            settings_.taa_enabled = services.renderer().config().taa_enabled;
            settings_.render_scale = services.renderer().config().render_scale;
            settings_.dynamic_resolution_enabled = services.renderer().config().dynamic_resolution_enabled;
            settings_message_ = error.what();
            RelayCore::log::warn("Player settings: {}", error.what());
        }
    }

    void DemoGame::apply_player_settings(RelayCore::EngineServices& services)
    {
        auto& renderer = services.renderer();
        if (settings_.hdr_enabled != renderer.config().hdr_enabled
            || settings_.bloom_enabled != renderer.config().bloom_enabled
            || settings_.ssao_enabled != renderer.config().ssao_enabled
            || settings_.ssr_enabled != renderer.config().ssr_enabled
            || settings_.volumetrics_enabled != renderer.config().volumetrics_enabled
            || settings_.ssgi_quality != renderer.config().ssgi_quality
            || settings_.reflection_quality != renderer.config().reflection_quality
            || settings_.dof_quality != renderer.config().dof_quality
            || settings_.motion_blur_quality != renderer.config().motion_blur_quality
            || settings_.taa_enabled != renderer.config().taa_enabled
            || settings_.render_scale != renderer.config().render_scale
            || settings_.dynamic_resolution_enabled != renderer.config().dynamic_resolution_enabled)
        {
            auto config = services.config();
            config.renderer = renderer.config();
            config.renderer.hdr_enabled = settings_.hdr_enabled;
            config.renderer.bloom_enabled = settings_.bloom_enabled;
            config.renderer.ssao_enabled = settings_.ssao_enabled;
            config.renderer.ssr_enabled = settings_.ssr_enabled;
            config.renderer.volumetrics_enabled = settings_.volumetrics_enabled;
            config.renderer.ssgi_quality = settings_.ssgi_quality;
            config.renderer.reflection_quality = settings_.reflection_quality;
            config.renderer.dof_quality = settings_.dof_quality;
            config.renderer.motion_blur_quality = settings_.motion_blur_quality;
            config.renderer.taa_enabled = settings_.taa_enabled;
            config.renderer.render_scale = settings_.render_scale;
            config.renderer.dynamic_resolution_enabled = settings_.dynamic_resolution_enabled;
            const auto pipeline =
                settings_.hdr_enabled ? RelayCore::render::ColorPipelineMode::hardware_srgb : sdr_color_pipeline_;
            auto backend = renderer.prepare_live_content_reload(config.renderer, renderer.authoring_config(), pipeline);
            std::optional<WorldSceneRenderer> world, editor_scene, return_scene;
            if (pipeline != renderer.color_pipeline_mode())
            {
                const auto prepare_scene = [&](const GameWorldRuntime& runtime) {
                    WorldSceneRenderer scene;
                    scene.initialize(
                        build_prepared_world_scene_data(runtime.level(), renderer.authoring_config(), pipeline),
                        renderer.static_models()
                    );
                    return scene;
                };
                if (!runtime_.level_path().empty())
                    world.emplace(prepare_scene(runtime_));
                if (editor_.active())
                    editor_scene.emplace(editor_.prepare_color_pipeline(pipeline, services));
                if (editor_return_)
                    return_scene.emplace(prepare_scene(editor_return_->session.runtime));
            }
            if (!runtime_.level_path().empty())
                backend.prepare_resources(world ? world->resource_items() : world_scene_.resource_items());
            if (editor_.active())
                backend.prepare_resources(editor_scene ? editor_scene->resource_items() : editor_.resource_items());
            if (editor_return_)
                backend.prepare_resources(
                    return_scene ? return_scene->resource_items() : editor_return_->session.world_scene.resource_items()
                );
            renderer.commit_live_content_reload(std::move(backend));
            if (world)
                world_scene_ = std::move(*world);
            if (editor_scene)
                editor_.commit_color_pipeline(std::move(*editor_scene));
            if (return_scene)
                editor_return_->session.world_scene = std::move(*return_scene);
            services.commit_live_reload_config(std::move(config));
        }
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

    void DemoGame::apply_ui_actions(RelayCore::EngineServices& services)
    {
        for (const auto& action : services.renderer().player_ui().consume_actions())
        {
            const auto source_screen = screen_;
            const auto& name = action.name;
            if (name == "continue" && screen_ == Screen::MainMenu && disk_save_)
                begin_episode(true, services);
            else if (name == "new" && screen_ == Screen::MainMenu)
            {
                if (save_file_exists_)
                    confirm_new_game_ = true;
                else
                    begin_episode(false, services);
            } else if (name == "confirm" && screen_ == Screen::MainMenu)
                begin_episode(false, services);
            else if (name == "cancel")
                confirm_new_game_ = false;
            else if (name == "scenario" && screen_ == Screen::MainMenu && !development_sandbox_)
                new_game_level_ = action.value;
            else if (name == "notice")
                update_notification_ = false;
            else if (name == "resume" && screen_ == Screen::Paused)
                set_pause(false, services);
            else if (name == "menu")
                return_to_menu(services);
            else if (name == "editor" && services.renderer().developer_ui_available())
                enter_editor(services);
            else if (name == "return-game" && editor_return_)
                return_from_editor(services);
            else if (name == "settings")
                show_settings_ = !show_settings_;
            else if (name == "save-settings")
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
            } else if (name == "volume" || name == "sensitivity" || name == "fov" || name == "camera-motion"
                       || name == "vsync" || name == "hdr" || name == "bloom" || name == "ssao" || name == "ssr"
                       || name == "volumetrics" || name == "taa" || name == "render-scale"
                       || name == "dynamic-resolution" || name == "ssgi-quality" || name == "reflection-quality"
                       || name == "dof-quality" || name == "motion-blur-quality")
            {
                if (name == "volume")
                    settings_.volume = std::stof(action.value);
                if (name == "sensitivity")
                    settings_.sensitivity = std::stof(action.value) / 1000.0F;
                if (name == "fov")
                    settings_.fov = std::stof(action.value);
                if (name == "camera-motion")
                    settings_.camera_motion = std::stof(action.value);
                if (name == "vsync")
                    settings_.vsync = action.value == "1";
                if (name == "hdr")
                {
                    settings_.hdr_enabled = action.value == "1";
                    if (!settings_.hdr_enabled)
                    {
                        settings_.bloom_enabled = settings_.ssr_enabled = settings_.taa_enabled = false;
                        settings_.ssgi_quality = 0;
                        settings_.reflection_quality = 0;
                        settings_.dof_quality = 0;
                        settings_.motion_blur_quality = 0;
                        settings_.render_scale = 1;
                        settings_.dynamic_resolution_enabled = false;
                    }
                }
                if (name == "bloom")
                    settings_.bloom_enabled = action.value == "1";
                if (name == "ssao")
                    settings_.ssao_enabled = action.value == "1";
                if (name == "ssr")
                    settings_.ssr_enabled = action.value == "1";
                if (name == "volumetrics")
                    settings_.volumetrics_enabled = action.value == "1";
                if (name == "ssgi-quality")
                    settings_.ssgi_quality = std::stoi(action.value);
                if (name == "reflection-quality")
                    settings_.reflection_quality = std::stoi(action.value);
                if (name == "dof-quality")
                    settings_.dof_quality = std::stoi(action.value);
                if (name == "motion-blur-quality")
                    settings_.motion_blur_quality = std::stoi(action.value);
                if (!settings_.ssr_enabled && name == "ssr")
                    settings_.reflection_quality = 0;
                if (settings_.reflection_quality)
                    settings_.ssr_enabled = true;
                if (name == "taa")
                    settings_.taa_enabled = action.value == "1";
                if (name == "render-scale")
                    settings_.render_scale = std::stof(action.value);
                if (name == "dynamic-resolution")
                    settings_.dynamic_resolution_enabled = action.value == "1";
                if (settings_.bloom_enabled || settings_.ssr_enabled || settings_.taa_enabled
                    || settings_.render_scale != 1 || settings_.dynamic_resolution_enabled || settings_.ssgi_quality
                    || settings_.dof_quality || settings_.motion_blur_quality)
                    settings_.hdr_enabled = true;
                settings_dirty_ = true;
                try
                {
                    apply_player_settings(services);
                    settings_message_.clear();
                } catch (const std::exception& error)
                {
                    settings_.hdr_enabled = services.renderer().config().hdr_enabled;
                    settings_.bloom_enabled = services.renderer().config().bloom_enabled;
                    settings_.ssao_enabled = services.renderer().config().ssao_enabled;
                    settings_.ssr_enabled = services.renderer().config().ssr_enabled;
                    settings_.volumetrics_enabled = services.renderer().config().volumetrics_enabled;
                    settings_.ssgi_quality = services.renderer().config().ssgi_quality;
                    settings_.reflection_quality = services.renderer().config().reflection_quality;
                    settings_.dof_quality = services.renderer().config().dof_quality;
                    settings_.motion_blur_quality = services.renderer().config().motion_blur_quality;
                    settings_.taa_enabled = services.renderer().config().taa_enabled;
                    settings_.render_scale = services.renderer().config().render_scale;
                    settings_.dynamic_resolution_enabled = services.renderer().config().dynamic_resolution_enabled;
                    settings_message_ = error.what();
                }
            } else if (name == "check-update" && !update_check_.valid() && can_update_game())
            {
                update_release_.reset();
                update_message_.clear();
                update_check_ = std::async(std::launch::async, check_game_update);
            } else if (name == "install-update" && update_release_ && update_release_->available)
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
            } else if (name == "quit")
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
            if (screen_ != source_screen)
                break;
        }
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
    }
    void DemoGame::draw_pause_menu(RelayCore::EngineServices& services)
    {
        auto& ui = services.renderer().player_ui();
        const bool main = screen_ == Screen::MainMenu, complete = screen_ == Screen::Complete;
        const auto& level = main ? new_game_level_ : runtime_.level_path();
        const bool pressure = level.ends_with("pressure_transfer.toml"), cooling = level.ends_with("cooling_loop.toml"),
                   bench = level.ends_with("bench_relay.toml"), occult = level.ends_with("occult_lab_b17.toml"),
                   containment = level.ends_with("containment_loop.toml");
        ui.set_text("version", RELAYCORE_GAME_VERSION);
        ui.set_text(
            "menu-title",
            development_sandbox_ ? "ИСПЫТАТЕЛЬНЫЙ ПОЛИГОН"
            : pressure           ? "ПЕРЕПУСК ДАВЛЕНИЯ"
            : cooling            ? "КОНТУР ОХЛАЖДЕНИЯ"
            : bench              ? "РЕЛЕ МАСТЕРСКОЙ"
            : occult             ? "ЛАБОРАТОРИЯ B-17"
            : containment        ? "КОНТУР СДЕРЖИВАНИЯ"
                                 : "КОНТУР 03 / ВОССТАНОВЛЕНИЕ"
        );
        ui.set_visible("main-actions", main);
        ui.set_visible("pause-actions", !main && !complete);
        ui.set_visible("complete-actions", complete);
        ui.set_visible("description", !complete);
        ui.set_text(
            "description",
            !main ? (editor_.active() ? "Редактор приостановлен" : "Игра приостановлена")
            : development_sandbox_
                ? "Движение, устройства, стрельбище и арена. F7 — сброс карты; F9/F10 — повтор встречи."
            : new_game_level_.empty() ? "Весь комплекс: питание, два сектора, мастерская, перепуск давления, "
                                        "охлаждение, лаборатория B-17 и финальный контур сдерживания Хранителя. "
                                        "Главы связаны переходами; прогресс сохраняется."
            : pressure ? "Перепускной сектор. Два оружия, служебный зал и длинная галерея. Запустите перепуск и "
                         "откройте выход."
            : bench    ? "Ремонтный зал, два оружия и охрана среди рабочих столов. Включите реле и откройте выход."
            : cooling  ? "Утечка в охлаждающем секторе. Изолируйте аварийную линию, запустите оборудование и откройте "
                         "служебный выход."
            : occult   ? "Наблюдательный пост и резонансная лаборатория. Осмотрите оборудование и проверьте изолятор."
            : containment ? "Архивно-исследовательский корпус. Graybox G-019: найдите кассету, восстановите захват, "
                            "заведите Хранителя в ловушку и запустите сдерживание."
                          : "Сервисная мастерская и насосная галерея. Найдите ключ и откройте путь через оба сектора."
        );
        ui.set_disabled("continue", !disk_save_);
        ui.set_text(
            "save-summary",
            disk_save_
                ? "Сохранено: "
                      + (development_sandbox_ ? std::string{"полигон"}
                                              : "этап " + std::to_string(disk_save_->world.mission.current_step + 1))
                      + " · " + std::to_string(int(disk_save_->played_seconds) / 60) + ":"
                      + (int(disk_save_->played_seconds) % 60 < 10 ? "0" : "")
                      + std::to_string(int(disk_save_->played_seconds) % 60)
                : ""
        );
        ui.set_visible("scenario-row", !development_sandbox_);
        if (!development_sandbox_)
            ui.set_value("scenario", new_game_level_);
        ui.set_text("new-game", development_sandbox_ ? "Начать испытания" : "Начать прохождение");
        ui.set_visible("confirmation", confirm_new_game_);
        ui.set_text(
            "confirmation-text",
            development_sandbox_ ? "Сбросить полигон? Предыдущее состояние испытаний будет заменено."
                                 : "Начать заново? Предыдущее сохранение будет заменено."
        );
        ui.set_visible("editor-button", services.renderer().developer_ui_available());
        ui.set_text("to-menu", editor_.active() ? "В главное меню" : "Сохранить и в меню");
        ui.set_visible("return-game", editor_return_.has_value());
        ui.set_visible(
            "return-editor",
            editor_return_ && !editor_.active() && services.renderer().developer_ui_available()
        );
        ui.set_visible("complete-editor", editor_return_.has_value() && services.renderer().developer_ui_available());
        if (complete)
        {
            const auto mission = runtime_.mission_status();
            ui.set_text("complete-title", mission.title);
            ui.set_text("complete-detail", mission.detail);
            ui.set_text(
                "played-time",
                "Время прохождения: " + std::to_string(int(played_seconds_) / 60) + ":"
                    + (int(played_seconds_) % 60 < 10 ? "0" : "") + std::to_string(int(played_seconds_) % 60)
            );
        }
        ui.set_text("progress-error", progress_message_);
        ui.set_visible("progress-error", !progress_message_.empty());
        ui.set_text("settings-button", show_settings_ ? "Скрыть настройки" : "Настройки");
        ui.set_visible("settings", show_settings_);
        ui.set_text("volume-label", std::format("{:.0f} %", settings_.volume * 100));
        ui.set_text("sensitivity-label", std::format("{:.1f}", settings_.sensitivity * 1000));
        ui.set_text("fov-label", std::format("{:.0f}°", settings_.fov));
        ui.set_text("camera-motion-label", std::format("{:.0f} %", settings_.camera_motion * 100));
        ui.set_value("volume", std::to_string(settings_.volume));
        ui.set_value("sensitivity", std::to_string(settings_.sensitivity * 1000));
        ui.set_value("fov", std::to_string(settings_.fov));
        ui.set_value("camera-motion", std::to_string(settings_.camera_motion));
        ui.set_value("vsync", settings_.vsync ? "1" : "0");
        ui.set_value("hdr", settings_.hdr_enabled ? "1" : "0");
        ui.set_value("bloom", settings_.bloom_enabled ? "1" : "0");
        ui.set_value("ssao", settings_.ssao_enabled ? "1" : "0");
        ui.set_value("ssr", settings_.ssr_enabled ? "1" : "0");
        ui.set_value("volumetrics", settings_.volumetrics_enabled ? "1" : "0");
        ui.set_value("ssgi-quality", std::to_string(settings_.ssgi_quality));
        ui.set_value("reflection-quality", std::to_string(settings_.reflection_quality));
        ui.set_value("dof-quality", std::to_string(settings_.dof_quality));
        ui.set_value("motion-blur-quality", std::to_string(settings_.motion_blur_quality));
        ui.set_value("taa", settings_.taa_enabled ? "1" : "0");
        ui.set_value("dynamic-resolution", settings_.dynamic_resolution_enabled ? "1" : "0");
        ui.set_value("render-scale", std::to_string(settings_.render_scale));
        ui.set_text(
            "render-scale-label",
            std::to_string(static_cast<int>(std::lround(settings_.render_scale * 100))) + "%"
        );
        ui.set_text("settings-message", settings_message_);
        ui.set_visible("controls", !complete);
        ui.set_visible("update-notice", main && update_notification_);
        ui.set_text("update-notice-text", std::string{"Игра обновлена до версии "} + RELAYCORE_GAME_VERSION);
        ui.set_visible("update-section", main && !development_sandbox_);
        ui.set_disabled("check-update", update_check_.valid() || !can_update_game());
        ui.set_text("check-update", update_check_.valid() ? "Проверяем обновления…" : "Проверить обновления");
        ui.set_text(
            "update-message",
            !can_update_game() && update_message_.empty() ? "Обновления доступны в упакованной сборке."
                                                          : update_message_
        );
        ui.set_visible("update-available", update_release_ && update_release_->available);
        if (update_release_)
        {
            ui.set_text(
                "update-size",
                "Загрузка: " + std::to_string(double(update_release_->bytes) / 1048576.0) + " МБ"
            );
            ui.set_text("update-notes", update_release_->notes);
        }
    }
} // namespace RelayGame
