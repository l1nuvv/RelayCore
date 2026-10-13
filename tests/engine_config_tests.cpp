#include "relaycore/core/engine_config.h"
#include "relaycore/core/log.h"
#include "relaycore/render/render_resolution.h"

#include <doctest/doctest.h>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string_view>

#include "scoped_temp_directory.h"
#include "scoped_test_logger.h"

namespace
{
    auto write_engine_config(const std::filesystem::path& root, const std::string_view contents)
        -> std::filesystem::path
    {
        const std::filesystem::path path = root / "relaycore_engine_config_tests.toml";
        std::ofstream out{path, std::ios::trunc};
        REQUIRE(out.is_open());
        out << contents;
        return path;
    }

    void
    check_invalid_config(const std::filesystem::path& root, const std::string_view contents, const std::string_view key)
    {
        const std::filesystem::path path = write_engine_config(root, contents);
        try
        {
            static_cast<void>(RelayCore::core::load_engine_config(path));
            FAIL("Expected engine config load to fail");
        } catch (const std::runtime_error& error)
        {
            CHECK(std::string_view{error.what()}.find(path.string()) != std::string_view::npos);
            CHECK(std::string_view{error.what()}.find(key) != std::string_view::npos);
        }
    }

} // namespace

TEST_CASE("engine config loads the shipped config")
{
    RelayCore::test::ScopedTempDirectory temp;
    RelayCore::test::ScopedTestLogger logger{temp.path() / "logs"};
    const RelayCore::core::EngineConfig config = RelayCore::core::load_default_engine_config();
    CHECK(config.window.width == 2560);
    CHECK(config.renderer.camera.far_plane == doctest::Approx(100.0F));
}

TEST_CASE("engine config reads optional combat cues and rejects invalid cue types")
{
    RelayCore::test::ScopedTempDirectory temp;
    RelayCore::test::ScopedTestLogger logger{temp.path() / "logs"};
    const auto path = write_engine_config(
        temp.path(),
        "[game.audio]\nweapon_hit = 'hit.wav'\nthreat_windup = 'windup.wav'\nrover_attack = 'strike.wav'\n"
    );
    const auto config = RelayCore::core::load_engine_config(path);
    CHECK(config.gameplay.audio.weapon_hit == "hit.wav");
    CHECK(config.gameplay.audio.threat_windup == "windup.wav");
    CHECK(config.gameplay.audio.rover_attack == "strike.wav");
    check_invalid_config(temp.path(), "[game.audio]\nweapon_hit = 12\n", "game.audio.weapon_hit");
    check_invalid_config(temp.path(), "[game.audio]\nthreat_windup = false\n", "game.audio.threat_windup");
    check_invalid_config(temp.path(), "[game.audio]\nrover_attack = []\n", "game.audio.rover_attack");
}

TEST_CASE("engine config keeps defaults for absent keys")
{
    RelayCore::test::ScopedTempDirectory temp;
    RelayCore::test::ScopedTestLogger logger{temp.path() / "logs"};
    const std::filesystem::path path = write_engine_config(temp.path(), "[window]\ntitle = \"Configured\"\n");
    const RelayCore::core::EngineConfig config = RelayCore::core::load_engine_config(path);
    CHECK(config.window.title == "Configured");
    CHECK(config.window.width == 1280);
    CHECK(config.source_path == path);
    CHECK_FALSE(config.renderer.hdr_enabled);
    CHECK(config.renderer.exposure_ev == 0);
}

TEST_CASE("engine config validates explicit HDR exposure")
{
    RelayCore::test::ScopedTempDirectory temp;
    RelayCore::test::ScopedTestLogger logger{temp.path() / "logs"};
    const auto path = write_engine_config(temp.path(), "[render]\nhdr_enabled = true\nexposure_ev = -1.5\n");
    const auto config = RelayCore::core::load_engine_config(path);
    CHECK(config.renderer.hdr_enabled);
    CHECK(config.renderer.exposure_ev == -1.5F);
    for (const auto value : {"nan", "inf", "10.1", "-10.1", "'bright'"})
        check_invalid_config(temp.path(), std::string{"[render]\nexposure_ev = "} + value + "\n", "render.exposure_ev");
    check_invalid_config(temp.path(), "[render]\nhdr_enabled = 1\n", "render.hdr_enabled");
}

TEST_CASE("engine config validates spatial effect controls and HDR dependencies")
{
    RelayCore::test::ScopedTempDirectory temp;
    RelayCore::test::ScopedTestLogger logger{temp.path() / "logs"};
    const auto path = write_engine_config(
        temp.path(),
        "[render]\nhdr_enabled = true\nbloom_enabled = true\nbloom_strength = 0.2\n"
        "bloom_threshold = 1.5\nssao_enabled = true\nssr_enabled = true\n"
        "volumetrics_enabled = true\nvolumetric_density = 0.04\n"
    );
    const auto config = RelayCore::core::load_engine_config(path);
    CHECK(config.renderer.bloom_enabled);
    CHECK(config.renderer.ssao_enabled);
    CHECK(config.renderer.ssr_enabled);
    CHECK(config.renderer.volumetrics_enabled);
    CHECK(config.renderer.bloom_strength == doctest::Approx(.2F));
    CHECK(config.renderer.bloom_threshold == doctest::Approx(1.5F));
    CHECK(config.renderer.volumetric_density == doctest::Approx(.04F));
    for (const auto effect : {"bloom_enabled", "ssr_enabled"})
        check_invalid_config(temp.path(), std::string{"[render]\n"} + effect + " = true\n", "render.hdr_enabled");
    for (const auto key : {"bloom_strength", "bloom_threshold", "volumetric_density"})
        for (const auto value : {"nan", "inf", "-0.1", "11", "'bright'"})
            check_invalid_config(
                temp.path(),
                std::string{"[render]\n"} + key + " = " + value + "\n",
                std::string{"render."} + key
            );
}

TEST_CASE("engine config validates temporal and resolution controls")
{
    RelayCore::test::ScopedTempDirectory temp;
    RelayCore::test::ScopedTestLogger logger{temp.path() / "logs"};
    const auto path =
        write_engine_config(temp.path(), "[render]\nhdr_enabled = true\ntaa_enabled = true\nrender_scale = 0.67\n");
    const auto config = RelayCore::core::load_engine_config(path);
    CHECK(config.renderer.taa_enabled);
    CHECK(config.renderer.render_scale == doctest::Approx(.67F));
    check_invalid_config(temp.path(), "[render]\ndynamic_resolution_enabled = true\n", "render.hdr_enabled");
    for (const auto value : {"nan", "inf", "3.9", "50.1", "'fast'"})
        check_invalid_config(
            temp.path(),
            std::string{"[render]\ndynamic_resolution_target_ms = "} + value,
            "render.dynamic_resolution_target_ms"
        );
    check_invalid_config(temp.path(), "[render]\ntaa_enabled = true\n", "render.hdr_enabled");
    check_invalid_config(temp.path(), "[render]\nrender_scale = 0.75\n", "render.hdr_enabled");
    check_invalid_config(temp.path(), "[render]\ntaa_enabled = 1\n", "render.taa_enabled");
    for (const auto value : {"nan", "inf", "0.49", "1.01", "'low'"})
        check_invalid_config(temp.path(), std::string{"[render]\nrender_scale = "} + value, "render.render_scale");
}

TEST_CASE("engine config validates advanced lighting and lens quality")
{
    RelayCore::test::ScopedTempDirectory temp;
    RelayCore::test::ScopedTestLogger logger{temp.path() / "logs"};
    const auto config = RelayCore::core::load_engine_config(write_engine_config(
        temp.path(),
        "[render]\nhdr_enabled = true\nssr_enabled = true\nssgi_quality = 2\n"
        "reflection_quality = 1\ndof_quality = 2\nmotion_blur_quality = 1\ndof_focus_distance = 3.5\n"
    ));
    CHECK(config.renderer.ssgi_quality == 2);
    CHECK(config.renderer.reflection_quality == 1);
    CHECK(config.renderer.dof_quality == 2);
    CHECK(config.renderer.motion_blur_quality == 1);
    CHECK(config.renderer.dof_focus_distance == doctest::Approx(3.5F));
    for (const auto key : {"ssgi_quality", "reflection_quality", "dof_quality", "motion_blur_quality"})
        for (const auto value : {"-1", "3", "1.5", "true", "'high'"})
            check_invalid_config(
                temp.path(),
                std::string{"[render]\n"} + key + " = " + value,
                std::string{"render."} + key
            );
    for (const auto key : {"ssgi_quality", "dof_quality", "motion_blur_quality"})
        check_invalid_config(temp.path(), std::string{"[render]\n"} + key + " = 1", "render.hdr_enabled");
    check_invalid_config(temp.path(), "[render]\nreflection_quality = 1", "render.reflection_quality");
    for (const auto value : {"nan", "inf", "0.24", "100.1"})
        check_invalid_config(
            temp.path(),
            std::string{"[render]\ndof_focus_distance = "} + value,
            "render.dof_focus_distance"
        );
}

TEST_CASE("render resolution filters GPU pressure and rejects stale-scale feedback")
{
    RelayCore::render::RenderResolution controller;
    for (int i = 0; i < 96; ++i)
        controller.update(i % 2 ? 17.0F : 16.0F, controller.scale, 1, 16.67F);
    CHECK(controller.scale == 1);
    for (int i = 0; i < 32; ++i)
        controller.update(30, controller.scale, 1, 16.67F);
    CHECK(controller.scale < 1);
    const float reduced = controller.scale;
    for (int i = 0; i < 100; ++i)
        controller.update(1, 1, 1, 16.67F);
    CHECK(controller.scale == reduced);
    for (int i = 0; i < 400; ++i)
        controller.update(30, controller.scale, 1, 16.67F);
    CHECK(controller.scale == .5F);
    for (int i = 0; i < 400; ++i)
        controller.update(1, controller.scale, 1, 16.67F);
    CHECK(controller.scale == 1);
    controller.reset(.67F);
    for (int i = 0; i < 96; ++i)
        controller.update(1, controller.scale, .67F, 16.67F);
    CHECK(controller.scale == .67F);
}

TEST_CASE("engine config validates MSAA sample counts")
{
    RelayCore::test::ScopedTempDirectory temp;
    RelayCore::test::ScopedTestLogger logger{temp.path() / "logs"};
    for (const auto samples : {1, 2, 4})
    {
        const auto path = write_engine_config(temp.path(), "[render]\nmsaa_samples = " + std::to_string(samples));
        CHECK(RelayCore::core::load_engine_config(path).renderer.msaa_samples == samples);
    }
    for (const auto value : {"0", "3", "8", "-1", "true", "1.5"})
        check_invalid_config(temp.path(), std::string{"[render]\nmsaa_samples = "} + value, "render.msaa_samples");
}

TEST_CASE("engine config ignores unsupported copy inspector source binding")
{
    RelayCore::test::ScopedTempDirectory temp;
    RelayCore::test::ScopedTestLogger logger{temp.path() / "logs"};
    const std::filesystem::path path = write_engine_config(
        temp.path(),
        "[game.controls]\n"
        "open_inspector_source = \"F8\"\n"
        "copy_inspector_source = \"F9\"\n"
    );
    const RelayCore::core::EngineConfig config = RelayCore::core::load_engine_config(path);
    CHECK(config.gameplay.controls.open_inspector_source == "F8");
}

TEST_CASE("engine config rejects invalid TOML types")
{
    RelayCore::test::ScopedTempDirectory temp;
    RelayCore::test::ScopedTestLogger logger{temp.path() / "logs"};
    SUBCASE("scalar")
    {
        check_invalid_config(temp.path(), "[window]\nwidth = \"wide\"\n", "window.width");
    }
    SUBCASE("integer outside int range")
    {
        check_invalid_config(temp.path(), "[window]\nwidth = 2147483648\n", "window.width");
    }
    SUBCASE("string")
    {
        check_invalid_config(temp.path(), "[window]\ntitle = true\n", "window.title");
    }
    SUBCASE("boolean")
    {
        check_invalid_config(temp.path(), "[window]\nvsync = 1\n", "window.vsync");
    }
    SUBCASE("number")
    {
        check_invalid_config(temp.path(), "[render]\nfog_start = \"near\"\n", "render.fog_start");
    }
    SUBCASE("table")
    {
        check_invalid_config(temp.path(), "window = \"not a table\"\n", "window");
    }
}

TEST_CASE("engine config validates vec3 values")
{
    RelayCore::test::ScopedTempDirectory temp;
    RelayCore::test::ScopedTestLogger logger{temp.path() / "logs"};
    SUBCASE("two elements")
    {
        check_invalid_config(temp.path(), "[render]\nfog_color = [0.0, 0.0]\n", "render.fog_color");
    }
    SUBCASE("four elements")
    {
        check_invalid_config(temp.path(), "[render]\nfog_color = [0.0, 0.0, 0.0, 0.0]\n", "render.fog_color");
    }
    SUBCASE("non-numeric element")
    {
        check_invalid_config(temp.path(), "[render]\nfog_color = [0.0, \"blue\", 0.0]\n", "render.fog_color");
    }
}

TEST_CASE("engine config rejects non-finite numbers")
{
    RelayCore::test::ScopedTempDirectory temp;
    RelayCore::test::ScopedTestLogger logger{temp.path() / "logs"};
    SUBCASE("nan")
    {
        check_invalid_config(temp.path(), "[render]\nfog_start = nan\n", "render.fog_start");
    }
    SUBCASE("infinity")
    {
        check_invalid_config(temp.path(), "[render]\nfog_start = inf\n", "render.fog_start");
    }
    SUBCASE("outside float range")
    {
        check_invalid_config(temp.path(), "[render]\nfog_start = 3.5e38\n", "render.fog_start");
    }
}

TEST_CASE("engine config validates semantic constraints")
{
    RelayCore::test::ScopedTempDirectory temp;
    RelayCore::test::ScopedTestLogger logger{temp.path() / "logs"};
    SUBCASE("zero width")
    {
        check_invalid_config(temp.path(), "[window]\nwidth = 0\n", "window.width");
    }
    SUBCASE("zero height")
    {
        check_invalid_config(temp.path(), "[window]\nheight = 0\n", "window.height");
    }
    SUBCASE("fov")
    {
        check_invalid_config(temp.path(), "[render]\nfov_degrees = 180\n", "render.fov_degrees");
    }
    SUBCASE("near")
    {
        check_invalid_config(temp.path(), "[render]\nnear_plane = 0.0\n", "render.near_plane");
    }
    SUBCASE("far")
    {
        check_invalid_config(temp.path(), "[render]\nnear_plane = 2.0\nfar_plane = 2.0\n", "render.far_plane");
    }
    SUBCASE("camera")
    {
        check_invalid_config(
            temp.path(),
            "[render]\ncamera_position = [1.0, 2.0, 3.0]\ncamera_target = [1.0, 2.0, 3.0]\n",
            "render.camera_position"
        );
    }
    SUBCASE("shadow direction")
    {
        check_invalid_config(
            temp.path(),
            "[render]\nshadow_light_direction = [0.0, 0.0, 0.0]\n",
            "render.shadow_light_direction"
        );
    }
    SUBCASE("degenerate shadow direction")
    {
        check_invalid_config(
            temp.path(),
            "[render]\nshadow_light_direction = [0.00001, 0.0, 0.0]\n",
            "render.shadow_light_direction"
        );
    }
    SUBCASE("shadow map resolution")
    {
        check_invalid_config(temp.path(), "[render]\nshadow_map_resolution = 255\n", "render.shadow_map_resolution");
    }
    SUBCASE("shadow radius")
    {
        check_invalid_config(
            temp.path(),
            "[render]\nshadow_projection_radius = 0.5\n",
            "render.shadow_projection_radius"
        );
    }
    SUBCASE("shadow depth")
    {
        check_invalid_config(
            temp.path(),
            "[render]\nshadow_projection_radius = 2.0\nshadow_projection_depth = 2.5\n",
            "render.shadow_projection_depth"
        );
    }
    SUBCASE("far shadow radius")
    {
        check_invalid_config(
            temp.path(),
            "[render]\nshadow_far_projection_radius = 0.5\n",
            "render.shadow_far_projection_radius"
        );
    }
    SUBCASE("far shadow depth")
    {
        check_invalid_config(
            temp.path(),
            "[render]\nshadow_far_projection_radius = 2.0\nshadow_far_projection_depth = 2.5\n",
            "render.shadow_far_projection_depth"
        );
    }
}

TEST_CASE("engine config validates optional GPU opt-ins")
{
    RelayCore::test::ScopedTempDirectory temp;
    RelayCore::test::ScopedTestLogger logger{temp.path() / "logs"};
    const auto path = write_engine_config(temp.path(), "[render]\ndescriptor_indexing = true\nmesh_shaders = true\n");
    CHECK(RelayCore::core::load_engine_config(path).renderer.descriptor_indexing);
    CHECK(RelayCore::core::load_engine_config(path).renderer.mesh_shaders);
    check_invalid_config(temp.path(), "[render]\ndescriptor_indexing = 1\n", "render.descriptor_indexing");
    check_invalid_config(temp.path(), "[render]\nmesh_shaders = 1\n", "render.mesh_shaders");
}
