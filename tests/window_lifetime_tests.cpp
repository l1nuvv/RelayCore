#include "relaycore/app/application.h"
#include "relaycore/app/engine_services.h"
#include "relaycore/core/log.h"
#include "relaycore/core/paths.h"
#include "relaycore/platform/window.h"

#include <SDL3/SDL.h>
#include <exception>
#include <glad/gl.h>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string_view>
#include <utility>

#include "scoped_temp_directory.h"

namespace
{
    struct DelegateLifetimeState {
        int shutdown_calls = 0;
        int destructor_calls = 0;
        bool shutdown_has_context = false;
        bool destructor_has_context = false;
        bool gpu_resource_released = false;
    };

    class LifetimeDelegate final : public RelayCore::AppDelegate {
    public:
        LifetimeDelegate(DelegateLifetimeState& state, const bool throw_on_startup, const bool throw_on_update) :
            state_(state), throw_on_startup_(throw_on_startup), throw_on_update_(throw_on_update)
        {}

        ~LifetimeDelegate() override
        {
            ++state_.destructor_calls;
            state_.destructor_has_context = SDL_GL_GetCurrentContext() != nullptr;
            if (state_.destructor_has_context && buffer_ != 0)
            {
                glDeleteBuffers(1, &buffer_);
                state_.gpu_resource_released = glGetError() == GL_NO_ERROR;
            }
        }

        void on_startup(RelayCore::EngineServices& services) override
        {
            SDL_HideWindow(services.window().handle());
            glCreateBuffers(1, &buffer_);
            if (buffer_ == 0 || glGetError() != GL_NO_ERROR)
                throw std::runtime_error("Could not create delegate GPU resource");
            if (throw_on_startup_)
                throw std::runtime_error("startup failure");
        }

        void on_update(const RelayCore::FrameInfo&, RelayCore::EngineServices& services) override
        {
            if (throw_on_update_)
                throw std::runtime_error("update failure");
            services.request_quit();
        }

        void on_shutdown(RelayCore::EngineServices&) noexcept override
        {
            ++state_.shutdown_calls;
            state_.shutdown_has_context = SDL_GL_GetCurrentContext() != nullptr;
        }

    private:
        DelegateLifetimeState& state_;
        bool throw_on_startup_;
        bool throw_on_update_;
        GLuint buffer_ = 0;
    };
} // namespace

int main()
{
    if (!SDL_Init(SDL_INIT_VIDEO))
    {
        std::cerr << "window_lifetime=skipped: " << SDL_GetError() << '\n';
        return 77;
    }

    int result = 0;
    for (const auto mode :
         {RelayCore::render::ColorPipelineMode::legacy_gamma, RelayCore::render::ColorPipelineMode::hardware_srgb})
    {
        try
        {
            // Logging is deliberately uninitialized: this fails after the GL context is created.
            RelayCore::Window window{RelayCore::WindowConfig{.width = 64, .height = 64, .vsync = false}, mode};
            std::cerr << "Window construction unexpectedly succeeded without a logger\n";
            result = 1;
        } catch (const std::exception& error)
        {
            if (std::string_view{error.what()} != "RelayCore logger is not initialized")
            {
                std::cerr << "window_lifetime=skipped: " << error.what() << '\n';
                result = 77;
                break;
            }

            int window_count = 0;
            const std::unique_ptr<SDL_Window*, decltype(&SDL_free)> windows{SDL_GetWindows(&window_count), SDL_free};
            if (!windows || window_count != 0 || SDL_GL_GetCurrentContext() != nullptr)
            {
                std::cerr << "Failed constructor retained SDL/GL resources: windows=" << window_count
                          << ", current_context=" << SDL_GL_GetCurrentContext() << '\n';
                result = 1;
            }
        }
        if (result != 0)
        {
            break;
        }
    }

    SDL_Quit();
    if (result == 0)
    {
        const RelayCore::test::ScopedTempDirectory temp;
        RelayCore::log::init({.logs_directory = temp.path().string(), .enable_timestamped_log = false});
        for (const auto [throw_on_startup, throw_on_update] :
             {std::pair{false, false}, std::pair{true, false}, std::pair{false, true}})
        {
            DelegateLifetimeState state{};
            try
            {
                RelayCore::Application app{
                    std::make_unique<LifetimeDelegate>(state, throw_on_startup, throw_on_update),
                    RelayCore::core::asset_path("config/engine.toml")
                };
                static_cast<void>(app.run());
                if (throw_on_startup || throw_on_update)
                    result = 1;
            } catch (const std::exception& error)
            {
                const std::string_view expected = throw_on_startup ? "startup failure" : "update failure";
                if ((!throw_on_startup && !throw_on_update) || error.what() != expected)
                {
                    std::cerr << "Application lifetime failed: " << error.what() << '\n';
                    result = 1;
                }
            }
            if (state.shutdown_calls != 1 || state.destructor_calls != 1 || !state.shutdown_has_context
                || !state.destructor_has_context || !state.gpu_resource_released)
            {
                std::cerr << "Delegate outlived engine resources or missed shutdown\n";
                result = 1;
            }
            if (result != 0)
                break;
        }
        RelayCore::log::shutdown();
    }
    if (result == 0)
    {
        std::cout << "window_lifetime=passed\n";
    }
    return result;
}
