#include "relaycore/physics/collision.h"

#include "relaycore/core/profiling.h"

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
            RELAYCORE_PROFILE_FUNCTION();
            const float dx = a.x - b.x;
            const float dz = a.z - b.z;
            return (dx * dx) + (dz * dz);
        }

        [[nodiscard]] auto move_horizontal(
            CollisionWorld world,
            const glm::vec3& start_position,
            const CollisionConfig& config,
            const glm::vec3& desired_delta_xz,
            bool* out_hit_x,
            bool* out_hit_z
        ) -> glm::vec3
        {
            RELAYCORE_PROFILE_FUNCTION();
            glm::vec3 position = start_position;
            bool hit_x = false;
            bool hit_z = false;

            const float travel_distance =
                std::sqrt((desired_delta_xz.x * desired_delta_xz.x) + (desired_delta_xz.z * desired_delta_xz.z));
            const float max_substep_distance = std::max(config.radius * 0.5F, 0.05F);
            const int step_count = std::max(1, static_cast<int>(std::ceil(travel_distance / max_substep_distance)));
            const glm::vec3 step_delta_xz = desired_delta_xz / static_cast<float>(step_count);

            for (int step_index = 0; step_index < step_count; ++step_index)
            {
                const AxisMoveResult x_result = move_axis(world, position, config, 0, step_delta_xz.x);
                hit_x = hit_x || x_result.hit;
                position.x = x_result.position_axis;

                const AxisMoveResult z_result = move_axis(world, position, config, 2, step_delta_xz.z);
                hit_z = hit_z || z_result.hit;
                position.z = z_result.position_axis;
            }

            if (out_hit_x != nullptr)
            {
                *out_hit_x = hit_x;
            }
            if (out_hit_z != nullptr)
            {
                *out_hit_z = hit_z;
            }

            return position;
        }

        [[nodiscard]] auto move_vertical(
            const CollisionWorld world,
            const glm::vec3& start_position,
            const CollisionConfig& config,
            const float desired_delta_y,
            bool* const out_hit_y
        ) -> glm::vec3
        {
            RELAYCORE_PROFILE_FUNCTION();
            glm::vec3 position = start_position;
            bool hit_y = false;

            const float max_substep_distance = std::max(config.radius * 0.5F, 0.05F);
            const int step_count =
                std::max(1, static_cast<int>(std::ceil(std::abs(desired_delta_y) / max_substep_distance)));
            const float step_delta_y = desired_delta_y / static_cast<float>(step_count);

            for (int step_index = 0; step_index < step_count; ++step_index)
            {
                const AxisMoveResult result = move_axis(world, position, config, 1, step_delta_y);
                position.y = result.position_axis;
                if (result.hit)
                {
                    hit_y = true;
                    break;
                }
            }

            if (out_hit_y != nullptr)
            {
                *out_hit_y = hit_y;
            }

            return position;
        }
    } // namespace

    auto make_player_aabb(const glm::vec3& position, const float radius, const float height) -> Aabb
    {
        RELAYCORE_PROFILE_FUNCTION();
        return {
            .mins = {position.x - radius, position.y, position.z - radius},
            .maxs = {position.x + radius, position.y + height, position.z + radius},
        };
    }

    auto overlaps(const Aabb& a, const Aabb& b, const float epsilon) -> bool
    {
        RELAYCORE_PROFILE_FUNCTION();
        return a.maxs.x > (b.mins.x + epsilon) && a.mins.x < (b.maxs.x - epsilon) && a.maxs.y > (b.mins.y + epsilon)
               && a.mins.y < (b.maxs.y - epsilon) && a.maxs.z > (b.mins.z + epsilon) && a.mins.z < (b.maxs.z - epsilon);
    }

    auto ray_intersection_distance(
        const glm::vec3& origin,
        const glm::vec3& direction,
        const Aabb& bounds,
        const float max_distance
    ) -> std::optional<float>
    {
        RELAYCORE_PROFILE_FUNCTION();
        constexpr float kDirectionEpsilon = 0.0001F;

        float t_min = 0.0F;
        float t_max = std::max(max_distance, 0.0F);

        for (int axis = 0; axis < 3; ++axis)
        {
            const float axis_direction = direction[axis];
            const float axis_origin = origin[axis];
            const float axis_min = bounds.mins[axis];
            const float axis_max = bounds.maxs[axis];

            if (std::abs(axis_direction) <= kDirectionEpsilon)
            {
                if (axis_origin < axis_min || axis_origin > axis_max)
                {
                    return std::nullopt;
                }
                continue;
            }

            const float inverse_direction = 1.0F / axis_direction;
            float axis_t0 = (axis_min - axis_origin) * inverse_direction;
            float axis_t1 = (axis_max - axis_origin) * inverse_direction;
            if (axis_t0 > axis_t1)
            {
                std::swap(axis_t0, axis_t1);
            }

            t_min = std::max(t_min, axis_t0);
            t_max = std::min(t_max, axis_t1);
            if (t_min > t_max)
            {
                return std::nullopt;
            }
        }

        return t_min <= max_distance ? std::optional<float>{t_min} : std::nullopt;
    }

    auto move_axis(
        CollisionWorld world,
        const glm::vec3& start_position,
        const CollisionConfig& config,
        const int axis,
        const float delta
    ) -> AxisMoveResult
    {
        RELAYCORE_PROFILE_FUNCTION();
        glm::vec3 position = start_position;
        position[axis] += delta;

        const auto min_offsets = make_min_offsets(config);
        const auto max_offsets = make_max_offsets(config);

        bool hit = false;

        for (const Aabb& solid : world)
        {
            const Aabb player = make_player_aabb(position, config.radius, config.height);
            if (!overlaps(player, solid, config.epsilon))
            {
                continue;
            }

            hit = true;
            if (delta > 0.0F)
            {
                position[axis] = std::min(position[axis], solid.mins[axis] - max_offsets[axis] - config.epsilon);
            } else if (delta < 0.0F)
            {
                position[axis] = std::max(position[axis], solid.maxs[axis] - min_offsets[axis] + config.epsilon);
            }
        }

        return {
            .position_axis = position[axis],
            .hit = hit,
        };
    }

    auto probe_ground(CollisionWorld world, const glm::vec3& position, const CollisionConfig& config) -> bool
    {
        RELAYCORE_PROFILE_FUNCTION();
        const AxisMoveResult result = move_axis(world, position, config, 1, -config.ground_probe_distance);
        if (!result.hit)
        {
            return false;
        }

        const float gap = position.y - result.position_axis;
        return gap >= -config.epsilon && gap <= (config.ground_probe_distance + config.epsilon);
    }

    auto snap_to_ground(CollisionWorld world, const glm::vec3& position, const CollisionConfig& config) -> glm::vec3
    {
        RELAYCORE_PROFILE_FUNCTION();
        const AxisMoveResult result = move_axis(world, position, config, 1, -config.max_ground_snap_distance);

        glm::vec3 snapped = position;
        if (!result.hit)
        {
            return snapped;
        }

        if (result.position_axis <= (position.y + config.epsilon))
        {
            snapped.y = result.position_axis;
        }

        return snapped;
    }

    auto try_step_move(
        CollisionWorld world,
        const glm::vec3& start_position,
        const glm::vec3& desired_delta_xz,
        const CollisionConfig& config
    ) -> std::optional<glm::vec3>
    {
        RELAYCORE_PROFILE_FUNCTION();
        if (std::abs(desired_delta_xz.x) <= config.epsilon && std::abs(desired_delta_xz.z) <= config.epsilon)
        {
            return std::nullopt;
        }

        const AxisMoveResult raise = move_axis(world, start_position, config, 1, config.step_height);
        if (raise.hit)
        {
            return std::nullopt;
        }

        glm::vec3 stepped = start_position;
        stepped.y = raise.position_axis;

        bool hit_x = false;
        bool hit_z = false;
        stepped = move_horizontal(world, stepped, config, desired_delta_xz, &hit_x, &hit_z);

        const float progressed = planar_distance_sq(stepped, start_position);
        if (progressed <= config.epsilon * config.epsilon)
        {
            return std::nullopt;
        }

        CollisionConfig down_config = config;
        down_config.max_ground_snap_distance = config.step_height + config.ground_probe_distance;
        stepped = snap_to_ground(world, stepped, down_config);

        if (!probe_ground(world, stepped, config))
        {
            return std::nullopt;
        }
        return stepped;
    }

    auto move_and_collide(
        CollisionWorld world,
        const glm::vec3& start_position,
        const glm::vec3& velocity,
        const float dt,
        const CollisionConfig& config,
        const bool grounded_before_move
    ) -> MoveResult
    {
        RELAYCORE_PROFILE_FUNCTION();
        const glm::vec3 desired_delta = velocity * dt;
        const glm::vec3 desired_delta_xz = {desired_delta.x, 0.0F, desired_delta.z};

        glm::vec3 position = start_position;
        bool hit_x = false;
        bool hit_y = false;
        bool hit_z = false;
        bool attempted_step = false;
        bool used_step = false;

        glm::vec3 flat_position = move_horizontal(world, position, config, desired_delta_xz, &hit_x, &hit_z);

        if (grounded_before_move && (hit_x || hit_z))
        {
            attempted_step = true;
            if (const std::optional<glm::vec3> stepped = try_step_move(world, position, desired_delta_xz, config))
            {
                const float flat_progress = planar_distance_sq(flat_position, position);
                const float step_progress = planar_distance_sq(*stepped, position);
                if (step_progress > flat_progress + config.epsilon)
                {
                    flat_position = *stepped;
                    hit_x = false;
                    hit_z = false;
                    used_step = true;
                }
            }
        }

        position = flat_position;

        position = move_vertical(world, position, config, desired_delta.y, &hit_y);

        const bool can_probe_ground = desired_delta.y <= config.epsilon;
        const bool probed_ground = !hit_y && can_probe_ground && probe_ground(world, position, config);
        const bool grounded = hit_y ? (desired_delta.y <= config.epsilon) : probed_ground;

        return {
            .position = position,
            .hit_x = hit_x,
            .hit_y = hit_y,
            .hit_z = hit_z,
            .grounded = grounded,
            .attempted_step = attempted_step,
            .used_step = used_step,
            .probed_ground = probed_ground,
        };
    }
} // namespace RelayCore::physics
