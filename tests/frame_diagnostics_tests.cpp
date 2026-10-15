#include "relaycore/app/frame_diagnostics.h"
#include "relaycore/app/frame_timer.h"
#include "relaycore/core/log.h"

#include <doctest/doctest.h>
#include <fstream>
#include <iterator>

#include "scoped_temp_directory.h"
#include "scoped_test_logger.h"

namespace
{
    auto read_log() -> std::string
    {
        RelayCore::log::flush();
        std::ifstream input{RelayCore::log::latest_log_path()};
        return {std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{}};
    }
} // namespace

TEST_CASE("frame timer preserves wall time beyond simulation clamp")
{
    using namespace std::chrono_literals;
    const RelayCore::FrameTimer::Clock::time_point start{};
    RelayCore::FrameTimer timer{start};
    const auto hitch = timer.begin_frame(start + 350ms);
    CHECK(hitch.delta_seconds == doctest::Approx(.1));
    CHECK(hitch.wall_delta_seconds == doctest::Approx(.35));
    CHECK(hitch.elapsed_seconds == doctest::Approx(.35));
    const auto next = timer.begin_frame(start + 366ms);
    CHECK(next.delta_seconds == doctest::Approx(.016));
    CHECK(next.wall_delta_seconds == doctest::Approx(.016));
    CHECK(next.frame_index == 1);
}

TEST_CASE("frame summaries include every loop and use nearest rank percentiles")
{
    using namespace std::chrono_literals;
    RelayCore::test::ScopedTempDirectory temp;
    RelayCore::test::ScopedTestLogger logger{temp.path()};
    RelayCore::FrameDiagnostics diagnostics;
    diagnostics.set_context("game", "known-map");
    auto now = RelayCore::FrameDiagnostics::Clock::now();
    for (int milliseconds = 1; milliseconds <= 100; ++milliseconds)
    {
        diagnostics.begin_frame(now);
        now += std::chrono::milliseconds(milliseconds);
        diagnostics.end_frame(true, 100, now);
    }
    diagnostics.flush();
    const auto text = read_log();
    CHECK(text.find("mode=game map='known-map' frames=100") != std::string::npos);
    CHECK(text.find("mean_ms=50.500 p95_ms=95.000 p99_ms=99.000 max_ms=100.000") != std::string::npos);
    diagnostics.flush();
    CHECK(read_log() == text);
}

TEST_CASE("frame summaries separate menu pause loading and boundary loops and flush tails")
{
    using namespace std::chrono_literals;
    RelayCore::test::ScopedTempDirectory temp;
    RelayCore::test::ScopedTestLogger logger{temp.path()};
    RelayCore::FrameDiagnostics diagnostics;
    auto now = RelayCore::FrameDiagnostics::Clock::now();
    const auto sample = [&](int milliseconds) {
        now += std::chrono::milliseconds(milliseconds);
        diagnostics.end_frame(true, .1, now);
    };
    diagnostics.set_context("menu", "A");
    diagnostics.begin_frame(now);
    sample(20);
    diagnostics.begin_frame(now);
    diagnostics.mark_loading();
    diagnostics.set_context("game", "B");
    sample(350);
    diagnostics.begin_frame(now);
    sample(15);
    diagnostics.begin_frame(now);
    diagnostics.set_context("pause", "B");
    sample(5);
    diagnostics.begin_frame(now);
    sample(25);
    diagnostics.flush();
    const auto text = read_log();
    CHECK(text.find("mode=menu map='A' frames=1") != std::string::npos);
    CHECK(text.find("mode=loading map='A' frames=1") != std::string::npos);
    CHECK(text.find("mean_ms=350.000 p95_ms=350.000 p99_ms=350.000 max_ms=350.000") != std::string::npos);
    CHECK(text.find("mode=game map='B' frames=1") != std::string::npos);
    CHECK(text.find("mode=boundary map='B' frames=1") != std::string::npos);
    CHECK(text.find("mode=pause map='B' frames=1") != std::string::npos);
}

TEST_CASE("frame summaries respect interval boundaries disable and suspended presentation")
{
    using namespace std::chrono_literals;
    RelayCore::test::ScopedTempDirectory temp;
    RelayCore::test::ScopedTestLogger logger{temp.path()};
    RelayCore::FrameDiagnostics diagnostics;
    auto now = RelayCore::FrameDiagnostics::Clock::now();
    diagnostics.set_context("game", "A");
    for (int i = 0; i < 6; ++i)
    {
        diagnostics.begin_frame(now);
        now += 20ms;
        diagnostics.end_frame(true, .1, now);
    }
    const auto interval_log = read_log();
    CHECK(interval_log.find("mode=game map='A' frames=5") != std::string::npos);
    CHECK(interval_log.find("frames=6") == std::string::npos);
    diagnostics.begin_frame(now);
    now += 500ms;
    diagnostics.end_frame(false, .1, now);
    CHECK(read_log().find("mode=game map='A' frames=1") != std::string::npos);
    CHECK(read_log().find("500.000") == std::string::npos);
    diagnostics.begin_frame(now);
    diagnostics.mark_suspended();
    now += 20ms;
    diagnostics.end_frame(true, .1, now);
    diagnostics.flush();
    CHECK(read_log().find("mode=suspended map='A' frames=1") != std::string::npos);
}
