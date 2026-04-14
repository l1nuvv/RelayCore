#pragma once

#include "relaycore/gameplay/interaction_runtime.h"

#include <algorithm>
#include <glm/common.hpp>
#include <glm/vec3.hpp>
#include <string_view>
#include <vector>

namespace RelayCore::gameplay::detail
{
    [[nodiscard]] inline auto aabb_overlaps(const physics::Aabb& a, const physics::Aabb& b) -> bool
    {
        return a.mins.x < b.maxs.x && a.maxs.x > b.mins.x && a.mins.y < b.maxs.y && a.maxs.y > b.mins.y
               && a.mins.z < b.maxs.z && a.maxs.z > b.mins.z;
    }

    [[nodiscard]] inline auto aabb_center(const physics::Aabb& bounds) -> glm::vec3
    {
        return (bounds.mins + bounds.maxs) * 0.5F;
    }

    [[nodiscard]] inline auto closest_point_on_aabb(const physics::Aabb& bounds, const glm::vec3& point) -> glm::vec3
    {
        return {
            glm::clamp(point.x, bounds.mins.x, bounds.maxs.x),
            glm::clamp(point.y, bounds.mins.y, bounds.maxs.y),
            glm::clamp(point.z, bounds.mins.z, bounds.maxs.z),
        };
    }

    [[nodiscard]] inline auto usable_spatial_bounds(const RuntimeUsable& usable) -> physics::Aabb
    {
        if (usable.has_focus_bounds)
        {
            return usable.focus_bounds;
        }

        const glm::vec3 half_extents{
            usable.use_radius,
            std::max(usable.use_radius * 0.5F, 1.0F),
            usable.use_radius,
        };
        return {usable.origin - half_extents, usable.origin + half_extents};
    }

    [[nodiscard]] inline auto find_target_door(const std::vector<RuntimeDoor>& doors, const std::string_view target)
        -> const RuntimeDoor*
    {
        const auto it = std::ranges::find_if(doors, [target](const RuntimeDoor& door) {
            return door.name == target;
        });
        if (it == doors.end())
        {
            return nullptr;
        }
        return &(*it);
    }

    [[nodiscard]] inline auto contains_string(const std::vector<std::string>& values, const std::string_view value)
        -> bool
    {
        return !value.empty() && std::ranges::find_if(values, [value](const std::string& candidate) {
                                     return candidate == value;
                                 }) != values.end();
    }

    inline void append_unique_string(std::vector<std::string>& values, const std::string_view value)
    {
        if (value.empty() || contains_string(values, value))
        {
            return;
        }

        values.emplace_back(value);
    }
} // namespace RelayCore::gameplay::detail
