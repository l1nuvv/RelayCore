#include "relaycore/core/engine_config.h"
#include "relaycore/physics/collision.h"
#include "relaygame/game_world_runtime.h"
#include "relaygame/level_editor_document.h"
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
    auto map_path() -> std::filesystem::path
    {
        return std::filesystem::path{__FILE__}.parent_path().parent_path() / "assets/levels/pressure_transfer.toml";
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

    struct PressureRoute {
        RelayGame::LevelSession session;
        RelayGame::MissionRuntime mission;
        RelayCore::gameplay::PlayerCombatState player;
        std::vector<std::string> inventory, activated, signaled;
        std::vector<RelayCore::physics::WorldCollider> enemy_scratch;
        glm::vec3 position;
        unsigned precise_shots = 0, shotgun_shots = 0;

        explicit PressureRoute(const RelayCore::core::GameplayConfig& config) :
            session(RelayGame::LevelSessionBuilder{config}.build(map_path())), position(session.spawn_position)
        {
            mission.reset(session.mission_definition);
        }

        void tick(bool fire = true)
        {
            constexpr float dt = 1.0F / 120;
            const auto actor = RelayCore::physics::make_player_aabb(position, .3F, 1.8F);
            const auto eye = position + glm::vec3{0, 1.65F, 0};
            RelayCore::gameplay::tick_combat_state(dt, player, session.combat.turrets);
            static_cast<void>(RelayCore::gameplay::update_doors(dt, actor, session.doors));
            static_cast<void>(RelayCore::gameplay::update_combat_threats(
                RelayCore::test::WorldQueryFixture{session.level, session.doors},
                actor,
                eye,
                player,
                session.combat.turrets,
                session.combat.hazards,
                dt
            ));
            static_cast<void>(RelayGame::update_enemies(
                RelayCore::test::WorldQueryFixture{session.level, session.doors},
                actor,
                eye,
                player,
                session.enemies,
                dt,
                enemy_scratch
            ));
            if (fire)
            {
                glm::vec3 target{};
                float nearest = 16;
                bool nearest_enemy = false;
                for (const auto& enemy : session.enemies)
                    if (enemy.state.mode != RelayGame::EnemyMode::Dead)
                    {
                        const auto center = enemy.state.position + glm::vec3{0, .45F, 0};
                        const float distance = glm::distance(eye, center);
                        if (distance < nearest)
                        {
                            nearest = distance;
                            target = center;
                            nearest_enemy = true;
                        }
                    }
                for (const auto& turret : session.combat.turrets)
                    if (!turret.destroyed && glm::distance(eye, turret.origin) < nearest)
                    {
                        nearest = glm::distance(eye, turret.origin);
                        target = turret.origin;
                        nearest_enemy = false;
                    }
                if (nearest < 16 && (!nearest_enemy || nearest < 4))
                {
                    const auto weapon = nearest < 4 && !inventory.empty() ? RelayGame::PlayerWeapon::Shotgun
                                                                          : RelayGame::PlayerWeapon::Precise;
                    const auto shot = RelayGame::fire_player_weapon(
                        RelayCore::test::WorldQueryFixture{session.level, session.doors},
                        eye,
                        glm::normalize(target - eye),
                        player,
                        session.combat.turrets,
                        session.enemies,
                        weapon
                    );
                    if (shot.combat.fired)
                        weapon == RelayGame::PlayerWeapon::Shotgun ? ++shotgun_shots : ++precise_shots;
                }
            }
            mission.update(actor, session.doors, inventory, activated, signaled);
        }

        auto use(std::string_view name) -> RelayCore::gameplay::UseInteractionOutcome
        {
            const auto found = std::ranges::find(session.usables, name, &RelayCore::gameplay::RuntimeUsable::name);
            REQUIRE(found != session.usables.end());
            const auto world = blockers(session);
            const auto eye = position + glm::vec3{0, 1.65F, 0};
            const auto focus = RelayCore::gameplay::find_focused_usable(
                eye,
                glm::normalize(found->origin - eye),
                session.usables,
                2.25F,
                .75F,
                RelayCore::test::WorldQueryFixture{world}
            );
            REQUIRE(focus.usable_index == found - session.usables.begin());
            const auto event = RelayGame::apply_usable_interaction(
                *found,
                session.doors,
                session.usables,
                inventory,
                activated,
                signaled
            );
            tick();
            return event.kind;
        }

        void walk_to(glm::vec3 destination)
        {
            for (int i = 0;
                 i < 4000
                 && glm::distance(glm::vec2{position.x, position.z}, glm::vec2{destination.x, destination.z}) > .06F;
                 ++i)
            {
                auto world = blockers(session);
                for (const auto& enemy : session.enemies)
                    if (enemy.state.mode != RelayGame::EnemyMode::Dead)
                        world.push_back(RelayGame::enemy_bounds(enemy.state));
                auto delta = destination - position;
                delta.y = 0;
                const auto velocity = glm::normalize(delta) * 3.0F + glm::vec3{0, -1, 0};
                position = RelayCore::physics::move_and_collide(
                               RelayCore::test::WorldQueryFixture{world},
                               position,
                               velocity,
                               1.0F / 120,
                               {},
                               true
                )
                               .position;
                tick();
                REQUIRE(player.alive);
            }
            CAPTURE(destination.x);
            CAPTURE(destination.z);
            CAPTURE(position.x);
            CAPTURE(position.z);
            REQUIRE(glm::distance(glm::vec2{position.x, position.z}, glm::vec2{destination.x, destination.z}) < .07F);
            CHECK(RelayCore::physics::probe_ground(RelayCore::test::WorldQueryFixture{blockers(session)}, position, {})
            );
        }
    };
} // namespace

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
        route.walk_to({2, .01F, 5.3F});
        CHECK(route.use("transfer_shotgun") == RelayCore::gameplay::UseInteractionOutcome::PickedUpItem);
        route.walk_to({6.1F, .01F, 8.5F});
        CHECK(route.use("service_reader") == RelayCore::gameplay::UseInteractionOutcome::ActivatedTarget);
        route.walk_to({4, .01F, 8});
        for (int i = 0; i < 270; ++i)
            route.tick();
        route.walk_to({4, .01F, 22});
        route.walk_to({10, .01F, 22});
        route.walk_to({10, .01F, 25});
        route.walk_to({18, .01F, 25});
        route.walk_to({18, .01F, 22});
        route.walk_to({24, .01F, 22});
        route.walk_to({30, .01F, 22});
        route.walk_to({30, .01F, 28});
        route.walk_to({28, .01F, 29});
        route.walk_to({28, .01F, 40});
        route.walk_to({33, .01F, 40});
        route.walk_to({38, .01F, 40});
        CHECK(route.session.doors[1].state == RelayCore::gameplay::DoorState::Closed);
        CHECK(route.use("transfer_control") == RelayCore::gameplay::UseInteractionOutcome::ActivatedTarget);
        route.walk_to({31, .01F, 42.5F});
        CHECK(route.use("exit_reader") == RelayCore::gameplay::UseInteractionOutcome::ActivatedTarget);
        route.walk_to({28, .01F, 42});
        for (int i = 0; i < 270; ++i)
            route.tick();
        route.walk_to({28, .01F, 49});
        CHECK(route.mission.completed());
        CHECK(route.player.kills == 4);
        CHECK(route.player.deaths == 0);
        CHECK(route.precise_shots > 0);
        CHECK(route.shotgun_shots > 0);
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
        CHECK(runtime.inventory_item_ids() == checkpoint.game.world.inventory_item_ids);
        CHECK(runtime.activated_target_ids() == checkpoint.game.world.activated_target_ids);
        CHECK(runtime.selected_weapon() == checkpoint.game.world.selected_weapon);
        runtime.restore_encounter_snapshot(checkpoint);
        CHECK(runtime.player_combat_state().deaths == checkpoint.game.world.player_combat.deaths);
        CHECK(runtime.doors()[1].state == checkpoint.game.world.doors[1].state);
    }
}
