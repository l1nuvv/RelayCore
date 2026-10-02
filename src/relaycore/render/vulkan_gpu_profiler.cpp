#include "relaycore/render/vulkan_gpu_profiler.h"

#include "relaycore/render/vulkan_device.h"

#if defined(TRACY_ENABLE)
#include <cstring>
#include <stdexcept>
#include <vector>
#endif

namespace RelayCore::render
{
#if defined(TRACY_ENABLE)
    VulkanGpuProfiler::VulkanGpuProfiler(const VulkanDevice& device, const char* name)
    {
        uint32_t count = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(device.physical_device(), &count, nullptr);
        std::vector<VkQueueFamilyProperties> families(count);
        vkGetPhysicalDeviceQueueFamilyProperties(device.physical_device(), &count, families.data());
        if (families[device.capabilities().graphics_queue_family].timestampValidBits == 0)
            throw std::runtime_error("Tracy Vulkan GPU profiling requires timestamp queries on the graphics queue");
        const auto require = [](VkResult result) {
            if (result != VK_SUCCESS)
                throw std::runtime_error(
                    "Failed to initialize Tracy Vulkan GPU profiling: VkResult " + std::to_string(result)
                );
        };
        VkCommandPool pool = VK_NULL_HANDLE;
        try
        {
            const VkCommandPoolCreateInfo info{
                .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
                .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
                .queueFamilyIndex = device.capabilities().graphics_queue_family
            };
            require(vkCreateCommandPool(device.handle(), &info, nullptr, &pool));
            VkCommandBuffer commands = VK_NULL_HANDLE;
            const VkCommandBufferAllocateInfo allocation{
                .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
                .commandPool = pool,
                .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
                .commandBufferCount = 1
            };
            require(vkAllocateCommandBuffers(device.handle(), &allocation, &commands));
            // Tracy synchronizes this startup calibration; normal frames collect without queue/device idle.
            context_ = TracyVkContext(device.physical_device(), device.handle(), device.graphics_queue(), commands);
            TracyVkContextName(context_, name, static_cast<uint16_t>(std::strlen(name)));
        } catch (...)
        {
            if (pool)
                vkDestroyCommandPool(device.handle(), pool, nullptr);
            throw;
        }
        vkDestroyCommandPool(device.handle(), pool, nullptr);
    }
    VulkanGpuProfiler::~VulkanGpuProfiler() noexcept
    {
        TracyVkDestroy(context_);
    }
    void VulkanGpuProfiler::collect() const
    {
        TracyVkCollectHost(context_);
    }
#else
    VulkanGpuProfiler::VulkanGpuProfiler(const VulkanDevice&, const char*) {}
    VulkanGpuProfiler::~VulkanGpuProfiler() noexcept = default;
    void VulkanGpuProfiler::collect() const {}
#endif
} // namespace RelayCore::render
