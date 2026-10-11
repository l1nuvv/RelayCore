#include "relaygame/level_session_builder.h"

#include "relaycore/core/paths.h"
#include "relaycore/core/profiling.h"
#include "relaycore/gameplay/entity_spawn_parsing.h"
#include "relaycore/physics/collision_asset.h"
#include "relaycore/world/blockout_loader.h"
#include "relaycore/world/blockout_loader_validation.h"
#include "relaycore/world/world_collision.h"
#include "relaygame/level_session_authoring_warnings.h"
#include "relaygame/static_props.h"
#include "relaygame/world_audio_zones.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <glm/trigonometric.hpp>
#include <stdexcept>

namespace RelayGame
{
    auto resolve_level_exit_destination(const std::filesystem::path& source_map, std::string_view reference)
        -> std::filesystem::path
    {
        const std::filesystem::path authored{std::string{reference}};
        if (authored.is_absolute())
            return authored.lexically_normal(); // Existing maps may still contain absolute links.
        if (!authored.empty() && (*authored.begin() == "." || *authored.begin() == ".."))
            return (std::filesystem::absolute(source_map).parent_path() / authored).lexically_normal();
        return RelayCore::core::asset_path(authored.generic_string()).lexically_normal();
    }

    auto
    make_level_exit_reference(const std::filesystem::path& source_map, const std::filesystem::path& destination_map)
        -> std::string
    {
        const auto destination = std::filesystem::absolute(destination_map).lexically_normal();
        const auto asset_relative = destination.lexically_relative(RelayCore::core::asset_root_path());
        if (!asset_relative.empty() && *asset_relative.begin() != "..")
            return asset_relative.generic_string();
        const auto relative = destination.lexically_relative(std::filesystem::absolute(source_map).parent_path());
        if (relative.empty())
            throw std::runtime_error("Карты находятся на разных дисках; поместите связанные карты в одну папку.");
        const auto text = relative.generic_string();
        return *relative.begin() == ".." ? text : "./" + text;
    }

    namespace
    {
        [[nodiscard]] auto resolve_level_path(const RelayCore::core::GameplayConfig& config) -> std::filesystem::path
        {
            RELAYCORE_PROFILE_FUNCTION();
            const std::filesystem::path configured = config.level_path;
            if (configured.empty())
            {
                return RelayCore::core::asset_path("levels/test_room.toml");
            }
            if (configured.is_absolute())
            {
                return configured;
            }
            return RelayCore::core::asset_path(configured.generic_string());
        }

        [[nodiscard]] auto spawn_view_angles_from_entity(const RelayCore::EntitySpawnData& entity)
            -> std::optional<glm::vec2>
        {
            RELAYCORE_PROFILE_FUNCTION();
            if (!entity.has_angles)
            {
                return std::nullopt;
            }

            // Level-authored spawn angles are [yaw, pitch, roll] in degrees with yaw=0 facing +Z.
            return glm::vec2{
                glm::radians(90.0F - entity.angles.x),
                glm::radians(entity.angles.y),
            };
        }

        void validate_mesh_spawn(const LevelSession& session, const glm::vec3& position, std::string_view label)
        {
            if (session.physics_fixture
                && session.physics_fixture->overlap_box(position + glm::vec3{0, .9F, 0}, {.299F, .899F, .299F}))
                throw std::runtime_error(
                    session.level_path.string() + ": " + std::string{label} + " intersects native collision geometry"
                );
        }

        void validate_entry_spawn(const LevelSession& session, const glm::vec3& position, const std::string& label)
        {
            validate_mesh_spawn(session, position, label);
            const auto player = RelayCore::physics::make_player_aabb(position, .3F, 1.8F);
            for (const auto* solids : {&session.level.solids, &session.level.prop_colliders})
                for (const auto& solid : *solids)
                    if (RelayCore::physics::overlaps(player, {solid.mins, solid.maxs}, .001F))
                        throw std::runtime_error(
                            session.level_path.string() + ": " + label + " intersects solid geometry"
                        );
            for (const auto& exit : session.exits)
                if (RelayCore::physics::overlaps(player, exit.bounds, 0))
                    throw std::runtime_error(
                        session.level_path.string() + ": " + label
                        + " overlaps a level_exit; move the entry away from the transition volume"
                    );
        }

    } // namespace

    LevelSessionBuilder::LevelSessionBuilder(const RelayCore::core::GameplayConfig& config) noexcept : config_(config)
    {
        RELAYCORE_PROFILE_FUNCTION();
    }

    auto LevelSessionBuilder::build() const -> LevelSession
    {
        const auto path = resolve_level_path(config_);
        return build(path);
    }

    auto LevelSessionBuilder::build(const std::filesystem::path& path) const -> LevelSession
    {
        return build(RelayCore::load_blockout_level(path), path);
    }

    auto LevelSessionBuilder::build_at_entry(const std::filesystem::path& path, const std::string& entry) const
        -> LevelSession
    {
        auto session = build(path);
        const auto found = std::ranges::find_if(session.level.entities, [&](const auto& entity) {
            return entity.classname == "level_entry" && entity.properties.contains("targetname")
                   && entity.properties.at("targetname") == entry;
        });
        if (found == session.level.entities.end())
            throw std::runtime_error(path.string() + ": missing level_entry '" + entry + "'");
        session.spawn_position = found->origin;
        session.spawn_view_angles = spawn_view_angles_from_entity(*found);
        validate_entry_spawn(session, session.spawn_position, "level_entry '" + entry + "'");
        return session;
    }

    auto LevelSessionBuilder::build_collision_fixture(
        RelayCore::BlockoutLevel level,
        std::filesystem::path path,
        const std::filesystem::path& collision_product,
        std::string_view model_reference,
        const glm::mat4& transform
    ) const -> LevelSession
    {
        const auto asset = RelayCore::physics::read_collision_asset(collision_product, model_reference);
        const auto triangles = RelayCore::physics::transform_collision_triangles(asset.triangles, transform);
        const std::array meshes{
            RelayCore::physics::PhysicsMesh{triangles, {RelayCore::physics::WorldObjectKind::CollisionMesh, 0}}
        };
        return build_physics_fixture(std::move(level), std::move(path), meshes);
    }

    auto LevelSessionBuilder::build_physics_fixture(
        RelayCore::BlockoutLevel level,
        std::filesystem::path path,
        const std::span<const RelayCore::physics::PhysicsMesh> meshes
    ) const -> LevelSession
    {
        auto session = build(std::move(level), std::move(path));
        session.physics_fixture = std::make_shared<RelayCore::physics::PhysicsWorld>(session.world_colliders, meshes);
        for (const auto& retry : session.retry_spawns)
            validate_mesh_spawn(session, retry.position, "mission retry entry");
        return session;
    }

    auto LevelSessionBuilder::build(RelayCore::BlockoutLevel level, std::filesystem::path path) const -> LevelSession
    {
        RELAYCORE_PROFILE_FUNCTION();
        LevelSession session{};
        session.level_path = std::move(path);
        session.level = std::move(level);
        session.level.prop_colliders.clear();
        const auto props = build_static_props(session.level);
        for (const auto& prop : props)
            if (prop.collider)
                session.level.prop_colliders.push_back(*prop.collider);
        RelayCore::detail::validate_loaded_blockout_level(session.level);
        session.world_colliders = RelayCore::build_static_world_colliders(session.level);
        std::size_t collider_index = session.level.solids.size();
        for (const auto& prop : props)
            if (prop.collider)
                session.world_colliders[collider_index++].object.authored_entity_index = prop.authored_entity_index;

        std::vector<std::vector<RelayCore::physics::PhysicsTriangle>> triangles;
        std::vector<RelayCore::physics::PhysicsMesh> meshes;
        triangles.reserve(props.size());
        meshes.reserve(props.size());
        for (std::size_t index = 0; index < props.size(); ++index)
        {
            const auto& prop = props[index];
            if (prop.collision_product.empty())
                continue;
            const auto& model = session.level.entities[prop.authored_entity_index].properties.at("model");
            const auto asset = RelayCore::physics::read_collision_asset(prop.collision_product, model);
            triangles.push_back(RelayCore::physics::transform_collision_triangles(asset.triangles, prop.transform));
            meshes.push_back(
                {triangles.back(),
                 {RelayCore::physics::WorldObjectKind::CollisionMesh, index, prop.authored_entity_index}}
            );
        }
        if (!meshes.empty())
            session.physics_fixture =
                std::make_shared<RelayCore::physics::PhysicsWorld>(session.world_colliders, meshes);

        const auto spawn_it = std::find_if(
            session.level.entities.begin(),
            session.level.entities.end(),
            [this](const RelayCore::EntitySpawnData& entity) {
                return entity.classname == config_.spawn_classname;
            }
        );
        if (spawn_it != session.level.entities.end())
        {
            session.spawn_position = spawn_it->origin;
            session.spawn_view_angles = spawn_view_angles_from_entity(*spawn_it);
        }
        validate_mesh_spawn(session, session.spawn_position, "player spawn");

        // Mission conditions share kind names with interactions, but are not
        // world objects. Keep entity indices stable for runtime/save bindings.
        RelayCore::BlockoutLevel interaction_level;
        interaction_level.entities = session.level.entities;
        for (auto& entity : interaction_level.entities)
            if (entity.classname == "mission_step")
                entity.properties.erase("kind");
        RelayCore::gameplay::build_runtime_interactions(
            interaction_level,
            session.doors,
            session.triggers,
            session.usables
        );
        RelayCore::gameplay::rebuild_trigger_spatial_index(session.triggers, session.trigger_spatial_index);
        RelayCore::gameplay::rebuild_usable_spatial_index(session.usables, session.usable_spatial_index);
        session.runtime_registry.build(session.level, session.doors, session.triggers, session.usables);
        session.mission_definition = RelayGame::MissionBuilder{session.level}.build();
        session.authoring_warnings = detail::collect_authoring_warnings(
            session.level,
            session.doors,
            session.triggers,
            session.runtime_registry.bindings(),
            session.mission_definition
        );
        session.containment = build_containment_encounter(session.level, session.usables, session.doors);
        session.enemies = build_enemies(session.level);
        session.combat = RelayCore::gameplay::build_runtime_combat(session.level);
        for (const auto& entity : session.level.entities)
        {
            if (entity.classname == "level_exit")
            {
                const auto destination = entity.properties.find("next_map"),
                           entry = entity.properties.find("next_entry");
                const auto size = RelayCore::gameplay::detail::parse_vec3_property(entity, "size");
                if (destination == entity.properties.end() || destination->second.empty()
                    || entry == entity.properties.end() || entry->second.empty() || !size || !std::isfinite(size->x)
                    || !std::isfinite(size->y) || !std::isfinite(size->z)
                    || glm::any(glm::lessThanEqual(*size, glm::vec3{0})))
                    throw std::runtime_error(
                        session.level_path.string() + ": level_exit requires next_map, next_entry and positive size"
                    );
                bool completion = true;
                if (const auto value = entity.properties.find("requires_completion"); value != entity.properties.end())
                {
                    if (value->second != "true" && value->second != "false")
                        throw std::runtime_error("level_exit.requires_completion must be true or false");
                    completion = value->second == "true";
                }
                const auto center = entity.origin + glm::vec3{0, size->y * .5F, 0};
                session.exits.push_back(
                    {{center - *size * .5F, center + *size * .5F},
                     resolve_level_exit_destination(session.level_path, destination->second),
                     entry->second,
                     completion}
                );
            }
            if (entity.classname == "episode_end")
            {
                const auto name = entity.properties.find("close_door");
                if (session.ending || !entity.has_angles || name == entity.properties.end())
                    throw std::runtime_error("episode_end requires angles, close_door and a single marker per level");
                const auto door =
                    std::ranges::find(session.doors, name->second, &RelayCore::gameplay::RuntimeDoor::name);
                if (door == session.doors.end())
                    throw std::runtime_error("episode_end close_door does not name an existing door: " + name->second);
                session.ending = EpisodeEnding{
                    entity.origin,
                    *spawn_view_angles_from_entity(entity),
                    static_cast<std::size_t>(door - session.doors.begin())
                };
            }
            if (entity.classname != "render_light" && entity.classname != "audio_zone"
                && entity.classname != "world_effect")
            {
                continue;
            }
            for (const char* key : {"required_target", "stop_target"})
            {
                const auto required = entity.properties.find(key);
                if (required == entity.properties.end())
                {
                    continue;
                }
                const bool has_usable = std::ranges::any_of(session.usables, [&required](const auto& usable) {
                    return usable.action_kind == RelayCore::gameplay::UsableActionKind::ActivateTarget
                           && usable.target == required->second;
                });
                const bool has_trigger = std::ranges::any_of(session.triggers, [&required](const auto& trigger) {
                    return trigger.action_kind == RelayCore::gameplay::TriggerActionKind::ActivateTarget
                           && trigger.target == required->second;
                });
                if (required->second.empty() || (!has_usable && !has_trigger))
                {
                    throw std::runtime_error(
                        session.level_path.string() + ": " + entity.classname + " " + key + " '" + required->second
                        + "' needs a matching activate_target usable or trigger"
                    );
                }
            }
        }
        for (std::size_t index = 0; index < session.mission_definition.steps.size(); ++index)
        {
            const auto& name = session.mission_definition.steps[index].retry_entry;
            if (name.empty())
                continue;
            const auto matches = [&](const auto& entity) {
                const auto target = entity.properties.find("targetname");
                return entity.classname == "level_entry" && target != entity.properties.end() && target->second == name;
            };
            const auto found = std::ranges::find_if(session.level.entities, matches);
            if (found == session.level.entities.end())
                throw std::runtime_error(
                    session.level_path.string() + ": mission_step.retry_entry '" + name
                    + "' does not name a level_entry"
                );
            const auto& entry = *found;
            const auto label = "mission_step.retry_entry '" + name + "'";
            validate_entry_spawn(session, entry.origin, label);
            const auto player = RelayCore::physics::make_player_aabb(entry.origin, .3F, 1.8F);
            for (const auto& door : session.doors)
                if (RelayCore::physics::overlaps(
                        player,
                        {glm::min(door.closed_bounds.mins, door.closed_bounds.mins + door.open_offset),
                         glm::max(door.closed_bounds.maxs, door.closed_bounds.maxs + door.open_offset)},
                        .001F
                    ))
                    throw std::runtime_error(session.level_path.string() + ": " + label + " intersects door geometry");
            session.retry_spawns.push_back({index + 1, entry.origin, spawn_view_angles_from_entity(entry)});
        }
        session.audio_zones = RelayGame::build_audio_zones(session.level, RelayCore::core::asset_root_path());
        session.environment = build_world_environment(session.level);
        return session;
    }
} // namespace RelayGame
