#include "relaycore/gameplay/interaction_runtime.h"
#include "relaygame/player_feedback.h"
#include "relaygame/player_settings.h"

#include <doctest/doctest.h>
#include <fstream>
#include <limits>

#include "scoped_temp_directory.h"
#include "world_query_fixture.h"

TEST_CASE("footsteps follow traveled ground distance, not airborne or blocked movement")
{
    RelayGame::PlayerFeedback feedback;
    int steps = 0;
    for (int i = 0; i < 120; ++i)
        steps += feedback.update(1.0F / 120, {3.5F / 120, 0, 0}, 0, true, true, false).step;
    CHECK(steps == 2);
    CHECK(feedback.stride() > 0.99F);
    CHECK(feedback.eye_offset(0) == 0);
    for (int i = 0; i < 120; ++i)
        CHECK_FALSE(feedback.update(1.0F / 120, {}, 0, true, true, false).step);
    CHECK(feedback.stride() < 0.001F);
    for (int i = 0; i < 60; ++i)
        CHECK_FALSE(feedback.update(1.0F / 120, {0.05F, -0.04F, 0}, -5, false, false, false).step);
    const auto landing = feedback.update(1.0F / 120, {}, -5, false, true, false);
    CHECK(landing.landed > 0);
    CHECK_FALSE(landing.step);
    CHECK(feedback.eye_offset(1) < 0);
    CHECK(feedback.update(1.0F / 120, {}, 0, true, true, false).landed == 0);
    CHECK_FALSE(feedback.update(1.0F / 120, {8, 0, 0}, 0, true, true, false).step);
    CHECK(feedback.stride() == 0);
    CHECK_FALSE(feedback.update(1.0F / 120, {0.1F, 0, 0}, 0, true, true, true).step);

    feedback.reset();
    steps = 0;
    for (int i = 0; i < 120; ++i)
        steps += feedback.update(1.0F / 120, {10.5F / 120, 0, 0}, 0, true, true, false).step;
    CHECK(steps == 3);
}

TEST_CASE("body travel direction follows ground displacement and survives stopping")
{
    RelayGame::PlayerFeedback feedback;
    (void) feedback.update(.1F, {-.3F, 0, -.4F}, 0, true, true, false);
    CHECK(feedback.travel_direction().x == doctest::Approx(-.6F));
    CHECK(feedback.travel_direction().z == doctest::Approx(-.8F));
    (void) feedback.update(.1F, {}, 0, true, true, false);
    CHECK(feedback.travel_direction().x == doctest::Approx(-.6F));
    (void) feedback.update(.1F, {.3F, .2F, 0}, 2, false, false, false);
    CHECK(feedback.travel_direction().z == doctest::Approx(-.8F));
    feedback.reset();
    CHECK(feedback.stride() == 0);
    CHECK(feedback.travel_direction() == glm::vec3{0, 0, 1});
}

TEST_CASE("footstep material comes from the support directly below the feet")
{
    RelayCore::BlockoutLevel level;
    level.solids.push_back({{-5, -1, -5}, {5, 0, 5}, RelayCore::SurfaceMaterial::Concrete});
    level.solids.push_back({{0, 0, 0}, {2, 0.1F, 2}, RelayCore::SurfaceMaterial::Metal});
    level.solids.push_back({{-2, 0, -2}, {0, 0.1F, 0}, RelayCore::SurfaceMaterial::Tile});
    CHECK(RelayGame::footstep_surface(level, {1, 0.1F, 1}) == RelayGame::FootstepSurface::Metal);
    CHECK(RelayGame::footstep_surface(level, {-1, 0.1F, -1}) == RelayGame::FootstepSurface::Tile);
    CHECK(RelayGame::footstep_surface(level, {3, 0, 3}) == RelayGame::FootstepSurface::Concrete);
}

TEST_CASE("player settings persist edits atomically and reject invalid authored values")
{
    RelayCore::test::ScopedTempDirectory directory;
    const auto path = directory.path() / "settings.toml";
    RelayGame::PlayerSettings settings;
    CHECK(RelayGame::load_player_settings(path, settings).fov == settings.fov);
    settings.fov = 85;
    settings.camera_motion = 0;
    settings.volume = 0.25F;
    settings.vsync = false;
    settings.hdr_enabled = true;
    RelayGame::save_player_settings(path, settings);
    auto loaded = RelayGame::load_player_settings(path, {});
    CHECK(loaded.fov == 85);
    CHECK(loaded.camera_motion == 0);
    CHECK(loaded.volume == 0.25F);
    CHECK_FALSE(loaded.vsync);
    CHECK(loaded.hdr_enabled);
    settings.fov = 63;
    settings.hdr_enabled = false;
    RelayGame::save_player_settings(path, settings);
    CHECK(RelayGame::load_player_settings(path, {}).fov == 63);
    CHECK_FALSE(RelayGame::load_player_settings(path, {}).hdr_enabled);
    settings.volume = std::numeric_limits<float>::quiet_NaN();
    CHECK_THROWS(RelayGame::save_player_settings(path, settings));
    CHECK(RelayGame::load_player_settings(path, {}).fov == 63);
    {
        std::ofstream output{path};
        output << "fov = 300\n";
    }
    CHECK_THROWS(static_cast<void>(RelayGame::load_player_settings(path, {})));
    {
        std::ofstream output{path};
        output << "vsync = 'yes'\n";
    }
    CHECK_THROWS(static_cast<void>(RelayGame::load_player_settings(path, {})));
    {
        std::ofstream output{path};
        output << "hdr_enabled = 'HDR'\n";
    }
    CHECK_THROWS_WITH(
        static_cast<void>(RelayGame::load_player_settings(path, {})),
        "player_settings.hdr_enabled must be a boolean"
    );
    {
        std::ofstream output{path};
        output << "vsync = true\n";
    }
    settings.hdr_enabled = true;
    settings.volume = 0.25F;
    CHECK(RelayGame::load_player_settings(path, settings).hdr_enabled);
    CHECK_FALSE(RelayGame::load_player_settings(path, {}).hdr_enabled);
}

TEST_CASE("interaction respects occlusion while allowing a panel on a wall face")
{
    using namespace RelayCore;
    std::vector<gameplay::RuntimeUsable> usables{{.origin = {0, 1, -2}, .use_radius = 3}};
    std::vector<physics::Aabb> blockers{{{-1, 0, -1.1F}, {1, 2, -1}}};
    const auto query = [&] {
        return gameplay::find_focused_usable(
            {0, 1, 0},
            {0, 0, -1},
            usables,
            3,
            0.75F,
            RelayCore::test::WorldQueryFixture{blockers}
        );
    };
    CHECK(query().usable_index == -1);
    blockers[0] = {{-1, 0, -2.2F}, {1, 2, -2}};
    CHECK(query().usable_index == 0);
    blockers.clear();
    CHECK(query().usable_index == 0);
}
