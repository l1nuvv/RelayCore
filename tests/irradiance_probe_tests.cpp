#include "relaycore/core/paths.h"
#include "relaycore/render/irradiance_probes.h"
#include "relaycore/world/blockout_loader.h"
#include "relaygame/world_irradiance.h"
#include "relaygame/world_scene_renderer.h"

#include <doctest/doctest.h>
#include <fstream>
#include <limits>

#include "scoped_temp_directory.h"
#include "scoped_test_logger.h"

using namespace RelayCore::render;

TEST_CASE("irradiance probes preserve constant radiance and interpolate the grid")
{
    IrradianceProbeVolume volume{{{-1, -1, -1}, {1, 1, 1}, {2, 2, 2}, 64}, 0, {}};
    volume.probes.resize(8);
    for (auto& probe : volume.probes)
        probe[0] = glm::vec3{2, .4F, .1F} / .282094792F;
    for (const auto normal : {glm::vec3{1, 0, 0}, glm::vec3{0, 1, 0}, glm::vec3{0, 0, -1}})
    {
        const auto value = sample_irradiance_probes(volume, {}, normal);
        CHECK(value.x == doctest::Approx(2));
        CHECK(value.y == doctest::Approx(.4F));
        CHECK(value.z == doctest::Approx(.1F));
    }
    CHECK(sample_irradiance_probes(volume, {1.001F, 0, 0}, {0, 1, 0}) == glm::vec3{0});
    for (std::size_t i = 0; i < 8; ++i)
        volume.probes[i][0] = glm::vec3{static_cast<float>(i)} / .282094792F;
    CHECK(sample_irradiance_probes(volume, {}, {0, 1, 0}).x == doctest::Approx(3.5F));
    CHECK(sample_irradiance_probes(volume, {1, 1, 1}, {0, 1, 0}).x == doctest::Approx(7));
    CHECK(sample_irradiance_probes(volume, {-1, -1, -1}, {0, 1, 0}).x == doctest::Approx(0));
    volume.probes[0][3].x = 4;
    CHECK(sample_irradiance_probes(volume, {-1, -1, -1}, {1, 0, 0}).x > 1.9F);
    CHECK(sample_irradiance_probes(volume, {-1, -1, -1}, {-1, 0, 0}).x == 0);
}

TEST_CASE("irradiance bake transports colored diffuse light and respects opaque blockers")
{
    IrradianceBakeScene scene;
    scene.triangles = {
        {{-20, -1, -20}, {-20, -1, 20}, {20, -1, 20}, {.8F, 0, 0}},
        {{-20, -1, -20}, {20, -1, 20}, {20, -1, -20}, {.8F, 0, 0}}
    };
    scene.lights.push_back({{0, 1, 0}, {8, 8, 8}, 12});
    IrradianceProbeSettings settings{{-1, 0, -1}, {1, 2, 1}, {2, 2, 2}, 512};
    const auto open = bake_irradiance_probes(scene, settings);
    CHECK(open.probes == bake_irradiance_probes(scene, settings).probes);
    const auto bounce = sample_irradiance_probes(open, {0, .2F, 0}, {0, -1, 0});
    CHECK(bounce.x > .3F);
    CHECK(bounce.y == 0);
    CHECK(bounce.z == 0);
    CHECK(bounce.x > sample_irradiance_probes(open, {0, .2F, 0}, {0, 1, 0}).x);
    scene.triangles.push_back({{-20, 0, -20}, {-20, 0, 20}, {20, 0, 20}, {0, 0, 0}});
    scene.triangles.push_back({{-20, 0, -20}, {20, 0, 20}, {20, 0, -20}, {0, 0, 0}});
    CHECK(sample_irradiance_probes(bake_irradiance_probes(scene, settings), {0, .2F, 0}, {0, -1, 0}).x == 0);
    CHECK(open.source_signature != irradiance_source_signature(scene, settings));
    scene.lights[0].radius = 0;
    CHECK_THROWS_WITH_AS(
        static_cast<void>(bake_irradiance_probes(scene, settings)),
        "irradiance light requires positive finite radius and nonnegative radiance",
        std::runtime_error
    );
}

TEST_CASE("irradiance products are deterministic versioned and reject malformed data")
{
    RelayCore::test::ScopedTempDirectory temp;
    IrradianceProbeVolume volume{{{-1, -1, -1}, {1, 1, 1}, {2, 2, 2}, 64}, 0x123456789abcdef0ULL, {}};
    volume.probes.resize(8);
    volume.probes[7][8] = {1, 2, 3};
    const auto path = temp.path() / "test.rcprobe";
    write_irradiance_probes(path, volume);
    const auto loaded = read_irradiance_probes(path);
    CHECK(loaded.source_signature == volume.source_signature);
    CHECK(loaded.probes == volume.probes);
    const auto bytes = [](const auto& file) {
        std::ifstream input{file, std::ios::binary};
        return std::string{std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{}};
    };
    const auto first = bytes(path);
    write_irradiance_probes(path, loaded);
    CHECK(first == bytes(path));
    {
        std::fstream output{path, std::ios::binary | std::ios::in | std::ios::out};
        output.seekp(4);
        output.put(2);
    }
    CHECK_THROWS_WITH(static_cast<void>(read_irradiance_probes(path)), "unsupported irradiance product version");
    write_irradiance_probes(path, volume);
    std::filesystem::resize_file(path, std::filesystem::file_size(path) - 1);
    CHECK_THROWS_WITH(static_cast<void>(read_irradiance_probes(path)), "truncated irradiance product");
    volume.probes[0][0].x = std::numeric_limits<float>::quiet_NaN();
    CHECK_THROWS_WITH(write_irradiance_probes(path, volume), "irradiance SH coefficients must be finite");
    volume.settings.dimensions = {4, 4, 5};
    CHECK_THROWS_WITH(validate_irradiance_settings(volume.settings), "irradiance grid exceeds 64 probes");
}

TEST_CASE("native irradiance products reject stale geometry lights and settings in either display mode")
{
    RelayCore::test::ScopedTempDirectory temp;
    RelayCore::test::ScopedTestLogger logger{temp.path() / "logs"};
    const auto original = RelayCore::load_blockout_level(RelayCore::core::asset_path("levels/native_lab.toml"));
    const auto authoring = load_default_render_authoring_config();
    for (const auto mode : {ColorPipelineMode::legacy_gamma, ColorPipelineMode::hardware_srgb})
    {
        const auto prepared = RelayGame::build_prepared_world_scene_data(original, authoring, mode);
        REQUIRE(prepared.irradiance);
        CHECK(prepared.irradiance->probes.size() == 48);
    }
    SUBCASE("geometry")
    {
        auto level = original;
        level.solids[0].maxs.y += .02F;
        CHECK_THROWS_WITH(
            static_cast<void>(
                RelayGame::build_prepared_world_scene_data(level, authoring, ColorPipelineMode::hardware_srgb)
            ),
            doctest::Contains("stale irradiance product")
        );
    }
    SUBCASE("static light")
    {
        auto level = original;
        for (auto& entity : level.entities)
            if (entity.classname == "render_light" && !entity.properties.contains("required_target"))
                entity.origin.x += .1F;
        CHECK_THROWS_WITH(
            static_cast<void>(
                RelayGame::build_prepared_world_scene_data(level, authoring, ColorPipelineMode::hardware_srgb)
            ),
            doctest::Contains("stale irradiance product")
        );
    }
    SUBCASE("prop")
    {
        auto level = original;
        for (auto& entity : level.entities)
            if (entity.classname == "prop_static")
                entity.origin.x += .01F;
        CHECK_THROWS_WITH(
            static_cast<void>(
                RelayGame::build_prepared_world_scene_data(level, authoring, ColorPipelineMode::hardware_srgb)
            ),
            doctest::Contains("stale irradiance product")
        );
    }
    SUBCASE("grid settings")
    {
        auto level = original;
        level.entities[0].properties["rays"] = "512";
        CHECK_THROWS_WITH(
            static_cast<void>(
                RelayGame::build_prepared_world_scene_data(level, authoring, ColorPipelineMode::hardware_srgb)
            ),
            doctest::Contains("stale irradiance product")
        );
    }
    SUBCASE("dynamic light remains excluded")
    {
        auto level = original;
        for (auto& entity : level.entities)
            if (entity.classname == "render_light" && entity.properties.contains("required_target"))
                entity.origin.x += 1;
        CHECK_NOTHROW(static_cast<void>(
            RelayGame::build_prepared_world_scene_data(level, authoring, ColorPipelineMode::hardware_srgb)
        ));
    }
}
