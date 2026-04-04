#pragma once

#include <cstddef>
#include <glm/vec3.hpp>
#include <optional>
#include <span>

namespace RelayCore::physics
{
    struct Aabb {
        glm::vec3 mins{};
        glm::vec3 maxs{};
    };

    struct AxisMoveResult {
        float position_axis = 0.0F;
        bool  hit           = false;
    };

    struct MoveResult {
        glm::vec3 position{};
        bool      hit_x    = false;
        bool      hit_y    = false;
        bool      hit_z    = false;
        bool      grounded = false;
    };

    struct CollisionConfig {
        float radius                   = 0.30F;
        float height                   = 1.80F;
        float epsilon                  = 0.001F;
        float step_height              = 0.35F;
        float ground_probe_distance    = 0.08F;
        float max_ground_snap_distance = 0.10F;
    };

    using CollisionWorld = std::span<const Aabb>;

    [[nodiscard]] auto make_player_aabb(const glm::vec3& position, float radius, float height) -> Aabb;

    [[nodiscard]] auto overlaps(const Aabb& a, const Aabb& b, float epsilon) -> bool;

    [[nodiscard]] auto move_axis(
        CollisionWorld world, const glm::vec3& start_position, const CollisionConfig& config, int axis, float delta
    ) -> AxisMoveResult;

    [[nodiscard]] auto probe_ground(CollisionWorld world, const glm::vec3& position, const CollisionConfig& config)
        -> bool;

    [[nodiscard]] auto snap_to_ground(CollisionWorld world, const glm::vec3& position, const CollisionConfig& config)
        -> glm::vec3;

    [[nodiscard]] auto try_step_move(
        CollisionWorld         world,
        const glm::vec3&       start_position,
        const glm::vec3&       desired_delta_xz,
        const CollisionConfig& config
    ) -> std::optional<glm::vec3>;

    [[nodiscard]] auto move_and_collide(
        CollisionWorld         world,
        const glm::vec3&       start_position,
        const glm::vec3&       velocity,
        float                  dt,
        const CollisionConfig& config,
        bool                   grounded_before_move
    ) -> MoveResult;
} // namespace RelayCore::physics
