#include "relaycore/app/run_application.hpp"

#include <exception>
#include <string_view>

#include "relaycore/app/application.hpp"
#include "relaycore/core/log.hpp"

namespace
{
    class LogLifetime final {
    public:
        LogLifetime() { RelayCore::log::init(); }
        ~LogLifetime() noexcept { RelayCore::log::shutdown(); }

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
            log::critical("Unhandled exception: {}", std::string_view{exception.what()});
            return 1;
        }
    }
} // namespace RelayCore
