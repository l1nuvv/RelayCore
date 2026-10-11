#pragma once
#include "relaycore/render/vulkan_device.h"

#include <stdexcept>
#include <string>

namespace RelayCore::test
{
    // A host-signaled timeline blocks actual GPU execution, making early retirement checks deterministic.
    class BlockGraphicsQueue {
    public:
        explicit BlockGraphicsQueue(const render::VulkanDevice& device) : device_(device)
        {
            const VkSemaphoreTypeCreateInfo type{
                .sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO,
                .semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE
            };
            const VkSemaphoreCreateInfo info{.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO, .pNext = &type};
            require_result(
                vkCreateSemaphore(device_.handle(), &info, nullptr, &gate_),
                "Failed to create GPU test gate"
            );
        }
        void block()
        {
            const VkSemaphoreSubmitInfo wait{
                .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
                .semaphore = gate_,
                .value = 1,
                .stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT
            };
            const VkSubmitInfo2 submit{
                .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
                .waitSemaphoreInfoCount = 1,
                .pWaitSemaphoreInfos = &wait
            };
            require_result(
                vkQueueSubmit2(device_.graphics_queue(), 1, &submit, VK_NULL_HANDLE),
                "Failed to block GPU test queue"
            );
        }
        void release()
        {
            const VkSemaphoreSignalInfo signal{
                .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SIGNAL_INFO,
                .semaphore = gate_,
                .value = 1
            };
            require_result(vkSignalSemaphore(device_.handle(), &signal), "Failed to release GPU test gate");
            released_ = true;
        }
        ~BlockGraphicsQueue() noexcept
        {
            if (!released_)
            {
                const VkSemaphoreSignalInfo signal{
                    .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SIGNAL_INFO,
                    .semaphore = gate_,
                    .value = 1
                };
                static_cast<void>(vkSignalSemaphore(device_.handle(), &signal));
            }
            // Test-only cleanup releases queued waits even when an assertion throws.
            static_cast<void>(vkQueueWaitIdle(device_.graphics_queue()));
            vkDestroySemaphore(device_.handle(), gate_, nullptr);
        }

    private:
        static void require_result(VkResult result, const char* operation)
        {
            if (result != VK_SUCCESS)
                throw std::runtime_error(std::string(operation) + ": VkResult " + std::to_string(result));
        }
        const render::VulkanDevice& device_;
        VkSemaphore gate_ = VK_NULL_HANDLE;
        bool released_ = false;
    };

} // namespace RelayCore::test
