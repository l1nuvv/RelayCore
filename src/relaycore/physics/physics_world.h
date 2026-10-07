#pragma once

#include "relaycore/physics/world_query.h"

#include <memory>

namespace RelayCore::physics
{
    struct PhysicsTriangle {
        glm::vec3 a{}, b{}, c{};
        SurfaceMaterial surface{};
    };

    struct PhysicsMesh {
        std::span<const PhysicsTriangle> triangles;
        WorldObjectId object;
    };

    // Immutable static query world. Coordinates are metres, +Y up, right handed; triangles are CCW.
    // Queries are two sided. This does not run dynamics or implement FPS movement policy.
    class PhysicsWorld {
    public:
        PhysicsWorld(std::span<const WorldCollider> boxes, std::span<const PhysicsMesh> meshes = {});
        ~PhysicsWorld();
        PhysicsWorld(const PhysicsWorld&) = delete;
        auto operator=(const PhysicsWorld&) -> PhysicsWorld& = delete;

        [[nodiscard]] auto object_bounds(WorldObjectId object) const -> std::optional<Aabb>;

        [[nodiscard]] auto raycast(
            const glm::vec3& origin,
            const glm::vec3& direction,
            float max_distance,
            WorldQueryMask mask = WorldQueryMask::World
        ) const -> std::optional<WorldQueryHit>;
        // Box centre/half extents are world aligned. Hit position is on the contacted world surface.
        [[nodiscard]] auto cast_box(
            const glm::vec3& centre,
            const glm::vec3& half_extents,
            const glm::vec3& displacement,
            WorldQueryMask mask = WorldQueryMask::World
        ) const -> std::optional<WorldQueryHit>;
        [[nodiscard]] auto
        overlap_box(const glm::vec3& centre, const glm::vec3& half_extents, WorldQueryMask mask = WorldQueryMask::World)
            const -> std::optional<WorldQueryHit>;

    private:
        friend class WorldQuery;
        [[nodiscard]] auto box_colliders() const noexcept -> std::span<const WorldCollider>;
        [[nodiscard]] auto cast_box(
            const glm::vec3& centre,
            const glm::vec3& half_extents,
            const glm::vec3& displacement,
            WorldQueryMask mask,
            bool meshes_only
        ) const -> std::optional<WorldQueryHit>;
        struct Impl;
        std::unique_ptr<Impl> impl_;
    };
} // namespace RelayCore::physics
