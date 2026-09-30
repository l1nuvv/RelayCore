#include "relaycore/core/log.h"
#include "relaycore/core/paths.h"
#include "relaycore/core/profiling.h"
#include "relaycore/render/blockout_texture_atlas.h"

#include <doctest/doctest.h>
#include <filesystem>

#include "scoped_temp_directory.h"
#include "scoped_test_logger.h"

TEST_CASE("blockout texture atlas loads from external asset")
{
    RelayCore::test::ScopedTempDirectory temp;
    RelayCore::test::ScopedTestLogger logger{temp.path() / "logs"};

    const auto atlas =
        RelayCore::render::load_blockout_texture_atlas(RelayCore::core::asset_path("textures/blockout_atlas.bmp"));

    REQUIRE(atlas.has_value());
    CHECK(atlas->width == 2048);
    CHECK(atlas->height == 1024);
    CHECK(atlas->pixels.size() == static_cast<std::size_t>(atlas->width * atlas->height * 4));

    RelayCore::log::shutdown();
}

TEST_CASE("blockout texture atlas is generated with expected dimensions and alpha")
{
    RelayCore::test::ScopedTempDirectory temp;
    RelayCore::test::ScopedTestLogger logger{temp.path() / "logs"};

    const RelayCore::render::CpuRgbaTextureData atlas = RelayCore::render::build_blockout_texture_atlas();

    CHECK((atlas.width % 8) == 0);
    CHECK((atlas.height % 4) == 0);
    REQUIRE(atlas.pixels.size() == static_cast<std::size_t>(atlas.width * atlas.height * 4));

    REQUIRE_FALSE(atlas.pixels.empty());
    CHECK(atlas.pixels[3] == 255);
    CHECK(atlas.pixels[(atlas.width * 4) - 1] == 255);
    CHECK(atlas.pixels[0] != atlas.pixels[(atlas.width / 8) * 4]);

    RelayCore::log::shutdown();
}
