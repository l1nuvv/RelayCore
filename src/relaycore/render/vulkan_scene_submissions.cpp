#include "relaycore/render/vulkan_scene_submissions.h"

#include "relaycore/core/log.h"
#include "relaycore/core/profiling.h"
#include "relaycore/render/vulkan_device.h"

#include <algorithm>
#include <array>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace RelayCore::render
{
    namespace
    {
        void require_scene_vk(const VkResult result, const char* operation)
        {
            if (result != VK_SUCCESS)
                throw std::runtime_error(std::string(operation) + ": VkResult " + std::to_string(result));
        }
    } // namespace

    struct VulkanSceneSubmissions::State {
        struct RetiredBuffer {
            uint64_t completion;
            VulkanBuffer buffer;
        };
        struct RetiredTexture {
            uint64_t completion;
            VulkanTexture texture;
        };
        const VulkanDevice& device;
        VkSemaphore timeline = VK_NULL_HANDLE;
        uint64_t submitted = 0;
        std::vector<RetiredBuffer> buffers;
        std::vector<RetiredTexture> textures;

        explicit State(const VulkanDevice& owner) : device(owner) {}
        void validate(const VulkanSceneCompletion completion) const
        {
            if ((completion.value == 0 && completion.timeline != VK_NULL_HANDLE && completion.timeline != timeline)
                || (completion.value != 0 && completion.timeline != timeline) || completion.value > submitted)
                throw std::invalid_argument("Vulkan scene completion requires this timeline and a submitted value");
        }
    };

    VulkanSceneSubmissions::VulkanSceneSubmissions(const VulkanDevice& device) : state_(std::make_unique<State>(device))
    {
        const VkSemaphoreTypeCreateInfo type{
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO,
            .semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE
        };
        const VkSemaphoreCreateInfo info{.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO, .pNext = &type};
        require_scene_vk(
            vkCreateSemaphore(device.handle(), &info, nullptr, &state_->timeline),
            "Failed to create Vulkan scene timeline"
        );
    }

    VulkanSceneSubmissions::~VulkanSceneSubmissions() noexcept
    {
        try
        {
            wait(last_submission());
        } catch (const std::exception& error)
        {
            log::error_noexcept("[renderer] Vulkan scene shutdown failed", error.what());
        }
        state_->buffers.clear();
        state_->textures.clear();
        vkDestroySemaphore(state_->device.handle(), state_->timeline, nullptr);
    }

    auto VulkanSceneSubmissions::submit(
        const VkCommandBuffer commands,
        const VkSemaphoreSubmitInfo dependency,
        const VkSemaphoreSubmitInfo extra_signal,
        const VkFence fence,
        const VkCommandBuffer before
    ) -> VulkanSceneCompletion
    {
        if (!commands)
            throw std::invalid_argument("Vulkan scene submission requires a command buffer");
        if (state_->submitted == std::numeric_limits<uint64_t>::max())
            throw std::overflow_error("Vulkan scene timeline exhausted");
        const VulkanSceneCompletion next{state_->timeline, state_->submitted + 1};
        const std::array command_buffers{
            VkCommandBufferSubmitInfo{.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO, .commandBuffer = before},
            VkCommandBufferSubmitInfo{.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO, .commandBuffer = commands}
        };
        const VkSemaphoreSubmitInfo signal{
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
            .semaphore = next.timeline,
            .value = next.value,
            .stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT
        };
        const VkSemaphoreSubmitInfo previous{
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
            .semaphore = state_->timeline,
            .value = state_->submitted,
            .stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT
        };
        const std::array waits{previous, dependency};
        const std::array signals{signal, extra_signal};
        const auto first = state_->submitted ? 0U : 1U;
        const auto count = (state_->submitted ? 1U : 0U) + (dependency.semaphore ? 1U : 0U);
        const VkSubmitInfo2 info{
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
            .waitSemaphoreInfoCount = count,
            .pWaitSemaphoreInfos = waits.data() + first,
            .commandBufferInfoCount = before ? 2U : 1U,
            .pCommandBufferInfos = command_buffers.data() + (before ? 0 : 1),
            .signalSemaphoreInfoCount = extra_signal.semaphore ? 2U : 1U,
            .pSignalSemaphoreInfos = signals.data()
        };
        require_scene_vk(
            vkQueueSubmit2(state_->device.graphics_queue(), 1, &info, fence),
            "Failed to submit Vulkan scene"
        );
        state_->submitted = next.value;
        return next;
    }

    auto VulkanSceneSubmissions::last_submission() const noexcept -> VulkanSceneCompletion
    {
        return {state_->timeline, state_->submitted};
    }

    auto VulkanSceneSubmissions::completed() const -> uint64_t
    {
        uint64_t value = 0;
        require_scene_vk(
            vkGetSemaphoreCounterValue(state_->device.handle(), state_->timeline, &value),
            "Failed to query Vulkan scene completion"
        );
        return value;
    }

    void VulkanSceneSubmissions::wait(const VulkanSceneCompletion completion) const
    {
        state_->validate(completion);
        if (!completion.value)
            return;
        RELAYCORE_PROFILE_ZONE_N("Vulkan scene completion wait");
        const VkSemaphoreWaitInfo info{
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO,
            .semaphoreCount = 1,
            .pSemaphores = &state_->timeline,
            .pValues = &completion.value
        };
        require_scene_vk(
            vkWaitSemaphores(state_->device.handle(), &info, std::numeric_limits<uint64_t>::max()),
            "Failed to wait for Vulkan scene completion"
        );
    }

    void VulkanSceneSubmissions::collect()
    {
        RELAYCORE_PROFILE_ZONE_N("Vulkan scene retirement collect");
        const auto value = completed();
        std::erase_if(state_->buffers, [value](const auto& retired) {
            return retired.completion <= value;
        });
        std::erase_if(state_->textures, [value](const auto& retired) {
            return retired.completion <= value;
        });
    }

    void VulkanSceneSubmissions::retire(VulkanBuffer&& buffer, const VulkanSceneCompletion last_use)
    {
        state_->validate(last_use);
        if (buffer.allocator_ != state_->device.allocator() || !buffer.handle())
            throw std::invalid_argument("Vulkan scene retirement requires a nonempty buffer from this device");
        state_->buffers.emplace_back(last_use.value, std::move(buffer));
    }

    auto VulkanSceneSubmissions::pending_retirements() const noexcept -> size_t
    {
        return state_->buffers.size() + state_->textures.size();
    }

    void VulkanSceneSubmissions::retire(VulkanTexture&& texture, const VulkanSceneCompletion last_use)
    {
        state_->validate(last_use);
        if (!texture.state_ || texture.state_->device != state_->device.handle())
            throw std::invalid_argument("Vulkan scene retirement requires a nonempty texture from this device");
        state_->textures.emplace_back(last_use.value, std::move(texture));
    }
} // namespace RelayCore::render
