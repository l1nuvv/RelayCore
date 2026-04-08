#include "relaycore/core/log.h"
#include "relaycore/core/paths.h"
#include "relaycore/render/blockout_texture_atlas.h"

#include <doctest/doctest.h>
#include <filesystem>

namespace
{
    void init_test_logger()
    {
        RelayCore::log::shutdown();
        RelayCore::log::Config log_config{};
        log_config.enable_console_sink = false;
        log_config.enable_timestamped_log = false;
        log_config.logs_directory = (std::filesystem::temp_directory_path() / "relaycore_test_logs").string();
        RelayCore::log::init(log_config);
    }
} // namespace

TEST_CASE("blockout texture atlas loads from external asset")
{
    init_test_logger();

    const auto atlas =
        RelayCore::render::load_blockout_texture_atlas(RelayCore::core::asset_path("textures/blockout_atlas.bmp"));

    REQUIRE(atlas.has_value());
    CHECK(atlas->width == 512);
    CHECK(atlas->height == 256);
    CHECK(atlas->pixels.size() == static_cast<std::size_t>(atlas->width * atlas->height * 4));

    RelayCore::log::shutdown();
}

TEST_CASE("blockout texture atlas is generated with expected dimensions and alpha")
{
    init_test_logger();

    const RelayCore::render::CpuRgbaTextureData atlas = RelayCore::render::build_blockout_texture_atlas();

    CHECK((atlas.width % 8) == 0);
    CHECK((atlas.height % 4) == 0);
    REQUIRE(atlas.pixels.size() == static_cast<std::size_t>(atlas.width * atlas.height * 4));

    REQUIRE_FALSE(atlas.pixels.empty());
    CHECK(atlas.pixels[3] == 255);
    CHECK(atlas.pixels[(atlas.width * 4) - 1] == 255);
    CHECK(atlas.pixels[0] != atlas.pixels[64 * 4]);

    RelayCore::log::shutdown();
}
