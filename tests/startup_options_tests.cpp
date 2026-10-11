#include "relaycore/app/startup_options.h"
#include "relaycore/core/paths.h"

#include <doctest/doctest.h>
#include <filesystem>
#include <initializer_list>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{
    auto parse(std::initializer_list<const char*> arguments) -> RelayCore::StartupOptions
    {
        std::vector<char*> argv{};
        argv.reserve(arguments.size());
        for (const char* argument : arguments)
        {
            argv.push_back(const_cast<char*>(argument));
        }
        return RelayCore::parse_startup_options(static_cast<int>(argv.size()), argv.data());
    }
} // namespace

TEST_CASE("startup options retain the default engine config")
{
    const RelayCore::StartupOptions options = parse({"RelayCore.exe"});
    CHECK(options.config_path == RelayCore::core::asset_path("config/engine.toml"));
    CHECK(options.color_pipeline_mode == RelayCore::render::ColorPipelineMode::hardware_srgb);
}

TEST_CASE("startup options accept explicit colour pipeline modes")
{
    CHECK(
        parse({"RelayCore.exe", "--color-pipeline", "legacy_gamma"}).color_pipeline_mode
        == RelayCore::render::ColorPipelineMode::legacy_gamma
    );
    CHECK(
        parse({"RelayCore.exe", "--color-pipeline", "hardware_srgb"}).color_pipeline_mode
        == RelayCore::render::ColorPipelineMode::hardware_srgb
    );
}

TEST_CASE("startup options reject invalid colour pipeline selections")
{
    CHECK_THROWS_WITH_AS(
        parse({"RelayCore.exe", "--color-pipeline"}),
        "--color-pipeline requires legacy_gamma or hardware_srgb",
        std::runtime_error
    );
    CHECK_THROWS_WITH_AS(
        parse({"RelayCore.exe", "--color-pipeline", "unknown"}),
        "Unknown --color-pipeline value: unknown",
        std::runtime_error
    );
    CHECK_THROWS_WITH_AS(
        parse({"RelayCore.exe", "--color-pipeline", "legacy_gamma", "--color-pipeline", "hardware_srgb"}),
        "--color-pipeline may be specified only once",
        std::runtime_error
    );
}

TEST_CASE("startup options accept config and colour pipeline in either order")
{
    const std::string config_path = RelayCore::core::asset_path("config/rc022_capture.toml").string();
    CHECK(
        parse({"RelayCore.exe", "--config", config_path.c_str(), "--color-pipeline", "hardware_srgb"})
            .color_pipeline_mode
        == RelayCore::render::ColorPipelineMode::hardware_srgb
    );
    CHECK(
        parse({"RelayCore.exe", "--color-pipeline", "hardware_srgb", "--config", config_path.c_str()}).config_path
        == config_path
    );
}

TEST_CASE("startup options accept one explicit config path")
{
    const std::filesystem::path config_path = RelayCore::core::asset_path("config/rc022_capture.toml");
    const std::string config_argument = config_path.string();
    const RelayCore::StartupOptions options = parse({"RelayCore.exe", "--config", config_argument.c_str()});
    CHECK(options.config_path == config_path);
}

TEST_CASE("startup options reject invalid config arguments")
{
    CHECK_THROWS_WITH_AS(parse({"RelayCore.exe", "--config"}), "--config requires a path", std::runtime_error);
    CHECK_THROWS_WITH_AS(
        parse({"RelayCore.exe", "--unknown"}),
        "Unknown startup option: --unknown",
        std::runtime_error
    );
    CHECK_THROWS_WITH_AS(
        parse({"RelayCore.exe", "--config", "missing.toml"}),
        "Explicit config file does not exist: missing.toml",
        std::runtime_error
    );
}

TEST_CASE("startup options reject duplicate config selection")
{
    const std::string config_path = RelayCore::core::asset_path("config/rc022_capture.toml").string();
    CHECK_THROWS_WITH_AS(
        parse({"RelayCore.exe", "--config", config_path.c_str(), "--config", config_path.c_str()}),
        "--config may be specified only once",
        std::runtime_error
    );
}
