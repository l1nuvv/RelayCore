#pragma once

#include "relaycore/render/texture2d.h"

#include <cstddef>
#include <memory>
#include <span>
#include <vk_mem_alloc.h>
#include <vulkan/vulkan.h>

namespace RelayCore::render
{
    class VulkanDevice;
    class VulkanUploads;
    class VulkanSceneSubmissions;

    enum class VulkanBufferMemory {
        device,
        upload,
        readback
    };

    class VulkanBuffer {
    public:
        VulkanBuffer() = default;
        // The device/allocator must outlive this owner. Wait or retire before releasing GPU references.
        VulkanBuffer(
            const VulkanDevice& device,
            VkDeviceSize size,
            VkBufferUsageFlags usage,
            VulkanBufferMemory memory
        );
        ~VulkanBuffer() noexcept;
        VulkanBuffer(const VulkanBuffer&) = delete;
        auto operator=(const VulkanBuffer&) -> VulkanBuffer& = delete;
        VulkanBuffer(VulkanBuffer&& other) noexcept;
        auto operator=(VulkanBuffer&& other) noexcept -> VulkanBuffer&;

        void write(std::span<const std::byte> bytes, VkDeviceSize offset = 0);
        // The caller must wait for GPU writes before reading; VMA invalidates non-coherent memory.
        void read(std::span<std::byte> bytes, VkDeviceSize offset = 0) const;
        [[nodiscard]] auto handle() const noexcept -> VkBuffer
        {
            return buffer_;
        }
        [[nodiscard]] auto size() const noexcept -> VkDeviceSize
        {
            return size_;
        }

    private:
        friend class VulkanUploads;
        friend class VulkanSceneSubmissions;
        void reset() noexcept;
        VmaAllocator allocator_ = nullptr;
        VkBuffer buffer_ = VK_NULL_HANDLE;
        VmaAllocation allocation_ = nullptr;
        VkDeviceSize size_ = 0;
        VkBufferUsageFlags usage_ = 0;
        VkSemaphore upload_timeline_ = VK_NULL_HANDLE;
        uint64_t upload_completion_ = 0;
        VulkanBufferMemory memory_ = VulkanBufferMemory::device;
    };

    class VulkanTexture {
    public:
        // Wait or retire before releasing/replacing a texture referenced by GPU work.
        VulkanTexture() = default;
        ~VulkanTexture() noexcept;
        VulkanTexture(const VulkanTexture&) = delete;
        auto operator=(const VulkanTexture&) -> VulkanTexture& = delete;
        VulkanTexture(VulkanTexture&&) noexcept;
        auto operator=(VulkanTexture&&) noexcept -> VulkanTexture&;

        [[nodiscard]] auto image() const noexcept -> VkImage;
        [[nodiscard]] auto view() const noexcept -> VkImageView;
        [[nodiscard]] auto sampler() const noexcept -> VkSampler;
        [[nodiscard]] auto extent() const noexcept -> VkExtent2D;
        [[nodiscard]] auto mip_levels() const noexcept -> uint32_t;

    private:
        friend class VulkanUploads;
        friend class VulkanSceneSubmissions;
        VulkanTexture(const VulkanDevice& device, int width, int height, TextureEncoding encoding, bool cube = false);
        struct State {
            explicit State(const VulkanDevice& owner);
            ~State() noexcept;
            VkDevice device;
            VmaAllocator allocator;
            VkImage image = VK_NULL_HANDLE;
            VmaAllocation allocation = nullptr;
            VkImageView view = VK_NULL_HANDLE;
            VkSampler sampler = VK_NULL_HANDLE;
            VkExtent2D extent{};
            uint32_t mip_levels = 0;
            bool cube = false;
            VkSemaphore upload_timeline = VK_NULL_HANDLE;
            uint64_t upload_completion = 0;
        };
        std::unique_ptr<State> state_;
    };
} // namespace RelayCore::render
