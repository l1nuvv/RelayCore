#include "relaygame/level_session_builder.h"

#include "relaycore/core/paths.h"
#include "relaycore/core/profiling.h"
#include "relaycore/physics/collision.h"
#include "relaycore/world/blockout_loader.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <glm/trigonometric.hpp>

namespace RelayGame
{
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

        [[nodiscard]] auto find_entity_property(const RelayCore::EntitySpawnData& entity, const std::string_view key)
            -> const std::string*
        {
            const auto found = entity.properties.find(std::string{key});
            return found == entity.properties.end() ? nullptr : &found->second;
        }

        [[nodiscard]] auto lowercase_copy(const std::string_view value) -> std::string
        {
            std::string normalized(value);
            std::ranges::transform(normalized, normalized.begin(), [](const unsigned char c) {
                return static_cast<char>(std::tolower(c));
            });
            return normalized;
        }

        [[nodiscard]] auto normalized_entity_kind(const RelayCore::EntitySpawnData& entity) -> std::string
        {
            if (const std::string* kind = find_entity_property(entity, "kind"); kind != nullptr && !kind->empty())
            {
                return lowercase_copy(*kind);
            }

            return lowercase_copy(entity.classname);
        }

        [[nodiscard]] auto format_entity_label(const RelayCore::EntitySpawnData& entity, const std::size_t entity_index)
            -> std::string
        {
            const std::string kind = normalized_entity_kind(entity);
            if (const std::string* targetname = find_entity_property(entity, "targetname");
                targetname != nullptr && !targetname->empty())
            {
                return kind + " '" + *targetname + "' [entity " + std::to_string(entity_index) + "]";
            }

            return kind + " [entity " + std::to_string(entity_index) + "]";
        }

        void assign_warning_source(
            RelayCore::render::AuthoringWarningDiagnostics& warning,
            const bool primary,
            const std::string& label,
            const RelayCore::AuthoredSourceLocation* source
        )
        {
            if (primary)
            {
                warning.primary_label = label;
                if (source != nullptr && source->valid())
                {
                    warning.primary_file_path =
                        source->has_placement() ? source->placement_file_path : source->file_path;
                    warning.primary_line =
                        static_cast<int>(source->has_placement() ? source->placement_line : source->line);
                    warning.primary_column =
                        static_cast<int>(source->has_placement() ? source->placement_column : source->column);
                }
                return;
            }

            warning.secondary_label = label;
            if (source != nullptr && source->valid())
            {
                warning.secondary_file_path = source->has_placement() ? source->placement_file_path : source->file_path;
                warning.secondary_line =
                    static_cast<int>(source->has_placement() ? source->placement_line : source->line);
                warning.secondary_column =
                    static_cast<int>(source->has_placement() ? source->placement_column : source->column);
            }
        }

        [[nodiscard]] auto warning_source_bucket(
            const RelayCore::AuthoredSourceLocation* primary_source,
            const RelayCore::AuthoredSourceLocation* secondary_source = nullptr
        ) -> std::string
        {
            const bool primary_instanced = primary_source != nullptr && primary_source->has_placement();
            const bool secondary_instanced = secondary_source != nullptr && secondary_source->has_placement();
            return (primary_instanced || secondary_instanced) ? std::string{"instanced"} : std::string{"direct"};
        }

        [[nodiscard]] auto collect_authoring_warnings(
            const RelayCore::BlockoutLevel& level,
            std::span<const RelayCore::gameplay::RuntimeDoor> doors,
            std::span<const RelayCore::gameplay::RuntimeTrigger> triggers
        ) -> std::vector<RelayCore::render::AuthoringWarningDiagnostics>
        {
            RELAYCORE_PROFILE_FUNCTION();
            std::vector<RelayCore::render::AuthoringWarningDiagnostics> warnings{};

            for (std::size_t trigger_index = 0; trigger_index < triggers.size(); ++trigger_index)
            {
                const RelayCore::gameplay::RuntimeTrigger& trigger = triggers[trigger_index];
                if (trigger.authored_entity_index < 0
                    || static_cast<std::size_t>(trigger.authored_entity_index) >= level.entities.size())
                {
                    continue;
                }

                const std::size_t trigger_entity_index = static_cast<std::size_t>(trigger.authored_entity_index);
                const RelayCore::EntitySpawnData& trigger_entity = level.entities[trigger_entity_index];
                const RelayCore::AuthoredSourceLocation* trigger_source =
                    trigger_entity_index < level.entity_sources.size() ? &level.entity_sources[trigger_entity_index]
                                                                       : nullptr;

                for (std::size_t door_index = 0; door_index < doors.size(); ++door_index)
                {
                    const RelayCore::gameplay::RuntimeDoor& door = doors[door_index];
                    if (door.authored_entity_index < 0
                        || static_cast<std::size_t>(door.authored_entity_index) >= level.entities.size())
                    {
                        continue;
                    }
                    if (!RelayCore::physics::overlaps(
                            trigger.bounds,
                            RelayCore::gameplay::current_door_bounds(door),
                            0.001F
                        ))
                    {
                        continue;
                    }

                    const std::size_t door_entity_index = static_cast<std::size_t>(door.authored_entity_index);
                    const RelayCore::EntitySpawnData& door_entity = level.entities[door_entity_index];
                    const RelayCore::AuthoredSourceLocation* door_source =
                        door_entity_index < level.entity_sources.size() ? &level.entity_sources[door_entity_index]
                                                                        : nullptr;

                    RelayCore::render::AuthoringWarningDiagnostics warning{};
                    warning.message = "Suspicious overlap between trigger "
                                      + format_entity_label(trigger_entity, trigger_entity_index) + " and door "
                                      + format_entity_label(door_entity, door_entity_index);
                    warning.source_bucket = warning_source_bucket(trigger_source, door_source);
                    assign_warning_source(
                        warning,
                        true,
                        format_entity_label(trigger_entity, trigger_entity_index),
                        trigger_source
                    );
                    assign_warning_source(
                        warning,
                        false,
                        format_entity_label(door_entity, door_entity_index),
                        door_source
                    );
                    warnings.push_back(std::move(warning));
                }

                for (std::size_t other_trigger_index = trigger_index + 1; other_trigger_index < triggers.size();
                     ++other_trigger_index)
                {
                    const RelayCore::gameplay::RuntimeTrigger& other_trigger = triggers[other_trigger_index];
                    if (other_trigger.authored_entity_index < 0
                        || static_cast<std::size_t>(other_trigger.authored_entity_index) >= level.entities.size())
                    {
                        continue;
                    }
                    if (!RelayCore::physics::overlaps(trigger.bounds, other_trigger.bounds, 0.001F))
                    {
                        continue;
                    }

                    const std::size_t other_entity_index =
                        static_cast<std::size_t>(other_trigger.authored_entity_index);
                    const RelayCore::EntitySpawnData& other_entity = level.entities[other_entity_index];
                    const RelayCore::AuthoredSourceLocation* other_source =
                        other_entity_index < level.entity_sources.size() ? &level.entity_sources[other_entity_index]
                                                                         : nullptr;

                    RelayCore::render::AuthoringWarningDiagnostics warning{};
                    warning.message = "Suspicious overlapping triggers "
                                      + format_entity_label(trigger_entity, trigger_entity_index) + " and "
                                      + format_entity_label(other_entity, other_entity_index);
                    warning.source_bucket = warning_source_bucket(trigger_source, other_source);
                    assign_warning_source(
                        warning,
                        true,
                        format_entity_label(trigger_entity, trigger_entity_index),
                        trigger_source
                    );
                    assign_warning_source(
                        warning,
                        false,
                        format_entity_label(other_entity, other_entity_index),
                        other_source
                    );
                    warnings.push_back(std::move(warning));
                }
            }

            return warnings;
        }
    } // namespace

    LevelSessionBuilder::LevelSessionBuilder(const RelayCore::core::GameplayConfig& config) noexcept : config_(config)
    {
        RELAYCORE_PROFILE_FUNCTION();
    }

    auto LevelSessionBuilder::build() const -> LevelSession
    {
        RELAYCORE_PROFILE_FUNCTION();
        LevelSession session{};
        session.level_path = resolve_level_path(config_);
        session.level = RelayCore::load_blockout_level(session.level_path);

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
        session.authoring_warnings = collect_authoring_warnings(session.level, session.doors, session.triggers);
        return session;
    }
} // namespace RelayGame
