#include "relaycore/platform/window.h"

#include <SDL3/SDL.h>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string_view>

#include "scoped_temp_directory.h"

namespace
{
    void require_no_windows()
    {
        int count = 0;
        const std::unique_ptr<SDL_Window*, decltype(&SDL_free)> windows{SDL_GetWindows(&count), SDL_free};
        if (!windows || count != 0 || SDL_GL_GetCurrentContext() != nullptr)
            throw std::runtime_error("Vulkan window lifetime retained SDL/GL resources");
    }
} // namespace

int main()
{
    if (!SDL_Init(SDL_INIT_VIDEO))
    {
        std::cerr << "vulkan_window=skipped: " << SDL_GetError() << '\n';
        return 77;
    }

    int result = 0;
    try
    {
        const RelayCore::WindowConfig config{.width = 320, .height = 240, .vsync = false};
        constexpr auto mode = RelayCore::render::ColorPipelineMode::legacy_gamma;
        constexpr auto api = RelayCore::WindowGraphicsApi::vulkan;
        const RelayCore::test::ScopedTempDirectory temp;
        const auto missing_loader = (temp.path() / "missing-vulkan-loader.dll").string();
        if (!SDL_SetHint(SDL_HINT_VULKAN_LIBRARY, missing_loader.c_str()))
            throw std::runtime_error("Could not set the missing Vulkan loader hint");
        bool loader_failure = false;
        try
        {
            RelayCore::Window window{config, mode, api};
        } catch (const std::runtime_error& error)
        {
            loader_failure = std::string_view{error.what()}.starts_with("Failed to create SDL window:");
            std::cout << "missing_loader=" << error.what() << '\n';
        }
        SDL_ResetHint(SDL_HINT_VULKAN_LIBRARY);
        if (!loader_failure)
            throw std::runtime_error("Missing Vulkan loader did not fail window creation with an SDL diagnostic");
        require_no_windows();

        for (int cycle = 0; cycle < 3; ++cycle)
        {
            {
                RelayCore::Window window{config, mode, api};
                const auto flags = SDL_GetWindowFlags(window.handle());
                if (!(flags & SDL_WINDOW_VULKAN) || (flags & SDL_WINDOW_OPENGL) || window.gl_context() != nullptr
                    || SDL_GL_GetCurrentContext() != nullptr)
                    throw std::runtime_error("Vulkan window unexpectedly owns or activates an OpenGL context");

                const auto extensions = window.vulkan_instance_extensions();
                bool surface_extension = false;
                for (const char* extension : extensions)
                {
                    if (cycle == 0)
                        std::cout << "SDL Vulkan instance extension: " << extension << '\n';
                    surface_extension |= std::string_view{extension} == "VK_KHR_surface";
                }
                if (!surface_extension || extensions.size() < 2)
                    throw std::runtime_error("SDL did not report surface/platform Vulkan instance extensions");

                if (!SDL_SetWindowSize(window.handle(), 400, 300) || !SDL_SyncWindow(window.handle()))
                    throw std::runtime_error(SDL_GetError());
                const auto extent = window.pixel_extent();
                if (extent.width <= 0 || extent.height <= 0)
                    throw std::runtime_error("Resized Vulkan window has no drawable extent");
                if (!SDL_HideWindow(window.handle()) || !SDL_SyncWindow(window.handle())
                    || RelayCore::can_present(window.presentation_state()))
                    throw std::runtime_error("Hidden Vulkan window remains presentable");

                bool rejected_gl_swap = false;
                try
                {
                    window.swap_buffers();
                } catch (const std::logic_error&)
                {
                    rejected_gl_swap = true;
                }
                if (!rejected_gl_swap)
                    throw std::runtime_error("Vulkan window allowed OpenGL buffer swapping");
            }
            require_no_windows();
        }
        std::cout << "vulkan_window=passed\n";
    } catch (const std::exception& error)
    {
        std::cerr << "Vulkan window failed: " << error.what() << '\n';
        result = 1;
    }
    SDL_Quit();
    return result;
}
