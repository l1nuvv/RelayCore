#include "relaycore/core/profiling.h"
#include "relaygame/fixed_simulation_loop.h"

#include <doctest/doctest.h>

TEST_CASE("fixed simulation loop exposes step budget, backlog and tick progress")
{
    RelayGame::FixedSimulationLoop loop{1.0 / 60.0, 4};

    CHECK(loop.step_seconds() == doctest::Approx(1.0 / 60.0));
    CHECK(loop.max_steps_per_frame() == 4);
    CHECK(loop.pending_seconds() == doctest::Approx(0.0));
    CHECK(loop.tick_index() == 0);
    CHECK(loop.last_steps_executed() == 0);

    std::uint32_t callback_count = 0;
    const RelayCore::FrameInfo hitch_frame{
        .delta_seconds = 0.20,
        .elapsed_seconds = 0.20,
        .frame_index = 1,
    };
    const std::uint32_t hitch_steps = loop.advance(hitch_frame, [&callback_count](const RelayCore::FrameInfo&) {
        ++callback_count;
    });

    CHECK(hitch_steps == 4);
    CHECK(callback_count == 4);
    CHECK(loop.last_steps_executed() == 4);
    CHECK(loop.pending_seconds() == doctest::Approx(0.0));
    CHECK(loop.tick_index() == 4);

    const RelayCore::FrameInfo partial_frame{
        .delta_seconds = 0.025,
        .elapsed_seconds = 0.225,
        .frame_index = 2,
    };
    const std::uint32_t partial_steps = loop.advance(partial_frame, [&callback_count](const RelayCore::FrameInfo&) {
        ++callback_count;
    });

    CHECK(partial_steps == 1);
    CHECK(callback_count == 5);
    CHECK(loop.last_steps_executed() == 1);
    CHECK(loop.pending_seconds() == doctest::Approx(0.025 - (1.0 / 60.0)).epsilon(0.0001));
    CHECK(loop.tick_index() == 5);
}
