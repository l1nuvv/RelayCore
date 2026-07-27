#include "relaycore/app/startup_options.h"

#include "relaycore/core/paths.h"

#include <stdexcept>
#include <string>

namespace RelayCore
{
    auto parse_startup_options(const int argc, char** argv) -> StartupOptions
    {
        StartupOptions options{.config_path = core::asset_path("config/engine.toml")};
        bool config_selected = false;
        bool color_pipeline_selected = false;
        for (int index = 1; index < argc; ++index)
        {
            const std::string option = argv[index];
            if (option == "--config")
            {
                if (index + 1 >= argc)
                {
                    throw std::runtime_error("--config requires a path");
                }
                if (config_selected)
                {
                    throw std::runtime_error("--config may be specified only once");
                }
                options.config_path = argv[++index];
                config_selected = true;
                if (!std::filesystem::exists(options.config_path))
                {
                    throw std::runtime_error("Explicit config file does not exist: " + options.config_path.string());
                }
                continue;
            }
            if (option == "--color-pipeline")
            {
                if (index + 1 >= argc)
                {
                    throw std::runtime_error("--color-pipeline requires legacy_gamma or hardware_srgb");
                }
                if (color_pipeline_selected)
                {
                    throw std::runtime_error("--color-pipeline may be specified only once");
                }
                const std::string value = argv[++index];
                if (value == "legacy_gamma")
                {
                    options.color_pipeline_mode = render::ColorPipelineMode::legacy_gamma;
                } else if (value == "hardware_srgb")
                {
                    options.color_pipeline_mode = render::ColorPipelineMode::hardware_srgb;
                } else
                {
                    throw std::runtime_error("Unknown --color-pipeline value: " + value);
                }
                color_pipeline_selected = true;
                continue;
            }
            throw std::runtime_error("Unknown startup option: " + option);
        }
        return options;
    }
} // namespace RelayCore
