#include "relaygame/world_environment.h"

#include <doctest/doctest.h>

TEST_CASE("environment reacts to power and cooling with delayed lamps and a coasting fan")
{
    RelayCore::BlockoutLevel level;
    RelayCore::EntitySpawnData fan;
    fan.classname = "world_effect";
    fan.properties = {{"kind", "fan"}, {"required_target", "cooling"}};
    auto steam = fan;
    steam.properties = {{"kind", "steam"}, {"stop_target", "cooling"}};
    auto lamp = fan;
    lamp.classname = "render_light";
    lamp.properties = {{"required_target", "power"}, {"start_delay", "0.5"}};
    level.entities = {fan, steam, lamp};
    auto effects = RelayGame::build_world_environment(level);
    RelayGame::update_world_environment(effects, {}, 2);
    CHECK(effects[0].strength == 0);
    CHECK(effects[1].strength > 0.95F);
    CHECK(effects[2].strength == 0);
    const std::vector<std::string> targets{"power", "cooling"};
    RelayGame::update_world_environment(effects, targets, 0.1F);
    CHECK(effects[0].strength > 0);
    CHECK(effects[0].strength < 0.1F);
    CHECK(effects[2].strength == 0);
    for (int i = 0; i < 120; ++i)
        RelayGame::update_world_environment(effects, targets, 0.05F);
    CHECK(effects[0].strength > 0.98F);
    CHECK(effects[1].strength < 0.001F);
    CHECK(effects[2].strength == 1);
    const auto phase = effects[0].phase;
    RelayGame::update_world_environment(effects, targets, 0);
    CHECK(effects[0].phase == phase);
    RelayGame::update_world_environment(effects, {}, 0.1F);
    CHECK(effects[0].strength > 0.8F);
    CHECK(effects[2].strength == 0);
}

TEST_CASE("environment rejects invalid authored effects")
{
    RelayCore::BlockoutLevel level;
    RelayCore::EntitySpawnData effect;
    effect.classname = "world_effect";
    effect.properties = {{"kind", "steam"}, {"scale", "nan"}};
    level.entities.push_back(effect);
    CHECK_THROWS_WITH(
        (void) RelayGame::build_world_environment(level),
        "world_effect.scale must be finite numeric data"
    );
    level.entities[0].properties = {{"kind", "decal"}, {"mark", "unknown"}};
    CHECK_THROWS((void) RelayGame::build_world_environment(level));
    level.entities[0].properties = {{"kind", "fan"}, {"scale", ""}};
    CHECK_THROWS((void) RelayGame::build_world_environment(level));
}

TEST_CASE("lamp fixtures reject invalid dimensions and follow stop and damaged states")
{
    RelayCore::BlockoutLevel level;
    RelayCore::EntitySpawnData lamp;
    lamp.classname = "render_light";
    lamp.properties =
        {{"stop_target", "running"}, {"state", "damaged"}, {"fixture_width", "1.2"}, {"fixture_depth", "0.28"}};
    level.entities = {lamp};
    auto effects = RelayGame::build_world_environment(level);
    CHECK(effects[0].strength == doctest::Approx(.08F));
    RelayGame::update_world_environment(effects, {}, .3F);
    CHECK(effects[0].strength == 1);
    const std::vector<std::string> running{"running"};
    RelayGame::update_world_environment(effects, running, 0);
    CHECK(effects[0].strength == 0);
    RelayGame::update_world_environment(effects, {}, 0);
    CHECK(effects[0].strength == 1);
    for (const auto* key : {"fixture_width", "fixture_depth"})
    {
        level.entities[0].properties[key] = "-1";
        CHECK_THROWS_WITH(
            (void) RelayGame::build_world_environment(level),
            "render_light: fixture_width must be >= 0 and fixture_depth > 0"
        );
        level.entities[0].properties[key] = "nan";
        CHECK_THROWS((void) RelayGame::build_world_environment(level));
        level.entities[0].properties[key] = "1";
    }
    level.entities[0].properties["fixture_width"] = "0";
    CHECK_NOTHROW((void) RelayGame::build_world_environment(level));
    level.entities[0].properties["fixture_depth"] = "0";
    CHECK_THROWS((void) RelayGame::build_world_environment(level));
}
