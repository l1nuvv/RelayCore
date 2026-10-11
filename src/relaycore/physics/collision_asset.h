#pragma once

#include "relaycore/physics/physics_world.h"

#include <filesystem>
#include <glm/mat4x4.hpp>
#include <string>
#include <string_view>
#include <vector>

namespace RelayCore::physics
{
    // RC collision v1: little endian floats, metres, +Y up, right handed, CCW triangles.
    // Geometry is model-local; instance transforms are baked before creating PhysicsWorld.
    struct CollisionAsset {
        std::string model;
        Aabb bounds;
        std::vector<PhysicsTriangle> triangles;
    };

    void validate_collision_asset(const CollisionAsset& asset);
    void write_collision_asset(const std::filesystem::path& path, const CollisionAsset& asset);
    [[nodiscard]] auto read_collision_asset(const std::filesystem::path& path, std::string_view expected_model)
        -> CollisionAsset;
    [[nodiscard]] auto
    transform_collision_triangles(std::span<const PhysicsTriangle> triangles, const glm::mat4& transform)
        -> std::vector<PhysicsTriangle>;
    [[nodiscard]] auto collision_bounds(std::span<const PhysicsTriangle> triangles) -> Aabb;
} // namespace RelayCore::physics
