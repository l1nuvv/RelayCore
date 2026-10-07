#include "relaygame/level_editor_document.h"

#include <doctest/doctest.h>

#include "cooling_loop_route.h"
#include "scoped_temp_directory.h"
#include "scoped_test_logger.h"

TEST_CASE("cooling loop service and firing lanes reach isolation startup and exit")
{
    RelayCore::test::ScopedTempDirectory temp;
    RelayCore::test::ScopedTestLogger logger{temp.path() / "logs"};
    RelayCore::test::CoolingLoopRoute service{{}}, firing{{}};
    for (auto* route : {&service, &firing})
    {
        REQUIRE(route->session.authoring_warnings.empty());
        route->reach_isolator(route == &service);
        route->isolate();
        CHECK(route->mission.status().current_step == 2);
        route->reach_control();
        CHECK(route->use("cooling_control") == RelayCore::gameplay::UseInteractionOutcome::ActivatedTarget);
        route->leave();
        CHECK(route->mission.completed());
        CHECK(route->player.deaths == 0);
        CHECK(route->session.ending.has_value());
    }
    CHECK(service.shotgun_shots > 0);
    CHECK(firing.shotgun_shots == 0);
    CHECK(firing.precise_shots > 0);
    CHECK(firing.exposed_ticks > service.exposed_ticks);
}

TEST_CASE("cooling loop gates startup and exit and preserves disk continue and local retry")
{
    RelayCore::test::ScopedTempDirectory temp;
    RelayCore::test::ScopedTestLogger logger{temp.path() / "logs"};
    RelayCore::test::CoolingLoopRoute route{{}};
    const auto control = [&]() -> auto& {
        const auto found =
            std::ranges::find(route.session.usables, "cooling_control", &RelayCore::gameplay::RuntimeUsable::name);
        REQUIRE(found != route.session.usables.end());
        return *found;
    };
    CHECK(
        RelayGame::apply_usable_interaction(
            control(),
            route.session.doors,
            route.session.usables,
            route.inventory,
            route.activated,
            route.signaled
        )
            .kind
        == RelayCore::gameplay::UseInteractionOutcome::Locked
    );
    CHECK(route.activated.empty());
    const auto gate_hit = route.world().raycast({0, 1.65F, 19}, {0, 0, 1}, 2);
    REQUIRE(gate_hit);
    CHECK(route.world().raycast({4, 1.65F, 19}, {0, 0, 1}, 2).has_value());
    route.reach_isolator(true);
    route.isolate();
    CHECK_FALSE(route.world().raycast({0, 1.65F, 19}, {0, 0, 1}, 2));
    const auto verify_continue = [&] {
        const auto saved = route.save();
        const auto file = temp.path() / "cooling-progress.toml";
        RelayGame::write_saved_game(file, saved);
        RelayGame::GameWorldRuntime runtime;
        runtime.load_level_session({}, RelayGame::LevelSessionBuilder{{}}.build(route.session.level_path));
        runtime.restore_saved_game(RelayGame::load_saved_game(file));
        CHECK(runtime.debug_player_position() == route.position);
        CHECK(runtime.inventory_item_ids() == route.inventory);
        CHECK(runtime.activated_target_ids() == route.activated);
        CHECK(runtime.mission_status().current_step == route.mission.status().current_step);
        const auto encounter = runtime.capture_encounter_snapshot(0, 0, false);
        runtime.restore_encounter_snapshot(encounter);
        CHECK(runtime.debug_player_position() == route.position);
        auto dead = runtime.capture_saved_game(0, false);
        dead.world.player_combat.alive = false;
        dead.world.player_combat.health = 0;
        dead.world.player_combat.respawn_remaining_seconds = 1;
        runtime.restore_saved_game(dead);
        REQUIRE(runtime.debug_advance_player_respawn(2));
        CHECK(runtime.debug_player_position() == glm::vec3{-4, .01F, 18.8F});
        CHECK(runtime.activated_target_ids() == route.activated);
        CHECK(runtime.mission_status().current_step == route.mission.status().current_step);
    };
    verify_continue();
    route.reach_control();
    route.walk({3, .01F, 24.5F});
    CHECK(route.use("cooling_exit_reader") == RelayCore::gameplay::UseInteractionOutcome::Locked);
    route.walk({5, .01F, 23});
    CHECK(route.use("cooling_control") == RelayCore::gameplay::UseInteractionOutcome::ActivatedTarget);
    verify_continue();
    route.leave();
    CHECK(route.mission.completed());
}

TEST_CASE("cooling loop isolation stops the leak and startup switches ventilation and exit light")
{
    RelayCore::test::ScopedTempDirectory temp;
    RelayCore::test::ScopedTestLogger logger{temp.path() / "logs"};
    RelayCore::test::CoolingLoopRoute route{{}};
    const auto effect = [&](RelayGame::EnvironmentKind kind) -> auto& {
        const auto found = std::ranges::find(route.session.environment, kind, &RelayGame::EnvironmentEffect::kind);
        REQUIRE(found != route.session.environment.end());
        return *found;
    };
    const auto update = [&] {
        for (int i = 0; i < 600; ++i)
            RelayGame::update_world_environment(route.session.environment, route.activated, 1.0F / 120);
    };
    const auto listener = RelayCore::physics::make_player_aabb({0, .01F, 23}, .3F, 1.8F);
    const auto zone = [&] {
        return RelayGame::find_active_audio_zone(route.session.audio_zones, listener, -1, route.activated);
    };
    update();
    CHECK(effect(RelayGame::EnvironmentKind::Steam).strength > .9F);
    CHECK(effect(RelayGame::EnvironmentKind::Fan).strength == 0);
    REQUIRE(zone() >= 0);
    CHECK(route.session.audio_zones[static_cast<std::size_t>(zone())].name == "cooling_standby");
    route.reach_isolator(true);
    route.isolate();
    update();
    CHECK_FALSE(effect(RelayGame::EnvironmentKind::Steam).active);
    CHECK(effect(RelayGame::EnvironmentKind::Steam).strength < .001F);
    route.reach_control();
    static_cast<void>(route.use("cooling_control"));
    update();
    CHECK(effect(RelayGame::EnvironmentKind::Fan).strength > .9F);
    REQUIRE(zone() >= 0);
    CHECK(route.session.audio_zones[static_cast<std::size_t>(zone())].name == "cooling_ventilation");
    CHECK(std::ranges::any_of(route.session.environment, [](const auto& e) {
        return e.kind == RelayGame::EnvironmentKind::Lamp && e.required_target == "cooling_running" && e.strength == 1;
    }));
}

TEST_CASE("cooling loop editor round trip preserves mission retry and save identity")
{
    RelayCore::test::ScopedTempDirectory temp;
    RelayCore::test::ScopedTestLogger logger{temp.path() / "logs"};
    const RelayGame::LevelSessionBuilder builder{{}};
    const auto original = builder.build(RelayCore::core::asset_path("levels/cooling_loop.toml"));
    RelayGame::LevelEditorDocument document;
    document.reset(original.level);
    const auto file = temp.path() / "cooling_loop.toml";
    document.save(file);
    const auto saved = builder.build(file);
    CHECK(saved.authoring_warnings.empty());
    CHECK(RelayGame::fingerprint_level(saved.level) == RelayGame::fingerprint_level(original.level));
    REQUIRE(saved.retry_spawns.size() == 1);
    CHECK(saved.retry_spawns[0].position == glm::vec3{-4, .01F, 18.8F});
}
