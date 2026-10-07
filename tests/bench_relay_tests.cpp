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

TEST_CASE("bench relay complete route uses both weapons native pursuit controls and exit")
{
    RelayCore::test::ScopedTempDirectory temp;
    RelayCore::test::ScopedTestLogger logger{temp.path() / "logs"};
    for (int attempt = 0; attempt < 2; ++attempt)
    {
        RelayCore::test::BenchRelayRoute route{{}};
        route.enter_bay();
        route.finish();
        CHECK(route.mission.completed());
        CHECK(route.player.kills == 2);
        CHECK(route.player.deaths == 0);
        CHECK(route.precise_shots > 0);
        CHECK(route.shotgun_shots > 0);
        CHECK(route.closest_rover_z < 6.1F);
        CHECK(route.session.doors.back().state == RelayCore::gameplay::DoorState::Open);
    }
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
