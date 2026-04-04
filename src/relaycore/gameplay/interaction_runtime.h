#pragma once

#include <cstdint>
#include <glm/vec3.hpp>
#include <string>
#include <string_view>
#include <vector>

#include "relaycore/physics/collision.h"

namespace RelayCore
{
    struct BlockoutLevel;
}

namespace RelayCore::gameplay
{
    struct RuntimeDoor {
        std::string   name{};
        physics::Aabb closed_bounds{};
        glm::vec3     open_offset{0.0F, 2.0F, 0.0F};
        float         open_fraction      = 0.0F;
        float         move_speed         = 1.5F;
        bool          opening            = false;
        bool          open               = false;
        bool          blocks_when_closed = true;
    };

    struct RuntimeTrigger {
        std::string   target{};
        physics::Aabb bounds{};
        bool          once  = true;
        bool          fired = false;
    };

    struct RuntimeUsable {
        std::string label{};
        std::string target{};
        glm::vec3   origin{};
        float       use_radius = 2.25F;
    };

    struct InteractionQueryResult {
        int   usable_index = -1;
        float distance     = 0.0F;
    };

    void build_runtime_interactions(
        const BlockoutLevel&         level,
        std::vector<RuntimeDoor>&    doors,
        std::vector<RuntimeTrigger>& triggers,
        std::vector<RuntimeUsable>&  usables
    );

    [[nodiscard]] auto current_door_bounds(const RuntimeDoor& door) -> physics::Aabb;

    [[nodiscard]] auto is_door_blocking(const RuntimeDoor& door) -> bool;

    void collect_blocking_doors(const std::vector<RuntimeDoor>& doors, std::vector<physics::Aabb>& out_blockers);

    [[nodiscard]] auto find_focused_usable(
        const glm::vec3&                  eye_position,
        const glm::vec3&                  view_forward,
        const std::vector<RuntimeUsable>& usables,
        float                             max_distance   = 2.25F,
        float                             min_facing_dot = 0.75F
    ) -> InteractionQueryResult;

    [[nodiscard]] auto activate_target(std::string_view target, std::vector<RuntimeDoor>& doors) -> bool;

    [[nodiscard]] auto update_trigger_overlaps(
        const physics::Aabb&         player_bounds,
        std::vector<RuntimeTrigger>& triggers,
        std::vector<RuntimeDoor>&    doors,
        std::string&                 last_target
    ) -> std::uint32_t;

    [[nodiscard]] auto update_doors(float dt, std::vector<RuntimeDoor>& doors) -> std::uint32_t;
} // namespace RelayCore::gameplay
