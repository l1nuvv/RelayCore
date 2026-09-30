#include "relaycore/render/vulkan_uploads.h"

#include "relaycore/core/profiling.h"
#include "relaycore/render/vulkan_device.h"

#include <algorithm>
#include <cstdio>
#include <deque>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>

namespace RelayCore::render
{
    namespace
    {
        void require_upload_result(const VkResult result, const char* operation)
        {
            if (result != VK_SUCCESS)
                throw std::runtime_error(std::string(operation) + ": VkResult " + std::to_string(result));
        }

        void transition_rgba(
            const VkCommandBuffer commands,
            const VkImage image,
            const uint32_t first_mip,
            const uint32_t mip_count,
            const VkImageLayout old_layout,
            const VkImageLayout new_layout
        )
        {
            VkImageMemoryBarrier2 barrier{
                .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
                .oldLayout = old_layout,
                .newLayout = new_layout,
                .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .image = image,
                .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, first_mip, mip_count, 0, 1},
            };
            switch (old_layout)
            {
                case VK_IMAGE_LAYOUT_UNDEFINED:
                    break;
                case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
                    barrier.srcStageMask = VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT;
                    barrier.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
                    break;
                case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
                    barrier.srcStageMask = VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT;
                    barrier.srcAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT;
                    break;
                case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
                    // Readback may follow an upload without an intervening draw, or follow graphics sampling.
                    barrier.srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
                    barrier.srcAccessMask = VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT;
                    break;
                default:
                    throw std::logic_error("Unsupported Vulkan RGBA source layout");
            }
            switch (new_layout)
            {
                case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
                    barrier.dstStageMask = VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT;
                    barrier.dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
                    break;
                case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
                    barrier.dstStageMask = VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT;
                    barrier.dstAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT;
                    break;
                case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
                    barrier.dstStageMask = VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT;
                    barrier.dstAccessMask = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT;
                    break;
                default:
                    throw std::logic_error("Unsupported Vulkan RGBA destination layout");
            }
            const VkDependencyInfo dependency{
                .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
                .imageMemoryBarrierCount = 1,
                .pImageMemoryBarriers = &barrier,
            };
            vkCmdPipelineBarrier2(commands, &dependency);
        }

        void transfer_to_host(const VkCommandBuffer commands)
        {
            const VkMemoryBarrier2 barrier{
                .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
                .srcStageMask = VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT,
                .srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
                .dstStageMask = VK_PIPELINE_STAGE_2_HOST_BIT,
                .dstAccessMask = VK_ACCESS_2_HOST_READ_BIT,
            };
            const VkDependencyInfo dependency{
                .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
                .memoryBarrierCount = 1,
                .pMemoryBarriers = &barrier,
            };
            vkCmdPipelineBarrier2(commands, &dependency);
        }
    } // namespace

    struct VulkanUploads::State {
        struct Batch {
            VkDevice device;
            VkCommandPool pool = VK_NULL_HANDLE;
            VkCommandBuffer commands = VK_NULL_HANDLE;
            VulkanBuffer transfer;
            uint64_t completion = 0;
            explicit Batch(const VkDevice owner) : device(owner) {}
            ~Batch() noexcept
            {
                if (pool != VK_NULL_HANDLE)
                    vkDestroyCommandPool(device, pool, nullptr);
            }
        };
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
        uint64_t last_submission = 0;
        std::deque<std::unique_ptr<Batch>> batches;
        std::vector<RetiredBuffer> buffers;
        std::vector<RetiredTexture> textures;
        explicit State(const VulkanDevice& owner) : device(owner) {}

        void wait(const uint64_t completion) const
        {
            if (completion > last_submission)
                throw std::invalid_argument("Vulkan upload completion has not been submitted on this timeline");
            if (completion == 0)
                return;
            RELAYCORE_PROFILE_ZONE_N("Vulkan upload wait");
            const VkSemaphoreWaitInfo wait_info{
                .sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO,
                .semaphoreCount = 1,
                .pSemaphores = &timeline,
                .pValues = &completion,
            };
            require_upload_result(
                vkWaitSemaphores(device.handle(), &wait_info, std::numeric_limits<uint64_t>::max()),
                "Failed to wait for Vulkan upload completion"
            );
        }

        ~State() noexcept
        {
            try
            {
                wait(last_submission);
            } catch (const std::exception& error)
            {
                std::fprintf(stderr, "Vulkan upload shutdown failed: %s\n", error.what());
            }
            batches.clear();
            buffers.clear();
            textures.clear();
            if (timeline != VK_NULL_HANDLE)
                vkDestroySemaphore(device.handle(), timeline, nullptr);
        }

        auto begin_batch() const -> std::unique_ptr<Batch>
        {
            auto batch = std::make_unique<Batch>(device.handle());
            const VkCommandPoolCreateInfo pool_info{
                .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
                .queueFamilyIndex = device.capabilities().graphics_queue_family,
            };
            require_upload_result(
                vkCreateCommandPool(device.handle(), &pool_info, nullptr, &batch->pool),
                "Failed to create Vulkan upload command pool"
            );
            const VkCommandBufferAllocateInfo allocate_info{
                .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
                .commandPool = batch->pool,
                .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
                .commandBufferCount = 1,
            };
            require_upload_result(
                vkAllocateCommandBuffers(device.handle(), &allocate_info, &batch->commands),
                "Failed to allocate Vulkan upload commands"
            );
            const VkCommandBufferBeginInfo begin_info{
                .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
                .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
            };
            require_upload_result(
                vkBeginCommandBuffer(batch->commands, &begin_info),
                "Failed to begin Vulkan upload commands"
            );
            return batch;
        }

        auto submit(std::unique_ptr<Batch> batch) -> uint64_t
        {
            require_upload_result(vkEndCommandBuffer(batch->commands), "Failed to end Vulkan upload commands");
            if (last_submission == std::numeric_limits<uint64_t>::max())
                throw std::overflow_error("Vulkan upload timeline exhausted");
            batch->completion = last_submission + 1;
            const VkCommandBufferSubmitInfo commands{
                .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
                .commandBuffer = batch->commands,
            };
            const VkSemaphoreSubmitInfo signal{
                .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
                .semaphore = timeline,
                .value = batch->completion,
                .stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
            };
            const VkSemaphoreSubmitInfo wait{
                .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
                .semaphore = timeline,
                .value = last_submission,
                .stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
            };
            const VkSubmitInfo2 submission{
                .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
                .waitSemaphoreInfoCount = 1,
                .pWaitSemaphoreInfos = &wait,
                .commandBufferInfoCount = 1,
                .pCommandBufferInfos = &commands,
                .signalSemaphoreInfoCount = 1,
                .pSignalSemaphoreInfos = &signal,
            };
            // Reserve CPU ownership before submitting; no allocation can fail after work is in flight.
            batches.push_back(std::move(batch));
            const auto result = vkQueueSubmit2(device.graphics_queue(), 1, &submission, VK_NULL_HANDLE);
            if (result != VK_SUCCESS)
            {
                batches.pop_back();
                require_upload_result(result, "Failed to submit Vulkan upload");
            }
            return ++last_submission;
        }

        void validate_retirement(const VkSemaphore owner, const uint64_t uploaded, const uint64_t completion) const
        {
            if (owner != timeline || uploaded == 0 || completion < uploaded || completion > last_submission)
                throw std::invalid_argument(
                    "Vulkan retirement requires this upload timeline and a submitted value at or after the resource "
                    "upload"
                );
        }
    };

    VulkanUploads::VulkanUploads(const VulkanDevice& device) : state_(std::make_unique<State>(device))
    {
        const VkSemaphoreTypeCreateInfo type_info{
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO,
            .semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE,
        };
        const VkSemaphoreCreateInfo semaphore_info{
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
            .pNext = &type_info
        };
        require_upload_result(
            vkCreateSemaphore(device.handle(), &semaphore_info, nullptr, &state_->timeline),
            "Failed to create Vulkan upload timeline"
        );
    }
    VulkanUploads::~VulkanUploads() noexcept = default;

    auto VulkanUploads::upload_mesh_buffer(const std::span<const std::byte> bytes, const VkBufferUsageFlagBits kind)
        -> VulkanBufferUpload
    {
        RELAYCORE_PROFILE_ZONE_N("Vulkan mesh upload");
        if (bytes.empty())
            throw std::invalid_argument("Vulkan mesh upload requires nonempty data");
        if (kind != VK_BUFFER_USAGE_VERTEX_BUFFER_BIT && kind != VK_BUFFER_USAGE_INDEX_BUFFER_BIT)
            throw std::invalid_argument("Vulkan mesh upload requires vertex or index buffer usage");
        VulkanBuffer destination{
            state_->device,
            bytes.size_bytes(),
            static_cast<VkBufferUsageFlags>(kind) | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VulkanBufferMemory::device
        };
        auto batch = state_->begin_batch();
        batch->transfer = VulkanBuffer{
            state_->device,
            bytes.size_bytes(),
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VulkanBufferMemory::upload
        };
        batch->transfer.write(bytes);
        const VkBufferCopy copy{.size = bytes.size_bytes()};
        vkCmdCopyBuffer(batch->commands, batch->transfer.handle(), destination.handle(), 1, &copy);
        const VkBufferMemoryBarrier2 barrier{
            .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2,
            .srcStageMask = VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT,
            .srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
            .dstStageMask = kind == VK_BUFFER_USAGE_VERTEX_BUFFER_BIT ? VK_PIPELINE_STAGE_2_VERTEX_ATTRIBUTE_INPUT_BIT
                                                                      : VK_PIPELINE_STAGE_2_INDEX_INPUT_BIT,
            .dstAccessMask = kind == VK_BUFFER_USAGE_VERTEX_BUFFER_BIT ? VK_ACCESS_2_VERTEX_ATTRIBUTE_READ_BIT
                                                                       : VK_ACCESS_2_INDEX_READ_BIT,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .buffer = destination.handle(),
            .size = VK_WHOLE_SIZE,
        };
        const VkDependencyInfo dependency{
            .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
            .bufferMemoryBarrierCount = 1,
            .pBufferMemoryBarriers = &barrier
        };
        vkCmdPipelineBarrier2(batch->commands, &dependency);
        const auto completion = state_->submit(std::move(batch));
        destination.upload_timeline_ = state_->timeline;
        destination.upload_completion_ = completion;
        return {std::move(destination), completion};
    }

    auto VulkanUploads::upload_rgba(const CpuRgbaTextureData& data, const TextureEncoding encoding)
        -> VulkanTextureUpload
    {
        RELAYCORE_PROFILE_ZONE_N("Vulkan texture upload");
        if (data.width <= 0 || data.height <= 0)
            throw std::invalid_argument("Vulkan RGBA upload requires positive dimensions");
        const auto size = static_cast<uint64_t>(data.width) * static_cast<uint64_t>(data.height) * 4;
        if (data.pixels.size() != size)
            throw std::invalid_argument("Vulkan RGBA upload size mismatch");
        VulkanTexture texture{state_->device, data.width, data.height, encoding};
        auto batch = state_->begin_batch();
        batch->transfer =
            VulkanBuffer{state_->device, size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VulkanBufferMemory::upload};
        batch->transfer.write(std::as_bytes(std::span{data.pixels}));
        transition_rgba(
            batch->commands,
            texture.image(),
            0,
            texture.mip_levels(),
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
        );
        const VkBufferImageCopy copy{
            .imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1},
            .imageExtent = {static_cast<uint32_t>(data.width), static_cast<uint32_t>(data.height), 1},
        };
        vkCmdCopyBufferToImage(
            batch->commands,
            batch->transfer.handle(),
            texture.image(),
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            1,
            &copy
        );
        int width = data.width;
        int height = data.height;
        for (uint32_t mip = 1; mip < texture.mip_levels(); ++mip)
        {
            transition_rgba(
                batch->commands,
                texture.image(),
                mip - 1,
                1,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL
            );
            const int next_width = std::max(1, width / 2);
            const int next_height = std::max(1, height / 2);
            const VkImageBlit blit{
                .srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, mip - 1, 0, 1},
                .srcOffsets = {{0, 0, 0}, {width, height, 1}},
                .dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, mip, 0, 1},
                .dstOffsets = {{0, 0, 0}, {next_width, next_height, 1}},
            };
            vkCmdBlitImage(
                batch->commands,
                texture.image(),
                VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                texture.image(),
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                1,
                &blit,
                VK_FILTER_LINEAR
            );
            transition_rgba(
                batch->commands,
                texture.image(),
                mip - 1,
                1,
                VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
            );
            width = next_width;
            height = next_height;
        }
        transition_rgba(
            batch->commands,
            texture.image(),
            texture.mip_levels() - 1,
            1,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
        );
        const auto completion = state_->submit(std::move(batch));
        texture.state_->upload_timeline = state_->timeline;
        texture.state_->upload_completion = completion;
        return {std::move(texture), completion};
    }

    void VulkanUploads::wait(const uint64_t completion) const
    {
        state_->wait(completion);
    }
    void VulkanUploads::collect()
    {
        uint64_t completed = 0;
        require_upload_result(
            vkGetSemaphoreCounterValue(state_->device.handle(), state_->timeline, &completed),
            "Failed to query Vulkan upload completion"
        );
        while (!state_->batches.empty() && state_->batches.front()->completion <= completed)
            state_->batches.pop_front();
        std::erase_if(state_->buffers, [completed](const auto& entry) {
            return entry.completion <= completed;
        });
        std::erase_if(state_->textures, [completed](const auto& entry) {
            return entry.completion <= completed;
        });
    }
    void VulkanUploads::retire(VulkanBuffer&& buffer, const uint64_t completion)
    {
        state_->validate_retirement(buffer.upload_timeline_, buffer.upload_completion_, completion);
        state_->buffers.emplace_back(completion, std::move(buffer));
    }
    void VulkanUploads::retire(VulkanTexture&& texture, const uint64_t completion)
    {
        if (!texture.state_)
            throw std::invalid_argument("Cannot retire an empty Vulkan texture");
        state_->validate_retirement(texture.state_->upload_timeline, texture.state_->upload_completion, completion);
        state_->textures.emplace_back(completion, std::move(texture));
    }

    auto VulkanUploads::readback(const VulkanBuffer& buffer) -> std::vector<std::byte>
    {
        if (buffer.handle() == VK_NULL_HANDLE || buffer.allocator_ != state_->device.allocator()
            || !(buffer.usage_ & VK_BUFFER_USAGE_TRANSFER_SRC_BIT))
            throw std::invalid_argument("Vulkan buffer readback requires this device and transfer-source usage");
        std::vector<std::byte> bytes(static_cast<size_t>(buffer.size()));
        auto batch = state_->begin_batch();
        batch->transfer =
            VulkanBuffer{state_->device, buffer.size(), VK_BUFFER_USAGE_TRANSFER_DST_BIT, VulkanBufferMemory::readback};
        const VkMemoryBarrier2 source_barrier{
            .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
            .srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
            .srcAccessMask = VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT,
            .dstStageMask = VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT,
            .dstAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT,
        };
        const VkDependencyInfo dependency{
            .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
            .memoryBarrierCount = 1,
            .pMemoryBarriers = &source_barrier
        };
        vkCmdPipelineBarrier2(batch->commands, &dependency);
        const VkBufferCopy copy{.size = buffer.size()};
        vkCmdCopyBuffer(batch->commands, buffer.handle(), batch->transfer.handle(), 1, &copy);
        transfer_to_host(batch->commands);
        auto* submitted = batch.get();
        const auto completion = state_->submit(std::move(batch));
        state_->wait(completion);
        submitted->transfer.read(bytes);
        collect();
        return bytes;
    }

    auto VulkanUploads::readback(const VulkanTexture& texture) -> std::vector<std::byte>
    {
        if (!texture.state_ || texture.state_->allocator != state_->device.allocator())
            throw std::invalid_argument("Vulkan texture readback requires a texture from this device");
        std::vector<VkBufferImageCopy> copies;
        auto extent = texture.extent();
        VkDeviceSize size = 0;
        for (uint32_t mip = 0; mip < texture.mip_levels(); ++mip)
        {
            copies.push_back(
                {.bufferOffset = size,
                 .imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, mip, 0, 1},
                 .imageExtent = {extent.width, extent.height, 1}}
            );
            size += static_cast<VkDeviceSize>(extent.width) * extent.height * 4;
            extent.width = std::max(1U, extent.width / 2);
            extent.height = std::max(1U, extent.height / 2);
        }
        std::vector<std::byte> bytes(static_cast<size_t>(size));
        auto batch = state_->begin_batch();
        batch->transfer =
            VulkanBuffer{state_->device, size, VK_BUFFER_USAGE_TRANSFER_DST_BIT, VulkanBufferMemory::readback};
        transition_rgba(
            batch->commands,
            texture.image(),
            0,
            texture.mip_levels(),
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL
        );
        vkCmdCopyImageToBuffer(
            batch->commands,
            texture.image(),
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            batch->transfer.handle(),
            static_cast<uint32_t>(copies.size()),
            copies.data()
        );
        transition_rgba(
            batch->commands,
            texture.image(),
            0,
            texture.mip_levels(),
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
        );
        transfer_to_host(batch->commands);
        auto* submitted = batch.get();
        const auto completion = state_->submit(std::move(batch));
        state_->wait(completion);
        submitted->transfer.read(bytes);
        collect();
        return bytes;
    }
} // namespace RelayCore::render
