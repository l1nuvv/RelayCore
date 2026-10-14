#include "relaycore/gameplay/interaction_runtime.h"
#include "relaycore/physics/world_query.h"
#include "relaycore/world/blockout_level.h"
#include "relaycore/world/world_collision.h"

#include <array>
#include <doctest/doctest.h>

using namespace RelayCore;
using namespace RelayCore::physics;

TEST_CASE("world query reports nearest surface identity distance and outward normal")
{
    const std::array solids{
        WorldCollider{{{4, 0, -1}, {5, 2, 1}}, SurfaceMaterial::Concrete, {WorldObjectKind::Solid, 3}},
        WorldCollider{{{2, 0, -1}, {3, 2, 1}}, SurfaceMaterial::Metal, {WorldObjectKind::PropCollider, 1, 7}},
    };
    const WorldQuery world{solids};
    const auto hit = world.raycast({0, 1, 0}, {1, 0, 0}, 10);
    REQUIRE(hit);
    CHECK(hit->distance == doctest::Approx(2));
    CHECK(hit->fraction == doctest::Approx(.2F));
    CHECK(hit->position == glm::vec3{2, 1, 0});
    CHECK(hit->normal == glm::vec3{-1, 0, 0});
    CHECK(hit->surface == SurfaceMaterial::Metal);
    CHECK(hit->object == WorldObjectId{WorldObjectKind::PropCollider, 1, 7});
    const auto reverse = world.raycast({6, 1, 0}, {-1, 0, 0}, 10);
    REQUIRE(reverse);
    CHECK(reverse->normal == glm::vec3{1, 0, 0});
    CHECK(reverse->object.index == 3);
    CHECK_FALSE(world.raycast({0, 1, 0}, {1, 0, 0}, 1.99F));
    CHECK_FALSE(world.raycast({0, 3, 0}, {1, 0, 0}, 10));
}

TEST_CASE("world query keeps actor targeting separate and resolves equal hits in source order")
{
    const std::array solids{
        WorldCollider{{{2, 0, -1}, {3, 2, 1}}, SurfaceMaterial::Concrete, {WorldObjectKind::Solid, 0}},
    };
    const std::array doors{
        WorldCollider{{{2, 0, -1}, {3, 2, 1}}, SurfaceMaterial::Metal, {WorldObjectKind::Door, 0, 4}},
    };
    const std::array actors{
        WorldCollider{{{1, 0, -1}, {1.5F, 2, 1}}, SurfaceMaterial::Concrete, {WorldObjectKind::Enemy, 2, 8}},
    };
    const WorldQuery world{solids, doors, actors};
    const auto world_hit = world.raycast({0, 1, 0}, {1, 0, 0}, 2);
    REQUIRE(world_hit);
    CHECK(world_hit->object.kind == WorldObjectKind::Door);
    CHECK(world_hit->distance == 2);
    CHECK(world_hit->fraction == 1);
    const auto actor = world.raycast({0, 1, 0}, {1, 0, 0}, 2, WorldQueryMask::All);
    REQUIRE(actor);
    CHECK(actor->object == WorldObjectId{WorldObjectKind::Enemy, 2, 8});
    CHECK(world.raycast({0, 1, 0}, {1, 0, 0}, 2, WorldQueryMask::Static)->object.kind == WorldObjectKind::Solid);
    const auto inside = world.raycast({2.5F, 1, 0}, {1, 0, 0}, 0);
    REQUIRE(inside);
    CHECK(inside->distance == 0);
    CHECK(inside->fraction == 0);
    CHECK(inside->normal == glm::vec3{});
    CHECK_FALSE(world.raycast({2.5F, 1, 0}, {1, 0, 0}, -1));
}

TEST_CASE("world queries refresh moved doors and preserve actor overlap movement policy")
{
    std::vector<gameplay::RuntimeDoor> doors{{
        .authored_entity_index = 9,
        .closed_bounds = {{-.5F, 0, 1}, {.5F, 2, 1.5F}},
        .open_offset = {2, 0, 0},
    }};
    std::vector<WorldCollider> blockers;
    gameplay::collect_blocking_doors(doors, blockers);
    REQUIRE(blockers.size() == 1);
    const auto hit = WorldQuery{{}, blockers}.raycast({0, 1, 0}, {0, 0, 1}, 3);
    REQUIRE(hit);
    CHECK(hit->object == WorldObjectId{WorldObjectKind::Door, 0, 9});
    CHECK(hit->surface == SurfaceMaterial::Metal);
    doors[0].open_fraction = .5F;
    doors[0].state = gameplay::DoorState::Closing;
    const Aabb actor{{.6F, 0, .9F}, {1.2F, 1.8F, 1.6F}};
    gameplay::collect_blocking_doors(doors, blockers);
    CHECK_FALSE(WorldQuery{{}, blockers}.raycast({0, 1, 0}, {0, 0, 1}, 3));
    CHECK(WorldQuery{{}, blockers}.raycast({1, 1, 0}, {0, 0, 1}, 3));
    gameplay::collect_blocking_doors(doors, actor, blockers);
    CHECK(blockers.empty());
    doors[0].state = gameplay::DoorState::Closed;
    gameplay::collect_blocking_doors(doors, actor, blockers);
    CHECK(blockers.size() == 1);
    doors[0].open_fraction = .95F;
    gameplay::collect_blocking_doors(doors, blockers);
    CHECK(blockers.empty());

    doors[0].closed_bounds = {{-1, -1, -1}, {1, 2, 1}};
    doors[0].open_fraction = 0;
    doors[0].state = gameplay::DoorState::Closing;
    gameplay::collect_blocking_doors(doors, blockers);
    REQUIRE(blockers.size() == 1);
    CHECK(WorldQuery{{}, blockers}.raycast({0, 1, 0}, {0, 0, 1}, 3));
}

TEST_CASE("local static candidates preserve contact cascades outside the query bounds")
{
    const std::array solids{
        WorldCollider{.bounds = {{-2, 0, -1}, {1, 2, 1}}},
        WorldCollider{.bounds = {{-4, 0, -1}, {-2, 2, 1}}},
    };
    const WorldQuery world{solids};
    std::vector<WorldCollider> candidates;
    bool contained = true;
    const auto local = world.local_static({{-.5F, -.2F, -.5F}, {.6F, 2, .5F}}, candidates, &contained);
    const auto raw = world.move_axis({}, {}, 0, .1F);
    CHECK(raw.position_axis < -4);
    CHECK(local.move_axis({}, {}, 0, .1F).position_axis == raw.position_axis);
    CHECK_FALSE(contained);
    CHECK(
        local.move_axis({-3, 0, 0}, {}, 0, .1F).position_axis == world.move_axis({-3, 0, 0}, {}, 0, .1F).position_axis
    );
}

TEST_CASE("static world query identity distinguishes prop colliders from blockout solids")
{
    BlockoutLevel level;
    level.solids.push_back({.mins = {2, 0, -1}, .maxs = {3, 2, 1}, .material = SurfaceMaterial::Tile});
    level.prop_colliders.push_back({.mins = {1, 0, -1}, .maxs = {1.5F, 2, 1}, .material = SurfaceMaterial::Glass});
    const auto colliders = build_static_world_colliders(level);
    const auto hit = WorldQuery{colliders}.raycast({0, 1, 0}, {1, 0, 0}, 4);
    REQUIRE(hit);
    CHECK(hit->surface == SurfaceMaterial::Glass);
    CHECK(hit->object == WorldObjectId{WorldObjectKind::PropCollider, 0});
}
