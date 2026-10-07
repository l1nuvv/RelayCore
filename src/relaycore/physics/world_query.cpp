#include "relaycore/physics/world_query.h"

#include "relaycore/core/profiling.h"
#include "relaycore/physics/physics_world.h"

#include <algorithm>
#include <cmath>

namespace RelayCore::physics
{
    WorldQuery::WorldQuery(
        const std::span<const WorldCollider> static_colliders,
        const std::span<const WorldCollider> doors,
        const std::span<const WorldCollider> actors
    ) noexcept : colliders_{static_colliders, doors, actors}
    {}

    WorldQuery::WorldQuery(
        const PhysicsWorld& physics,
        const std::span<const WorldCollider> doors,
        const std::span<const WorldCollider> actors
    ) noexcept : colliders_{{physics.box_colliders(), doors, actors}}, physics_(&physics)
    {}

    auto WorldQuery::with_actors(const std::span<const WorldCollider> actors) const noexcept -> WorldQuery
    {
        auto result = *this;
        result.colliders_[2] = actors;
        return result;
    }

    auto WorldQuery::raycast(
        const glm::vec3& origin,
        const glm::vec3& direction,
        const float max_distance,
        const WorldQueryMask mask
    ) const -> std::optional<WorldQueryHit>
    {
        RELAYCORE_PROFILE_FUNCTION();
        auto result =
            physics_ ? physics_->raycast(origin, direction, max_distance, mask) : std::optional<WorldQueryHit>{};
        float distance = result ? result->distance : max_distance;
        for (const auto colliders :
             {physics_ ? std::span<const WorldCollider>{} : colliders_[0], colliders_[1], colliders_[2]})
            for (const auto& collider : colliders)
            {
                if ((static_cast<std::uint8_t>(mask) & static_cast<std::uint8_t>(query_layer(collider.object.kind)))
                    == 0)
                    continue;
                const auto hit = ray_intersection(origin, direction, collider.bounds, distance);
                if (!hit)
                    continue;
                distance = hit->distance;
                result = WorldQueryHit{
                    .distance = distance,
                    .fraction = max_distance > 0 ? distance / max_distance : 0,
                    .position = origin + direction * distance,
                    .normal = hit->normal,
                    .surface = collider.surface,
                    .object = collider.object,
                };
            }
        return result;
    }

    auto WorldQuery::move_axis(
        const glm::vec3& start_position,
        const CollisionConfig& config,
        const int axis,
        const float delta
    ) const -> AxisMoveResult
    {
        RELAYCORE_PROFILE_FUNCTION();
        glm::vec3 position = start_position;
        position[axis] += delta;
        const std::array<float, 3> min_offsets{-config.radius, 0, -config.radius};
        const std::array<float, 3> max_offsets{config.radius, config.height, config.radius};
        bool hit = false;
        if (physics_ && delta != 0)
        {
            glm::vec3 half_extents{config.radius, config.height * .5F, config.radius};
            // Match the AABB overlap tolerance on the two axes orthogonal to travel.
            for (int component = 0; component < 3; ++component)
                if (component != axis)
                    half_extents[component] -= config.epsilon;
            const auto centre = start_position + glm::vec3{0, config.height * .5F, 0};
            glm::vec3 displacement{};
            displacement[axis] = delta;
            if (const auto contact =
                    physics_->cast_box(centre, half_extents, displacement, WorldQueryMask::World, true))
            {
                const auto travel = std::max(0.0F, std::abs(delta) * contact->fraction - config.epsilon);
                position[axis] = start_position[axis] + std::copysign(travel, delta);
                hit = true;
            }
        }
        for (const auto colliders : colliders_)
            for (const auto& collider : colliders)
            {
                const auto& solid = collider.bounds;
                if (!overlaps(make_player_aabb(position, config.radius, config.height), solid, config.epsilon))
                    continue;
                hit = true;
                if (delta > 0)
                    position[axis] = std::min(position[axis], solid.mins[axis] - max_offsets[axis] - config.epsilon);
                else if (delta < 0)
                    position[axis] = std::max(position[axis], solid.maxs[axis] - min_offsets[axis] + config.epsilon);
            }
        return {.position_axis = position[axis], .hit = hit};
    }
} // namespace RelayCore::physics
