#pragma once

#include "relaycore/physics/physics_world.h"
#include "relaycore/world/blockout_level.h"

namespace RelayCore::tool
{
    inline auto physics_fixture_level() -> BlockoutLevel
    {
        BlockoutLevel level;
        level.solids.push_back({.mins = {-4, -.2F, -2}, .maxs = {4, 0, 8}, .material = SurfaceMaterial::Floor});
        return level;
    }

    inline auto physics_fixture_doorway() -> std::vector<physics::PhysicsTriangle>
    {
        std::vector<physics::PhysicsTriangle> triangles;
        const auto panel = [&](float left, float right, float bottom, float top, SurfaceMaterial surface) {
            const glm::vec3 a{left, bottom, 3}, b{left, top, 3}, c{right, top, 3}, d{right, bottom, 3};
            triangles.push_back({a, b, c, surface});
            triangles.push_back({a, c, d, surface});
        };
        panel(-3, -.6F, 0, 3, SurfaceMaterial::Concrete);
        panel(.6F, 3, 0, 3, SurfaceMaterial::Metal);
        panel(-.6F, .6F, 2, 3, SurfaceMaterial::Tile);
        return triangles;
    }
} // namespace RelayCore::tool
