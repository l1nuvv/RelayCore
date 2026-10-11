#include "relaycore/core/paths.h"
#include "relaygame/game_world_runtime.h"
#include "relaygame/usable_interaction.h"
#include "relaygame/world_audio_zones.h"

#include <algorithm>
#include <doctest/doctest.h>
#include <glm/geometric.hpp>

#include "scoped_temp_directory.h"
#include "scoped_test_logger.h"

TEST_CASE("two sectors route carries the key starts the pumps and preserves continue and retry")
{
    RelayCore::test::ScopedTempDirectory temp;
    RelayCore::test::ScopedTestLogger logger{temp.path() / "logs"};
    const RelayCore::core::EngineConfig config;
    const RelayGame::LevelSessionBuilder builder{config.gameplay};
    auto session = builder.build(RelayCore::core::asset_path("levels/foundation_intake.toml"));
    REQUIRE(
        std::ranges::count(
            session.usables,
            RelayCore::gameplay::UsableVisualKind::Pickup,
            &RelayCore::gameplay::RuntimeUsable::visual_kind
        )
        == 1
    );
    REQUIRE(session.usables.size() == 2);
    RelayGame::MissionRuntime mission;
    mission.reset(session.mission_definition);
    auto position = session.spawn_position;
    RelayCore::gameplay::PlayerCombatState player;
    std::vector<std::string> inventory, activated, signaled;
    std::vector<RelayCore::physics::WorldCollider> doors, actors, scratch;
    std::uint64_t ticks = 0;
    const auto world = [&] {
        RelayCore::gameplay::collect_blocking_doors(session.doors, doors);
        return RelayCore::physics::WorldQuery{session.world_colliders, doors};
    };
    const auto tick = [&] {
        constexpr float dt = 1.0F / 120;
        ++ticks;
        const auto bounds = RelayCore::physics::make_player_aabb(position, .3F, 1.8F);
        const auto eye = position + glm::vec3{0, 1.65F, 0};
        RelayCore::gameplay::tick_combat_state(dt, player, session.combat.turrets);
        static_cast<void>(RelayCore::gameplay::update_doors(dt, bounds, session.doors));
        static_cast<void>(RelayCore::gameplay::update_combat_threats(
            world(),
            bounds,
            eye,
            player,
            session.combat.turrets,
            session.combat.hazards,
            dt
        ));
        static_cast<void>(RelayGame::update_enemies(world(), bounds, eye, player, session.enemies, dt, scratch));
        glm::vec3 target{};
        float nearest = player.weapon_range;
        for (const auto& enemy : session.enemies)
            if (enemy.state.mode != RelayGame::EnemyMode::Dead)
            {
                const auto center = enemy.state.position + glm::vec3{0, .45F, 0};
                if (glm::distance(eye, center) < nearest)
                {
                    nearest = glm::distance(eye, center);
                    target = center;
                }
            }
        for (const auto& turret : session.combat.turrets)
            if (!turret.destroyed && glm::distance(eye, turret.origin) < nearest)
            {
                nearest = glm::distance(eye, turret.origin);
                target = turret.origin;
            }
        if (nearest < player.weapon_range && !world().raycast(eye, glm::normalize(target - eye), nearest))
            static_cast<void>(RelayGame::fire_player_weapon(
                world(),
                eye,
                glm::normalize(target - eye),
                player,
                session.combat.turrets,
                session.enemies,
                RelayGame::PlayerWeapon::Precise
            ));
        mission.update(bounds, session.doors, inventory, activated, signaled);
    };
    const auto walk = [&](glm::vec3 destination) {
        for (int i = 0; i < 3000 && glm::distance(position, destination) > .06F; ++i)
        {
            actors.clear();
            for (const auto& enemy : session.enemies)
                if (enemy.state.mode != RelayGame::EnemyMode::Dead)
                    actors.push_back(
                        {.bounds = RelayGame::enemy_bounds(enemy.state),
                         .object = {RelayCore::physics::WorldObjectKind::Enemy}}
                    );
            auto delta = destination - position;
            delta.y = 0;
            position = RelayCore::physics::move_and_collide(
                           world().with_actors(actors),
                           position,
                           glm::normalize(delta) * 3.0F + glm::vec3{0, -1, 0},
                           1.0F / 120,
                           {},
                           true
            )
                           .position;
            tick();
            REQUIRE(player.alive);
        }
        CAPTURE(position.x);
        CAPTURE(position.z);
        CAPTURE(destination.x);
        CAPTURE(destination.z);
        REQUIRE(glm::distance(position, destination) < .07F);
        REQUIRE(RelayCore::physics::probe_ground(world(), position, {}));
    };
    const auto use = [&](std::string_view name) {
        const auto found = std::ranges::find(session.usables, name, &RelayCore::gameplay::RuntimeUsable::name);
        REQUIRE(found != session.usables.end());
        const auto eye = position + glm::vec3{0, 1.65F, 0};
        const auto focus = RelayCore::gameplay::find_focused_usable(
            eye,
            glm::normalize(found->origin - eye),
            session.usables,
            2.25F,
            .75F,
            world()
        );
        REQUIRE(focus.usable_index == found - session.usables.begin());
        const auto result =
            RelayGame::apply_usable_interaction(*found, session.doors, session.usables, inventory, activated, signaled);
        tick();
        return result.kind;
    };
    walk({1.3F, .01F, 1.5F});
    CHECK(use("intake_button") == RelayCore::gameplay::UseInteractionOutcome::MissingRequiredItem);
    walk({0, .01F, -1});
    walk({-2, .01F, -1});
    CHECK(use("intake_key") == RelayCore::gameplay::UseInteractionOutcome::PickedUpItem);
    walk({0, .01F, -1});
    walk({1.3F, .01F, 1.5F});
    CHECK(use("intake_button") == RelayCore::gameplay::UseInteractionOutcome::ActivatedTarget);
    for (int i = 0; i < 180; ++i)
        tick();
    walk({0, .01F, 1.5F});
    walk({0, .01F, 4.5F});
    REQUIRE(mission.completed());
    REQUIRE(session.exits.size() == 1);
    CHECK(session.exits[0].entry == "arrival");
    CHECK(player.kills == 1);

    session = builder.build(RelayCore::core::asset_path("levels/foundation_pump.toml"));
    REQUIRE(session.authoring_warnings.empty());
    mission.reset(session.mission_definition);
    activated.clear();
    signaled.clear();
    position = session.spawn_position;
    REQUIRE(inventory == std::vector<std::string>{"service_key"});
    walk({0, .01F, .7F});
    walk({-1.5F, .01F, 1.5F});
    inventory.clear();
    CHECK(use("pump_control") == RelayCore::gameplay::UseInteractionOutcome::MissingRequiredItem);
    inventory.push_back("service_key");
    CHECK(use("pump_control") == RelayCore::gameplay::UseInteractionOutcome::ActivatedTarget);
    CHECK(session.doors[0].state == RelayCore::gameplay::DoorState::Closed);
    const auto continue_at_pump = [&] {
        RelayGame::SavedGame save;
        save.world = RelayGame::capture_runtime_level_snapshot(
            session.level_path.string(),
            session.runtime_registry,
            session.doors,
            session.triggers,
            session.usables,
            session.combat.turrets,
            inventory,
            activated,
            signaled,
            player,
            mission,
            ticks
        );
        save.player.position = position;
        save.player.grounded = true;
        save.level_fingerprint = RelayGame::fingerprint_level(session.level);
        for (const auto& enemy : session.enemies)
            save.enemies.push_back(enemy.state);
        const auto path = temp.path() / "pump-progress.toml";
        RelayGame::write_saved_game(path, save);
        RelayGame::GameWorldRuntime runtime;
        runtime.load_level_session(config, builder.build(session.level_path));
        runtime.restore_saved_game(RelayGame::load_saved_game(path));
        CHECK(runtime.inventory_item_ids() == inventory);
        CHECK(runtime.activated_target_ids() == activated);
        CHECK(runtime.mission_status().current_step == mission.status().current_step);
        const auto checkpoint = runtime.capture_encounter_snapshot(0, 0, false);
        runtime.restore_encounter_snapshot(checkpoint);
        CHECK(runtime.debug_player_position() == position);
        auto dead = runtime.capture_saved_game(0, false);
        dead.world.player_combat.alive = false;
        dead.world.player_combat.health = 0;
        dead.world.player_combat.respawn_remaining_seconds = 1;
        runtime.restore_saved_game(dead);
        REQUIRE(runtime.debug_advance_player_respawn(2));
        CHECK(runtime.inventory_item_ids() == inventory);
        CHECK(runtime.activated_target_ids() == activated);
    };
    continue_at_pump();
    walk({-1.4F, .01F, 4.7F});
    CHECK(use("pump_button") == RelayCore::gameplay::UseInteractionOutcome::ActivatedTarget);
    walk({0, .01F, 4.7F});
    for (int i = 0; i < 180; ++i)
        tick();
    walk({0, .01F, 8});
    CHECK(mission.completed());
    CHECK(player.kills == 3);
    CHECK(player.deaths == 0);
    CHECK(session.ending.has_value());
    continue_at_pump();
}

TEST_CASE("foundation control housings meet their mounting surfaces")
{
    RelayCore::test::ScopedTempDirectory temp;
    RelayCore::test::ScopedTestLogger logger{temp.path() / "logs"};
    for (const auto* map : {"levels/foundation_intake.toml", "levels/foundation_pump.toml"})
    {
        const auto session = RelayGame::LevelSessionBuilder{{}}.build(RelayCore::core::asset_path(map));
        const RelayCore::physics::WorldQuery world{session.world_colliders};
        for (const auto& usable : session.usables)
        {
            if (usable.visual_kind != RelayCore::gameplay::UsableVisualKind::Panel)
                continue;
            CAPTURE(map);
            CAPTURE(usable.name);
            const glm::vec3 normal = usable.panel_mount_axis == RelayCore::gameplay::PanelMountAxis::PositiveX
                                         ? glm::vec3{1, 0, 0}
                                         : glm::vec3{0, 0, -1};
            const auto center = (usable.visual_bounds.mins + usable.visual_bounds.maxs) * .5F;
            // The rendered housing extends 75 mm behind its model origin.
            const auto rear = center - normal * .075F;
            const auto contact = world.raycast(rear + normal * .02F, -normal, .04F);
            REQUIRE(contact.has_value());
            CHECK(contact->distance >= .01F);
            CHECK(contact->distance <= .03F);
        }
    }
}

TEST_CASE("pump shelter blocks turret fire while the rover retains a clear approach")
{
    RelayCore::test::ScopedTempDirectory temp;
    RelayCore::test::ScopedTestLogger logger{temp.path() / "logs"};
    const auto session =
        RelayGame::LevelSessionBuilder{{}}.build(RelayCore::core::asset_path("levels/foundation_pump.toml"));
    const RelayCore::physics::WorldQuery world{session.world_colliders};
    REQUIRE(session.combat.turrets.size() == 1);
    const auto origin = session.combat.turrets[0].origin;
    const auto rear = origin + glm::vec3{0, 0, .78F * .48F};
    CHECK(std::ranges::any_of(session.level.solids, [&](const auto& solid) {
        return glm::all(glm::greaterThanEqual(rear, solid.mins + glm::vec3{.005F}))
               && glm::all(glm::lessThanEqual(rear, solid.maxs - glm::vec3{.005F}));
    }));
    const auto blocked = [&](glm::vec3 position) {
        const auto delta = origin - position;
        return world.raycast(position, glm::normalize(delta), glm::length(delta)).has_value();
    };
    CHECK(blocked({0, 1.65F, -1}));
    CHECK_FALSE(blocked({-1.5F, 1.65F, 1.5F}));
    auto enemies = session.enemies;
    RelayCore::gameplay::PlayerCombatState player;
    std::vector<RelayCore::physics::WorldCollider> scratch;
    const glm::vec3 position{0, .01F, -1};
    for (int i = 0; i < 600 && enemies[0].state.mode != RelayGame::EnemyMode::Windup; ++i)
        static_cast<void>(RelayGame::update_enemies(
            world,
            RelayCore::physics::make_player_aabb(position, .3F, 1.8F),
            position + glm::vec3{0, 1.65F, 0},
            player,
            enemies,
            1.0F / 120,
            scratch
        ));
    CHECK(enemies[0].state.mode == RelayGame::EnemyMode::Windup);
}
