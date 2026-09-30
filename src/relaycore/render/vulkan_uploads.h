#pragma once

#include "relaycore/render/vulkan_resources.h"

#include <cstdint>
#include <vector>

namespace RelayCore::render
{
    struct VulkanBufferUpload {
        VulkanBuffer buffer;
        uint64_t completion = 0;
    };

    struct VulkanTextureUpload {
        VulkanTexture texture;
        uint64_t completion = 0;
    };

    class VulkanUploads {
    public:
        // Device and graphics queue must outlive this owner. Queue access is serialized by the render thread.
        explicit VulkanUploads(const VulkanDevice& device);
        ~VulkanUploads() noexcept;
        VulkanUploads(const VulkanUploads&) = delete;
        auto operator=(const VulkanUploads&) -> VulkanUploads& = delete;

        [[nodiscard]] auto upload_mesh_buffer(std::span<const std::byte> bytes, VkBufferUsageFlagBits kind)
            -> VulkanBufferUpload;
        [[nodiscard]] auto upload_rgba(const CpuRgbaTextureData& data, TextureEncoding encoding) -> VulkanTextureUpload;
        void wait(uint64_t completion) const;
        void collect();
        // Values belong to this upload timeline; frame/render retirement is a separate lifetime domain.
        void retire(VulkanBuffer&& buffer, uint64_t completion);
        void retire(VulkanTexture&& texture, uint64_t completion);

        [[nodiscard]] auto readback(const VulkanBuffer& buffer) -> std::vector<std::byte>;
        // Tightly packed RGBA8 bytes, ascending mip level, with no row padding; restores shader-read layout.
        [[nodiscard]] auto readback(const VulkanTexture& texture) -> std::vector<std::byte>;

    private:
        struct State;
        std::unique_ptr<State> state_;
    };
} // namespace RelayCore::render
