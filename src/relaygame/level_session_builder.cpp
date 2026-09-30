#include "relaygame/level_session_builder.h"

#include "relaycore/core/paths.h"
#include "relaycore/core/profiling.h"
#include "relaycore/gameplay/entity_spawn_parsing.h"
#include "relaycore/world/blockout_loader.h"
#include "relaycore/world/blockout_loader_validation.h"
#include "relaygame/level_session_authoring_warnings.h"
#include "relaygame/static_props.h"
#include "relaygame/world_audio_zones.h"

#include <algorithm>
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
        const auto player = RelayCore::physics::make_player_aabb(session.spawn_position, .3F, 1.8F);
        for (const auto* solids : {&session.level.solids, &session.level.prop_colliders})
            for (const auto& solid : *solids)
                if (RelayCore::physics::overlaps(player, {solid.mins, solid.maxs}, .001F))
                    throw std::runtime_error(path.string() + ": level_entry '" + entry + "' intersects solid geometry");
        for (const auto& exit : session.exits)
            if (RelayCore::physics::overlaps(player, exit.bounds, 0))
                throw std::runtime_error(
                    path.string() + ": level_entry '" + entry
                    + "' overlaps a level_exit; move the entry away from the transition volume"
                );
        return session;
    }

    auto LevelSessionBuilder::build(RelayCore::BlockoutLevel level, std::filesystem::path path) const -> LevelSession
    {
        RELAYCORE_PROFILE_FUNCTION();
        LevelSession session{};
        session.level_path = std::move(path);
        session.level = std::move(level);
        session.level.prop_colliders.clear();
        for (const auto& prop : build_static_props(session.level))
            if (prop.collider)
                session.level.prop_colliders.push_back(*prop.collider);
        RelayCore::detail::validate_loaded_blockout_level(session.level);

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

        RelayCore::gameplay::build_runtime_interactions(
            session.level,
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
        session.audio_zones = RelayGame::build_audio_zones(session.level, RelayCore::core::asset_root_path());
        session.environment = build_world_environment(session.level);
        return session;
    }
} // namespace RelayGame
