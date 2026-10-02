#pragma once

#include "relaycore/core/profiling.h"

#include <vulkan/vulkan.h>

#if defined(TRACY_ENABLE)
#include <tracy/TracyVulkan.hpp>
#define RELAYCORE_PROFILE_VK_ZONE(profiler, commands, name)                                                            \
    TracyVkNamedZone((profiler).context(), RELAYCORE_PROFILE_CONCAT(relaycore_vk_zone_, __LINE__), commands, name, true)
#else
#define RELAYCORE_PROFILE_VK_ZONE(profiler, commands, name) ((void) 0)
#endif

namespace RelayCore::render
{
    class VulkanDevice;

    class VulkanGpuProfiler {
    public:
        VulkanGpuProfiler(const VulkanDevice& device, const char* name);
        ~VulkanGpuProfiler() noexcept;
        VulkanGpuProfiler(const VulkanGpuProfiler&) = delete;
        auto operator=(const VulkanGpuProfiler&) -> VulkanGpuProfiler& = delete;
        // Collect/reset only completed queries; the owning renderer drains its submissions before destruction.
        void collect() const;
#if defined(TRACY_ENABLE)
        auto context() const noexcept -> tracy::VkCtx*
        {
            return context_;
        }

    private:
        tracy::VkCtx* context_ = nullptr;
#endif
    };
} // namespace RelayCore::render
