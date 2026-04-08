#include "relaycore/core/log.h"
#include "relaycore/world/blockout_loader.h"

#include <algorithm>
#include <doctest/doctest.h>
#include <filesystem>
#include <fstream>

TEST_CASE("blockout loader parses optional tint and brightness")
{
    RelayCore::log::shutdown();
    RelayCore::log::Config log_config{};
    log_config.enable_console_sink = false;
    log_config.enable_timestamped_log = false;
    log_config.logs_directory = (std::filesystem::temp_directory_path() / "relaycore_test_logs").string();
    RelayCore::log::init(log_config);

    const std::filesystem::path path =
        std::filesystem::temp_directory_path() / "relaycore_blockout_loader_tint_brightness.toml";

    {
        std::ofstream out{path, std::ios::trunc};
        REQUIRE(out.is_open());
        out << "[[solid]]\n";
        out << "mins = [0.0, 0.0, 0.0]\n";
        out << "maxs = [1.0, 2.0, 3.0]\n";
        out << "material = \"Wall\"\n";
        out << "tint = [0.8, 1.0, 1.2]\n";
        out << "brightness = 1.35\n";
    }

    const RelayCore::BlockoutLevel level = RelayCore::load_blockout_level(path);

    REQUIRE(level.solids.size() == 1);
    CHECK(level.solids.front().tint.x == doctest::Approx(0.8F));
    CHECK(level.solids.front().tint.y == doctest::Approx(1.0F));
    CHECK(level.solids.front().tint.z == doctest::Approx(1.2F));
    CHECK(level.solids.front().brightness == doctest::Approx(1.35F));

    CHECK(std::filesystem::remove(path));
    RelayCore::log::shutdown();
}

TEST_CASE("blockout loader supports grouped solids and native TOML entity values")
{
    RelayCore::log::shutdown();
    RelayCore::log::Config log_config{};
    log_config.enable_console_sink = false;
    log_config.enable_timestamped_log = false;
    log_config.logs_directory = (std::filesystem::temp_directory_path() / "relaycore_test_logs").string();
    RelayCore::log::init(log_config);

    const std::filesystem::path path =
        std::filesystem::temp_directory_path() / "relaycore_blockout_loader_grouped_format.toml";

    {
        std::ofstream out{path, std::ios::trunc};
        REQUIRE(out.is_open());
        out << "[[solid_group]]\n";
        out << "material = \"Wall\"\n";
        out << "tint = [0.7, 0.8, 0.9]\n";
        out << "brightness = 1.15\n";
        out << "boxes = [\n";
        out << "  { mins = [0.0, 0.0, 0.0], maxs = [1.0, 2.0, 3.0] },\n";
        out << "  { mins = [2.0, 0.0, 0.0], maxs = [3.0, 2.0, 3.0], material = \"Glass\", brightness = 1.05 }\n";
        out << "]\n\n";
        out << "[[entity_group]]\n";
        out << "classname = \"gameplay_entity\"\n";
        out << "kind = \"panel\"\n";
        out << "size = [0.3, 0.5, 0.1]\n";
        out << "use_radius = 1.9\n";
        out << "entries = [\n";
        out << "  { origin = [0.0, 1.0, 0.0], target = \"door_a\", label = \"Panel A\" },\n";
        out << "  { origin = [1.0, 1.0, 0.0], target = \"door_b\", label = \"Panel B\", enabled = false }\n";
        out << "]\n\n";
        out << "[[entity]]\n";
        out << "classname = \"gameplay_entity\"\n";
        out << "kind = \"door\"\n";
        out << "origin = [0.0, 0.0, 1.0]\n";
        out << "size = [1.2, 2.4, 0.2]\n";
        out << "targetname = \"door_a\"\n\n";
        out << "[[entity]]\n";
        out << "classname = \"gameplay_entity\"\n";
        out << "kind = \"door\"\n";
        out << "origin = [2.0, 0.0, 1.0]\n";
        out << "size = [1.2, 2.4, 0.2]\n";
        out << "targetname = \"door_b\"\n\n";
        out << "[[entity]]\n";
        out << "classname = \"render_light\"\n";
        out << "origin = [0.0, 2.0, 0.0]\n";
        out << "color = [0.14, 0.20, 0.28]\n";
        out << "radius = 5.6\n";
        out << "intensity = 0.96\n";
    }

    const RelayCore::BlockoutLevel level = RelayCore::load_blockout_level(path);

    REQUIRE(level.solids.size() == 2);
    CHECK(level.solids[0].material == RelayCore::SurfaceMaterial::Wall);
    CHECK(level.solids[0].tint.x == doctest::Approx(0.7F));
    CHECK(level.solids[0].brightness == doctest::Approx(1.15F));
    CHECK(level.solids[1].material == RelayCore::SurfaceMaterial::Glass);
    CHECK(level.solids[1].brightness == doctest::Approx(1.05F));

    REQUIRE(level.entities.size() == 5);
    const auto first_panel = std::find_if(level.entities.begin(), level.entities.end(), [](const auto& entity) {
        const auto kind = entity.properties.find("kind");
        return entity.classname == "gameplay_entity" && kind != entity.properties.end() && kind->second == "panel"
               && entity.origin.x == doctest::Approx(0.0F);
    });
    REQUIRE(first_panel != level.entities.end());
    CHECK(first_panel->properties.at("size") == "0.3 0.5 0.1");
    CHECK(first_panel->properties.at("use_radius") == "1.9");

    const auto second_panel = std::find_if(level.entities.begin(), level.entities.end(), [](const auto& entity) {
        const auto target = entity.properties.find("target");
        return entity.classname == "gameplay_entity" && target != entity.properties.end() && target->second == "door_b";
    });
    REQUIRE(second_panel != level.entities.end());
    CHECK(second_panel->properties.at("enabled") == "false");

    const auto light = std::find_if(level.entities.begin(), level.entities.end(), [](const auto& entity) {
        return entity.classname == "render_light";
    });
    REQUIRE(light != level.entities.end());
    CHECK(light->properties.at("color") == "0.14 0.2 0.28");
    CHECK(light->properties.at("radius") == "5.6");
    CHECK(light->properties.at("intensity") == "0.96");

    const bool removed = std::filesystem::remove(path);
    CHECK(removed);
    RelayCore::log::shutdown();
}

TEST_CASE("blockout loader applies group offsets to solids and entities")
{
    RelayCore::log::shutdown();
    RelayCore::log::Config log_config{};
    log_config.enable_console_sink = false;
    log_config.enable_timestamped_log = false;
    log_config.logs_directory = (std::filesystem::temp_directory_path() / "relaycore_test_logs").string();
    RelayCore::log::init(log_config);

    const std::filesystem::path path =
        std::filesystem::temp_directory_path() / "relaycore_blockout_loader_group_offsets.toml";

    {
        std::ofstream out{path, std::ios::trunc};
        REQUIRE(out.is_open());
        out << "[[solid_group]]\n";
        out << "material = \"Post\"\n";
        out << "offset = [10.0, 0.0, -2.0]\n";
        out << "entries = [\n";
        out << "  { origin = [0.0, 1.0, 0.0], size = [1.0, 2.0, 1.0] },\n";
        out << "  { mins = [-1.0, 0.0, -1.0], maxs = [1.0, 1.0, 1.0], material = \"Beam\" }\n";
        out << "]\n\n";
        out << "[[entity_group]]\n";
        out << "classname = \"gameplay_entity\"\n";
        out << "kind = \"panel\"\n";
        out << "offset = [3.0, 0.0, 5.0]\n";
        out << "entries = [\n";
        out << "  { origin = [0.0, 1.0, 0.0], target = \"door_a\", label = \"Panel A\" }\n";
        out << "]\n\n";
        out << "[[entity]]\n";
        out << "classname = \"gameplay_entity\"\n";
        out << "kind = \"door\"\n";
        out << "origin = [5.0, 0.0, 5.0]\n";
        out << "size = [1.2, 2.4, 0.2]\n";
        out << "targetname = \"door_a\"\n";
    }

    const RelayCore::BlockoutLevel level = RelayCore::load_blockout_level(path);

    REQUIRE(level.solids.size() == 2);
    CHECK(level.solids[0].mins.x == doctest::Approx(9.5F));
    CHECK(level.solids[0].maxs.z == doctest::Approx(-1.5F));
    CHECK(level.solids[1].mins.x == doctest::Approx(9.0F));
    CHECK(level.solids[1].maxs.z == doctest::Approx(-1.0F));

    REQUIRE(level.entities.size() == 2);
    const auto panel = std::find_if(level.entities.begin(), level.entities.end(), [](const auto& entity) {
        const auto kind = entity.properties.find("kind");
        const auto label = entity.properties.find("label");
        return kind != entity.properties.end() && kind->second == "panel" && label != entity.properties.end()
               && label->second == "Panel A";
    });
    REQUIRE(panel != level.entities.end());
    CHECK(panel->origin.x == doctest::Approx(3.0F));
    CHECK(panel->origin.y == doctest::Approx(1.0F));
    CHECK(panel->origin.z == doctest::Approx(5.0F));
    CHECK(panel->properties.at("target") == "door_a");
    CHECK(panel->properties.contains("offset") == false);

    const bool removed = std::filesystem::remove(path);
    CHECK(removed);
    RelayCore::log::shutdown();
}

TEST_CASE("blockout loader supports solid origin size syntax")
{
    RelayCore::log::shutdown();
    RelayCore::log::Config log_config{};
    log_config.enable_console_sink = false;
    log_config.enable_timestamped_log = false;
    log_config.logs_directory = (std::filesystem::temp_directory_path() / "relaycore_test_logs").string();
    RelayCore::log::init(log_config);

    const std::filesystem::path path =
        std::filesystem::temp_directory_path() / "relaycore_blockout_loader_origin_size.toml";

    {
        std::ofstream out{path, std::ios::trunc};
        REQUIRE(out.is_open());
        out << "[[solid]]\n";
        out << "origin = [2.0, 1.0, -3.0]\n";
        out << "size = [4.0, 2.0, 6.0]\n";
        out << "material = \"Crate\"\n\n";
        out << "[[solid_group]]\n";
        out << "material = \"Pillar\"\n";
        out << "size = [1.0, 4.0, 1.0]\n";
        out << "entries = [\n";
        out << "  { origin = [-2.0, 2.0, 0.0] },\n";
        out << "  { origin = [2.0, 2.0, 0.0], brightness = 1.1 }\n";
        out << "]\n";
    }

    const RelayCore::BlockoutLevel level = RelayCore::load_blockout_level(path);

    REQUIRE(level.solids.size() == 3);
    CHECK(level.solids[0].mins.x == doctest::Approx(0.0F));
    CHECK(level.solids[0].maxs.y == doctest::Approx(2.0F));
    CHECK(level.solids[0].mins.z == doctest::Approx(-6.0F));
    CHECK(level.solids[1].mins.x == doctest::Approx(-2.5F));
    CHECK(level.solids[1].maxs.y == doctest::Approx(4.0F));
    CHECK(level.solids[2].maxs.x == doctest::Approx(2.5F));
    CHECK(level.solids[2].brightness == doctest::Approx(1.1F));

    CHECK(std::filesystem::remove(path));
    RelayCore::log::shutdown();
}

TEST_CASE("blockout loader supports solid template instances")
{
    RelayCore::log::shutdown();
    RelayCore::log::Config log_config{};
    log_config.enable_console_sink = false;
    log_config.enable_timestamped_log = false;
    log_config.logs_directory = (std::filesystem::temp_directory_path() / "relaycore_test_logs").string();
    RelayCore::log::init(log_config);

    const std::filesystem::path path =
        std::filesystem::temp_directory_path() / "relaycore_blockout_loader_template_instances.toml";

    {
        std::ofstream out{path, std::ios::trunc};
        REQUIRE(out.is_open());
        out << "[[solid_template]]\n";
        out << "name = \"bay_shell\"\n";
        out << "entries = [\n";
        out << "  { origin = [0.0, 0.1, 0.0], size = [4.0, 0.2, 6.0], material = \"Floor\" },\n";
        out << "  { origin = [2.0, 1.0, 0.0], size = [1.0, 2.0, 1.0], material = \"Crate\" }\n";
        out << "]\n\n";
        out << "[[solid_instance]]\n";
        out << "template = \"bay_shell\"\n\n";
        out << "[[solid_instance]]\n";
        out << "template = \"bay_shell\"\n";
        out << "offset = [10.0, 0.0, -2.0]\n";
    }

    const RelayCore::BlockoutLevel level = RelayCore::load_blockout_level(path);

    REQUIRE(level.solids.size() == 4);
    CHECK(level.solids[0].mins.x == doctest::Approx(-2.0F));
    CHECK(level.solids[1].maxs.x == doctest::Approx(2.5F));
    CHECK(level.solids[2].mins.x == doctest::Approx(8.0F));
    CHECK(level.solids[2].maxs.z == doctest::Approx(1.0F));
    CHECK(level.solids[3].mins.z == doctest::Approx(-2.5F));

    std::filesystem::remove(path);
    RelayCore::log::shutdown();
}

TEST_CASE("blockout loader supports module template instances")
{
    RelayCore::log::shutdown();
    RelayCore::log::Config log_config{};
    log_config.enable_console_sink = false;
    log_config.enable_timestamped_log = false;
    log_config.logs_directory = (std::filesystem::temp_directory_path() / "relaycore_test_logs").string();
    RelayCore::log::init(log_config);

    const std::filesystem::path path =
        std::filesystem::temp_directory_path() / "relaycore_blockout_loader_module_instances.toml";

    {
        std::ofstream out{path, std::ios::trunc};
        REQUIRE(out.is_open());
        out << "[[module_template]]\n";
        out << "name = \"bulkhead_stub\"\n";
        out << "solids = [\n";
        out << "  { origin = [0.0, 1.0, 0.0], size = [2.0, 2.0, 1.0], material = \"Post\" }\n";
        out << "]\n";
        out << "entities = [\n";
        out << "  { classname = \"gameplay_entity\", kind = \"door\", origin = [1.0, 0.0, 0.0], size = [2.0, 3.0, "
               "0.2], move_offset = [2.0, 0.0, 0.0], targetname = \"door_a\" },\n";
        out << "  { classname = \"gameplay_entity\", kind = \"pickup\", origin = [-1.0, 1.0, 0.0], size = [0.2, 0.1, "
               "0.02], item_id = \"keycard_a\", item_name = \"Keycard A\" },\n";
        out << "  { classname = \"gameplay_entity\", kind = \"panel\", origin = [0.0, 1.2, 1.0], size = [0.3, 0.5, "
               "0.1], mount_axis = \"+x\", target = \"door_a\", required_item = \"keycard_a\", label = \"Panel A\" }\n";
        out << "]\n";
        out << "anchors = [\n";
        out << "  { name = \"service_point\", origin = [0.0, 1.2, 1.0] }\n";
        out << "]\n\n";
        out << "[[module_instance]]\n";
        out << "template = \"bulkhead_stub\"\n";
        out << "name = \"entry_a\"\n";
        out << "name_prefix = \"entry_a/\"\n";
        out << "offset = [10.0, 0.0, 5.0]\n";
        out << "rotation_y = 90\n\n";
        out << "[[entity_group]]\n";
        out << "classname = \"gameplay_entity\"\n";
        out << "kind = \"panel\"\n";
        out << "entries = [\n";
        out << "  { anchor = \"entry_a:service_point\", origin = [0.2, 0.0, 0.0], target = \"entry_a/door_a\", label = "
               "\"Attached Panel\" }\n";
        out << "]\n";
    }

    const RelayCore::BlockoutLevel level = RelayCore::load_blockout_level(path);

    REQUIRE(level.solids.size() == 1);
    CHECK(level.solids[0].mins.x == doctest::Approx(9.5F));
    CHECK(level.solids[0].maxs.z == doctest::Approx(6.0F));

    REQUIRE(level.entities.size() == 4);
    const auto door = std::find_if(level.entities.begin(), level.entities.end(), [](const auto& entity) {
        const auto kind = entity.properties.find("kind");
        return kind != entity.properties.end() && kind->second == "door";
    });
    REQUIRE(door != level.entities.end());
    CHECK(door->origin.x == doctest::Approx(10.0F));
    CHECK(door->origin.z == doctest::Approx(4.0F));
    CHECK(door->properties.at("size") == "0.2 3 2");
    CHECK(door->properties.at("move_offset") == "0 0 -2");
    CHECK(door->properties.at("targetname") == "entry_a/door_a");

    const auto pickup = std::find_if(level.entities.begin(), level.entities.end(), [](const auto& entity) {
        const auto kind = entity.properties.find("kind");
        return kind != entity.properties.end() && kind->second == "pickup";
    });
    REQUIRE(pickup != level.entities.end());
    CHECK(pickup->origin.x == doctest::Approx(10.0F));
    CHECK(pickup->origin.z == doctest::Approx(6.0F));
    CHECK(pickup->properties.at("item_id") == "entry_a/keycard_a");

    const auto panel = std::find_if(level.entities.begin(), level.entities.end(), [](const auto& entity) {
        const auto kind = entity.properties.find("kind");
        const auto label = entity.properties.find("label");
        return kind != entity.properties.end() && kind->second == "panel" && label != entity.properties.end()
               && label->second == "Panel A";
    });
    REQUIRE(panel != level.entities.end());
    CHECK(panel->origin.x == doctest::Approx(11.0F));
    CHECK(panel->origin.z == doctest::Approx(5.0F));
    CHECK(panel->properties.at("mount_axis") == "-z");
    CHECK(panel->properties.at("target") == "entry_a/door_a");
    CHECK(panel->properties.at("required_item") == "entry_a/keycard_a");

    const auto attached_panel = std::find_if(level.entities.begin(), level.entities.end(), [](const auto& entity) {
        const auto label = entity.properties.find("label");
        return label != entity.properties.end() && label->second == "Attached Panel";
    });
    REQUIRE(attached_panel != level.entities.end());
    CHECK(attached_panel->origin.x == doctest::Approx(11.2F));
    CHECK(attached_panel->origin.y == doctest::Approx(1.2F));
    CHECK(attached_panel->origin.z == doctest::Approx(5.0F));
    CHECK(attached_panel->properties.at("target") == "entry_a/door_a");
    CHECK(attached_panel->properties.contains("anchor") == false);

    std::filesystem::remove(path);
    RelayCore::log::shutdown();
}

TEST_CASE("blockout loader loads shipped facility levels")
{
    RelayCore::log::shutdown();
    RelayCore::log::Config log_config{};
    log_config.enable_console_sink = false;
    log_config.logs_directory = (std::filesystem::temp_directory_path() / "relaycore_test_logs").string();
    RelayCore::log::init(log_config);

    const std::filesystem::path repo_root = std::filesystem::path(__FILE__).parent_path().parent_path();
    const std::filesystem::path levels_dir = repo_root / "assets" / "levels";

    const std::filesystem::path test_room = levels_dir / "test_room.toml";
    const std::filesystem::path stress_room_01 = levels_dir / "stress_room_01.toml";
    const std::filesystem::path stress_room_02 = levels_dir / "stress_room_02.toml";

    const RelayCore::BlockoutLevel level_a = RelayCore::load_blockout_level(test_room);
    const RelayCore::BlockoutLevel level_b = RelayCore::load_blockout_level(stress_room_01);
    const RelayCore::BlockoutLevel level_c = RelayCore::load_blockout_level(stress_room_02);

    CHECK(level_a.solids.size() >= 24);
    CHECK(level_a.entities.size() >= 8);
    CHECK(level_b.solids.size() >= 40);
    CHECK(level_b.entities.size() >= 5);
    CHECK(level_c.solids.size() >= 70);
    CHECK(level_c.entities.size() >= 8);

    RelayCore::log::shutdown();
}

TEST_CASE("blockout loader rejects degenerate solids")
{
    RelayCore::log::shutdown();
    RelayCore::log::Config log_config{};
    log_config.enable_console_sink = false;
    log_config.enable_timestamped_log = false;
    log_config.logs_directory = (std::filesystem::temp_directory_path() / "relaycore_test_logs").string();
    RelayCore::log::init(log_config);

    const std::filesystem::path path =
        std::filesystem::temp_directory_path() / "relaycore_blockout_loader_invalid_degenerate_solid.toml";

    {
        std::ofstream out{path, std::ios::trunc};
        REQUIRE(out.is_open());
        out << "[[solid]]\n";
        out << "mins = [0.0, 0.0, 0.0]\n";
        out << "maxs = [2.0, 2.0, 0.0]\n";
        out << "material = \"WallPanel\"\n";
    }

    CHECK_THROWS_WITH(
        [&]() {
            (void) RelayCore::load_blockout_level(path);
        }(),
        doctest::Contains("Degenerate solid")
    );

    const bool removed = std::filesystem::remove(path);
    CHECK(removed);
    RelayCore::log::shutdown();
}

TEST_CASE("blockout loader rejects duplicate entity targetnames")
{
    RelayCore::log::shutdown();
    RelayCore::log::Config log_config{};
    log_config.enable_console_sink = false;
    log_config.enable_timestamped_log = false;
    log_config.logs_directory = (std::filesystem::temp_directory_path() / "relaycore_test_logs").string();
    RelayCore::log::init(log_config);

    const std::filesystem::path path =
        std::filesystem::temp_directory_path() / "relaycore_blockout_loader_invalid_duplicate_targetname.toml";

    {
        std::ofstream out{path, std::ios::trunc};
        REQUIRE(out.is_open());
        out << "[[entity]]\n";
        out << "classname = \"gameplay_entity\"\n";
        out << "kind = \"door\"\n";
        out << "origin = [0.0, 0.0, 0.0]\n";
        out << "size = [2.0, 3.0, 0.2]\n";
        out << "targetname = \"door_a\"\n\n";
        out << "[[entity]]\n";
        out << "classname = \"gameplay_entity\"\n";
        out << "kind = \"door\"\n";
        out << "origin = [4.0, 0.0, 0.0]\n";
        out << "size = [2.0, 3.0, 0.2]\n";
        out << "targetname = \"door_a\"\n";
    }

    CHECK_THROWS_WITH(
        [&]() {
            (void) RelayCore::load_blockout_level(path);
        }(),
        doctest::Contains("Duplicate entity targetname")
    );

    const bool removed = std::filesystem::remove(path);
    CHECK(removed);
    RelayCore::log::shutdown();
}

TEST_CASE("blockout loader rejects gameplay targets that reference unknown targetnames")
{
    RelayCore::log::shutdown();
    RelayCore::log::Config log_config{};
    log_config.enable_console_sink = false;
    log_config.enable_timestamped_log = false;
    log_config.logs_directory = (std::filesystem::temp_directory_path() / "relaycore_test_logs").string();
    RelayCore::log::init(log_config);

    const std::filesystem::path path =
        std::filesystem::temp_directory_path() / "relaycore_blockout_loader_invalid_unknown_target.toml";

    {
        std::ofstream out{path, std::ios::trunc};
        REQUIRE(out.is_open());
        out << "[[entity]]\n";
        out << "classname = \"gameplay_entity\"\n";
        out << "kind = \"door\"\n";
        out << "origin = [0.0, 0.0, 0.0]\n";
        out << "size = [2.0, 3.0, 0.2]\n";
        out << "targetname = \"door_a\"\n\n";
        out << "[[entity]]\n";
        out << "classname = \"gameplay_entity\"\n";
        out << "kind = \"panel\"\n";
        out << "origin = [1.0, 1.0, 0.0]\n";
        out << "size = [0.3, 0.5, 0.1]\n";
        out << "target = \"door_missing\"\n";
    }

    CHECK_THROWS_WITH(
        [&]() {
            (void) RelayCore::load_blockout_level(path);
        }(),
        doctest::Contains("references unknown target")
    );

    const bool removed = std::filesystem::remove(path);
    CHECK(removed);
    RelayCore::log::shutdown();
}

TEST_CASE("blockout loader rejects duplicate module template anchor names")
{
    RelayCore::log::shutdown();
    RelayCore::log::Config log_config{};
    log_config.enable_console_sink = false;
    log_config.enable_timestamped_log = false;
    log_config.logs_directory = (std::filesystem::temp_directory_path() / "relaycore_test_logs").string();
    RelayCore::log::init(log_config);

    const std::filesystem::path path =
        std::filesystem::temp_directory_path() / "relaycore_blockout_loader_invalid_duplicate_anchor.toml";

    {
        std::ofstream out{path, std::ios::trunc};
        REQUIRE(out.is_open());
        out << "[[module_template]]\n";
        out << "name = \"bulkhead_stub\"\n";
        out << "solids = [\n";
        out << "  { origin = [0.0, 1.0, 0.0], size = [2.0, 2.0, 0.2], material = \"WallPanel\" }\n";
        out << "]\n";
        out << "anchors = [\n";
        out << "  { name = \"service_point\", origin = [0.0, 1.0, 0.0] },\n";
        out << "  { name = \"service_point\", origin = [1.0, 1.0, 0.0] }\n";
        out << "]\n";
    }

    CHECK_THROWS_WITH(
        [&]() {
            (void) RelayCore::load_blockout_level(path);
        }(),
        doctest::Contains("Duplicate module template anchor name")
    );

    const bool removed = std::filesystem::remove(path);
    CHECK(removed);
    RelayCore::log::shutdown();
}
