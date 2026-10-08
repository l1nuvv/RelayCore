#include "relaycore/core/engine_config.h"
#include "relaycore/physics/collision.h"
#include "relaycore/render/model_asset.h"
#include "relaygame/game_world_runtime.h"
#include "relaygame/level_editor_document.h"
#include "relaygame/static_props.h"
#include "relaygame/usable_interaction.h"
#include "relaygame/world_audio_zones.h"

#include <algorithm>
#include <array>
#include <doctest/doctest.h>
#include <filesystem>
#include <glm/ext/matrix_transform.hpp>
#include <glm/geometric.hpp>
#include <limits>

#include "pressure_transfer_route.h"
#include "scoped_temp_directory.h"
#include "scoped_test_logger.h"

using RelayCore::test::PressureRoute;

namespace
{
    auto map_path() -> std::filesystem::path
    {
        return std::filesystem::path{__FILE__}.parent_path().parent_path() / "assets/levels/pressure_transfer.toml";
    }

} // namespace

TEST_CASE("pressure transfer wall equipment and lights meet their supporting surfaces")
{
    RelayCore::test::ScopedTempDirectory temp;
    RelayCore::test::ScopedTestLogger logger{temp.path() / "logs"};
    PressureRoute route{RelayCore::core::GameplayConfig{}};
    const auto supported = [&](glm::vec3 point) {
        return std::ranges::any_of(route.session.level.solids, [&](const auto& solid) {
            return solid.material != RelayCore::SurfaceMaterial::LightStrip
                   && glm::all(glm::greaterThanEqual(point, solid.mins - glm::vec3{.002F}))
                   && glm::all(glm::lessThanEqual(point, solid.maxs + glm::vec3{.002F}));
        });
    };
    int mounted = 0;
    for (const auto& prop : RelayGame::build_static_props(route.session.level))
    {
        const auto name = prop.model.stem().string();
        if (name != "ventilation_unit" && name != "utility_pipe" && name != "safety_notice"
            && name != "lab_notes_board")
            continue;
        CAPTURE(name);
        CAPTURE(prop.authored_entity_index);
        const auto model = RelayCore::render::read_model_asset(prop.model);
        glm::vec3 mins{std::numeric_limits<float>::max()}, maxs{-std::numeric_limits<float>::max()};
        for (const auto& part : model.parts)
            for (const auto& vertex : model.meshes[part.mesh].geometry.vertices)
            {
                const auto position = glm::vec3{prop.transform * part.transform * glm::vec4{vertex.position, 1}};
                mins = glm::min(mins, position);
                maxs = glm::max(maxs, position);
            }
        auto rear = (mins + maxs) * .5F;
        const auto yaw = route.session.level.entities[prop.authored_entity_index].angles.x;
        if (yaw == 270)
            rear.x = maxs.x;
        else if (yaw == 90)
            rear.x = mins.x;
        else if (yaw == 180)
            rear.z = maxs.z;
        else
            rear.z = mins.z;
        CHECK(supported(rear));
        ++mounted;
    }
    CHECK(mounted == 14);
    for (const auto& turret : route.session.combat.turrets)
    {
        CHECK(turret.yaw == 180);
        const auto size = turret.bounds.maxs - turret.bounds.mins;
        CHECK(supported(turret.origin + glm::vec3{0, 0, size.z * .48F}));
        CHECK_FALSE(supported(turret.origin - glm::vec3{0, 0, size.z * .49F}));
    }
    for (const auto& solid : route.session.level.solids)
        if (solid.material == RelayCore::SurfaceMaterial::LightStrip)
            CHECK(supported({(solid.mins.x + solid.maxs.x) * .5F, solid.maxs.y, (solid.mins.z + solid.maxs.z) * .5F}));
    for (const auto& effect : route.session.environment)
    {
        if (effect.kind == RelayGame::EnvironmentKind::Fan)
        {
            CHECK(effect.angles.x == 270);
            CHECK(supported(effect.origin + glm::vec3{.12F, 0, 0}));
        }
        if (effect.kind == RelayGame::EnvironmentKind::Decal)
        {
            CAPTURE(effect.mark);
            auto transform = glm::translate(glm::mat4{1}, effect.origin);
            transform = glm::rotate(transform, glm::radians(effect.angles.x), glm::vec3{0, 1, 0});
            for (const float x : {-.42F, .42F})
                for (const float y : {-.35F, .35F})
                    CHECK(supported(glm::vec3{transform * glm::vec4{x, y, -.001F, 1}}));
        }
    }
}

TEST_CASE("pressure transfer complete route keeps interactions reachable and repeats both weapon roles")
{
    RelayCore::test::ScopedTempDirectory temp;
    RelayCore::test::ScopedTestLogger logger{temp.path() / "logs"};
    RelayCore::core::GameplayConfig config;
    for (int repeat = 0; repeat < 3; ++repeat)
    {
        CAPTURE(repeat);
        PressureRoute route{config};
        REQUIRE(route.session.authoring_warnings.empty());
        REQUIRE(route.session.ending.has_value());
        for (const auto& zone : route.session.audio_zones)
            CHECK(std::filesystem::is_regular_file(zone.loop_path));
        route.walk_to({6.1F, .01F, 8.5F});
        CHECK(route.use("service_reader") == RelayCore::gameplay::UseInteractionOutcome::MissingRequiredItem);
        CHECK(route.session.doors[0].state == RelayCore::gameplay::DoorState::Closed);
        route.reach_mixed();
        const auto tactic =
            repeat == 1 ? RelayCore::test::PressureTactic::Distance : RelayCore::test::PressureTactic::Cover;
        route.fight_mixed(tactic);
        CAPTURE(route.mixed_rover_hit_range);
        CHECK(route.mixed_rover_hit_range > 0);
        if (tactic == RelayCore::test::PressureTactic::Distance)
        {
            CHECK(route.mixed_rover_hit_range > 4);
            CHECK(route.mixed_shotgun_shots == 0);
        } else
        {
            CHECK(route.mixed_rover_hit_range < 4);
            CHECK(route.mixed_shotgun_shots > 0);
            CHECK(route.closest_mixed_rover_z < 33);
        }
        CHECK(route.session.doors[1].state == RelayCore::gameplay::DoorState::Closed);
        route.power_transfer();
        route.leave();
        CHECK(route.mission.completed());
        CHECK(route.player.kills == 4);
        CHECK(route.player.deaths == 0);
        CHECK(route.precise_shots > 0);
        CHECK(route.shotgun_shots > 0);
    }
}

TEST_CASE("pressure transfer native bench admits rover and low shots but blocks a standing shortcut")
{
    RelayCore::test::ScopedTempDirectory temp;
    RelayCore::test::ScopedTestLogger logger{temp.path() / "logs"};
    PressureRoute route{{}};
    REQUIRE(route.session.physics_fixture);
    const auto& world = *route.session.physics_fixture;
    CHECK_FALSE(world.raycast({28, .6F, 32}, {0, 0, 1}, 3));
    CHECK_FALSE(world.cast_box({28, .485F, 32}, {.42F, .475F, .42F}, {0, 0, 3}));
    const auto top = world.raycast({28, 1.1F, 32}, {0, 0, 1}, 3);
    REQUIRE(top);
    CHECK(top->object.kind == RelayCore::physics::WorldObjectKind::CollisionMesh);
    CHECK(top->surface == RelayCore::SurfaceMaterial::Tile);
    CHECK(world.cast_box({28, .91F, 32}, {.3F, .9F, .3F}, {0, 0, 3}).has_value());
}

TEST_CASE("pressure transfer remains completable with the pistol alone")
{
    RelayCore::test::ScopedTempDirectory temp;
    RelayCore::test::ScopedTestLogger logger{temp.path() / "logs"};
    PressureRoute route{{}};
    route.pistol_only = true;
    route.reach_mixed();
    route.fight_mixed(RelayCore::test::PressureTactic::Distance);
    route.power_transfer();
    route.leave();
    CHECK(route.mission.completed());
    CHECK(route.player.kills == 4);
    CHECK(route.shotgun_shots == 0);
    MESSAGE(
        "pistol-only: shots=",
        route.precise_shots,
        "; damage=",
        route.player.damage_taken,
        "; ticks=",
        route.ticks
    );
}

TEST_CASE("pressure transfer close rover rewards shotgun timing while distant fire rewards the pistol")
{
    RelayCore::test::ScopedTempDirectory temp;
    RelayCore::test::ScopedTestLogger logger{temp.path() / "logs"};
    const auto fight = [](float range, RelayGame::PlayerWeapon weapon) {
        PressureRoute route{{}};
        route.position = {4, .01F, 12};
        route.session.enemies[0].state.position = {4, .01F, 12 + range};
        for (int i = 0; i < 1200 && route.session.enemies[0].state.mode != RelayGame::EnemyMode::Dead; ++i)
        {
            route.tick(false);
            const auto eye = route.position + glm::vec3{0, 1.65F, 0};
            const auto target = route.session.enemies[0].state.position + glm::vec3{0, .45F, 0};
            static_cast<void>(RelayGame::fire_player_weapon(
                route.world(),
                eye,
                glm::normalize(target - eye),
                route.player,
                route.session.combat.turrets,
                route.session.enemies,
                weapon
            ));
        }
        CHECK(route.session.enemies[0].state.mode == RelayGame::EnemyMode::Dead);
        CHECK(route.player.alive);
        return std::pair{route.ticks, route.player.shots_fired};
    };
    const auto close_shotgun = fight(2.2F, RelayGame::PlayerWeapon::Shotgun);
    const auto close_pistol = fight(2.2F, RelayGame::PlayerWeapon::Precise);
    CHECK(close_shotgun.first < close_pistol.first);
    CHECK(close_shotgun.second == 1);
    CHECK(close_pistol.second == 3);
    const auto distant_shotgun = fight(8, RelayGame::PlayerWeapon::Shotgun);
    const auto distant_pistol = fight(8, RelayGame::PlayerWeapon::Precise);
    CHECK(distant_pistol.first < distant_shotgun.first);
    MESSAGE(
        "close kill ticks: shotgun=",
        close_shotgun.first,
        "; pistol=",
        close_pistol.first,
        "; distant: shotgun=",
        distant_shotgun.first,
        "; pistol=",
        distant_pistol.first
    );
}

TEST_CASE("pressure transfer stops the pressure leak and switches to running ventilation")
{
    RelayCore::test::ScopedTempDirectory temp;
    RelayCore::test::ScopedTestLogger logger{temp.path() / "logs"};
    PressureRoute route{{}};
    const auto listener = RelayCore::physics::make_player_aabb({38, .01F, 40}, .3F, 1.8F);
    const auto standby = RelayGame::find_active_audio_zone(route.session.audio_zones, listener);
    REQUIRE(standby >= 0);
    CHECK(route.session.audio_zones[standby].name == "transfer_standby");
    route.reach_mixed();
    route.fight_mixed(RelayCore::test::PressureTactic::Cover);
    route.power_transfer();
    RelayGame::update_world_environment(route.session.environment, route.activated, 2);
    RelayGame::update_world_environment(route.session.environment, route.activated, 2);
    const auto running =
        RelayGame::find_active_audio_zone(route.session.audio_zones, listener, standby, route.activated);
    REQUIRE(running >= 0);
    CHECK(route.session.audio_zones[running].name == "transfer_ventilation");
    for (const auto& effect : route.session.environment)
    {
        if (effect.kind == RelayGame::EnvironmentKind::Steam)
        {
            CHECK_FALSE(effect.active);
            CHECK(effect.strength < .001F);
        }
        if (effect.kind == RelayGame::EnvironmentKind::Fan)
            CHECK(effect.strength > .9F);
        if (effect.kind == RelayGame::EnvironmentKind::Lamp && effect.required_target == "transfer_running")
            CHECK(effect.strength == 1);
    }
}

TEST_CASE("pressure transfer editor round trip preserves save compatibility")
{
    RelayCore::test::ScopedTempDirectory temp;
    RelayCore::test::ScopedTestLogger logger{temp.path() / "logs"};
    const RelayCore::core::GameplayConfig config;
    const RelayGame::LevelSessionBuilder builder{config};
    const auto original = builder.build(map_path());
    RelayGame::LevelEditorDocument document;
    document.reset(original.level);
    const auto exported = temp.path() / "pressure_transfer.toml";
    document.save(exported);
    const auto round_trip = builder.build(exported);
    CHECK(round_trip.authoring_warnings.empty());
    CHECK(RelayGame::fingerprint_level(round_trip.level) == RelayGame::fingerprint_level(original.level));
}

TEST_CASE("pressure transfer rover reaches the open doorway and the hall corner")
{
    RelayCore::test::ScopedTempDirectory temp;
    RelayCore::test::ScopedTestLogger logger{temp.path() / "logs"};
    for (const auto position : {glm::vec3{4, .01F, 9.5F}, glm::vec3{7, .01F, 22}})
    {
        PressureRoute route{RelayCore::core::GameplayConfig{}};
        route.position = position;
        if (position.z < 10)
        {
            for (int i = 0; i < 240; ++i)
                route.tick(false);
            CHECK(route.session.enemies[0].state.mode == RelayGame::EnemyMode::Idle);
            CHECK(route.player.health == 100);
        }
        REQUIRE(RelayCore::gameplay::activate_target("service_gate", route.session.doors));
        for (int i = 0; i < 1100 && route.session.enemies[0].state.mode != RelayGame::EnemyMode::Windup; ++i)
            route.tick(false);
        CHECK(route.session.enemies[0].state.mode == RelayGame::EnemyMode::Windup);
        CHECK(
            glm::distance(
                glm::vec2{position.x, position.z},
                glm::vec2{route.session.enemies[0].state.position.x, route.session.enemies[0].state.position.z}
            )
            < 1.2F
        );
    }
}

TEST_CASE("pressure transfer mixed cover blocks turret fire while rover keeps approaching")
{
    RelayCore::test::ScopedTempDirectory temp;
    RelayCore::test::ScopedTestLogger logger{temp.path() / "logs"};
    for (bool covered : {true, false})
    {
        CAPTURE(covered);
        PressureRoute route{RelayCore::core::GameplayConfig{}};
        route.position = covered ? glm::vec3{28, .01F, 29} : glm::vec3{34, .01F, 29};
        const auto initial = route.session.enemies[1].state.position;
        unsigned shots = 0;
        bool windup = false;
        for (int i = 0; i < 660; ++i)
        {
            route.tick(false);
            windup = windup || route.session.enemies[1].state.mode == RelayGame::EnemyMode::Windup;
            shots += route.session.combat.turrets[1].shot_trace.remaining_seconds > 0;
        }
        if (covered)
        {
            CHECK(shots == 0);
            CHECK(windup);
            CHECK(glm::distance(initial, route.session.enemies[1].state.position) > 5);
        } else
        {
            CHECK(shots > 0);
            CHECK(route.player.health < 100);
        }
    }
}

TEST_CASE("pressure transfer continue and retry preserve pickup device and exit progression")
{
    RelayCore::test::ScopedTempDirectory temp;
    RelayCore::test::ScopedTestLogger logger{temp.path() / "logs"};
    const RelayCore::core::EngineConfig config;
    RelayGame::GameWorldRuntime runtime;
    runtime.load_level_session(config, RelayGame::LevelSessionBuilder{config.gameplay}.build(map_path()));
    const auto use = [&](std::string_view name) {
        const auto found = std::ranges::find(runtime.usables(), name, &RelayCore::gameplay::RuntimeUsable::name);
        REQUIRE(found != runtime.usables().end());
        return runtime.debug_apply_usable_interaction(static_cast<std::size_t>(found - runtime.usables().begin()));
    };
    for (int stage = 0; stage < 5; ++stage)
    {
        CAPTURE(stage);
        if (stage == 1)
        {
            CHECK(use("transfer_shotgun").kind == RelayCore::gameplay::UseInteractionOutcome::PickedUpItem);
            REQUIRE(runtime.select_weapon(RelayGame::PlayerWeapon::Shotgun));
        }
        if (stage == 2)
            CHECK(use("service_reader").kind == RelayCore::gameplay::UseInteractionOutcome::ActivatedTarget);
        if (stage == 3)
            CHECK(use("transfer_control").kind == RelayCore::gameplay::UseInteractionOutcome::ActivatedTarget);
        if (stage == 4)
            CHECK(use("exit_reader").kind == RelayCore::gameplay::UseInteractionOutcome::ActivatedTarget);
        const auto checkpoint = runtime.capture_encounter_snapshot(12, 4, false);
        const auto save_path = temp.path() / "progress.toml";
        RelayGame::write_saved_game(save_path, runtime.capture_saved_game(12, false));
        runtime.load_level_session(config, RelayGame::LevelSessionBuilder{config.gameplay}.build(map_path()));
        runtime.restore_saved_game(RelayGame::load_saved_game(save_path));
        CHECK(runtime.inventory_item_ids() == checkpoint.game.world.inventory_item_ids);
        CHECK(runtime.activated_target_ids() == checkpoint.game.world.activated_target_ids);
        CHECK(runtime.selected_weapon() == checkpoint.game.world.selected_weapon);
        CHECK(runtime.capture_runtime_snapshot().usables[3].locked == (stage < 3));
        CHECK(runtime.doors()[1].state == checkpoint.game.world.doors[1].state);
        for (int i = 0; i < 40 && runtime.player_combat_state().alive; ++i)
        {
            runtime.debug_tick_combat(2);
            static_cast<void>(runtime.debug_apply_combat_threats(
                RelayCore::physics::make_player_aabb({38, .01F, 41}, .3F, 1.8F),
                {38, 1.65F, 41},
                2
            ));
        }
        REQUIRE_FALSE(runtime.player_combat_state().alive);
        REQUIRE(runtime.debug_advance_player_respawn(3));
        CHECK(runtime.debug_player_position() == checkpoint.game.player.position);
        CHECK(runtime.inventory_item_ids() == checkpoint.game.world.inventory_item_ids);
        CHECK(runtime.activated_target_ids() == checkpoint.game.world.activated_target_ids);
        CHECK(runtime.selected_weapon() == checkpoint.game.world.selected_weapon);
        runtime.restore_encounter_snapshot(checkpoint);
        CHECK(runtime.player_combat_state().deaths == checkpoint.game.world.player_combat.deaths);
        CHECK(runtime.doors()[1].state == checkpoint.game.world.doors[1].state);
    }
}

TEST_CASE("pressure transfer native tactics survive disk continue and encounter replacement")
{
    RelayCore::test::ScopedTempDirectory temp;
    RelayCore::test::ScopedTestLogger logger{temp.path() / "logs"};
    const RelayCore::core::EngineConfig config;
    for (const auto tactic : {RelayCore::test::PressureTactic::Distance, RelayCore::test::PressureTactic::Cover})
    {
        PressureRoute route{config.gameplay};
        route.reach_mixed();
        RelayGame::GameWorldRuntime runtime;
        for (int phase = 0; phase < 2; ++phase)
        {
            if (phase == 1)
            {
                route.fight_mixed(tactic);
                route.power_transfer();
            }
            const auto save = route.save();
            const auto disk = temp.path() / "native-progress.toml";
            RelayGame::write_saved_game(disk, save);
            runtime.load_level_session(
                config,
                RelayGame::LevelSessionBuilder{config.gameplay}.build(route.session.level_path)
            );
            runtime.restore_saved_game(RelayGame::load_saved_game(disk));
            const auto checkpoint = runtime.capture_encounter_snapshot(save.played_seconds, 0, false);
            const auto check_state = [&] {
                CHECK(runtime.debug_player_position() == route.position);
                CHECK(runtime.selected_weapon() == route.weapon);
                CHECK(runtime.player_combat_state().health == route.player.health);
                CHECK(runtime.player_combat_state().kills == route.player.kills);
                CHECK(runtime.enemies()[1].state.position == route.session.enemies[1].state.position);
                CHECK(runtime.enemies()[1].state.mode == route.session.enemies[1].state.mode);
                CHECK(runtime.enemies()[1].state.health == route.session.enemies[1].state.health);
                CHECK(runtime.turrets()[1].destroyed == route.session.combat.turrets[1].destroyed);
                CHECK(runtime.activated_target_ids() == route.activated);
                CHECK(runtime.mission_status().current_step == static_cast<std::uint32_t>(phase == 0 ? 6 : 7));
                CHECK(runtime.doors()[1].state == RelayCore::gameplay::DoorState::Closed);
                CHECK_FALSE(runtime.query_physics_fixture({28, .6F, 31.4F}, {0, 0, 1}, 3));
                const auto top = runtime.query_physics_fixture({28, 1.1F, 31.4F}, {0, 0, 1}, 3);
                REQUIRE(top);
                CHECK(top->object.kind == RelayCore::physics::WorldObjectKind::CollisionMesh);
            };
            check_state();
            runtime.load_level_session(
                config,
                RelayGame::LevelSessionBuilder{config.gameplay}.build(route.session.level_path)
            );
            runtime.restore_encounter_snapshot(checkpoint);
            check_state();

            auto dead = runtime.capture_saved_game(save.played_seconds, false);
            dead.world.player_combat.alive = false;
            dead.world.player_combat.health = 0;
            dead.world.player_combat.respawn_remaining_seconds = runtime.player_combat_state().respawn_delay_seconds;
            ++dead.world.player_combat.deaths;
            RelayGame::write_saved_game(disk, dead);
            runtime.load_level_session(
                config,
                RelayGame::LevelSessionBuilder{config.gameplay}.build(route.session.level_path)
            );
            runtime.restore_saved_game(RelayGame::load_saved_game(disk));
            REQUIRE_FALSE(runtime.player_combat_state().alive);
            REQUIRE(runtime.debug_advance_player_respawn(3));
            CHECK(runtime.debug_player_position() == glm::vec3{28, .01F, 29});
            CHECK(runtime.capture_saved_game(0, false).player.view_angles == glm::vec2{glm::radians(90.0F), 0});
            CHECK(runtime.player_combat_state().kills == route.player.kills);
            CHECK(runtime.player_combat_state().deaths == dead.world.player_combat.deaths);
            CHECK(runtime.inventory_item_ids() == route.inventory);
            CHECK(runtime.activated_target_ids() == route.activated);
            CHECK(runtime.enemies()[1].state.mode == route.session.enemies[1].state.mode);
            CHECK(runtime.turrets()[1].destroyed == route.session.combat.turrets[1].destroyed);
            CHECK(runtime.mission_status().current_step == static_cast<std::uint32_t>(phase == 0 ? 6 : 7));
            if (phase == 0)
            {
                route.position = runtime.debug_player_position();
                route.player = runtime.player_combat_state();
                for (int tick = 0; tick < 120; ++tick)
                    route.tick(false);
                CHECK(route.player.health == 100);
            }
        }
        route.leave();
        const auto disk = temp.path() / "completed.toml";
        RelayGame::write_saved_game(disk, route.save());
        runtime.restore_saved_game(RelayGame::load_saved_game(disk));
        CHECK(runtime.mission_status().completed);
        CHECK(runtime.player_combat_state().kills == 4);
    }
}
