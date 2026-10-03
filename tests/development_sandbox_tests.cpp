#include "relaycore/core/engine_config.h"
#include "relaycore/gameplay/combat_runtime.h"
#include "relaycore/physics/collision.h"
#include "relaygame/game_world_runtime.h"
#include "relaygame/level_session_builder.h"
#include "relaygame/usable_interaction.h"

#include <algorithm>
#include <array>
#include <doctest/doctest.h>
#include <filesystem>
#include <glm/geometric.hpp>

#include "scoped_temp_directory.h"
#include "scoped_test_logger.h"
#include "world_query_fixture.h"

namespace
{
    auto sandbox_path() -> std::filesystem::path
    {
        return std::filesystem::path{__FILE__}.parent_path().parent_path() / "assets/levels/development_sandbox.toml";
    }

    auto blockers(const RelayGame::LevelSession& session) -> std::vector<RelayCore::physics::Aabb>
    {
        std::vector<RelayCore::physics::Aabb> result;
        for (const auto* solids : {&session.level.solids, &session.level.prop_colliders})
            for (const auto& solid : *solids)
                result.push_back({solid.mins, solid.maxs});
        for (const auto& door : session.doors)
            if (RelayCore::gameplay::is_door_blocking(door))
                result.push_back(RelayCore::gameplay::current_door_bounds(door));
        return result;
    }

    auto walk(const std::vector<RelayCore::physics::Aabb>& world, glm::vec3 position, glm::vec3 velocity, int ticks)
        -> glm::vec3
    {
        for (int i = 0; i < ticks; ++i)
            position = RelayCore::physics::move_and_collide(
                           RelayCore::test::WorldQueryFixture{world},
                           position,
                           velocity,
                           1.0F / 120,
                           {},
                           true
            )
                           .position;
        return position;
    }
} // namespace

TEST_CASE("sandbox entries are grounded and the safe hub isolates combat bays")
{
    RelayCore::test::ScopedTempDirectory temp;
    RelayCore::test::ScopedTestLogger logger{temp.path() / "logs"};
    const RelayCore::core::GameplayConfig config;
    const RelayGame::LevelSessionBuilder builder{config};
    auto session = builder.build(sandbox_path());
    CHECK(session.authoring_warnings.empty());
    auto without_consumers = session.level;
    std::erase_if(without_consumers.entities, [](const auto& entity) {
        return entity.properties.contains("required_target") || entity.properties.contains("stop_target");
    });
    without_consumers.entity_sources.clear();
    const auto unbound = builder.build(std::move(without_consumers), sandbox_path());
    REQUIRE(unbound.authoring_warnings.size() == 1);
    CHECK(unbound.authoring_warnings.front().message.find("lab_power") != std::string::npos);
    CHECK(session.mission_definition.steps.empty());
    CHECK(session.exits.empty());
    CHECK_FALSE(session.ending.has_value());
    for (const char* entry : {"hub", "movement", "devices", "range", "arena", "cover", "rover", "mixed"})
    {
        const auto at_entry = builder.build_at_entry(sandbox_path(), entry);
        CAPTURE(entry);
        const auto world = blockers(at_entry);
        CHECK(RelayCore::physics::probe_ground(RelayCore::test::WorldQueryFixture{world}, at_entry.spawn_position, {}));
    }
    const auto world = blockers(session);
    CHECK(walk(world, {-4, 0.01F, 0}, {0, -0.1F, 3}, 480).z < 5.6F);
    CHECK(walk(world, {4, 0.01F, 0}, {0, -0.1F, 3}, 480).z < 5.6F);
    CHECK(walk(world, {0, 0.01F, 0}, {-3, -0.1F, 0}, 600).x < -12);
    CHECK(walk(world, {0, 0.01F, 0}, {3, -0.1F, 0}, 600).x < 7.6F);
    CHECK(walk(world, {-10, 0.01F, -3}, {-3, -0.1F, 0}, 360).y > 0.99F);
    auto player = RelayCore::gameplay::PlayerCombatState{};
    std::vector<RelayCore::physics::WorldCollider> enemy_blockers;
    for (int i = 0; i < 240; ++i)
    {
        const auto bounds = RelayCore::physics::make_player_aabb(session.spawn_position, .3F, 1.8F);
        const auto threats = RelayCore::gameplay::update_combat_threats(
            RelayCore::test::WorldQueryFixture{session.level, session.doors},
            bounds,
            session.spawn_position + glm::vec3{0, 1.65F, 0},
            player,
            session.combat.turrets,
            session.combat.hazards,
            1.0F / 60
        );
        CHECK(threats.turret_shots == 0);
        const auto enemies = RelayGame::update_enemies(
            RelayCore::test::WorldQueryFixture{session.level, session.doors},
            bounds,
            session.spawn_position + glm::vec3{0, 1.65F, 0},
            player,
            session.enemies,
            1.0F / 60,
            enemy_blockers
        );
        CHECK(enemies.strikes == 0);
    }
    CHECK(player.health == player.max_health);
}

TEST_CASE("sandbox isolated rover lane lets the player dodge an authored windup")
{
    RelayCore::test::ScopedTempDirectory temp;
    RelayCore::test::ScopedTestLogger logger{temp.path() / "logs"};
    const RelayCore::core::GameplayConfig config;
    auto session = RelayGame::LevelSessionBuilder{config}.build_at_entry(sandbox_path(), "rover");
    auto player = RelayCore::gameplay::PlayerCombatState{};
    std::vector<RelayCore::physics::WorldCollider> scratch;
    const auto step = [&](glm::vec3 position) {
        const auto bounds = RelayCore::physics::make_player_aabb(position, .3F, 1.8F);
        const auto eye = position + glm::vec3{0, 1.65F, 0};
        RelayCore::gameplay::tick_combat_state(1.0F / 120, player, session.combat.turrets);
        CHECK(
            RelayCore::gameplay::update_combat_threats(
                RelayCore::test::WorldQueryFixture{session.level, session.doors},
                bounds,
                eye,
                player,
                session.combat.turrets,
                session.combat.hazards,
                1.0F / 120
            )
                .turret_shots
            == 0
        );
        return RelayGame::update_enemies(
            RelayCore::test::WorldQueryFixture{session.level, session.doors},
            bounds,
            eye,
            player,
            session.enemies,
            1.0F / 120,
            scratch
        );
    };
    unsigned windups = 0;
    for (int i = 0; i < 240 && windups == 0; ++i)
        windups += step(session.spawn_position).windups;
    REQUIRE(windups == 1);
    REQUIRE(player.health == player.max_health);
    const auto attacking = std::ranges::find_if(session.enemies, [](const auto& enemy) {
        return enemy.state.mode == RelayGame::EnemyMode::Windup;
    });
    REQUIRE(attacking != session.enemies.end());
    const auto retreat = walk(blockers(session), session.spawn_position, {0, -.1F, -4.5F}, 48);
    REQUIRE(retreat.z < 16.3F);
    unsigned strikes = 0;
    for (int i = 0; i < 60; ++i)
        strikes += step(retreat).strikes;
    CHECK(strikes == 1);
    CHECK(player.health == player.max_health);
    CHECK(attacking->state.mode == RelayGame::EnemyMode::Recovery);
}

TEST_CASE("sandbox mixed lane combines mobile pressure with a turret and standing cover")
{
    RelayCore::test::ScopedTempDirectory temp;
    RelayCore::test::ScopedTestLogger logger{temp.path() / "logs"};
    const RelayCore::core::GameplayConfig config;
    const RelayGame::LevelSessionBuilder builder{config};
    for (bool behind_cover : {true, false})
    {
        CAPTURE(behind_cover);
        auto session = builder.build_at_entry(sandbox_path(), "mixed");
        const auto position = behind_cover ? glm::vec3{22, .02F, 20} : session.spawn_position;
        auto player = RelayCore::gameplay::PlayerCombatState{};
        std::vector<RelayCore::physics::WorldCollider> scratch;
        unsigned shots = 0, windups = 0, strikes = 0;
        const auto rover = std::ranges::find_if(session.enemies, [](const auto& enemy) {
            return enemy.state.position.x > 20;
        });
        REQUIRE(rover != session.enemies.end());
        const auto initial_position = rover->state.position;
        const auto simulate = [&](glm::vec3 at, int ticks) {
            const auto bounds = RelayCore::physics::make_player_aabb(at, .3F, 1.8F);
            const auto eye = at + glm::vec3{0, 1.65F, 0};
            for (int i = 0; i < ticks; ++i)
            {
                RelayCore::gameplay::tick_combat_state(1.0F / 120, player, session.combat.turrets);
                shots += RelayCore::gameplay::update_combat_threats(
                             RelayCore::test::WorldQueryFixture{session.level, session.doors},
                             bounds,
                             eye,
                             player,
                             session.combat.turrets,
                             session.combat.hazards,
                             1.0F / 120
                )
                             .turret_shots;
                const auto enemies = RelayGame::update_enemies(
                    RelayCore::test::WorldQueryFixture{session.level, session.doors},
                    bounds,
                    eye,
                    player,
                    session.enemies,
                    1.0F / 120,
                    scratch
                );
                windups += enemies.windups;
                strikes += enemies.strikes;
            }
        };
        if (behind_cover)
            simulate(session.spawn_position, 60);
        simulate(position, behind_cover ? 120 : 360);
        CHECK(glm::distance(rover->state.position, initial_position) > 1);
        if (behind_cover)
        {
            CHECK(shots == 0);
            CHECK(player.health == player.max_health);
        } else
        {
            CHECK(shots > 0);
            CHECK(windups > 0);
            CHECK(strikes > 0);
            CHECK(player.health < player.max_health);
        }
        const auto world = blockers(session);
        const auto escape = walk(world, session.spawn_position, {0, -.1F, -4.5F}, 120);
        CHECK(escape.z < 15);
        const auto around_cover = walk(world, {22, .02F, 17}, {4.5F, -.1F, 0}, 160);
        CHECK(around_cover.x > 27);
    }
}

TEST_CASE("sandbox mixed encounter repeats health targets and attack preparation")
{
    RelayCore::test::ScopedTempDirectory temp;
    RelayCore::test::ScopedTestLogger logger{temp.path() / "logs"};
    RelayCore::core::EngineConfig config;
    auto session = RelayGame::LevelSessionBuilder{config.gameplay}.build_at_entry(sandbox_path(), "mixed");
    const auto position = session.spawn_position;
    RelayGame::GameWorldRuntime runtime;
    runtime.load_level_session(config, std::move(session));
    const auto eye = position + glm::vec3{0, 1.65F, 0};
    const auto bounds = RelayCore::physics::make_player_aabb(position, .3F, 1.8F);
    CHECK(runtime.debug_apply_combat_threats(bounds, eye, 1.0F / 120).turret_windups == 1);
    runtime.debug_tick_combat(.4F);
    const auto initial = runtime.capture_encounter_snapshot(0, 0, false);
    const auto turret = std::ranges::find(runtime.turrets(), "mixed_turret", &RelayCore::gameplay::RuntimeTurret::name);
    REQUIRE(turret != runtime.turrets().end());
    const auto turret_index = static_cast<std::size_t>(turret - runtime.turrets().begin());
    for (int repeat = 0; repeat < 3; ++repeat)
    {
        CAPTURE(repeat);
        unsigned shots = 0;
        for (int i = 0; i < 120; ++i)
        {
            runtime.debug_tick_combat(1.0F / 120);
            shots += runtime.debug_apply_combat_threats(bounds, eye, 1.0F / 120).turret_shots;
        }
        REQUIRE(shots == 1);
        CHECK(runtime.player_combat_state().health < initial.game.world.player_combat.health);
        for (int shot = 0; shot < 3; ++shot)
        {
            runtime.debug_tick_combat(.3F);
            REQUIRE(runtime.debug_apply_player_fire(eye, {0, 0, 1}).fired);
            runtime.debug_tick_combat(.3F);
            REQUIRE(runtime.debug_apply_player_fire(position + glm::vec3{0, .6F, 0}, {0, 0, -1}).fired);
        }
        CHECK(runtime.turrets()[turret_index].destroyed);
        CHECK(runtime.player_combat_state().kills == 2);
        runtime.debug_set_player_position({22, .02F, 20});
        runtime.restore_encounter_snapshot(initial);
        CHECK(runtime.debug_player_position() == initial.game.player.position);
        CHECK(runtime.player_combat_state().health == initial.game.world.player_combat.health);
        CHECK(runtime.player_combat_state().kills == initial.game.world.player_combat.kills);
        CHECK_FALSE(runtime.turrets()[turret_index].destroyed);
        CHECK(runtime.turrets()[turret_index].health == initial.game.world.turrets[turret_index].health);
        CHECK(runtime.turrets()[turret_index].warming_up);
        CHECK(
            runtime.turrets()[turret_index].warmup_remaining_seconds
            == initial.game.world.turrets[turret_index].warmup_remaining_seconds
        );
        REQUIRE(runtime.enemies().size() == initial.game.enemies.size());
        for (std::size_t i = 0; i < runtime.enemies().size(); ++i)
        {
            CHECK(runtime.enemies()[i].state.position == initial.game.enemies[i].position);
            CHECK(runtime.enemies()[i].state.health == initial.game.enemies[i].health);
            CHECK(runtime.enemies()[i].state.mode == initial.game.enemies[i].mode);
        }
    }
}

TEST_CASE("sandbox shotgun pickup switching death and both save paths preserve weapon state")
{
    RelayCore::test::ScopedTempDirectory temp;
    RelayCore::test::ScopedTestLogger logger{temp.path() / "logs"};
    RelayCore::core::EngineConfig config;
    const auto session = RelayGame::LevelSessionBuilder{config.gameplay}.build(sandbox_path());
    RelayGame::GameWorldRuntime runtime;
    runtime.load_level_session(config, session);
    CHECK_FALSE(runtime.select_weapon(RelayGame::PlayerWeapon::Shotgun));
    const auto pickup =
        std::ranges::find(runtime.usables(), RelayGame::shotgun_item_id, &RelayCore::gameplay::RuntimeUsable::item_id);
    REQUIRE(pickup != runtime.usables().end());
    CHECK(
        runtime.debug_apply_usable_interaction(static_cast<std::size_t>(pickup - runtime.usables().begin())).kind
        == RelayCore::gameplay::UseInteractionOutcome::PickedUpItem
    );
    REQUIRE(runtime.select_weapon(RelayGame::PlayerWeapon::Shotgun));
    const auto snapshot = runtime.capture_encounter_snapshot(0, 0, false);
    REQUIRE(runtime.debug_apply_player_fire({11, .45F, 18}, {0, 0, 1}).fired);
    CHECK(
        std::ranges::count_if(
            runtime.player_pellet_traces(),
            [](const auto& trace) {
                return trace.remaining_seconds > 0;
            }
        )
        == 7
    );
    CHECK(runtime.player_combat_state().kills == 1);
    CHECK(runtime.shotgun_cycle_remaining_seconds() == doctest::Approx(.85F));
    REQUIRE(runtime.select_weapon(RelayGame::PlayerWeapon::Precise));
    CHECK_FALSE(runtime.debug_apply_player_fire({28, .45F, 12}, {0, 0, 1}).fired);
    REQUIRE(runtime.select_weapon(RelayGame::PlayerWeapon::Shotgun));
    const auto path = temp.path() / "progress.toml";
    RelayGame::write_saved_game(path, runtime.capture_saved_game(0, false));
    runtime.debug_tick_combat(.1F);
    CHECK(runtime.shotgun_cycle_remaining_seconds() == doctest::Approx(.75F));
    CHECK(std::ranges::none_of(runtime.player_pellet_traces(), [](const auto& trace) {
        return trace.remaining_seconds > 0;
    }));
    runtime.load_level_session(config, session);
    runtime.restore_saved_game(RelayGame::load_saved_game(path));
    CHECK(runtime.shotgun_cycle_remaining_seconds() == 0);
    CHECK(std::ranges::none_of(runtime.player_pellet_traces(), [](const auto& trace) {
        return trace.remaining_seconds > 0;
    }));
    CHECK(runtime.selected_weapon() == RelayGame::PlayerWeapon::Shotgun);
    CHECK(runtime.player_combat_state().weapon_cooldown_remaining_seconds == doctest::Approx(.85F));
    CHECK(runtime.player_combat_state().kills == 1);
    const glm::vec3 hazard_position{-25, .02F, 0};
    static_cast<void>(runtime.debug_apply_combat_threats(
        RelayCore::physics::make_player_aabb(hazard_position, .3F, 1.8F),
        hazard_position + glm::vec3{0, 1.65F, 0},
        10
    ));
    REQUIRE_FALSE(runtime.player_combat_state().alive);
    CHECK_FALSE(runtime.select_weapon(RelayGame::PlayerWeapon::Precise));
    REQUIRE(runtime.debug_advance_player_respawn(2));
    CHECK(runtime.selected_weapon() == RelayGame::PlayerWeapon::Shotgun);
    CHECK(
        std::ranges::find(runtime.inventory_item_ids(), RelayGame::shotgun_item_id)
        != runtime.inventory_item_ids().end()
    );
    runtime.restore_encounter_snapshot(snapshot);
    CHECK(runtime.selected_weapon() == RelayGame::PlayerWeapon::Shotgun);
    CHECK(runtime.player_combat_state().kills == 0);
    CHECK(runtime.enemies()[0].state.mode != RelayGame::EnemyMode::Dead);
    auto invalid = runtime.capture_runtime_snapshot();
    invalid.inventory_item_ids.clear();
    invalid.player_combat.health = 1;
    CHECK_FALSE(runtime.restore_runtime_snapshot(invalid).restored);
    CHECK(runtime.player_combat_state().health == 100);
    CHECK(runtime.selected_weapon() == RelayGame::PlayerWeapon::Shotgun);
    runtime.debug_tick_combat(.86F);
    CHECK(runtime.select_weapon(RelayGame::PlayerWeapon::Precise));
    CHECK(runtime.debug_apply_player_fire({28, .45F, 12}, {0, 0, 1}).fired);
    CHECK(std::ranges::none_of(runtime.player_pellet_traces(), [](const auto& trace) {
        return trace.remaining_seconds > 0;
    }));
}

TEST_CASE("sandbox devices exercise prerequisites signals and full repeat")
{
    RelayCore::test::ScopedTempDirectory temp;
    RelayCore::test::ScopedTestLogger logger{temp.path() / "logs"};
    RelayCore::core::EngineConfig config;
    auto session = RelayGame::LevelSessionBuilder{config.gameplay}.build(sandbox_path());
    RelayGame::GameWorldRuntime runtime;
    runtime.load_level_session(config, session);
    const auto initial = runtime.capture_runtime_snapshot();
    const auto use = [&](std::string_view name) {
        const auto found = std::ranges::find(runtime.usables(), name, &RelayCore::gameplay::RuntimeUsable::name);
        REQUIRE(found != runtime.usables().end());
        return runtime.debug_apply_usable_interaction(static_cast<int>(found - runtime.usables().begin())).kind;
    };
    using RelayCore::gameplay::UseInteractionOutcome;
    CHECK(use("device_reader") == UseInteractionOutcome::MissingRequiredItem);
    const auto card =
        std::ranges::find(runtime.usables(), "sandbox_card", &RelayCore::gameplay::RuntimeUsable::item_id);
    REQUIRE(card != runtime.usables().end());
    CHECK(
        runtime.debug_apply_usable_interaction(static_cast<int>(card - runtime.usables().begin())).kind
        == UseInteractionOutcome::PickedUpItem
    );
    CHECK(use("device_reader") == UseInteractionOutcome::ActivatedTarget);
    CHECK(use("range_control") == UseInteractionOutcome::ActivatedTarget);
    CHECK(use("arena_control") == UseInteractionOutcome::ActivatedTarget);
    CHECK(use("lab_power_control") == UseInteractionOutcome::ActivatedTarget);
    CHECK(std::ranges::find(runtime.activated_target_ids(), "lab_power") != runtime.activated_target_ids().end());
    RelayGame::update_world_environment(session.environment, runtime.activated_target_ids(), 1);
    for (const auto& effect : session.environment)
    {
        if (effect.required_target == "lab_power")
            CHECK(effect.active);
        if (effect.stop_target == "lab_power")
            CHECK_FALSE(effect.active);
    }
    auto triggers = session.triggers;
    auto doors = session.doors;
    auto usables = session.usables;
    std::vector<std::string> activated, signaled;
    RelayCore::gameplay::InteractionEvent event;
    const auto sensor_bounds = RelayCore::physics::make_player_aabb({22, .02F, 0}, .3F, 1.8F);
    CHECK(
        RelayCore::gameplay::update_trigger_overlaps(
            sensor_bounds,
            triggers,
            doors,
            usables,
            activated,
            signaled,
            event
        )
        == 1
    );
    CHECK(
        RelayCore::gameplay::update_trigger_overlaps(
            sensor_bounds,
            triggers,
            doors,
            usables,
            activated,
            signaled,
            event
        )
        == 0
    );
    const auto relay = std::ranges::find(doors, "relay_gate", &RelayCore::gameplay::RuntimeDoor::name);
    REQUIRE(relay != doors.end());
    CHECK(relay->state == RelayCore::gameplay::DoorState::Opening);
    REQUIRE(runtime.restore_runtime_snapshot(initial).restored);
    CHECK(runtime.inventory_item_ids().empty());
    CHECK(runtime.activated_target_ids().empty());
    CHECK(use("device_reader") == UseInteractionOutcome::MissingRequiredItem);
    CHECK(use("lab_power_control") == UseInteractionOutcome::ActivatedTarget);
}

TEST_CASE("sandbox firing stations hit their targets and cover blocks turret fire")
{
    RelayCore::test::ScopedTempDirectory temp;
    RelayCore::test::ScopedTestLogger logger{temp.path() / "logs"};
    const RelayCore::core::GameplayConfig config;
    auto session = RelayGame::LevelSessionBuilder{config}.build(sandbox_path());
    for (const auto& station :
         std::array{glm::vec3{-26, 1.65F, 20}, glm::vec3{-20, 1.65F, 16}, glm::vec3{-14, 1.65F, 11}})
    {
        auto player = RelayCore::gameplay::PlayerCombatState{};
        const auto found = std::ranges::find_if(session.combat.turrets, [&](const auto& turret) {
            return turret.origin.x == station.x;
        });
        REQUIRE(found != session.combat.turrets.end());
        const auto shot = RelayCore::gameplay::apply_player_fire(
            RelayCore::test::WorldQueryFixture{session.level, session.doors},
            station,
            glm::normalize(found->origin - station),
            player,
            session.combat.turrets
        );
        CHECK(shot.hit_turret);
    }
    auto turret = std::ranges::find(session.combat.turrets, "cover_turret", &RelayCore::gameplay::RuntimeTurret::name);
    REQUIRE(turret != session.combat.turrets.end());
    auto player = RelayCore::gameplay::PlayerCombatState{};
    auto test_turrets = std::vector{*turret};
    for (int i = 0; i < 180; ++i)
    {
        RelayCore::gameplay::tick_combat_state(1.0F / 60, player, test_turrets);
        (void) RelayCore::gameplay::update_combat_threats(
            RelayCore::test::WorldQueryFixture{session.level, session.doors},
            RelayCore::physics::make_player_aabb({-4, .01F, 13.8F}, .3F, 1.8F),
            {-4, 1.65F, 13.8F},
            player,
            test_turrets,
            {},
            1.0F / 60
        );
    }
    CHECK(player.health == player.max_health);
    for (int i = 0; i < 180; ++i)
    {
        RelayCore::gameplay::tick_combat_state(1.0F / 60, player, test_turrets);
        (void) RelayCore::gameplay::update_combat_threats(
            RelayCore::test::WorldQueryFixture{session.level, session.doors},
            RelayCore::physics::make_player_aabb({-2, .01F, 17}, .3F, 1.8F),
            {-2, 1.65F, 17},
            player,
            test_turrets,
            {},
            1.0F / 60
        );
    }
    CHECK(player.health < player.max_health);
}
