#include "relaycore/core/paths.h"
#include "relaycore/physics/collision_asset.h"
#include "relaycore/render/static_model.h"
#include "relaygame/level_editor_document.h"
#include "relaygame/level_session_builder.h"
#include "relaygame/static_props.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <queue>
#include <sstream>
#include <stdexcept>

namespace RelayGame
{
    void LevelEditorDocument::add_enemy(glm::vec3 position)
    {
        checkpoint();
        RelayCore::EntitySpawnData entity;
        entity.classname = "enemy_rover";
        entity.origin = position;
        entity.properties = {{"health", "60"}, {"speed", "1.6"}, {"detection_range", "10"}};
        level_.entities.push_back(std::move(entity));
        selection_ = EditorSelection{true, level_.entities.size() - 1};
        changed();
    }

    void LevelEditorDocument::cache_model(const std::string& path)
    {
        if (model_bounds_.contains(path))
            return;
        if (std::filesystem::path{path}.extension() == ".rcmodel")
        {
            const auto model = RelayCore::render::read_model_asset(RelayCore::core::asset_path(path));
            RelayCore::physics::Aabb box{
                glm::vec3{std::numeric_limits<float>::max()},
                glm::vec3{-std::numeric_limits<float>::max()}
            };
            for (const auto& part : model.parts)
                for (const auto& vertex : model.meshes[part.mesh].geometry.vertices)
                {
                    const auto position = glm::vec3{part.transform * glm::vec4{vertex.position, 1}};
                    box.mins = glm::min(box.mins, position);
                    box.maxs = glm::max(box.maxs, position);
                }
            model_bounds_.emplace(path, box);
            return;
        }
        const auto mesh = RelayCore::render::load_obj_mesh(RelayCore::core::asset_path(path));
        RelayCore::physics::Aabb box{mesh.vertices.front().position, mesh.vertices.front().position};
        for (const auto& vertex : mesh.vertices)
        {
            box.mins = glm::min(box.mins, vertex.position);
            box.maxs = glm::max(box.maxs, vertex.position);
        }
        model_bounds_.emplace(path, box);
    }

    void LevelEditorDocument::add_model(glm::vec3 position, const std::string& model, const std::string& texture)
    {
        cache_model(model);
        const bool native = std::filesystem::path{model}.extension() == ".rcmodel";
        if (!native && !std::filesystem::is_regular_file(RelayCore::core::asset_path(texture)))
            throw std::runtime_error("Текстура не найдена: " + texture);
        checkpoint();
        RelayCore::EntitySpawnData entity;
        entity.classname = "prop_static";
        entity.origin = position;
        entity.properties = {{"model", model}, {"scale", "1 1 1"}};
        if (!native)
            entity.properties["texture"] = texture;
        level_.entities.push_back(std::move(entity));
        selection_ = EditorSelection{true, level_.entities.size() - 1};
        changed();
    }

    void LevelEditorDocument::add_volume(glm::vec3 position, bool exit)
    {
        std::string target;
        if (!exit)
        {
            if (!selection_ || !selection_->entity)
                throw std::runtime_error("Выберите дверь для триггера.");
            const auto& door = level_.entities[selection_->index];
            if (!door.properties.contains("kind") || door.properties.at("kind") != "door")
                throw std::runtime_error("Выберите дверь для триггера.");
            target = door.properties.at("targetname");
        }
        checkpoint();
        RelayCore::EntitySpawnData entity;
        entity.classname = exit ? "mission_step" : "gameplay_entity";
        entity.origin = position;
        entity.properties = {{"kind", exit ? "zone" : "trigger"}, {"size", "2 2 2"}};
        if (exit)
        {
            entity.properties["title"] = "Добраться до выхода";
            entity.properties["detail"] = "Пройдите в отмеченную зону выхода.";
        } else
        {
            entity.properties["target"] = target;
            entity.properties["targetname"] = unique_name("editor_trigger");
            entity.properties["once"] = "true";
        }
        level_.entities.push_back(std::move(entity));
        selection_ = EditorSelection{true, level_.entities.size() - 1};
        changed();
    }

    void LevelEditorDocument::new_room()
    {
        reset({});
        add_solid({0, -.125F, 1}, {8, .25F, 10}, RelayCore::SurfaceMaterial::Floor);
        add_solid({0, 3.5F, 1}, {8, .25F, 10}, RelayCore::SurfaceMaterial::Ceiling);
        for (const float side : {-1.F, 1.F})
            add_solid({side * 4, 1.7F, 1}, {.25F, 3.4F, 10}, RelayCore::SurfaceMaterial::Wall);
        add_solid({0, 1.7F, -4}, {8, 3.4F, .25F}, RelayCore::SurfaceMaterial::Wall);
        add_solid({0, 1.7F, 6}, {8, 3.4F, .25F}, RelayCore::SurfaceMaterial::Wall);
        for (const float side : {-1.F, 1.F})
            add_solid({side * 2.5F, 1.7F, 3}, {3, 3.4F, .25F}, RelayCore::SurfaceMaterial::Wall);
        add_solid({0, 3.15F, 3}, {2, .5F, .25F}, RelayCore::SurfaceMaterial::Wall);
        place_spawn({0, .01F, -2});
        set_angles({0, 0, 0});
        add_door({0, 0, 3});
        add_light({0, 2.8F, 0});
        add_light({0, 2.8F, 4.5F});
        add_volume({0, 0, 4.5F}, true);
        undo_.clear();
        redo_.clear();
    }

    void LevelEditorDocument::add_level_link(
        glm::vec3 position,
        bool entry,
        const std::string& map,
        const std::string& destination_entry
    )
    {
        checkpoint();
        RelayCore::EntitySpawnData entity;
        entity.origin = position;
        entity.classname = entry ? "level_entry" : "level_exit";
        if (entry)
        {
            entity.properties["targetname"] = unique_name("entry");
            entity.has_angles = true;
        } else
            entity.properties = {
                {"size", "2 2 1"},
                {"next_map", map},
                {"next_entry", destination_entry},
                {"requires_completion", "true"}
            };
        level_.entities.push_back(std::move(entity));
        selection_ = EditorSelection{true, level_.entities.size() - 1};
        changed();
    }

    void LevelEditorDocument::set_model_collision(bool enabled)
    {
        if (!selection_ || !selection_->entity || level_.entities[selection_->index].classname != "prop_static")
            return;
        checkpoint();
        auto& entity = level_.entities[selection_->index];
        if (enabled)
        {
            entity.properties.erase("collision_asset");
            const auto box = model_bounds_.at(entity.properties.at("model"));
            const auto encode = [](glm::vec3 v) {
                return std::to_string(v.x) + ' ' + std::to_string(v.y) + ' ' + std::to_string(v.z);
            };
            entity.properties["collision_mins"] = encode(box.mins);
            entity.properties["collision_maxs"] = encode(box.maxs);
            entity.properties["collision_material"] = "Metal";
        } else
            for (const char* key : {"collision_mins", "collision_maxs", "collision_material"})
                entity.properties.erase(key);
        changed();
    }

    void LevelEditorDocument::set_model_collision_asset(const std::string& reference)
    {
        if (!selection_ || !selection_->entity || level_.entities[selection_->index].classname != "prop_static")
            return;
        auto candidate = level_;
        auto& properties = candidate.entities[selection_->index].properties;
        for (const char* key : {"collision_mins", "collision_maxs", "collision_material", "collision_asset"})
            properties.erase(key);
        if (!reference.empty())
            properties["collision_asset"] = reference;
        for (const auto& prop : build_static_props(candidate))
            if (prop.authored_entity_index == static_cast<int>(selection_->index) && !prop.collision_product.empty())
                static_cast<void>(
                    RelayCore::physics::read_collision_asset(prop.collision_product, properties.at("model"))
                );
        checkpoint();
        level_ = std::move(candidate);
        changed();
    }

    void LevelEditorDocument::add_door_objective()
    {
        if (!selection_ || !selection_->entity)
            throw std::runtime_error("Выберите дверь.");
        const auto door = level_.entities[selection_->index];
        if (!door.properties.contains("kind") || door.properties.at("kind") != "door")
            throw std::runtime_error("Выберите дверь.");
        checkpoint();
        RelayCore::EntitySpawnData step;
        step.classname = "mission_step";
        step.origin = door.origin;
        step.properties =
            {{"kind", "door_open"}, {"target", door.properties.at("targetname")}, {"title", "Открыть проход"}};
        const auto first_zone = std::ranges::find_if(level_.entities, [](const auto& e) {
            return e.classname == "mission_step" && e.properties.contains("kind") && e.properties.at("kind") == "zone";
        });
        const auto index = static_cast<std::size_t>(first_zone - level_.entities.begin());
        level_.entities.insert(first_zone, std::move(step));
        selection_ = EditorSelection{true, index};
        changed();
    }

    void LevelEditorDocument::move_mission_step(int direction)
    {
        if (!selection_ || !selection_->entity || level_.entities[selection_->index].classname != "mission_step")
            return;
        for (int i = static_cast<int>(selection_->index) + direction;
             i >= 0 && i < static_cast<int>(level_.entities.size());
             i += direction)
            if (level_.entities[i].classname == "mission_step")
            {
                checkpoint();
                std::swap(level_.entities[selection_->index], level_.entities[i]);
                selection_->index = static_cast<std::size_t>(i);
                changed();
                return;
            }
    }

    auto LevelEditorDocument::place_on_surface(glm::vec3 origin, glm::vec3 direction, float grid) -> bool
    {
        if (!selection_ || !(grid > 0))
            return false;
        float nearest = 100;
        const RelayCore::BlockoutSolid* surface = nullptr;
        for (std::size_t i = 0; i < level_.solids.size(); ++i)
        {
            if (*selection_ == EditorSelection{false, i})
                continue;
            const auto& solid = level_.solids[i];
            if (const auto distance =
                    RelayCore::physics::ray_intersection_distance(origin, direction, {solid.mins, solid.maxs}, nearest))
            {
                nearest = *distance;
                surface = &solid;
            }
        }
        if (!surface)
            return false;
        const auto hit = origin + direction * nearest;
        int axis = 0;
        float sign = 1, best = 100;
        for (int i = 0; i < 3; ++i)
            for (float side : {-1.F, 1.F})
            {
                const float distance = std::abs(hit[i] - (side < 0 ? surface->mins[i] : surface->maxs[i]));
                if (distance < best)
                {
                    best = distance;
                    axis = i;
                    sign = side;
                }
            }
        checkpoint();
        if (selection_->entity)
        {
            auto& entity = level_.entities[selection_->index];
            if (entity.properties.contains("mount_axis") && axis != 1)
            {
                const auto old_axis = entity.properties.at("mount_axis").back();
                entity.properties["mount_axis"] = std::string(sign > 0 ? "+" : "-") + (axis == 0 ? "x" : "z");
                if (old_axis != (axis == 0 ? 'x' : 'z'))
                {
                    glm::vec3 size{.3F, .5F, .15F};
                    if (entity.properties.contains("size"))
                    {
                        std::istringstream input(entity.properties.at("size"));
                        input >> size.x >> size.y >> size.z;
                    }
                    std::swap(size.x, size.z);
                    entity.properties["size"] =
                        std::to_string(size.x) + ' ' + std::to_string(size.y) + ' ' + std::to_string(size.z);
                }
            }
        }
        const auto box = bounds(*selection_);
        const auto size = box.maxs - box.mins;
        auto center = glm::round(hit / grid) * grid;
        center[axis] = hit[axis] + sign * (size[axis] * .5F + .005F);
        if (selection_->entity)
        {
            const auto& entity = level_.entities[selection_->index];
            if (entity.properties.contains("kind") && entity.properties.at("kind") == "door")
                for (auto& other : level_.entities)
                    if (other.properties.contains("target")
                        && other.properties.at("target") == entity.properties.at("targetname")
                        && other.properties.contains("kind") && other.properties.at("kind") == "panel")
                        other.origin += center - (box.mins + box.maxs) * .5F;
        }
        transform(center, size, false);
        return true;
    }

    auto LevelEditorDocument::validation_messages(const std::filesystem::path& source_map) const
        -> std::vector<std::string>
    {
        std::vector<std::string> messages;
        try
        {
            for (const auto& entity : level_.entities)
                if (entity.properties.contains("kind")
                    && (entity.properties.at("kind") == "panel" || entity.properties.at("kind") == "door_open"))
                {
                    const auto target = entity.properties.find("target");
                    if (target == entity.properties.end()
                        || std::ranges::none_of(level_.entities, [&](const auto& door) {
                               return door.properties.contains("kind") && door.properties.at("kind") == "door"
                                      && door.properties.contains("targetname")
                                      && door.properties.at("targetname") == target->second;
                           }))
                        messages.push_back(
                            "Отсутствует дверь-цель: "
                            + (target == entity.properties.end() ? std::string("<не задана>") : target->second)
                        );
                }
            const auto session = LevelSessionBuilder{RelayCore::core::GameplayConfig{}
            }.build(level_, source_map.empty() ? std::filesystem::path{"editor"} : source_map);
            for (const auto& exit : session.exits)
                try
                {
                    (void) LevelSessionBuilder{RelayCore::core::GameplayConfig{}
                    }.build_at_entry(exit.destination, exit.entry);
                } catch (const std::exception& error)
                {
                    messages.push_back(
                        "Переход в '" + exit.destination.string() + "', вход '" + exit.entry + "': " + error.what()
                    );
                }
            const auto spawn =
                std::ranges::find(level_.entities, std::string("player_spawn"), &RelayCore::EntitySpawnData::classname);
            if (spawn == level_.entities.end())
                messages.push_back("Нет старта игрока.");
            for (const auto& door : session.doors)
                if (door.blocks_when_closed
                    && std::ranges::none_of(
                        session.usables,
                        [&](const auto& usable) {
                            if (usable.target != door.name || !usable.enabled)
                                return false;
                            if (!usable.locked)
                                return true;
                            using namespace RelayCore::gameplay;
                            if (usable.signal_action != UsableSignalAction::Unlock
                                && usable.signal_action != UsableSignalAction::ToggleLock)
                                return false;
                            return std::ranges::any_of(
                                       session.usables,
                                       [&](const auto& source) {
                                           return source.enabled && !source.locked
                                                  && (source.chain_target == usable.name
                                                      || (source.action_kind == UsableActionKind::SignalTarget
                                                          && source.target == usable.name));
                                       }
                                   )
                                   || std::ranges::any_of(session.triggers, [&](const auto& trigger) {
                                          return trigger.action_kind == TriggerActionKind::SignalTarget
                                                 && trigger.target == usable.name;
                                      });
                        }
                    )
                    && std::ranges::none_of(session.triggers, [&](const auto& trigger) {
                           return trigger.target == door.name;
                       }))
                    messages.push_back("Закрытая дверь без действующей кнопки/триггера: " + door.name);
            // A bounded walkability check at the spawn elevation; stairs/jumps remain a playtest concern.
            if (spawn != level_.entities.end())
            {
                constexpr float cell = .5F;
                constexpr int limit = 128;
                int min_x = 0, max_x = 0, min_z = 0, max_z = 0;
                for (const auto* solids : {&session.level.solids, &session.level.prop_colliders})
                    for (const auto& solid : *solids)
                    {
                        min_x = std::min(min_x, static_cast<int>(std::floor((solid.mins.x - spawn->origin.x) / cell)));
                        max_x = std::max(max_x, static_cast<int>(std::ceil((solid.maxs.x - spawn->origin.x) / cell)));
                        min_z = std::min(min_z, static_cast<int>(std::floor((solid.mins.z - spawn->origin.z) / cell)));
                        max_z = std::max(max_z, static_cast<int>(std::ceil((solid.maxs.z - spawn->origin.z) / cell)));
                    }
                min_x = std::max(-limit, min_x);
                max_x = std::min(limit, max_x);
                min_z = std::max(-limit, min_z);
                max_z = std::min(limit, max_z);
                const int width = max_x - min_x + 1, depth = max_z - min_z + 1;
                std::vector<unsigned char> reachable(width * depth, 0);
                const auto position = [&](int index) {
                    return spawn->origin + glm::vec3{(index % width + min_x) * cell, 0, (index / width + min_z) * cell};
                };
                const auto walkable = [&](glm::vec3 feet) {
                    bool floor = false;
                    const auto player = RelayCore::physics::make_player_aabb(feet, .30F, 1.8F);
                    for (const auto* solids : {&session.level.solids, &session.level.prop_colliders})
                        for (const auto& solid : *solids)
                        {
                            if (RelayCore::physics::overlaps(player, {solid.mins, solid.maxs}, .015F))
                                return false;
                            if (feet.x >= solid.mins.x && feet.x <= solid.maxs.x && feet.z >= solid.mins.z
                                && feet.z <= solid.maxs.z && std::abs(feet.y - solid.maxs.y) < .15F)
                                floor = true;
                        }
                    return floor;
                };
                std::queue<int> pending;
                const int start = -min_z * width - min_x;
                if (walkable(spawn->origin))
                {
                    reachable[start] = 1;
                    pending.push(start);
                } else
                    messages.push_back("Старт пересекает геометрию или находится без опоры.");
                while (!pending.empty())
                {
                    const int index = pending.front();
                    pending.pop();
                    for (int delta : {-1, 1, -width, width})
                    {
                        const int next = index + delta;
                        if (next < 0 || next >= width * depth || (std::abs(delta) == 1 && index / width != next / width)
                            || reachable[next])
                            continue;
                        reachable[next] = 2;
                        if (walkable(position(next)))
                        {
                            reachable[next] = 1;
                            pending.push(next);
                        }
                    }
                }
                for (const auto& step : session.mission_definition.steps)
                    if (step.kind == MissionStepKind::EnterZone)
                    {
                        bool reached = false;
                        for (int i = 0; i < width * depth && !reached; ++i)
                            reached = reachable[i] == 1
                                      && RelayCore::physics::overlaps(
                                          RelayCore::physics::make_player_aabb(position(i), .3F, 1.8F),
                                          step.bounds,
                                          0
                                      );
                        if (!reached)
                            messages.push_back(
                                "Нет пешего пути на высоте старта к зоне: " + step.title
                                + " (проверьте в игре лестницы/прыжки)."
                            );
                    }
            }
            if (session.mission_definition.steps.empty())
                messages.push_back("Нет задания или зоны выхода.");
            for (const auto& warning : session.authoring_warnings)
                messages.push_back(warning.message);
        } catch (const std::exception& error)
        {
            messages.push_back(error.what());
        }
        return messages;
    }
} // namespace RelayGame
