#include "relaycore/core/paths.h"
#include "relaygame/game_world_runtime.h"
#include "relaygame/mission_runtime.h"

#include <algorithm>
#include <array>
#include <doctest/doctest.h>

#include "scoped_temp_directory.h"
#include "scoped_test_logger.h"

TEST_CASE("campaign route gates six forward transitions and preserves seven map snapshots")
{
    RelayCore::test::ScopedTempDirectory temp;
    RelayCore::test::ScopedTestLogger logger{temp.path() / "logs"};
    const RelayCore::core::EngineConfig config;
    const RelayGame::LevelSessionBuilder builder{config.gameplay};
    const std::array maps{
        "power_restoration",
        "foundation_intake",
        "foundation_pump",
        "bench_relay",
        "pressure_transfer",
        "cooling_loop",
        "occult_lab_b17"
    };
    std::vector<RelayGame::SavedGame> history;
    for (std::size_t i = 0; i < maps.size(); ++i)
    {
        CAPTURE(maps[i]);
        const auto path = RelayCore::core::asset_path(std::string{"levels/"} + maps[i] + ".toml");
        auto session = i == 0 ? builder.build(path) : builder.build_at_entry(path, "campaign_arrival");
        REQUIRE(session.authoring_warnings.empty());
        const auto step_count = session.mission_definition.steps.size();
        const auto exits = session.exits;
        RelayGame::GameWorldRuntime runtime;
        runtime.load_level_session(config, std::move(session));
        CHECK_FALSE(runtime.active_level_exit());
        auto save = runtime.capture_saved_game(42, true);
        save.world.inventory_item_ids = {"power_fuse", "service_key", "weapon_shotgun"};
        save.world.player_combat.health = 67;
        save.world.selected_weapon = RelayGame::PlayerWeapon::Shotgun;
        runtime.restore_saved_game(save);
        if (i + 1 < maps.size())
        {
            const auto next = std::ranges::find_if(exits, [](const auto& exit) {
                return exit.requires_completion;
            });
            REQUIRE(next != exits.end());
            CHECK(std::ranges::count(exits, true, &RelayGame::LevelExit::requires_completion) == 1);
            CHECK(next->destination.filename() == std::string{maps[i + 1]} + ".toml");
            REQUIRE_NOTHROW(static_cast<void>(builder.build_at_entry(next->destination, next->entry)));
            save.player.position = (next->bounds.mins + next->bounds.maxs) * .5F;
            save.player.position.y = .01F;
            runtime.restore_saved_game(save);
            CHECK_FALSE(runtime.active_level_exit());
            save.world.mission.current_step = static_cast<std::uint32_t>(step_count);
            save.world.mission.completed = true;
            for (auto& door : save.world.doors)
            {
                door.state = RelayCore::gameplay::DoorState::Open;
                door.open_fraction = 1;
            }
            runtime.restore_saved_game(save);
            REQUIRE(runtime.active_level_exit());
            CHECK(runtime.active_level_exit()->destination == next->destination);
            CHECK(runtime.has_completion_exit());
        } else
            CHECK_FALSE(runtime.has_completion_exit());
        save = runtime.capture_saved_game(42, true);
        save.visited_levels = history;
        const auto progress = temp.path() / "campaign.toml";
        RelayGame::write_saved_game(progress, save);
        const auto loaded = RelayGame::load_saved_game(progress);
        CHECK(loaded.visited_levels.size() == i);
        CHECK(loaded.world.inventory_item_ids == save.world.inventory_item_ids);
        CHECK(loaded.world.player_combat.health == 67);
        CHECK(loaded.world.selected_weapon == RelayGame::PlayerWeapon::Shotgun);
        CHECK(loaded.flashlight);
        CHECK(loaded.played_seconds == 42);
        RelayGame::GameWorldRuntime continued;
        continued.load_level_session(config, builder.build(path));
        REQUIRE_NOTHROW(continued.restore_saved_game(loaded));
        CHECK(continued.mission_status().completed == runtime.mission_status().completed);
        save.visited_levels.clear();
        history.push_back(std::move(save));
    }
    const auto pump = builder.build(RelayCore::core::asset_path("levels/foundation_pump.toml"));
    CHECK(std::ranges::any_of(pump.exits, [](const auto& exit) {
        return !exit.requires_completion && exit.destination.filename() == "foundation_intake.toml";
    }));
}
