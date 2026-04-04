/**
 * @file src/relaycore/app/run_application.cpp
 * @brief Top-level application bootstrap helper.
 */

#include "relaycore/app/run_application.h"

#include "relaycore/app/application.h"
#include "relaycore/core/log.h"

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
            RelayCore::log::init();
        }

        /**
         * @brief Shuts down the shared engine logger.
         */
        ~LogLifetime() noexcept
        {
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
        (void) argc;
        (void) argv;

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
    }
} // namespace RelayCore
