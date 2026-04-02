/**
 * @file src/relaycore/core/log.h
 * @brief Shared logging entry points built on top of spdlog.
 */

#pragma once

#include <memory>
#include <spdlog/logger.h>
#include <string_view>
#include <utility>

namespace RelayCore::log
{
    /**
     * @brief Initializes the shared engine logger.
     */
    void init();

    /**
     * @brief Shuts down the shared engine logger.
     */
    void shutdown();

    /**
     * @brief Returns the shared spdlog logger instance.
     * @return Shared engine logger.
     */
    [[nodiscard]] auto logger() -> spdlog::logger&;

    /**
     * @brief Writes an info-level message without formatting arguments.
     * @param message Message text to write.
     */
    void info(std::string_view message);

    /**
     * @brief Writes a trace-level formatted log message.
     * @tparam Args Format argument types.
     * @param fmt Format string accepted by spdlog.
     * @param args Format arguments.
     */
    template<typename... Args>
    void trace(spdlog::format_string_t<Args...> fmt, Args&&... args)
    {
        logger().trace(fmt, std::forward<Args>(args)...);
    }

    /**
     * @brief Writes an info-level formatted log message.
     * @tparam Args Format argument types.
     * @param fmt Format string accepted by spdlog.
     * @param args Format arguments.
     */
    template<typename... Args>
    void info(spdlog::format_string_t<Args...> fmt, Args&&... args)
    {
        logger().info(fmt, std::forward<Args>(args)...);
    }

    /**
     * @brief Writes a debug-level formatted log message.
     * @tparam Args Format argument types.
     * @param fmt Format string accepted by spdlog.
     * @param args Format arguments.
     */
    template<typename... Args>
    void debug(spdlog::format_string_t<Args...> fmt, Args&&... args)
    {
        logger().debug(fmt, std::forward<Args>(args)...);
    }

    /**
     * @brief Writes a warning-level formatted log message.
     * @tparam Args Format argument types.
     * @param fmt Format string accepted by spdlog.
     * @param args Format arguments.
     */
    template<typename... Args>
    void warn(spdlog::format_string_t<Args...> fmt, Args&&... args)
    {
        logger().warn(fmt, std::forward<Args>(args)...);
    }

    /**
     * @brief Writes an error-level formatted log message.
     * @tparam Args Format argument types.
     * @param fmt Format string accepted by spdlog.
     * @param args Format arguments.
     */
    template<typename... Args>
    void error(spdlog::format_string_t<Args...> fmt, Args&&... args)
    {
        logger().error(fmt, std::forward<Args>(args)...);
    }

    /**
     * @brief Writes a critical-level formatted log message.
     * @tparam Args Format argument types.
     * @param fmt Format string accepted by spdlog.
     * @param args Format arguments.
     */
    template<typename... Args>
    void critical(spdlog::format_string_t<Args...> fmt, Args&&... args)
    {
        logger().critical(fmt, std::forward<Args>(args)...);
    }
} // namespace RelayCore::log
