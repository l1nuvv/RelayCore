#include "relaygame/level_editor_document.h"
#include "relaygame/static_props.h"

#include <doctest/doctest.h>

#include "bench_relay_route.h"
#include "scoped_temp_directory.h"
#include "scoped_test_logger.h"

TEST_CASE("bench relay native opening passes rover and low rays while the enclosing box blocks them")
{
    RelayCore::test::ScopedTempDirectory temp;
    RelayCore::test::ScopedTestLogger logger{temp.path() / "logs"};
    RelayCore::test::BenchRelayRoute route{{}};
    REQUIRE(route.session.physics_fixture);
    REQUIRE(route.session.authoring_warnings.empty());
    REQUIRE(route.session.enemies.size() == 1);
    REQUIRE(route.session.combat.turrets.size() == 1);
    const auto& world = *route.session.physics_fixture;
    CHECK_FALSE(world.raycast({0, .6F, 5.5F}, {0, 0, 1}, 3));
    CHECK_FALSE(world.cast_box({0, .485F, 5.5F}, {.42F, .475F, .42F}, {0, 0, 3}));
    const auto top = world.raycast({0, 1.1F, 5.5F}, {0, 0, 1}, 3);
    REQUIRE(top);
    CHECK(top->surface == RelayCore::SurfaceMaterial::Tile);
    const auto bounds = world.object_bounds(top->object);
    REQUIRE(bounds);
    RelayCore::physics::WorldCollider enclosing{*bounds};
    RelayCore::physics::PhysicsWorld box{std::span{&enclosing, 1}};
    CHECK(box.raycast({0, .6F, 5.5F}, {0, 0, 1}, 3).has_value());
    CHECK(box.cast_box({0, .485F, 5.5F}, {.42F, .475F, .42F}, {0, 0, 3}).has_value());
}

TEST_CASE("bench relay distance and cover tactics reach relay and exit with different exposure")
{
    RelayCore::test::ScopedTempDirectory temp;
    RelayCore::test::ScopedTestLogger logger{temp.path() / "logs"};
    for (int attempt = 0; attempt < 2; ++attempt)
    {
        RelayCore::test::BenchRelayRoute distance{{}, RelayCore::test::BenchRelayTactic::Distance};
        RelayCore::test::BenchRelayRoute cover{{}, RelayCore::test::BenchRelayTactic::Cover};
        for (auto* route : {&distance, &cover})
        {
            route->enter_bay();
            route->finish();
            CHECK(route->mission.completed());
            CHECK(route->player.kills == 2);
            CHECK(route->player.deaths == 0);
            CHECK(route->precise_shots > 0);
            CHECK(route->session.doors.back().state == RelayCore::gameplay::DoorState::Open);
        }
        CAPTURE(distance.first_rover_hit_range);
        CAPTURE(cover.first_rover_hit_range);
        CAPTURE(distance.turret_exposed_ticks);
        CAPTURE(cover.turret_exposed_ticks);
        CHECK(distance.shotgun_shots == 0);
        CHECK(distance.first_rover_hit_range > 4);
        CHECK(cover.shotgun_shots > 0);
        CHECK(cover.first_rover_hit_range < 4);
        CHECK(cover.closest_rover_z < 6.1F);
        CHECK(cover.turret_exposed_ticks > distance.turret_exposed_ticks);
    }
}

TEST_CASE("bench relay closed gate and standing cover stop turret damage and outgoing fire")
{
    RelayCore::test::ScopedTempDirectory temp;
    RelayCore::test::ScopedTestLogger logger{temp.path() / "logs"};
    RelayCore::test::BenchRelayRoute route{{}};
    const auto check_cover = [&] {
        const auto eye = route.position + glm::vec3{0, 1.65F, 0};
        const auto direction = glm::normalize(route.session.combat.turrets.front().origin - eye);
        const auto health = route.player.health;
        const auto turret_health = route.session.combat.turrets.front().health;
        const auto shot = RelayGame::fire_player_weapon(
            route.world(),
            eye,
            direction,
            route.player,
            route.session.combat.turrets,
            route.session.enemies
        );
        REQUIRE(shot.combat.fired);
        CHECK_FALSE(shot.combat.hit_turret);
        REQUIRE(shot.combat.world_impact);
        for (int i = 0; i < 480; ++i)
            route.tick(false);
        CHECK(route.player.health == health);
        CHECK(route.session.combat.turrets.front().health == turret_health);
    };
    route.position = {0, .01F, -.4F};
    check_cover();
    route.enter_bay();
    route.walk({-2.6F, .01F, 3.1F});
    // Isolate the turret: the rover remains active in the two complete tactical routes above.
    route.session.enemies.front().state.mode = RelayGame::EnemyMode::Dead;
    REQUIRE_FALSE(route.session.combat.turrets.front().destroyed);
    check_cover();
}

TEST_CASE("bench relay editor and disk continue preserve native references encounters and objective state")
{
    RelayCore::test::ScopedTempDirectory temp;
    RelayCore::test::ScopedTestLogger logger{temp.path() / "logs"};
    RelayCore::core::EngineConfig config;
    RelayCore::test::BenchRelayRoute route{config.gameplay};
    RelayGame::LevelEditorDocument editor;
    editor.reset(route.session.level);
    const auto issues = editor.validation_messages(route.session.level_path);
    CHECK_MESSAGE(issues.empty(), (issues.empty() ? "" : issues.front()));
    const auto control = std::ranges::find_if(editor.level().entities, [](const auto& entity) {
        const auto name = entity.properties.find("targetname");
        return name != entity.properties.end() && name->second == "relay_control";
    });
    REQUIRE(control != editor.level().entities.end());
    editor.select(RelayGame::EditorSelection{true, std::size_t(control - editor.level().entities.begin())});
    editor.set_property("chain_target", "");
    CHECK(std::ranges::any_of(editor.validation_messages(route.session.level_path), [](const auto& message) {
        return message.find("bay_exit") != std::string::npos;
    }));
    editor.undo();
    editor.select(RelayGame::EditorSelection{true, 3});
    const auto fingerprint = RelayGame::fingerprint_level(editor.level());
    editor.set_property("targetname", "renamed_supply");
    editor.undo();
    CHECK(RelayGame::fingerprint_level(editor.level()) == fingerprint);
    const auto copy = temp.path() / "bench_relay.toml";
    editor.save(copy);
    const auto rebuilt = RelayGame::LevelSessionBuilder{config.gameplay}.build(copy);
    REQUIRE(rebuilt.physics_fixture);
    CHECK(
        std::ranges::count_if(
            RelayGame::build_static_props(rebuilt.level),
            [](const auto& prop) {
                return prop.collision_product.extension() == ".rccollision";
            }
        )
        == 2
    );
    route.enter_bay();
    const auto disk = temp.path() / "progress.toml";
    RelayGame::write_saved_game(disk, route.save());
    RelayGame::GameWorldRuntime restored;
    restored.load_level_session(
        config,
        RelayGame::LevelSessionBuilder{config.gameplay}.build(route.session.level_path)
    );
    restored.restore_saved_game(RelayGame::load_saved_game(disk));
    CHECK(restored.inventory_item_ids() == route.inventory);
    CHECK(restored.enemies().front().state.position == route.session.enemies.front().state.position);
    CHECK(restored.doors().front().state == route.session.doors.front().state);
    route.finish();
    RelayGame::write_saved_game(disk, route.save());
    restored.restore_saved_game(RelayGame::load_saved_game(disk));
    CHECK(restored.mission_status().completed);
    CHECK(restored.player_combat_state().kills == 2);
    CHECK(restored.activated_target_ids() == route.activated);
}

TEST_CASE("bench relay tactical snapshots disk continue and death retry preserve relay state")
{
    RelayCore::test::ScopedTempDirectory temp;
    RelayCore::test::ScopedTestLogger logger{temp.path() / "logs"};
    RelayCore::core::EngineConfig config;
    for (const auto tactic : {RelayCore::test::BenchRelayTactic::Distance, RelayCore::test::BenchRelayTactic::Cover})
    {
        RelayCore::test::BenchRelayRoute route{config.gameplay, tactic};
        route.enter_bay();
        RelayGame::GameWorldRuntime restored;
        restored.load_level_session(
            config,
            RelayGame::LevelSessionBuilder{config.gameplay}.build(route.session.level_path)
        );
        for (int phase = 0; phase < 2; ++phase)
        {
            CAPTURE(phase);
            if (phase == 1)
                route.power_relay();
            const auto disk = temp.path() / "tactic.toml";
            const auto save = route.save();
            RelayGame::write_saved_game(disk, save);
            restored.restore_saved_game(RelayGame::load_saved_game(disk));
            const auto checkpoint = restored.capture_encounter_snapshot(save.played_seconds, 0, false);
            const auto check_state = [&] {
                CHECK(restored.debug_player_position() == save.player.position);
                CHECK(restored.selected_weapon() == save.world.selected_weapon);
                CHECK(restored.inventory_item_ids() == route.inventory);
                CHECK(restored.player_combat_state().health == route.player.health);
                CHECK(restored.player_combat_state().kills == save.world.player_combat.kills);
                CHECK(restored.enemies().front().state.position == save.enemies.front().position);
                CHECK(restored.enemies().front().state.health == save.enemies.front().health);
                CHECK(restored.enemies().front().state.mode == save.enemies.front().mode);
                CHECK(restored.turrets().front().health == route.session.combat.turrets.front().health);
                CHECK(restored.activated_target_ids() == route.activated);
                CHECK(restored.mission_status().current_step == static_cast<std::uint32_t>(phase == 0 ? 3 : 4));
                CHECK_FALSE(restored.mission_status().completed);
                const auto exit_reader =
                    std::ranges::find(restored.usables(), "exit_reader", &RelayCore::gameplay::RuntimeUsable::name);
                REQUIRE(exit_reader != restored.usables().end());
                CHECK(exit_reader->locked == (phase == 0));
                CHECK(restored.doors().front().state == route.session.doors.front().state);
                CHECK(restored.doors().back().state == RelayCore::gameplay::DoorState::Closed);
            };
            check_state();
            const auto live_player = route.player;
            static_cast<void>(RelayCore::gameplay::apply_player_damage(route.player, 1000, "test"));
            const auto dead = route.save();
            route.player = live_player;
            RelayGame::write_saved_game(disk, dead);
            restored.restore_saved_game(RelayGame::load_saved_game(disk));
            CHECK_FALSE(restored.player_combat_state().alive);
            CHECK(restored.debug_advance_player_respawn(2));
            CHECK(restored.player_combat_state().alive);
            CHECK(restored.debug_player_position() == route.session.spawn_position);
            CHECK(restored.activated_target_ids() == route.activated);
            CHECK(restored.enemies().front().state.mode == save.enemies.front().mode);
            restored.restore_encounter_snapshot(checkpoint);
            check_state();
        }
        route.leave_bay();
        CHECK(route.mission.completed());
    }
}
