#include "relaycore/app/engine_services.h"
#include "relaycore/core/paths.h"
#include "relaycore/render/renderer.h"
#include "relaycore/world/blockout_loader.h"
#include "relaycore/world/surface_material_utils.h"
#include "relaygame/level_editor.h"
#include "relaygame/level_session_builder.h"

#include <SDL3/SDL_dialog.h>
#include <SDL3/SDL_error.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <imgui.h>
#include <sstream>
#include <stdexcept>

namespace RelayGame
{
    void LevelEditor::refresh_catalogs()
    {
        map_files_.clear();
        model_files_.clear();
        texture_files_.clear();
        for (const auto& folder : {RelayCore::core::asset_path("levels"), user_maps_path_})
            if (std::filesystem::is_directory(folder))
                for (const auto& file : std::filesystem::directory_iterator(folder))
                    if (file.is_regular_file() && file.path().extension() == ".toml")
                        map_files_.push_back(file.path());
        const auto models = RelayCore::core::asset_path("models");
        if (std::filesystem::is_directory(models))
            for (const auto& file : std::filesystem::recursive_directory_iterator(models))
            {
                if (!file.is_regular_file())
                    continue;
                const auto path =
                    std::filesystem::relative(file.path(), RelayCore::core::asset_root_path()).generic_string();
                if (file.path().extension() == ".obj")
                    model_files_.push_back(path);
                if (file.path().extension() == ".bmp")
                    texture_files_.push_back(path);
            }
        std::ranges::sort(map_files_);
        map_files_.erase(std::unique(map_files_.begin(), map_files_.end()), map_files_.end());
        std::ranges::sort(model_files_);
        std::ranges::sort(texture_files_);
    }

    void LevelEditor::open_map(const std::filesystem::path& path, RelayCore::EngineServices& services, bool discard)
    {
        if (document_.dirty() && !discard)
            throw std::runtime_error("Сначала сохраните изменения или подтвердите их отмену.");
        const auto absolute = std::filesystem::absolute(path).lexically_normal();
        LevelEditorDocument staged;
        staged.reset(RelayCore::load_blockout_level(absolute));
        rebuild_scene(staged.level(), absolute, services);
        document_ = std::move(staged);
        const auto asset_relative = absolute.lexically_relative(RelayCore::core::asset_path("levels"));
        if (!asset_relative.empty() && *asset_relative.begin() != "..")
            save_path_ = user_maps_path_ / absolute.filename();
        else
        {
            user_maps_path_ = absolute.parent_path();
            save_path_ = absolute;
        }
        refresh_catalogs();
        std::snprintf(map_name_, sizeof(map_name_), "%s", save_path_.stem().string().c_str());
        preview_revision_ = document_.revision();
        validation_.clear();
        validated_revision_ = 0;
        placing_ = false;
        message_ = "Карта открыта. Папка работы: " + user_maps_path_.string();
        const auto spawn = std::ranges::find(
            document_.level().entities,
            std::string("player_spawn"),
            &RelayCore::EntitySpawnData::classname
        );
        if (spawn != document_.level().entities.end())
            services.camera().set_position(spawn->origin + glm::vec3{0, 1.65F, 0});
    }

    void LevelEditor::new_map(RelayCore::EngineServices& services, bool discard)
    {
        if (document_.dirty() && !discard)
            throw std::runtime_error("Сначала сохраните изменения или подтвердите их отмену.");
        LevelEditorDocument staged;
        staged.new_room();
        std::filesystem::path path;
        for (int i = 1;; ++i)
        {
            path = user_maps_path_ / ("room_" + std::to_string(i) + ".toml");
            if (!std::filesystem::exists(path))
                break;
        }
        rebuild_scene(staged.level(), path, services);
        document_ = std::move(staged);
        save_path_ = path;
        std::snprintf(map_name_, sizeof(map_name_), "%s", path.stem().string().c_str());
        preview_revision_ = document_.revision();
        validation_.clear();
        validated_revision_ = 0;
        placing_ = false;
        services.camera().set_position({0, 1.65F, -2});
        services.camera().set_target({0, 1.65F, 3});
        message_ = "Новая комната: старт, дверь с кнопкой и зона выхода. Добавьте оборудование и запустите F11.";
    }

    void LevelEditor::save_map(const std::filesystem::path& path, RelayCore::EngineServices& services)
    {
        const auto absolute = std::filesystem::absolute(path).lexically_normal();
        if (!save_path_.empty() && absolute.parent_path() != save_path_.parent_path())
            for (const auto& entity : document_.level().entities)
                if (entity.classname == "level_exit" && entity.properties.contains("next_map"))
                {
                    const auto& reference = entity.properties.at("next_map");
                    const std::filesystem::path authored{reference};
                    if (!authored.empty() && (*authored.begin() == "." || *authored.begin() == ".."))
                        throw std::runtime_error(
                            "Карта содержит переходы к соседним файлам. Перенесите всю папку карт вместе."
                        );
                }
        static_cast<void>(LevelSessionBuilder{services.config().gameplay}.build(document_.level(), absolute));
        document_.save(absolute);
        save_path_ = absolute;
        user_maps_path_ = absolute.parent_path();
        refresh_catalogs();
        message_ = "Карта сохранена: " + path.filename().string();
    }

    void LevelEditor::draw_map_tools(RelayCore::EngineServices& services)
    {
        bool requested_open = false;
        if (open_dialog_)
        {
            auto result = open_dialog_;
            std::lock_guard lock(result->mutex);
            if (result->finished)
            {
                if (!result->error.empty())
                    report_error(result->error);
                else if (!result->path.empty())
                {
                    pending_new_ = false;
                    pending_map_ = result->path;
                    requested_open = true;
                }
                open_dialog_.reset();
            }
        }
        const auto perform_pending = [&] {
            if (pending_new_)
                new_map(services, true);
            else
                open_map(pending_map_, services, true);
        };
        if (ImGui::Button("Новая комната"))
        {
            pending_new_ = true;
            pending_map_.clear();
            if (document_.dirty())
                ImGui::OpenPopup("Сменить карту?");
            else
                perform_pending();
        }
        ImGui::SameLine();
        if (ImGui::Button("Обновить список"))
            refresh_catalogs();
        if (ImGui::BeginCombo("Открыть карту", save_path_.filename().string().c_str()))
        {
            for (const auto& path : map_files_)
            {
                const auto label =
                    path.filename().string() + (path.parent_path() == user_maps_path_ ? " — моя" : " — образец");
                if (ImGui::Selectable(label.c_str()))
                {
                    pending_new_ = false;
                    pending_map_ = path;
                    requested_open = true;
                }
            }
            ImGui::EndCombo();
        }
        ImGui::BeginDisabled(static_cast<bool>(open_dialog_));
        if (ImGui::Button("Открыть файл из папки..."))
        {
            static constexpr SDL_DialogFileFilter filters[] = {{"Карты RelayCore", "toml"}};
            open_dialog_ = std::make_shared<OpenDialogResult>();
            auto* token = new std::shared_ptr<OpenDialogResult>(open_dialog_);
            SDL_ShowOpenFileDialog(
                [](void* userdata, const char* const* files, int) {
                    std::unique_ptr<std::shared_ptr<OpenDialogResult>> token{
                        static_cast<std::shared_ptr<OpenDialogResult>*>(userdata)
                    };
                    auto& result = **token;
                    std::lock_guard lock(result.mutex);
                    try
                    {
                        if (!files)
                            result.error = SDL_GetError();
                        else if (*files)
                            result.path = std::filesystem::path{reinterpret_cast<const char8_t*>(*files)};
                    } catch (const std::exception& error)
                    {
                        result.error = error.what();
                    }
                    result.finished = true;
                },
                token,
                services.window().handle(),
                filters,
                1,
                nullptr,
                false
            );
        }
        ImGui::EndDisabled();
        if (requested_open)
        {
            if (document_.dirty())
                ImGui::OpenPopup("Сменить карту?");
            else
            {
                perform_pending();
                pending_map_.clear();
            }
        }
        if (ImGui::BeginPopupModal("Сменить карту?", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::TextUnformatted("В текущей карте есть изменения.");
            if (ImGui::Button("Сохранить и открыть"))
            {
                try
                {
                    save_map(save_path_, services);
                    perform_pending();
                    pending_map_.clear();
                    ImGui::CloseCurrentPopup();
                } catch (const std::exception& error)
                {
                    report_error(error.what());
                }
            }
            if (ImGui::Button("Открыть без сохранения"))
            {
                try
                {
                    perform_pending();
                    pending_map_.clear();
                    ImGui::CloseCurrentPopup();
                } catch (const std::exception& error)
                {
                    report_error(error.what());
                }
            }
            ImGui::SameLine();
            if (ImGui::Button("Отмена"))
            {
                pending_map_.clear();
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
        if (ImGui::Button("Сохранить [Ctrl+S]") || (ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S)))
            save_map(save_path_, services);
        ImGui::InputText("Имя копии", map_name_, sizeof(map_name_));
        const auto copy_path = [&] {
            const std::string name(map_name_);
            if (name.empty() || name.find_first_of("/\\:*?\"<>|") != std::string::npos || name == "." || name == "..")
                throw std::runtime_error("Укажите имя карты без разделителей пути.");
            return user_maps_path_ / (name.ends_with(".toml") ? name : name + ".toml");
        };
        if (ImGui::Button("Сохранить как"))
        {
            const auto path = copy_path();
            if (std::filesystem::exists(path) && path != save_path_)
                ImGui::OpenPopup("Заменить карту?");
            else
                save_map(path, services);
        }
        if (ImGui::BeginPopupModal("Заменить карту?", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::TextUnformatted("Карта с таким именем уже существует.");
            if (ImGui::Button("Заменить"))
            {
                try
                {
                    save_map(copy_path(), services);
                    ImGui::CloseCurrentPopup();
                } catch (const std::exception& error)
                {
                    report_error(error.what());
                }
            }
            ImGui::SameLine();
            if (ImGui::Button("Отмена"))
                ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
        }
        if (ImGui::Button("Проверить карту"))
        {
            validation_ = document_.validation_messages(save_path_);
            validated_revision_ = document_.revision();
        }
        if (validated_revision_)
        {
            if (validated_revision_ != document_.revision())
                ImGui::TextWrapped("Карта изменена — повторите проверку.");
            else if (validation_.empty())
                ImGui::TextWrapped("Проверка пройдена. Проверьте прохождение через F11.");
            for (const auto& issue : validation_)
                ImGui::BulletText("%s", issue.c_str());
        }
    }

    void LevelEditor::draw_asset_tools(glm::vec3 position)
    {
        if (ImGui::Button("Охранный робот"))
            document_.add_enemy(position);
        if (ImGui::TreeNode("Переход между картами"))
        {
            if (ImGui::BeginCombo("Следующая карта", next_map_.c_str()))
            {
                for (const auto& path : map_files_)
                    if (ImGui::Selectable(path.filename().string().c_str()))
                    {
                        next_map_ = make_level_exit_reference(save_path_, path);
                        entry_names_.clear();
                        next_entry_.clear();
                        try
                        {
                            for (const auto& entity : RelayCore::load_blockout_level(path).entities)
                                if (entity.classname == "level_entry" && entity.properties.contains("targetname"))
                                    entry_names_.push_back(entity.properties.at("targetname"));
                        } catch (const std::exception& error)
                        {
                            report_error(error.what());
                        }
                        if (!entry_names_.empty())
                            next_entry_ = entry_names_.front();
                    }
                ImGui::EndCombo();
            }
            if (ImGui::BeginCombo("Вход", next_entry_.c_str()))
            {
                for (const auto& name : entry_names_)
                    if (ImGui::Selectable(name.c_str()))
                        next_entry_ = name;
                ImGui::EndCombo();
            }
            if (ImGui::Button("Добавить вход на эту карту"))
                document_.add_level_link(position, true, {}, {});
            ImGui::BeginDisabled(next_entry_.empty());
            if (ImGui::Button("Добавить переход"))
                document_.add_level_link(position, false, next_map_, next_entry_);
            ImGui::EndDisabled();
            if (const auto selected = document_.selection();
                selected && selected->entity && document_.level().entities[selected->index].classname == "level_exit")
            {
                ImGui::BeginDisabled(next_entry_.empty());
                if (ImGui::Button("Обновить выбранный переход"))
                {
                    document_.set_property("next_map", next_map_);
                    document_.set_property("next_entry", next_entry_);
                }
                ImGui::EndDisabled();
            }
            ImGui::TreePop();
        }
        if (ImGui::Button("Зона выхода / этап задания"))
            document_.add_volume(position, true);
        if (ImGui::Button("Триггер для выбранной двери"))
            document_.add_volume(position, false);
        if (ImGui::Button("Задание: открыть выбранную дверь"))
            document_.add_door_objective();
        if (ImGui::BeginCombo("Модель", selected_model_.c_str()))
        {
            for (const auto& path : model_files_)
                if (ImGui::Selectable(path.c_str(), path == selected_model_))
                    selected_model_ = path;
            ImGui::EndCombo();
        }
        if (ImGui::BeginCombo("Текстура модели", selected_texture_.c_str()))
        {
            for (const auto& path : texture_files_)
                if (ImGui::Selectable(path.c_str(), path == selected_texture_))
                    selected_texture_ = path;
            ImGui::EndCombo();
        }
        if (ImGui::Button("Добавить модель"))
            document_.add_model(position, selected_model_, selected_texture_);
        if (document_.selection() && ImGui::Button("Выбранный объект — на поверхность"))
            placing_ = true;
    }

    void LevelEditor::draw_object_properties(RelayCore::EngineServices&)
    {
        const auto selected = document_.selection();
        if (!selected || !selected->entity)
            return;
        const auto entity = document_.level().entities[selected->index];
        const auto prop = [&](const char* key, const char* fallback = "") {
            const auto found = entity.properties.find(key);
            return found == entity.properties.end() ? std::string(fallback) : found->second;
        };
        const auto text = [&](const char* label, const char* key) {
            char value[512];
            std::snprintf(value, sizeof(value), "%s", prop(key).c_str());
            if (ImGui::InputText(label, value, sizeof(value), ImGuiInputTextFlags_EnterReturnsTrue))
                document_.set_property(key, value);
        };
        const auto number = [&](const char* label, const char* key, float fallback) {
            const auto value = prop(key);
            float n = value.empty() ? fallback : std::stof(value);
            if (ImGui::InputFloat(label, &n, .1F, 1, "%.2f", ImGuiInputTextFlags_EnterReturnsTrue))
            {
                if (!std::isfinite(n) || n <= 0)
                    throw std::runtime_error("Значение должно быть положительным.");
                document_.set_property(key, std::to_string(n));
            }
        };
        const auto vector = [&](const char* label, const char* key, const char* fallback) {
            glm::vec3 v{};
            std::istringstream input(prop(key, fallback));
            if (!(input >> v.x >> v.y >> v.z))
                throw std::runtime_error(std::string("Неверный вектор: ") + key);
            if (ImGui::InputFloat3(label, &v.x, "%.2f", ImGuiInputTextFlags_EnterReturnsTrue))
            {
                if (!std::isfinite(v.x) || !std::isfinite(v.y) || !std::isfinite(v.z))
                    throw std::runtime_error("Вектор должен содержать конечные числа.");
                if ((std::string_view(key) == "scale" || std::string_view(key) == "size")
                    && glm::any(glm::lessThanEqual(v, glm::vec3{0})))
                    throw std::runtime_error("Размер и масштаб должны быть положительными.");
                document_.set_property(
                    key,
                    std::to_string(v.x) + ' ' + std::to_string(v.y) + ' ' + std::to_string(v.z)
                );
            }
        };
        ImGui::SeparatorText("Параметры объекта");
        if (entity.classname == "enemy_rover")
        {
            number("Здоровье", "health", 60);
            number("Скорость", "speed", 1.6F);
            number("Дальность обнаружения", "detection_range", 10);
        }
        if (entity.classname == "level_entry")
        {
            text("Имя входа", "targetname");
            auto angles = entity.angles;
            if (ImGui::InputFloat3("Направление входа", &angles.x, "%.1f", ImGuiInputTextFlags_EnterReturnsTrue))
                document_.set_angles(angles);
        }
        if (entity.classname == "level_exit")
        {
            text("Следующая карта", "next_map");
            text("Имя входа", "next_entry");
            vector("Объём перехода", "size", "2 2 1");
            bool requires_completion = prop("requires_completion", "true") == "true";
            if (ImGui::Checkbox("После выполнения заданий", &requires_completion))
                document_.set_property("requires_completion", requires_completion ? "true" : "false");
        }
        if (entity.classname == "prop_static")
        {
            vector("Масштаб XYZ", "scale", "1 1 1");
            auto angles = entity.angles;
            if (ImGui::InputFloat3("Поворот Y/P/R", &angles.x, "%.1f", ImGuiInputTextFlags_EnterReturnsTrue))
                document_.set_angles(angles);
            text("Файл OBJ", "model");
            text("Файл BMP", "texture");
            bool collision = entity.properties.contains("collision_mins");
            if (ImGui::Checkbox("Коллизия: отдельный бокс", &collision))
                document_.set_model_collision(collision);
            if (collision)
            {
                vector("Бокс: минимум", "collision_mins", "-.25 0 -.25");
                vector("Бокс: максимум", "collision_maxs", ".25 1.5 .25");
                if (ImGui::BeginCombo("Материал коллизии", prop("collision_material", "Metal").c_str()))
                {
                    for (const char* material : {"Metal", "Concrete", "Tile"})
                        if (ImGui::Selectable(material))
                            document_.set_property("collision_material", material);
                    ImGui::EndCombo();
                }
            }
        }
        if (entity.classname == "render_light")
        {
            vector("Цвет RGB", "color", "1 .85 .65");
            number("Радиус", "radius", 6);
            number("Яркость", "intensity", 1.5F);
        }
        if (entity.classname == "gameplay_entity")
        {
            text("Имя объекта", "targetname");
            if (prop("kind") == "door")
            {
                vector("Сдвиг открывания", "move_offset", "2.2 0 0");
                number("Скорость", "move_speed", 1.5F);
            }
            if (prop("kind") == "panel" || prop("kind") == "terminal")
            {
                text("Подпись", "label");
                number("Дистанция использования", "use_radius", 2.25F);
                bool locked = prop("locked", "false") == "true";
                if (ImGui::Checkbox("Заблокирована", &locked))
                    document_.set_property("locked", locked ? "true" : "false");
            }
        }
        if (entity.classname == "mission_step")
        {
            text("Название задания", "title");
            text("Описание", "detail");
            if (ImGui::BeginCombo("Условие", prop("kind").c_str()))
            {
                for (const char* kind : {"zone", "door_open", "activate_target", "signal", "pickup"})
                    if (ImGui::Selectable(kind))
                        document_.set_property("kind", kind);
                ImGui::EndCombo();
            }
            if (ImGui::Button("Этап раньше"))
                document_.move_mission_step(-1);
            ImGui::SameLine();
            if (ImGui::Button("Этап позже"))
                document_.move_mission_step(1);
        }
        if (entity.classname == "gameplay_entity" || (entity.classname == "mission_step" && prop("kind") != "zone"))
        {
            if (ImGui::BeginCombo("Цель связи", prop("target").c_str()))
            {
                for (const auto& target : document_.level().entities)
                {
                    const auto name = target.properties.find("targetname");
                    if (name != target.properties.end()
                        && ImGui::Selectable(name->second.c_str(), prop("target") == name->second))
                        document_.set_property("target", name->second);
                }
                ImGui::EndCombo();
            }
            text("Цель / событие", "target");
        }
        if (ImGui::TreeNode("Связи выбранного объекта"))
        {
            const auto name = prop("targetname");
            for (std::size_t i = 0; i < document_.level().entities.size(); ++i)
            {
                const auto& other = document_.level().entities[i];
                for (const char* key : {"target", "chain_target", "required_target", "stop_target", "close_door"})
                {
                    const auto link = other.properties.find(key);
                    if (link == other.properties.end())
                        continue;
                    if (i == selected->index || (!name.empty() && link->second == name))
                    {
                        const auto label =
                            std::to_string(i + 1) + ": " + other.classname + " → " + link->second + " (" + key + ")";
                        if (ImGui::Selectable(label.c_str()))
                            document_.select(EditorSelection{true, i});
                    }
                }
            }
            ImGui::TreePop();
        }
    }
} // namespace RelayGame
