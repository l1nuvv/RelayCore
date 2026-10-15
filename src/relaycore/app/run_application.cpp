/**
 * @file src/relaycore/app/run_application.cpp
 * @brief Top-level application bootstrap helper.
 */

#include "relaycore/app/run_application.h"

#include "relaycore/app/application.h"
#include "relaycore/app/startup_options.h"
#include "relaycore/core/log.h"
#include "relaycore/core/paths.h"
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
        try
        {
            LogLifetime log_lifetime{};

            try
            {
                const StartupOptions options = parse_startup_options(argc, argv);
                log::info(
                    "[app] Color pipeline mode: {}",
                    render::color_pipeline_mode_name(options.color_pipeline_mode)
                );
                Application app{std::move(app_delegate), options.config_path, options.color_pipeline_mode};
                log::info(
                    "[app] Session executable: {} | asset_root: {}",
                    (core::executable_base_path() / std::filesystem::path(argv[0]).filename()).string(),
                    core::asset_root_path().string()
                );
                return app.run();
            } catch (const std::exception& exception)
            {
                const std::string_view message{exception.what()};
                std::fprintf(stderr, "RelayCore exception: %s\n", exception.what());
                std::fflush(stderr);
                log::critical("Unhandled exception: {}", message);
                log::flush();
                return 1;
            }
        } catch (const std::exception& exception)
        {
            std::fprintf(stderr, "RelayCore logger initialization failed: %s\n", exception.what());
            std::fflush(stderr);
            return 1;
        }
    }
} // namespace RelayCore
