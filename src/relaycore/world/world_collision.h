#pragma once

#include "relaycore/physics/world_query.h"

#include <vector>

namespace RelayCore
{
    struct BlockoutLevel;

    // Solid/prop indices refer to their respective arrays in the prepared level.
    [[nodiscard]] auto build_static_world_colliders(const BlockoutLevel& level) -> std::vector<physics::WorldCollider>;
} // namespace RelayCore
