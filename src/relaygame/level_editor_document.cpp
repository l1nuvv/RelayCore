#include "relaygame/level_editor_document.h"

#include "relaycore/core/paths.h"
#include "relaycore/render/static_model.h"
#include "relaycore/world/blockout_loader.h"
#include "relaycore/world/surface_material_utils.h"
#include "relaygame/static_props.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <toml++/toml.hpp>

namespace RelayGame
{
    namespace
    {
        auto property(const RelayCore::EntitySpawnData& entity, const char* key) -> std::string
        {
            const auto found = entity.properties.find(key);
            return found == entity.properties.end() ? std::string{} : found->second;
        }

        auto vector_text(glm::vec3 value) -> std::string
        {
            std::ostringstream stream;
            stream << std::setprecision(std::numeric_limits<float>::max_digits10) << value.x << ' ' << value.y << ' '
                   << value.z;
            return stream.str();
        }

        auto entity_size(const RelayCore::EntitySpawnData& entity) -> glm::vec3
        {
            if (entity.classname == "enemy_rover" || entity.classname == "enemy_shooter")
                return {.84F, .95F, .84F};
            const auto text = property(entity, "size");
            if (text.empty())
            {
                if (entity.classname == "player_spawn" || entity.classname == "level_entry")
                    return {0.6F, 1.8F, 0.6F};
                return glm::vec3{0.35F};
            }
            std::istringstream stream{text};
            glm::vec3 result{};
            if (!(stream >> result.x >> result.y >> result.z))
                throw std::runtime_error("Invalid entity size: " + text);
            return result;
        }

        auto bottom_origin(const RelayCore::EntitySpawnData& entity) -> bool
        {
            return entity.classname == "enemy_rover" || entity.classname == "enemy_shooter"
                   || entity.classname == "player_spawn" || entity.classname == "level_entry"
                   || entity.classname == "mission_step" || entity.classname == "level_exit"
                   || property(entity, "kind") == "door";
        }

        auto array(glm::vec3 value) -> toml::array
        {
            return toml::array{value.x, value.y, value.z};
        }
    } // namespace

    void LevelEditorDocument::reset(RelayCore::BlockoutLevel level)
    {
        auto previous_bounds = std::move(model_bounds_);
        model_bounds_.clear();
        try
        {
            for (const auto& entity : level.entities)
                if (entity.classname == "prop_static")
                    cache_model(entity.properties.at("model"));
        } catch (...)
        {
            model_bounds_ = std::move(previous_bounds);
            throw;
        }
        level_ = std::move(level);
        selection_.reset();
        undo_.clear();
        redo_.clear();
        changed();
        dirty_ = false;
    }

    void LevelEditorDocument::select(std::optional<EditorSelection> selection)
    {
        if (selection && selection->index >= (selection->entity ? level_.entities.size() : level_.solids.size()))
            throw std::out_of_range("Editor selection outside level");
        selection_ = selection;
    }

    auto LevelEditorDocument::visible_entity(std::size_t index) const -> bool
    {
        const auto& entity = level_.entities.at(index);
        return entity.classname == "enemy_rover" || entity.classname == "enemy_shooter"
               || entity.classname == "enemy_route_point" || entity.classname == "player_spawn"
               || entity.classname == "render_light" || entity.classname == "gameplay_entity"
               || entity.classname == "world_effect" || entity.classname == "prop_static"
               || entity.classname == "mission_step" || entity.classname == "level_entry"
               || entity.classname == "level_exit";
    }

    auto LevelEditorDocument::bounds(EditorSelection selection) const -> RelayCore::physics::Aabb
    {
        if (!selection.entity)
        {
            const auto& solid = level_.solids.at(selection.index);
            return {solid.mins, solid.maxs};
        }
        const auto& entity = level_.entities.at(selection.index);
        if (entity.classname == "prop_static")
        {
            if (const auto cached = entity_bounds_.find(selection.index); cached != entity_bounds_.end())
                return cached->second;
            const auto local = model_bounds_.at(entity.properties.at("model"));
            RelayCore::BlockoutLevel single;
            single.entities.push_back(entity);
            const auto transform = build_static_props(single).front().transform;
            RelayCore::physics::Aabb world{
                glm::vec3{std::numeric_limits<float>::max()},
                glm::vec3{std::numeric_limits<float>::lowest()}
            };
            for (int corner = 0; corner < 8; ++corner)
            {
                const glm::vec3 point = transform
                                        * glm::vec4{
                                            (corner & 1) ? local.maxs.x : local.mins.x,
                                            (corner & 2) ? local.maxs.y : local.mins.y,
                                            (corner & 4) ? local.maxs.z : local.mins.z,
                                            1
                                        };
                world.mins = glm::min(world.mins, point);
                world.maxs = glm::max(world.maxs, point);
            }
            entity_bounds_[selection.index] = world;
            return world;
        }
        const auto size = entity_size(entity);
        auto center = entity.origin;
        if (bottom_origin(entity))
            center.y += size.y * 0.5F;
        return {center - size * 0.5F, center + size * 0.5F};
    }

    auto LevelEditorDocument::pick(glm::vec3 origin, glm::vec3 direction) const -> std::optional<EditorSelection>
    {
        std::optional<EditorSelection> result;
        float nearest = 500.0F;
        const auto consider = [&](EditorSelection object) {
            const auto hit = RelayCore::physics::ray_intersection_distance(origin, direction, bounds(object), nearest);
            if (hit && *hit < nearest)
            {
                nearest = *hit;
                result = object;
            }
        };
        for (std::size_t i = 0; i < level_.solids.size(); ++i)
            consider({false, i});
        for (std::size_t i = 0; i < level_.entities.size(); ++i)
            if (visible_entity(i) && level_.entities[i].classname != "player_spawn"
                && level_.entities[i].classname != "level_entry" && level_.entities[i].classname != "render_light")
                consider({true, i});
        return result;
    }

    void LevelEditorDocument::checkpoint()
    {
        if (undo_.size() == 64)
            undo_.erase(undo_.begin());
        undo_.push_back({level_, selection_});
        redo_.clear();
    }

    void LevelEditorDocument::changed()
    {
        entity_bounds_.clear();
        level_.prop_colliders.clear();
        level_.solid_sources.clear();
        level_.entity_sources.clear();
        ++revision_;
        dirty_ = true;
    }

    void LevelEditorDocument::transform(glm::vec3 center, glm::vec3 size, bool remember)
    {
        if (!selection_)
            return;
        for (int axis = 0; axis < 3; ++axis)
            if (!std::isfinite(center[axis]) || !std::isfinite(size[axis]) || size[axis] <= 0)
                throw std::invalid_argument("Размер должен быть положительным, координаты — конечными числами.");
        if (remember)
            checkpoint();
        if (selection_->entity)
        {
            auto& entity = level_.entities.at(selection_->index);
            if (entity.classname == "prop_static")
            {
                const auto old = bounds(*selection_);
                entity.origin += center - (old.mins + old.maxs) * 0.5F;
                changed();
                return;
            }
            entity.origin = center;
            if (bottom_origin(entity))
                entity.origin.y -= size.y * 0.5F;
            if (entity.classname == "gameplay_entity" || entity.classname == "mission_step"
                || entity.classname == "level_exit")
                entity.properties["size"] = vector_text(size);
        } else
        {
            auto& solid = level_.solids.at(selection_->index);
            solid.mins = center - size * 0.5F;
            solid.maxs = center + size * 0.5F;
        }
        changed();
    }

    void LevelEditorDocument::material(RelayCore::SurfaceMaterial material)
    {
        if (!selection_ || selection_->entity)
            return;
        checkpoint();
        level_.solids.at(selection_->index).material = material;
        changed();
    }

    void LevelEditorDocument::add_solid(glm::vec3 center, glm::vec3 size, RelayCore::SurfaceMaterial material)
    {
        checkpoint();
        level_.solids.push_back({center - size * 0.5F, center + size * 0.5F, material});
        selection_ = EditorSelection{false, level_.solids.size() - 1};
        changed();
    }

    auto LevelEditorDocument::unique_name(const std::string& prefix) const -> std::string
    {
        for (std::size_t i = 1;; ++i)
        {
            const auto name = prefix + '_' + std::to_string(i);
            if (std::ranges::none_of(level_.entities, [&](const auto& entity) {
                    return property(entity, "targetname") == name;
                }))
                return name;
        }
    }

    void LevelEditorDocument::add_light(glm::vec3 position)
    {
        checkpoint();
        RelayCore::EntitySpawnData entity;
        entity.classname = "render_light";
        entity.origin = position;
        entity.properties = {{"color", "1 0.85 0.65"}, {"intensity", "1.5"}, {"radius", "6"}};
        level_.entities.push_back(std::move(entity));
        selection_ = EditorSelection{true, level_.entities.size() - 1};
        changed();
    }

    auto LevelEditorDocument::attach_decal(glm::vec3 origin, glm::vec3 direction) -> bool
    {
        if (!selection_ || !selection_->entity)
            return false;
        auto& entity = level_.entities[selection_->index];
        if (entity.classname != "world_effect" || property(entity, "kind") != "decal")
            return false;
        float nearest = 30;
        const RelayCore::BlockoutSolid* hit = nullptr;
        for (const auto& solid : level_.solids)
            if (const auto distance =
                    RelayCore::physics::ray_intersection_distance(origin, direction, {solid.mins, solid.maxs}, nearest))
            {
                nearest = *distance;
                hit = &solid;
            }
        if (!hit)
            return false;
        const auto point = origin + direction * nearest;
        int axis = 0;
        float sign = 1;
        float best = std::numeric_limits<float>::max();
        for (int i = 0; i < 3; ++i)
            for (const float side : {-1.0F, 1.0F})
            {
                const float face = side < 0 ? hit->mins[i] : hit->maxs[i];
                const float distance = std::abs(point[i] - face);
                if (distance < best)
                {
                    best = distance;
                    axis = i;
                    sign = side;
                }
            }
        checkpoint();
        entity.origin = point;
        entity.origin[axis] += sign * 0.008F;
        entity.angles = {
            axis == 0               ? sign * 90
            : axis == 2 && sign < 0 ? 180.0F
                                    : 0.0F,
            axis == 1 ? -sign * 90 : 0.0F,
            0
        };
        entity.has_angles = true;
        changed();
        return true;
    }

    void LevelEditorDocument::add_effect(glm::vec3 position, const std::string& kind)
    {
        checkpoint();
        RelayCore::EntitySpawnData entity;
        entity.classname = "world_effect";
        entity.origin = position;
        entity.properties = {{"kind", kind}, {"scale", "1"}};
        if (kind == "decal")
            entity.properties["mark"] = "arrow";
        level_.entities.push_back(std::move(entity));
        selection_ = EditorSelection{true, level_.entities.size() - 1};
        changed();
    }

    void LevelEditorDocument::set_angles(glm::vec3 angles)
    {
        if (!selection_ || !selection_->entity)
            return;
        for (int axis = 0; axis < 3; ++axis)
            if (!std::isfinite(angles[axis]))
                throw std::invalid_argument("Углы должны быть конечными числами.");
        checkpoint();
        auto& entity = level_.entities[selection_->index];
        entity.angles = angles;
        entity.has_angles = true;
        changed();
    }

    void LevelEditorDocument::add_door(glm::vec3 position)
    {
        checkpoint();
        const auto name = unique_name("editor_door");
        RelayCore::EntitySpawnData door;
        door.classname = "gameplay_entity";
        door.origin = position;
        door.properties = {{"kind", "door"}, {"targetname", name}, {"size", "2 2.9 0.2"}, {"move_offset", "2.2 0 0"}};
        RelayCore::EntitySpawnData button;
        button.classname = "gameplay_entity";
        button.origin = position + glm::vec3{1.3F, 1.3F, -0.25F};
        button.properties = {
            {"kind", "panel"},
            {"targetname", unique_name("editor_button")},
            {"size", "0.3 0.5 0.15"},
            {"target", name},
            {"mount_axis", "-z"}
        };
        level_.entities.push_back(std::move(door));
        level_.entities.push_back(std::move(button));
        selection_ = EditorSelection{true, level_.entities.size() - 2};
        changed();
    }

    void LevelEditorDocument::place_spawn(glm::vec3 position)
    {
        checkpoint();
        for (std::size_t i = 0; i < level_.entities.size(); ++i)
            if (level_.entities[i].classname == "player_spawn")
            {
                level_.entities[i].origin = position;
                selection_ = EditorSelection{true, i};
                changed();
                return;
            }
        RelayCore::EntitySpawnData spawn;
        spawn.classname = "player_spawn";
        spawn.origin = position;
        level_.entities.push_back(std::move(spawn));
        selection_ = EditorSelection{true, level_.entities.size() - 1};
        changed();
    }

    void LevelEditorDocument::add_button(glm::vec3 position)
    {
        if (!selection_ || !selection_->entity || property(level_.entities[selection_->index], "kind") != "door")
            throw std::runtime_error("Сначала выберите дверь, которую будет открывать новая кнопка.");
        const auto target = property(level_.entities[selection_->index], "targetname");
        checkpoint();
        RelayCore::EntitySpawnData button;
        button.classname = "gameplay_entity";
        button.origin = position;
        button.properties = {
            {"kind", "panel"},
            {"targetname", unique_name("editor_button")},
            {"size", "0.3 0.5 0.15"},
            {"target", target},
            {"mount_axis", "-z"}
        };
        level_.entities.push_back(std::move(button));
        selection_ = EditorSelection{true, level_.entities.size() - 1};
        changed();
    }

    void LevelEditorDocument::set_property(const std::string& key, const std::string& value)
    {
        if (!selection_ || !selection_->entity)
            return;
        const auto& selected = level_.entities.at(selection_->index);
        if (selected.classname == "prop_static")
        {
            RelayCore::BlockoutLevel candidate;
            candidate.entities.push_back(selected);
            candidate.entities.front().properties[key] = value;
            static_cast<void>(build_static_props(candidate));
            if (key == "model")
                cache_model(value);
        }
        const auto old_name = key == "targetname" ? property(selected, "targetname") : std::string{};
        if (key == "targetname" && (value.empty() || std::ranges::any_of(level_.entities, [&](const auto& entity) {
                                        return &entity != &selected && property(entity, "targetname") == value;
                                    })))
            throw std::runtime_error("Имя должно быть непустым и уникальным.");
        checkpoint();
        level_.entities.at(selection_->index).properties[key] = value;
        if (!old_name.empty() && old_name != value)
            for (auto& entity : level_.entities)
                for (const char* link : {"target", "chain_target", "required_target", "stop_target", "close_door"})
                    if (property(entity, link) == old_name)
                        entity.properties[link] = value;
        changed();
    }

    void LevelEditorDocument::drop_to_floor()
    {
        if (!selection_)
            return;
        const auto box = bounds(*selection_);
        auto center = (box.mins + box.maxs) * 0.5F;
        std::optional<float> floor;
        for (std::size_t i = 0; i < level_.solids.size(); ++i)
        {
            if (!selection_->entity && selection_->index == i)
                continue;
            const auto& solid = level_.solids[i];
            if (center.x >= solid.mins.x && center.x <= solid.maxs.x && center.z >= solid.mins.z
                && center.z <= solid.maxs.z && solid.maxs.y <= center.y && (!floor || solid.maxs.y > *floor))
                floor = solid.maxs.y;
        }
        if (!floor)
            throw std::runtime_error("Под объектом нет пола. Переместите его над комнатой.");
        center.y = *floor + (box.maxs.y - box.mins.y) * 0.5F;
        transform(center, box.maxs - box.mins);
    }

    void LevelEditorDocument::duplicate()
    {
        if (!selection_)
            return;
        if (selection_->entity && level_.entities[selection_->index].classname == "player_spawn")
            throw std::runtime_error("Старт игрока один. Переместите существующий маркер.");
        checkpoint();
        if (selection_->entity)
        {
            auto copy = level_.entities.at(selection_->index);
            const auto name = property(copy, "targetname");
            if (!name.empty())
                copy.properties["targetname"] = unique_name(name);
            copy.origin.x += 0.5F;
            level_.entities.push_back(std::move(copy));
            selection_->index = level_.entities.size() - 1;
        } else
        {
            auto copy = level_.solids.at(selection_->index);
            copy.mins.x += 0.5F;
            copy.maxs.x += 0.5F;
            level_.solids.push_back(copy);
            selection_->index = level_.solids.size() - 1;
        }
        changed();
    }

    void LevelEditorDocument::erase()
    {
        if (!selection_)
            return;
        if (selection_->entity)
        {
            const auto& entity = level_.entities.at(selection_->index);
            if (entity.classname == "player_spawn")
                throw std::runtime_error("Старт игрока нельзя удалить.");
            const auto name = property(entity, "targetname");
            if (!name.empty())
                for (const auto& other : level_.entities)
                    for (const char* key : {"target", "chain_target", "required_target"})
                        if (property(other, key) == name)
                            throw std::runtime_error("Объект используется другим объектом: " + name);
        }
        checkpoint();
        if (selection_->entity)
            level_.entities.erase(level_.entities.begin() + static_cast<std::ptrdiff_t>(selection_->index));
        else
            level_.solids.erase(level_.solids.begin() + static_cast<std::ptrdiff_t>(selection_->index));
        selection_.reset();
        changed();
    }

    void LevelEditorDocument::undo()
    {
        if (undo_.empty())
            return;
        redo_.push_back({level_, selection_});
        auto state = std::move(undo_.back());
        undo_.pop_back();
        level_ = std::move(state.level);
        selection_ = state.selection;
        changed();
    }

    void LevelEditorDocument::redo()
    {
        if (redo_.empty())
            return;
        undo_.push_back({level_, selection_});
        auto state = std::move(redo_.back());
        redo_.pop_back();
        level_ = std::move(state.level);
        selection_ = state.selection;
        changed();
    }

    void LevelEditorDocument::save(const std::filesystem::path& path)
    {
        toml::table root;
        toml::array solids;
        for (const auto& solid : level_.solids)
            solids.push_back(toml::table{
                {"mins", array(solid.mins)},
                {"maxs", array(solid.maxs)},
                {"material", RelayCore::canonical_surface_material_name(solid.material)},
                {"tint", array(solid.tint)},
                {"brightness", solid.brightness}
            });
        root.insert("solid", std::move(solids));
        toml::array entities;
        for (const auto& entity : level_.entities)
        {
            toml::table entry{{"classname", entity.classname}, {"origin", array(entity.origin)}};
            if (entity.has_angles)
                entry.insert("angles", array(entity.angles));
            for (const auto& [key, value] : entity.properties)
                entry.insert(key, value);
            entities.push_back(std::move(entry));
        }
        root.insert("entity", std::move(entities));
        std::filesystem::create_directories(path.parent_path());
        auto temporary = path;
        temporary += ".tmp";
        {
            std::ofstream stream{temporary, std::ios::binary | std::ios::trunc};
            stream.exceptions(std::ios::badbit | std::ios::failbit);
            stream << root << '\n';
            stream.close();
        }
        static_cast<void>(RelayCore::load_blockout_level(temporary));
        std::filesystem::rename(temporary, path);
        dirty_ = false;
    }
} // namespace RelayGame
