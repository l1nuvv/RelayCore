/**
 * @file src/relaycore/app/run_application.cpp
 * @brief Top-level application bootstrap helper.
 */

#include "relaycore/app/run_application.h"

#include "relaycore/app/application.h"
#include "relaycore/core/log.h"
#include "relaycore/core/profiling.h"

#include <cstdio>
#include <exception>
#include <string_view>

namespace
{
    /**
     * @brief Initializes logging for the duration of run_application().
     */
    class LogLifetime final {
    public:
        /**
         * @brief Initializes the shared engine logger.
         */
        LogLifetime()
        {
            RELAYCORE_PROFILE_FUNCTION();
            RelayCore::log::init();
        }

        /**
         * @brief Shuts down the shared engine logger.
         */
        ~LogLifetime() noexcept
        {
            RELAYCORE_PROFILE_FUNCTION();
            RelayCore::log::shutdown();
        }

        LogLifetime(const LogLifetime&) = delete;

        auto operator=(const LogLifetime&) -> LogLifetime& = delete;
    };
} // namespace

namespace RelayCore
{
    auto run_application(std::unique_ptr<AppDelegate> app_delegate, int argc, char** argv) -> int
    {
        RELAYCORE_PROFILE_FUNCTION();
        (void) argc;
        (void) argv;

        try
        {
            LogLifetime log_lifetime{};

            try
            {
                Application app{std::move(app_delegate)};
                return app.run();
            } catch (const std::exception& exception)
            {
                const std::string_view message{exception.what()};
                std::fprintf(stderr, "RelayCore exception: %s\n", message.data());
                std::fflush(stderr);
                log::critical("Unhandled exception: {}", message);
                log::flush();
                return 1;
            }
        } catch (const std::exception& exception)
        {
            const std::string_view message{exception.what()};
            std::fprintf(stderr, "RelayCore logger initialization failed: %s\n", message.data());
            std::fflush(stderr);
            return 1;
        }
    }
} // namespace RelayCore
