#include "relaycore/physics/collision.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace RelayCore::physics
{
    namespace
    {
        [[nodiscard]] constexpr auto make_min_offsets(const CollisionConfig& config) -> std::array<float, 3>
        {
            return {-config.radius, 0.0F, -config.radius};
        }

        [[nodiscard]] constexpr auto make_max_offsets(const CollisionConfig& config) -> std::array<float, 3>
        {
            return {config.radius, config.height, config.radius};
        }

        [[nodiscard]] auto planar_distance_sq(const glm::vec3& a, const glm::vec3& b) -> float
        {
            const float dx = a.x - b.x;
            const float dz = a.z - b.z;
            return (dx * dx) + (dz * dz);
        }

        [[nodiscard]] auto move_horizontal(
            CollisionWorld         world,
            const glm::vec3&       start_position,
            const CollisionConfig& config,
            const glm::vec3&       desired_delta_xz,
            bool*                  out_hit_x,
            bool*                  out_hit_z
        ) -> glm::vec3
        {
            glm::vec3 position = start_position;

            const AxisMoveResult x_result = move_axis(world, position, config, 0, desired_delta_xz.x);
            if (out_hit_x != nullptr) { *out_hit_x = x_result.hit; }
            position.x = x_result.position_axis;

            const AxisMoveResult z_result = move_axis(world, position, config, 2, desired_delta_xz.z);
            if (out_hit_z != nullptr) { *out_hit_z = z_result.hit; }
            position.z = z_result.position_axis;

            return position;
        }
    } // namespace

    auto make_player_aabb(const glm::vec3& position, const float radius, const float height) -> Aabb
    {
        return {
            .mins = {position.x - radius, position.y, position.z - radius},
            .maxs = {position.x + radius, position.y + height, position.z + radius},
        };
    }

    auto overlaps(const Aabb& a, const Aabb& b, const float epsilon) -> bool
    {
        return a.maxs.x > (b.mins.x + epsilon) && a.mins.x < (b.maxs.x - epsilon) && a.maxs.y > (b.mins.y + epsilon)
               && a.mins.y < (b.maxs.y - epsilon) && a.maxs.z > (b.mins.z + epsilon) && a.mins.z < (b.maxs.z - epsilon);
    }

    auto move_axis(
        CollisionWorld         world,
        const glm::vec3&       start_position,
        const CollisionConfig& config,
        const std::size_t      axis,
        const float            delta
    ) -> AxisMoveResult
    {
        glm::vec3 position = start_position;
        position[axis] += delta;

        const auto min_offsets = make_min_offsets(config);
        const auto max_offsets = make_max_offsets(config);

        bool hit = false;

        for (const Aabb& solid: world)
        {
            const Aabb player = make_player_aabb(position, config.radius, config.height);
            if (!overlaps(player, solid, config.epsilon)) { continue; }

            hit = true;
            if (delta > 0.0F)
            {
                const float separation = (axis == 1) ? 0.0F : config.epsilon;
                position[axis]         = std::min(position[axis], solid.mins[axis] - max_offsets[axis] - separation);
            } else if (delta < 0.0F)
            {
                const float separation = (axis == 1) ? 0.0F : config.epsilon;
                position[axis]         = std::max(position[axis], solid.maxs[axis] - min_offsets[axis] + separation);
            }
        }

        return {
            .position_axis = position[axis],
            .hit           = hit,
        };
    }

    auto probe_ground(CollisionWorld world, const glm::vec3& position, const CollisionConfig& config) -> bool
    {
        const AxisMoveResult result = move_axis(world, position, config, 1, -config.ground_probe_distance);
        if (!result.hit) { return false; }

        const float gap = position.y - result.position_axis;
        return gap >= -config.epsilon && gap <= (config.ground_probe_distance + config.epsilon);
    }

    auto snap_to_ground(CollisionWorld world, const glm::vec3& position, const CollisionConfig& config) -> glm::vec3
    {
        const AxisMoveResult result = move_axis(world, position, config, 1, -config.max_ground_snap_distance);

        glm::vec3 snapped = position;
        if (!result.hit) { return snapped; }

        if (result.position_axis <= (position.y + config.epsilon)) { snapped.y = result.position_axis; }

        return snapped;
    }

    auto try_step_move(
        CollisionWorld         world,
        const glm::vec3&       start_position,
        const glm::vec3&       desired_delta_xz,
        const CollisionConfig& config
    ) -> std::optional<glm::vec3>
    {
        if (std::abs(desired_delta_xz.x) <= config.epsilon && std::abs(desired_delta_xz.z) <= config.epsilon)
        {
            return std::nullopt;
        }

        const AxisMoveResult raise = move_axis(world, start_position, config, 1, config.step_height);
        if (raise.hit) { return std::nullopt; }

        glm::vec3 stepped = start_position;
        stepped.y         = raise.position_axis;

        bool hit_x = false;
        bool hit_z = false;
        stepped    = move_horizontal(world, stepped, config, desired_delta_xz, &hit_x, &hit_z);

        const float progressed = planar_distance_sq(stepped, start_position);
        if (progressed <= config.epsilon * config.epsilon) { return std::nullopt; }

        CollisionConfig down_config          = config;
        down_config.max_ground_snap_distance = config.step_height + config.ground_probe_distance;
        stepped                              = snap_to_ground(world, stepped, down_config);

        if (!probe_ground(world, stepped, config)) { return std::nullopt; }
        return stepped;
    }

    auto move_and_collide(
        CollisionWorld         world,
        const glm::vec3&       start_position,
        const glm::vec3&       velocity,
        const float            dt,
        const CollisionConfig& config,
        const bool             grounded_before_move
    ) -> MoveResult
    {
        const glm::vec3 desired_delta    = velocity * dt;
        const glm::vec3 desired_delta_xz = {desired_delta.x, 0.0F, desired_delta.z};

        glm::vec3 position = start_position;
        bool      hit_x    = false;
        bool      hit_y    = false;
        bool      hit_z    = false;

        glm::vec3 flat_position = move_horizontal(world, position, config, desired_delta_xz, &hit_x, &hit_z);

        if (grounded_before_move && (hit_x || hit_z))
        {
            if (const std::optional<glm::vec3> stepped = try_step_move(world, position, desired_delta_xz, config))
            {
                const float flat_progress = planar_distance_sq(flat_position, position);
                const float step_progress = planar_distance_sq(*stepped, position);
                if (step_progress > flat_progress + config.epsilon)
                {
                    flat_position = *stepped;
                    hit_x         = false;
                    hit_z         = false;
                }
            }
        }

        position = flat_position;

        const AxisMoveResult y_result = move_axis(world, position, config, 1, desired_delta.y);
        hit_y                         = y_result.hit;
        position.y                    = y_result.position_axis;

        const bool can_probe_ground = desired_delta.y <= config.epsilon;
        const bool grounded =
            hit_y ? (desired_delta.y <= config.epsilon) : (can_probe_ground && probe_ground(world, position, config));

        return {
            .position = position,
            .hit_x    = hit_x,
            .hit_y    = hit_y,
            .hit_z    = hit_z,
            .grounded = grounded,
        };
    }
} // namespace RelayCore::physics
