#include "relaygame/level_editor_document.h"

#include <doctest/doctest.h>

#include "containment_loop_route.h"
#include "scoped_temp_directory.h"
#include "scoped_test_logger.h"

TEST_CASE("containment graybox branches stairs shortcut and final passage are walkable")
{
    RelayCore::test::ScopedTempDirectory temp;
    RelayCore::test::ScopedTestLogger logger{temp.path() / "logs"};
    for (bool equipment_first : {false, true})
    {
        CAPTURE(equipment_first);
        RelayCore::test::ContainmentLoopRoute route;
        REQUIRE(route.session.authoring_warnings.empty());
        route.observe_and_enter_service();
        CHECK(route.mission.status().current_step == 2);
        if (equipment_first)
        {
            route.east_stairs(true);
            CHECK(route.use("grip_service") == RelayCore::gameplay::UseInteractionOutcome::ActivatedTarget);
            route.east_stairs(false);
            route.east_archive_return();
            CHECK(route.use("limiter_cassette") == RelayCore::gameplay::UseInteractionOutcome::PickedUpItem);
            route.west_stairs_and_bridge();
            route.east_stairs(false);
        } else
        {
            route.archive();
            CHECK(route.use("limiter_cassette") == RelayCore::gameplay::UseInteractionOutcome::PickedUpItem);
            route.west_stairs_and_bridge();
            CHECK(route.use("grip_service") == RelayCore::gameplay::UseInteractionOutcome::ActivatedTarget);
            route.east_stairs(false);
        }
        route.walk({-8, .01F, 29});
        route.walk({-7, .01F, 29});
        CHECK(route.use("limiter_socket") == RelayCore::gameplay::UseInteractionOutcome::ActivatedTarget);
        route.walk({-8, .01F, 36});
        CHECK(route.world().raycast({-10, 1.65F, 36}, {-1, 0, 0}, 4).has_value());
        route.walk({-8, .01F, 20});
        route.walk({-15, .01F, 20});
        route.walk({-15, .01F, 28});
        route.walk({-14, .01F, 28});
        route.walk({-14, .01F, 36});
        route.walk({-13.1F, .01F, 36.3F});
        CHECK(route.use("shortcut_release") == RelayCore::gameplay::UseInteractionOutcome::ActivatedTarget);
        route.walk({-8, .01F, 36});
        route.walk({0, .01F, 39});
        route.walk({0, .01F, 49});
        CHECK(route.mission.completed());
        CHECK(route.session.exits.size() == 1);
        CHECK(route.session.exits.front().destination.filename() == "occult_lab_b17.toml");
    }
}

TEST_CASE("containment graybox editor round trip preserves elevated entries and authored gates")
{
    RelayCore::test::ScopedTempDirectory temp;
    RelayCore::test::ScopedTestLogger logger{temp.path() / "logs"};
    RelayCore::test::ContainmentLoopRoute route;
    RelayGame::LevelEditorDocument document;
    document.reset(route.session.level);
    const auto messages = document.validation_messages();
    CHECK_MESSAGE(messages.empty(), (messages.empty() ? "" : messages.front()));
    const auto path = temp.path() / "containment_loop.toml";
    document.save(path);
    const auto restored = RelayGame::LevelSessionBuilder{{}}.build(path);
    CHECK(restored.authoring_warnings.empty());
    CHECK(restored.level.solids.size() == route.session.level.solids.size());
    CHECK(restored.level.entities.size() == route.session.level.entities.size());
    CHECK(restored.mission_definition.steps.size() == route.session.mission_definition.steps.size());
    CHECK(restored.exits.front().entry == "campaign_arrival");
    CHECK(restored.retry_spawns.size() == 1);
    CHECK(restored.spawn_position == route.session.spawn_position);
}
