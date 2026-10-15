#include "relaycore/core/paths.h"
#include "relaycore/render/camera.h"
#include "relaycore/render/static_model.h"
#include "relaygame/enemy_navigation.h"
#include "relaygame/enemy_runtime.h"
#include "relaygame/game_world_runtime.h"
#include "relaygame/level_editor_document.h"
#include "relaygame/player_feedback.h"

#include <doctest/doctest.h>

#include "scoped_temp_directory.h"
#include "scoped_test_logger.h"
#include "world_query_fixture.h"

namespace
{
    auto room() -> RelayCore::BlockoutLevel
    {
        RelayCore::BlockoutLevel level;
        level.solids.push_back({{-10, -.2F, -10}, {10, 0, 10}, RelayCore::SurfaceMaterial::Tile});
        RelayCore::EntitySpawnData entity;
        entity.classname = "enemy_rover";
        entity.origin = {0, .01F, 3};
        level.entities.push_back(entity);
        return level;
    }
} // namespace

TEST_CASE("routed rover reaches remembered player around cover that stops direct pursuit")
{
    for (const bool routed : {false, true})
    {
        CAPTURE(routed);
        auto level = room();
        level.solids.push_back({{-.8F, 0, 1.2F}, {.8F, 2, 2}, RelayCore::SurfaceMaterial::Metal});
        auto enemies = RelayGame::build_enemies(level);
        if (routed)
            enemies[0].route_points = {{2, .01F, 3}, {2, .01F, .5F}};
        enemies[0].state.mode = RelayGame::EnemyMode::Chasing;
        enemies[0].state.last_seen = {0, .01F, 0};
        RelayCore::gameplay::PlayerCombatState player;
        std::vector<RelayCore::physics::WorldCollider> scratch;
        const RelayCore::test::WorldQueryFixture world{level};
        RelayGame::prepare_enemy_navigation(world, enemies);
        for (int i = 0; i < 1200; ++i)
            RelayGame::update_enemies(
                world,
                RelayCore::physics::make_player_aabb({0, .01F, 0}, .3F, 1.8F),
                {0, 1.66F, 0},
                player,
                enemies,
                1.F / 120,
                scratch
            );
        if (routed)
        {
            CHECK(enemies[0].state.position.z < 1.15F);
            CHECK(player.health < 100);
        } else
        {
            CHECK(enemies[0].state.position.z >= 2.419F);
            CHECK(player.health == 100);
        }
    }
}

TEST_CASE("routed enemies respect closed doors unreachable heights and reopen without stale paths")
{
    auto level = room();
    auto enemies = RelayGame::build_enemies(level);
    enemies[0].route_points = {{2, .01F, 3}, {2, .01F, .5F}};
    enemies[0].state.mode = RelayGame::EnemyMode::Chasing;
    enemies[0].state.last_seen = {0, .01F, 0};
    RelayCore::test::WorldQueryFixture world{level};
    world.door_colliders.push_back(
        {.bounds = {{-10, 0, 1.2F}, {10, 3, 1.4F}}, .object = {RelayCore::physics::WorldObjectKind::Door}}
    );
    RelayGame::prepare_enemy_navigation(world, enemies);
    RelayCore::gameplay::PlayerCombatState player;
    std::vector<RelayCore::physics::WorldCollider> scratch;
    const auto tick = [&] {
        RelayGame::update_enemies(
            world,
            RelayCore::physics::make_player_aabb({0, .01F, 0}, .3F, 1.8F),
            {0, 1.66F, 0},
            player,
            enemies,
            1.F / 120,
            scratch
        );
    };
    for (int i = 0; i < 600; ++i)
        tick();
    CHECK(enemies[0].state.position.z == doctest::Approx(3));
    CHECK(player.health == 100);
    world.door_colliders.clear();
    for (int i = 0; i < 400; ++i)
        tick();
    CHECK(player.health < 100);
    enemies[0].state.position = {0, .01F, 3};
    enemies[0].state.mode = RelayGame::EnemyMode::Chasing;
    enemies[0].state.last_seen = {0, 4, 0};
    enemies[0].detection_range = .1F;
    enemies[0].route_replan_seconds = 0;
    for (int i = 0; i < 600; ++i)
        tick();
    CHECK(enemies[0].state.position.z == doctest::Approx(3));
}

TEST_CASE("prepared route links match live planning after doors guardian endpoints and level changes")
{
    auto level = room();
    level.solids.push_back({{-.8F, 0, 1.2F}, {.8F, 2, 2}, RelayCore::SurfaceMaterial::Metal});
    auto enemies = RelayGame::build_enemies(level);
    auto& enemy = enemies[0];
    enemy.route_points = {{2, .01F, 3}, {2, .01F, .5F}};
    RelayCore::test::WorldQueryFixture fixture{level};
    RelayGame::prepare_enemy_navigation(fixture, enemies);
    const glm::vec3 goal{0, .01F, 0};
    const auto compare = [&](RelayCore::physics::WorldQuery world, glm::vec3 target, bool reachable) {
        auto live = enemy;
        live.route_links_prepared = false;
        live.route_replan_seconds = enemy.route_replan_seconds = 0;
        CHECK(
            RelayGame::enemy_route_velocity(world, enemy, target, 1.65F, 1.F / 120)
            == RelayGame::enemy_route_velocity(world, live, target, 1.65F, 1.F / 120)
        );
        REQUIRE(enemy.route_count == live.route_count);
        CHECK((enemy.route_count > 0) == reachable);
        for (std::size_t i = 0; i < enemy.route_count; ++i)
            CHECK(enemy.route[i] == live.route[i]);
    };
    compare(fixture, goal, true);
    fixture.door_colliders.push_back(
        {.bounds = {{1, 0, 1.2F}, {10, 3, 1.4F}}, .object = {RelayCore::physics::WorldObjectKind::Door}}
    );
    compare(fixture, goal, false);
    fixture.door_colliders.clear();
    compare(fixture, goal, true);
    const std::array guardian{RelayCore::physics::WorldCollider{
        .bounds = {{1, 0, 1.2F}, {10, 3, 1.4F}},
        .object = {RelayCore::physics::WorldObjectKind::Enemy}
    }};
    compare(RelayCore::physics::WorldQuery{fixture}.with_actors(guardian), goal, false);
    compare(fixture, goal, true);
    compare(fixture, {0, 4, 0}, false);
    enemy.state.position = {2, .01F, .5F};
    compare(fixture, goal, true);
    fixture.colliders.push_back(
        {.bounds = {{1, 0, 1.2F}, {10, 3, 1.4F}}, .object = {RelayCore::physics::WorldObjectKind::Solid}}
    );
    enemy.state.position = level.entities[0].origin;
    RelayGame::prepare_enemy_navigation(fixture, enemies);
    compare(fixture, goal, false);
}

TEST_CASE("prepared route links revalidate ground supplied by a dynamic body")
{
    auto level = room();
    level.solids = {
        {{-10, -.2F, -10}, {10, 0, 1}, RelayCore::SurfaceMaterial::Tile},
        {{-10, -.2F, 2.3F}, {10, 0, 10}, RelayCore::SurfaceMaterial::Tile}
    };
    auto enemies = RelayGame::build_enemies(level);
    auto& enemy = enemies[0];
    enemy.route_points = {{0, .01F, 3}, {0, .01F, .5F}};
    const RelayCore::test::WorldQueryFixture fixture{level};
    RelayGame::prepare_enemy_navigation(fixture, enemies);
    const std::array bridge{RelayCore::physics::WorldCollider{
        .bounds = {{-10, -.2F, 1}, {10, 0, 2.3F}},
        .object = {RelayCore::physics::WorldObjectKind::Enemy}
    }};
    for (const bool supported : {false, true, false})
    {
        CAPTURE(supported);
        const auto world = RelayCore::physics::WorldQuery{fixture}.with_actors(
            supported ? std::span<const RelayCore::physics::WorldCollider>{bridge}
                      : std::span<const RelayCore::physics::WorldCollider>{}
        );
        auto live = enemy;
        live.route_links_prepared = false;
        live.route_replan_seconds = enemy.route_replan_seconds = 0;
        CHECK(
            RelayGame::enemy_route_velocity(world, enemy, {}, 1.65F, 1.F / 120)
            == RelayGame::enemy_route_velocity(world, live, {}, 1.65F, 1.F / 120)
        );
        CHECK((enemy.route_count > 0) == supported);
    }
}

TEST_CASE("mobile shooter locks warning aim and respects movement doors and intervening bodies")
{
    for (int reaction = 0; reaction < 4; ++reaction)
    {
        CAPTURE(reaction);
        auto level = room();
        auto enemies = RelayGame::build_enemies(level);
        enemies[0].ranged_attack = true;
        enemies[0].route_points = {{2, .01F, 3}};
        RelayCore::test::WorldQueryFixture world{level};
        RelayCore::gameplay::PlayerCombatState player;
        std::vector<RelayCore::physics::WorldCollider> scratch;
        glm::vec3 position{0, .01F, 0};
        const auto tick = [&](float dt) {
            return RelayGame::update_enemies(
                world,
                RelayCore::physics::make_player_aabb(position, .3F, 1.8F),
                position + glm::vec3{0, 1.65F, 0},
                player,
                enemies,
                dt,
                scratch
            );
        };
        REQUIRE(tick(.01F).windups == 1);
        const auto locked = enemies[0].state.last_seen;
        CHECK(tick(0).shots == 0);
        CHECK(enemies[0].state.timer == doctest::Approx(.7F));
        if (reaction == 1)
            position.x = 2;
        if (reaction == 2)
            world.door_colliders.push_back(
                {.bounds = {{-10, 0, 1}, {10, 3, 1.2F}}, .object = {RelayCore::physics::WorldObjectKind::Door}}
            );
        if (reaction == 3)
        {
            auto blocker = enemies[0];
            blocker.ranged_attack = false;
            blocker.state.position.z = 1.5F;
            blocker.state.mode = RelayGame::EnemyMode::Recovery;
            blocker.state.timer = 1;
            enemies.push_back(blocker);
        }
        unsigned shots = 0;
        for (int i = 0; i < 85; ++i)
            shots += tick(1.F / 120).shots;
        CHECK(shots == 1);
        CHECK(enemies[0].state.last_seen == locked);
        CHECK(player.health == (reaction == 0 ? 88 : 100));
        CHECK(enemies[0].state.mode == RelayGame::EnemyMode::Recovery);
        player.alive = false;
        const auto timer = enemies[0].state.timer;
        CHECK(tick(.2F).shots == 0);
        CHECK(enemies[0].state.timer == timer);
    }
}

TEST_CASE("mobile shooter searches remembered position without learning an unseen player location")
{
    auto level = room();
    level.solids.push_back({{-.8F, 0, 1.2F}, {.8F, 2, 2}, RelayCore::SurfaceMaterial::Metal});
    auto enemies = RelayGame::build_enemies(level);
    enemies[0].ranged_attack = true;
    enemies[0].route_points = {{2.5F, .01F, 3}, {2.5F, .01F, .5F}};
    enemies[0].detection_range = .1F;
    enemies[0].state.mode = RelayGame::EnemyMode::Chasing;
    enemies[0].state.last_seen = {0, .01F, 0};
    RelayCore::gameplay::PlayerCombatState player;
    std::vector<RelayCore::physics::WorldCollider> scratch;
    const RelayCore::test::WorldQueryFixture world{level};
    RelayGame::prepare_enemy_navigation(world, enemies);
    for (int i = 0; i < 1200; ++i)
        RelayGame::update_enemies(
            world,
            RelayCore::physics::make_player_aabb({-6, .01F, 0}, .3F, 1.8F),
            {-6, 1.66F, 0},
            player,
            enemies,
            1.F / 120,
            scratch
        );
    CHECK(glm::distance(enemies[0].state.position, glm::vec3{0, .01F, 0}) < .15F);
    CHECK(enemies[0].state.last_seen == glm::vec3{0, .01F, 0});
    CHECK(enemies[0].state.mode == RelayGame::EnemyMode::Idle);
    CHECK(player.health == 100);
}

TEST_CASE("mobile shooter finds a reachable firing angle after player disappears behind cover")
{
    auto level = room();
    level.solids.push_back({{-.8F, 0, 1.2F}, {.8F, 2, 2}, RelayCore::SurfaceMaterial::Metal});
    auto enemies = RelayGame::build_enemies(level);
    enemies[0].ranged_attack = true;
    enemies[0].route_points = {{2, .01F, 3}, {2, .01F, .5F}};
    enemies[0].state.mode = RelayGame::EnemyMode::Chasing;
    enemies[0].state.last_seen = {0, .01F, 0};
    RelayCore::gameplay::PlayerCombatState player;
    std::vector<RelayCore::physics::WorldCollider> scratch;
    const RelayCore::test::WorldQueryFixture world{level};
    RelayGame::prepare_enemy_navigation(world, enemies);
    for (int i = 0; i < 600; ++i)
        RelayGame::update_enemies(
            world,
            RelayCore::physics::make_player_aabb({0, .01F, 0}, .3F, 1.8F),
            {0, 1.66F, 0},
            player,
            enemies,
            1.F / 120,
            scratch
        );
    CHECK(std::abs(enemies[0].state.position.x) > 1);
    CHECK(player.health < 100);
}

TEST_CASE("mobile shooter reaches firing range instead of stalling at a boundary route point")
{
    for (const bool starts_at_point : {false, true})
    {
        CAPTURE(starts_at_point);
        auto level = room();
        auto enemies = RelayGame::build_enemies(level);
        auto& enemy = enemies[0];
        enemy.ranged_attack = true;
        enemy.detection_range = 20;
        enemy.state.position.z = starts_at_point ? 3.F : 4.F;
        enemy.route_points = {{0, .01F, 3}};
        const glm::vec3 position{0, .01F, starts_at_point ? -8.98F : -8.9F};
        RelayCore::gameplay::PlayerCombatState player;
        std::vector<RelayCore::physics::WorldCollider> scratch;
        const RelayCore::test::WorldQueryFixture world{level};
        RelayGame::prepare_enemy_navigation(world, enemies);
        unsigned shots = 0;
        for (int i = 0; i < 480; ++i)
            shots += RelayGame::update_enemies(
                         world,
                         RelayCore::physics::make_player_aabb(position, .3F, 1.8F),
                         position + glm::vec3{0, 1.65F, 0},
                         player,
                         enemies,
                         1.F / 120,
                         scratch
            )
                         .shots;
        CHECK(shots > 0);
        CHECK(player.health < 100);
    }
}

TEST_CASE("enemy routes reject missing oversized and nonflat authoring")
{
    auto level = room();
    level.entities[0].classname = "enemy_shooter";
    CHECK_THROWS_WITH(static_cast<void>(RelayGame::build_enemies(level)), "enemy_shooter 0: route is required");
    level.entities[0].properties["route"] = "arena";
    CHECK_THROWS_WITH(
        static_cast<void>(RelayGame::build_enemies(level)),
        "enemy_shooter 0: route needs 1 to 16 points"
    );
    RelayCore::EntitySpawnData point;
    point.classname = "enemy_route_point";
    point.properties["route"] = "arena";
    point.origin = {2, 4, 3};
    level.entities.push_back(point);
    CHECK_THROWS_WITH(static_cast<void>(RelayGame::build_enemies(level)), "enemy_shooter 0: route must be flat");
    level.entities[1].origin.y = .01F;
    CHECK(RelayGame::build_enemies(level).size() == 1);
    level.entities[0].properties["attack_style"] = "lunge";
    CHECK_THROWS_WITH(
        static_cast<void>(RelayGame::build_enemies(level)),
        "enemy_shooter 0: attack_style is not supported"
    );
    level.entities[0].properties.erase("attack_style");
    for (int i = 0; i < 16; ++i)
        level.entities.push_back(level.entities[1]);
    CHECK_THROWS_WITH(
        static_cast<void>(RelayGame::build_enemies(level)),
        "enemy_shooter 0: route needs 1 to 16 points"
    );
}

TEST_CASE("chapter rover close attack outlasts stationary pistol fire while shotgun or lateral dodge stops it")
{
    for (int choice = 0; choice < 3; ++choice)
    {
        CAPTURE(choice);
        auto level = room();
        level.entities[0].origin.z = 1.6F;
        level.entities[0].properties = {{"health", "84"}, {"speed", "2.6"}, {"attack_style", "lunge"}};
        auto enemies = RelayGame::build_enemies(level);
        RelayCore::gameplay::PlayerCombatState player;
        glm::vec3 position{0, .01F, 0};
        const RelayCore::test::WorldQueryFixture world{level};
        std::vector<RelayCore::physics::WorldCollider> scratch;
        for (int i = 0; i < 100; ++i)
        {
            RelayCore::gameplay::tick_combat_state(1.F / 120, player, {});
            if (choice == 2 && i > 15 && i < 65)
                position.x += 3.F / 120;
            const auto eye = position + glm::vec3{0, .6F, 0};
            static_cast<void>(RelayGame::fire_player_weapon(
                world,
                eye,
                enemies[0].state.position + glm::vec3{0, .6F, 0} - eye,
                player,
                {},
                enemies,
                choice == 1 ? RelayGame::PlayerWeapon::Shotgun : RelayGame::PlayerWeapon::Precise
            ));
            RelayGame::update_enemies(
                world,
                RelayCore::physics::make_player_aabb(position, .3F, 1.8F),
                position + glm::vec3{0, 1.65F, 0},
                player,
                enemies,
                1.F / 120,
                scratch
            );
        }
        CHECK(enemies[0].state.mode == RelayGame::EnemyMode::Dead);
        CHECK(player.health == (choice == 0 ? 86 : 100));
    }
}

TEST_CASE("rover detects pursues and attacks at a bounded cadence")
{
    auto level = room();
    auto enemies = RelayGame::build_enemies(level);
    RelayCore::gameplay::PlayerCombatState player;
    std::vector<RelayCore::physics::WorldCollider> scratch;
    const auto actor = RelayCore::physics::make_player_aabb({0, .01F, 0}, .3F, 1.8F);
    for (int i = 0; i < 360; ++i)
        RelayGame::update_enemies(
            RelayCore::test::WorldQueryFixture{level, {}},
            actor,
            {0, 1.6F, 0},
            player,
            enemies,
            1.F / 120,
            scratch
        );
    CHECK(enemies[0].state.position.z < 1.2F);
    CHECK(enemies[0].state.position.z >= .7F);
    CHECK(player.health == doctest::Approx(86));
    CHECK(enemies[0].state.position.y >= -.002F);
}

TEST_CASE("lunge rover telegraphs a committed attack that a lateral dodge avoids")
{
    for (const bool dodge : {false, true})
    {
        CAPTURE(dodge);
        auto level = room();
        level.entities[0].origin.z = 2.3F;
        level.entities[0].properties["speed"] = "2.3";
        level.entities[0].properties["attack_style"] = "lunge";
        auto enemies = RelayGame::build_enemies(level);
        RelayCore::gameplay::PlayerCombatState player;
        std::vector<RelayCore::physics::WorldCollider> scratch;
        glm::vec3 position{0, .01F, 0};
        const auto tick = [&] {
            return RelayGame::update_enemies(
                RelayCore::test::WorldQueryFixture{level, {}},
                RelayCore::physics::make_player_aabb(position, .3F, 1.8F),
                position + glm::vec3{0, 1.6F, 0},
                player,
                enemies,
                1.F / 120,
                scratch
            );
        };
        REQUIRE(tick().windups == 1);
        const auto aim = enemies[0].state.last_seen;
        if (dodge)
            position.x = 1.8F;
        for (int i = 0; i < 50; ++i)
        {
            CHECK(tick().strikes == 0);
            CHECK(player.health == 100);
        }
        CHECK(enemies[0].state.position.z == doctest::Approx(2.3F));
        unsigned strikes = 0;
        bool lunged = false;
        for (int i = 0; i < 45; ++i)
        {
            strikes += tick().strikes;
            lunged = lunged || enemies[0].state.mode == RelayGame::EnemyMode::Lunging;
        }
        CHECK(lunged);
        CHECK(strikes == 1);
        CHECK(player.health == (dodge ? 100 : 86));
        CHECK(enemies[0].state.mode == RelayGame::EnemyMode::Recovery);
        CHECK(enemies[0].state.position.x == doctest::Approx(0));
        CHECK(enemies[0].state.position.z < 1.15F);
        CHECK(aim == glm::vec3{0, .01F, 0});
        for (int i = 0; i < 60; ++i)
            CHECK(tick().strikes == 0);
    }
}

TEST_CASE("lunge rover cannot cross a newly closed door or retarget when hit")
{
    auto level = room();
    level.entities[0].origin.z = 2.3F;
    level.entities[0].properties["speed"] = "2.3";
    level.entities[0].properties["attack_style"] = "lunge";
    auto enemies = RelayGame::build_enemies(level);
    RelayCore::gameplay::PlayerCombatState player;
    std::vector<RelayCore::physics::WorldCollider> scratch;
    std::vector<RelayCore::gameplay::RuntimeDoor> doors;
    const auto tick = [&] {
        return RelayGame::update_enemies(
            RelayCore::test::WorldQueryFixture{level, doors},
            RelayCore::physics::make_player_aabb({0, .01F, 0}, .3F, 1.8F),
            {0, 1.6F, 0},
            player,
            enemies,
            1.F / 120,
            scratch
        );
    };
    REQUIRE(tick().windups == 1);
    const auto aim = enemies[0].state.last_seen;
    const glm::vec3 origin{1, .5F, 1};
    const auto hit = RelayGame::fire_player_weapon(
        RelayCore::test::WorldQueryFixture{level, doors},
        origin,
        enemies[0].state.position + glm::vec3{0, .45F, 0} - origin,
        player,
        {},
        enemies
    );
    REQUIRE(hit.enemy_index == 0);
    CHECK(enemies[0].state.last_seen == aim);
    CHECK(enemies[0].state.mode == RelayGame::EnemyMode::Windup);
    RelayCore::gameplay::RuntimeDoor door;
    door.closed_bounds = {{-4, 0, 1.4F}, {4, 3, 1.6F}};
    doors.push_back(door);
    for (int i = 0; i < 95; ++i)
        tick();
    CHECK(enemies[0].state.position.z >= 2.019F);
    CHECK(enemies[0].state.mode == RelayGame::EnemyMode::Recovery);
    CHECK(player.health == 100);
}

TEST_CASE("rover attack style rejects unsupported authored behavior")
{
    auto level = room();
    level.entities[0].properties["attack_style"] = "teleport";
    CHECK_THROWS_WITH(
        static_cast<void>(RelayGame::build_enemies(level)),
        "enemy_rover 0: attack_style must be melee or lunge"
    );
}

TEST_CASE("destroying a lunge rover during windup or the dash cancels its strike")
{
    for (const auto phase : {RelayGame::EnemyMode::Windup, RelayGame::EnemyMode::Lunging})
    {
        auto level = room();
        level.entities[0].origin.z = 2.3F;
        level.entities[0].properties["speed"] = "2.3";
        level.entities[0].properties["attack_style"] = "lunge";
        auto enemies = RelayGame::build_enemies(level);
        RelayCore::gameplay::PlayerCombatState player;
        std::vector<RelayCore::physics::WorldCollider> scratch;
        const auto tick = [&] {
            return RelayGame::update_enemies(
                RelayCore::test::WorldQueryFixture{level, {}},
                RelayCore::physics::make_player_aabb({0, .01F, 0}, .3F, 1.8F),
                {0, 1.6F, 0},
                player,
                enemies,
                1.F / 120,
                scratch
            );
        };
        for (int i = 0; i < 100 && enemies[0].state.mode != phase; ++i)
            tick();
        REQUIRE(enemies[0].state.mode == phase);
        const auto aim = enemies[0].state.last_seen;
        for (int i = 0; i < 3; ++i)
        {
            player.weapon_cooldown_remaining_seconds = 0;
            const auto hit = RelayGame::fire_player_weapon(
                RelayCore::test::WorldQueryFixture{level, {}},
                {0, .6F, 0},
                {0, 0, 1},
                player,
                {},
                enemies
            );
            REQUIRE(hit.enemy_index == 0);
            CHECK(enemies[0].state.last_seen == aim);
        }
        REQUIRE(enemies[0].state.mode == RelayGame::EnemyMode::Dead);
        for (int i = 0; i < 150; ++i)
            CHECK(tick().strikes == 0);
        CHECK(player.health == 100);
        CHECK(player.kills == 1);
    }
}

TEST_CASE("mobile actor contact preserves nearest target face and resolved damage")
{
    auto level = room();
    auto enemies = RelayGame::build_enemies(level);
    enemies.push_back(enemies[0]);
    enemies[0].state.position.z = 6;
    enemies[1].state.health = 9;
    RelayCore::gameplay::PlayerCombatState player;
    const auto attack = RelayGame::fire_player_weapon(
        RelayCore::test::WorldQueryFixture{level, {}},
        {0, .45F, 0},
        {.1F, 0, 1},
        player,
        {},
        enemies
    );
    REQUIRE(attack.ray_count == 1);
    REQUIRE(attack.rays[0].actor_contact);
    REQUIRE(attack.combat.actor_contact);
    const auto& contact = *attack.rays[0].actor_contact;
    CHECK(contact.kind == RelayCore::gameplay::CombatActorKind::Enemy);
    CHECK(contact.index == 1);
    CHECK(contact.position.z == doctest::Approx(2.58F));
    CHECK(contact.position.x == doctest::Approx(.258F));
    CHECK(contact.normal == glm::vec3{0, 0, -1});
    CHECK(glm::length(contact.direction) == doctest::Approx(1));
    CHECK(contact.direction.x / contact.direction.z == doctest::Approx(.1F));
    CHECK(contact.damage == 9);
    CHECK(player.damage_dealt == 9);
    CHECK(attack.killed_enemy);
    CHECK_FALSE(attack.rays[0].world_impact);
    CHECK(attack.combat.actor_contact->position == contact.position);
    CHECK(attack.combat.hit_distance == doctest::Approx(glm::length(contact.position - glm::vec3{0, .45F, 0})));
    CHECK(enemies[0].state.health == 60);
}

TEST_CASE("shotgun actor contacts retain multiple targets and sum actual damage")
{
    auto level = room();
    auto enemies = RelayGame::build_enemies(level);
    enemies.push_back(enemies[0]);
    enemies[0].state.position = {-.48F, .01F, 3};
    enemies[1].state.position = {.48F, .01F, 3};
    enemies[0].state.health = 5;
    enemies[1].state.health = 8;
    RelayCore::gameplay::PlayerCombatState player;
    const auto shot = RelayGame::fire_player_weapon(
        RelayCore::test::WorldQueryFixture{level, {}},
        {0, .45F, 0},
        {0, 0, 1},
        player,
        {},
        enemies,
        RelayGame::PlayerWeapon::Shotgun
    );
    std::array<bool, 2> hit{};
    float damage = 0;
    for (std::size_t i = 0; i < shot.ray_count; ++i)
        if (const auto& contact = shot.rays[i].actor_contact)
        {
            CHECK(contact->kind == RelayCore::gameplay::CombatActorKind::Enemy);
            REQUIRE(contact->index >= 0);
            REQUIRE(contact->index < 2);
            hit[static_cast<std::size_t>(contact->index)] = true;
            damage += contact->damage;
            CHECK(contact->position == shot.pellet_traces[i].end);
            CHECK_FALSE(shot.rays[i].world_impact);
        }
    CHECK(hit[0]);
    CHECK(hit[1]);
    CHECK(damage == 13);
    CHECK(damage == player.damage_dealt);
    CHECK(player.kills == 2);
}

TEST_CASE("actor hit feedback is bounded per discharge and expires without changing committed attacks")
{
    for (const auto weapon : {RelayGame::PlayerWeapon::Precise, RelayGame::PlayerWeapon::Shotgun})
        for (const bool ranged : {false, true})
        {
            auto level = room();
            auto enemies = RelayGame::build_enemies(level);
            auto& enemy = enemies[0];
            enemy.ranged_attack = ranged;
            enemy.state.health = 1000;
            enemy.state.mode = RelayGame::EnemyMode::Windup;
            enemy.state.timer = .5F;
            RelayCore::gameplay::PlayerCombatState player;
            const RelayCore::test::WorldQueryFixture world{level, {}};
            const auto fire = [&] {
                return RelayGame::fire_player_weapon(world, {0, .45F, 0}, {0, 0, 1}, player, {}, enemies, weapon);
            };
            const auto attack = fire();
            REQUIRE(attack.rays[0].actor_contact);
            CHECK(enemy.hit_feedback.remaining_seconds == doctest::Approx(.07F));
            CHECK(enemy.hit_feedback.position == attack.rays[0].actor_contact->position);
            CHECK(enemy.hit_feedback.normal == attack.rays[0].actor_contact->normal);
            CHECK(enemy.state.mode == RelayGame::EnemyMode::Windup);
            CHECK(enemy.state.timer == .5F);
            CHECK_FALSE(player.shot_trace.impact);
            for (std::size_t i = 0; i < attack.ray_count; ++i)
                if (attack.rays[i].actor_contact)
                    CHECK_FALSE(attack.pellet_traces[i].impact);
            std::vector<RelayCore::physics::WorldCollider> scratch;
            const auto tick = [&](float dt) {
                RelayGame::update_enemies(
                    world,
                    {{-.3F, 0, 0}, {.3F, 1.8F, .6F}},
                    {0, 1.6F, 0},
                    player,
                    enemies,
                    dt,
                    scratch
                );
            };
            tick(.03F);
            CHECK(enemy.hit_feedback.remaining_seconds == doctest::Approx(.04F));
            CHECK_FALSE(fire().combat.fired);
            CHECK(enemy.hit_feedback.remaining_seconds == doctest::Approx(.04F));
            player.weapon_cooldown_remaining_seconds = 0;
            const auto miss =
                RelayGame::fire_player_weapon(world, {0, .45F, 0}, {0, 0, -1}, player, {}, enemies, weapon);
            REQUIRE(miss.combat.fired);
            CHECK_FALSE(miss.rays[0].actor_contact);
            CHECK(enemy.hit_feedback.remaining_seconds == doctest::Approx(.04F));
            player.weapon_cooldown_remaining_seconds = 0;
            REQUIRE(fire().combat.fired);
            CHECK(enemy.hit_feedback.remaining_seconds == doctest::Approx(.07F));
            tick(.08F);
            CHECK(enemy.hit_feedback.remaining_seconds == 0);
            CHECK(enemy.hit_feedback.position == glm::vec3{0});
            player.weapon_cooldown_remaining_seconds = 0;
            enemy.state.health = 1;
            REQUIRE(fire().killed_enemy);
            CHECK(enemy.hit_feedback.remaining_seconds == 0);
        }
}

TEST_CASE("shotgun hit feedback keeps one contact for each surviving target")
{
    const auto level = room();
    auto enemies = RelayGame::build_enemies(level);
    enemies.push_back(enemies[0]);
    enemies[0].state.position.x = -.48F;
    enemies[1].state.position.x = .48F;
    for (auto& enemy : enemies)
        enemy.state.health = 1000;
    RelayCore::gameplay::PlayerCombatState player;
    const auto attack = RelayGame::fire_player_weapon(
        RelayCore::test::WorldQueryFixture{level, {}},
        {0, .45F, 0},
        {0, 0, 1},
        player,
        {},
        enemies,
        RelayGame::PlayerWeapon::Shotgun
    );
    std::array<bool, 2> seen{};
    for (std::size_t i = 0; i < attack.ray_count; ++i)
        if (const auto& contact = attack.rays[i].actor_contact)
        {
            CHECK_FALSE(attack.pellet_traces[i].impact);
            if (!seen[contact->index])
                CHECK(enemies[contact->index].hit_feedback.position == contact->position);
            CHECK(enemies[contact->index].hit_feedback.remaining_seconds == doctest::Approx(.07F));
            seen[contact->index] = true;
        }
    CHECK(seen[0]);
    CHECK(seen[1]);
}

TEST_CASE("turret hit feedback uses actor contact and clears on expiry and destruction")
{
    auto level = room();
    level.entities.clear();
    std::array<RelayCore::gameplay::RuntimeTurret, 1> turrets;
    auto& turret = turrets[0];
    turret.bounds = {{-.5F, 0, 2.5F}, {.5F, 1, 3.5F}};
    turret.health = 1000;
    RelayCore::gameplay::PlayerCombatState player;
    const auto fire = [&] {
        return RelayGame::fire_player_weapon(
            RelayCore::test::WorldQueryFixture{level, {}},
            {0, .45F, 0},
            {0, 0, 1},
            player,
            turrets,
            {},
            RelayGame::PlayerWeapon::Shotgun
        );
    };
    const auto attack = fire();
    REQUIRE(attack.rays[0].actor_contact);
    CHECK(turret.hit_feedback.remaining_seconds == doctest::Approx(.07F));
    CHECK(turret.hit_feedback.position == attack.rays[0].actor_contact->position);
    CHECK(turret.hit_feedback.normal == attack.rays[0].actor_contact->normal);
    RelayCore::gameplay::tick_combat_state(.08F, player, turrets);
    CHECK(turret.hit_feedback.remaining_seconds == 0);
    CHECK(turret.hit_feedback.position == glm::vec3{0});
    player.weapon_cooldown_remaining_seconds = 0;
    REQUIRE(fire().combat.fired);
    turret.health = 1;
    player.weapon_cooldown_remaining_seconds = 0;
    REQUIRE(fire().combat.killed_turret);
    CHECK(turret.hit_feedback.remaining_seconds == 0);
}

TEST_CASE("shotgun has a close range advantage and counts one discharge")
{
    for (float distance : {3.0F, 12.0F})
    {
        CAPTURE(distance);
        auto level = room();
        level.entities[0].origin.z = distance;
        auto shotgun_enemies = RelayGame::build_enemies(level);
        auto precise_enemies = shotgun_enemies;
        RelayCore::gameplay::PlayerCombatState shotgun, precise;
        const auto shot = RelayGame::fire_player_weapon(
            RelayCore::test::WorldQueryFixture{level, {}},
            {0, .45F, 0},
            {0, 0, 1},
            shotgun,
            {},
            shotgun_enemies,
            RelayGame::PlayerWeapon::Shotgun
        );
        REQUIRE(shot.combat.fired);
        CHECK(shot.ray_count == 7);
        CHECK(shotgun.shot_trace.remaining_seconds == 0);
        for (const auto& trace : shot.pellet_traces)
        {
            CHECK(trace.start == glm::vec3{0, .45F, 0});
            CHECK(trace.remaining_seconds > 0);
            CHECK(glm::distance(trace.start, trace.end) <= 14.001F);
        }
        CHECK(shot.pellet_traces[1].end.x * shot.pellet_traces[2].end.x < 0);
        CHECK(shotgun.shots_fired == 1);
        CHECK(shotgun.shots_hit == 1);
        CHECK_FALSE(shot.rays[0].world_impact);
        REQUIRE(shot.rays[0].actor_contact);
        if (shot.killed_enemy)
        {
            REQUIRE(shot.pellet_traces[0].impact);
            CHECK(shot.pellet_traces[0].impact->surface == RelayCore::SurfaceMaterial::Metal);
        } else
        {
            CHECK_FALSE(shot.pellet_traces[0].impact);
            CHECK(shotgun_enemies[0].hit_feedback.position == shot.rays[0].actor_contact->position);
            CHECK(shotgun_enemies[0].hit_feedback.remaining_seconds == doctest::Approx(.07F));
        }
        CHECK(shotgun.weapon_cooldown_remaining_seconds == doctest::Approx(.85F));
        CHECK(shotgun.weapon_damage == precise.weapon_damage);
        static_cast<void>(RelayGame::fire_player_weapon(
            RelayCore::test::WorldQueryFixture{level, {}},
            {0, .45F, 0},
            {0, 0, 1},
            precise,
            {},
            precise_enemies
        ));
        if (distance == 3)
        {
            CHECK(shot.killed_enemy);
            CHECK(shotgun.kills == 1);
            CHECK(shotgun.damage_dealt == 60);
            CHECK(precise_enemies[0].state.health == 35);
        } else
        {
            CHECK_FALSE(shot.killed_enemy);
            CHECK(shotgun.damage_dealt < precise.damage_dealt);
            CHECK(shotgun.kills == 0);
        }
        CHECK_FALSE(RelayGame::fire_player_weapon(
                        RelayCore::test::WorldQueryFixture{level, {}},
                        {0, .45F, 0},
                        {0, 0, 1},
                        shotgun,
                        {},
                        shotgun_enemies
        )
                        .combat.fired);
    }
}

TEST_CASE("shotgun respects walls doors range and nearest target throughout a discharge")
{
    auto level = room();
    auto enemies = RelayGame::build_enemies(level);
    RelayCore::gameplay::PlayerCombatState player;
    std::vector<RelayCore::gameplay::RuntimeDoor> doors;
    std::vector<RelayCore::gameplay::RuntimeTurret> turrets;
    SUBCASE("wall")
    {
        level.solids.push_back({{-2, 0, 1}, {2, 3, 1.2F}, RelayCore::SurfaceMaterial::Wall});
    }
    SUBCASE("closed door")
    {
        RelayCore::gameplay::RuntimeDoor door;
        door.closed_bounds = {{-2, 0, 1}, {2, 3, 1.2F}};
        doors.push_back(door);
    }
    SUBCASE("range")
    {
        enemies[0].state.position.z = 16;
    }
    SUBCASE("early kill does not let later pellets penetrate the target")
    {
        enemies[0].state.health = 14;
        RelayCore::gameplay::RuntimeTurret behind;
        behind.origin = {0, 1, 5};
        behind.bounds = {{-2, 0, 4.5F}, {2, 3, 5.5F}};
        turrets.push_back(behind);
    }
    const auto shot = RelayGame::fire_player_weapon(
        RelayCore::test::WorldQueryFixture{level, doors},
        {0, .45F, 0},
        {0, 0, 1},
        player,
        turrets,
        enemies,
        RelayGame::PlayerWeapon::Shotgun
    );
    CHECK(player.shots_fired == 1);
    if (turrets.empty())
    {
        for (std::size_t i = 0; i < shot.ray_count; ++i)
            CHECK_FALSE(shot.rays[i].actor_contact);
        CHECK(enemies[0].state.health == 60);
        CHECK(player.shots_hit == 0);
        if (enemies[0].state.position.z < 14)
            for (const auto& trace : shot.pellet_traces)
                CHECK(trace.end.z <= 1.201F);
    } else
    {
        CHECK(shot.killed_enemy);
        CHECK(turrets[0].health == turrets[0].max_health);
        CHECK(player.kills == 1);
        for (std::size_t i = 0; i < shot.ray_count; ++i)
        {
            CHECK_FALSE(shot.rays[i].world_impact);
            if (shot.rays[i].actor_contact)
            {
                CHECK(shot.rays[i].actor_contact->kind == RelayCore::gameplay::CombatActorKind::Enemy);
                CHECK(shot.rays[i].actor_contact->index == 0);
            }
        }
    }
}

TEST_CASE("weapon impacts select the nearest real surface per family for both weapons")
{
    using RelayCore::SurfaceMaterial;
    using RelayCore::physics::WorldCollider;
    using RelayCore::physics::WorldObjectKind;
    const std::array walls{
        WorldCollider{{{-5, -5, 4}, {0.01F, 5, 4.2F}}, SurfaceMaterial::Concrete, {WorldObjectKind::Solid, 0}},
        WorldCollider{{{0.02F, -5, 5}, {5, 5, 5.2F}}, SurfaceMaterial::Metal, {WorldObjectKind::PropCollider, 1}},
    };
    for (const auto weapon : {RelayGame::PlayerWeapon::Precise, RelayGame::PlayerWeapon::Shotgun})
    {
        RelayCore::gameplay::PlayerCombatState player;
        auto attack = RelayGame::fire_player_weapon({walls}, {}, {0, 0, 1}, player, {}, {}, weapon);
        const auto feedback = RelayGame::weapon_impact_feedback(std::span{attack.rays.data(), attack.ray_count});
        REQUIRE(feedback.hard);
        CHECK(feedback.hard->position.z == doctest::Approx(4));
        CHECK(feedback.hard->object.index == 0);
        CHECK(feedback.hard->normal == glm::vec3{0, 0, -1});
        CHECK(feedback.metal.has_value() == (weapon == RelayGame::PlayerWeapon::Shotgun));
        if (feedback.metal)
        {
            CHECK(feedback.metal->position.z == doctest::Approx(5));
            CHECK(feedback.metal->object.index == 1);
        }
        CHECK(player.shots_hit == 0);
        CHECK(player.hit_confirmation_remaining_seconds == 0);
        const auto cooldown = RelayGame::fire_player_weapon({walls}, {}, {0, 0, 1}, player, {}, {}, weapon);
        CHECK_FALSE(cooldown.combat.fired);
        CHECK_FALSE(cooldown.combat.world_impact);
        player.weapon_cooldown_remaining_seconds = 0;
        attack = RelayGame::fire_player_weapon({walls}, {}, {0, 0, -1}, player, {}, {}, weapon);
        for (std::size_t i = 0; i < attack.ray_count; ++i)
            CHECK_FALSE(attack.rays[i].world_impact);
    }

    std::array<RelayCore::gameplay::CombatFireResult, 4> rays;
    rays[0].fired = rays[1].fired = rays[2].fired = true;
    rays[0].world_impact = RelayCore::physics::WorldQueryHit{.distance = 8, .surface = SurfaceMaterial::WallPanel};
    rays[1].world_impact = RelayCore::physics::WorldQueryHit{.distance = 3, .surface = SurfaceMaterial::Metal};
    rays[2].world_impact = RelayCore::physics::WorldQueryHit{.distance = 2, .surface = SurfaceMaterial::Tile};
    rays[3].world_impact = RelayCore::physics::WorldQueryHit{.distance = 1, .surface = SurfaceMaterial::Concrete};
    const auto feedback = RelayGame::weapon_impact_feedback(rays);
    REQUIRE(feedback.metal);
    REQUIRE(feedback.hard);
    CHECK(feedback.metal->distance == 3);
    CHECK(feedback.hard->distance == 2);
}

TEST_CASE("shotgun can destroy multiple turrets with one event per target")
{
    auto level = room();
    level.entities.clear();
    std::array<RelayCore::gameplay::RuntimeTurret, 2> turrets;
    for (std::size_t i = 0; i < turrets.size(); ++i)
    {
        const float x = i == 0 ? -.4F : .4F;
        turrets[i].origin = {x, 1, 3};
        turrets[i].bounds = {{x - .15F, 0, 2.7F}, {x + .15F, 2, 3.3F}};
        turrets[i].health = 14;
        turrets[i].destroy_target = i == 0 ? "left" : "right";
        turrets[i].destroy_action = RelayCore::gameplay::CombatTargetActionKind::ActivateTarget;
    }
    RelayCore::gameplay::PlayerCombatState player;
    const auto result = RelayGame::fire_player_weapon(
        RelayCore::test::WorldQueryFixture{level, {}},
        {0, .6F, 0},
        {0, 0, 1},
        player,
        turrets,
        {},
        RelayGame::PlayerWeapon::Shotgun
    );
    CHECK(player.shots_fired == 1);
    CHECK(player.shots_hit == 1);
    CHECK(player.kills == 2);
    CHECK(player.damage_dealt == 28);
    unsigned left = 0, right = 0;
    for (std::size_t i = 0; i < result.ray_count; ++i)
    {
        if (!result.rays[i].killed_turret)
            continue;
        REQUIRE(result.rays[i].actor_contact);
        const auto& contact = *result.rays[i].actor_contact;
        CHECK(contact.kind == RelayCore::gameplay::CombatActorKind::Turret);
        CHECK(contact.index == result.rays[i].turret_index);
        CHECK(contact.damage == 14);
        CHECK(contact.position == result.pellet_traces[i].end);
        CHECK_FALSE(result.rays[i].world_impact);
        left += result.rays[i].target_event.target == "left";
        right += result.rays[i].target_event.target == "right";
    }
    CHECK(left == 1);
    CHECK(right == 1);
}

TEST_CASE("shotgun handles vertical aim and does not fire an invalid direction")
{
    auto level = room();
    RelayCore::gameplay::PlayerCombatState player;
    CHECK_FALSE(RelayGame::fire_player_weapon(
                    RelayCore::test::WorldQueryFixture{level, {}},
                    {0, 1, 0},
                    {},
                    player,
                    {},
                    {},
                    RelayGame::PlayerWeapon::Shotgun
    )
                    .combat.fired);
    CHECK(player.shots_fired == 0);
    CHECK(RelayGame::fire_player_weapon(
              RelayCore::test::WorldQueryFixture{level, {}},
              {0, 1, 0},
              {0, 1, 0},
              player,
              {},
              {},
              RelayGame::PlayerWeapon::Shotgun
    )
              .combat.fired);
    CHECK(player.shots_fired == 1);
    CHECK(player.shots_hit == 0);
}

TEST_CASE("rover signals windup once and a dodged strike does not damage the player")
{
    auto level = room();
    auto enemies = RelayGame::build_enemies(level);
    enemies[0].state.position = {0, .01F, .9F};
    RelayCore::gameplay::PlayerCombatState player;
    std::vector<RelayCore::physics::WorldCollider> scratch;
    const auto actor = RelayCore::physics::make_player_aabb({0, .01F, 0}, .3F, 1.8F);
    const auto start = RelayGame::update_enemies(
        RelayCore::test::WorldQueryFixture{level, {}},
        actor,
        {0, 1.6F, 0},
        player,
        enemies,
        .01F,
        scratch
    );
    CHECK(start.windups == 1);
    CHECK(start.strikes == 0);
    const auto paused = RelayGame::update_enemies(
        RelayCore::test::WorldQueryFixture{level, {}},
        actor,
        {0, 1.6F, 0},
        player,
        enemies,
        0,
        scratch
    );
    CHECK(paused.windups == 0);
    CHECK(paused.strikes == 0);
    const auto waiting = RelayGame::update_enemies(
        RelayCore::test::WorldQueryFixture{level, {}},
        actor,
        {0, 1.6F, 0},
        player,
        enemies,
        .1F,
        scratch
    );
    CHECK(waiting.windups == 0);
    CHECK(waiting.strikes == 0);
    const auto escaped = RelayCore::physics::make_player_aabb({0, .01F, -2}, .3F, 1.8F);
    const auto strike = RelayGame::update_enemies(
        RelayCore::test::WorldQueryFixture{level, {}},
        escaped,
        {0, 1.6F, -2},
        player,
        enemies,
        .4F,
        scratch
    );
    CHECK(strike.windups == 0);
    CHECK(strike.strikes == 1);
    CHECK(player.health == 100);
    const auto recovery = RelayGame::update_enemies(
        RelayCore::test::WorldQueryFixture{level, {}},
        escaped,
        {0, 1.6F, -2},
        player,
        enemies,
        .1F,
        scratch
    );
    CHECK(recovery.strikes == 0);
}

TEST_CASE("nonlethal hits do not restart rover windup or bypass its recovery")
{
    const auto level = room();
    auto enemies = RelayGame::build_enemies(level);
    enemies[0].state.position = {0, .01F, .9F};
    RelayCore::gameplay::PlayerCombatState player;
    std::vector<RelayCore::physics::WorldCollider> scratch;
    const auto actor = RelayCore::physics::make_player_aabb({0, .01F, 0}, .3F, 1.8F);
    RelayGame::update_enemies(
        RelayCore::test::WorldQueryFixture{level, {}},
        actor,
        {0, 1.6F, 0},
        player,
        enemies,
        .01F,
        scratch
    );
    RelayGame::update_enemies(
        RelayCore::test::WorldQueryFixture{level, {}},
        actor,
        {0, 1.6F, 0},
        player,
        enemies,
        .20F,
        scratch
    );
    const auto hit = RelayGame::fire_player_weapon(
        RelayCore::test::WorldQueryFixture{level, {}},
        {0, .6F, 0},
        {0, 0, 1},
        player,
        {},
        enemies
    );
    REQUIRE(hit.enemy_index == 0);
    CHECK(enemies[0].state.mode == RelayGame::EnemyMode::Windup);
    CHECK(enemies[0].state.timer == doctest::Approx(.25F));
    CHECK(enemies[0].state.last_seen == glm::vec3{0, .6F, 0});
    const auto strike = RelayGame::update_enemies(
        RelayCore::test::WorldQueryFixture{level, {}},
        actor,
        {0, 1.6F, 0},
        player,
        enemies,
        .26F,
        scratch
    );
    CHECK(strike.strikes == 1);
    CHECK(player.health == 86);
    player.weapon_cooldown_remaining_seconds = 0;
    static_cast<void>(RelayGame::fire_player_weapon(
        RelayCore::test::WorldQueryFixture{level, {}},
        {0, .6F, 0},
        {0, 0, 1},
        player,
        {},
        enemies
    ));
    CHECK(enemies[0].state.mode == RelayGame::EnemyMode::Recovery);
    CHECK(enemies[0].state.timer == doctest::Approx(.95F));
}

TEST_CASE("rover cannot see shoot or walk through walls")
{
    auto level = room();
    level.solids.push_back({{-4, 0, 1.4F}, {4, 3, 1.6F}, RelayCore::SurfaceMaterial::Wall});
    auto enemies = RelayGame::build_enemies(level);
    RelayCore::gameplay::PlayerCombatState player;
    std::vector<RelayCore::physics::WorldCollider> scratch;
    const auto actor = RelayCore::physics::make_player_aabb({0, 0, 0}, .3F, 1.8F);
    RelayGame::update_enemies(
        RelayCore::test::WorldQueryFixture{level, {}},
        actor,
        {0, 1.6F, 0},
        player,
        enemies,
        .1F,
        scratch
    );
    CHECK(enemies[0].state.mode == RelayGame::EnemyMode::Idle);
    auto shot = RelayGame::fire_player_weapon(
        RelayCore::test::WorldQueryFixture{level, {}},
        {0, .6F, 0},
        {0, 0, 1},
        player,
        {},
        enemies
    );
    CHECK(shot.enemy_index == -1);
    enemies[0].state.mode = RelayGame::EnemyMode::Chasing;
    enemies[0].state.last_seen = {0, 0, 0};
    for (int i = 0; i < 360; ++i)
        RelayGame::update_enemies(
            RelayCore::test::WorldQueryFixture{level, {}},
            actor,
            {0, 1.6F, 0},
            player,
            enemies,
            1.F / 120,
            scratch
        );
    CHECK(enemies[0].state.position.z >= 2.019F);
    CHECK(player.health == 100);
}

TEST_CASE("rover dies from player fire and no longer attacks")
{
    auto level = room();
    auto enemies = RelayGame::build_enemies(level);
    RelayCore::gameplay::PlayerCombatState player;
    for (int i = 0; i < 3; ++i)
    {
        player.weapon_cooldown_remaining_seconds = 0;
        const auto hit = RelayGame::fire_player_weapon(
            RelayCore::test::WorldQueryFixture{level, {}},
            {0, .6F, 0},
            {0, 0, 1},
            player,
            {},
            enemies
        );
        CHECK(hit.enemy_index == 0);
        CHECK(hit.killed_enemy == (i == 2));
    }
    CHECK(player.kills == 1);
    CHECK(player.shots_hit == 3);
    CHECK(player.hit_confirmation_remaining_seconds > 0.0F);
    CHECK(enemies[0].state.mode == RelayGame::EnemyMode::Dead);
    std::vector<RelayCore::physics::WorldCollider> scratch;
    RelayGame::update_enemies(
        RelayCore::test::WorldQueryFixture{level, {}},
        {{-.3F, 0, 2}, {.3F, 1.8F, 2.6F}},
        {0, 1.6F, 2.3F},
        player,
        enemies,
        1,
        scratch
    );
    CHECK(player.health == 100);
}

TEST_CASE("rover authored in two maps preserves position health and attack state through disk")
{
    RelayCore::test::ScopedTempDirectory directory;
    RelayCore::test::ScopedTestLogger logger{directory.path() / "logs"};
    RelayCore::core::EngineConfig config;
    for (const char* map : {"foundation_intake", "foundation_pump"})
    {
        RelayGame::GameWorldRuntime runtime;
        runtime.load_level_session(
            config,
            RelayGame::LevelSessionBuilder{config.gameplay}.build(
                RelayCore::core::asset_path(std::string("levels/") + map + ".toml")
            )
        );
        REQUIRE(runtime.enemies().size() == 1);
        auto save = runtime.capture_saved_game(12, true);
        save.enemies[0].position.z -= .2F;
        save.enemies[0].health = 25;
        save.enemies[0].mode = RelayGame::EnemyMode::Windup;
        save.enemies[0].timer = .3F;
        const auto path = directory.path() / "progress.toml";
        RelayGame::write_saved_game(path, save);
        runtime.restore_saved_game(RelayGame::load_saved_game(path));
        CHECK(runtime.enemies()[0].state.position == save.enemies[0].position);
        CHECK(runtime.enemies()[0].state.health == 25);
        CHECK(runtime.enemies()[0].state.mode == RelayGame::EnemyMode::Windup);
        CHECK(runtime.enemies()[0].state.timer == doctest::Approx(.3F));
    }
    RelayGame::LevelEditorDocument document;
    document.new_room();
    document.add_enemy({0, 0, 2});
    REQUIRE(document.selection());
    CHECK(document.visible_entity(document.selection()->index));
    document.save(directory.path() / "enemy.toml");
    CHECK(RelayGame::LevelSessionBuilder{config.gameplay}.build(directory.path() / "enemy.toml").enemies.size() == 1);
    const auto model = RelayCore::render::load_obj_mesh(RelayCore::core::asset_path("models/security_rover.obj"));
    CHECK(model.indices.size() > 100);
    const auto wreck =
        RelayCore::render::load_obj_mesh(RelayCore::core::asset_path("models/security_rover_destroyed.obj"));
    CHECK(wreck.indices.size() > 100);
}

TEST_CASE("enemy HUD projects health and actual damage with visibility and restore")
{
    RelayCore::test::ScopedTempDirectory directory;
    RelayCore::test::ScopedTestLogger logger{directory.path() / "logs"};
    RelayCore::core::EngineConfig config;
    auto level = room();
    const auto build = [&](const RelayCore::BlockoutLevel& source) {
        RelayGame::GameWorldRuntime runtime;
        runtime.load_level_session(config, RelayGame::LevelSessionBuilder{config.gameplay}.build(source, "hud.toml"));
        return runtime;
    };
    auto runtime = build(level);
    RelayCore::render::PerspectiveCamera camera;
    camera.set_position({0, .6F, 0});
    camera.set_target({0, .6F, 3});
    const auto initial = runtime.capture_saved_game(0, false);
    auto hud = runtime.build_hud_presentation(&camera);
    REQUIRE(hud.world_markers.size() == 1);
    CHECK(hud.world_markers[0].text == "60 / 60");
    CHECK(hud.world_markers[0].x == doctest::Approx(50));
    CHECK(hud.world_markers[0].y < 50);
    CHECK(runtime.debug_apply_player_fire(camera.position(), camera.forward()).fired);
    CHECK(runtime.enemies()[0].hit_feedback.remaining_seconds > 0);
    hud = runtime.build_hud_presentation(&camera);
    REQUIRE(hud.world_markers.size() == 2);
    CHECK(hud.world_markers[0].text == "35 / 60");
    CHECK(hud.world_markers[1].text == "−25");
    CHECK(hud.world_markers[1].damage);
    CHECK_FALSE(runtime.debug_apply_player_fire(camera.position(), camera.forward()).fired);
    CHECK(runtime.build_hud_presentation(&camera).world_markers.size() == 2);
    const auto hit_snapshot = runtime.capture_encounter_snapshot(0, 0, false);
    runtime.restore_encounter_snapshot(hit_snapshot);
    CHECK(runtime.enemies()[0].hit_feedback.remaining_seconds == 0);
    CHECK(runtime.enemies()[0].state.health == 35);
    runtime.restore_saved_game(initial);
    CHECK(runtime.enemies()[0].hit_feedback.remaining_seconds == 0);
    CHECK(runtime.build_hud_presentation(&camera).world_markers.size() == 1);
    auto shotgun = initial;
    shotgun.world.inventory_item_ids.push_back(std::string(RelayGame::shotgun_item_id));
    shotgun.world.selected_weapon = RelayGame::PlayerWeapon::Shotgun;
    runtime.restore_saved_game(shotgun);
    CHECK(runtime.debug_apply_player_fire(camera.position(), camera.forward()).fired);
    hud = runtime.build_hud_presentation(&camera);
    REQUIRE(hud.world_markers.size() == 1);
    CHECK(hud.world_markers[0].damage);
    CHECK(hud.world_markers[0].text == "−60");
    camera.set_target({0, .6F, -3});
    CHECK(runtime.build_hud_presentation(&camera).world_markers.empty());
    camera.set_target({0, .6F, 3});
    level.solids.push_back({{-1, 0, 1}, {1, 3, 2}, RelayCore::SurfaceMaterial::Metal});
    auto blocked = build(level);
    CHECK(blocked.build_hud_presentation(&camera).world_markers.empty());
}
