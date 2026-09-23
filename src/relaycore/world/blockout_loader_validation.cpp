#include "relaycore/world/blockout_loader_validation.h"

#include "relaycore/core/log.h"
#include "relaycore/world/blockout_loader_entity_props.h"

#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace RelayCore::detail
{
    namespace
    {
        struct ValidationBounds {
            glm::vec3 mins{};
            glm::vec3 maxs{};
        };

        [[nodiscard]] auto format_entity_label(const EntitySpawnData& entity, const std::size_t entity_index)
            -> std::string
        {
            const std::string kind = normalized_entity_kind(entity);
            if (const std::string* targetname = find_entity_property(entity, "targetname");
                targetname != nullptr && !targetname->empty())
            {
                return "'" + *targetname + "' (" + (kind.empty() ? std::string{"unknown"} : kind) + ")";
            }

            return (kind.empty() ? std::string{"entity"} : kind) + " at index " + std::to_string(entity_index);
        }

        void validate_positive_size(const std::string_view kind, const glm::vec3& size)
        {
            if (size.x <= 0.0F || size.y <= 0.0F || size.z <= 0.0F)
            {
                throw std::runtime_error(
                    "Entity of kind '" + std::string(kind)
                    + "' has non-positive size components; authored size must stay above zero"
                );
            }
        }

        [[nodiscard]] auto build_validation_bounds(const EntitySpawnData& entity) -> std::optional<ValidationBounds>
        {
            const std::string kind = normalized_entity_kind(entity);
            if (kind == "door")
            {
                const glm::vec3 size = parse_validation_size(entity, {1.0F, 2.0F, 0.20F}, "depth");
                validate_positive_size(kind, size);
                const glm::vec3 half_extents = {size.x * 0.5F, size.y * 0.5F, size.z * 0.5F};
                const glm::vec3 center = entity.origin + glm::vec3{0.0F, half_extents.y, 0.0F};
                return ValidationBounds{center - half_extents, center + half_extents};
            }

            if (kind == "trigger")
            {
                const glm::vec3 size = parse_validation_size(entity, {1.5F, 2.0F, 1.5F}, "depth");
                validate_positive_size(kind, size);
                const glm::vec3 half_extents = size * 0.5F;
                const glm::vec3 center = entity.origin + glm::vec3{0.0F, half_extents.y, 0.0F};
                return ValidationBounds{center - half_extents, center + half_extents};
            }

            return std::nullopt;
        }

        [[nodiscard]] auto bounds_overlap(const ValidationBounds& a, const ValidationBounds& b) -> bool
        {
            return a.mins.x < b.maxs.x && a.maxs.x > b.mins.x && a.mins.y < b.maxs.y && a.maxs.y > b.mins.y
                   && a.mins.z < b.maxs.z && a.maxs.z > b.mins.z;
        }

        void validate_entity_definition(const EntitySpawnData& entity, const std::size_t entity_index)
        {
            const std::string kind = normalized_entity_kind(entity);
            if (kind.empty())
            {
                return;
            }

            const bool is_pickup = kind == "pickup" || kind == "item" || kind == "keycard";
            const bool is_usable =
                kind == "panel" || kind == "button" || kind == "terminal" || kind == "console" || kind == "usable";
            const bool is_trigger = kind == "trigger";
            const auto has_non_empty_property = [&entity](const std::string_view key) {
                const std::string* value = find_entity_property(entity, key);
                return value != nullptr && !value->empty();
            };

            if (kind == "door")
            {
                const std::string* targetname = find_entity_property(entity, "targetname");
                if (targetname == nullptr || targetname->empty())
                {
                    throw std::runtime_error("Door entity requires a non-empty targetname");
                }

                const auto activation_policy = find_entity_property(entity, "activation_policy");
                const std::string normalized_policy =
                    activation_policy == nullptr ? std::string{} : normalize_lower(*activation_policy);
                if (activation_policy != nullptr && !activation_policy->empty() && normalized_policy != "toggle"
                    && normalized_policy != "open" && normalized_policy != "open_only"
                    && normalized_policy != "open-only")
                {
                    throw std::runtime_error(
                        "Door entity '" + *targetname + "' has unsupported activation_policy '" + *activation_policy
                        + "'"
                    );
                }
            }

            if (kind == "door" || kind == "trigger" || kind == "pickup" || kind == "item" || kind == "keycard")
            {
                (void) build_validation_bounds(entity);
            }

            if (kind == "panel" || kind == "button" || kind == "terminal" || kind == "console" || kind == "usable")
            {
                const glm::vec3 size = parse_validation_size(entity, {0.18F, 0.42F, 0.12F}, "depth");
                validate_positive_size(kind, size);

                const std::string* mount_axis = find_entity_property(entity, "mount_axis");
                if (mount_axis == nullptr || mount_axis->empty())
                {
                    mount_axis = find_entity_property(entity, "panel_axis");
                }
                if (mount_axis != nullptr && !mount_axis->empty() && !is_valid_panel_axis(*mount_axis))
                {
                    throw std::runtime_error(
                        "Entity " + format_entity_label(entity, entity_index) + " has unsupported mount axis '"
                        + *mount_axis + "'"
                    );
                }
            }

            const std::string action = normalized_entity_action(entity);
            if (!action.empty() && !is_supported_usable_action_name(action))
            {
                throw std::runtime_error(
                    "Entity " + format_entity_label(entity, entity_index) + " has unsupported action '" + action + "'"
                );
            }
            if (is_pickup && !action.empty() && !is_pickup_action_name(action))
            {
                throw std::runtime_error(
                    "Entity " + format_entity_label(entity, entity_index)
                    + " is a pickup but defines a non-pickup action '" + action + "'"
                );
            }
            if (is_usable_receiver_kind(kind) && is_pickup_action_name(action))
            {
                throw std::runtime_error(
                    "Entity " + format_entity_label(entity, entity_index)
                    + " uses pickup action, but only pickup entities may collect items"
                );
            }

            if (is_pickup)
            {
                const glm::vec3 size = parse_validation_size(entity, {0.22F, 0.12F, 0.02F}, "depth");
                validate_positive_size(kind, size);
            }

            if (!is_pickup
                && (has_non_empty_property("item_id") || has_non_empty_property("item")
                    || has_non_empty_property("item_name")))
            {
                throw std::runtime_error(
                    "Entity " + format_entity_label(entity, entity_index)
                    + " defines pickup item properties, but only pickup entities may author inventory items"
                );
            }
            if (is_pickup
                && (has_non_empty_property("required_item") || has_non_empty_property("requires")
                    || has_non_empty_property("required_item_name") || has_non_empty_property("required_item_label")))
            {
                throw std::runtime_error(
                    "Entity " + format_entity_label(entity, entity_index)
                    + " is a pickup and may not require another inventory item"
                );
            }
            if ((has_non_empty_property("required_item_name") || has_non_empty_property("required_item_label"))
                && !(has_non_empty_property("required_item") || has_non_empty_property("requires")))
            {
                throw std::runtime_error(
                    "Entity " + format_entity_label(entity, entity_index)
                    + " defines required_item_name without a required_item"
                );
            }
            if (is_pickup && (has_non_empty_property("starts_locked") || has_non_empty_property("locked")))
            {
                throw std::runtime_error(
                    "Entity " + format_entity_label(entity, entity_index) + " is a pickup and may not start locked"
                );
            }
            if (is_trigger && (has_non_empty_property("chain_target") || has_non_empty_property("on_activate_target")))
            {
                throw std::runtime_error(
                    "Entity " + format_entity_label(entity, entity_index)
                    + " defines chain_target, but trigger chaining is not supported"
                );
            }

            if (const std::string* signal_action = find_entity_property(entity, "signal_action");
                signal_action != nullptr && !signal_action->empty())
            {
                const std::string normalized_signal_action = normalize_lower(*signal_action);
                if (!is_usable_receiver_kind(kind))
                {
                    throw std::runtime_error(
                        "Entity " + format_entity_label(entity, entity_index)
                        + " defines signal_action, but only usable entities may receive signals"
                    );
                }
                if (!is_supported_signal_action_name(normalized_signal_action))
                {
                    throw std::runtime_error(
                        "Entity " + format_entity_label(entity, entity_index) + " has unsupported signal_action '"
                        + *signal_action + "'"
                    );
                }
            }

            if (const std::string* signal_response = find_entity_property(entity, "signal_response");
                signal_response != nullptr && !signal_response->empty())
            {
                const std::string normalized_signal_response = normalize_lower(*signal_response);
                if (!is_usable_receiver_kind(kind))
                {
                    throw std::runtime_error(
                        "Entity " + format_entity_label(entity, entity_index)
                        + " defines signal_response, but only usable entities may receive signals"
                    );
                }
                if (!is_supported_signal_action_name(normalized_signal_response))
                {
                    throw std::runtime_error(
                        "Entity " + format_entity_label(entity, entity_index) + " has unsupported signal_action '"
                        + *signal_response + "'"
                    );
                }
            }

            if (signal_action_requires_targetname(entity))
            {
                const std::string* targetname = find_entity_property(entity, "targetname");
                if (targetname == nullptr || targetname->empty())
                {
                    throw std::runtime_error(
                        "Entity " + format_entity_label(entity, entity_index)
                        + " defines signal_action but has no targetname to receive signals"
                    );
                }
            }

            if (is_trigger
                && (has_non_empty_property("required_item") || has_non_empty_property("requires")
                    || has_non_empty_property("required_item_name") || has_non_empty_property("required_item_label")))
            {
                throw std::runtime_error(
                    "Entity " + format_entity_label(entity, entity_index)
                    + " is a trigger and may not declare inventory requirements"
                );
            }
            if (!is_usable && !is_pickup && !is_trigger
                && (has_non_empty_property("single_use") || has_non_empty_property("once")))
            {
                throw std::runtime_error(
                    "Entity " + format_entity_label(entity, entity_index)
                    + " defines single-use interaction flags, but is not a usable or trigger"
                );
            }
        }

        [[nodiscard]] auto entity_requires_resolved_target(const EntitySpawnData& entity) -> bool
        {
            const std::string kind = normalized_entity_kind(entity);
            if (kind == "trigger")
            {
                return false;
            }

            if (kind == "panel" || kind == "button" || kind == "terminal" || kind == "console" || kind == "usable")
            {
                return !is_signal_action(entity) && !is_activate_target_action(entity);
            }

            return false;
        }

        [[nodiscard]] auto entity_requires_non_empty_target(const EntitySpawnData& entity) -> bool
        {
            const std::string kind = normalized_entity_kind(entity);
            if (kind == "trigger")
            {
                return true;
            }

            if (kind == "panel" || kind == "button" || kind == "terminal" || kind == "console" || kind == "usable")
            {
                return true;
            }

            return false;
        }

        [[nodiscard]] auto is_pickup_entity_kind(const std::string_view kind) -> bool
        {
            return kind == "pickup" || kind == "item" || kind == "keycard";
        }

        [[nodiscard]] auto is_usable_entity_kind(const std::string_view kind) -> bool
        {
            return kind == "panel" || kind == "button" || kind == "terminal" || kind == "console" || kind == "usable";
        }

        enum class ValidationMissionStepKind : std::uint8_t {
            DoorOpened = 0,
            EnterZone,
            AcquireItem,
            ActivateTarget,
            SignalTarget,
        };

        [[nodiscard]] auto parse_validation_mission_step_kind(const EntitySpawnData& entity)
            -> ValidationMissionStepKind
        {
            const std::string* kind = find_entity_property(entity, "kind");
            if (kind == nullptr || kind->empty())
            {
                throw std::runtime_error("mission_step.kind is required");
            }

            const std::string normalized = normalize_lower(*kind);
            if (normalized == "door_open" || normalized == "door-open" || normalized == "door")
            {
                return ValidationMissionStepKind::DoorOpened;
            }
            if (normalized == "zone" || normalized == "enter_zone" || normalized == "enter-zone")
            {
                return ValidationMissionStepKind::EnterZone;
            }
            if (normalized == "pickup" || normalized == "pickup_item" || normalized == "pickup-item"
                || normalized == "collect_item" || normalized == "collect-item" || normalized == "item")
            {
                return ValidationMissionStepKind::AcquireItem;
            }
            if (normalized == "activate_target" || normalized == "activate-target" || normalized == "activate")
            {
                return ValidationMissionStepKind::ActivateTarget;
            }
            if (normalized == "signal" || normalized == "signal_target" || normalized == "signal-target")
            {
                return ValidationMissionStepKind::SignalTarget;
            }

            throw std::runtime_error("Unsupported mission_step.kind value: " + *kind);
        }

    } // namespace

    void validate_loaded_blockout_level(const BlockoutLevel& level)
    {
        constexpr float kMinExtent = 0.0001F;

        for (std::size_t solid_index = 0; solid_index < level.solids.size(); ++solid_index)
        {
            const BlockoutSolid& solid = level.solids[solid_index];
            const glm::vec3 extents = solid.maxs - solid.mins;
            if (extents.x <= kMinExtent || extents.y <= kMinExtent || extents.z <= kMinExtent)
            {
                throw std::runtime_error(
                    "Degenerate solid at index " + std::to_string(solid_index) + " has non-positive extents"
                );
            }
        }

        std::unordered_map<std::string, std::string> target_name_kinds{};
        std::unordered_map<std::string, std::size_t> pickup_item_ids{};
        std::unordered_set<std::string> activatable_targets{};
        std::unordered_set<std::string> signal_receiver_names{};
        std::vector<std::pair<std::size_t, ValidationBounds>> door_bounds{};
        std::vector<std::pair<std::size_t, ValidationBounds>> trigger_bounds{};
        door_bounds.reserve(level.entities.size());
        trigger_bounds.reserve(level.entities.size());
        pickup_item_ids.reserve(level.entities.size());
        activatable_targets.reserve(level.entities.size());
        signal_receiver_names.reserve(level.entities.size());
        std::size_t pickup_index = 0;

        for (std::size_t entity_index = 0; entity_index < level.entities.size(); ++entity_index)
        {
            const EntitySpawnData& entity = level.entities[entity_index];
            const std::string kind = normalized_entity_kind(entity);
            validate_entity_definition(entity, entity_index);

            const std::string* targetname = find_entity_property(entity, "targetname");
            if (targetname != nullptr)
            {
                if (targetname->empty())
                {
                    throw std::runtime_error("Entity property 'targetname' must not be empty");
                }
                if (!target_name_kinds.emplace(*targetname, normalized_entity_kind(entity)).second)
                {
                    throw std::runtime_error("Duplicate entity targetname: " + *targetname);
                }
                if (signal_action_requires_targetname(entity))
                {
                    signal_receiver_names.insert(*targetname);
                }
            }

            if (entity.classname == "inventory_item")
            {
                const auto* id = find_entity_property(entity, "item_id");
                if (id == nullptr || id->empty() || !pickup_item_ids.emplace(*id, entity_index).second)
                    throw std::runtime_error("inventory_item requires a unique item_id");
            }
            if (is_pickup_entity_kind(kind))
            {
                const std::string item_id = resolved_pickup_item_id(entity, pickup_index++);
                if (!pickup_item_ids.emplace(item_id, entity_index).second)
                {
                    throw std::runtime_error("Duplicate pickup item_id: " + item_id);
                }
            }

            if ((is_usable_entity_kind(kind) || kind == "trigger") && !is_signal_action(entity))
            {
                if (const std::string* target = find_entity_property(entity, "target");
                    target != nullptr && !target->empty())
                {
                    activatable_targets.insert(*target);
                }
            }

            if (const auto bounds = build_validation_bounds(entity); bounds.has_value())
            {
                if (kind == "door")
                {
                    door_bounds.emplace_back(entity_index, *bounds);
                } else if (kind == "trigger")
                {
                    trigger_bounds.emplace_back(entity_index, *bounds);
                }
            }
        }

        for (std::size_t entity_index = 0; entity_index < level.entities.size(); ++entity_index)
        {
            const EntitySpawnData& entity = level.entities[entity_index];
            const std::string kind = normalized_entity_kind(entity);
            const std::string* target = find_entity_property(entity, "target");
            const std::string* chain_target = find_entity_property(entity, "chain_target");

            if (chain_target == nullptr || chain_target->empty())
            {
                chain_target = find_entity_property(entity, "on_activate_target");
            }

            if (entity_requires_non_empty_target(entity) && (target == nullptr || target->empty()))
            {
                throw std::runtime_error(
                    "Entity of kind '" + (kind.empty() ? std::string{"unknown"} : kind)
                    + "' requires a non-empty target property"
                );
            }

            if (entity_requires_resolved_target(entity) && target != nullptr && !target->empty()
                && !target_name_kinds.contains(*target))
            {
                throw std::runtime_error("Entity of kind '" + kind + "' references unknown target '" + *target + "'");
            }

            if (entity_requires_resolved_target(entity) && target != nullptr && !target->empty())
            {
                const auto found = target_name_kinds.find(*target);
                if (found != target_name_kinds.end() && found->second != "door")
                {
                    throw std::runtime_error(
                        "Entity of kind '" + kind + "' targets '" + *target + "', but that targetname belongs to a '"
                        + (found->second.empty() ? std::string{"unknown"} : found->second) + "' instead of a door"
                    );
                }
            }

            if ((kind == "trigger" || is_activate_target_action(entity)) && !is_signal_action(entity)
                && target != nullptr && !target->empty())
            {
                const auto found = target_name_kinds.find(*target);
                if (found != target_name_kinds.end() && found->second != "door")
                {
                    throw std::runtime_error(
                        "Entity of kind '" + kind + "' targets '" + *target + "', but that targetname belongs to a '"
                        + (found->second.empty() ? std::string{"unknown"} : found->second) + "' instead of a door"
                    );
                }
            }

            if (is_signal_action(entity) && target != nullptr && !target->empty())
            {
                const auto found = target_name_kinds.find(*target);
                if (found == target_name_kinds.end())
                {
                    throw std::runtime_error(
                        "Signal entity of kind '" + kind + "' references unknown target '" + *target + "'"
                    );
                }

                const bool targets_door = found->second == "door";
                const bool targets_signal_receiver = signal_receiver_names.contains(*target);
                if (!targets_door && !targets_signal_receiver)
                {
                    throw std::runtime_error(
                        "Signal entity of kind '" + kind + "' targets '" + *target
                        + "', but that targetname is neither a door nor a signal receiver"
                    );
                }
            }

            if (chain_target != nullptr && !chain_target->empty() && !target_name_kinds.contains(*chain_target))
            {
                throw std::runtime_error(
                    "Entity " + format_entity_label(entity, entity_index) + " references unknown chain_target '"
                    + *chain_target + "'"
                );
            }
            if (chain_target != nullptr && !chain_target->empty())
            {
                const auto found = target_name_kinds.find(*chain_target);
                const bool targets_door = found != target_name_kinds.end() && found->second == "door";
                const bool targets_signal_receiver = signal_receiver_names.contains(*chain_target);
                if (!targets_door && !targets_signal_receiver)
                {
                    throw std::runtime_error(
                        "Entity " + format_entity_label(entity, entity_index) + " references chain_target '"
                        + *chain_target + "', but that targetname is neither a door nor a signal receiver"
                    );
                }
            }

            if (const std::string* required_item = find_entity_property(entity, "required_item");
                required_item != nullptr && !required_item->empty() && !pickup_item_ids.contains(*required_item))
            {
                throw std::runtime_error(
                    "Entity " + format_entity_label(entity, entity_index) + " requires unknown item '" + *required_item
                    + "'"
                );
            }

            if (entity.classname == "mission_step")
            {
                const ValidationMissionStepKind step_kind = parse_validation_mission_step_kind(entity);
                if (step_kind == ValidationMissionStepKind::EnterZone)
                {
                    const glm::vec3 zone_size = parse_validation_size(entity, {2.4F, 2.2F, 2.4F}, "depth");
                    validate_positive_size("mission_step", zone_size);
                    continue;
                }

                const std::string step_target =
                    property_or(entity, "target", property_or(entity, "item", std::string{}));
                if (step_target.empty())
                {
                    throw std::runtime_error(
                        "mission_step target is required for door_open, pickup, activate_target and signal steps"
                    );
                }

                if (step_kind == ValidationMissionStepKind::DoorOpened)
                {
                    const auto found = target_name_kinds.find(step_target);
                    if (found == target_name_kinds.end())
                    {
                        throw std::runtime_error(
                            "mission_step door_open references unknown door target '" + step_target + "'"
                        );
                    }
                    if (found->second != "door")
                    {
                        throw std::runtime_error(
                            "mission_step door_open target '" + step_target + "' does not resolve to a door"
                        );
                    }
                } else if (step_kind == ValidationMissionStepKind::AcquireItem)
                {
                    if (!pickup_item_ids.contains(step_target))
                    {
                        throw std::runtime_error("mission_step pickup references unknown item '" + step_target + "'");
                    }
                } else if (step_kind == ValidationMissionStepKind::ActivateTarget)
                {
                    if (!activatable_targets.contains(step_target))
                    {
                        throw std::runtime_error(
                            "mission_step activate_target references unknown activatable target '" + step_target + "'"
                        );
                    }
                } else if (step_kind == ValidationMissionStepKind::SignalTarget)
                {
                    const auto found = target_name_kinds.find(step_target);
                    if (found == target_name_kinds.end())
                    {
                        throw std::runtime_error(
                            "mission_step signal references unknown signal target '" + step_target + "'"
                        );
                    }

                    const bool targets_door = found->second == "door";
                    const bool targets_signal_receiver = signal_receiver_names.contains(step_target);
                    if (!targets_door && !targets_signal_receiver)
                    {
                        throw std::runtime_error(
                            "mission_step signal target '" + step_target + "' is neither a door nor a signal receiver"
                        );
                    }
                }
            }
        }

        for (std::size_t trigger_index = 0; trigger_index < trigger_bounds.size(); ++trigger_index)
        {
            const auto& [trigger_entity_index, trigger_bound] = trigger_bounds[trigger_index];
            for (std::size_t door_index = 0; door_index < door_bounds.size(); ++door_index)
            {
                const auto& [door_entity_index, door_bound] = door_bounds[door_index];
                if (!bounds_overlap(trigger_bound, door_bound))
                {
                    continue;
                }

                log::warn(
                    "Suspicious overlap between trigger {} and door {} in loaded level",
                    format_entity_label(level.entities[trigger_entity_index], trigger_entity_index),
                    format_entity_label(level.entities[door_entity_index], door_entity_index)
                );
            }
        }

        for (std::size_t left = 0; left < trigger_bounds.size(); ++left)
        {
            for (std::size_t right = left + 1; right < trigger_bounds.size(); ++right)
            {
                if (!bounds_overlap(trigger_bounds[left].second, trigger_bounds[right].second))
                {
                    continue;
                }

                log::warn(
                    "Suspicious overlapping triggers {} and {} in loaded level",
                    format_entity_label(level.entities[trigger_bounds[left].first], trigger_bounds[left].first),
                    format_entity_label(level.entities[trigger_bounds[right].first], trigger_bounds[right].first)
                );
            }
        }
    }
} // namespace RelayCore::detail
