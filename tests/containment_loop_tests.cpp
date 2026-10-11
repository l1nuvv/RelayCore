#include "relaygame/game_world_runtime.h"
#include "relaygame/level_editor_document.h"

#include <doctest/doctest.h>
#include <limits>

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
            route.walk({-24, .01F, 14});
            route.walk({-24, .01F, 17.6F});
            CHECK(route.use("containment_shotgun") == RelayCore::gameplay::UseInteractionOutcome::PickedUpItem);
            route.walk({-24, .01F, 14});
            route.walk({-16, .01F, 14});
            route.archive();
            CHECK(route.use("limiter_cassette") == RelayCore::gameplay::UseInteractionOutcome::PickedUpItem);
            route.west_stairs_and_bridge();
            CHECK(route.use("grip_service") == RelayCore::gameplay::UseInteractionOutcome::ActivatedTarget);
            route.east_stairs(false);
        }
        route.walk({-8, .01F, 29});
        route.walk({-7, .01F, 29});
        CHECK(route.use("limiter_socket") == RelayCore::gameplay::UseInteractionOutcome::ActivatedTarget);
        CHECK(route.session.doors.back().state == RelayCore::gameplay::DoorState::Closed);
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
        route.lure_and_capture();
        CHECK(route.player.deaths == 0);
        route.walk({-7, .01F, 39});
        route.walk({0, .01F, 39});
        route.walk({0, .01F, 49});
        CHECK(route.mission.completed());
        CHECK(route.session.exits.empty());
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
    CHECK(restored.exits.empty());
    CHECK(restored.retry_spawns.size() == 1);
    CHECK(restored.spawn_position == route.session.spawn_position);
    REQUIRE(restored.enemies.size() == 4);
    CHECK(restored.enemies[0].route_points == route.session.enemies[0].route_points);
    CHECK(restored.enemies[2].ranged_attack);
    CHECK(restored.enemies[2].route_points == route.session.enemies[2].route_points);
}

namespace
{
    auto containment_runtime() -> RelayGame::GameWorldRuntime
    {
        RelayGame::GameWorldRuntime runtime;
        runtime.load_level_session(
            {},
            RelayGame::LevelSessionBuilder{{}}.build(RelayCore::core::asset_path("levels/containment_loop.toml"))
        );
        return runtime;
    }

    auto use(RelayGame::GameWorldRuntime& runtime, std::string_view name) -> RelayCore::gameplay::UseInteractionOutcome
    {
        const auto found = std::ranges::find(runtime.usables(), name, &RelayCore::gameplay::RuntimeUsable::name);
        if (found == runtime.usables().end())
            throw std::runtime_error("Missing containment usable");
        return runtime.debug_apply_usable_interaction(static_cast<std::size_t>(found - runtime.usables().begin())).kind;
    }

    void prepare(RelayGame::GameWorldRuntime& runtime)
    {
        REQUIRE(use(runtime, "limiter_cassette") == RelayCore::gameplay::UseInteractionOutcome::PickedUpItem);
        REQUIRE(use(runtime, "limiter_socket") == RelayCore::gameplay::UseInteractionOutcome::ActivatedTarget);
        REQUIRE(use(runtime, "grip_service") == RelayCore::gameplay::UseInteractionOutcome::ActivatedTarget);
        for (int i = 0; i < 361; ++i)
            runtime.debug_update_containment(1.0F / 120);
    }
} // namespace

TEST_CASE("containment mobile shooter warning position and recovery survive disk continue and retry")
{
    RelayCore::test::ScopedTempDirectory temp;
    RelayCore::test::ScopedTestLogger logger{temp.path() / "logs"};
    auto runtime = containment_runtime();
    REQUIRE(runtime.enemies().size() == 4);
    for (const auto mode :
         {RelayGame::EnemyMode::Chasing, RelayGame::EnemyMode::Windup, RelayGame::EnemyMode::Recovery})
    {
        auto saved = runtime.capture_saved_game(12, true);
        saved.enemies[2].position = {-17.5F, .01F, 51.5F};
        saved.enemies[2].last_seen = {-26, .01F, 51};
        saved.enemies[2].mode = mode;
        saved.enemies[2].timer = .35F;
        RelayGame::write_saved_game(temp.path() / "progress.toml", saved);
        runtime.restore_saved_game(RelayGame::load_saved_game(temp.path() / "progress.toml"));
        CHECK(runtime.enemies()[2].state.position == saved.enemies[2].position);
        CHECK(runtime.enemies()[2].state.last_seen == saved.enemies[2].last_seen);
        CHECK(runtime.enemies()[2].state.mode == mode);
        CHECK(runtime.enemies()[2].state.timer == doctest::Approx(.35F));
        CHECK(runtime.enemies()[2].route_count == 0);
        saved = runtime.capture_saved_game(12, true);
        saved.world.player_combat.health = 0;
        saved.world.player_combat.alive = false;
        saved.world.player_combat.respawn_remaining_seconds = 0;
        runtime.restore_saved_game(saved);
        REQUIRE(runtime.debug_advance_player_respawn(0));
        CHECK(runtime.enemies()[2].state.mode == mode);
        CHECK(runtime.enemies()[2].state.timer == doctest::Approx(.35F));
    }
}

TEST_CASE("containment archive crossfire drives relocation while entrance cannot clear the hidden shooters")
{
    RelayCore::test::ScopedTempDirectory temp;
    RelayCore::test::ScopedTestLogger logger{temp.path() / "logs"};
    RelayCore::test::ContainmentLoopRoute route;
    REQUIRE(route.session.enemies.size() == 4);
    const auto world = route.world();
    for (const auto& enemy : route.session.enemies)
        if (enemy.ranged_attack)
        {
            const auto origin = glm::vec3{-16, 1.66F, 14};
            const auto delta = enemy.state.position + glm::vec3{0, .6F, 0} - origin;
            CHECK(world.raycast(origin, glm::normalize(delta), glm::length(delta)).has_value());
        }
    // Reach the encounter through the same movement/focus path as the chapter, then withhold fire.
    route.observe_and_enter_service();
    route.fight = false;
    const auto rover_before = route.session.enemies[1].state.position;
    const auto before = route.session.enemies[2].state.position;
    route.walk({-15, .01F, 24});
    route.walk({-15, .01F, 28});
    route.walk({-26, .01F, 28});
    route.walk({-26, .01F, 42});
    route.walk({-26, .01F, 51});
    for (int i = 0; i < 240; ++i)
        route.tick();
    CHECK(route.enemy_shots > 0);
    CHECK(glm::distance(before, route.session.enemies[2].state.position) > .5F);
    CHECK(glm::distance(rover_before, route.session.enemies[1].state.position) > 1);
    CHECK(route.player.health < 100);
    route.fight = true;
    route.walk({-26, .01F, 48});
    CHECK(route.player.alive);
}

TEST_CASE("containment cassette can be installed before repairing the drive")
{
    RelayCore::test::ScopedTempDirectory temp;
    RelayCore::test::ScopedTestLogger logger{temp.path() / "logs"};
    RelayCore::test::ContainmentLoopRoute route;
    route.observe_and_enter_service();
    route.archive();
    REQUIRE(route.use("limiter_cassette") == RelayCore::gameplay::UseInteractionOutcome::PickedUpItem);
    route.walk({-26, .01F, 28});
    route.walk({-15, .01F, 28});
    route.walk({-15, .01F, 20});
    route.walk({-8, .01F, 20});
    route.walk({-7, .01F, 29});
    CHECK(route.use("limiter_socket") == RelayCore::gameplay::UseInteractionOutcome::ActivatedTarget);
    CHECK(route.session.containment->state.grip_fraction == 0);
    route.east_stairs(true);
    CHECK(route.use("grip_service") == RelayCore::gameplay::UseInteractionOutcome::ActivatedTarget);
    route.east_stairs(false);
    route.lure_and_capture();
}

TEST_CASE("containment rejects early and out of sector activation and missed windows remain repeatable")
{
    RelayCore::test::ScopedTempDirectory temp;
    RelayCore::test::ScopedTestLogger logger{temp.path() / "logs"};
    auto runtime = containment_runtime();
    CHECK(use(runtime, "containment_fire") == RelayCore::gameplay::UseInteractionOutcome::Locked);
    CHECK(use(runtime, "limiter_socket") == RelayCore::gameplay::UseInteractionOutcome::MissingRequiredItem);
    prepare(runtime);
    auto save = runtime.capture_saved_game(0, false);
    save.world.containment->mode = RelayGame::GuardianMode::Recovery;
    save.world.containment->timer = 2;
    runtime.restore_saved_game(save);
    CHECK(use(runtime, "containment_fire") == RelayCore::gameplay::UseInteractionOutcome::Locked);
    save.world.containment->position = {0, .241F, 28};
    runtime.restore_saved_game(save);
    runtime.debug_set_player_position({-7, .01F, 26});
    for (int i = 0; i < 241; ++i)
        runtime.debug_update_containment(1.0F / 120);
    CHECK(use(runtime, "containment_fire") == RelayCore::gameplay::UseInteractionOutcome::Locked);
    CHECK(
        std::ranges::find(runtime.activated_target_ids(), "containment_engaged") == runtime.activated_target_ids().end()
    );
    runtime.restore_saved_game(save);
    CHECK(use(runtime, "containment_fire") == RelayCore::gameplay::UseInteractionOutcome::ActivatedTarget);
    runtime.debug_update_containment(.01F);
    runtime.debug_update_containment(1);
    CHECK(runtime.containment()->state.mode == RelayGame::GuardianMode::Capturing);
    CHECK(runtime.doors().back().state == RelayCore::gameplay::DoorState::Closed);
    runtime.debug_update_containment(1);
    CHECK(runtime.containment()->state.mode == RelayGame::GuardianMode::Contained);
    CHECK(runtime.doors().back().state == RelayCore::gameplay::DoorState::Opening);
}

TEST_CASE("containment disk and encounter snapshots restore every moving and timed state atomically")
{
    RelayCore::test::ScopedTempDirectory temp;
    RelayCore::test::ScopedTestLogger logger{temp.path() / "logs"};
    auto runtime = containment_runtime();
    prepare(runtime);
    const auto prepared = runtime.capture_saved_game(5, true);
    for (const auto mode :
         {RelayGame::GuardianMode::Watching,
          RelayGame::GuardianMode::Pursuing,
          RelayGame::GuardianMode::Windup,
          RelayGame::GuardianMode::Recovery,
          RelayGame::GuardianMode::Suppressed,
          RelayGame::GuardianMode::Capturing,
          RelayGame::GuardianMode::Contained})
    {
        CAPTURE(mode);
        auto save = prepared;
        auto& state = *save.world.containment;
        state.mode = mode;
        state.position = {0, .241F, 28};
        state.strike_target = {0, 1, 25};
        state.timer =
            mode == RelayGame::GuardianMode::Recovery
                ? 2.3F
                : (mode == RelayGame::GuardianMode::Windup || mode == RelayGame::GuardianMode::Suppressed ? .3F : 0);
        if (mode == RelayGame::GuardianMode::Capturing || mode == RelayGame::GuardianMode::Contained)
        {
            save.world.activated_target_ids.push_back("containment_engaged");
            state.capture_fraction = mode == RelayGame::GuardianMode::Contained ? 1 : .4F;
            if (mode == RelayGame::GuardianMode::Contained)
            {
                save.world.activated_target_ids.push_back("guardian_contained");
                save.world.doors.back().state = RelayCore::gameplay::DoorState::Opening;
            }
            const auto fire =
                std::ranges::find(runtime.usables(), "containment_fire", &RelayCore::gameplay::RuntimeUsable::name);
            const auto id =
                runtime.runtime_registry().find_usable(static_cast<std::size_t>(fire - runtime.usables().begin()))->id;
            for (auto& usable : save.world.usables)
                if (id == usable.runtime_id)
                    usable.consumed = true;
        }
        RelayGame::write_saved_game(temp.path() / "progress.toml", save);
        runtime.restore_saved_game(RelayGame::load_saved_game(temp.path() / "progress.toml"));
        CHECK(runtime.containment()->state.timer == doctest::Approx(state.timer));
        CHECK(runtime.containment()->state.capture_fraction == doctest::Approx(state.capture_fraction));
        CHECK(runtime.containment()->state.position == state.position);
        const auto snapshot = runtime.capture_encounter_snapshot(5, 2, true);
        runtime.debug_update_containment(.1F);
        runtime.restore_encounter_snapshot(snapshot);
        CHECK(runtime.containment()->state.mode == mode);
        CHECK(runtime.containment()->state.timer == doctest::Approx(state.timer));
        auto invalid = runtime.capture_runtime_snapshot();
        invalid.containment->position.x = std::numeric_limits<float>::quiet_NaN();
        CHECK_FALSE(runtime.restore_runtime_snapshot(invalid).restored);
        CHECK(runtime.containment()->state.position == state.position);
        invalid = runtime.capture_runtime_snapshot();
        invalid.containment.reset();
        CHECK_FALSE(runtime.restore_runtime_snapshot(invalid).restored);
    }
    auto moving = prepared;
    moving.world.containment->grip_fraction = .45F;
    runtime.restore_saved_game(moving);
    RelayGame::write_saved_game(temp.path() / "progress.toml", runtime.capture_saved_game(5, false));
    runtime.debug_update_containment(1);
    runtime.restore_saved_game(RelayGame::load_saved_game(temp.path() / "progress.toml"));
    CHECK(runtime.containment()->state.grip_fraction == doctest::Approx(.45F));
}

TEST_CASE("containment bullets suppress but never kill and walls prevent strikes")
{
    RelayCore::test::ScopedTempDirectory temp;
    RelayCore::test::ScopedTestLogger logger{temp.path() / "logs"};
    auto runtime = containment_runtime();
    const auto hit = runtime.debug_apply_player_fire({0, 1.65F, 20}, {0, 0, 1});
    REQUIRE(hit.world_impact);
    CHECK(hit.world_impact->object.authored_entity_index == runtime.containment()->entity_index);
    CHECK(runtime.containment()->state.mode == RelayGame::GuardianMode::Suppressed);
    for (int i = 0; i < 60; ++i)
    {
        runtime.debug_tick_combat(1);
        runtime.debug_update_containment(1);
        CHECK(runtime.debug_apply_player_fire({0, 1.65F, 20}, {0, 0, 1}).fired);
    }
    CHECK(runtime.player_combat_state().kills == 0);
    CHECK(runtime.containment()->state.mode != RelayGame::GuardianMode::Contained);
    CHECK(runtime.doors().back().state == RelayCore::gameplay::DoorState::Closed);
    auto save = runtime.capture_saved_game(0, false);
    save.world.containment->mode = RelayGame::GuardianMode::Windup;
    save.world.containment->position = {-10, .01F, 28};
    CHECK_THROWS(runtime.restore_saved_game(save));
    save.world.containment->position = {0, .01F, 34};
    save.world.containment->strike_target = {0, 1, 9};
    save.world.containment->timer = .1F;
    runtime.restore_saved_game(save);
    runtime.debug_update_containment(.2F);
    CHECK(runtime.player_combat_state().health == 100);

    auto encounter = *runtime.containment();
    encounter.state = {
        .position = {0, .01F, 34},
        .strike_target = {0, 1, 31.5F},
        .timer = .1F,
        .mode = RelayGame::GuardianMode::Windup
    };
    RelayCore::gameplay::PlayerCombatState player;
    RelayCore::gameplay::reset_player_combat_state(player);
    const auto bounds = RelayCore::physics::make_player_aabb({0, .01F, 31.5F}, .3F, 1.8F);
    const auto wall = RelayCore::physics::WorldCollider{{{-2, 0, 32}, {2, 4, 32.2F}}};
    std::vector<std::string> activated;
    std::vector<RelayCore::gameplay::RuntimeDoor> doors;
    encounter.update({{&wall, 1}}, bounds, {0, 1.65F, 31.5F}, player, .2F, doors, activated);
    CHECK(player.health == 100);
    encounter.state.mode = RelayGame::GuardianMode::Windup;
    encounter.state.timer = .1F;
    encounter.update({}, bounds, {0, 1.65F, 31.5F}, player, .2F, doors, activated);
    CHECK(player.health == 68);
}

TEST_CASE("containment malformed apparatus definitions fail with actionable errors")
{
    RelayCore::test::ScopedTempDirectory temp;
    RelayCore::test::ScopedTestLogger logger{temp.path() / "logs"};
    RelayCore::test::ContainmentLoopRoute route;
    auto level = route.session.level;
    auto marker = std::ranges::find(level.entities, "containment_apparatus", &RelayCore::EntitySpawnData::classname);
    REQUIRE(marker != level.entities.end());
    marker->properties.erase("trap_origin");
    CHECK_THROWS_WITH_AS(
        static_cast<void>(RelayGame::LevelSessionBuilder{{}}.build(level, "invalid.toml")),
        "containment_apparatus requires one marker, finite trap_origin and positive trap_size wider than the Guardian",
        std::runtime_error
    );
    level = route.session.level;
    auto socket = std::ranges::find_if(level.entities, [](const auto& entity) {
        const auto name = entity.properties.find("targetname");
        return name != entity.properties.end() && name->second == "limiter_socket";
    });
    socket->properties["single_use"] = "false";
    CHECK_THROWS_WITH_AS(
        static_cast<void>(RelayGame::LevelSessionBuilder{{}}.build(level, "invalid.toml")),
        "containment_apparatus requires single-use activate_target binding: limiter_socket",
        std::runtime_error
    );
}

TEST_CASE("containment death retry preserves repairs and resets the unsolved threat")
{
    RelayCore::test::ScopedTempDirectory temp;
    RelayCore::test::ScopedTestLogger logger{temp.path() / "logs"};
    auto runtime = containment_runtime();
    prepare(runtime);
    auto save = runtime.capture_saved_game(0, false);
    save.world.containment->position = {0, .241F, 28};
    save.world.containment->mode = RelayGame::GuardianMode::Windup;
    save.world.containment->timer = .2F;
    save.world.player_combat.health = 0;
    save.world.player_combat.alive = false;
    save.world.player_combat.respawn_remaining_seconds = 0;
    runtime.restore_saved_game(save);
    runtime.debug_update_containment(1);
    CHECK(runtime.containment()->state.timer == doctest::Approx(.2F));
    REQUIRE(runtime.debug_advance_player_respawn(.1F));
    CHECK(runtime.containment()->state.position == runtime.containment()->home);
    CHECK(runtime.containment()->state.mode == RelayGame::GuardianMode::Watching);
    CHECK(runtime.containment()->state.grip_fraction == 1);
    CHECK(use(runtime, "limiter_socket") == RelayCore::gameplay::UseInteractionOutcome::None);
    CHECK(runtime.player_combat_state().alive);
    save = runtime.capture_saved_game(0, false);
    save.world.containment->position = {0, .241F, 28};
    save.world.containment->mode = RelayGame::GuardianMode::Recovery;
    save.world.containment->timer = 2;
    runtime.restore_saved_game(save);
    REQUIRE(use(runtime, "containment_fire") == RelayCore::gameplay::UseInteractionOutcome::ActivatedTarget);
    runtime.debug_update_containment(.5F);
    for (bool completed : {false, true})
    {
        if (completed)
            runtime.debug_update_containment(2);
        save = runtime.capture_saved_game(0, false);
        save.world.player_combat.health = 0;
        save.world.player_combat.alive = false;
        save.world.player_combat.respawn_remaining_seconds = 0;
        runtime.restore_saved_game(save);
        runtime.debug_update_containment(1);
        CHECK(runtime.containment()->state.capture_fraction == save.world.containment->capture_fraction);
        REQUIRE(runtime.debug_advance_player_respawn(.1F));
        CHECK(runtime.containment()->state.mode == save.world.containment->mode);
        CHECK(runtime.containment()->state.position == save.world.containment->position);
        CHECK(runtime.activated_target_ids() == save.world.activated_target_ids);
    }
}
