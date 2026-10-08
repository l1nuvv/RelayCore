#pragma once

#include "relaycore/render/color_pipeline_mode.h"

#include <filesystem>

namespace RelayCore
{
    struct StartupOptions {
        std::filesystem::path config_path{};
        render::ColorPipelineMode color_pipeline_mode = render::ColorPipelineMode::hardware_srgb;
    };

    [[nodiscard]] auto parse_startup_options(int argc, char** argv) -> StartupOptions;
} // namespace RelayCore
