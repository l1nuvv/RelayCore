#include "relaygame/level_editor.h"

#include "relaycore/app/engine_services.h"
#include "relaycore/app/frame_timer.h"
#include "relaycore/core/profiling.h"
#include "relaycore/platform/window.h"
#include "relaycore/render/renderer.h"
#include "relaycore/world/blockout_loader.h"
#include "relaycore/world/surface_material_utils.h"
#include "relaygame/level_session_builder.h"
#include "relaygame/save_game.h"

#include <SDL3/SDL.h>
#include <algorithm>
#include <cmath>
#include <glm/geometric.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <imgui.h>
#include <stdexcept>

namespace RelayGame
{
    namespace
    {
        auto project(glm::vec3 position, const RelayCore::render::PerspectiveCamera& camera) -> std::optional<ImVec2>
        {
            const auto clip = camera.view_projection_matrix() * glm::vec4{position, 1};
            if (clip.w <= 0.05F || clip.z < -clip.w)
                return {};
            const auto size = ImGui::GetIO().DisplaySize;
            return ImVec2{(clip.x / clip.w + 1) * size.x * 0.5F, (1 - clip.y / clip.w) * size.y * 0.5F};
        }

        auto mouse_ray(const RelayCore::render::PerspectiveCamera& camera) -> glm::vec3
        {
            const auto& io = ImGui::GetIO();
            const auto inverse = glm::inverse(camera.view_projection_matrix());
            const auto point =
                inverse
                * glm::vec4{2 * io.MousePos.x / io.DisplaySize.x - 1, 1 - 2 * io.MousePos.y / io.DisplaySize.y, 1, 1};
            return glm::normalize(glm::vec3{point} / point.w - camera.position());
        }

        auto object_label(const RelayCore::EntitySpawnData& entity) -> std::string
        {
            if (entity.classname == "player_spawn")
                return "Старт игрока";
            if (entity.classname == "render_light")
                return "Свет";
            const auto name = entity.properties.find("targetname");
            return name == entity.properties.end() ? entity.classname : name->second;
        }
    } // namespace

    void LevelEditor::enter(const RelayCore::BlockoutLevel& level, RelayCore::EngineServices& services)
    {
        RELAYCORE_PROFILE_ZONE_N("LevelEditor::enter");
        if (!ImGui::GetCurrentContext() || !services.config().renderer.imgui.enabled)
            throw std::runtime_error("Для редактора включите ImGui в настройках проекта.");
        if (save_path_.empty())
        {
            char* folder = SDL_GetPrefPath("RelayCore", "RelayCore");
            if (!folder)
                throw std::runtime_error(SDL_GetError());
            save_path_ =
                std::filesystem::path{reinterpret_cast<const char8_t*>(folder)} / "levels" / "editor_level.toml";
            SDL_free(folder);
            user_maps_path_ = save_path_.parent_path();
            refresh_catalogs();
        }
        if (!has_document_)
        {
            document_.reset(level);
            has_document_ = true;
        }
        resume(services);
    }

    void LevelEditor::resume(RelayCore::EngineServices& services)
    {
        rebuild(services);
        active_ = true;
        flying_ = false;
        play_requested_ = false;
        exit_requested_ = false;
        drag_axis_ = -1;
        message_ = "Игра на паузе. Сохранение создаёт отдельную копию карты.";
        services.window().set_relative_mouse_mode(false);
    }

    void LevelEditor::leave(RelayCore::EngineServices& services)
    {
        services.window().set_relative_mouse_mode(false);
        leave_scene();
    }

    void LevelEditor::leave_scene() noexcept
    {
        active_ = false;
        play_requested_ = false;
        flying_ = false;
        drag_axis_ = -1;
        scene_.shutdown();
    }

    void LevelEditor::shutdown() noexcept
    {
        scene_.shutdown();
        active_ = false;
        play_requested_ = false;
        exit_requested_ = false;
        suspend_camera();
        has_document_ = false;
        document_ = LevelEditorDocument{};
    }

    void LevelEditor::on_event(const SDL_Event& event, RelayCore::EngineServices& services)
    {
        if (event.type == SDL_EVENT_WINDOW_FOCUS_LOST)
        {
            flying_ = false;
            drag_axis_ = -1;
            services.window().set_relative_mouse_mode(false);
        }
        if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN && event.button.button == SDL_BUTTON_RIGHT
            && event.button.x > panel_width_ && !ImGui::GetIO().WantCaptureMouse)
        {
            flying_ = true;
            services.window().set_relative_mouse_mode(true);
        }
        if (event.type == SDL_EVENT_MOUSE_BUTTON_UP && event.button.button == SDL_BUTTON_RIGHT)
        {
            flying_ = false;
            services.window().set_relative_mouse_mode(false);
        }
        if (event.type == SDL_EVENT_MOUSE_MOTION && flying_)
            services.camera().add_yaw_pitch(event.motion.xrel * 0.003F, -event.motion.yrel * 0.003F);
    }

    void LevelEditor::rebuild(RelayCore::EngineServices& services)
    {
        preview_revision_ = document_.revision();
        rebuild_scene(document_.level(), save_path_, services);
    }

    void LevelEditor::rebuild_scene(
        const RelayCore::BlockoutLevel& level,
        const std::filesystem::path& path,
        RelayCore::EngineServices& services
    )
    {
        RELAYCORE_PROFILE_ZONE_N("LevelEditor::rebuild_scene");
        auto session = LevelSessionBuilder{services.config().gameplay}.build(level, path);
        WorldSceneRenderer staged;
        staged.initialize(
            build_prepared_world_scene_data(
                level,
                services.renderer().authoring_config(),
                services.renderer().color_pipeline_mode()
            ),
            services.renderer().static_models()
        );
        scene_.shutdown();
        scene_ = std::move(staged);
        doors_ = std::move(session.doors);
        usables_ = std::move(session.usables);
        turrets_ = std::move(session.combat.turrets);
        enemies_ = std::move(session.enemies);
        environment_ = std::move(session.environment);
        light_targets_.clear();
        for (auto& effect : environment_)
        {
            if (!effect.required_target.empty())
                light_targets_.push_back(effect.required_target);
            effect.stop_target.clear();
        }
        update_world_environment(environment_, light_targets_, 0);
        update_world_environment(environment_, light_targets_, 2);
    }

    void LevelEditor::update(const RelayCore::FrameInfo& frame, RelayCore::EngineServices& services)
    {
        RELAYCORE_PROFILE_ZONE_N("LevelEditor::update");
        update_world_environment(environment_, light_targets_, static_cast<float>(frame.delta_seconds));
        if (flying_)
        {
            const bool* keys = SDL_GetKeyboardState(nullptr);
            const float distance = static_cast<float>(frame.delta_seconds) * (keys[SDL_SCANCODE_LSHIFT] ? 12.0F : 4.0F);
            services.camera().translate_local(
                distance * (keys[SDL_SCANCODE_W] - keys[SDL_SCANCODE_S]),
                distance * (keys[SDL_SCANCODE_D] - keys[SDL_SCANCODE_A]),
                distance * (keys[SDL_SCANCODE_E] - keys[SDL_SCANCODE_Q])
            );
        }
        if (preview_revision_ != document_.revision())
        {
            try
            {
                rebuild(services);
            } catch (const std::exception& error)
            {
                report_error(error.what());
            }
        }
    }

    void LevelEditor::render(const RelayCore::FrameInfo& frame, RelayCore::EngineServices& services)
    {
        RELAYCORE_PROFILE_ZONE_N("LevelEditor::render");
        scene_.render(
            services.renderer(),
            services.camera(),
            static_cast<float>(frame.elapsed_seconds),
            doors_,
            turrets_,
            usables_,
            -1,
            {},
            false,
            light_targets_,
            {},
            environment_,
            enemies_
        );
    }

    void LevelEditor::draw_selection(RelayCore::EngineServices& services)
    {
        auto& camera = services.camera();
        auto* draw = ImGui::GetForegroundDrawList();
        draw->PushClipRect({panel_width_, 0}, ImGui::GetIO().DisplaySize, true);
        for (std::size_t i = 0; i < document_.level().entities.size(); ++i)
        {
            const auto& entity = document_.level().entities[i];
            if (entity.classname != "render_light" && entity.classname != "player_spawn"
                && entity.classname != "mission_step" && entity.classname != "level_entry"
                && entity.classname != "level_exit"
                && !(entity.properties.contains("kind") && entity.properties.at("kind") == "trigger"))
                continue;
            const auto bounds = document_.bounds({true, i});
            if (auto point = project((bounds.mins + bounds.maxs) * 0.5F, camera))
            {
                draw->AddCircle(*point, 7, IM_COL32(255, 210, 100, 255), 12, 2);
                draw->AddText(
                    {point->x + 10, point->y},
                    IM_COL32(255, 230, 180, 255),
                    entity.classname == "render_light"   ? "Свет"
                    : entity.classname == "player_spawn" ? "Старт"
                                                         : "Зона / задание"
                );
            }
        }

        const auto& io = ImGui::GetIO();
        const bool scene_mouse = !io.WantCaptureMouse && io.MousePos.x > panel_width_ && !flying_;
        if (placing_)
        {
            draw->AddText(
                {panel_width_ + 20, 20},
                IM_COL32(255, 220, 100, 255),
                "ЛКМ: поставить на поверхность. Esc: оставить на месте."
            );
            if (ImGui::IsKeyPressed(ImGuiKey_Escape))
                placing_ = false;
            if (scene_mouse && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
            {
                try
                {
                    if (document_.place_on_surface(camera.position(), mouse_ray(camera), grid_))
                        placing_ = false;
                    else
                        message_ = "Под курсором нет поверхности.";
                } catch (const std::exception& error)
                {
                    report_error(error.what());
                    placing_ = false;
                }
            }
            draw->PopClipRect();
            return;
        }
        if (!document_.selection() || io.KeyCtrl)
            drag_axis_ = -1;
        bool handle_clicked = false;
        if (const auto selection = document_.selection())
        {
            if (selection->entity)
            {
                const auto& entities = document_.level().entities;
                for (std::size_t i = 0; i < entities.size(); ++i)
                    for (const char* key : {"target", "chain_target", "required_target", "stop_target", "close_door"})
                    {
                        const auto link = entities[i].properties.find(key);
                        if (link == entities[i].properties.end())
                            continue;
                        for (std::size_t j = 0; j < entities.size(); ++j)
                        {
                            const auto name = entities[j].properties.find("targetname");
                            if (name == entities[j].properties.end() || name->second != link->second
                                || (i != selection->index && j != selection->index))
                                continue;
                            const auto from = document_.bounds({true, i}), to = document_.bounds({true, j});
                            const auto a = project((from.mins + from.maxs) * .5F, camera),
                                       b = project((to.mins + to.maxs) * .5F, camera);
                            if (a && b)
                            {
                                draw->AddLine(*a, *b, IM_COL32(90, 220, 235, 230), 2);
                                draw->AddCircleFilled(*b, 5, IM_COL32(90, 220, 235, 230));
                                draw->AddText(*a, IM_COL32(90, 220, 235, 230), link->second.c_str());
                            }
                        }
                    }
            }
            const auto bounds = document_.bounds(*selection);
            const auto center = (bounds.mins + bounds.maxs) * 0.5F;
            const auto size = bounds.maxs - bounds.mins;
            for (int corner = 0; corner < 8; ++corner)
            {
                glm::vec3 from{};
                for (int axis = 0; axis < 3; ++axis)
                    from[axis] = (corner & (1 << axis)) ? bounds.maxs[axis] : bounds.mins[axis];
                for (int axis = 0; axis < 3; ++axis)
                    if (!(corner & (1 << axis)))
                    {
                        auto to = from;
                        to[axis] = bounds.maxs[axis];
                        const auto a = project(from, camera), b = project(to, camera);
                        if (a && b)
                            draw->AddLine(*a, *b, IM_COL32(255, 210, 70, 255), 2);
                    }
            }
            const float length = std::max(0.7F, glm::length(camera.position() - center) * 0.12F);
            const ImU32 colors[] =
                {IM_COL32(255, 100, 90, 255), IM_COL32(100, 240, 130, 255), IM_COL32(90, 175, 255, 255)};
            const auto base = project(center, camera);
            if (base)
                for (int axis = 0; axis < 3; ++axis)
                {
                    auto end = center;
                    end[axis] += length;
                    const auto tip = project(end, camera);
                    if (!tip)
                        continue;
                    draw->AddLine(*base, *tip, colors[axis], 4);
                    draw->AddCircleFilled(*tip, 6, colors[axis]);
                    const char label[] = {static_cast<char>('X' + axis), '\0'};
                    draw->AddText({tip->x + 8, tip->y - 8}, colors[axis], label);
                    const glm::vec2 line{tip->x - base->x, tip->y - base->y};
                    const glm::vec2 offset{io.MousePos.x - base->x, io.MousePos.y - base->y};
                    if (glm::dot(line, line) < 25)
                        continue;
                    const float t = std::clamp(glm::dot(offset, line) / glm::dot(line, line), 0.2F, 1.0F);
                    if (scene_mouse && ImGui::IsMouseClicked(ImGuiMouseButton_Left)
                        && glm::length(offset - line * t) < 10)
                    {
                        document_.checkpoint();
                        drag_axis_ = axis;
                        drag_mouse_ = {io.MousePos.x, io.MousePos.y};
                        drag_screen_axis_ = line / length;
                        drag_center_ = center;
                        drag_size_ = size;
                        handle_clicked = true;
                        break;
                    }
                }
        }
        if (drag_axis_ >= 0)
        {
            if (!ImGui::IsMouseDown(ImGuiMouseButton_Left))
                drag_axis_ = -1;
            else
            {
                const auto mouse_delta = glm::vec2{io.MousePos.x, io.MousePos.y} - drag_mouse_;
                const float units =
                    glm::dot(mouse_delta, drag_screen_axis_) / glm::dot(drag_screen_axis_, drag_screen_axis_);
                auto center = drag_center_;
                center[drag_axis_] += std::round(units / grid_) * grid_;
                const auto bounds = document_.bounds(*document_.selection());
                if (glm::length(center - (bounds.mins + bounds.maxs) * 0.5F) > 0.0001F)
                    document_.transform(center, drag_size_, false);
            }
        } else if (scene_mouse && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !handle_clicked)
        {
            document_.select(document_.pick(camera.position(), mouse_ray(camera)));
            // Light/spawn markers are selectable even when embedded in geometry.
            for (std::size_t i = 0; i < document_.level().entities.size(); ++i)
            {
                const auto& entity = document_.level().entities[i];
                if (entity.classname != "render_light" && entity.classname != "player_spawn"
                    && entity.classname != "level_entry")
                    continue;
                const auto bounds = document_.bounds({true, i});
                if (auto point = project((bounds.mins + bounds.maxs) * 0.5F, camera))
                    if (glm::length(glm::vec2{point->x - io.MousePos.x, point->y - io.MousePos.y}) < 10)
                        document_.select(EditorSelection{true, i});
            }
        }
        draw->PopClipRect();
    }

    void LevelEditor::draw_ui(RelayCore::EngineServices& services)
    {
        RELAYCORE_PROFILE_ZONE_N("LevelEditor::draw_ui");
        const auto& io = ImGui::GetIO();
        panel_width_ = std::min(350.0F, io.DisplaySize.x * 0.38F);
        ImGui::SetNextWindowPos({0, 0});
        ImGui::SetNextWindowSize({panel_width_, io.DisplaySize.y});
        ImGui::Begin(
            "Мастерская уровня",
            nullptr,
            ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse
        );
        try
        {
            ImGui::TextWrapped("ПКМ + WASD — полёт; Q/E — вниз/вверх. Shift — быстрее. ЛКМ — выбор. Потяни цветную "
                               "стрелку для перемещения.");
            ImGui::Separator();
            if (ImGui::Button("Играть с начала [F11]", {-1, 30}))
                request_play();
            if (ImGui::Button("Закрыть редактор", {-1, 28}))
                exit_requested_ = true;
            draw_map_tools(services);
            if (ImGui::Button("Отменить [Ctrl+Z]")
                || (io.KeyCtrl && !io.WantTextInput && ImGui::IsKeyPressed(ImGuiKey_Z)))
                document_.undo();
            ImGui::SameLine();
            if (ImGui::Button("Повторить") || (io.KeyCtrl && !io.WantTextInput && ImGui::IsKeyPressed(ImGuiKey_Y)))
                document_.redo();
            ImGui::TextUnformatted(document_.dirty() ? "Есть несохранённые изменения" : "Карта без изменений");
            ImGui::TextWrapped("%s", message_.c_str());
            if (ImGui::TreeNode("Путь сохранения"))
            {
                const auto utf8 = save_path_.u8string();
                ImGui::TextWrapped("%s", reinterpret_cast<const char*>(utf8.c_str()));
                ImGui::TreePop();
            }
            ImGui::SeparatorText("Добавить объект");
            ImGui::Checkbox("Размещать щелчком по поверхности", &place_by_click_);
            const auto object_count = document_.level().entities.size() + document_.level().solids.size();
            auto position = services.camera().position() + services.camera().forward() * 3.0F;
            position = glm::round(position / grid_) * grid_;
            if (ImGui::Button("Стена"))
                document_.add_solid(position, {3, 3, 0.25F}, RelayCore::SurfaceMaterial::Wall);
            ImGui::SameLine();
            if (ImGui::Button("Пол"))
                document_.add_solid(position, {3, 0.25F, 3}, RelayCore::SurfaceMaterial::Floor);
            ImGui::SameLine();
            if (ImGui::Button("Ящик"))
                document_.add_solid(position, {1, 1, 1}, RelayCore::SurfaceMaterial::Crate);
            if (ImGui::Button("Свет"))
                document_.add_light(position);
            ImGui::SameLine();
            if (ImGui::Button("Дверь + кнопка"))
                document_.add_door(position);
            if (ImGui::Button("Кнопка для выбранной двери"))
                document_.add_button(position);
            if (ImGui::Button("Перенести старт сюда"))
                document_.place_spawn(position);
            if (ImGui::Button("Декаль"))
                document_.add_effect(position, "decal");
            ImGui::SameLine();
            if (ImGui::Button("Вентилятор"))
                document_.add_effect(position, "fan");
            if (ImGui::Button("Пар"))
                document_.add_effect(position, "steam");
            ImGui::SameLine();
            if (ImGui::Button("Искры"))
                document_.add_effect(position, "sparks");
            draw_asset_tools(position);
            if (place_by_click_ && object_count != document_.level().entities.size() + document_.level().solids.size())
                placing_ = true;
            ImGui::SeparatorText("Размещение");
            const float grids[] = {0.1F, 0.25F, 0.5F, 1.0F};
            const char* labels[] = {"0.1 м", "0.25 м", "0.5 м", "1 м"};
            for (int i = 0; i < 4; ++i)
            {
                if (i)
                    ImGui::SameLine();
                if (ImGui::RadioButton(labels[i], grid_ == grids[i]))
                    grid_ = grids[i];
            }
            draw_object_properties(services);
            if (const auto selection = document_.selection())
            {
                auto bounds = document_.bounds(*selection);
                auto center = (bounds.mins + bounds.maxs) * 0.5F;
                auto size = bounds.maxs - bounds.mins;
                ImGui::Text("Выбрано: %s #%zu", selection->entity ? "объект" : "геометрия", selection->index + 1);
                if (ImGui::Button("Поставить на пол", {-1, 26}))
                    document_.drop_to_floor();
                ImGui::TextUnformatted("Сдвиг                  Размер");
                const bool resizable = !selection->entity
                                       || (document_.level().entities[selection->index].classname == "gameplay_entity"
                                           || document_.level().entities[selection->index].classname == "mission_step");
                for (int axis = 0; axis < 3; ++axis)
                {
                    ImGui::PushID(axis);
                    ImGui::Text("%c", 'X' + axis);
                    ImGui::SameLine();
                    if (ImGui::Button("−", {55, 25}))
                    {
                        center[axis] -= grid_;
                        document_.transform(center, size);
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("+", {55, 25}))
                    {
                        center[axis] += grid_;
                        document_.transform(center, size);
                    }
                    if (resizable)
                    {
                        ImGui::SameLine();
                        ImGui::BeginDisabled(size[axis] <= grid_);
                        if (ImGui::Button("−##size", {45, 25}))
                        {
                            size[axis] -= grid_;
                            document_.transform(center, size);
                        }
                        ImGui::EndDisabled();
                        ImGui::SameLine();
                        if (ImGui::Button("+##size", {45, 25}))
                        {
                            size[axis] += grid_;
                            document_.transform(center, size);
                        }
                    }
                    ImGui::PopID();
                }
                if (!selection->entity)
                {
                    const auto current = document_.level().solids[selection->index].material;
                    if (ImGui::BeginCombo("Текстура", RelayCore::canonical_surface_material_name(current).data()))
                    {
                        for (const auto material : RelayCore::all_surface_materials())
                            if (ImGui::Selectable(
                                    RelayCore::canonical_surface_material_name(material).data(),
                                    current == material
                                ))
                                document_.material(material);
                        ImGui::EndCombo();
                    }
                } else
                {
                    const auto& entity = document_.level().entities[selection->index];
                    if (entity.classname == "world_effect")
                    {
                        const auto prop = [&](const char* key, const char* fallback) {
                            const auto found = entity.properties.find(key);
                            return found == entity.properties.end() ? std::string{fallback} : found->second;
                        };
                        const auto mark = prop("mark", "arrow");
                        if (prop("kind", "") == "decal" && ImGui::BeginCombo("Маркировка", mark.c_str()))
                        {
                            for (const char* choice : {"arrow", "warning", "01", "02", "03"})
                                if (ImGui::Selectable(choice, mark == choice))
                                    document_.set_property("mark", choice);
                            ImGui::EndCombo();
                        }
                        if (ImGui::Button("Повернуть на 90°"))
                            document_.set_angles(entity.angles + glm::vec3{90, 0, 0});
                        float scale = std::stof(prop("scale", "1"));
                        if (ImGui::InputFloat(
                                "Масштаб",
                                &scale,
                                0.1F,
                                0.5F,
                                "%.2f",
                                ImGuiInputTextFlags_EnterReturnsTrue
                            )
                            && scale > 0)
                            document_.set_property("scale", std::to_string(scale));
                        if (prop("kind", "") == "decal" && ImGui::Button("На поверхность под прицелом"))
                            if (!document_.attach_decal(services.camera().position(), services.camera().forward()))
                                report_error("В пределах 30 м нет поверхности для декали.");
                    }
                    if (const auto kind = entity.properties.find("kind");
                        kind != entity.properties.end() && (kind->second == "panel" || kind->second == "terminal"))
                    {
                        const auto mount = entity.properties.find("mount_axis");
                        const std::string current = mount == entity.properties.end() ? "-z" : mount->second;
                        if (ImGui::BeginCombo("Сторона кнопки", current.c_str()))
                        {
                            for (const char* axis : {"-z", "+z", "-x", "+x"})
                                if (ImGui::Selectable(axis, current == axis))
                                    document_.set_property("mount_axis", axis);
                            ImGui::EndCombo();
                        }
                    }
                }
                if (ImGui::InputFloat3("Центр", &center.x, "%.2f", ImGuiInputTextFlags_EnterReturnsTrue))
                    document_.transform(center, size);
                if (!selection->entity
                    || (document_.level().entities[selection->index].classname == "gameplay_entity"
                        || document_.level().entities[selection->index].classname == "mission_step"))
                    if (ImGui::InputFloat3("Размер", &size.x, "%.2f", ImGuiInputTextFlags_EnterReturnsTrue))
                        document_.transform(center, size);
                if (ImGui::Button("Дубликат") || (io.KeyCtrl && !io.WantTextInput && ImGui::IsKeyPressed(ImGuiKey_D)))
                    document_.duplicate();
                ImGui::SameLine();
                if (ImGui::Button("Удалить") || (!io.WantTextInput && ImGui::IsKeyPressed(ImGuiKey_Delete)))
                    document_.erase();
            } else
                ImGui::TextWrapped("Нажми на стену или объект в сцене. Свет и старт отмечены кружками.");
            if (ImGui::TreeNode("Объекты уровня"))
            {
                for (std::size_t i = 0; i < document_.level().entities.size(); ++i)
                    if (document_.visible_entity(i))
                    {
                        ImGui::PushID(static_cast<int>(i));
                        if (ImGui::Selectable(
                                object_label(document_.level().entities[i]).c_str(),
                                document_.selection() == EditorSelection{true, i}
                            ))
                            document_.select(EditorSelection{true, i});
                        ImGui::PopID();
                    }
                ImGui::TreePop();
            }
        } catch (const std::exception& error)
        {
            report_error(error.what());
        }
        ImGui::End();
        draw_selection(services);
    }
} // namespace RelayGame
