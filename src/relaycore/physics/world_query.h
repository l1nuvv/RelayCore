#pragma once

#include "relaycore/physics/collision.h"

#include <array>
#include <cstdint>
#include <span>

namespace RelayCore
{
    enum class SurfaceMaterial : std::uint8_t;
}

namespace RelayCore::physics
{
    enum class WorldObjectKind : std::uint8_t {
        Solid,
        PropCollider,
        Door,
        Player,
        Enemy,
        CollisionMesh,
    };

    struct WorldObjectId {
        WorldObjectKind kind = WorldObjectKind::Solid;
        std::size_t index = 0;
        int authored_entity_index = -1;
        auto operator==(const WorldObjectId&) const -> bool = default;
    };

    struct WorldCollider {
        Aabb bounds{};
        SurfaceMaterial surface{};
        WorldObjectId object{};
    };

    enum class WorldQueryMask : std::uint8_t {
        Static = 1,
        Doors = 2,
        Actors = 4,
        World = 3,
        All = 7,
    };

    [[nodiscard]] constexpr auto query_layer(const WorldObjectKind kind) -> WorldQueryMask
    {
        switch (kind)
        {
            case WorldObjectKind::Door:
                return WorldQueryMask::Doors;
            case WorldObjectKind::Player:
            case WorldObjectKind::Enemy:
                return WorldQueryMask::Actors;
            default:
                return WorldQueryMask::Static;
        }
    }

    class PhysicsWorld;

    struct WorldQueryHit {
        float distance = 0;
        float fraction = 0;
        glm::vec3 position{};
        glm::vec3 normal{};
        SurfaceMaterial surface{};
        WorldObjectId object{};
    };

    // Views borrow level-owned static data and refreshed blockers for the duration of a query.
    class WorldQuery {
    public:
        WorldQuery(
            std::span<const WorldCollider> static_colliders = {},
            std::span<const WorldCollider> doors = {},
            std::span<const WorldCollider> actors = {}
        ) noexcept;
        WorldQuery(
            const PhysicsWorld& physics,
            std::span<const WorldCollider> doors = {},
            std::span<const WorldCollider> actors = {}
        ) noexcept;
        [[nodiscard]] auto with_actors(std::span<const WorldCollider> actors) const noexcept -> WorldQuery;

        // Direction is unit length; distance and position use world units. Inside hits have a zero normal.
        [[nodiscard]] auto raycast(
            const glm::vec3& origin,
            const glm::vec3& direction,
            float max_distance,
            WorldQueryMask mask = WorldQueryMask::World
        ) const -> std::optional<WorldQueryHit>;
        // Feet-origin, world-aligned box; the caller owns step/ground/velocity policy.
        [[nodiscard]] auto
        move_axis(const glm::vec3& start_position, const CollisionConfig& config, int axis, float delta) const
            -> AxisMoveResult;

    private:
        std::array<std::span<const WorldCollider>, 3> colliders_;
        const PhysicsWorld* physics_ = nullptr;
    };
} // namespace RelayCore::physics
