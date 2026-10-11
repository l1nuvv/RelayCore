#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vk_mem_alloc.h>
#include <vulkan/vulkan.h>

namespace RelayCore
{
    class Window;
}

namespace RelayCore::render
{
    inline constexpr uint32_t kVulkanApiVersion = VK_API_VERSION_1_3;
    inline constexpr uint32_t kVulkanPreferredApiVersion = VK_API_VERSION_1_4;

    struct VulkanDeviceCapabilities {
        std::string device_name;
        uint32_t loader_api_version = 0;
        uint32_t instance_api_version = 0;
        uint32_t physical_api_version = 0;
        uint32_t effective_api_version = 0;
        uint32_t driver_version = 0;
        uint32_t graphics_queue_family = 0;
        uint32_t present_queue_family = 0;
        bool validation_enabled = false;
        bool force_api_1_3 = false;
        float max_texture_anisotropy = 1.0F;
    };

    class VulkanDevice {
    public:
        // The Vulkan window and SDL must outlive this renderer-owned device state.
        explicit VulkanDevice(const Window& window);
        ~VulkanDevice() noexcept;

        VulkanDevice(const VulkanDevice&) = delete;
        auto operator=(const VulkanDevice&) -> VulkanDevice& = delete;
        VulkanDevice(VulkanDevice&&) noexcept;
        auto operator=(VulkanDevice&&) noexcept -> VulkanDevice&;

        [[nodiscard]] auto capabilities() const noexcept -> const VulkanDeviceCapabilities&;
        [[nodiscard]] auto instance() const noexcept -> VkInstance;
        [[nodiscard]] auto physical_device() const noexcept -> VkPhysicalDevice;
        [[nodiscard]] auto handle() const noexcept -> VkDevice;
        [[nodiscard]] auto surface() const noexcept -> VkSurfaceKHR;
        [[nodiscard]] auto graphics_queue() const noexcept -> VkQueue;
        [[nodiscard]] auto present_queue() const noexcept -> VkQueue;
        [[nodiscard]] auto allocator() const noexcept -> VmaAllocator;

    private:
        struct State;
        std::unique_ptr<State> state_;
    };
} // namespace RelayCore::render
