#include "relaycore/core/log.h"
#include "relaycore/core/profiling.h"
#include "relaycore/world/blockout_loader_detail.h"
#include "relaycore/world/blockout_loader_parse.h"
#include "relaycore/world/blockout_loader_transform.h"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cstdint>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <toml++/toml.hpp>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace RelayCore::detail
{
    namespace
    {
        [[nodiscard]] auto find_entity_property(const EntitySpawnData& entity, const std::string_view key)
            -> const std::string*
        {
            const auto found = entity.properties.find(std::string{key});
            return found == entity.properties.end() ? nullptr : &found->second;
        }

        [[nodiscard]] auto property_or(const EntitySpawnData& entity, std::string_view key, std::string fallback)
            -> std::string
        {
            if (const std::string* value = find_entity_property(entity, key); value != nullptr && !value->empty())
            {
                return *value;
            }

            return fallback;
        }

        [[nodiscard]] auto normalize_lower(std::string value) -> std::string
        {
            std::ranges::transform(value, value.begin(), [](const unsigned char c) {
                return static_cast<char>(std::tolower(c));
            });
            return value;
        }

        struct ValidationBounds {
            glm::vec3 mins{};
            glm::vec3 maxs{};
        };

        [[nodiscard]] auto try_parse_float_property(const EntitySpawnData& entity, const std::string_view key)
            -> std::optional<float>
        {
            const std::string* value = find_entity_property(entity, key);
            if (value == nullptr || value->empty())
            {
                return std::nullopt;
            }

            float result = 0.0F;
            const auto [ptr, ec] = std::from_chars(value->data(), value->data() + value->size(), result);
            if (ec != std::errc{} || ptr != value->data() + value->size())
            {
                throw std::runtime_error(
                    "Entity property '" + std::string(key) + "' must be a number, got '" + *value + "'"
                );
            }

            return result;
        }

        [[nodiscard]] auto try_parse_vec3_property(const EntitySpawnData& entity, const std::string_view key)
            -> std::optional<glm::vec3>
        {
            const std::string* value = find_entity_property(entity, key);
            if (value == nullptr || value->empty())
            {
                return std::nullopt;
            }

            std::string normalized = *value;
            std::replace(normalized.begin(), normalized.end(), ',', ' ');

            std::istringstream stream(normalized);
            glm::vec3 result{};
            if (!(stream >> result.x >> result.y >> result.z))
            {
                throw std::runtime_error(
                    "Entity property '" + std::string(key) + "' must be a 3-component vector, got '" + *value + "'"
                );
            }

            stream >> std::ws;
            if (!stream.eof())
            {
                throw std::runtime_error(
                    "Entity property '" + std::string(key) + "' must be a 3-component vector, got '" + *value + "'"
                );
            }

            return result;
        }

        [[nodiscard]] auto normalized_entity_kind(const EntitySpawnData& entity) -> std::string
        {
            if (const std::string* kind = find_entity_property(entity, "kind"); kind != nullptr)
            {
                return normalize_lower(*kind);
            }
            return {};
        }

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

        [[nodiscard]] auto normalized_entity_action(const EntitySpawnData& entity) -> std::string
        {
            if (const std::string* action = find_entity_property(entity, "action");
                action != nullptr && !action->empty())
            {
                return normalize_lower(*action);
            }
            if (const std::string* action = find_entity_property(entity, "action_kind");
                action != nullptr && !action->empty())
            {
                return normalize_lower(*action);
            }
            return {};
        }

        [[nodiscard]] auto is_signal_action(const EntitySpawnData& entity) -> bool
        {
            const std::string action = normalized_entity_action(entity);
            return action == "signal" || action == "signal_target" || action == "signal-target";
        }

        [[nodiscard]] auto is_activate_target_action(const EntitySpawnData& entity) -> bool
        {
            const std::string action = normalized_entity_action(entity);
            return action == "activate_target" || action == "activate-target" || action == "activate";
        }

        [[nodiscard]] auto is_pickup_action_name(const std::string_view action) -> bool
        {
            return action == "pickup" || action == "pickup_item" || action == "pickup-item";
        }

        [[nodiscard]] auto is_supported_usable_action_name(const std::string_view action) -> bool
        {
            return action.empty() || action == "signal" || action == "signal_target" || action == "signal-target"
                   || action == "activate_target" || action == "activate-target" || action == "activate"
                   || is_pickup_action_name(action);
        }

        [[nodiscard]] auto is_supported_signal_action_name(const std::string_view signal_action) -> bool
        {
            return signal_action.empty() || signal_action == "unlock" || signal_action == "lock"
                   || signal_action == "toggle_lock" || signal_action == "toggle-lock" || signal_action == "toggle";
        }

        [[nodiscard]] auto is_usable_receiver_kind(const std::string_view kind) -> bool
        {
            return kind == "panel" || kind == "button" || kind == "terminal" || kind == "console" || kind == "usable";
        }

        [[nodiscard]] auto is_valid_panel_axis(const std::string_view axis) -> bool
        {
            const std::string normalized = normalize_lower(std::string{axis});
            return normalized.empty() || normalized == "x" || normalized == "+x" || normalized == "positive_x"
                   || normalized == "pos_x" || normalized == "-x" || normalized == "negative_x" || normalized == "neg_x"
                   || normalized == "minus_x" || normalized == "z" || normalized == "+z" || normalized == "positive_z"
                   || normalized == "pos_z" || normalized == "-z" || normalized == "negative_z" || normalized == "neg_z"
                   || normalized == "minus_z";
        }

        [[nodiscard]] auto signal_action_requires_targetname(const EntitySpawnData& entity) -> bool
        {
            const std::string* signal_action = find_entity_property(entity, "signal_action");
            if (signal_action != nullptr && !signal_action->empty())
            {
                return true;
            }

            signal_action = find_entity_property(entity, "signal_response");
            return signal_action != nullptr && !signal_action->empty();
        }

        [[nodiscard]] auto parse_validation_size(
            const EntitySpawnData& entity,
            const glm::vec3& fallback,
            const std::string_view depth_key
        ) -> glm::vec3
        {
            glm::vec3 size = fallback;
            if (const auto compact = try_parse_vec3_property(entity, "size"); compact.has_value())
            {
                size = *compact;
            } else
            {
                size.x = try_parse_float_property(entity, "width").value_or(size.x);
                size.y = try_parse_float_property(entity, "height").value_or(size.y);
                size.z = try_parse_float_property(entity, depth_key)
                             .value_or(try_parse_float_property(entity, "thickness").value_or(size.z));
            }

            return size;
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

        [[nodiscard]] auto resolved_pickup_item_id(const EntitySpawnData& entity, const std::size_t pickup_index)
            -> std::string
        {
            return property_or(
                entity,
                "item_id",
                property_or(entity, "item", property_or(entity, "targetname", "item_" + std::to_string(pickup_index)))
            );
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

        void
        validate_module_template_anchors(const ModuleTemplateData& module_template, const std::string& template_name)
        {
            std::unordered_set<std::string> anchor_names{};
            for (const ModuleTemplateData::Anchor& anchor : module_template.anchors)
            {
                if (!anchor_names.insert(anchor.name).second)
                {
                    throw std::runtime_error(
                        "Duplicate module template anchor name '" + anchor.name + "' in module template '"
                        + template_name + "'"
                    );
                }
            }
        }

        void validate_loaded_level(const BlockoutLevel& level)
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
                    throw std::runtime_error(
                        "Entity of kind '" + kind + "' references unknown target '" + *target + "'"
                    );
                }

                if (entity_requires_resolved_target(entity) && target != nullptr && !target->empty())
                {
                    const auto found = target_name_kinds.find(*target);
                    if (found != target_name_kinds.end() && found->second != "door")
                    {
                        throw std::runtime_error(
                            "Entity of kind '" + kind + "' targets '" + *target
                            + "', but that targetname belongs to a '"
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
                            "Entity of kind '" + kind + "' targets '" + *target
                            + "', but that targetname belongs to a '"
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
                        "Entity " + format_entity_label(entity, entity_index) + " requires unknown item '"
                        + *required_item + "'"
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
                            throw std::runtime_error(
                                "mission_step pickup references unknown item '" + step_target + "'"
                            );
                        }
                    } else if (step_kind == ValidationMissionStepKind::ActivateTarget)
                    {
                        if (!activatable_targets.contains(step_target))
                        {
                            throw std::runtime_error(
                                "mission_step activate_target references unknown activatable target '" + step_target
                                + "'"
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
                                "mission_step signal target '" + step_target
                                + "' is neither a door nor a signal receiver"
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

        void append_solid_entries(
            const toml::array& solids,
            const std::unordered_map<std::string, AnchorPlacement>& anchors,
            std::vector<BlockoutSolid>& out_solids,
            std::vector<AuthoredSourceLocation>& out_solid_sources
        )
        {
            for (const toml::node& node : solids)
            {
                const toml::table* solid = node.as_table();
                if (solid == nullptr)
                {
                    throw std::runtime_error("Each [[solid]] entry must be a TOML table");
                }

                append_solid_from_table(*solid, "solid", nullptr, out_solids, &out_solid_sources);
                translate_solid(out_solids.back(), resolve_base_offset(*solid, "solid", anchors));
            }
        }

        void append_solid_group_entries(
            const toml::array& groups,
            const std::unordered_map<std::string, AnchorPlacement>& anchors,
            std::vector<BlockoutSolid>& out_solids,
            std::vector<AuthoredSourceLocation>& out_solid_sources
        )
        {
            for (const toml::node& node : groups)
            {
                const toml::table* group = node.as_table();
                if (group == nullptr)
                {
                    throw std::runtime_error("Each [[solid_group]] entry must be a TOML table");
                }

                SolidDefaults defaults{};
                defaults.material = parse_optional_surface_material(*group, "material", "solid_group.material");
                defaults.tint = parse_optional_vec3(*group, "tint", "solid_group.tint");
                defaults.size = parse_optional_vec3(*group, "size", "solid_group.size");
                defaults.brightness = parse_optional_float_field(*group, "brightness", "solid_group.brightness");
                const glm::vec3 group_offset = resolve_base_offset(*group, "solid_group", anchors);

                const toml::node* entries_node = group->get("entries");
                if (entries_node == nullptr)
                {
                    entries_node = group->get("boxes");
                }
                if (entries_node == nullptr)
                {
                    throw std::runtime_error("Field 'solid_group.entries' or 'solid_group.boxes' is required");
                }

                const toml::array* entries = entries_node->as_array();
                if (entries == nullptr)
                {
                    throw std::runtime_error("Field 'solid_group.entries' must be an array of tables");
                }

                for (const toml::node& entry_node : *entries)
                {
                    const toml::table* entry = entry_node.as_table();
                    if (entry == nullptr)
                    {
                        throw std::runtime_error("Each solid_group.entries entry must be a TOML table");
                    }

                    append_solid_from_table(*entry, "solid_group.entry", &defaults, out_solids, &out_solid_sources);
                    translate_solid(out_solids.back(), group_offset);
                }
            }
        }

        void append_entity_entries(
            const toml::array& entities,
            const std::unordered_map<std::string, AnchorPlacement>& anchors,
            std::vector<EntitySpawnData>& out_entities,
            std::vector<AuthoredSourceLocation>& out_entity_sources
        )
        {
            for (const toml::node& node : entities)
            {
                const toml::table* entity = node.as_table();
                if (entity == nullptr)
                {
                    throw std::runtime_error("Each [[entity]] entry must be a TOML table");
                }

                AuthoredSourceLocation source_location{};
                EntitySpawnData out = parse_entity_table(*entity, "entity", &source_location);
                out.origin += resolve_base_offset(*entity, "entity", anchors);
                out.properties.erase("anchor");
                out_entities.push_back(std::move(out));
                out_entity_sources.push_back(std::move(source_location));
            }
        }

        void append_entity_group_entries(
            const toml::array& groups,
            const std::unordered_map<std::string, AnchorPlacement>& anchors,
            std::vector<EntitySpawnData>& out_entities,
            std::vector<AuthoredSourceLocation>& out_entity_sources
        )
        {
            for (const toml::node& node : groups)
            {
                const toml::table* group = node.as_table();
                if (group == nullptr)
                {
                    throw std::runtime_error("Each [[entity_group]] entry must be a TOML table");
                }

                const toml::array* entries = get_required_node(*group, "entries", "entity_group").as_array();
                if (entries == nullptr)
                {
                    throw std::runtime_error("Field 'entity_group.entries' must be an array of tables");
                }

                const glm::vec3 group_offset = resolve_base_offset(*group, "entity_group", anchors);
                for (const toml::node& entry_node : *entries)
                {
                    const toml::table* entry = entry_node.as_table();
                    if (entry == nullptr)
                    {
                        throw std::runtime_error("Each entity_group.entries entry must be a TOML table");
                    }

                    EntitySpawnData out{};
                    apply_entity_fields(*group, "entity_group", out);
                    apply_entity_fields(*entry, "entity_group.entry", out);
                    out.origin += group_offset + resolve_base_offset(*entry, "entity_group.entry", anchors);
                    out.properties.erase("anchor");
                    if (out.classname.empty())
                    {
                        throw std::runtime_error("Missing required field 'entity_group.classname'");
                    }

                    out_entities.push_back(std::move(out));
                    out_entity_sources.push_back(make_authored_source_location(*entry));
                }
            }
        }

        void
        build_solid_templates(const toml::array& groups, std::unordered_map<std::string, SolidTemplateData>& templates)
        {
            for (const toml::node& node : groups)
            {
                const toml::table* solid_template = node.as_table();
                if (solid_template == nullptr)
                {
                    throw std::runtime_error("Each [[solid_template]] entry must be a TOML table");
                }

                const std::string name = parse_required_string(*solid_template, "name", "solid_template");
                if (name.empty())
                {
                    throw std::runtime_error("Field 'solid_template.name' must not be empty");
                }
                if (templates.contains(name))
                {
                    throw std::runtime_error("Duplicate solid template name: " + name);
                }

                SolidDefaults defaults{};
                defaults.material =
                    parse_optional_surface_material(*solid_template, "material", "solid_template.material");
                defaults.tint = parse_optional_vec3(*solid_template, "tint", "solid_template.tint");
                defaults.size = parse_optional_vec3(*solid_template, "size", "solid_template.size");
                defaults.brightness =
                    parse_optional_float_field(*solid_template, "brightness", "solid_template.brightness");

                const toml::node* entries_node = solid_template->get("entries");
                if (entries_node == nullptr)
                {
                    entries_node = solid_template->get("boxes");
                }
                if (entries_node == nullptr)
                {
                    throw std::runtime_error("Field 'solid_template.entries' or 'solid_template.boxes' is required");
                }

                const toml::array* entries = entries_node->as_array();
                if (entries == nullptr)
                {
                    throw std::runtime_error("Field 'solid_template.entries' must be an array of tables");
                }

                SolidTemplateData built_template{};
                built_template.solids.reserve(entries->size());
                built_template.solid_sources.reserve(entries->size());
                for (const toml::node& entry_node : *entries)
                {
                    const toml::table* entry = entry_node.as_table();
                    if (entry == nullptr)
                    {
                        throw std::runtime_error("Each solid_template.entries entry must be a TOML table");
                    }

                    append_solid_from_table(
                        *entry,
                        "solid_template.entry",
                        &defaults,
                        built_template.solids,
                        &built_template.solid_sources
                    );
                }

                templates.emplace(name, std::move(built_template));
            }
        }

        void build_module_templates(
            const toml::array& groups,
            std::unordered_map<std::string, ModuleTemplateData>& templates
        )
        {
            for (const toml::node& node : groups)
            {
                const toml::table* module_template = node.as_table();
                if (module_template == nullptr)
                {
                    throw std::runtime_error("Each [[module_template]] entry must be a TOML table");
                }

                const std::string name = parse_required_string(*module_template, "name", "module_template");
                if (name.empty())
                {
                    throw std::runtime_error("Field 'module_template.name' must not be empty");
                }
                if (templates.contains(name))
                {
                    throw std::runtime_error("Duplicate module template name: " + name);
                }

                ModuleTemplateData built_template{};

                if (const toml::node* solids_node = module_template->get("solids"); solids_node != nullptr)
                {
                    const toml::array* solids = solids_node->as_array();
                    if (solids == nullptr)
                    {
                        throw std::runtime_error("Field 'module_template.solids' must be an array of tables");
                    }

                    built_template.solids.reserve(solids->size());
                    built_template.solid_sources.reserve(solids->size());
                    for (const toml::node& entry_node : *solids)
                    {
                        const toml::table* entry = entry_node.as_table();
                        if (entry == nullptr)
                        {
                            throw std::runtime_error("Each module_template.solids entry must be a TOML table");
                        }

                        append_solid_from_table(
                            *entry,
                            "module_template.solid",
                            nullptr,
                            built_template.solids,
                            &built_template.solid_sources
                        );
                    }
                }

                if (const toml::node* entities_node = module_template->get("entities"); entities_node != nullptr)
                {
                    const toml::array* entities = entities_node->as_array();
                    if (entities == nullptr)
                    {
                        throw std::runtime_error("Field 'module_template.entities' must be an array of tables");
                    }

                    built_template.entities.reserve(entities->size());
                    built_template.entity_sources.reserve(entities->size());
                    for (const toml::node& entry_node : *entities)
                    {
                        const toml::table* entry = entry_node.as_table();
                        if (entry == nullptr)
                        {
                            throw std::runtime_error("Each module_template.entities entry must be a TOML table");
                        }

                        AuthoredSourceLocation source_location{};
                        built_template.entities.push_back(
                            parse_entity_table(*entry, "module_template.entity", &source_location)
                        );
                        built_template.entity_sources.push_back(std::move(source_location));
                    }
                }

                if (const toml::node* anchors_node = module_template->get("anchors"); anchors_node != nullptr)
                {
                    const toml::array* anchors = anchors_node->as_array();
                    if (anchors == nullptr)
                    {
                        throw std::runtime_error("Field 'module_template.anchors' must be an array of tables");
                    }

                    built_template.anchors.reserve(anchors->size());
                    for (const toml::node& entry_node : *anchors)
                    {
                        const toml::table* entry = entry_node.as_table();
                        if (entry == nullptr)
                        {
                            throw std::runtime_error("Each module_template.anchors entry must be a TOML table");
                        }

                        ModuleTemplateData::Anchor anchor{};
                        anchor.name = parse_required_string(*entry, "name", "module_template.anchor");
                        if (anchor.name.empty())
                        {
                            throw std::runtime_error("Field 'module_template.anchor.name' must not be empty");
                        }

                        anchor.origin = parse_required_vec3(*entry, "origin", "module_template.anchor");
                        anchor.angles = parse_optional_vec3(*entry, "angles", "module_template.anchor.angles")
                                            .value_or(glm::vec3{0.0F});
                        built_template.anchors.push_back(std::move(anchor));
                    }
                }

                if (built_template.solids.empty() && built_template.entities.empty())
                {
                    throw std::runtime_error(
                        "Module template '" + name + "' requires at least one solid or entity entry"
                    );
                }

                validate_module_template_anchors(built_template, name);
                templates.emplace(name, std::move(built_template));
            }
        }

        void append_solid_instances(
            const toml::array& instances,
            const std::unordered_map<std::string, SolidTemplateData>& templates,
            const std::unordered_map<std::string, AnchorPlacement>& anchors,
            std::vector<BlockoutSolid>& out_solids,
            std::vector<AuthoredSourceLocation>& out_solid_sources
        )
        {
            for (const toml::node& node : instances)
            {
                const toml::table* instance = node.as_table();
                if (instance == nullptr)
                {
                    throw std::runtime_error("Each [[solid_instance]] entry must be a TOML table");
                }

                const std::string template_name = parse_required_string(*instance, "template", "solid_instance");
                const auto found = templates.find(template_name);
                if (found == templates.end())
                {
                    throw std::runtime_error("Unknown solid template: " + template_name);
                }

                const glm::vec3 offset = resolve_base_offset(*instance, "solid_instance", anchors);
                out_solids.reserve(out_solids.size() + found->second.solids.size());
                out_solid_sources.reserve(out_solid_sources.size() + found->second.solid_sources.size());
                for (std::size_t source_index = 0; source_index < found->second.solids.size(); ++source_index)
                {
                    BlockoutSolid placed = found->second.solids[source_index];
                    placed.mins += offset;
                    placed.maxs += offset;
                    out_solids.push_back(std::move(placed));
                    out_solid_sources.push_back(make_instanced_authored_source_location(
                        found->second.solid_sources[source_index],
                        *instance,
                        "solid_instance",
                        template_name
                    ));
                }
            }
        }

        void append_module_instances(
            const toml::array& instances,
            const std::unordered_map<std::string, ModuleTemplateData>& templates,
            std::unordered_map<std::string, AnchorPlacement>& anchors,
            std::vector<BlockoutSolid>& out_solids,
            std::vector<AuthoredSourceLocation>& out_solid_sources,
            std::vector<EntitySpawnData>& out_entities,
            std::vector<AuthoredSourceLocation>& out_entity_sources
        )
        {
            for (const toml::node& node : instances)
            {
                const toml::table* instance = node.as_table();
                if (instance == nullptr)
                {
                    throw std::runtime_error("Each [[module_instance]] entry must be a TOML table");
                }

                const std::string template_name = parse_required_string(*instance, "template", "module_instance");
                const auto found = templates.find(template_name);
                if (found == templates.end())
                {
                    throw std::runtime_error("Unknown module template: " + template_name);
                }

                const glm::vec3 offset = resolve_base_offset(*instance, "module_instance", anchors);
                const QuarterTurn rotation = parse_optional_rotation_y(*instance, "module_instance");
                const auto instance_name_node = instance->get("name");
                const std::string instance_name = instance_name_node != nullptr
                                                      ? parse_required_string(*instance, "name", "module_instance")
                                                      : std::string{};
                const auto name_prefix_node = instance->get("name_prefix");
                const std::string name_prefix = name_prefix_node != nullptr
                                                    ? parse_required_string(*instance, "name_prefix", "module_instance")
                                                    : std::string{};
                const auto remap = build_module_name_remap(found->second, name_prefix);

                out_solids.reserve(out_solids.size() + found->second.solids.size());
                out_solid_sources.reserve(out_solid_sources.size() + found->second.solid_sources.size());
                for (std::size_t source_index = 0; source_index < found->second.solids.size(); ++source_index)
                {
                    out_solids.push_back(rotate_bounds_y(found->second.solids[source_index], rotation, offset));
                    out_solid_sources.push_back(make_instanced_authored_source_location(
                        found->second.solid_sources[source_index],
                        *instance,
                        "module_instance",
                        template_name,
                        instance_name
                    ));
                }

                out_entities.reserve(out_entities.size() + found->second.entities.size());
                out_entity_sources.reserve(out_entity_sources.size() + found->second.entity_sources.size());
                for (std::size_t source_index = 0; source_index < found->second.entities.size(); ++source_index)
                {
                    out_entities.push_back(apply_module_name_prefix(
                        rotate_module_entity(found->second.entities[source_index], rotation, offset),
                        name_prefix,
                        remap
                    ));
                    out_entity_sources.push_back(make_instanced_authored_source_location(
                        found->second.entity_sources[source_index],
                        *instance,
                        "module_instance",
                        template_name,
                        instance_name
                    ));
                }

                if (!instance_name.empty())
                {
                    for (const ModuleTemplateData::Anchor& source : found->second.anchors)
                    {
                        const std::string anchor_key = instance_name + ":" + source.name;
                        if (anchors.contains(anchor_key))
                        {
                            throw std::runtime_error("Duplicate anchor placement: " + anchor_key);
                        }
                        anchors[anchor_key] = rotate_anchor_placement(source, rotation, offset);
                    }
                }
            }
        }

        void build_level_from_document(
            const toml::table& data,
            BlockoutLevel& level,
            std::unordered_map<std::string, AnchorPlacement>& anchors,
            std::unordered_map<std::string, SolidTemplateData>& solid_templates,
            std::unordered_map<std::string, ModuleTemplateData>& module_templates
        )
        {
            if (const toml::array* templates = data["solid_template"].as_array(); templates != nullptr)
            {
                build_solid_templates(*templates, solid_templates);
            }
            if (const toml::array* templates = data["module_template"].as_array(); templates != nullptr)
            {
                build_module_templates(*templates, module_templates);
            }

            if (const toml::array* entries = data["module_instance"].as_array(); entries != nullptr)
            {
                append_module_instances(
                    *entries,
                    module_templates,
                    anchors,
                    level.solids,
                    level.solid_sources,
                    level.entities,
                    level.entity_sources
                );
            }

            if (const toml::array* entries = data["solid_instance"].as_array(); entries != nullptr)
            {
                append_solid_instances(*entries, solid_templates, anchors, level.solids, level.solid_sources);
            }

            if (const toml::array* entries = data["solid"].as_array(); entries != nullptr)
            {
                append_solid_entries(*entries, anchors, level.solids, level.solid_sources);
            }

            if (const toml::array* entries = data["solid_group"].as_array(); entries != nullptr)
            {
                append_solid_group_entries(*entries, anchors, level.solids, level.solid_sources);
            }

            if (const toml::array* entries = data["entity"].as_array(); entries != nullptr)
            {
                append_entity_entries(*entries, anchors, level.entities, level.entity_sources);
            }

            if (const toml::array* entries = data["entity_group"].as_array(); entries != nullptr)
            {
                append_entity_group_entries(*entries, anchors, level.entities, level.entity_sources);
            }
        }
    } // namespace

    BlockoutLevel load_blockout_level_data(const std::filesystem::path& path)
    {
        RELAYCORE_PROFILE_ZONE_N("BlockoutLoader::load");
        if (!std::filesystem::exists(path))
        {
            throw std::runtime_error("Blockout level file does not exist: " + path.string());
        }

        try
        {
            toml::table data{};
            {
                RELAYCORE_PROFILE_ZONE_N("BlockoutLoader::parse");
                data = toml::parse_file(path.string());
            }

            BlockoutLevel level{};
            std::unordered_map<std::string, AnchorPlacement> anchors{};
            std::unordered_map<std::string, SolidTemplateData> solid_templates{};
            std::unordered_map<std::string, ModuleTemplateData> module_templates{};

            {
                RELAYCORE_PROFILE_ZONE_N("BlockoutLoader::build");
                build_level_from_document(data, level, anchors, solid_templates, module_templates);
            }
            if (level.solids.size() != level.solid_sources.size())
            {
                throw std::runtime_error("Internal loader error: solid source metadata is out of sync with solids");
            }
            if (level.entities.size() != level.entity_sources.size())
            {
                throw std::runtime_error("Internal loader error: entity source metadata is out of sync with entities");
            }
            {
                RELAYCORE_PROFILE_ZONE_N("BlockoutLoader::validate");
                validate_loaded_level(level);
            }

            log::info(
                "Loaded blockout level '{}' ({} solids, {} entities)",
                path.string(),
                level.solids.size(),
                level.entities.size()
            );

            return level;
        } catch (const toml::parse_error& error)
        {
            throw std::runtime_error(
                "Failed to parse blockout level '" + path.string() + "' at line "
                + std::to_string(error.source().begin.line) + ", column " + std::to_string(error.source().begin.column)
                + ": " + std::string{error.description()}
            );
        } catch (const std::exception& error)
        {
            throw std::runtime_error("Failed to load blockout level '" + path.string() + "': " + error.what());
        }
    }
} // namespace RelayCore::detail
