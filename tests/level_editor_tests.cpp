#include "relaycore/core/paths.h"
#include "relaycore/world/blockout_loader.h"
#include "relaygame/level_editor_document.h"
#include "relaygame/level_session_builder.h"
#include "relaygame/static_props.h"
#include "relaygame/usable_interaction.h"

#include <algorithm>
#include <doctest/doctest.h>

#include "scoped_temp_directory.h"
#include "scoped_test_logger.h"

TEST_CASE("editor decals attach to surfaces and survive save reload undo")
{
    RelayCore::test::ScopedTempDirectory temp;
    RelayCore::test::ScopedTestLogger logger{temp.path() / "logs"};
    RelayCore::BlockoutLevel level;
    level.solids.push_back({{-2, 0, 4}, {2, 3, 4.2F}, RelayCore::SurfaceMaterial::Wall});
    RelayGame::LevelEditorDocument document;
    document.reset(level);
    document.add_effect({0, 1, 0}, "decal");
    document.set_property("mark", "02");
    REQUIRE(document.attach_decal({0, 1.5F, 0}, {0, 0, 1}));
    CHECK(document.level().entities.back().origin.z == doctest::Approx(3.992F));
    CHECK(document.level().entities.back().angles.x == 180);
    document.save(temp.path() / "decal.toml");
    const auto loaded = RelayCore::load_blockout_level(temp.path() / "decal.toml");
    CHECK(loaded.entities.back().properties.at("mark") == "02");
    CHECK(loaded.entities.back().angles.x == 180);
    document.undo();
    CHECK(document.level().entities.back().origin.z == 0);
    document.redo();
    CHECK(document.level().entities.back().origin.z == doctest::Approx(3.992F));
}

TEST_CASE("editor transforms undo and redo without touching the original level")
{
    RelayCore::BlockoutLevel original;
    original.solids.push_back({{-1, 0, -1}, {1, 2, 1}, RelayCore::SurfaceMaterial::Wall});
    RelayGame::LevelEditorDocument document;
    document.reset(original);
    document.select(document.pick({0, 1, -5}, {0, 0, 1}));
    REQUIRE(document.selection().has_value());
    document.transform({3, 1, 0}, {2, 2, 2});
    CHECK(document.level().solids[0].mins.x == 2);
    CHECK(original.solids[0].mins.x == -1);
    document.undo();
    CHECK(document.level().solids[0].mins.x == -1);
    document.redo();
    CHECK(document.level().solids[0].mins.x == 2);
    document.duplicate();
    REQUIRE(document.level().solids.size() == 2);
    document.erase();
    CHECK(document.level().solids.size() == 1);
    document.undo();
    CHECK(document.level().solids.size() == 2);
    CHECK_THROWS_AS(document.transform({0, 0, 0}, {1, 0, 1}), std::invalid_argument);
}

TEST_CASE("editor save roundtrip preserves entity properties and supports replacing the user copy")
{
    RelayCore::test::ScopedTempDirectory temp;
    RelayCore::test::ScopedTestLogger logger{temp.path() / "logs"};
    RelayGame::LevelEditorDocument document;
    document.reset({});
    document.add_solid({0, -0.1F, 0}, {10, 0.2F, 10}, RelayCore::SurfaceMaterial::Floor);
    document.place_spawn({0, 0.01F, 0});
    document.add_door({0, 0, 3});
    document.add_light({0, 2, 0});
    const auto path = temp.path() / "levels" / "editor.toml";
    document.save(path);
    CHECK_FALSE(document.dirty());
    const auto loaded = RelayCore::load_blockout_level(path);
    REQUIRE(loaded.entities.size() == document.level().entities.size());
    for (std::size_t i = 0; i < loaded.entities.size(); ++i)
        CHECK(loaded.entities[i].properties == document.level().entities[i].properties);
    RelayCore::core::GameplayConfig config;
    const auto session = RelayGame::LevelSessionBuilder{config}.build(loaded, path);
    REQUIRE(session.doors.size() == 1);
    REQUIRE(session.usables.size() == 1);
    CHECK(session.usables[0].target == session.doors[0].name);
    document.add_solid({4, 1, 0}, {1, 2, 1}, RelayCore::SurfaceMaterial::Crate);
    document.save(path);
    CHECK(RelayCore::load_blockout_level(path).solids.size() == 2);
}

TEST_CASE("editor protects the spawn and linked doors and gives duplicates unique names")
{
    RelayGame::LevelEditorDocument document;
    document.reset({});
    document.place_spawn({0, 0, 0});
    CHECK_THROWS(document.duplicate());
    CHECK_THROWS(document.erase());
    document.add_door({0, 0, 0});
    CHECK_THROWS(document.erase());
    document.select(RelayGame::EditorSelection{true, 2});
    document.duplicate();
    CHECK(
        document.level().entities[2].properties.at("targetname")
        != document.level().entities[3].properties.at("targetname")
    );
    CHECK(document.level().entities[2].properties.at("target") == document.level().entities[3].properties.at("target"));
    document.place_spawn({1, 0, 1});
    CHECK(document.level().entities.size() == 4);
}

TEST_CASE("editor flattened episode preserves the full mission and authored values")
{
    RelayCore::test::ScopedTempDirectory temp;
    RelayCore::test::ScopedTestLogger logger{temp.path() / "logs"};
    const auto original = RelayCore::load_blockout_level(RelayCore::core::asset_path("levels/power_restoration.toml"));
    RelayGame::LevelEditorDocument document;
    document.reset(original);
    const auto path = temp.path() / "episode.toml";
    document.save(path);
    const auto loaded = RelayCore::load_blockout_level(path);
    REQUIRE(loaded.solids.size() == original.solids.size());
    REQUIRE(loaded.entities.size() == original.entities.size());
    for (std::size_t i = 0; i < original.solids.size(); ++i)
    {
        CHECK(loaded.solids[i].mins == original.solids[i].mins);
        CHECK(loaded.solids[i].maxs == original.solids[i].maxs);
        CHECK(loaded.solids[i].tint == original.solids[i].tint);
        CHECK(loaded.solids[i].material == original.solids[i].material);
    }
    for (std::size_t i = 0; i < original.entities.size(); ++i)
    {
        CHECK(loaded.entities[i].origin == original.entities[i].origin);
        CHECK(loaded.entities[i].properties == original.entities[i].properties);
    }
    RelayCore::core::GameplayConfig config;
    auto session = RelayGame::LevelSessionBuilder{config}.build(loaded, path);
    REQUIRE(session.mission_definition.steps.size() == 6);
    REQUIRE(session.ending.has_value());
    std::vector<std::string> inventory, activated, signaled;
    for (const auto* name : {"spare_fuse", "power_breaker", "cooling_control", "airlock_reader"})
    {
        auto usable = std::ranges::find(session.usables, std::string{name}, &RelayCore::gameplay::RuntimeUsable::name);
        REQUIRE(usable != session.usables.end());
        CHECK(
            RelayGame::apply_usable_interaction(*usable, session.doors, session.usables, inventory, activated, signaled)
                .primary_applied
        );
    }
}

TEST_CASE("editor floor placement uses the selected object's bottom and undo restores height")
{
    RelayGame::LevelEditorDocument document;
    document.reset({});
    document.add_solid({0, -0.1F, 0}, {10, 0.2F, 10}, RelayCore::SurfaceMaterial::Floor);
    document.add_solid({0, 3, 0}, {1, 2, 1}, RelayCore::SurfaceMaterial::Crate);
    document.drop_to_floor();
    CHECK(document.level().solids[1].mins.y == doctest::Approx(0));
    document.undo();
    CHECK(document.level().solids[1].mins.y == 2);
    document.place_spawn({3, 3, 0});
    document.drop_to_floor();
    CHECK(document.level().entities[0].origin.y == doctest::Approx(0));
}

TEST_CASE("editor markers do not swallow scene picks from inside the player spawn")
{
    RelayGame::LevelEditorDocument document;
    document.reset({});
    document.place_spawn({0, 0, 0});
    document.add_light({0, 1, 2});
    document.add_solid({0, 1, 4}, {3, 2, 0.2F}, RelayCore::SurfaceMaterial::Wall);
    const auto picked = document.pick({0, 1, 0}, {0, 0, 1});
    REQUIRE(picked.has_value());
    CHECK_FALSE(picked->entity);
    CHECK(picked->index == 0);
}

TEST_CASE("editor room model placement and links survive two saved maps")
{
    RelayCore::test::ScopedTempDirectory temp;
    RelayCore::test::ScopedTestLogger logger{temp.path() / "logs"};
    RelayGame::LevelEditorDocument document;
    document.new_room();
    CHECK(document.validation_messages().empty());
    document.select(RelayGame::EditorSelection{true, 1});
    document.set_property("targetname", "workshop_door");
    document.add_door_objective();
    document.add_model({2, 2, 0}, "models/service_cylinder.obj", "models/service_cylinder.bmp");
    document.set_model_collision(true);
    REQUIRE(document.place_on_surface({2, 2, 0}, {0, -1, 0}, .25F));
    CHECK(document.bounds(*document.selection()).mins.y == doctest::Approx(.005F));
    const auto position = document.level().entities.back().origin;
    document.set_property("scale", "2 1 1");
    CHECK(
        document.bounds(*document.selection()).maxs.x - document.bounds(*document.selection()).mins.x
        == doctest::Approx(1)
    );
    document.undo();
    CHECK(document.level().entities.back().origin == position);
    const auto first = temp.path() / "first.toml", second = temp.path() / "second.toml";
    document.save(first);
    document.duplicate();
    document.save(second);
    CHECK(
        RelayCore::load_blockout_level(second).entities.size()
        == RelayCore::load_blockout_level(first).entities.size() + 1
    );
    const RelayCore::core::GameplayConfig config;
    const auto session = RelayGame::LevelSessionBuilder{config}.build(first);
    CHECK(session.usables[0].target == "workshop_door");
    CHECK(session.mission_definition.steps[0].target == "workshop_door");
    CHECK(session.level.prop_colliders.size() == 1);
    document.select(RelayGame::EditorSelection{true, 2});
    document.set_property("target", "missing_door");
    CHECK_FALSE(document.validation_messages().empty());
}

TEST_CASE("editor reports a disconnected exit")
{
    RelayGame::LevelEditorDocument document;
    document.new_room();
    document.add_solid({0, 1.5F, 1}, {8, 3, .5F}, RelayCore::SurfaceMaterial::Wall);
    const auto issues = document.validation_messages();
    CHECK(std::ranges::any_of(issues, [](const auto& text) {
        return text.find("Нет пешего пути") != std::string::npos;
    }));
}

TEST_CASE("linked user maps still resolve after their folder is moved")
{
    RelayCore::test::ScopedTempDirectory temp;
    RelayCore::test::ScopedTestLogger logger{temp.path() / "logs"};
    const auto original = temp.path() / "original";
    const auto moved = temp.path() / "moved";
    std::filesystem::create_directories(original);
    const auto first = original / "workshop.toml";
    const auto second = original / "pump.toml";

    RelayGame::LevelEditorDocument pump;
    pump.new_room();
    pump.add_level_link({0, .01F, -2}, true, {}, {});
    const auto entry = pump.level().entities.back().properties.at("targetname");
    pump.save(second);

    RelayGame::LevelEditorDocument workshop;
    workshop.new_room();
    const auto reference = RelayGame::make_level_exit_reference(first, second);
    CHECK(reference == "./pump.toml");
    workshop.add_level_link({0, 0, 4.5F}, false, reference, entry);
    workshop.save(first);
    std::filesystem::rename(original, moved);

    const RelayCore::core::GameplayConfig config;
    const auto relocated = moved / "workshop.toml";
    const auto destination = moved / "pump.toml";
    const auto session = RelayGame::LevelSessionBuilder{config}.build(relocated);
    REQUIRE(session.exits.size() == 1);
    CHECK(session.exits[0].destination == destination);
    CHECK_NOTHROW(static_cast<void>(RelayGame::LevelSessionBuilder{config}.build_at_entry(destination, entry)));
    CHECK(
        RelayGame::resolve_level_exit_destination(relocated, "levels/foundation_pump.toml")
        == RelayCore::core::asset_path("levels/foundation_pump.toml")
    );
    CHECK(RelayGame::resolve_level_exit_destination(relocated, destination.generic_string()) == destination);

    std::filesystem::rename(destination, moved / "missing-pump.toml");
    const auto issues = workshop.validation_messages(relocated);
    CHECK(std::ranges::any_of(issues, [&](const auto& issue) {
        return issue.find(destination.string()) != std::string::npos && issue.find(entry) != std::string::npos;
    }));
}
