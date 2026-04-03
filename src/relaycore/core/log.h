/**
 * @file src/relaycore/core/log.h
 * @brief Shared logging entry points built on top of spdlog.
 */

#pragma once

#include <chrono>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <spdlog/common.h>
#include <spdlog/fmt/fmt.h>
#include <spdlog/logger.h>
#include <spdlog/spdlog.h>

namespace RelayCore::log
{
    struct Config {
        std::string          logger_name            = "engine";
        std::string logs_directory = R"(E:\Code\C++\RelayCore\logs)";
        std::string          latest_filename        = "RelayCore_latest.log";
        bool                 enable_console_sink    = true;
        bool                 enable_timestamped_log = true;
        bool                 enable_latest_log      = true;
        std::chrono::seconds periodic_flush{3};
#ifndef NDEBUG
        spdlog::level::level_enum level = spdlog::level::trace;
#else
        spdlog::level::level_enum level = spdlog::level::info;
#endif
    };

    void init(const Config& config = {});

    void shutdown();

    [[nodiscard]] auto logger() -> spdlog::logger&;

    void flush();

    void set_level(spdlog::level::level_enum level);

    [[nodiscard]] auto level() -> spdlog::level::level_enum;

    [[nodiscard]] auto run_log_path() -> const std::string&;

    [[nodiscard]] auto latest_log_path() -> const std::string&;

    void info(std::string_view message);

    void warn(std::string_view message);

    void error(std::string_view message);

    template<typename... Args>
    void trace(spdlog::format_string_t<Args...> fmt, Args&&... args)
    {
        logger().trace(fmt, std::forward<Args>(args)...);
    }

    template<typename... Args>
    void debug(spdlog::format_string_t<Args...> fmt, Args&&... args)
    {
        logger().debug(fmt, std::forward<Args>(args)...);
    }

    template<typename... Args>
    void info(spdlog::format_string_t<Args...> fmt, Args&&... args) { logger().info(fmt, std::forward<Args>(args)...); }

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

    class MessageBuffer final {
    public:
        explicit MessageBuffer(const std::size_t reserve_size = 64) { messages_.reserve(reserve_size); }

        ~MessageBuffer();

        MessageBuffer(const MessageBuffer&) = delete;

        auto operator=(const MessageBuffer&) -> MessageBuffer& = delete;

        MessageBuffer(MessageBuffer&&) = delete;

        auto operator=(MessageBuffer&&) -> MessageBuffer& = delete;

        void add(std::string message)
        {
            std::lock_guard<std::mutex> lock(mutex_);
            messages_.push_back(std::move(message));
        }

        template<typename... Args>
        void add_formatted(spdlog::format_string_t<Args...> fmt_string, Args&&... args)
        {
            add(fmt::format(fmt_string, std::forward<Args>(args)...));
        }

        void flush_info(std::string_view title = "Buffered messages");

        void clear();

        [[nodiscard]] auto size() const -> std::size_t;

        [[nodiscard]] auto empty() const -> bool;

    private:
        std::vector<std::string> messages_{};
        mutable std::mutex       mutex_{};
    };
} // namespace RelayCore::log
