#include "relaycore/gameplay/interaction_runtime.h"

#include <algorithm>
#include <glm/common.hpp>
#include <glm/geometric.hpp>
#include <limits>

#include "relaycore/world/blockout_level.h"

namespace RelayCore::gameplay
{
    namespace
    {
        [[nodiscard]] auto make_centered_aabb(const glm::vec3& center, const glm::vec3& half_extents) -> physics::Aabb
        {
            return {center - half_extents, center + half_extents};
        }

        [[nodiscard]] auto aabb_overlaps(const physics::Aabb& a, const physics::Aabb& b) -> bool
        {
            return a.mins.x < b.maxs.x && a.maxs.x > b.mins.x && a.mins.y < b.maxs.y && a.maxs.y > b.mins.y
                   && a.mins.z < b.maxs.z && a.maxs.z > b.mins.z;
        }

        [[nodiscard]] auto is_door_classname(const std::string& classname) -> bool
        {
            return classname == "func_door" || classname == "door" || classname == "dev_door";
        }

        [[nodiscard]] auto is_trigger_classname(const std::string& classname) -> bool
        {
            return classname == "trigger_once" || classname == "trigger_multiple" || classname == "dev_trigger";
        }

        [[nodiscard]] auto is_usable_classname(const std::string& classname) -> bool
        {
            return classname == "func_button" || classname == "button" || classname == "use_target"
                   || classname == "dev_use";
        }
    } // namespace

    void build_runtime_interactions(
        const BlockoutLevel&         level,
        std::vector<RuntimeDoor>&    doors,
        std::vector<RuntimeTrigger>& triggers,
        std::vector<RuntimeUsable>&  usables
    )
    {
        doors.clear();
        triggers.clear();
        usables.clear();

        std::size_t door_index = 0;
        for (const EntitySpawnData& entity: level.entities)
        {
            if (!is_door_classname(entity.classname)) { continue; }

            RuntimeDoor door{};
            door.name = "door_" + std::to_string(door_index++);
            door.closed_bounds =
                make_centered_aabb(entity.origin + glm::vec3{0.0F, 1.0F, 0.0F}, glm::vec3{0.50F, 1.00F, 0.10F});
            door.open_offset = glm::vec3{0.0F, 2.10F, 0.0F};
            door.move_speed  = 1.5F;
            doors.push_back(std::move(door));
        }

        std::size_t use_index     = 0;
        std::size_t trigger_index = 0;
        for (const EntitySpawnData& entity: level.entities)
        {
            if (is_usable_classname(entity.classname))
            {
                RuntimeUsable     usable{};
                const std::size_t target_index = doors.empty() ? 0 : std::min(use_index, doors.size() - 1);
                usable.label                   = "Use door";
                usable.origin                  = entity.origin;
                usable.use_radius              = 1.75F;
                if (!doors.empty()) { usable.target = doors[target_index].name; }
                usables.push_back(std::move(usable));
                ++use_index;
            }

            if (is_trigger_classname(entity.classname))
            {
                RuntimeTrigger    trigger{};
                const std::size_t target_index = doors.empty() ? 0 : std::min(trigger_index, doors.size() - 1);
                if (!doors.empty()) { trigger.target = doors[target_index].name; }
                trigger.bounds =
                    make_centered_aabb(entity.origin + glm::vec3{0.0F, 1.0F, 0.0F}, glm::vec3{0.75F, 1.00F, 0.75F});
                trigger.once = (entity.classname != "trigger_multiple");
                triggers.push_back(std::move(trigger));
                ++trigger_index;
            }
        }
    }

    auto current_door_bounds(const RuntimeDoor& door) -> physics::Aabb
    {
        const glm::vec3 translation = door.open_offset * glm::clamp(door.open_fraction, 0.0F, 1.0F);
        return {door.closed_bounds.mins + translation, door.closed_bounds.maxs + translation};
    }

    auto is_door_blocking(const RuntimeDoor& door) -> bool
    {
        return door.blocks_when_closed && door.open_fraction < 0.95F;
    }

    void collect_blocking_doors(const std::vector<RuntimeDoor>& doors, std::vector<physics::Aabb>& out_blockers)
    {
        out_blockers.clear();
        out_blockers.reserve(doors.size());

        for (const RuntimeDoor& door: doors)
        {
            if (!is_door_blocking(door)) { continue; }
            out_blockers.push_back(current_door_bounds(door));
        }
    }

    auto find_focused_usable(
        const glm::vec3&                  eye_position,
        const glm::vec3&                  view_forward,
        const std::vector<RuntimeUsable>& usables,
        const float                       max_distance,
        const float                       min_facing_dot
    ) -> InteractionQueryResult
    {
        InteractionQueryResult result{};
        float                  best_distance = std::numeric_limits<float>::max();

        const float     forward_len_sq = glm::dot(view_forward, view_forward);
        const glm::vec3 forward = forward_len_sq > 0.0F ? glm::normalize(view_forward) : glm::vec3{0.0F, 0.0F, -1.0F};

        for (std::size_t i = 0; i < usables.size(); ++i)
        {
            const glm::vec3 to_usable = usables[i].origin - eye_position;
            const float     distance  = glm::length(to_usable);
            if (distance <= 0.0001F || distance > std::min(max_distance, usables[i].use_radius)) { continue; }

            const glm::vec3 direction  = to_usable / distance;
            const float     facing_dot = glm::dot(forward, direction);
            if (facing_dot < min_facing_dot) { continue; }

            const float forward_distance = glm::dot(to_usable, forward);
            if (forward_distance <= 0.0F) { continue; }

            const glm::vec3 closest_point    = eye_position + forward * forward_distance;
            const float     lateral_distance = glm::length(usables[i].origin - closest_point);
            if (lateral_distance > 0.35F) { continue; }

            if (distance < best_distance)
            {
                best_distance       = distance;
                result.usable_index = static_cast<int>(i);
                result.distance     = distance;
            }
        }

        return result;
    }

    auto activate_target(std::string_view target, std::vector<RuntimeDoor>& doors) -> bool
    {
        bool activated = false;
        for (RuntimeDoor& door: doors)
        {
            if (door.name != target) { continue; }
            door.opening = true;
            activated    = true;
        }
        return activated;
    }

    auto update_trigger_overlaps(
        const physics::Aabb&         player_bounds,
        std::vector<RuntimeTrigger>& triggers,
        std::vector<RuntimeDoor>&    doors,
        std::string&                 last_target
    ) -> std::uint32_t
    {
        std::uint32_t fired_count = 0;

        for (RuntimeTrigger& trigger: triggers)
        {
            if (trigger.once && trigger.fired) { continue; }
            if (!aabb_overlaps(player_bounds, trigger.bounds)) { continue; }

            if (!trigger.target.empty() && activate_target(trigger.target, doors)) { last_target = trigger.target; }

            trigger.fired = true;
            ++fired_count;
        }

        return fired_count;
    }

    auto update_doors(const float dt, std::vector<RuntimeDoor>& doors) -> std::uint32_t
    {
        std::uint32_t active_doors = 0;

        for (RuntimeDoor& door: doors)
        {
            if (!door.opening && !door.open) { continue; }

            if (door.opening)
            {
                door.open_fraction = glm::clamp(door.open_fraction + (door.move_speed * dt), 0.0F, 1.0F);
                ++active_doors;
                if (door.open_fraction >= 0.999F)
                {
                    door.open_fraction = 1.0F;
                    door.opening       = false;
                    door.open          = true;
                }
            } else if (door.open)
            {
                ++active_doors;
            }
        }

        return active_doors;
    }
} // namespace RelayCore::gameplay
