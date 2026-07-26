#include "relaycore/core/engine_config.h"
#include "relaycore/core/log.h"

#include <doctest/doctest.h>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string_view>

namespace
{
    void init_test_logger()
    {
        RelayCore::log::shutdown();
        RelayCore::log::Config config{};
        config.enable_console_sink = false;
        config.enable_timestamped_log = false;
        config.logs_directory = (std::filesystem::temp_directory_path() / "relaycore_test_logs").string();
        RelayCore::log::init(config);
    }

    auto write_engine_config(const std::string_view contents) -> std::filesystem::path
    {
        const std::filesystem::path path =
            std::filesystem::temp_directory_path() / "relaycore_engine_config_tests.toml";
        std::ofstream out{path, std::ios::trunc};
        REQUIRE(out.is_open());
        out << contents;
        return path;
    }

    void check_invalid_config(const std::string_view contents, const std::string_view key)
    {
        const std::filesystem::path path = write_engine_config(contents);
        try
        {
            static_cast<void>(RelayCore::core::load_engine_config(path));
            FAIL("Expected engine config load to fail");
        } catch (const std::runtime_error& error)
        {
            CHECK(std::string_view{error.what()}.find(path.string()) != std::string_view::npos);
            CHECK(std::string_view{error.what()}.find(key) != std::string_view::npos);
        }
        static_cast<void>(std::filesystem::remove(path));
    }
} // namespace

TEST_CASE("engine config loads the shipped config")
{
    init_test_logger();
    const RelayCore::core::EngineConfig config = RelayCore::core::load_default_engine_config();
    CHECK(config.window.width == 2560);
    CHECK(config.renderer.camera.far_plane == doctest::Approx(100.0F));
    RelayCore::log::shutdown();
}

TEST_CASE("engine config keeps defaults for absent keys")
{
    init_test_logger();
    const std::filesystem::path path = write_engine_config("[window]\ntitle = \"Configured\"\n");
    const RelayCore::core::EngineConfig config = RelayCore::core::load_engine_config(path);
    CHECK(config.window.title == "Configured");
    CHECK(config.window.width == 1280);
    static_cast<void>(std::filesystem::remove(path));
    RelayCore::log::shutdown();
}

TEST_CASE("engine config rejects invalid TOML types")
{
    SUBCASE("scalar")
    {
        check_invalid_config("[window]\nwidth = \"wide\"\n", "window.width");
    }
    SUBCASE("integer outside int range")
    {
        check_invalid_config("[window]\nwidth = 2147483648\n", "window.width");
    }
    SUBCASE("string")
    {
        check_invalid_config("[window]\ntitle = true\n", "window.title");
    }
    SUBCASE("boolean")
    {
        check_invalid_config("[window]\nvsync = 1\n", "window.vsync");
    }
    SUBCASE("number")
    {
        check_invalid_config("[render]\nfog_start = \"near\"\n", "render.fog_start");
    }
    SUBCASE("table")
    {
        check_invalid_config("window = \"not a table\"\n", "window");
    }
}

TEST_CASE("engine config validates vec3 values")
{
    SUBCASE("two elements")
    {
        check_invalid_config("[render]\nfog_color = [0.0, 0.0]\n", "render.fog_color");
    }
    SUBCASE("four elements")
    {
        check_invalid_config("[render]\nfog_color = [0.0, 0.0, 0.0, 0.0]\n", "render.fog_color");
    }
    SUBCASE("non-numeric element")
    {
        check_invalid_config("[render]\nfog_color = [0.0, \"blue\", 0.0]\n", "render.fog_color");
    }
}

TEST_CASE("engine config rejects non-finite numbers")
{
    SUBCASE("nan")
    {
        check_invalid_config("[render]\nfog_start = nan\n", "render.fog_start");
    }
    SUBCASE("infinity")
    {
        check_invalid_config("[render]\nfog_start = inf\n", "render.fog_start");
    }
    SUBCASE("outside float range")
    {
        check_invalid_config("[render]\nfog_start = 3.5e38\n", "render.fog_start");
    }
}

TEST_CASE("engine config validates semantic constraints")
{
    SUBCASE("zero width")
    {
        check_invalid_config("[window]\nwidth = 0\n", "window.width");
    }
    SUBCASE("zero height")
    {
        check_invalid_config("[window]\nheight = 0\n", "window.height");
    }
    SUBCASE("fov")
    {
        check_invalid_config("[render]\nfov_degrees = 180\n", "render.fov_degrees");
    }
    SUBCASE("near")
    {
        check_invalid_config("[render]\nnear_plane = 0.0\n", "render.near_plane");
    }
    SUBCASE("far")
    {
        check_invalid_config("[render]\nnear_plane = 2.0\nfar_plane = 2.0\n", "render.far_plane");
    }
    SUBCASE("camera")
    {
        check_invalid_config(
            "[render]\ncamera_position = [1.0, 2.0, 3.0]\ncamera_target = [1.0, 2.0, 3.0]\n",
            "render.camera_position"
        );
    }
    SUBCASE("shadow direction")
    {
        check_invalid_config("[render]\nshadow_light_direction = [0.0, 0.0, 0.0]\n", "render.shadow_light_direction");
    }
    SUBCASE("degenerate shadow direction")
    {
        check_invalid_config(
            "[render]\nshadow_light_direction = [0.00001, 0.0, 0.0]\n",
            "render.shadow_light_direction"
        );
    }
    SUBCASE("shadow map resolution")
    {
        check_invalid_config("[render]\nshadow_map_resolution = 255\n", "render.shadow_map_resolution");
    }
    SUBCASE("shadow radius")
    {
        check_invalid_config("[render]\nshadow_projection_radius = 0.5\n", "render.shadow_projection_radius");
    }
    SUBCASE("shadow depth")
    {
        check_invalid_config(
            "[render]\nshadow_projection_radius = 2.0\nshadow_projection_depth = 2.5\n",
            "render.shadow_projection_depth"
        );
    }
    SUBCASE("far shadow radius")
    {
        check_invalid_config("[render]\nshadow_far_projection_radius = 0.5\n", "render.shadow_far_projection_radius");
    }
    SUBCASE("far shadow depth")
    {
        check_invalid_config(
            "[render]\nshadow_far_projection_radius = 2.0\nshadow_far_projection_depth = 2.5\n",
            "render.shadow_far_projection_depth"
        );
    }
}
