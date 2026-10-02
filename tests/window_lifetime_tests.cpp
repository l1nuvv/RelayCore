#include "relaycore/app/application.h"
#include "relaycore/app/engine_services.h"
#include "relaycore/core/log.h"
#include "relaycore/core/paths.h"
#include "relaycore/platform/window.h"

#include <SDL3/SDL.h>
#include <exception>
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
        bool shutdown_has_renderer = false;
        bool destructor_has_renderer = false;
        bool mesh_source_released = false;
    };

    class LifetimeDelegate final : public RelayCore::AppDelegate {
    public:
        LifetimeDelegate(DelegateLifetimeState& state, const bool throw_on_startup, const bool throw_on_update) :
            state_(state), throw_on_startup_(throw_on_startup), throw_on_update_(throw_on_update)
        {}

        ~LifetimeDelegate() override
        {
            ++state_.destructor_calls;
            try
            {
                state_.destructor_has_renderer = renderer_ && !renderer_->capture_frame().pixels.empty();
                mesh_ = {};
                state_.mesh_source_released = mesh_source_.expired();
            } catch (...)
            {
                state_.destructor_has_renderer = false;
            }
        }

        void on_startup(RelayCore::EngineServices& services) override
        {
            SDL_HideWindow(services.window().handle());
            renderer_ = &services.renderer();
            mesh_.initialize(RelayCore::render::make_color_box_mesh_data());
            mesh_source_ = mesh_.cpu_data();
            const std::array items{RelayCore::render::RenderItem{.mesh = &mesh_}};
            renderer_->begin_frame();
            renderer_->draw_render_queue(items, items);
            renderer_->finish_scene();
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
            try
            {
                state_.shutdown_has_renderer = renderer_ && !renderer_->capture_frame().pixels.empty();
            } catch (...)
            {
                state_.shutdown_has_renderer = false;
            }
        }

    private:
        DelegateLifetimeState& state_;
        bool throw_on_startup_;
        bool throw_on_update_;
        RelayCore::Renderer* renderer_ = nullptr;
        RelayCore::render::StaticColorMesh mesh_;
        std::weak_ptr<const RelayCore::render::CpuColorMeshData> mesh_source_;
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
    SDL_Quit();
    if (result == 0)
    {
        const RelayCore::test::ScopedTempDirectory temp;
        RelayCore::log::init({.logs_directory = temp.path().string(), .enable_timestamped_log = false});
        try
        {
            if (!SDL_Init(SDL_INIT_VIDEO))
                throw std::runtime_error(SDL_GetError());
            SDL_Rect usable{};
            if (!SDL_GetDisplayUsableBounds(SDL_GetPrimaryDisplay(), &usable))
                throw std::runtime_error(SDL_GetError());
            for (const bool high_dpi : {false, true})
            {
                RelayCore::Window window{RelayCore::WindowConfig{
                    .width = usable.w * 2,
                    .height = usable.h * 2,
                    .high_dpi = high_dpi,
                    .vsync = false,
                }};
                int x = 0, y = 0, width = 0, height = 0;
                if (!SDL_GetWindowPosition(window.handle(), &x, &y)
                    || !SDL_GetWindowSize(window.handle(), &width, &height)
                    || !SDL_GetDisplayUsableBounds(SDL_GetDisplayForWindow(window.handle()), &usable))
                    throw std::runtime_error(SDL_GetError());
                if (!(SDL_GetWindowFlags(window.handle()) & SDL_WINDOW_MAXIMIZED) || x < usable.x || y < usable.y
                    || x + width > usable.x + usable.w || y + height > usable.y + usable.h)
                    throw std::runtime_error("Oversized window client area extends beyond the usable desktop");
                std::cout << "window_fit=" << width << 'x' << height << ", high_dpi=" << high_dpi
                          << ", desktop=" << usable.w << 'x' << usable.h << '\n';
            }
            RelayCore::Window small{RelayCore::WindowConfig{.width = 320, .height = 240, .vsync = false}};
            int width = 0, height = 0;
            if (!SDL_GetWindowSize(small.handle(), &width, &height) || width != 320 || height != 240
                || (SDL_GetWindowFlags(small.handle()) & SDL_WINDOW_MAXIMIZED))
                throw std::runtime_error("Window that fits the display did not retain its requested size");

            {
                RelayCore::Window second{RelayCore::WindowConfig{.width = 64, .height = 64}};
                if ((SDL_GetWindowFlags(second.handle()) & SDL_WINDOW_VULKAN) == 0)
                    throw std::runtime_error("Second window has no Vulkan surface");
            }
            if ((SDL_GetWindowFlags(small.handle()) & SDL_WINDOW_VULKAN) == 0)
                throw std::runtime_error("Vulkan window ownership changed after another window was destroyed");
        } catch (const std::exception& error)
        {
            std::cerr << "Window display fit failed: " << error.what() << '\n';
            result = 1;
        }
        SDL_Quit();
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
            if (state.shutdown_calls != 1 || state.destructor_calls != 1 || !state.shutdown_has_renderer
                || !state.destructor_has_renderer || !state.mesh_source_released)
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
