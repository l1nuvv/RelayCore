#pragma once

#include "relaycore/physics/collision.h"
#include "relaycore/world/blockout_level.h"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <glm/common.hpp>
#include <glm/vec3.hpp>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>

namespace RelayCore::gameplay::detail
{
    [[nodiscard]] inline auto find_property(const EntitySpawnData& entity, const std::string_view key)
        -> const std::string*
    {
        const auto found = entity.properties.find(std::string{key});
        return found == entity.properties.end() ? nullptr : &found->second;
    }

    [[nodiscard]] inline auto
    property_or(const EntitySpawnData& entity, const std::string_view key, std::string fallback) -> std::string
    {
        if (const std::string* value = find_property(entity, key); value != nullptr && !value->empty())
        {
            return *value;
        }

        return fallback;
    }

    [[nodiscard]] inline auto lowercase_copy(const std::string_view value) -> std::string
    {
        std::string normalized(value);
        std::ranges::transform(normalized, normalized.begin(), [](const unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });
        return normalized;
    }

    [[nodiscard]] inline auto
    parse_bool_property(const EntitySpawnData& entity, const std::string_view key, const bool fallback) -> bool
    {
        const std::string* value = find_property(entity, key);
        if (value == nullptr)
        {
            return fallback;
        }

        const std::string normalized = lowercase_copy(*value);
        if (normalized == "1" || normalized == "true" || normalized == "yes" || normalized == "on")
        {
            return true;
        }
        if (normalized == "0" || normalized == "false" || normalized == "no" || normalized == "off")
        {
            return false;
        }

        return fallback;
    }

    [[nodiscard]] inline auto parse_float_property(const EntitySpawnData& entity, const std::string_view key)
        -> std::optional<float>
    {
        const std::string* value = find_property(entity, key);
        if (value == nullptr || value->empty())
        {
            return std::nullopt;
        }

        float parsed_value = 0.0F;
        const auto [ptr, ec] = std::from_chars(value->data(), value->data() + value->size(), parsed_value);
        if (ec != std::errc{} || ptr != value->data() + value->size())
        {
            return std::nullopt;
        }

        return parsed_value;
    }

    [[nodiscard]] inline auto parse_vec3_property(const EntitySpawnData& entity, const std::string_view key)
        -> std::optional<glm::vec3>
    {
        const std::string* value = find_property(entity, key);
        if (value == nullptr || value->empty())
        {
            return std::nullopt;
        }

        std::string normalized = *value;
        std::replace(normalized.begin(), normalized.end(), ',', ' ');

        std::istringstream stream(normalized);
        glm::vec3 parsed_value{};
        if (!(stream >> parsed_value.x >> parsed_value.y >> parsed_value.z))
        {
            return std::nullopt;
        }

        stream >> std::ws;
        if (!stream.eof())
        {
            return std::nullopt;
        }

        return parsed_value;
    }

    [[nodiscard]] inline auto centered_bounds(const glm::vec3& center, const glm::vec3& size) -> physics::Aabb
    {
        const glm::vec3 half_size = size * 0.5F;
        return {
            .mins = center - half_size,
            .maxs = center + half_size,
        };
    }

    [[nodiscard]] inline auto
    parse_centered_bounds(const EntitySpawnData& entity, const glm::vec3& default_size, const glm::vec3& min_size)
        -> physics::Aabb
    {
        glm::vec3 size{
            parse_float_property(entity, "width").value_or(default_size.x),
            parse_float_property(entity, "height").value_or(default_size.y),
            parse_float_property(entity, "depth")
                .value_or(parse_float_property(entity, "thickness").value_or(default_size.z)),
        };
        if (const auto compact_size = parse_vec3_property(entity, "size"); compact_size.has_value())
        {
            size = *compact_size;
        }

        size = glm::max(size, min_size);
        return centered_bounds(entity.origin, size);
    }
} // namespace RelayCore::gameplay::detail
