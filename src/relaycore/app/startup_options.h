#pragma once

#include <filesystem>

namespace RelayCore
{
    struct StartupOptions {
        std::filesystem::path config_path{};
    };

    [[nodiscard]] auto parse_startup_options(int argc, char** argv) -> StartupOptions;
} // namespace RelayCore
