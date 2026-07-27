#include "relaycore/render/pipeline_authoring_overlay.h"

#include <doctest/doctest.h>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string_view>

#include "scoped_temp_directory.h"

namespace
{
    [[nodiscard]] auto write_overlay(const std::filesystem::path& root, const std::string_view contents)
        -> std::filesystem::path
    {
        const std::filesystem::path path = root / "pipeline_authoring_overlay.toml";
        std::ofstream out{path, std::ios::trunc};
        REQUIRE(out.is_open());
        out << contents;
        return path;
    }
} // namespace

TEST_CASE("pipeline authoring overlay loads global material and level scopes")
{
    RelayCore::test::ScopedTempDirectory temp;
    const std::filesystem::path path = write_overlay(
        temp.path(),
        "[global]\n"
        "indirect_bounce_strength = 0.12\n"
        "fog_start = 16.0\n"
        "fog_end = 42.0\n"
        "shadow_strength = 0.80\n"
        "\n"
        "[[materials]]\n"
        "name = \"glass\"\n"
        "base_color = [0.40, 0.50, 0.60]\n"
        "ambient_bias = 0.90\n"
        "\n"
        "[[levels]]\n"
        "path = \"levels/test_room.toml\"\n"
        "\n"
        "[[levels.solids]]\n"
        "id = \"relay_hall.glass_probe\"\n"
        "brightness = 0.82\n"
        "tint = [0.90, 1.00, 1.10]\n"
        "\n"
        "[[levels.render_lights]]\n"
        "id = \"relay_hall.key_light\"\n"
        "radius = 4.4\n"
        "intensity = 0.68\n"
    );

    const RelayCore::render::PipelineAuthoringOverlay overlay =
        RelayCore::render::load_pipeline_authoring_overlay(path);

    CHECK(overlay.source_path == path);
    REQUIRE(overlay.renderer.indirect_bounce_strength.has_value());
    CHECK(*overlay.renderer.indirect_bounce_strength == doctest::Approx(0.12F));
    REQUIRE(overlay.renderer.fog_start.has_value());
    CHECK(*overlay.renderer.fog_start == doctest::Approx(16.0F));
    REQUIRE(overlay.renderer.fog_end.has_value());
    CHECK(*overlay.renderer.fog_end == doctest::Approx(42.0F));
    REQUIRE(overlay.materials.size() == 1);
    CHECK(overlay.materials.front().material == RelayCore::SurfaceMaterial::Glass);
    REQUIRE(overlay.materials.front().base_color.has_value());
    CHECK(overlay.materials.front().base_color->x == doctest::Approx(0.40F));
    REQUIRE(overlay.levels.size() == 1);
    CHECK(overlay.levels.front().level_path.generic_string() == "levels/test_room.toml");
    REQUIRE(overlay.levels.front().solids.size() == 1);
    CHECK(overlay.levels.front().solids.front().authoring_id == "relay_hall.glass_probe");
    REQUIRE(overlay.levels.front().render_lights.size() == 1);
    CHECK(overlay.levels.front().render_lights.front().authoring_id == "relay_hall.key_light");

    RelayCore::render::RendererConfig renderer{};
    RelayCore::render::apply_renderer_authoring_overrides(renderer, overlay.renderer);
    CHECK(renderer.indirect_bounce_strength == doctest::Approx(0.12F));
    CHECK(renderer.fog_start == doctest::Approx(16.0F));
    CHECK(renderer.fog_end == doctest::Approx(42.0F));
    CHECK(renderer.shadow_strength == doctest::Approx(0.80F));

    RelayCore::render::RenderAuthoringConfig authoring = RelayCore::render::default_render_authoring_config();
    RelayCore::render::apply_material_authoring_overrides(authoring, overlay.materials);
    const RelayCore::render::MaterialAuthoringConfig& glass =
        RelayCore::render::material_authoring(authoring, RelayCore::SurfaceMaterial::Glass);
    CHECK(glass.base_color.x == doctest::Approx(0.40F));
    CHECK(glass.ambient_bias == doctest::Approx(0.90F));
}

TEST_CASE("pipeline authoring overlay rejects duplicate and empty entries")
{
    RelayCore::test::ScopedTempDirectory temp;

    SUBCASE("duplicate material")
    {
        const std::filesystem::path path = write_overlay(
            temp.path(),
            "[[materials]]\n"
            "name = \"glass\"\n"
            "base_color = [0.4, 0.5, 0.6]\n"
            "[[materials]]\n"
            "name = \"glass\"\n"
            "base_color = [0.3, 0.4, 0.5]\n"
        );
        CHECK_THROWS_WITH_AS(
            RelayCore::render::load_pipeline_authoring_overlay(path),
            doctest::Contains("duplicate material"),
            std::runtime_error
        );
    }

    SUBCASE("duplicate solid id")
    {
        const std::filesystem::path path = write_overlay(
            temp.path(),
            "[[levels]]\n"
            "path = \"levels/test_room.toml\"\n"
            "[[levels.solids]]\n"
            "id = \"duplicate\"\n"
            "brightness = 0.8\n"
            "[[levels.solids]]\n"
            "id = \"duplicate\"\n"
            "brightness = 0.9\n"
        );
        CHECK_THROWS_WITH_AS(
            RelayCore::render::load_pipeline_authoring_overlay(path),
            doctest::Contains("duplicate id"),
            std::runtime_error
        );
    }

    SUBCASE("empty document")
    {
        const std::filesystem::path path = write_overlay(temp.path(), "# no overrides\n");
        CHECK_THROWS_WITH_AS(
            RelayCore::render::load_pipeline_authoring_overlay(path),
            doctest::Contains("at least one override"),
            std::runtime_error
        );
    }

    SUBCASE("empty material override")
    {
        const std::filesystem::path path = write_overlay(
            temp.path(),
            "[[materials]]\n"
            "name = \"glass\"\n"
        );
        CHECK_THROWS_WITH_AS(
            RelayCore::render::load_pipeline_authoring_overlay(path),
            doctest::Contains("must override at least one field"),
            std::runtime_error
        );
    }
}

TEST_CASE("pipeline authoring overlay rejects invalid values and material names")
{
    RelayCore::test::ScopedTempDirectory temp;

    SUBCASE("unknown material")
    {
        const std::filesystem::path path = write_overlay(
            temp.path(),
            "[[materials]]\n"
            "name = \"unobtainium\"\n"
            "base_color = [0.4, 0.5, 0.6]\n"
        );
        CHECK_THROWS_WITH_AS(
            RelayCore::render::load_pipeline_authoring_overlay(path),
            doctest::Contains("unknown material"),
            std::runtime_error
        );
    }

    SUBCASE("invalid fog range")
    {
        const std::filesystem::path path = write_overlay(
            temp.path(),
            "[global]\n"
            "fog_start = 20.0\n"
            "fog_end = 10.0\n"
        );
        CHECK_THROWS_WITH_AS(
            RelayCore::render::load_pipeline_authoring_overlay(path),
            doctest::Contains("greater than fog_start"),
            std::runtime_error
        );
    }

    SUBCASE("negative solid brightness")
    {
        const std::filesystem::path path = write_overlay(
            temp.path(),
            "[[levels]]\n"
            "path = \"levels/test_room.toml\"\n"
            "[[levels.solids]]\n"
            "id = \"glass\"\n"
            "brightness = -0.1\n"
        );
        CHECK_THROWS_WITH_AS(
            RelayCore::render::load_pipeline_authoring_overlay(path),
            doctest::Contains("must be non-negative"),
            std::runtime_error
        );
    }
}

TEST_CASE("pipeline authoring overlay validates stable level targets")
{
    RelayCore::render::LevelVisualOverrides overrides{};
    overrides.level_path = "levels/test_room.toml";
    overrides.solids.push_back(RelayCore::render::SolidVisualOverrides{
        .authoring_id = "relay_hall.glass_probe",
        .brightness = 0.82F,
    });
    overrides.render_lights.push_back(RelayCore::render::RenderLightVisualOverrides{
        .authoring_id = "relay_hall.key_light",
        .intensity = 0.68F,
    });

    RelayCore::render::LevelAuthoringIdentityIndex identities{};
    identities.solid_ids.insert("relay_hall.glass_probe");
    identities.render_light_ids.insert("relay_hall.key_light");

    CHECK_NOTHROW(RelayCore::render::validate_level_authoring_overlay_targets(overrides, identities));

    identities.render_light_ids.clear();
    CHECK_THROWS_WITH_AS(
        RelayCore::render::validate_level_authoring_overlay_targets(overrides, identities),
        doctest::Contains("unknown or stale render-light id"),
        std::runtime_error
    );
}
