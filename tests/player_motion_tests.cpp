#include "relaycore/core/profiling.h"
#include "relaycore/physics/player_motion.h"

#include <doctest/doctest.h>
#include <glm/common.hpp>
#include <vector>

#include "world_query_fixture.h"

namespace
{
    using RelayCore::physics::Aabb;
    using RelayCore::physics::GroundedMoveDiagnostics;
    using RelayCore::physics::MoveInput;
    using RelayCore::physics::MoveResult;
    using RelayCore::physics::PlayerMotionConfig;
    using RelayCore::physics::PlayerMotionState;
    using RelayCore::test::WorldQueryFixture;

    [[nodiscard]] auto make_config() -> PlayerMotionConfig
    {
        PlayerMotionConfig config{};
        config.max_speed = 4.0F;
        config.ground_accelerate = 10.0F;
        config.air_accelerate = 10.0F;
        config.ground_friction = 4.0F;
        config.stop_speed = 1.5F;
        config.gravity = 20.0F;
        config.jump_speed = 6.5F;
        config.air_wishspeed_cap = 1.0F;
        config.collision.radius = 0.30F;
        config.collision.height = 1.80F;
        config.collision.step_height = 0.35F;
        config.collision.ground_probe_distance = 0.08F;
        config.collision.max_ground_snap_distance = 0.10F;
        return config;
    }

    [[nodiscard]] auto make_floor_world() -> std::vector<Aabb>
    {
        return {
            Aabb{
                .mins = {-20.0F, -1.0F, -20.0F},
                .maxs = {20.0F, 0.0F, 20.0F},
            },
        };
    }
} // namespace

TEST_CASE("small thresholds are walkable at the 120 Hz game simulation rate")
{
    auto world = make_floor_world();
    world.push_back({{-1.5F, 0, 0.5F}, {1.5F, 0.022F, 0.88F}});
    for (const float speed : {1.0F, 3.5F, 10.5F})
    {
        glm::vec3 position{0, 0.001F, 0};
        for (int tick = 0; tick < 240; ++tick)
            position = RelayCore::physics::move_and_collide(
                           RelayCore::test::WorldQueryFixture{world},
                           position,
                           {0, -0.1F, speed},
                           1.0F / 120.0F,
                           make_config().collision,
                           true
            )
                           .position;
        CAPTURE(speed);
        CHECK(position.z > 1.5F);
    }
}

TEST_CASE("player falls onto floor and becomes grounded")
{
    const PlayerMotionConfig config = make_config();
    const std::vector<Aabb> world = make_floor_world();

    PlayerMotionState state{};
    state.position = {0.0F, 1.0F, 0.0F};
    state.velocity = {0.0F, 0.0F, 0.0F};
    state.grounded = false;

    for (int i = 0; i < 120 && !state.grounded; ++i)
    {
        RelayCore::physics::update_grounded(state, MoveInput{}, 0.0F, 1.0F / 60.0F, WorldQueryFixture{world}, config);
    }

    CHECK(state.grounded);
    CHECK(state.position.y == doctest::Approx(0.0F).epsilon(0.01));
    CHECK(state.velocity.y == doctest::Approx(0.0F).epsilon(0.01));
}

TEST_CASE("jump gives positive Y velocity and leaves grounded state")
{
    const PlayerMotionConfig config = make_config();
    const std::vector<Aabb> world = make_floor_world();

    PlayerMotionState state{};
    state.position = {0.0F, 0.0F, 0.0F};
    state.grounded = true;

    MoveInput input{};
    input.jump_pressed = true;

    RelayCore::physics::update_grounded(state, input, 0.0F, 1.0F / 60.0F, WorldQueryFixture{world}, config);

    CHECK_FALSE(state.grounded);
    CHECK(state.velocity.y > 0.0F);
    CHECK(state.position.y > 0.0F);
}

TEST_CASE("wall collision blocks forward movement")
{
    const PlayerMotionConfig config = make_config();
    std::vector<Aabb> world = make_floor_world();
    world.push_back(Aabb{
        .mins = {0.8F, 0.0F, -1.0F},
        .maxs = {1.0F, 2.0F, 1.0F},
    });

    PlayerMotionState state{};
    state.position = {0.0F, 0.0F, 0.0F};
    state.grounded = true;

    MoveInput input{};
    input.forward_axis = 1.0F;

    for (int i = 0; i < 60; ++i)
    {
        RelayCore::physics::update_grounded(state, input, 0.0F, 1.0F / 60.0F, WorldQueryFixture{world}, config);
    }

    CHECK(state.position.x <= doctest::Approx(0.50F).epsilon(0.05));
}

TEST_CASE("diagonal move into wall slides along free axis")
{
    const PlayerMotionConfig config = make_config();
    std::vector<Aabb> world = make_floor_world();
    world.push_back(Aabb{
        .mins = {0.8F, 0.0F, -4.0F},
        .maxs = {1.0F, 2.0F, 4.0F},
    });

    PlayerMotionState state{};
    state.position = {0.0F, 0.0F, 0.0F};
    state.grounded = true;

    MoveInput input{};
    input.forward_axis = 1.0F;
    input.right_axis = 1.0F;

    for (int i = 0; i < 60; ++i)
    {
        RelayCore::physics::update_grounded(state, input, 0.0F, 1.0F / 60.0F, WorldQueryFixture{world}, config);
    }

    CHECK(state.position.x <= doctest::Approx(0.50F).epsilon(0.05));
    CHECK(state.position.z > 0.25F);
}

TEST_CASE("noclip ignores solids")
{
    const PlayerMotionConfig config = make_config();

    PlayerMotionState state{};
    state.position = {0.0F, 0.0F, 0.0F};
    state.grounded = true;

    MoveInput input{};
    input.forward_axis = 1.0F;
    input.sprint = true;

    for (int i = 0; i < 30; ++i)
    {
        RelayCore::physics::update_noclip(state, input, -1.5707963268F, 0.0F, 1.0F / 60.0F, config);
    }

    CHECK_FALSE(state.grounded);
    CHECK(state.position.z < -2.0F);
}

TEST_CASE("step-up climbs small obstacle")
{
    const PlayerMotionConfig config = make_config();
    std::vector<Aabb> world = make_floor_world();
    world.push_back(Aabb{
        .mins = {0.6F, 0.0F, -4.0F},
        .maxs = {1.4F, 0.25F, 4.0F},
    });

    PlayerMotionState state{};
    state.position = {0.0F, 0.0F, 0.0F};
    state.grounded = true;

    MoveInput input{};
    input.forward_axis = 1.0F;

    for (int i = 0; i < 30; ++i)
    {
        RelayCore::physics::update_grounded(state, input, 0.0F, 1.0F / 60.0F, WorldQueryFixture{world}, config);
    }

    CHECK(state.position.x > 0.8F);
    CHECK(state.position.y >= doctest::Approx(0.24F).epsilon(0.06));
}

TEST_CASE("move_and_collide reports successful step-up queries")
{
    RelayCore::physics::CollisionConfig collision = make_config().collision;
    std::vector<Aabb> world = make_floor_world();
    world.push_back(Aabb{
        .mins = {0.6F, 0.0F, -4.0F},
        .maxs = {1.4F, 0.25F, 4.0F},
    });

    const MoveResult result = RelayCore::physics::move_and_collide(
        WorldQueryFixture{world},
        {0.0F, 0.0F, 0.0F},
        {4.0F, 0.0F, 0.0F},
        0.25F,
        collision,
        true
    );

    CHECK(result.attempted_step);
    CHECK(result.used_step);
    CHECK_FALSE(result.hit_x);
    CHECK(result.position.x > 0.8F);
    CHECK(result.position.y >= doctest::Approx(0.24F).epsilon(0.06));
}

TEST_CASE("move_and_collide reports blocked step queries on tall obstacles")
{
    RelayCore::physics::CollisionConfig collision = make_config().collision;
    std::vector<Aabb> world = make_floor_world();
    world.push_back(Aabb{
        .mins = {0.6F, 0.0F, -4.0F},
        .maxs = {1.4F, 0.60F, 4.0F},
    });

    const MoveResult result = RelayCore::physics::move_and_collide(
        WorldQueryFixture{world},
        {0.0F, 0.0F, 0.0F},
        {4.0F, 0.0F, 0.0F},
        0.25F,
        collision,
        true
    );

    CHECK(result.attempted_step);
    CHECK_FALSE(result.used_step);
    CHECK(result.hit_x);
    CHECK(result.position.x < 0.55F);
}

TEST_CASE("move_and_collide blocks diagonal tunneling through a thin wall")
{
    RelayCore::physics::CollisionConfig collision = make_config().collision;
    std::vector<Aabb> world = make_floor_world();
    world.push_back(Aabb{
        .mins = {0.55F, 0.0F, 0.15F},
        .maxs = {0.67F, 2.0F, 1.20F},
    });

    const MoveResult result = RelayCore::physics::move_and_collide(
        WorldQueryFixture{world},
        {0.0F, 0.0F, 0.0F},
        {4.0F, 0.0F, 4.0F},
        0.25F,
        collision,
        true
    );

    CHECK(result.hit_x);
    CHECK(result.position.x < 0.30F);
    CHECK(result.position.z > 0.20F);
}

TEST_CASE("move_and_collide blocks vertical tunneling through a thin floor")
{
    const RelayCore::physics::CollisionConfig collision = make_config().collision;
    const std::vector<Aabb> world{
        Aabb{
            .mins = {-2.0F, 0.0F, -2.0F},
            .maxs = {2.0F, 0.05F, 2.0F},
        },
    };

    const MoveResult result = RelayCore::physics::move_and_collide(
        WorldQueryFixture{world},
        {0.0F, 2.0F, 0.0F},
        {0.0F, -20.0F, 0.0F},
        0.25F,
        collision,
        false
    );

    CHECK(result.hit_y);
    CHECK(result.grounded);
    CHECK(result.position.y == doctest::Approx(0.051F).epsilon(0.01));
}

TEST_CASE("move_and_collide blocks vertical tunneling through a thin ceiling")
{
    const RelayCore::physics::CollisionConfig collision = make_config().collision;
    const std::vector<Aabb> world{
        Aabb{
            .mins = {-2.0F, 3.0F, -2.0F},
            .maxs = {2.0F, 3.05F, 2.0F},
        },
    };

    const MoveResult result = RelayCore::physics::move_and_collide(
        WorldQueryFixture{world},
        {0.0F, 0.0F, 0.0F},
        {0.0F, 20.0F, 0.0F},
        0.25F,
        collision,
        false
    );

    CHECK(result.hit_y);
    CHECK_FALSE(result.grounded);
    CHECK(result.position.y == doctest::Approx(1.199F).epsilon(0.01));
}

TEST_CASE("grounded update diagnostics report probe and snap when landing")
{
    const PlayerMotionConfig config = make_config();
    const std::vector<Aabb> world = make_floor_world();

    PlayerMotionState state{};
    state.position = {0.0F, 0.05F, 0.0F};
    state.velocity = {0.0F, -0.1F, 0.0F};
    state.grounded = false;
    GroundedMoveDiagnostics diagnostics{};

    RelayCore::physics::update_grounded(
        state,
        MoveInput{},
        0.0F,
        1.0F / 60.0F,
        WorldQueryFixture{world},
        config,
        &diagnostics
    );

    CHECK(diagnostics.ground_probe_hit);
    CHECK(diagnostics.ground_snap_applied);
    CHECK_FALSE(diagnostics.started_grounded);
    CHECK(diagnostics.ended_grounded);
    CHECK(state.grounded);
    CHECK(state.position.y == doctest::Approx(0.0F).epsilon(0.01));
}

TEST_CASE("obstacle higher than step height blocks movement")
{
    const PlayerMotionConfig config = make_config();
    std::vector<Aabb> world = make_floor_world();
    world.push_back(Aabb{
        .mins = {0.6F, 0.0F, -4.0F},
        .maxs = {1.4F, 0.60F, 4.0F},
    });

    PlayerMotionState state{};
    state.position = {0.0F, 0.0F, 0.0F};
    state.grounded = true;

    MoveInput input{};
    input.forward_axis = 1.0F;

    for (int i = 0; i < 120; ++i)
    {
        RelayCore::physics::update_grounded(state, input, 0.0F, 1.0F / 60.0F, WorldQueryFixture{world}, config);
    }

    CHECK(state.position.x < 0.55F);
    CHECK(state.position.y == doctest::Approx(0.0F).epsilon(0.05));
}
