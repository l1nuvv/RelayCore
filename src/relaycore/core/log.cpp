/**
 * @file src/relaycore/core/log.cpp
 * @brief Shared logger initialization and access.
 */

#include "relaycore/core/log.h"

#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>
#include <stdexcept>

namespace RelayCore::log
{
    namespace
    {
        std::shared_ptr<spdlog::logger> g_logger; ///< Shared engine logger instance.
    }

    void init()
    {
        if (g_logger) { return; }

        g_logger = spdlog::stdout_color_mt("engine");
        g_logger->set_pattern("[%H:%M:%S.%e] [%^%l%$] [%n] %v");

#ifndef NDEBUG
        g_logger->set_level(spdlog::level::trace);
#else
        g_logger->set_level(spdlog::level::info);
#endif

        g_logger->flush_on(spdlog::level::warn);

        spdlog::set_default_logger(g_logger);
    }

    void shutdown()
    {
        spdlog::shutdown();
        g_logger.reset();
    }

    auto logger() -> spdlog::logger&
    {
        if (!g_logger) { throw std::runtime_error("Logger is not initialized"); }

        return *g_logger;
    }

    void info(const std::string_view message) { logger().info(message); }
} // namespace RelayCore::log
