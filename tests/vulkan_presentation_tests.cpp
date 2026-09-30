#include "relaycore/core/log.h"
#include "relaycore/core/sdl_error.h"
#include "relaycore/platform/window.h"
#include "relaycore/render/vulkan_device.h"
#include "relaycore/render/vulkan_swapchain.h"

#include <iostream>
#include <stdexcept>
#include <string_view>

#include "scoped_temp_directory.h"

#ifdef _WIN32
#include <Windows.h>
#include <malloc.h>

namespace
{
    struct HostAllocations {
        bool fail = false;
        size_t failures = 0;
        size_t live = 0;
    };

    auto VKAPI_CALL allocate(void* user, const size_t size, const size_t alignment, VkSystemAllocationScope) -> void*
    {
        auto& allocations = *static_cast<HostAllocations*>(user);
        if (allocations.fail)
        {
            ++allocations.failures;
            return nullptr;
        }
        auto* memory = _aligned_malloc(size, alignment < sizeof(void*) ? sizeof(void*) : alignment);
        if (memory != nullptr)
            ++allocations.live;
        return memory;
    }

    void VKAPI_CALL release(void* user, void* memory)
    {
        if (memory != nullptr)
        {
            --static_cast<HostAllocations*>(user)->live;
            _aligned_free(memory);
        }
    }

    auto VKAPI_CALL reallocate(
        void* user,
        void* original,
        const size_t size,
        const size_t alignment,
        const VkSystemAllocationScope scope
    ) -> void*
    {
        if (original == nullptr)
            return allocate(user, size, alignment, scope);
        if (size == 0)
        {
            release(user, original);
            return nullptr;
        }
        auto& allocations = *static_cast<HostAllocations*>(user);
        if (allocations.fail)
        {
            ++allocations.failures;
            return nullptr;
        }
        return _aligned_realloc(original, size, alignment < sizeof(void*) ? sizeof(void*) : alignment);
    }

    void pump_events()
    {
        SDL_Event event{};
        while (SDL_PollEvent(&event))
        {}
    }

    void show_color(
        RelayCore::render::VulkanSwapchain& swapchain,
        const RelayCore::Window& window,
        const std::array<float, 4>& color,
        const COLORREF expected
    )
    {
        const auto hwnd = static_cast<HWND>(SDL_GetPointerProperty(
            SDL_GetWindowProperties(window.handle()),
            SDL_PROP_WINDOW_WIN32_HWND_POINTER,
            nullptr
        ));
        if (hwnd == nullptr)
            throw std::runtime_error("SDL did not expose the presentation test HWND");
        COLORREF observed = CLR_INVALID;
        // Observe the presented client pixel, rather than only successful Vulkan return codes.
        for (int attempt = 0; attempt < 120; ++attempt)
        {
            pump_events();
            if (!swapchain.draw_clear(color))
                continue;
            SDL_Delay(8);
            RECT client{};
            if (!GetClientRect(hwnd, &client))
                throw std::runtime_error("Failed to query presentation test client rectangle");
            POINT center{client.right / 2, client.bottom / 2};
            if (!ClientToScreen(hwnd, &center))
                throw std::runtime_error("Failed to locate presentation test client pixel");
            const auto dc = GetDC(nullptr);
            observed = GetPixel(dc, center.x, center.y);
            ReleaseDC(nullptr, dc);
            if (observed == expected)
                return;
        }
        throw std::runtime_error(
            "Presented client pixel did not match the Vulkan clear color: expected " + std::to_string(expected)
            + ", observed " + std::to_string(observed)
        );
    }

    void check_presentation(const bool inject_allocation_failure)
    {
        const RelayCore::test::ScopedTempDirectory temp;
        RelayCore::log::init({.logs_directory = temp.path().string(), .enable_timestamped_log = false});
        for (int cycle = 0; cycle < 2; ++cycle)
        {
            RelayCore::Window window{
                {.title = "RelayCore Vulkan presentation check", .width = 320, .height = 240, .vsync = false},
                RelayCore::render::ColorPipelineMode::legacy_gamma,
                RelayCore::WindowGraphicsApi::vulkan
            };
            RelayCore::core::require_sdl(SDL_RaiseWindow(window.handle()), "Failed to raise Vulkan check window");
            RelayCore::core::require_sdl(
                SDL_SetWindowAlwaysOnTop(window.handle(), true),
                "Failed to expose Vulkan check client pixel"
            );
            RelayCore::render::VulkanDevice device{window};
            HostAllocations allocations{};
            VkAllocationCallbacks callbacks{
                .pUserData = &allocations,
                .pfnAllocation = allocate,
                .pfnReallocation = reallocate,
                .pfnFree = release,
            };
            {
                RelayCore::render::VulkanSwapchain swapchain{device, window, cycle == 0, &callbacks};
                show_color(swapchain, window, {1, 0, 0, 1}, RGB(255, 0, 0));
                for (int resize = 0; resize < 3; ++resize)
                {
                    RelayCore::core::require_sdl(
                        SDL_SetWindowSize(window.handle(), 360 + resize * 30, 260 + resize * 20),
                        "Failed to resize Vulkan check window"
                    );
                    RelayCore::core::require_sdl(
                        SDL_SyncWindow(window.handle()),
                        "Failed to synchronize Vulkan resize"
                    );
                    show_color(swapchain, window, {0, 1, 0, 1}, RGB(0, 255, 0));
                    const auto pixels = window.pixel_extent();
                    if (swapchain.extent().width != static_cast<uint32_t>(pixels.width)
                        || swapchain.extent().height != static_cast<uint32_t>(pixels.height))
                        throw std::runtime_error("Swapchain extent did not follow the resized window");

                    RelayCore::core::require_sdl(
                        SDL_MinimizeWindow(window.handle()),
                        "Failed to minimize Vulkan window"
                    );
                    RelayCore::core::require_sdl(SDL_SyncWindow(window.handle()), "Failed to synchronize minimize");
                    pump_events();
                    if (!window.presentation_state().minimized || swapchain.draw_clear({1, 0, 0, 1}))
                        throw std::runtime_error("Minimized Vulkan window continued presenting");
                    RelayCore::core::require_sdl(SDL_RestoreWindow(window.handle()), "Failed to restore Vulkan window");
                    RelayCore::core::require_sdl(SDL_SyncWindow(window.handle()), "Failed to synchronize restore");
                    show_color(swapchain, window, {0, 0, 1, 1}, RGB(0, 0, 255));
                }
                RelayCore::core::require_sdl(SDL_HideWindow(window.handle()), "Failed to hide Vulkan window");
                if (swapchain.draw_clear({1, 0, 0, 1}))
                    throw std::runtime_error("Hidden Vulkan window continued presenting");
                RelayCore::core::require_sdl(SDL_ShowWindow(window.handle()), "Failed to show Vulkan window");
                show_color(swapchain, window, {1, 0, 0, 1}, RGB(255, 0, 0));

                if (inject_allocation_failure)
                {
                    allocations.fail = true;
                    bool recreation_failed = false;
                    try
                    {
                        swapchain.recreate();
                    } catch (const std::runtime_error& error)
                    {
                        recreation_failed = std::string_view{error.what()}.find("Failed to recreate Vulkan swapchain")
                                            != std::string_view::npos;
                        if (!recreation_failed)
                            throw;
                    }
                    allocations.fail = false;
                    if (!recreation_failed || allocations.failures == 0 || swapchain.extent().width != 0)
                        throw std::runtime_error("Injected recreation failure did not discard the retired swapchain");
                    bool presentation_stopped = false;
                    try
                    {
                        static_cast<void>(swapchain.draw_clear({0, 1, 0, 1}));
                    } catch (const std::runtime_error& error)
                    {
                        presentation_stopped =
                            std::string_view{error.what()}.find("presentation stopped") != std::string_view::npos;
                    }
                    if (!presentation_stopped)
                        throw std::runtime_error("Presentation reused a retired swapchain after failed recreation");
                    swapchain.recreate();
                    show_color(swapchain, window, {0, 1, 0, 1}, RGB(0, 255, 0));
                }
                for (int frame = 0; frame < 120; ++frame)
                {
                    pump_events();
                    if (!swapchain.draw_clear({0, 1, 0, 1}))
                        throw std::runtime_error("Recovered swapchain did not sustain presentation");
                }
            }
            if (allocations.live != 0)
                throw std::runtime_error("Swapchain host allocations leaked after failure/recovery/shutdown");
        }
        std::cout << "vulkan_presentation=passed: clear pixels, resize, minimize/restore, hidden; allocation_failure="
                  << inject_allocation_failure << '\n';
    }
} // namespace
#endif

int main(const int argc, char** argv)
{
#ifndef _WIN32
    std::cerr << "vulkan_presentation=skipped: client pixel check requires Windows\n";
    return 77;
#else
    if (!SDL_Init(SDL_INIT_VIDEO))
    {
        std::cerr << "vulkan_presentation=skipped: " << SDL_GetError() << '\n';
        return 77;
    }
    int result = 0;
    try
    {
        check_presentation(argc == 2 && std::string_view{argv[1]} == "--allocation-failure");
    } catch (const std::exception& error)
    {
        std::cerr << "Vulkan presentation failed: " << error.what() << '\n';
        result = 1;
    }
    RelayCore::log::shutdown();
    SDL_Quit();
    return result;
#endif
}
