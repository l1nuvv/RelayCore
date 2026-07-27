#include "relaycore/core/log.h"
#include "relaycore/core/profiling.h"
#include "relaygame/level_session_builder.h"

#include <cmath>
#include <doctest/doctest.h>
#include <filesystem>
#include <fstream>

#include "scoped_temp_directory.h"
#include "scoped_test_logger.h"

namespace
{
    [[nodiscard]] auto spawn_angles_nearly_equal(const float lhs, const float rhs) -> bool
    {
        return std::abs(lhs - rhs) <= 0.0001F;
    }
} // namespace

TEST_CASE("level session builder applies authored spawn view angles")
{
    RelayCore::test::ScopedTempDirectory temp;
    RelayCore::test::ScopedTestLogger logger{temp.path() / "logs"};

    const std::filesystem::path path = temp.path() / "relaycore_level_session_builder_spawn_angles.toml";

    {
        std::ofstream out{path, std::ios::trunc};
        REQUIRE(out.is_open());
        out << "[[solid]]\n";
        out << "origin = [0.0, 0.0, 0.0]\n";
        out << "size = [4.0, 0.2, 4.0]\n";
        out << "material = \"Floor\"\n\n";
        out << "[[entity]]\n";
        out << "classname = \"player_spawn\"\n";
        out << "origin = [1.0, 0.25, -2.0]\n";
        out << "angles = [180.0, 15.0, 0.0]\n";
    }

    RelayCore::core::GameplayConfig config{};
    config.level_path = path.string();
    RelayGame::LevelSessionBuilder builder{config};
    const RelayGame::LevelSession session = builder.build();

    CHECK(session.level_path == path);
    CHECK(session.spawn_position.x == doctest::Approx(1.0F));
    CHECK(session.spawn_position.y == doctest::Approx(0.25F));
    CHECK(session.spawn_position.z == doctest::Approx(-2.0F));
    REQUIRE(session.spawn_view_angles.has_value());
    CHECK(spawn_angles_nearly_equal(session.spawn_view_angles->x, -1.5707963268F));
    CHECK(spawn_angles_nearly_equal(session.spawn_view_angles->y, 0.2617993951F));

    std::filesystem::remove(path);
    RelayCore::log::shutdown();
}

TEST_CASE("level session builder collects authoring overlap warnings")
{
    RelayCore::test::ScopedTempDirectory temp;
    RelayCore::test::ScopedTestLogger logger{temp.path() / "logs"};

    const std::filesystem::path path = temp.path() / "relaycore_level_session_builder_warnings.toml";

    {
        std::ofstream out{path, std::ios::trunc};
        REQUIRE(out.is_open());
        out << "[[solid]]\n";
        out << "origin = [0.0, -0.1, 0.0]\n";
        out << "size = [8.0, 0.2, 8.0]\n";
        out << "material = \"Floor\"\n\n";
        out << "[[entity]]\n";
        out << "classname = \"gameplay_entity\"\n";
        out << "kind = \"door\"\n";
        out << "origin = [0.0, 1.2, 2.0]\n";
        out << "size = [2.0, 2.4, 0.2]\n";
        out << "targetname = \"door_a\"\n\n";
        out << "[[entity]]\n";
        out << "classname = \"gameplay_entity\"\n";
        out << "kind = \"trigger\"\n";
        out << "origin = [0.0, 1.2, 2.0]\n";
        out << "size = [2.4, 2.4, 0.8]\n";
        out << "target = \"door_a\"\n";
        out << "action = \"activate_target\"\n";
    }

    RelayCore::core::GameplayConfig config{};
    config.level_path = path.string();
    RelayGame::LevelSessionBuilder builder{config};
    const RelayGame::LevelSession session = builder.build();

    REQUIRE(session.authoring_warnings.size() == 1);
    CHECK(session.authoring_warnings.front().message.find("Suspicious overlap between trigger") != std::string::npos);
    CHECK(session.authoring_warnings.front().primary_line > 0);
    CHECK(session.authoring_warnings.front().secondary_line > 0);

    std::filesystem::remove(path);
    RelayCore::log::shutdown();
}

TEST_CASE("level session builder collects unresolved authored target warnings")
{
    RelayCore::test::ScopedTempDirectory temp;
    RelayCore::test::ScopedTestLogger logger{temp.path() / "logs"};

    const std::filesystem::path path = temp.path() / "relaycore_level_session_builder_unresolved_target.toml";

    {
        std::ofstream out{path, std::ios::trunc};
        REQUIRE(out.is_open());
        out << "[[solid]]\n";
        out << "origin = [0.0, -0.1, 0.0]\n";
        out << "size = [8.0, 0.2, 8.0]\n";
        out << "material = \"Floor\"\n\n";
        out << "[[entity]]\n";
        out << "classname = \"gameplay_entity\"\n";
        out << "kind = \"usable\"\n";
        out << "origin = [0.0, 1.0, 1.0]\n";
        out << "size = [0.4, 0.6, 0.2]\n";
        out << "label = \"Broken panel\"\n";
        out << "targetname = \"broken_panel\"\n";
        out << "target = \"missing_bulkhead\"\n";
        out << "action = \"activate_target\"\n";
    }

    RelayCore::core::GameplayConfig config{};
    config.level_path = path.string();
    RelayGame::LevelSessionBuilder builder{config};
    const RelayGame::LevelSession session = builder.build();

    REQUIRE(session.authoring_warnings.size() == 1);
    CHECK(
        session.authoring_warnings.front().message.find("Unresolved authored activate_target link") != std::string::npos
    );
    CHECK(session.authoring_warnings.front().message.find("missing_bulkhead") != std::string::npos);
    CHECK(session.authoring_warnings.front().primary_line > 0);
    CHECK(session.authoring_warnings.front().primary_column > 0);

    std::filesystem::remove(path);
    RelayCore::log::shutdown();
}
