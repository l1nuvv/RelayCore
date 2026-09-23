#pragma once

#include "relaycore/core/profiling.h"
#include "relaycore/physics/collision.h"
#include "relaycore/render/render_diagnostics.h"
#include "relaycore/world/blockout_loader_entity_props.h"
#include "relaygame/mission_runtime.h"
#include "relaygame/runtime_model.h"

#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace RelayGame::detail
{
    [[nodiscard]] inline auto warning_entity_kind(const RelayCore::EntitySpawnData& entity) -> std::string
    {
        if (const std::string kind = RelayCore::detail::normalized_entity_kind(entity); !kind.empty())
        {
            return kind;
        }

        return RelayCore::detail::normalize_lower(entity.classname);
    }

    [[nodiscard]] inline auto
    format_entity_label(const RelayCore::EntitySpawnData& entity, const std::size_t entity_index) -> std::string
    {
        const std::string kind = warning_entity_kind(entity);
        if (const std::string* targetname = RelayCore::detail::find_entity_property(entity, "targetname");
            targetname != nullptr && !targetname->empty())
        {
            return kind + " '" + *targetname + "' [entity " + std::to_string(entity_index) + "]";
        }

        return kind + " [entity " + std::to_string(entity_index) + "]";
    }

    inline void assign_warning_source(
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
                warning.primary_file_path = source->has_placement() ? source->placement_file_path : source->file_path;
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
            warning.secondary_line = static_cast<int>(source->has_placement() ? source->placement_line : source->line);
            warning.secondary_column =
                static_cast<int>(source->has_placement() ? source->placement_column : source->column);
        }
    }

    [[nodiscard]] inline auto warning_source_bucket(
        const RelayCore::AuthoredSourceLocation* primary_source,
        const RelayCore::AuthoredSourceLocation* secondary_source = nullptr
    ) -> std::string
    {
        const bool primary_instanced = primary_source != nullptr && primary_source->has_placement();
        const bool secondary_instanced = secondary_source != nullptr && secondary_source->has_placement();
        return (primary_instanced || secondary_instanced) ? std::string{"instanced"} : std::string{"direct"};
    }

    [[nodiscard]] inline auto format_binding_label(const RelayGame::RuntimeBindingInfo& binding) -> std::string
    {
        const std::string_view source_kind = RelayGame::runtime_object_kind_label(binding.source_kind);
        if (!binding.source_name.empty())
        {
            return std::string{source_kind} + " '" + binding.source_name + "'";
        }
        if (!binding.source_handle.empty())
        {
            return binding.source_handle;
        }
        return std::string{source_kind};
    }

    inline void assign_binding_source(
        RelayCore::render::AuthoringWarningDiagnostics& warning,
        const RelayGame::RuntimeBindingInfo& binding
    )
    {
        warning.primary_label = format_binding_label(binding);
        warning.primary_file_path = binding.source_file_path;
        warning.primary_line = binding.source_line;
        warning.primary_column = binding.source_column;
    }

    [[nodiscard]] inline auto
    mission_declares_target(const RelayGame::MissionDefinition& mission_definition, std::string_view target) -> bool
    {
        for (const RelayGame::MissionStep& step : mission_definition.steps)
        {
            if (step.target == target)
            {
                return true;
            }
        }

        return false;
    }

    [[nodiscard]] inline auto collect_authoring_warnings(
        const RelayCore::BlockoutLevel& level,
        std::span<const RelayCore::gameplay::RuntimeDoor> doors,
        std::span<const RelayCore::gameplay::RuntimeTrigger> triggers,
        std::span<const RelayGame::RuntimeBindingInfo> bindings,
        const RelayGame::MissionDefinition& mission_definition
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
            const RelayCore::AuthoredSourceLocation* trigger_source = trigger_entity_index < level.entity_sources.size()
                                                                          ? &level.entity_sources[trigger_entity_index]
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
                const RelayCore::AuthoredSourceLocation* door_source = door_entity_index < level.entity_sources.size()
                                                                           ? &level.entity_sources[door_entity_index]
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
                assign_warning_source(warning, false, format_entity_label(door_entity, door_entity_index), door_source);
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

                const std::size_t other_entity_index = static_cast<std::size_t>(other_trigger.authored_entity_index);
                const RelayCore::EntitySpawnData& other_entity = level.entities[other_entity_index];
                const RelayCore::AuthoredSourceLocation* other_source = other_entity_index < level.entity_sources.size()
                                                                            ? &level.entity_sources[other_entity_index]
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

        for (const RelayGame::RuntimeBindingInfo& binding : bindings)
        {
            if (!RelayGame::runtime_binding_requires_resolved_target(binding.kind) || binding.target.empty())
            {
                continue;
            }

            if (binding.resolved_target_count > 1)
            {
                RelayCore::render::AuthoringWarningDiagnostics warning{};
                warning.message =
                    "Ambiguous authored " + std::string{RelayGame::runtime_binding_kind_label(binding.kind)}
                    + " link: " + format_binding_label(binding) + " -> '" + binding.target + "' resolved to "
                    + std::to_string(binding.resolved_target_count) + " runtime objects";
                warning.source_bucket = binding.source_bucket.empty() ? std::string{"direct"} : binding.source_bucket;
                assign_binding_source(warning, binding);
                warnings.push_back(std::move(warning));
                continue;
            }

            if (binding.resolved_target_count > 0 || mission_declares_target(mission_definition, binding.target))
            {
                continue;
            }

            RelayCore::render::AuthoringWarningDiagnostics warning{};
            warning.message = "Unresolved authored " + std::string{RelayGame::runtime_binding_kind_label(binding.kind)}
                              + " link: " + format_binding_label(binding) + " -> '" + binding.target
                              + "' has no runtime object match and no mission-step contract";
            warning.source_bucket = binding.source_bucket.empty() ? std::string{"direct"} : binding.source_bucket;
            assign_binding_source(warning, binding);
            warnings.push_back(std::move(warning));
        }

        return warnings;
    }
} // namespace RelayGame::detail
