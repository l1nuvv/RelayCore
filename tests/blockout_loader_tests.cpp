#include "relaycore/core/log.h"
#include "relaycore/core/profiling.h"
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

TEST_CASE("blockout loader records authored source locations for solids and entities")
{
    RelayCore::log::shutdown();
    RelayCore::log::Config log_config{};
    log_config.enable_console_sink = false;
    log_config.enable_timestamped_log = false;
    log_config.logs_directory = (std::filesystem::temp_directory_path() / "relaycore_test_logs").string();
    RelayCore::log::init(log_config);

    const std::filesystem::path path =
        std::filesystem::temp_directory_path() / "relaycore_blockout_loader_source_locations.toml";

    {
        std::ofstream out{path, std::ios::trunc};
        REQUIRE(out.is_open());
        out << "name = \"source_locations\"\n";
        out << "version = 1\n\n";
        out << "[[solid]]\n";
        out << "mins = [0.0, 0.0, 0.0]\n";
        out << "maxs = [1.0, 2.0, 3.0]\n";
        out << "material = \"Wall\"\n\n";
        out << "[[entity]]\n";
        out << "classname = \"gameplay_entity\"\n";
        out << "kind = \"door\"\n";
        out << "origin = [0.0, 0.0, 2.0]\n";
        out << "size = [2.0, 3.0, 0.2]\n";
        out << "targetname = \"door_a\"\n";
    }

    const RelayCore::BlockoutLevel level = RelayCore::load_blockout_level(path);

    REQUIRE(level.solids.size() == 1);
    REQUIRE(level.entities.size() == 1);
    REQUIRE(level.solid_sources.size() == level.solids.size());
    REQUIRE(level.entity_sources.size() == level.entities.size());

    CHECK(level.solid_sources.front().valid());
    CHECK(std::filesystem::path(level.solid_sources.front().file_path) == path);
    CHECK(level.solid_sources.front().line == 4);
    CHECK(level.solid_sources.front().column == 1);
    CHECK(level.solid_sources.front().source_kind == "direct");
    CHECK(level.solid_sources.front().has_placement() == false);

    CHECK(level.entity_sources.front().valid());
    CHECK(std::filesystem::path(level.entity_sources.front().file_path) == path);
    CHECK(level.entity_sources.front().line == 9);
    CHECK(level.entity_sources.front().column == 1);
    CHECK(level.entity_sources.front().source_kind == "direct");
    CHECK(level.entity_sources.front().has_placement() == false);

    CHECK(std::filesystem::remove(path));
    RelayCore::log::shutdown();
}

TEST_CASE("blockout loader records placement provenance for solid template instances")
{
    RelayCore::log::shutdown();
    RelayCore::log::Config log_config{};
    log_config.enable_console_sink = false;
    log_config.enable_timestamped_log = false;
    log_config.logs_directory = (std::filesystem::temp_directory_path() / "relaycore_test_logs").string();
    RelayCore::log::init(log_config);

    const std::filesystem::path path =
        std::filesystem::temp_directory_path() / "relaycore_blockout_loader_solid_instance_provenance.toml";

    {
        std::ofstream out{path, std::ios::trunc};
        REQUIRE(out.is_open());
        out << "[[solid_template]]\n";
        out << "name = \"bay_shell\"\n";
        out << "entries = [\n";
        out << "  { origin = [0.0, 1.0, 0.0], size = [2.0, 2.0, 2.0], material = \"Floor\" }\n";
        out << "]\n\n";
        out << "[[solid_instance]]\n";
        out << "template = \"bay_shell\"\n";
        out << "offset = [10.0, 0.0, 0.0]\n";
    }

    const RelayCore::BlockoutLevel level = RelayCore::load_blockout_level(path);

    REQUIRE(level.solid_sources.size() == 1);
    const RelayCore::AuthoredSourceLocation& source = level.solid_sources.front();
    CHECK(source.valid());
    CHECK(std::filesystem::path(source.file_path) == path);
    CHECK(source.line == 4);
    CHECK(source.column > 0);
    CHECK(source.source_kind == "solid_instance");
    CHECK(source.template_name == "bay_shell");
    CHECK(source.instance_name.empty());
    CHECK(source.has_placement());
    CHECK(std::filesystem::path(source.placement_file_path) == path);
    CHECK(source.placement_line == 7);
    CHECK(source.placement_column == 1);

    CHECK(std::filesystem::remove(path));
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

TEST_CASE("blockout loader records placement provenance for module template instances")
{
    RelayCore::log::shutdown();
    RelayCore::log::Config log_config{};
    log_config.enable_console_sink = false;
    log_config.enable_timestamped_log = false;
    log_config.logs_directory = (std::filesystem::temp_directory_path() / "relaycore_test_logs").string();
    RelayCore::log::init(log_config);

    const std::filesystem::path path =
        std::filesystem::temp_directory_path() / "relaycore_blockout_loader_module_instance_provenance.toml";

    {
        std::ofstream out{path, std::ios::trunc};
        REQUIRE(out.is_open());
        out << "[[module_template]]\n";
        out << "name = \"bulkhead_stub\"\n";
        out << "entities = [\n";
        out << "  { classname = \"gameplay_entity\", kind = \"door\", origin = [0.0, 0.0, 2.0], size = [2.0, 3.0, "
               "0.2], targetname = \"door_a\" },\n";
        out << "  { classname = \"gameplay_entity\", kind = \"panel\", origin = [0.0, 1.0, 0.0], size = [0.3, 0.5, "
               "0.1], target = \"door_a\", label = \"Panel A\" }\n";
        out << "]\n\n";
        out << "[[module_instance]]\n";
        out << "template = \"bulkhead_stub\"\n";
        out << "name = \"entry_a\"\n";
        out << "offset = [10.0, 0.0, 0.0]\n";
    }

    const RelayCore::BlockoutLevel level = RelayCore::load_blockout_level(path);

    REQUIRE(level.entities.size() == 2);
    REQUIRE(level.entity_sources.size() == 2);
    const auto panel_it = std::find_if(level.entities.begin(), level.entities.end(), [](const auto& entity) {
        const auto label = entity.properties.find("label");
        return label != entity.properties.end() && label->second == "Panel A";
    });
    REQUIRE(panel_it != level.entities.end());
    const std::size_t panel_index = static_cast<std::size_t>(std::distance(level.entities.begin(), panel_it));
    const RelayCore::AuthoredSourceLocation& source = level.entity_sources[panel_index];
    CHECK(source.valid());
    CHECK(std::filesystem::path(source.file_path) == path);
    CHECK(source.line == 5);
    CHECK(source.column > 0);
    CHECK(source.source_kind == "module_instance");
    CHECK(source.template_name == "bulkhead_stub");
    CHECK(source.instance_name == "entry_a");
    CHECK(source.has_placement());
    CHECK(std::filesystem::path(source.placement_file_path) == path);
    CHECK(source.placement_line == 8);
    CHECK(source.placement_column == 1);

    CHECK(std::filesystem::remove(path));
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

TEST_CASE("blockout loader rejects unknown anchor references")
{
    RelayCore::log::shutdown();
    RelayCore::log::Config log_config{};
    log_config.enable_console_sink = false;
    log_config.enable_timestamped_log = false;
    log_config.logs_directory = (std::filesystem::temp_directory_path() / "relaycore_test_logs").string();
    RelayCore::log::init(log_config);

    const std::filesystem::path path =
        std::filesystem::temp_directory_path() / "relaycore_blockout_loader_invalid_unknown_anchor.toml";

    {
        std::ofstream out{path, std::ios::trunc};
        REQUIRE(out.is_open());
        out << "[[entity]]\n";
        out << "classname = \"gameplay_entity\"\n";
        out << "kind = \"panel\"\n";
        out << "anchor = \"missing_anchor\"\n";
        out << "origin = [0.0, 1.0, 0.0]\n";
        out << "size = [0.3, 0.5, 0.1]\n";
        out << "target = \"door_a\"\n\n";
        out << "[[entity]]\n";
        out << "classname = \"gameplay_entity\"\n";
        out << "kind = \"door\"\n";
        out << "origin = [0.0, 0.0, 1.0]\n";
        out << "size = [1.2, 2.4, 0.2]\n";
        out << "targetname = \"door_a\"\n";
    }

    CHECK_THROWS_WITH(
        [&]() {
            (void) RelayCore::load_blockout_level(path);
        }(),
        doctest::Contains("Unknown anchor reference")
    );

    const bool removed = std::filesystem::remove(path);
    CHECK(removed);
    RelayCore::log::shutdown();
}

TEST_CASE("blockout loader rejects activate targets that point at non-door entities")
{
    RelayCore::log::shutdown();
    RelayCore::log::Config log_config{};
    log_config.enable_console_sink = false;
    log_config.enable_timestamped_log = false;
    log_config.logs_directory = (std::filesystem::temp_directory_path() / "relaycore_test_logs").string();
    RelayCore::log::init(log_config);

    const std::filesystem::path path =
        std::filesystem::temp_directory_path() / "relaycore_blockout_loader_invalid_non_door_target.toml";

    {
        std::ofstream out{path, std::ios::trunc};
        REQUIRE(out.is_open());
        out << "[[entity]]\n";
        out << "classname = \"gameplay_entity\"\n";
        out << "kind = \"panel\"\n";
        out << "origin = [0.0, 1.0, 0.0]\n";
        out << "size = [0.3, 0.5, 0.1]\n";
        out << "targetname = \"reader_lock\"\n";
        out << "target = \"door_a\"\n";
        out << "signal_action = \"unlock\"\n\n";
        out << "[[entity]]\n";
        out << "classname = \"gameplay_entity\"\n";
        out << "kind = \"door\"\n";
        out << "origin = [2.0, 0.0, 0.0]\n";
        out << "size = [2.0, 3.0, 0.2]\n";
        out << "targetname = \"door_a\"\n\n";
        out << "[[entity]]\n";
        out << "classname = \"gameplay_entity\"\n";
        out << "kind = \"trigger\"\n";
        out << "origin = [0.0, 0.0, 0.0]\n";
        out << "size = [2.0, 2.0, 2.0]\n";
        out << "target = \"reader_lock\"\n";
    }

    CHECK_THROWS_WITH(
        [&]() {
            (void) RelayCore::load_blockout_level(path);
        }(),
        doctest::Contains("instead of a door")
    );

    const bool removed = std::filesystem::remove(path);
    CHECK(removed);
    RelayCore::log::shutdown();
}

TEST_CASE("blockout loader rejects signal receivers without targetname")
{
    RelayCore::log::shutdown();
    RelayCore::log::Config log_config{};
    log_config.enable_console_sink = false;
    log_config.enable_timestamped_log = false;
    log_config.logs_directory = (std::filesystem::temp_directory_path() / "relaycore_test_logs").string();
    RelayCore::log::init(log_config);

    const std::filesystem::path path =
        std::filesystem::temp_directory_path() / "relaycore_blockout_loader_invalid_signal_receiver.toml";

    {
        std::ofstream out{path, std::ios::trunc};
        REQUIRE(out.is_open());
        out << "[[entity]]\n";
        out << "classname = \"gameplay_entity\"\n";
        out << "kind = \"panel\"\n";
        out << "origin = [0.0, 1.0, 0.0]\n";
        out << "size = [0.3, 0.5, 0.1]\n";
        out << "signal_action = \"unlock\"\n";
    }

    CHECK_THROWS_WITH(
        [&]() {
            (void) RelayCore::load_blockout_level(path);
        }(),
        doctest::Contains("defines signal_action but has no targetname")
    );

    const bool removed = std::filesystem::remove(path);
    CHECK(removed);
    RelayCore::log::shutdown();
}

TEST_CASE("blockout loader rejects unknown chain targets")
{
    RelayCore::log::shutdown();
    RelayCore::log::Config log_config{};
    log_config.enable_console_sink = false;
    log_config.enable_timestamped_log = false;
    log_config.logs_directory = (std::filesystem::temp_directory_path() / "relaycore_test_logs").string();
    RelayCore::log::init(log_config);

    const std::filesystem::path path =
        std::filesystem::temp_directory_path() / "relaycore_blockout_loader_invalid_chain_target.toml";

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
        out << "kind = \"pickup\"\n";
        out << "origin = [0.0, 1.0, 0.0]\n";
        out << "size = [0.2, 0.1, 0.02]\n";
        out << "item_id = \"keycard_a\"\n";
        out << "chain_target = \"missing_target\"\n";
    }

    CHECK_THROWS_WITH(
        [&]() {
            (void) RelayCore::load_blockout_level(path);
        }(),
        doctest::Contains("references unknown chain_target")
    );

    const bool removed = std::filesystem::remove(path);
    CHECK(removed);
    RelayCore::log::shutdown();
}

TEST_CASE("blockout loader rejects non-positive gameplay entity sizes")
{
    RelayCore::log::shutdown();
    RelayCore::log::Config log_config{};
    log_config.enable_console_sink = false;
    log_config.enable_timestamped_log = false;
    log_config.logs_directory = (std::filesystem::temp_directory_path() / "relaycore_test_logs").string();
    RelayCore::log::init(log_config);

    const std::filesystem::path path =
        std::filesystem::temp_directory_path() / "relaycore_blockout_loader_invalid_entity_size.toml";

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
        out << "size = [0.3, 0.0, 0.1]\n";
        out << "target = \"door_a\"\n";
    }

    CHECK_THROWS_WITH(
        [&]() {
            (void) RelayCore::load_blockout_level(path);
        }(),
        doctest::Contains("non-positive size components")
    );

    const bool removed = std::filesystem::remove(path);
    CHECK(removed);
    RelayCore::log::shutdown();
}

TEST_CASE("blockout loader rejects duplicate pickup item ids")
{
    RelayCore::log::shutdown();
    RelayCore::log::Config log_config{};
    log_config.enable_console_sink = false;
    log_config.enable_timestamped_log = false;
    log_config.logs_directory = (std::filesystem::temp_directory_path() / "relaycore_test_logs").string();
    RelayCore::log::init(log_config);

    const std::filesystem::path path =
        std::filesystem::temp_directory_path() / "relaycore_blockout_loader_invalid_duplicate_item_id.toml";

    {
        std::ofstream out{path, std::ios::trunc};
        REQUIRE(out.is_open());
        out << "[[entity]]\n";
        out << "classname = \"gameplay_entity\"\n";
        out << "kind = \"pickup\"\n";
        out << "origin = [0.0, 1.0, 0.0]\n";
        out << "size = [0.2, 0.1, 0.02]\n";
        out << "item_id = \"lab_keycard\"\n\n";
        out << "[[entity]]\n";
        out << "classname = \"gameplay_entity\"\n";
        out << "kind = \"pickup\"\n";
        out << "origin = [1.0, 1.0, 0.0]\n";
        out << "size = [0.2, 0.1, 0.02]\n";
        out << "item_id = \"lab_keycard\"\n";
    }

    CHECK_THROWS_WITH(
        [&]() {
            (void) RelayCore::load_blockout_level(path);
        }(),
        doctest::Contains("Duplicate pickup item_id")
    );

    const bool removed = std::filesystem::remove(path);
    CHECK(removed);
    RelayCore::log::shutdown();
}

TEST_CASE("blockout loader rejects unknown required_item references")
{
    RelayCore::log::shutdown();
    RelayCore::log::Config log_config{};
    log_config.enable_console_sink = false;
    log_config.enable_timestamped_log = false;
    log_config.logs_directory = (std::filesystem::temp_directory_path() / "relaycore_test_logs").string();
    RelayCore::log::init(log_config);

    const std::filesystem::path path =
        std::filesystem::temp_directory_path() / "relaycore_blockout_loader_invalid_required_item.toml";

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
        out << "target = \"door_a\"\n";
        out << "required_item = \"missing_keycard\"\n";
    }

    CHECK_THROWS_WITH(
        [&]() {
            (void) RelayCore::load_blockout_level(path);
        }(),
        doctest::Contains("requires unknown item")
    );

    const bool removed = std::filesystem::remove(path);
    CHECK(removed);
    RelayCore::log::shutdown();
}

TEST_CASE("blockout loader rejects signal entities that reference unknown targets")
{
    RelayCore::log::shutdown();
    RelayCore::log::Config log_config{};
    log_config.enable_console_sink = false;
    log_config.enable_timestamped_log = false;
    log_config.logs_directory = (std::filesystem::temp_directory_path() / "relaycore_test_logs").string();
    RelayCore::log::init(log_config);

    const std::filesystem::path path =
        std::filesystem::temp_directory_path() / "relaycore_blockout_loader_invalid_signal_target.toml";

    {
        std::ofstream out{path, std::ios::trunc};
        REQUIRE(out.is_open());
        out << "[[entity]]\n";
        out << "classname = \"gameplay_entity\"\n";
        out << "kind = \"terminal\"\n";
        out << "origin = [0.0, 1.2, 0.0]\n";
        out << "size = [0.4, 0.3, 0.1]\n";
        out << "action = \"signal\"\n";
        out << "target = \"missing_signal_target\"\n";
    }

    CHECK_THROWS_WITH(
        [&]() {
            (void) RelayCore::load_blockout_level(path);
        }(),
        doctest::Contains("Signal entity of kind 'terminal' references unknown target")
    );

    const bool removed = std::filesystem::remove(path);
    CHECK(removed);
    RelayCore::log::shutdown();
}

TEST_CASE("blockout loader rejects signal entities that target non-receiver usables")
{
    RelayCore::log::shutdown();
    RelayCore::log::Config log_config{};
    log_config.enable_console_sink = false;
    log_config.enable_timestamped_log = false;
    log_config.logs_directory = (std::filesystem::temp_directory_path() / "relaycore_test_logs").string();
    RelayCore::log::init(log_config);

    const std::filesystem::path path =
        std::filesystem::temp_directory_path() / "relaycore_blockout_loader_invalid_signal_receiver.toml";

    {
        std::ofstream out{path, std::ios::trunc};
        REQUIRE(out.is_open());
        out << "[[entity]]\n";
        out << "classname = \"gameplay_entity\"\n";
        out << "kind = \"panel\"\n";
        out << "origin = [0.0, 1.2, 0.0]\n";
        out << "size = [0.3, 0.5, 0.1]\n";
        out << "targetname = \"plain_panel\"\n";
        out << "target = \"door_a\"\n\n";
        out << "[[entity]]\n";
        out << "classname = \"gameplay_entity\"\n";
        out << "kind = \"terminal\"\n";
        out << "origin = [1.0, 1.2, 0.0]\n";
        out << "size = [0.4, 0.3, 0.1]\n";
        out << "action = \"signal\"\n";
        out << "target = \"plain_panel\"\n\n";
        out << "[[entity]]\n";
        out << "classname = \"gameplay_entity\"\n";
        out << "kind = \"door\"\n";
        out << "origin = [0.0, 0.0, 2.0]\n";
        out << "size = [2.0, 3.0, 0.2]\n";
        out << "targetname = \"door_a\"\n";
    }

    CHECK_THROWS_WITH(
        [&]() {
            (void) RelayCore::load_blockout_level(path);
        }(),
        doctest::Contains("neither a door nor a signal receiver")
    );

    const bool removed = std::filesystem::remove(path);
    CHECK(removed);
    RelayCore::log::shutdown();
}

TEST_CASE("blockout loader accepts trigger signal actions that target signal receivers")
{
    RelayCore::log::shutdown();
    RelayCore::log::Config log_config{};
    log_config.enable_console_sink = false;
    log_config.enable_timestamped_log = false;
    log_config.logs_directory = (std::filesystem::temp_directory_path() / "relaycore_test_logs").string();
    RelayCore::log::init(log_config);

    const std::filesystem::path path =
        std::filesystem::temp_directory_path() / "relaycore_blockout_loader_valid_trigger_signal_receiver.toml";

    {
        std::ofstream out{path, std::ios::trunc};
        REQUIRE(out.is_open());
        out << "[[entity]]\n";
        out << "classname = \"gameplay_entity\"\n";
        out << "kind = \"panel\"\n";
        out << "origin = [0.0, 1.2, 0.0]\n";
        out << "size = [0.3, 0.5, 0.1]\n";
        out << "targetname = \"reader_lock\"\n";
        out << "signal_action = \"unlock\"\n";
        out << "target = \"door_a\"\n\n";
        out << "[[entity]]\n";
        out << "classname = \"gameplay_entity\"\n";
        out << "kind = \"door\"\n";
        out << "origin = [0.0, 0.0, 2.0]\n";
        out << "size = [2.0, 3.0, 0.2]\n";
        out << "targetname = \"door_a\"\n\n";
        out << "[[entity]]\n";
        out << "classname = \"gameplay_entity\"\n";
        out << "kind = \"trigger\"\n";
        out << "origin = [1.0, 0.0, 0.0]\n";
        out << "size = [1.5, 2.0, 1.5]\n";
        out << "action = \"signal\"\n";
        out << "target = \"reader_lock\"\n";
    }

    CHECK_NOTHROW((void) RelayCore::load_blockout_level(path));

    const bool removed = std::filesystem::remove(path);
    CHECK(removed);
    RelayCore::log::shutdown();
}

TEST_CASE("blockout loader accepts trigger authored activate_target ids for mission steps")
{
    RelayCore::log::shutdown();
    RelayCore::log::Config log_config{};
    log_config.enable_console_sink = false;
    log_config.enable_timestamped_log = false;
    log_config.logs_directory = (std::filesystem::temp_directory_path() / "relaycore_test_logs").string();
    RelayCore::log::init(log_config);

    const std::filesystem::path path =
        std::filesystem::temp_directory_path() / "relaycore_blockout_loader_valid_trigger_activate_target.toml";

    {
        std::ofstream out{path, std::ios::trunc};
        REQUIRE(out.is_open());
        out << "[[entity]]\n";
        out << "classname = \"gameplay_entity\"\n";
        out << "kind = \"trigger\"\n";
        out << "origin = [0.0, 0.0, 0.0]\n";
        out << "size = [1.5, 2.0, 1.5]\n";
        out << "target = \"airlock_cycle_event\"\n\n";
        out << "[[entity]]\n";
        out << "classname = \"mission_step\"\n";
        out << "kind = \"activate_target\"\n";
        out << "target = \"airlock_cycle_event\"\n";
    }

    CHECK_NOTHROW((void) RelayCore::load_blockout_level(path));

    const bool removed = std::filesystem::remove(path);
    CHECK(removed);
    RelayCore::log::shutdown();
}

TEST_CASE("blockout loader rejects mission signal steps that target non-signal activations")
{
    RelayCore::log::shutdown();
    RelayCore::log::Config log_config{};
    log_config.enable_console_sink = false;
    log_config.enable_timestamped_log = false;
    log_config.logs_directory = (std::filesystem::temp_directory_path() / "relaycore_test_logs").string();
    RelayCore::log::init(log_config);

    const std::filesystem::path path =
        std::filesystem::temp_directory_path() / "relaycore_blockout_loader_invalid_mission_signal_step.toml";

    {
        std::ofstream out{path, std::ios::trunc};
        REQUIRE(out.is_open());
        out << "[[entity]]\n";
        out << "classname = \"gameplay_entity\"\n";
        out << "kind = \"terminal\"\n";
        out << "origin = [0.0, 1.2, 0.0]\n";
        out << "size = [0.4, 0.3, 0.1]\n";
        out << "action = \"activate_target\"\n";
        out << "target = \"lab_relay_console\"\n\n";
        out << "[[entity]]\n";
        out << "classname = \"mission_step\"\n";
        out << "kind = \"signal\"\n";
        out << "target = \"lab_relay_console\"\n";
    }

    CHECK_THROWS_WITH(
        [&]() {
            (void) RelayCore::load_blockout_level(path);
        }(),
        doctest::Contains("unknown signal target")
    );

    const bool removed = std::filesystem::remove(path);
    CHECK(removed);
    RelayCore::log::shutdown();
}

TEST_CASE("blockout loader rejects unsupported usable actions")
{
    RelayCore::log::shutdown();
    RelayCore::log::Config log_config{};
    log_config.enable_console_sink = false;
    log_config.enable_timestamped_log = false;
    log_config.logs_directory = (std::filesystem::temp_directory_path() / "relaycore_test_logs").string();
    RelayCore::log::init(log_config);

    const std::filesystem::path path =
        std::filesystem::temp_directory_path() / "relaycore_blockout_loader_invalid_usable_action.toml";

    {
        std::ofstream out{path, std::ios::trunc};
        REQUIRE(out.is_open());
        out << "[[entity]]\n";
        out << "classname = \"gameplay_entity\"\n";
        out << "kind = \"panel\"\n";
        out << "origin = [0.0, 1.2, 0.0]\n";
        out << "size = [0.3, 0.5, 0.1]\n";
        out << "target = \"door_a\"\n";
        out << "action = \"pulse_beam\"\n\n";
        out << "[[entity]]\n";
        out << "classname = \"gameplay_entity\"\n";
        out << "kind = \"door\"\n";
        out << "origin = [0.0, 0.0, 2.0]\n";
        out << "size = [2.0, 3.0, 0.2]\n";
        out << "targetname = \"door_a\"\n";
    }

    CHECK_THROWS_WITH(
        [&]() {
            (void) RelayCore::load_blockout_level(path);
        }(),
        doctest::Contains("has unsupported action")
    );

    const bool removed = std::filesystem::remove(path);
    CHECK(removed);
    RelayCore::log::shutdown();
}

TEST_CASE("blockout loader rejects pickup actions on non-pickup entities")
{
    RelayCore::log::shutdown();
    RelayCore::log::Config log_config{};
    log_config.enable_console_sink = false;
    log_config.enable_timestamped_log = false;
    log_config.logs_directory = (std::filesystem::temp_directory_path() / "relaycore_test_logs").string();
    RelayCore::log::init(log_config);

    const std::filesystem::path path =
        std::filesystem::temp_directory_path() / "relaycore_blockout_loader_invalid_panel_pickup_action.toml";

    {
        std::ofstream out{path, std::ios::trunc};
        REQUIRE(out.is_open());
        out << "[[entity]]\n";
        out << "classname = \"gameplay_entity\"\n";
        out << "kind = \"panel\"\n";
        out << "origin = [0.0, 1.2, 0.0]\n";
        out << "size = [0.3, 0.5, 0.1]\n";
        out << "target = \"door_a\"\n";
        out << "action = \"pickup\"\n\n";
        out << "[[entity]]\n";
        out << "classname = \"gameplay_entity\"\n";
        out << "kind = \"door\"\n";
        out << "origin = [0.0, 0.0, 2.0]\n";
        out << "size = [2.0, 3.0, 0.2]\n";
        out << "targetname = \"door_a\"\n";
    }

    CHECK_THROWS_WITH(
        [&]() {
            (void) RelayCore::load_blockout_level(path);
        }(),
        doctest::Contains("only pickup entities may collect items")
    );

    const bool removed = std::filesystem::remove(path);
    CHECK(removed);
    RelayCore::log::shutdown();
}

TEST_CASE("blockout loader rejects unsupported signal actions")
{
    RelayCore::log::shutdown();
    RelayCore::log::Config log_config{};
    log_config.enable_console_sink = false;
    log_config.enable_timestamped_log = false;
    log_config.logs_directory = (std::filesystem::temp_directory_path() / "relaycore_test_logs").string();
    RelayCore::log::init(log_config);

    const std::filesystem::path path =
        std::filesystem::temp_directory_path() / "relaycore_blockout_loader_invalid_signal_action.toml";

    {
        std::ofstream out{path, std::ios::trunc};
        REQUIRE(out.is_open());
        out << "[[entity]]\n";
        out << "classname = \"gameplay_entity\"\n";
        out << "kind = \"panel\"\n";
        out << "origin = [0.0, 1.2, 0.0]\n";
        out << "size = [0.3, 0.5, 0.1]\n";
        out << "targetname = \"reader_lock\"\n";
        out << "signal_action = \"jammed\"\n";
    }

    CHECK_THROWS_WITH(
        [&]() {
            (void) RelayCore::load_blockout_level(path);
        }(),
        doctest::Contains("has unsupported signal_action")
    );

    const bool removed = std::filesystem::remove(path);
    CHECK(removed);
    RelayCore::log::shutdown();
}

TEST_CASE("blockout loader rejects signal actions on non-usable entities")
{
    RelayCore::log::shutdown();
    RelayCore::log::Config log_config{};
    log_config.enable_console_sink = false;
    log_config.enable_timestamped_log = false;
    log_config.logs_directory = (std::filesystem::temp_directory_path() / "relaycore_test_logs").string();
    RelayCore::log::init(log_config);

    const std::filesystem::path path =
        std::filesystem::temp_directory_path() / "relaycore_blockout_loader_invalid_door_signal_action.toml";

    {
        std::ofstream out{path, std::ios::trunc};
        REQUIRE(out.is_open());
        out << "[[entity]]\n";
        out << "classname = \"gameplay_entity\"\n";
        out << "kind = \"door\"\n";
        out << "origin = [0.0, 0.0, 0.0]\n";
        out << "size = [2.0, 3.0, 0.2]\n";
        out << "targetname = \"door_a\"\n";
        out << "signal_action = \"unlock\"\n";
    }

    CHECK_THROWS_WITH(
        [&]() {
            (void) RelayCore::load_blockout_level(path);
        }(),
        doctest::Contains("only usable entities may receive signals")
    );

    const bool removed = std::filesystem::remove(path);
    CHECK(removed);
    RelayCore::log::shutdown();
}

TEST_CASE("blockout loader rejects chain targets that point to non-receiver usables")
{
    RelayCore::log::shutdown();
    RelayCore::log::Config log_config{};
    log_config.enable_console_sink = false;
    log_config.enable_timestamped_log = false;
    log_config.logs_directory = (std::filesystem::temp_directory_path() / "relaycore_test_logs").string();
    RelayCore::log::init(log_config);

    const std::filesystem::path path =
        std::filesystem::temp_directory_path() / "relaycore_blockout_loader_invalid_chain_receiver.toml";

    {
        std::ofstream out{path, std::ios::trunc};
        REQUIRE(out.is_open());
        out << "[[entity]]\n";
        out << "classname = \"gameplay_entity\"\n";
        out << "kind = \"panel\"\n";
        out << "origin = [0.0, 1.2, 0.0]\n";
        out << "size = [0.3, 0.5, 0.1]\n";
        out << "targetname = \"plain_panel\"\n";
        out << "target = \"door_a\"\n\n";
        out << "[[entity]]\n";
        out << "classname = \"gameplay_entity\"\n";
        out << "kind = \"pickup\"\n";
        out << "origin = [1.0, 1.0, 0.0]\n";
        out << "size = [0.2, 0.1, 0.02]\n";
        out << "item_id = \"keycard_a\"\n";
        out << "chain_target = \"plain_panel\"\n\n";
        out << "[[entity]]\n";
        out << "classname = \"gameplay_entity\"\n";
        out << "kind = \"door\"\n";
        out << "origin = [0.0, 0.0, 2.0]\n";
        out << "size = [2.0, 3.0, 0.2]\n";
        out << "targetname = \"door_a\"\n";
    }

    CHECK_THROWS_WITH(
        [&]() {
            (void) RelayCore::load_blockout_level(path);
        }(),
        doctest::Contains("neither a door nor a signal receiver")
    );

    const bool removed = std::filesystem::remove(path);
    CHECK(removed);
    RelayCore::log::shutdown();
}

TEST_CASE("blockout loader rejects mission pickup steps that reference unknown items")
{
    RelayCore::log::shutdown();
    RelayCore::log::Config log_config{};
    log_config.enable_console_sink = false;
    log_config.enable_timestamped_log = false;
    log_config.logs_directory = (std::filesystem::temp_directory_path() / "relaycore_test_logs").string();
    RelayCore::log::init(log_config);

    const std::filesystem::path path =
        std::filesystem::temp_directory_path() / "relaycore_blockout_loader_invalid_mission_pickup_target.toml";

    {
        std::ofstream out{path, std::ios::trunc};
        REQUIRE(out.is_open());
        out << "[[entity]]\n";
        out << "classname = \"mission_step\"\n";
        out << "kind = \"pickup_item\"\n";
        out << "target = \"missing_keycard\"\n";
        out << "title = \"Collect key\"\n";
    }

    CHECK_THROWS_WITH(
        [&]() {
            (void) RelayCore::load_blockout_level(path);
        }(),
        doctest::Contains("mission_step pickup references unknown item")
    );

    const bool removed = std::filesystem::remove(path);
    CHECK(removed);
    RelayCore::log::shutdown();
}

TEST_CASE("blockout loader rejects mission activate_target steps that reference unknown usable targets")
{
    RelayCore::log::shutdown();
    RelayCore::log::Config log_config{};
    log_config.enable_console_sink = false;
    log_config.enable_timestamped_log = false;
    log_config.logs_directory = (std::filesystem::temp_directory_path() / "relaycore_test_logs").string();
    RelayCore::log::init(log_config);

    const std::filesystem::path path =
        std::filesystem::temp_directory_path() / "relaycore_blockout_loader_invalid_mission_activate_target.toml";

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
        out << "kind = \"terminal\"\n";
        out << "origin = [0.0, 1.2, 1.0]\n";
        out << "size = [0.4, 0.3, 0.1]\n";
        out << "action = \"activate_target\"\n";
        out << "target = \"relay_console\"\n\n";
        out << "[[entity]]\n";
        out << "classname = \"mission_step\"\n";
        out << "kind = \"activate_target\"\n";
        out << "target = \"missing_console\"\n";
        out << "title = \"Stabilize relay\"\n";
    }

    CHECK_THROWS_WITH(
        [&]() {
            (void) RelayCore::load_blockout_level(path);
        }(),
        doctest::Contains("mission_step activate_target references unknown activatable target")
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

TEST_CASE("blockout loader rejects non-pickup entities with item properties")
{
    RelayCore::log::shutdown();
    RelayCore::log::Config log_config{};
    log_config.enable_console_sink = false;
    log_config.enable_timestamped_log = false;
    log_config.logs_directory = (std::filesystem::temp_directory_path() / "relaycore_test_logs").string();
    RelayCore::log::init(log_config);

    const std::filesystem::path path =
        std::filesystem::temp_directory_path() / "relaycore_blockout_loader_invalid_panel_item_id.toml";

    {
        std::ofstream out{path, std::ios::trunc};
        REQUIRE(out.is_open());
        out << "name = \"invalid\"\n";
        out << "version = 1\n\n";
        out << "[[entity]]\n";
        out << "classname = \"gameplay_entity\"\n";
        out << "kind = \"panel\"\n";
        out << "origin = [0.0, 1.0, 0.0]\n";
        out << "size = [0.3, 0.5, 0.1]\n";
        out << "target = \"door_a\"\n";
        out << "item_id = \"not_allowed\"\n\n";
        out << "[[entity]]\n";
        out << "classname = \"gameplay_entity\"\n";
        out << "kind = \"door\"\n";
        out << "origin = [0.0, 0.0, 0.0]\n";
        out << "size = [2.0, 2.0, 0.2]\n";
        out << "targetname = \"door_a\"\n";
    }

    CHECK_THROWS_WITH(
        [&]() {
            (void) RelayCore::load_blockout_level(path);
        }(),
        doctest::Contains("only pickup entities may author inventory items")
    );

    const bool removed = std::filesystem::remove(path);
    CHECK(removed);
    RelayCore::log::shutdown();
}

TEST_CASE("blockout loader rejects pickup inventory gates")
{
    RelayCore::log::shutdown();
    RelayCore::log::Config log_config{};
    log_config.enable_console_sink = false;
    log_config.enable_timestamped_log = false;
    log_config.logs_directory = (std::filesystem::temp_directory_path() / "relaycore_test_logs").string();
    RelayCore::log::init(log_config);

    const std::filesystem::path path =
        std::filesystem::temp_directory_path() / "relaycore_blockout_loader_invalid_pickup_requirement.toml";

    {
        std::ofstream out{path, std::ios::trunc};
        REQUIRE(out.is_open());
        out << "name = \"invalid\"\n";
        out << "version = 1\n\n";
        out << "[[entity]]\n";
        out << "classname = \"gameplay_entity\"\n";
        out << "kind = \"pickup\"\n";
        out << "origin = [0.0, 1.0, 0.0]\n";
        out << "size = [0.2, 0.1, 0.02]\n";
        out << "item_id = \"lab_keycard\"\n";
        out << "required_item = \"other_key\"\n";
    }

    CHECK_THROWS_WITH(
        [&]() {
            (void) RelayCore::load_blockout_level(path);
        }(),
        doctest::Contains("pickup and may not require another inventory item")
    );

    const bool removed = std::filesystem::remove(path);
    CHECK(removed);
    RelayCore::log::shutdown();
}

TEST_CASE("blockout loader rejects locked pickups")
{
    RelayCore::log::shutdown();
    RelayCore::log::Config log_config{};
    log_config.enable_console_sink = false;
    log_config.enable_timestamped_log = false;
    log_config.logs_directory = (std::filesystem::temp_directory_path() / "relaycore_test_logs").string();
    RelayCore::log::init(log_config);

    const std::filesystem::path path =
        std::filesystem::temp_directory_path() / "relaycore_blockout_loader_invalid_locked_pickup.toml";

    {
        std::ofstream out{path, std::ios::trunc};
        REQUIRE(out.is_open());
        out << "name = \"invalid\"\n";
        out << "version = 1\n\n";
        out << "[[entity]]\n";
        out << "classname = \"gameplay_entity\"\n";
        out << "kind = \"pickup\"\n";
        out << "origin = [0.0, 1.0, 0.0]\n";
        out << "size = [0.2, 0.1, 0.02]\n";
        out << "item_id = \"lab_keycard\"\n";
        out << "locked = true\n";
    }

    CHECK_THROWS_WITH(
        [&]() {
            (void) RelayCore::load_blockout_level(path);
        }(),
        doctest::Contains("pickup and may not start locked")
    );

    const bool removed = std::filesystem::remove(path);
    CHECK(removed);
    RelayCore::log::shutdown();
}

TEST_CASE("blockout loader rejects required_item_name without required_item")
{
    RelayCore::log::shutdown();
    RelayCore::log::Config log_config{};
    log_config.enable_console_sink = false;
    log_config.enable_timestamped_log = false;
    log_config.logs_directory = (std::filesystem::temp_directory_path() / "relaycore_test_logs").string();
    RelayCore::log::init(log_config);

    const std::filesystem::path path =
        std::filesystem::temp_directory_path() / "relaycore_blockout_loader_invalid_required_item_name.toml";

    {
        std::ofstream out{path, std::ios::trunc};
        REQUIRE(out.is_open());
        out << "name = \"invalid\"\n";
        out << "version = 1\n\n";
        out << "[[entity]]\n";
        out << "classname = \"gameplay_entity\"\n";
        out << "kind = \"panel\"\n";
        out << "origin = [0.0, 1.0, 0.0]\n";
        out << "size = [0.3, 0.5, 0.1]\n";
        out << "target = \"door_a\"\n";
        out << "required_item_name = \"Lab Keycard\"\n\n";
        out << "[[entity]]\n";
        out << "classname = \"gameplay_entity\"\n";
        out << "kind = \"door\"\n";
        out << "origin = [0.0, 0.0, 0.0]\n";
        out << "size = [2.0, 2.0, 0.2]\n";
        out << "targetname = \"door_a\"\n";
    }

    CHECK_THROWS_WITH(
        [&]() {
            (void) RelayCore::load_blockout_level(path);
        }(),
        doctest::Contains("defines required_item_name without a required_item")
    );

    const bool removed = std::filesystem::remove(path);
    CHECK(removed);
    RelayCore::log::shutdown();
}

TEST_CASE("blockout loader rejects trigger chain_target authoring")
{
    RelayCore::log::shutdown();
    RelayCore::log::Config log_config{};
    log_config.enable_console_sink = false;
    log_config.enable_timestamped_log = false;
    log_config.logs_directory = (std::filesystem::temp_directory_path() / "relaycore_test_logs").string();
    RelayCore::log::init(log_config);

    const std::filesystem::path path =
        std::filesystem::temp_directory_path() / "relaycore_blockout_loader_invalid_trigger_chain.toml";

    {
        std::ofstream out{path, std::ios::trunc};
        REQUIRE(out.is_open());
        out << "name = \"invalid\"\n";
        out << "version = 1\n\n";
        out << "[[entity]]\n";
        out << "classname = \"gameplay_entity\"\n";
        out << "kind = \"door\"\n";
        out << "origin = [0.0, 0.0, 0.0]\n";
        out << "size = [2.0, 2.0, 0.2]\n";
        out << "targetname = \"door_a\"\n\n";
        out << "[[entity]]\n";
        out << "classname = \"gameplay_entity\"\n";
        out << "kind = \"trigger\"\n";
        out << "origin = [0.0, 0.0, 2.0]\n";
        out << "size = [2.0, 2.0, 2.0]\n";
        out << "target = \"door_a\"\n";
        out << "chain_target = \"door_a\"\n";
    }

    CHECK_THROWS_WITH(
        [&]() {
            (void) RelayCore::load_blockout_level(path);
        }(),
        doctest::Contains("trigger chaining is not supported")
    );

    const bool removed = std::filesystem::remove(path);
    CHECK(removed);
    RelayCore::log::shutdown();
}
