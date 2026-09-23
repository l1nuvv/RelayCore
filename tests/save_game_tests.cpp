#include "relaycore/world/blockout_level.h"
#include "relaygame/save_game.h"

#include <doctest/doctest.h>
#include <fstream>
#include <limits>

#include "scoped_temp_directory.h"

namespace
{
    auto sample_save() -> RelayGame::SavedGame
    {
        RelayGame::SavedGame save;
        save.level_fingerprint = 12345678901234567890ULL;
        save.played_seconds = 145.5F;
        save.flashlight = true;
        save.player = {{3, 0, 8}, {1, 0, 2}, {0.8F, -0.4F}, true};
        auto& world = save.world;
        world.level_path = "test_level.toml";
        world.registry_signature = 9876543210987654321ULL;
        world.simulation_tick = 17550;
        world.doors = {{101, 0.4F, RelayCore::gameplay::DoorState::Opening}};
        world.usables = {{102, false, true}};
        world.triggers = {{103, true}};
        world.turrets = {{4, 25, 0.3F, 0.4F, true, false, true}};
        world.inventory_item_ids = {"power_fuse"};
        world.activated_target_ids = {"power", "cooling"};
        world.signaled_target_ids = {"door"};
        world.mission = {true, false, 3};
        world.player_combat.health = 75;
        world.player_combat.alive = true;
        world.player_combat.shots_fired = 8;
        world.player_combat.damage_taken = 25;
        world.player_combat.last_damage_source = "steam";
        return save;
    }
} // namespace

TEST_CASE("saved game survives disk round trip and atomic replacement")
{
    RelayCore::test::ScopedTempDirectory directory;
    const auto path = directory.path() / "profile/progress.toml";
    auto save = sample_save();
    RelayGame::write_saved_game(path, save);
    auto loaded = RelayGame::load_saved_game(path);
    CHECK(loaded.level_fingerprint == save.level_fingerprint);
    CHECK(loaded.world.registry_signature == save.world.registry_signature);
    CHECK(loaded.world.simulation_tick == save.world.simulation_tick);
    CHECK(loaded.player.position == save.player.position);
    CHECK(loaded.player.velocity == save.player.velocity);
    CHECK(loaded.player.view_angles == save.player.view_angles);
    CHECK(loaded.player.grounded);
    CHECK(loaded.flashlight);
    CHECK(loaded.played_seconds == save.played_seconds);
    REQUIRE(loaded.world.doors.size() == 1);
    CHECK(loaded.world.doors.front().state == RelayCore::gameplay::DoorState::Opening);
    CHECK(loaded.world.doors.front().open_fraction == doctest::Approx(0.4));
    CHECK(loaded.world.usables.front().consumed);
    CHECK_FALSE(loaded.world.usables.front().locked);
    CHECK(loaded.world.triggers.front().fired);
    CHECK(loaded.world.turrets.front().health == 25);
    CHECK(loaded.world.turrets.front().warming_up);
    CHECK(loaded.world.inventory_item_ids == save.world.inventory_item_ids);
    CHECK(loaded.world.activated_target_ids == save.world.activated_target_ids);
    CHECK(loaded.world.signaled_target_ids == save.world.signaled_target_ids);
    CHECK(loaded.world.player_combat.health == 75);
    CHECK(loaded.world.player_combat.last_damage_source == "steam");
    CHECK(loaded.world.mission.current_step == 3);
    save.world.mission.completed = true;
    save.player.position.z = 9;
    RelayGame::write_saved_game(path, save);
    loaded = RelayGame::load_saved_game(path);
    CHECK(loaded.world.mission.completed);
    CHECK(loaded.player.position.z == 9);
    CHECK_FALSE(std::filesystem::exists(path.string() + ".tmp"));
}

TEST_CASE("campaign save preserves inactive maps and rejects recursive or duplicate history")
{
    RelayCore::test::ScopedTempDirectory directory;
    const auto path = directory.path() / "campaign.toml";
    auto save = sample_save();
    auto previous = sample_save();
    previous.world.level_path = "earlier.toml";
    previous.world.mission.completed = true;
    save.visited_levels.push_back(previous);
    RelayGame::write_saved_game(path, save);
    const auto loaded = RelayGame::load_saved_game(path);
    REQUIRE(loaded.visited_levels.size() == 1);
    CHECK(loaded.visited_levels[0].world.doors[0].open_fraction == doctest::Approx(.4F));
    CHECK(loaded.visited_levels[0].world.mission.completed);
    save.visited_levels[0].visited_levels.push_back(previous);
    CHECK_THROWS(RelayGame::write_saved_game(path, save));
    CHECK(RelayGame::load_saved_game(path).visited_levels.size() == 1);
    save.visited_levels = {sample_save()};
    CHECK_THROWS(RelayGame::write_saved_game(path, save));
}

TEST_CASE("version one progress remains loadable without map history")
{
    RelayCore::test::ScopedTempDirectory directory;
    const auto path = directory.path() / "progress.toml";
    RelayGame::write_saved_game(path, sample_save());
    std::ifstream input(path);
    std::string text{std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{}};
    input.close();
    const auto version = text.find("format_version = 3");
    REQUIRE(version != std::string::npos);
    text.replace(version, 18, "format_version = 1");
    const auto history = text.find("visited_levels = []");
    REQUIRE(history != std::string::npos);
    text.erase(history, 19);
    std::ofstream(path) << text;
    const auto loaded = RelayGame::load_saved_game(path);
    CHECK(loaded.visited_levels.empty());
    CHECK(loaded.world.player_combat.health == 75);
    CHECK(loaded.player.position == sample_save().player.position);
}

TEST_CASE("version two progress remains loadable without enemies")
{
    RelayCore::test::ScopedTempDirectory directory;
    const auto path = directory.path() / "progress.toml";
    RelayGame::write_saved_game(path, sample_save());
    std::ifstream input(path);
    std::string text{std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{}};
    input.close();
    const auto version = text.find("format_version = 3");
    REQUIRE(version != std::string::npos);
    text.replace(version, 18, "format_version = 2");
    const auto enemies = text.find("enemies = []");
    REQUIRE(enemies != std::string::npos);
    text.erase(enemies, 12);
    std::ofstream(path) << text;
    const auto loaded = RelayGame::load_saved_game(path);
    CHECK(loaded.enemies.empty());
    CHECK(loaded.world.player_combat.health == 75);
}

TEST_CASE("invalid save cannot replace valid progress")
{
    RelayCore::test::ScopedTempDirectory directory;
    const auto path = directory.path() / "progress.toml";
    auto save = sample_save();
    RelayGame::write_saved_game(path, save);
    SUBCASE("nonfinite position")
    {
        save.player.position.x = std::numeric_limits<float>::quiet_NaN();
    }
    SUBCASE("invalid door state")
    {
        save.world.doors[0].state = static_cast<RelayCore::gameplay::DoorState>(20);
    }
    SUBCASE("duplicate inventory")
    {
        save.world.inventory_item_ids.push_back("power_fuse");
    }
    SUBCASE("duplicate runtime identifier")
    {
        save.world.triggers[0].runtime_id = 101;
    }
    SUBCASE("contradictory health")
    {
        save.world.player_combat.alive = false;
    }
    CHECK_THROWS(RelayGame::write_saved_game(path, save));
    CHECK(RelayGame::load_saved_game(path).player.position == glm::vec3(3, 0, 8));
}

TEST_CASE("malformed and unsupported saves fail explicitly")
{
    RelayCore::test::ScopedTempDirectory directory;
    const auto path = directory.path() / "progress.toml";
    for (const auto* text : {"format_version = 2", "format_version = 1", "invalid = ["})
    {
        std::ofstream(path) << text;
        CHECK_THROWS(static_cast<void>(RelayGame::load_saved_game(path)));
    }
    std::ofstream(path) << "format_version = 99";
    CHECK_THROWS_WITH(
        static_cast<void>(RelayGame::load_saved_game(path)),
        doctest::Contains("unsupported format version 99; expected 3")
    );
}

TEST_CASE("save compatibility follows geometry and entity properties")
{
    RelayCore::BlockoutLevel level;
    level.solids.push_back({{-1, -1, -1}, {1, 0, 1}, RelayCore::SurfaceMaterial::Concrete});
    RelayCore::EntitySpawnData entity;
    entity.classname = "player_spawn";
    entity.properties = {{"first", "one"}, {"second", "two"}};
    level.entities.push_back(entity);
    const auto fingerprint = RelayGame::fingerprint_level(level);
    level.entities[0].properties = {{"second", "two"}, {"first", "one"}};
    CHECK(RelayGame::fingerprint_level(level) == fingerprint);
    level.entities[0].origin.x = 2;
    CHECK(RelayGame::fingerprint_level(level) != fingerprint);
    level.entities[0].origin.x = 0;
    level.solids[0].maxs.z = 2;
    CHECK(RelayGame::fingerprint_level(level) != fingerprint);
}
