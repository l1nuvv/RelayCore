#include "relaycore/physics/physics_world.h"
#include "relaycore/world/world_collision.h"
#include "relaygame/game_world_runtime.h"
#include "relaygame/level_session_builder.h"

#include <array>
#include <cmath>
#include <doctest/doctest.h>
#include <limits>

#include "../tools/physics_fixture_data.h"
#include "scoped_temp_directory.h"
#include "scoped_test_logger.h"

using namespace RelayCore;
using namespace RelayCore::physics;

namespace
{
    auto doorway_mesh(std::span<const PhysicsTriangle> triangles) -> PhysicsMesh
    {
        return {triangles, {WorldObjectKind::CollisionMesh, 0}};
    }
} // namespace

TEST_CASE("Jolt fixture boxes preserve ray distance surface identity and masks")
{
    const std::array boxes{
        WorldCollider{{{2, 0, -1}, {3, 2, 1}}, SurfaceMaterial::Metal, {WorldObjectKind::PropCollider, 1, 7}},
        WorldCollider{{{1, 0, -1}, {1.5F, 2, 1}}, SurfaceMaterial::Concrete, {WorldObjectKind::Enemy, 0, 8}},
    };
    const PhysicsWorld physics{boxes};
    const auto hit = WorldQuery{physics}.raycast({0, 1, 0}, {1, 0, 0}, 4);
    REQUIRE(hit);
    CHECK(hit->distance == doctest::Approx(2));
    CHECK(hit->fraction == doctest::Approx(.5F));
    CHECK(hit->position.x == doctest::Approx(2));
    CHECK(hit->normal == glm::vec3{-1, 0, 0});
    CHECK(hit->surface == SurfaceMaterial::Metal);
    CHECK(hit->object == WorldObjectId{WorldObjectKind::PropCollider, 1, 7});
    const auto actor = physics.raycast({0, 1, 0}, {1, 0, 0}, 4, WorldQueryMask::Actors);
    REQUIRE(actor);
    CHECK(actor->object.kind == WorldObjectKind::Enemy);
    const auto inside = physics.raycast({2.5F, 1, 0}, {1, 0, 0}, 4);
    REQUIRE(inside);
    CHECK(inside->distance == 0);
    CHECK(inside->normal == glm::vec3{});
    CHECK_FALSE(physics.raycast({0, 1, 0}, {1, 0, 0}, 1.99F));
    CHECK_FALSE(physics.raycast({0, 3, 0}, {1, 0, 0}, 4));
    CHECK_FALSE(physics.raycast({0, 1, 0}, {1, 0, 0}, -1));

    const std::array doors{
        WorldCollider{{{.5F, 0, -1}, {.7F, 2, 1}}, SurfaceMaterial::Metal, {WorldObjectKind::Door, 0, 9}},
    };
    CHECK(WorldQuery{physics, doors}.raycast({0, 1, 0}, {1, 0, 0}, 4)->object.kind == WorldObjectKind::Door);
    CHECK_THROWS_AS(static_cast<void>(WorldQuery{physics}.move_axis({}, {}, 0, 1)), std::logic_error);
}

TEST_CASE("Jolt fixture triangle doorway preserves holes winding and per triangle surfaces")
{
    const auto triangles = tool::physics_fixture_doorway();
    const std::array meshes{doorway_mesh(triangles)};
    const PhysicsWorld physics{{}, meshes};
    CHECK_FALSE(physics.raycast({0, 1, 0}, {0, 0, 1}, 6));
    for (const auto [x, surface] :
         {std::pair{-1.0F, SurfaceMaterial::Concrete}, std::pair{1.0F, SurfaceMaterial::Metal}})
    {
        const auto hit = physics.raycast({x, 1, 0}, {0, 0, 1}, 6);
        REQUIRE(hit);
        CHECK(hit->distance == doctest::Approx(3));
        CHECK(hit->normal == glm::vec3{0, 0, -1});
        CHECK(hit->surface == surface);
        CHECK(hit->object == WorldObjectId{WorldObjectKind::CollisionMesh, 0});
    }
    CHECK(physics.raycast({0, 2.5F, 0}, {0, 0, 1}, 6)->surface == SurfaceMaterial::Tile);
    const auto back = physics.raycast({1, 1, 6}, {0, 0, -1}, 6);
    REQUIRE(back);
    CHECK(back->normal == glm::vec3{0, 0, -1});
}

TEST_CASE("Jolt fixture box casts and overlaps distinguish doorway clearance from wall contact")
{
    const auto triangles = tool::physics_fixture_doorway();
    const std::array meshes{doorway_mesh(triangles)};
    const auto boxes = build_static_world_colliders(tool::physics_fixture_level());
    const PhysicsWorld physics{boxes, meshes};
    CHECK_FALSE(physics.cast_box({0, 1, 0}, {.3F, .8F, .3F}, {0, 0, 6}));
    const auto wall = physics.cast_box({1, 1, 0}, {.3F, .8F, .3F}, {0, 0, 6});
    REQUIRE(wall);
    CHECK(wall->distance == doctest::Approx(2.7F).epsilon(.001F));
    CHECK(wall->fraction == doctest::Approx(.45F).epsilon(.001F));
    CHECK(wall->position.z == doctest::Approx(3).epsilon(.001F));
    CHECK(wall->normal.z == doctest::Approx(-1).epsilon(.001F));
    CHECK(wall->surface == SurfaceMaterial::Metal);
    const auto floor = physics.cast_box({0, 1, 0}, {.3F, .3F, .3F}, {0, -2, 0});
    REQUIRE(floor);
    CHECK(floor->fraction == doctest::Approx(.35F).epsilon(.001F));
    CHECK(floor->normal.y == doctest::Approx(1).epsilon(.001F));
    CHECK(floor->surface == SurfaceMaterial::Floor);
    CHECK_FALSE(physics.overlap_box({0, 1, 3}, {.3F, .8F, .3F}));
    const auto overlap = physics.overlap_box({1, 1, 3}, {.3F, .8F, .3F});
    REQUIRE(overlap);
    CHECK(overlap->surface == SurfaceMaterial::Metal);
    CHECK(overlap->fraction == 0);
    CHECK(physics.cast_box({1, 1, 3}, {.3F, .8F, .3F}, {}));
    CHECK_FALSE(physics.overlap_box({0, 1, 3}, {.3F, .8F, .3F}, WorldQueryMask::Actors));
}

TEST_CASE("Jolt fixture malformed preparation fails without changing another live world")
{
    const std::array boxes{WorldCollider{{{2, 0, -1}, {3, 2, 1}}}};
    const PhysicsWorld active{boxes};
    for (int repeat = 0; repeat < 16; ++repeat)
    {
        const std::array triangles{PhysicsTriangle{{}, {}, {1, 1, 1}}};
        const std::array meshes{doorway_mesh(triangles)};
        CHECK_THROWS_WITH_AS(PhysicsWorld(boxes, meshes), doctest::Contains("degenerate"), std::invalid_argument);
        REQUIRE(active.raycast({0, 1, 0}, {1, 0, 0}, 4));
        const PhysicsWorld staged{boxes};
        CHECK(staged.raycast({0, 1, 0}, {1, 0, 0}, 4)->distance == 2);
    }
    CHECK_THROWS_AS(static_cast<void>(active.cast_box({}, {-1, 1, 1}, {0, 0, 1})), std::invalid_argument);
    CHECK_THROWS_AS(static_cast<void>(active.raycast({}, {}, 4)), std::invalid_argument);
    CHECK_THROWS_AS(
        static_cast<void>(active.raycast({std::numeric_limits<float>::quiet_NaN(), 0, 0}, {1, 0, 0}, 4)),
        std::invalid_argument
    );
}

TEST_CASE("Jolt fixture owns geometry and surfaces after input spans are released")
{
    const auto world = [] {
        const auto triangles = tool::physics_fixture_doorway();
        const std::array meshes{doorway_mesh(triangles)};
        return std::make_unique<PhysicsWorld>(std::span<const WorldCollider>{}, meshes);
    }();
    const auto hit = world->raycast({1, 1, 0}, {0, 0, 1}, 6);
    REQUIRE(hit);
    CHECK(hit->distance == doctest::Approx(3));
    CHECK(hit->surface == SurfaceMaterial::Metal);
    CHECK_FALSE(world->cast_box({0, 1, 0}, {.3F, .8F, .3F}, {0, 0, 6}));
}

TEST_CASE("Jolt fixture level session transfers immutable world and retains active scene on failed prepare")
{
    test::ScopedTempDirectory temp;
    test::ScopedTestLogger logger{temp.path() / "logs"};
    core::EngineConfig config;
    RelayGame::LevelSessionBuilder builder{config.gameplay};
    const auto triangles = tool::physics_fixture_doorway();
    const std::array meshes{doorway_mesh(triangles)};
    RelayGame::GameWorldRuntime runtime;
    std::weak_ptr<const PhysicsWorld> previous;
    for (int repeat = 0; repeat < 8; ++repeat)
    {
        auto prepared = builder.build_physics_fixture(tool::physics_fixture_level(), "jolt_fixture.toml", meshes);
        const std::weak_ptr<const PhysicsWorld> current = prepared.physics_fixture;
        runtime.load_level_session(config, std::move(prepared));
        CHECK(previous.expired());
        CHECK_FALSE(current.expired());
        CHECK(runtime.query_physics_fixture({1, 1, 0}, {0, 0, 1}, 6)->surface == SurfaceMaterial::Metal);
        CHECK_FALSE(runtime.query_physics_fixture({0, 1, 0}, {0, 0, 1}, 6));
        const std::array invalid_triangles{PhysicsTriangle{{}, {}, {1, 1, 1}}};
        const std::array invalid_meshes{doorway_mesh(invalid_triangles)};
        CHECK_THROWS(static_cast<void>(
            builder.build_physics_fixture(tool::physics_fixture_level(), "invalid_fixture.toml", invalid_meshes)
        ));
        CHECK(runtime.query_physics_fixture({1, 1, 0}, {0, 0, 1}, 6)->distance == 3);
        previous = current;
    }
    runtime.load_level_session(config, builder.build(tool::physics_fixture_level(), "aabb_fixture.toml"));
    CHECK(previous.expired());
    CHECK_THROWS_AS(static_cast<void>(runtime.query_physics_fixture({1, 1, 0}, {0, 0, 1}, 6)), std::logic_error);
}
