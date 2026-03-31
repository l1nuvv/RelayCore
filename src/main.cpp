#include <SDL3/SDL_main.h>
#include <SDL3/SDL.h>

#include "app/application.hpp"
#include "core/log.hpp"

#include <exception>

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

int main(int argc, char* argv[])
{
    (void)argc;
    (void)argv;

    LogLifetime log_lifetime{};

    try
    {
        RelayCore::Application app;
        return app.run();
    }
    catch (const std::exception& exception)
    {
        RelayCore::log::critical("Unhandled exception: {}", std::string_view{exception.what()});
        return 1;
    }
}
