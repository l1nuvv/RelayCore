#include "relaycore/core/log.h"
#include "relaygame/level_session_builder.h"

#include <cmath>
#include <doctest/doctest.h>
#include <filesystem>
#include <fstream>

namespace
{
    [[nodiscard]] auto spawn_angles_nearly_equal(const float lhs, const float rhs) -> bool
    {
        return std::abs(lhs - rhs) <= 0.0001F;
    }
} // namespace

TEST_CASE("level session builder applies authored spawn view angles")
{
    RelayCore::log::shutdown();
    RelayCore::log::Config log_config{};
    log_config.enable_console_sink = false;
    log_config.enable_timestamped_log = false;
    log_config.logs_directory = (std::filesystem::temp_directory_path() / "relaycore_test_logs").string();
    RelayCore::log::init(log_config);

    const std::filesystem::path path =
        std::filesystem::temp_directory_path() / "relaycore_level_session_builder_spawn_angles.toml";

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

    CHECK(session.spawn_position.x == doctest::Approx(1.0F));
    CHECK(session.spawn_position.y == doctest::Approx(0.25F));
    CHECK(session.spawn_position.z == doctest::Approx(-2.0F));
    REQUIRE(session.spawn_view_angles.has_value());
    CHECK(spawn_angles_nearly_equal(session.spawn_view_angles->x, -1.5707963268F));
    CHECK(spawn_angles_nearly_equal(session.spawn_view_angles->y, 0.2617993951F));

    std::filesystem::remove(path);
    RelayCore::log::shutdown();
}
