#include "relaycore/core/log.h"
#include "relaycore/render/render_authoring_config.h"
#include "relaycore/world/blockout_level.h"
#include "relaygame/world_scene_renderer.h"

#include <doctest/doctest.h>
#include <filesystem>
#include <fstream>
#include <stdexcept>

#include "scoped_temp_directory.h"
#include "scoped_test_logger.h"

TEST_CASE("pickup support height rejects malformed authored data")
{
    RelayCore::test::ScopedTempDirectory temp;
    RelayCore::test::ScopedTestLogger logger{temp.path() / "logs"};
    RelayCore::BlockoutLevel level;
    level.entities.push_back({.classname = "gameplay_entity", .properties = {{"kind", "pickup"}}});
    const auto authoring = RelayCore::render::load_default_render_authoring_config();
    const auto prepare = [&] {
        return RelayGame::build_prepared_world_scene_data(
            level,
            authoring,
            RelayCore::render::ColorPipelineMode::hardware_srgb
        );
    };
    CHECK_FALSE(prepare().pickup_surfaces[0].has_value());
    level.entities[0].properties["pickup_surface_y"] = "0.92";
    CHECK(*prepare().pickup_surfaces[0] == doctest::Approx(.92F));
    for (const auto* value : {"bench", "1.0junk", "nan", "inf"})
    {
        level.entities[0].properties["pickup_surface_y"] = value;
        CHECK_THROWS_WITH(prepare(), doctest::Contains("pickup_surface_y must be a finite height"));
    }
}

TEST_CASE("interactive panel presentation preserves authored roles and rejects unknown styles")
{
    RelayCore::test::ScopedTempDirectory temp;
    RelayCore::test::ScopedTestLogger logger{temp.path() / "logs"};
    RelayCore::BlockoutLevel level;
    for (const auto* kind : {"panel", "button", "terminal", "console"})
        level.entities.push_back({.classname = "gameplay_entity", .properties = {{"kind", kind}}});
    const auto authoring = RelayCore::render::load_default_render_authoring_config();
    const auto prepare = [&] {
        return RelayGame::build_prepared_world_scene_data(
            level,
            authoring,
            RelayCore::render::ColorPipelineMode::hardware_srgb
        );
    };
    using RelayGame::PanelPresentation;
    CHECK(
        prepare().panel_presentations
        == std::
            vector{PanelPresentation::Door, PanelPresentation::Door, PanelPresentation::Instrument, PanelPresentation::Instrument}
    );
    level.entities[0].properties["panel_style"] = "start";
    level.entities[2].properties["panel_style"] = "isolator";
    CHECK(
        prepare().panel_presentations
        == std::
            vector{PanelPresentation::Start, PanelPresentation::Door, PanelPresentation::Isolator, PanelPresentation::Instrument}
    );
    level.entities[0].properties["panel_style"] = "strat";
    CHECK_THROWS_WITH_AS(
        prepare(),
        doctest::Contains("gameplay_entity #0: panel_style must be door, isolator, start or instrument; got 'strat'"),
        std::runtime_error
    );
}

TEST_CASE("render authoring config loads shipped material and light contract")
{
    RelayCore::test::ScopedTempDirectory temp;
    RelayCore::test::ScopedTestLogger logger{temp.path() / "logs"};
    const RelayCore::render::RenderAuthoringConfig config = RelayCore::render::load_default_render_authoring_config();

    CHECK(config.source_path.filename() == "render_authoring.toml");
    CHECK(RelayCore::render::material_authoring(config, RelayCore::SurfaceMaterial::Glass).transparent);
    CHECK(
        RelayCore::render::material_authoring(config, RelayCore::SurfaceMaterial::WallPanel).render_profile
        == RelayCore::render::MaterialRenderProfile::Default
    );
    CHECK(RelayCore::render::material_authoring(config, RelayCore::SurfaceMaterial::WallPanel).atlas_mix == 1.0F);
    const auto& matte = RelayCore::render::material_authoring(config, RelayCore::SurfaceMaterial::Matte);
    CHECK(matte.atlas_mix == 0.0F);
    CHECK(matte.render_profile == RelayCore::render::MaterialRenderProfile::Default);
    CHECK_FALSE(matte.transparent);
    CHECK(config.render_light.max_radius == doctest::Approx(48.0F));
    CHECK(config.lighting.local_light_gain == doctest::Approx(2.0F));
}

TEST_CASE("render authoring config rejects duplicate material entries")
{
    RelayCore::test::ScopedTempDirectory temp;
    RelayCore::test::ScopedTestLogger logger{temp.path() / "logs"};
    const std::filesystem::path temp_path = temp.path() / "relaycore_render_authoring_duplicate.toml";

    {
        std::ofstream out{temp_path, std::ios::trunc};
        REQUIRE(out.is_open());
        out << "[[materials]]\n";
        out << "name = \"floor\"\n";
        out << "[[materials]]\n";
        out << "name = \"floor\"\n";
    }

    const auto load_config = [&temp_path]() {
        static_cast<void>(RelayCore::render::load_render_authoring_config(temp_path));
    };
    CHECK_THROWS_WITH_AS(
        load_config(),
        doctest::Contains("Duplicate render authoring material entry"),
        std::runtime_error
    );
}

TEST_CASE("render authoring config rejects transparent non-glass materials")
{
    RelayCore::test::ScopedTempDirectory temp;
    RelayCore::test::ScopedTestLogger logger{temp.path() / "logs"};
    const std::filesystem::path temp_path = temp.path() / "relaycore_render_authoring_invalid_transparent.toml";

    {
        std::ofstream out{temp_path, std::ios::trunc};
        REQUIRE(out.is_open());
        out << "[[materials]]\n";
        out << "name = \"wall\"\n";
        out << "transparent = true\n";
    }

    const auto load_config = [&temp_path]() {
        static_cast<void>(RelayCore::render::load_render_authoring_config(temp_path));
    };
    CHECK_THROWS_WITH_AS(
        load_config(),
        doctest::Contains("may only use transparent=true with profile=glass"),
        std::runtime_error
    );
}

TEST_CASE("render authoring config rejects light strip materials without emissive contract")
{
    RelayCore::test::ScopedTempDirectory temp;
    RelayCore::test::ScopedTestLogger logger{temp.path() / "logs"};
    const std::filesystem::path temp_path = temp.path() / "relaycore_render_authoring_invalid_light_strip.toml";

    {
        std::ofstream out{temp_path, std::ios::trunc};
        REQUIRE(out.is_open());
        out << "[[materials]]\n";
        out << "name = \"light_strip\"\n";
        out << "emissive_strength = 0.0\n";
    }

    const auto load_config = [&temp_path]() {
        static_cast<void>(RelayCore::render::load_render_authoring_config(temp_path));
    };
    CHECK_THROWS_WITH_AS(load_config(), doctest::Contains("requires emissive_strength > 0"), std::runtime_error);
}

TEST_CASE("render authoring config rejects render light defaults outside configured bounds")
{
    RelayCore::test::ScopedTempDirectory temp;
    RelayCore::test::ScopedTestLogger logger{temp.path() / "logs"};
    const std::filesystem::path temp_path = temp.path() / "relaycore_render_authoring_invalid_render_light.toml";

    {
        std::ofstream out{temp_path, std::ios::trunc};
        REQUIRE(out.is_open());
        out << "[render_light]\n";
        out << "default_radius = 64.0\n";
    }

    const auto load_config = [&temp_path]() {
        static_cast<void>(RelayCore::render::load_render_authoring_config(temp_path));
    };
    CHECK_THROWS_WITH_AS(
        load_config(),
        doctest::Contains("default_radius inside [min_radius, max_radius]"),
        std::runtime_error
    );
}
