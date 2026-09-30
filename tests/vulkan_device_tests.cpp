#include "relaycore/core/log.h"
#include "relaycore/platform/window.h"
#include "relaycore/render/vulkan_device.h"

#include <iostream>
#include <stdexcept>
#include <string_view>
#include <utility>

#include "scoped_temp_directory.h"

namespace
{
    void check_device_resources(const RelayCore::render::VulkanDevice& device)
    {
        const auto& capabilities = device.capabilities();
        if (capabilities.physical_api_version < RelayCore::render::kVulkanApiVersion)
            throw std::runtime_error("Selected GPU does not meet the Vulkan API baseline");
#if RELAYCORE_VULKAN_VALIDATION
        if (!capabilities.validation_enabled)
            throw std::runtime_error("Development startup did not enable Vulkan validation");
#else
        if (capabilities.validation_enabled)
            throw std::runtime_error("Non-development startup unexpectedly enabled Vulkan validation");
#endif
        VkBool32 present_supported = VK_FALSE;
        if (device.graphics_queue() == VK_NULL_HANDLE || device.present_queue() == VK_NULL_HANDLE
            || vkGetPhysicalDeviceSurfaceSupportKHR(
                   device.physical_device(),
                   capabilities.present_queue_family,
                   device.surface(),
                   &present_supported
               ) != VK_SUCCESS
            || !present_supported)
            throw std::runtime_error("Selected queue cannot present to the SDL surface");

        VmaAllocatorInfo allocator_info{};
        vmaGetAllocatorInfo(device.allocator(), &allocator_info);
        if (allocator_info.instance != device.instance() || allocator_info.physicalDevice != device.physical_device()
            || allocator_info.device != device.handle())
            throw std::runtime_error("VMA allocator belongs to a different Vulkan device/instance");

        const VkBufferCreateInfo buffer_info{
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .size = 256,
            .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        };
        const VmaAllocationCreateInfo allocation_info{
            .usage = VMA_MEMORY_USAGE_AUTO,
            .requiredFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
        };
        VkBuffer buffer = VK_NULL_HANDLE;
        VmaAllocation allocation = nullptr;
        if (vmaCreateBuffer(device.allocator(), &buffer_info, &allocation_info, &buffer, &allocation, nullptr)
            != VK_SUCCESS)
            throw std::runtime_error("VMA could not allocate the device lifetime probe buffer");
        VmaTotalStatistics allocated{};
        vmaCalculateStatistics(device.allocator(), &allocated);
        vmaDestroyBuffer(device.allocator(), buffer, allocation);
        VmaTotalStatistics released{};
        vmaCalculateStatistics(device.allocator(), &released);
        if (allocated.total.statistics.allocationCount != 1 || released.total.statistics.allocationCount != 0)
            throw std::runtime_error("VMA probe allocation was not released");

        const VkSemaphoreTypeCreateInfo type_info{
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO,
            .semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE,
            .initialValue = 7,
        };
        const VkSemaphoreCreateInfo semaphore_info{
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
            .pNext = &type_info,
        };
        VkSemaphore semaphore = VK_NULL_HANDLE;
        if (vkCreateSemaphore(device.handle(), &semaphore_info, nullptr, &semaphore) != VK_SUCCESS)
            throw std::runtime_error("Selected device did not enable timeline semaphores");
        uint64_t counter = 0;
        const auto counter_result = vkGetSemaphoreCounterValue(device.handle(), semaphore, &counter);
        vkDestroySemaphore(device.handle(), semaphore, nullptr);
        if (counter_result != VK_SUCCESS || counter != 7)
            throw std::runtime_error("Timeline semaphore counter query failed");
    }

    void run_checks(const bool expect_missing_validation)
    {
        RelayCore::Window window{
            RelayCore::WindowConfig{.width = 64, .height = 64, .vsync = false},
            RelayCore::render::ColorPipelineMode::legacy_gamma,
            RelayCore::WindowGraphicsApi::vulkan
        };
        if (expect_missing_validation)
        {
            try
            {
                RelayCore::render::VulkanDevice device{window};
            } catch (const std::runtime_error& error)
            {
                if (std::string_view{error.what()}.find("require VK_LAYER_KHRONOS_validation")
                    != std::string_view::npos)
                {
                    std::cout << "missing_validation=" << error.what() << '\n';
                    return;
                }
                throw;
            }
            throw std::runtime_error("Missing validation layers did not fail development startup");
        }

        for (int attempt = 0; attempt < 2; ++attempt)
        {
            bool failed_after_allocator_creation = false;
            try
            {
                // The startup log fails after instance/surface/device/allocator creation.
                RelayCore::render::VulkanDevice device{window};
            } catch (const std::runtime_error& error)
            {
                failed_after_allocator_creation =
                    std::string_view{error.what()} == "RelayCore logger is not initialized";
                if (!failed_after_allocator_creation)
                    throw;
            }
            if (!failed_after_allocator_creation)
                throw std::runtime_error("Expected late Vulkan startup failure did not occur");
        }

        const RelayCore::test::ScopedTempDirectory temp;
        RelayCore::log::init({.logs_directory = temp.path().string(), .enable_timestamped_log = false});
        for (int cycle = 0; cycle < 3; ++cycle)
        {
            RelayCore::render::VulkanDevice device{window};
            auto moved = std::move(device);
            check_device_resources(moved);
        }
        std::cout << "vulkan_device=passed\n";
    }
} // namespace

int main(const int argc, char** argv)
{
    if (!SDL_Init(SDL_INIT_VIDEO))
    {
        std::cerr << "vulkan_device=skipped: " << SDL_GetError() << '\n';
        return 77;
    }
    int result = 0;
    try
    {
        run_checks(argc == 2 && std::string_view{argv[1]} == "--expect-missing-validation");
    } catch (const std::exception& error)
    {
        std::cerr << "Vulkan device failed: " << error.what() << '\n';
        result = 1;
    }
    RelayCore::log::shutdown();
    SDL_Quit();
    return result;
}
