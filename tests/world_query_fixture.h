#pragma once

#include "relaycore/gameplay/interaction_runtime.h"
#include "relaycore/world/world_collision.h"

namespace RelayCore::test
{
    struct WorldQueryFixture {
        explicit WorldQueryFixture(const std::span<const physics::Aabb> bounds)
        {
            colliders.reserve(bounds.size());
            for (std::size_t index = 0; index < bounds.size(); ++index)
                colliders.push_back({.bounds = bounds[index], .object = {physics::WorldObjectKind::Solid, index}});
        }

        explicit WorldQueryFixture(
            const BlockoutLevel& level,
            const std::span<const gameplay::RuntimeDoor> doors = {}
        ) : colliders(build_static_world_colliders(level))
        {
            gameplay::collect_blocking_doors(doors, door_colliders);
        }

        operator physics::WorldQuery() const noexcept
        {
            return {colliders, door_colliders};
        }

        std::vector<physics::WorldCollider> colliders;
        std::vector<physics::WorldCollider> door_colliders;
    };
} // namespace RelayCore::test
