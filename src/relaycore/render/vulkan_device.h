#pragma once

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>
#include <vk_mem_alloc.h>
#include <vulkan/vulkan.h>

namespace RelayCore
{
    class Window;
}

namespace RelayCore::render
{
    inline constexpr uint32_t kVulkanApiVersion = VK_API_VERSION_1_4;

    struct VulkanDeviceCapabilities {
        std::string device_name;
        uint32_t loader_api_version = 0;
        uint32_t instance_api_version = 0;
        uint32_t physical_api_version = 0;
        uint32_t effective_api_version = 0;
        uint32_t driver_version = 0;
        uint32_t graphics_queue_family = 0;
        uint32_t present_queue_family = 0;
        bool graphics_compute = false;
        bool validation_enabled = false;
        bool memory_budget_enabled = false;
        float max_texture_anisotropy = 1.0F;
    };

    struct VulkanMemoryHeapUsage {
        uint64_t usage_bytes = 0, budget_bytes = 0;
        uint64_t block_bytes = 0, allocation_bytes = 0;
        uint32_t blocks = 0, allocations = 0;
        bool device_local = false;
    };

    struct VulkanMemoryUsage {
        bool driver_budget = false;
        std::vector<VulkanMemoryHeapUsage> heaps;
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
        // Only after the swapchain owner drains presentation and destroys every old swapchain.
        void recreate_surface(const Window& window) const;
        [[nodiscard]] auto graphics_queue() const noexcept -> VkQueue;
        [[nodiscard]] auto present_queue() const noexcept -> VkQueue;
        [[nodiscard]] auto allocator() const noexcept -> VmaAllocator;
        // Explicit diagnostic snapshot; never collected automatically by the frame loop.
        [[nodiscard]] auto memory_usage() const -> VulkanMemoryUsage;
        // Render-thread access, shared by scene, HDR and UI pipeline creation.
        [[nodiscard]] auto pipeline_cache() const noexcept -> VkPipelineCache;
        [[nodiscard]] auto select_depth_format(
            bool sampled_shadow,
            VkSampleCountFlagBits samples,
            VkFormat preferred = VK_FORMAT_UNDEFINED
        ) const -> VkFormat;
        [[nodiscard]] auto
        load_pipeline_cache(const std::filesystem::path& file, const std::filesystem::path& shaders) const -> bool;
        [[nodiscard]] auto
        save_pipeline_cache(const std::filesystem::path& file, const std::filesystem::path& shaders) const -> bool;

    private:
        struct State;
        std::unique_ptr<State> state_;
    };
} // namespace RelayCore::render
