#pragma once

#include "relaycore/render/vulkan_resources.h"

#include <cstdint>

namespace RelayCore::render
{
    struct VulkanSceneCompletion {
        VkSemaphore timeline = VK_NULL_HANDLE;
        uint64_t value = 0;
    };

    class VulkanSceneSubmissions {
    public:
        // Device/queue outlive this owner; queue access belongs to the render thread.
        explicit VulkanSceneSubmissions(const VulkanDevice& device);
        ~VulkanSceneSubmissions() noexcept;
        VulkanSceneSubmissions(const VulkanSceneSubmissions&) = delete;
        auto operator=(const VulkanSceneSubmissions&) -> VulkanSceneSubmissions& = delete;

        [[nodiscard]] auto submit(VkCommandBuffer commands) -> VulkanSceneCompletion;
        [[nodiscard]] auto last_submission() const noexcept -> VulkanSceneCompletion;
        [[nodiscard]] auto completed() const -> uint64_t;
        void wait(VulkanSceneCompletion completion) const;
        void collect();
        // Uploads must already be complete. The caller supplies the resource's final scene use.
        void retire(VulkanBuffer&& buffer, VulkanSceneCompletion last_use);
        void retire(VulkanTexture&& texture, VulkanSceneCompletion last_use);

    private:
        struct State;
        std::unique_ptr<State> state_;
    };
} // namespace RelayCore::render
