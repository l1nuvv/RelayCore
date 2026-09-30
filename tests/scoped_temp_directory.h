#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <random>
#include <stdexcept>
#include <string>
#include <system_error>

namespace RelayCore::test
{
    class ScopedTempDirectory {
    public:
        ScopedTempDirectory()
        {
            const std::filesystem::path parent = std::filesystem::temp_directory_path() / "relaycore-tests";
            std::error_code error;
            std::filesystem::create_directories(parent, error);
            if (error)
            {
                throw std::runtime_error("Failed to create RelayCore test temp parent: " + error.message());
            }

            std::random_device random_device;
            for (std::uint32_t retry = 0; retry < 64; ++retry)
            {
                const auto timestamp = std::chrono::high_resolution_clock::now().time_since_epoch().count();
                const std::uint64_t counter = next_counter_.fetch_add(1, std::memory_order_relaxed);
                const std::uint32_t random_component = random_device();
                const std::filesystem::path candidate =
                    parent
                    / (std::to_string(timestamp) + "-" + std::to_string(counter) + "-"
                       + std::to_string(random_component) + "-" + std::to_string(retry));

                error.clear();
                if (std::filesystem::create_directory(candidate, error))
                {
                    path_ = candidate;
                    return;
                }

                if (error && error != std::errc::file_exists)
                {
                    throw std::runtime_error("Failed to create RelayCore test temp directory: " + error.message());
                }
            }

            throw std::runtime_error("Failed to create a unique RelayCore test temp directory after 64 attempts");
        }

        ~ScopedTempDirectory() noexcept
        {
            if (path_.empty())
            {
                return;
            }

            std::error_code error;
            static_cast<void>(std::filesystem::remove_all(path_, error));
        }

        ScopedTempDirectory(const ScopedTempDirectory&) = delete;
        auto operator=(const ScopedTempDirectory&) -> ScopedTempDirectory& = delete;
        ScopedTempDirectory(ScopedTempDirectory&&) = delete;
        auto operator=(ScopedTempDirectory&&) -> ScopedTempDirectory& = delete;

        [[nodiscard]] auto path() const noexcept -> const std::filesystem::path&
        {
            return path_;
        }

    private:
        inline static std::atomic_uint64_t next_counter_{0};
        std::filesystem::path path_;
    };
} // namespace RelayCore::test
