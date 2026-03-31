#pragma once

#include <spdlog/logger.h>

#include <memory>
#include <utility>
#include <string_view>

namespace RelayCore::log
{
    void init();

    void shutdown();

    [[nodiscard]] auto logger() -> spdlog::logger&;

    void info(std::string_view message);

    template<typename... Args>
    void trace(spdlog::format_string_t<Args...> fmt, Args&&... args)
    {
        logger().trace(fmt, std::forward<Args>(args)...);
    }

    template<typename... Args>
    void info(spdlog::format_string_t<Args...> fmt, Args&&... args) { logger().info(fmt, std::forward<Args>(args)...); }

    template<typename... Args>
    void debug(spdlog::format_string_t<Args...> fmt, Args&&... args)
    {
        logger().debug(fmt, std::forward<Args>(args)...);
    }

    template<typename... Args>
    void warn(spdlog::format_string_t<Args...> fmt, Args&&... args) { logger().warn(fmt, std::forward<Args>(args)...); }

    template<typename... Args>
    void error(spdlog::format_string_t<Args...> fmt, Args&&... args)
    {
        logger().error(fmt, std::forward<Args>(args)...);
    }

    template<typename... Args>
    void critical(spdlog::format_string_t<Args...> fmt, Args&&... args)
    {
        logger().critical(fmt, std::forward<Args>(args)...);
    }
} // namespace RelayCore::log
