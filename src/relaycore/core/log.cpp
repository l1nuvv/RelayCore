/**
 * @file src/relaycore/core/log.cpp
 * @brief Shared logger initialization and access.
 */

#include "relaycore/core/log.h"

#include <ctime>
#include <filesystem>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <stdexcept>
#include <vector>

#ifndef RELAYCORE_ASSET_DIR
#define RELAYCORE_ASSET_DIR ""
#endif

namespace RelayCore::log
{
    namespace
    {
        std::shared_ptr<spdlog::logger> g_logger;
        std::mutex                      g_logger_mutex;
        std::string                     g_run_log_path;
        std::string                     g_latest_log_path;

        auto local_time(const std::time_t time_value) -> std::tm
        {
            std::tm tm_value{};
#if defined(_WIN32)
            localtime_s(&tm_value, &time_value);
#else
            localtime_r(&time_value, &tm_value);
#endif
            return tm_value;
        }

        auto make_run_stamp() -> std::string
        {
            const auto now      = std::chrono::system_clock::now();
            const auto now_time = std::chrono::system_clock::to_time_t(now);
            const auto tm_value = local_time(now_time);

            char buffer[32]{};
            std::strftime(buffer, sizeof(buffer), "%Y-%m-%d_%H-%M-%S", &tm_value);
            return std::string(buffer);
        }

        auto make_run_filename() -> std::string { return "RelayCore_" + make_run_stamp() + ".log"; }

        auto resolve_logs_directory(const std::string& configured_path) -> std::filesystem::path
        {
            const std::filesystem::path requested_path(configured_path);

            if (requested_path.is_absolute()) { return requested_path; }

            const std::filesystem::path asset_dir(RELAYCORE_ASSET_DIR);
            if (!asset_dir.empty())
            {
                const auto project_root = std::filesystem::weakly_canonical(asset_dir).parent_path();
                return project_root / requested_path;
            }

            return std::filesystem::absolute(requested_path);
        }
    } // namespace

    MessageBuffer::~MessageBuffer()
    {
        std::size_t pending_count = 0;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            pending_count = messages_.size();
        }

        if (pending_count > 0 && spdlog::default_logger_raw() != nullptr)
        {
            spdlog::default_logger_raw()->warn("MessageBuffer destroyed with {} pending message(s)", pending_count);
        }
    }

    void MessageBuffer::flush_info(const std::string_view title)
    {
        std::vector<std::string> pending;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (messages_.empty()) { return; }
            pending.swap(messages_);
        }

        log::info("{}: {}", title, pending.size());
        for (const auto& message: pending)
        {
            log::info("  {}", message);
        }
    }

    void MessageBuffer::clear()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        messages_.clear();
    }

    auto MessageBuffer::size() const -> std::size_t
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return messages_.size();
    }

    auto MessageBuffer::empty() const -> bool
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return messages_.empty();
    }

    void init(const Config& config)
    {
        std::lock_guard<std::mutex> lock(g_logger_mutex);
        if (g_logger) { return; }

        try
        {
            const std::filesystem::path logs_dir = std::filesystem::path(config.logs_directory);
            std::filesystem::create_directories(logs_dir);
            const std::filesystem::path resolved_logs_dir = std::filesystem::weakly_canonical(logs_dir);

            std::vector<spdlog::sink_ptr> sinks;
            g_run_log_path.clear();
            g_latest_log_path.clear();

            if (config.enable_console_sink)
            {
                auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
                console_sink->set_level(spdlog::level::trace);
                console_sink->set_pattern("[%H:%M:%S.%e] [%^%l%$] [%n] %v");
                console_sink->set_color_mode(spdlog::color_mode::automatic);
                sinks.push_back(console_sink);
            }

            if (config.enable_timestamped_log)
            {
                const auto run_path = resolved_logs_dir / make_run_filename();
                auto run_file_sink  = std::make_shared<spdlog::sinks::basic_file_sink_mt>(run_path.string(), false);
                run_file_sink->set_level(spdlog::level::trace);
                run_file_sink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] [%n] %v");
                sinks.push_back(run_file_sink);

                g_run_log_path = run_path.string();
            }

            if (config.enable_latest_log)
            {
                const auto latest_path = resolved_logs_dir / config.latest_filename;
                auto latest_file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(latest_path.string(), true);
                latest_file_sink->set_level(spdlog::level::trace);
                latest_file_sink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] [%n] %v");
                sinks.push_back(latest_file_sink);

                g_latest_log_path = latest_path.string();
            }

            if (sinks.empty()) { throw std::runtime_error("Logger initialization failed: no sinks configured"); }

            auto created_logger = std::make_shared<spdlog::logger>(config.logger_name, sinks.begin(), sinks.end());

            created_logger->set_level(config.level);
#ifndef NDEBUG
            created_logger->flush_on(spdlog::level::info);
#else
            created_logger->flush_on(spdlog::level::warn);
#endif

            spdlog::set_default_logger(created_logger);
            spdlog::flush_every(config.periodic_flush);

            g_logger = std::move(created_logger);

            log::info("Logger initialized");
            log::info("Resolved logs directory: {}", resolved_logs_dir.string());
            log::info("Logger name: {}", config.logger_name);
            log::info("Log level: {}", spdlog::level::to_string_view(config.level));
            if (!g_run_log_path.empty()) { log::info("Run log: {}", g_run_log_path); }
            if (!g_latest_log_path.empty()) { log::info("Latest log: {}", g_latest_log_path); }
        } catch (const spdlog::spdlog_ex& ex)
        {
            throw std::runtime_error(std::string("Failed to initialize logger: ") + ex.what());
        } catch (const std::filesystem::filesystem_error& ex)
        {
            throw std::runtime_error(std::string("Failed to prepare logs directory: ") + ex.what());
        }
    }

    void shutdown()
    {
        std::lock_guard<std::mutex> lock(g_logger_mutex);
        if (!g_logger) { return; }

        g_logger->flush();
        spdlog::shutdown();
        g_logger.reset();
        g_run_log_path.clear();
        g_latest_log_path.clear();
    }

    auto logger() -> spdlog::logger&
    {
        if (!g_logger) { throw std::runtime_error("RelayCore logger is not initialized"); }
        return *g_logger;
    }

    void flush()
    {
        if (g_logger) { g_logger->flush(); }
    }

    void set_level(const spdlog::level::level_enum level_value)
    {
        logger().set_level(level_value);
        logger().info("Log level changed to {}", spdlog::level::to_string_view(level_value));
    }

    auto level() -> spdlog::level::level_enum { return logger().level(); }

    auto run_log_path() -> const std::string& { return g_run_log_path; }

    auto latest_log_path() -> const std::string& { return g_latest_log_path; }

    void info(const std::string_view message) { logger().info(message); }

    void warn(const std::string_view message) { logger().warn(message); }

    void error(const std::string_view message) { logger().error(message); }
} // namespace RelayCore::log
