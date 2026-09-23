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
}
