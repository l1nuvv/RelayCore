#include "relaycore/physics/collision_asset.h"
#include "relaycore/physics/physics_world.h"
#include "relaycore/physics/player_motion.h"

#include <array>
#include <cmath>
#include <doctest/doctest.h>
#include <glm/ext/matrix_transform.hpp>

#include "../tools/physics_fixture_data.h"

using namespace RelayCore::physics;

TEST_CASE("mesh movement baseline fixes acceleration friction jump and air control at 120 Hz")
{
    const std::array floor{WorldCollider{{{-20, -1, -20}, {20, 0, 20}}}};
    const WorldQuery world{floor};
    PlayerMotionConfig config;
    config.max_speed = 4.5F;
    config.gravity = 24;
    config.jump_speed = 8.5F;
    PlayerMotionState state{{0, .001F, 0}, {}, true};
    const MoveInput forward{.forward_axis = 1};
    update_grounded(state, forward, 0, 1.0F / 120, world, config);
    CHECK(state.velocity.x == doctest::Approx(.375F));
    update_grounded(state, forward, 0, 1.0F / 120, world, config);
    CHECK(state.velocity.x == doctest::Approx(.7F));
    for (int tick = 2; tick < 120; ++tick)
        update_grounded(state, forward, 0, 1.0F / 120, world, config);
    CHECK(state.velocity.x == doctest::Approx(4.5F));
    update_grounded(state, {}, 0, 1.0F / 120, world, config);
    CHECK(state.velocity.x == doctest::Approx(4.35F));
    const auto feet_y = state.position.y;
    update_grounded(state, {.jump_pressed = true}, 0, 1.0F / 120, world, config);
    CHECK_FALSE(state.grounded);
    CHECK(state.velocity.y == doctest::Approx(8.3F));
    CHECK(state.position.y - feet_y == doctest::Approx(.07F));
    const auto velocity_x = state.velocity.x;
    update_grounded(state, {.right_axis = 1}, 0, 1.0F / 120, world, config);
    CHECK(state.velocity.x == velocity_x);
    CHECK(state.velocity.z > 0);
}

TEST_CASE("mesh movement preserves the AABB trajectory across acceleration jump slide step and snap")
{
    const std::array floor{WorldCollider{{{-20, -1, -20}, {20, 0, 20}}}};
    for (const auto obstacle : std::array<Aabb, 4>{
             {{{3, 0, -3}, {3.02F, 5, 3}},
              {{1, 0, -3}, {2, .25F, 3}},
              {{1, 0, -3}, {2, .022F, 3}},
              {{-20, 2.2F, -20}, {20, 2.22F, 20}}}
         })
    {
        const std::array boxes{floor.front(), WorldCollider{obstacle}};
        const PhysicsWorld mesh{boxes};
        PlayerMotionState aabb{{0, .001F, 0}, {}, true}, actual = aabb;
        PlayerMotionConfig config;
        for (int tick = 0; tick < 360; ++tick)
        {
            const MoveInput input{
                .forward_axis = tick < 180 ? 1.0F : 0.0F,
                .right_axis = tick < 180 ? .3F : 0.0F,
                .jump_pressed = tick == 90
            };
            update_grounded(aabb, input, 0, 1.0F / 120, WorldQuery{boxes}, config);
            update_grounded(actual, input, 0, 1.0F / 120, WorldQuery{mesh}, config);
            CAPTURE(tick);
            REQUIRE(actual.position == aabb.position);
            REQUIRE(actual.grounded == aabb.grounded);
            REQUIRE(actual.velocity == aabb.velocity);
        }
    }
}

TEST_CASE("mesh floors preserve ground probe snap jump and ceiling clearance")
{
    const std::array triangles{
        PhysicsTriangle{{-4, 0, -4}, {4, 0, 4}, {4, 0, -4}},
        PhysicsTriangle{{-4, 0, -4}, {-4, 0, 4}, {4, 0, 4}},
        PhysicsTriangle{{-4, 2.2F, -4}, {4, 2.2F, -4}, {4, 2.2F, 4}},
        PhysicsTriangle{{-4, 2.2F, -4}, {4, 2.2F, 4}, {-4, 2.2F, 4}}
    };
    const std::array meshes{PhysicsMesh{triangles, {WorldObjectKind::CollisionMesh, 0}}};
    const PhysicsWorld physics{{}, meshes};
    const WorldQuery world{physics};
    PlayerMotionConfig config;
    PlayerMotionState state{{0, .06F, 0}, {}, false};
    GroundedMoveDiagnostics diagnostics;
    update_grounded(state, {}, 0, 1.F / 120, world, config, &diagnostics);
    CHECK(diagnostics.ground_probe_hit);
    CHECK(state.grounded);
    CHECK(std::abs(state.position.y - .001F) <= .002F);
    update_grounded(state, {.jump_pressed = true}, 0, 1.F / 120, world, config);
    CHECK_FALSE(state.grounded);
    bool hit_ceiling = false;
    for (int tick = 0; tick < 120; ++tick)
    {
        update_grounded(state, {}, 0, 1.F / 120, world, config, &diagnostics);
        hit_ceiling = hit_ceiling || diagnostics.hit_y && !state.grounded;
        CHECK(state.position.y <= .402F);
    }
    CHECK(hit_ceiling);
    CHECK(state.grounded);
    CHECK(std::abs(state.position.y - .001F) <= .002F);
}

TEST_CASE("native mesh step uses the existing step height and returns to the floor")
{
    const auto asset = read_collision_asset(RELAYCORE_TEST_COLLISION_PRODUCT, "models/lab_bench.rcmodel");
    const std::array floor{WorldCollider{{{-4, -.2F, -2}, {4, 0, 8}}}};
    for (const float scale_y : {.25F, 1.0F})
    {
        const auto transform = glm::scale(glm::translate(glm::mat4{1}, glm::vec3{0, 0, 3}), glm::vec3{1, scale_y, 1});
        const auto triangles = transform_collision_triangles(asset.triangles, transform);
        const std::array meshes{PhysicsMesh{triangles, {WorldObjectKind::CollisionMesh, 0}}};
        const PhysicsWorld physics{floor, meshes};
        PlayerMotionState state{{0, .001F, 0}, {}, true};
        PlayerMotionConfig config;
        bool stepped = false;
        for (int tick = 0; tick < 180; ++tick)
        {
            GroundedMoveDiagnostics diagnostics;
            update_grounded(
                state,
                {.forward_axis = 1},
                1.5707963268F,
                1.F / 120,
                WorldQuery{physics},
                config,
                &diagnostics
            );
            stepped = stepped || diagnostics.used_step_move;
        }
        CHECK(stepped == (scale_y == .25F));
        CHECK((state.position.z > 4) == (scale_y == .25F));
        CHECK(state.grounded);
        CHECK(std::abs(state.position.y - .001F) <= .002F);
    }
}

TEST_CASE("mesh movement passes through a triangle opening and stops at thin panels floors and ceilings")
{
    const auto triangles = RelayCore::tool::physics_fixture_doorway();
    const std::array meshes{PhysicsMesh{triangles, {WorldObjectKind::CollisionMesh, 0}}};
    const std::array boxes{WorldCollider{{{-4, -.02F, -2}, {4, 0, 8}}}, WorldCollider{{{-4, 3.1F, -2}, {4, 3.12F, 8}}}};
    const PhysicsWorld physics{boxes, meshes};
    const WorldQuery world{physics};
    const CollisionConfig config;
    const auto open = move_and_collide(world, {0, .001F, 0}, {0, -.1F, 20}, .3F, config, true);
    CHECK(open.position.z > 5.9F);
    const auto blocked = move_and_collide(world, {1, .001F, 0}, {0, -.1F, 20}, .3F, config, true);
    CHECK(blocked.hit_z);
    CHECK(std::abs(blocked.position.z - 2.699F) <= .002F);
    const auto floor = move_and_collide(world, {0, 1, 0}, {0, -100, 0}, .1F, config, false);
    CHECK(std::abs(floor.position.y - .001F) <= .002F);
    const auto ceiling = move_and_collide(world, {0, .001F, 0}, {0, 100, 0}, .1F, config, false);
    CHECK(std::abs(ceiling.position.y - 1.299F) <= .002F);
}
