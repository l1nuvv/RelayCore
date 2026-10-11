#include "relaycore/core/profiling.h"
#include "relaycore/world/blockout_level.h"
#include "relaycore/world/world_collision.h"
#include "relaygame/player_controller.h"

#include <array>
#include <doctest/doctest.h>

namespace
{
    struct SdlEventsGuard {
        SdlEventsGuard()
        {
            REQUIRE(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS));
        }

        ~SdlEventsGuard()
        {
            SDL_Quit();
        }
    };
} // namespace

TEST_CASE("player controller collision diagnostics narrow static candidates and append dynamic blockers")
{
    SdlEventsGuard sdl_guard{};

    RelayGame::GroundedPlayerController controller{};
    RelayCore::BlockoutLevel level{};
    level.solids.push_back(RelayCore::BlockoutSolid{
        .mins = {-4.0F, -1.0F, -4.0F},
        .maxs = {4.0F, 0.0F, 4.0F},
    });
    level.solids.push_back(RelayCore::BlockoutSolid{
        .mins = {40.0F, 0.0F, -1.0F},
        .maxs = {42.0F, 2.0F, 1.0F},
    });

    const auto colliders = RelayCore::build_static_world_colliders(level);
    controller.set_static_collision_world(colliders);
    controller.initialize({0.0F, 0.0F, 0.0F});

    const std::array dynamic_bounds{
        RelayCore::physics::Aabb{
            .mins = {10.0F, 0.0F, -1.0F},
            .maxs = {11.0F, 2.0F, 1.0F},
        },
    };
    const std::array dynamic_blockers{RelayCore::physics::WorldCollider{
        .bounds = dynamic_bounds[0],
        .object = {RelayCore::physics::WorldObjectKind::Door}
    }};
    const RelayCore::FrameInfo frame{
        .delta_seconds = 1.0 / 60.0,
        .elapsed_seconds = 1.0 / 60.0,
        .frame_index = 1,
    };

    controller.update(frame, dynamic_blockers);

    const auto& diagnostics = controller.collision_query_diagnostics();
    CHECK(diagnostics.static_world_solids == 2);
    CHECK(diagnostics.static_candidate_count == 1);
    CHECK(diagnostics.dynamic_blocker_count == 1);
    CHECK(diagnostics.collision_world_solids == 2);
    CHECK(diagnostics.ground_probe_hit);
    CHECK(controller.grounded());
    CHECK(controller.position().y == doctest::Approx(0.0F).epsilon(0.01));
    CHECK_FALSE(diagnostics.attempted_step_move);
    CHECK_FALSE(diagnostics.used_step_move);
    CHECK_FALSE(diagnostics.hit_x);
    CHECK_FALSE(diagnostics.hit_z);
}
