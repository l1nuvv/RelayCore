#pragma once

#include <cstddef>
#include <glm/vec3.hpp>
#include <optional>

namespace RelayCore::physics
{
    struct Aabb {
        glm::vec3 mins{};
        glm::vec3 maxs{};
    };

    struct AxisMoveResult {
        float position_axis = 0.0F;
        bool hit = false;
    };

    struct MoveResult {
        glm::vec3 position{};
        bool hit_x = false;
        bool hit_y = false;
        bool hit_z = false;
        bool grounded = false;
        bool attempted_step = false;
        bool used_step = false;
        bool probed_ground = false;
    };

    struct CollisionConfig {
        float radius = 0.30F;
        float height = 1.80F;
        float epsilon = 0.001F;
        float step_height = 0.35F;
        float ground_probe_distance = 0.08F;
        float max_ground_snap_distance = 0.10F;
    };

    class WorldQuery;

    struct RayIntersection {
        float distance = 0;
        glm::vec3 normal{};
    };

    [[nodiscard]] auto
    ray_intersection(const glm::vec3& origin, const glm::vec3& direction, const Aabb& bounds, float max_distance)
        -> std::optional<RayIntersection>;

    [[nodiscard]] auto make_player_aabb(const glm::vec3& position, float radius, float height) -> Aabb;

    [[nodiscard]] auto overlaps(const Aabb& a, const Aabb& b, float epsilon) -> bool;

    [[nodiscard]] auto ray_intersection_distance(
        const glm::vec3& origin,
        const glm::vec3& direction,
        const Aabb& bounds,
        float max_distance
    ) -> std::optional<float>;

    [[nodiscard]] auto probe_ground(WorldQuery world, const glm::vec3& position, const CollisionConfig& config) -> bool;

    [[nodiscard]] auto snap_to_ground(WorldQuery world, const glm::vec3& position, const CollisionConfig& config)
        -> glm::vec3;

    [[nodiscard]] auto try_step_move(
        WorldQuery world,
        const glm::vec3& start_position,
        const glm::vec3& desired_delta_xz,
        const CollisionConfig& config
    ) -> std::optional<glm::vec3>;

    [[nodiscard]] auto move_and_collide(
        WorldQuery world,
        const glm::vec3& start_position,
        const glm::vec3& velocity,
        float dt,
        const CollisionConfig& config,
        bool grounded_before_move
    ) -> MoveResult;
} // namespace RelayCore::physics
