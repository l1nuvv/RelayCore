#include "relaycore/platform/window.h"

#include <SDL3/SDL.h>
#include <exception>
#include <iostream>
#include <memory>
#include <string_view>

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
        std::cout << "window_lifetime=passed\n";
    }
    return result;
}
