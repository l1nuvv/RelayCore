#include "relaycore/core/paths.h"
#include "relaycore/render/static_model.h"
#include "relaygame/enemy_runtime.h"
#include "relaygame/game_world_runtime.h"
#include "relaygame/level_editor_document.h"

#include <doctest/doctest.h>

#include "scoped_temp_directory.h"
#include "scoped_test_logger.h"

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

TEST_CASE("rover detects pursues and attacks at a bounded cadence")
{
    auto level = room();
    auto enemies = RelayGame::build_enemies(level);
    RelayCore::gameplay::PlayerCombatState player;
    std::vector<RelayCore::physics::Aabb> scratch;
    const auto actor = RelayCore::physics::make_player_aabb({0, .01F, 0}, .3F, 1.8F);
    for (int i = 0; i < 360; ++i)
        RelayGame::update_enemies(level, {}, actor, {0, 1.6F, 0}, player, enemies, 1.F / 120, scratch);
    CHECK(enemies[0].state.position.z < 1.2F);
    CHECK(enemies[0].state.position.z >= .7F);
    CHECK(player.health == doctest::Approx(86));
    CHECK(enemies[0].state.position.y >= -.002F);
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
            level,
            {},
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
        CHECK(shotgun.weapon_cooldown_remaining_seconds == doctest::Approx(.85F));
        CHECK(shotgun.weapon_damage == precise.weapon_damage);
        static_cast<void>(
            RelayGame::fire_player_weapon(level, {}, {0, .45F, 0}, {0, 0, 1}, precise, {}, precise_enemies)
        );
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
        CHECK_FALSE(
            RelayGame::fire_player_weapon(level, {}, {0, .45F, 0}, {0, 0, 1}, shotgun, {}, shotgun_enemies).combat.fired
        );
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
        level,
        doors,
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
    }
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
        level,
        {},
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
    CHECK_FALSE(
        RelayGame::fire_player_weapon(level, {}, {0, 1, 0}, {}, player, {}, {}, RelayGame::PlayerWeapon::Shotgun)
            .combat.fired
    );
    CHECK(player.shots_fired == 0);
    CHECK(
        RelayGame::fire_player_weapon(level, {}, {0, 1, 0}, {0, 1, 0}, player, {}, {}, RelayGame::PlayerWeapon::Shotgun)
            .combat.fired
    );
    CHECK(player.shots_fired == 1);
    CHECK(player.shots_hit == 0);
}

TEST_CASE("rover signals windup once and a dodged strike does not damage the player")
{
    auto level = room();
    auto enemies = RelayGame::build_enemies(level);
    enemies[0].state.position = {0, .01F, .9F};
    RelayCore::gameplay::PlayerCombatState player;
    std::vector<RelayCore::physics::Aabb> scratch;
    const auto actor = RelayCore::physics::make_player_aabb({0, .01F, 0}, .3F, 1.8F);
    const auto start = RelayGame::update_enemies(level, {}, actor, {0, 1.6F, 0}, player, enemies, .01F, scratch);
    CHECK(start.windups == 1);
    CHECK(start.strikes == 0);
    const auto paused = RelayGame::update_enemies(level, {}, actor, {0, 1.6F, 0}, player, enemies, 0, scratch);
    CHECK(paused.windups == 0);
    CHECK(paused.strikes == 0);
    const auto waiting = RelayGame::update_enemies(level, {}, actor, {0, 1.6F, 0}, player, enemies, .1F, scratch);
    CHECK(waiting.windups == 0);
    CHECK(waiting.strikes == 0);
    const auto escaped = RelayCore::physics::make_player_aabb({0, .01F, -2}, .3F, 1.8F);
    const auto strike = RelayGame::update_enemies(level, {}, escaped, {0, 1.6F, -2}, player, enemies, .4F, scratch);
    CHECK(strike.windups == 0);
    CHECK(strike.strikes == 1);
    CHECK(player.health == 100);
    const auto recovery = RelayGame::update_enemies(level, {}, escaped, {0, 1.6F, -2}, player, enemies, .1F, scratch);
    CHECK(recovery.strikes == 0);
}

TEST_CASE("nonlethal hits do not restart rover windup or bypass its recovery")
{
    const auto level = room();
    auto enemies = RelayGame::build_enemies(level);
    enemies[0].state.position = {0, .01F, .9F};
    RelayCore::gameplay::PlayerCombatState player;
    std::vector<RelayCore::physics::Aabb> scratch;
    const auto actor = RelayCore::physics::make_player_aabb({0, .01F, 0}, .3F, 1.8F);
    RelayGame::update_enemies(level, {}, actor, {0, 1.6F, 0}, player, enemies, .01F, scratch);
    RelayGame::update_enemies(level, {}, actor, {0, 1.6F, 0}, player, enemies, .20F, scratch);
    const auto hit = RelayGame::fire_player_weapon(level, {}, {0, .6F, 0}, {0, 0, 1}, player, {}, enemies);
    REQUIRE(hit.enemy_index == 0);
    CHECK(enemies[0].state.mode == RelayGame::EnemyMode::Windup);
    CHECK(enemies[0].state.timer == doctest::Approx(.25F));
    const auto strike = RelayGame::update_enemies(level, {}, actor, {0, 1.6F, 0}, player, enemies, .26F, scratch);
    CHECK(strike.strikes == 1);
    CHECK(player.health == 86);
    player.weapon_cooldown_remaining_seconds = 0;
    static_cast<void>(RelayGame::fire_player_weapon(level, {}, {0, .6F, 0}, {0, 0, 1}, player, {}, enemies));
    CHECK(enemies[0].state.mode == RelayGame::EnemyMode::Recovery);
    CHECK(enemies[0].state.timer == doctest::Approx(.95F));
}

TEST_CASE("rover cannot see shoot or walk through walls")
{
    auto level = room();
    level.solids.push_back({{-4, 0, 1.4F}, {4, 3, 1.6F}, RelayCore::SurfaceMaterial::Wall});
    auto enemies = RelayGame::build_enemies(level);
    RelayCore::gameplay::PlayerCombatState player;
    std::vector<RelayCore::physics::Aabb> scratch;
    const auto actor = RelayCore::physics::make_player_aabb({0, 0, 0}, .3F, 1.8F);
    RelayGame::update_enemies(level, {}, actor, {0, 1.6F, 0}, player, enemies, .1F, scratch);
    CHECK(enemies[0].state.mode == RelayGame::EnemyMode::Idle);
    auto shot = RelayGame::fire_player_weapon(level, {}, {0, .6F, 0}, {0, 0, 1}, player, {}, enemies);
    CHECK(shot.enemy_index == -1);
    enemies[0].state.mode = RelayGame::EnemyMode::Chasing;
    enemies[0].state.last_seen = {0, 0, 0};
    for (int i = 0; i < 360; ++i)
        RelayGame::update_enemies(level, {}, actor, {0, 1.6F, 0}, player, enemies, 1.F / 120, scratch);
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
        const auto hit = RelayGame::fire_player_weapon(level, {}, {0, .6F, 0}, {0, 0, 1}, player, {}, enemies);
        CHECK(hit.enemy_index == 0);
        CHECK(hit.killed_enemy == (i == 2));
    }
    CHECK(player.kills == 1);
    CHECK(player.shots_hit == 3);
    CHECK(player.hit_confirmation_remaining_seconds > 0.0F);
    CHECK(enemies[0].state.mode == RelayGame::EnemyMode::Dead);
    std::vector<RelayCore::physics::Aabb> scratch;
    RelayGame::update_enemies(
        level,
        {},
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
