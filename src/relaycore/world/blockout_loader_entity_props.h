#pragma once

#include "relaycore/world/blockout_level.h"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cmath>
#include <glm/vec3.hpp>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace RelayCore::detail
{
    [[nodiscard]] inline auto find_entity_property(const EntitySpawnData& entity, const std::string_view key)
        -> const std::string*
    {
        const auto found = entity.properties.find(std::string{key});
        return found == entity.properties.end() ? nullptr : &found->second;
    }

    [[nodiscard]] inline auto property_or(const EntitySpawnData& entity, std::string_view key, std::string fallback)
        -> std::string
    {
        if (const std::string* value = find_entity_property(entity, key); value != nullptr && !value->empty())
        {
            return *value;
        }

        return fallback;
    }

    [[nodiscard]] inline auto normalize_lower(std::string value) -> std::string
    {
        std::ranges::transform(value, value.begin(), [](const unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });
        return value;
    }

    [[nodiscard]] inline auto try_parse_float_property(const EntitySpawnData& entity, const std::string_view key)
        -> std::optional<float>
    {
        const std::string* value = find_entity_property(entity, key);
        if (value == nullptr)
        {
            return std::nullopt;
        }

        float result = 0.0F;
        const auto [ptr, ec] = std::from_chars(value->data(), value->data() + value->size(), result);
        if (ec != std::errc{} || ptr != value->data() + value->size() || !std::isfinite(result))
        {
            throw std::runtime_error(
                "Entity property '" + std::string(key) + "' must be a finite number, got '" + *value + "'"
            );
        }

        return result;
    }

    [[nodiscard]] inline auto try_parse_vec3_property(const EntitySpawnData& entity, const std::string_view key)
        -> std::optional<glm::vec3>
    {
        const std::string* value = find_entity_property(entity, key);
        if (value == nullptr)
        {
            return std::nullopt;
        }

        std::string normalized = *value;
        std::replace(normalized.begin(), normalized.end(), ',', ' ');

        std::istringstream stream(normalized);
        glm::vec3 result{};
        if (!(stream >> result.x >> result.y >> result.z) || !std::isfinite(result.x) || !std::isfinite(result.y)
            || !std::isfinite(result.z))
        {
            throw std::runtime_error(
                "Entity property '" + std::string(key) + "' must be a finite 3-component vector, got '" + *value + "'"
            );
        }

        stream >> std::ws;
        if (!stream.eof())
        {
            throw std::runtime_error(
                "Entity property '" + std::string(key) + "' must be a finite 3-component vector, got '" + *value + "'"
            );
        }

        return result;
    }

    [[nodiscard]] inline auto normalized_entity_kind(const EntitySpawnData& entity) -> std::string
    {
        if (const std::string* kind = find_entity_property(entity, "kind"); kind != nullptr)
        {
            return normalize_lower(*kind);
        }
        return {};
    }

    [[nodiscard]] inline auto normalized_entity_action(const EntitySpawnData& entity) -> std::string
    {
        if (const std::string* action = find_entity_property(entity, "action"); action != nullptr && !action->empty())
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

    [[nodiscard]] inline auto is_signal_action(const EntitySpawnData& entity) -> bool
    {
        const std::string action = normalized_entity_action(entity);
        return action == "signal" || action == "signal_target" || action == "signal-target";
    }

    [[nodiscard]] inline auto is_activate_target_action(const EntitySpawnData& entity) -> bool
    {
        const std::string action = normalized_entity_action(entity);
        return action == "activate_target" || action == "activate-target" || action == "activate";
    }

    [[nodiscard]] inline auto is_pickup_action_name(const std::string_view action) -> bool
    {
        return action == "pickup" || action == "pickup_item" || action == "pickup-item";
    }

    [[nodiscard]] inline auto is_supported_usable_action_name(const std::string_view action) -> bool
    {
        return action.empty() || action == "signal" || action == "signal_target" || action == "signal-target"
               || action == "activate_target" || action == "activate-target" || action == "activate"
               || is_pickup_action_name(action);
    }

    [[nodiscard]] inline auto is_supported_signal_action_name(const std::string_view signal_action) -> bool
    {
        return signal_action.empty() || signal_action == "unlock" || signal_action == "lock"
               || signal_action == "toggle_lock" || signal_action == "toggle-lock" || signal_action == "toggle";
    }

    [[nodiscard]] inline auto is_usable_receiver_kind(const std::string_view kind) -> bool
    {
        return kind == "panel" || kind == "button" || kind == "terminal" || kind == "console" || kind == "usable";
    }

    [[nodiscard]] inline auto is_valid_panel_axis(const std::string_view axis) -> bool
    {
        const std::string normalized = normalize_lower(std::string{axis});
        return normalized.empty() || normalized == "x" || normalized == "+x" || normalized == "positive_x"
               || normalized == "pos_x" || normalized == "-x" || normalized == "negative_x" || normalized == "neg_x"
               || normalized == "minus_x" || normalized == "z" || normalized == "+z" || normalized == "positive_z"
               || normalized == "pos_z" || normalized == "-z" || normalized == "negative_z" || normalized == "neg_z"
               || normalized == "minus_z";
    }

    [[nodiscard]] inline auto signal_action_requires_targetname(const EntitySpawnData& entity) -> bool
    {
        const std::string* signal_action = find_entity_property(entity, "signal_action");
        if (signal_action != nullptr && !signal_action->empty())
        {
            return true;
        }

        signal_action = find_entity_property(entity, "signal_response");
        return signal_action != nullptr && !signal_action->empty();
    }

    [[nodiscard]] inline auto
    parse_validation_size(const EntitySpawnData& entity, const glm::vec3& fallback, const std::string_view depth_key)
        -> glm::vec3
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

    [[nodiscard]] inline auto resolved_pickup_item_id(const EntitySpawnData& entity, const std::size_t pickup_index)
        -> std::string
    {
        return property_or(
            entity,
            "item_id",
            property_or(entity, "item", property_or(entity, "targetname", "item_" + std::to_string(pickup_index)))
        );
    }
} // namespace RelayCore::detail
