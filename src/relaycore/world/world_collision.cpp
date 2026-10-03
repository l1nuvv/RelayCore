#include "relaycore/world/world_collision.h"

#include "relaycore/world/blockout_level.h"

namespace RelayCore
{
    auto build_static_world_colliders(const BlockoutLevel& level) -> std::vector<physics::WorldCollider>
    {
        std::vector<physics::WorldCollider> result;
        result.reserve(level.solids.size() + level.prop_colliders.size());
        for (std::size_t index = 0; index < level.solids.size(); ++index)
        {
            const auto& solid = level.solids[index];
            result.push_back({{solid.mins, solid.maxs}, solid.material, {physics::WorldObjectKind::Solid, index}});
        }
        for (std::size_t index = 0; index < level.prop_colliders.size(); ++index)
        {
            const auto& solid = level.prop_colliders[index];
            result.push_back({{solid.mins, solid.maxs}, solid.material, {physics::WorldObjectKind::PropCollider, index}}
            );
        }
        return result;
    }
} // namespace RelayCore
