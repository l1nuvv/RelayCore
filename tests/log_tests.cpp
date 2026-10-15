#include "relaycore/core/log.h"
#include "relaycore/core/profiling.h"

#include <chrono>
#include <doctest/doctest.h>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>

#include "scoped_temp_directory.h"

namespace
{
    void write_text_file(const std::filesystem::path& path, const std::string_view contents)
    {
        std::ofstream out{path, std::ios::trunc};
        if (!out.is_open())
        {
            throw std::runtime_error("Failed to open log test output file");
        }
        out << contents;
    }

    auto read_text_file(const std::filesystem::path& path) -> std::string
    {
        std::ifstream in{path};
        if (!in.is_open())
        {
            throw std::runtime_error("Failed to open log test input file");
        }

        return {
            std::istreambuf_iterator<char>{in},
            std::istreambuf_iterator<char>{},
        };
    }
} // namespace

TEST_CASE("logger truncates latest log and prunes old timestamped runs")
{
    RelayCore::log::shutdown();

    RelayCore::test::ScopedTempDirectory temp;
    const std::filesystem::path logs_dir = temp.path() / "relaycore_log_tests";
    std::filesystem::create_directories(logs_dir);

    const std::filesystem::path old_run_a = logs_dir / "RelayCore_2026-04-01_10-00-00.log";
    const std::filesystem::path old_run_b = logs_dir / "RelayCore_2026-04-01_11-00-00.log";
    const std::filesystem::path old_run_c = logs_dir / "RelayCore_2026-04-01_12-00-00.log";
    const std::filesystem::path latest_log = logs_dir / "RelayCore_latest.log";

    write_text_file(old_run_a, "old-a\n");
    write_text_file(old_run_b, "old-b\n");
    write_text_file(old_run_c, "old-c\n");
    write_text_file(latest_log, "stale-latest\n");

    using namespace std::chrono_literals;
    const auto base_time = std::filesystem::file_time_type::clock::now() - 24h;
    std::filesystem::last_write_time(old_run_a, base_time + 1h);
    std::filesystem::last_write_time(old_run_b, base_time + 2h);
    std::filesystem::last_write_time(old_run_c, base_time + 3h);
    std::filesystem::last_write_time(latest_log, base_time + 4h);

    RelayCore::log::Config config{};
    config.enable_console_sink = false;
    config.enable_timestamped_log = true;
    config.logs_directory = logs_dir.string();
    config.max_timestamped_logs = 2;
    RelayCore::log::init(config);
    RelayCore::log::info("log test payload");
    RelayCore::log::flush();

    const std::filesystem::path run_log_path = RelayCore::log::run_log_path();
    const std::filesystem::path latest_log_path = RelayCore::log::latest_log_path();

    REQUIRE(std::filesystem::exists(run_log_path));
    REQUIRE(std::filesystem::exists(latest_log_path));
    CHECK(run_log_path.parent_path() == logs_dir);
    CHECK(latest_log_path == latest_log);

    const std::string latest_contents = read_text_file(latest_log_path);
    CHECK(latest_contents.find("stale-latest") == std::string::npos);
    CHECK(latest_contents.find("log test payload") != std::string::npos);
    CHECK(latest_contents.find("Session") != std::string::npos);

    RelayCore::log::shutdown();

    std::vector<std::filesystem::path> timestamped_logs;
    for (const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator(logs_dir))
    {
        if (!entry.is_regular_file())
        {
            continue;
        }

        const std::string filename = entry.path().filename().string();
        if (filename != "RelayCore_latest.log" && filename.starts_with("RelayCore_") && filename.ends_with(".log"))
        {
            timestamped_logs.push_back(entry.path());
        }
    }

    CHECK(timestamped_logs.size() == 2);
    CHECK(std::filesystem::exists(run_log_path));
    CHECK(!std::filesystem::exists(old_run_a));
    CHECK(!std::filesystem::exists(old_run_b));
}

TEST_CASE("logger keeps only the latest session file by default")
{
    RelayCore::log::shutdown();
    RelayCore::test::ScopedTempDirectory temp;
    RelayCore::log::Config config{};
    config.enable_console_sink = false;
    config.logs_directory = temp.path().string();

    RelayCore::log::init(config);
    RelayCore::log::info("previous session payload");
    RelayCore::log::shutdown();
    RelayCore::log::init(config);
    RelayCore::log::info("current session payload");
    RelayCore::log::shutdown();

    const auto contents = read_text_file(temp.path() / "RelayCore_latest.log");
    CHECK(contents.find("current session payload") != std::string::npos);
    CHECK(contents.find("previous session payload") == std::string::npos);
    CHECK(std::distance(std::filesystem::directory_iterator(temp.path()), std::filesystem::directory_iterator{}) == 1);
}
