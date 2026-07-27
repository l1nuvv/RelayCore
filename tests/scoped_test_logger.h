#pragma once

#include "relaycore/core/log.h"

#include <filesystem>

namespace RelayCore::test
{
    class ScopedTestLogger {
    public:
        explicit ScopedTestLogger(
            const std::filesystem::path& logs_directory,
            const bool enable_timestamped_log = false
        )
        {
            RelayCore::log::shutdown();
            RelayCore::log::Config config{};
            config.enable_console_sink = false;
            config.enable_timestamped_log = enable_timestamped_log;
            config.logs_directory = logs_directory.string();
            RelayCore::log::init(config);
        }

        ~ScopedTestLogger() noexcept
        {
            try
            {
                RelayCore::log::shutdown();
            } catch (...)
            {}
        }

        ScopedTestLogger(const ScopedTestLogger&) = delete;
        auto operator=(const ScopedTestLogger&) -> ScopedTestLogger& = delete;
        ScopedTestLogger(ScopedTestLogger&&) = delete;
        auto operator=(ScopedTestLogger&&) -> ScopedTestLogger& = delete;
    };
} // namespace RelayCore::test
