#pragma once

#include "relaycore/render/vulkan_resources.h"

#include <cstdint>
#include <vector>

namespace RelayCore::render
{
    struct ReflectionProbe;
    struct VulkanBufferUpload {
        VulkanBuffer buffer;
        uint64_t completion = 0;
    };

    struct VulkanTextureUpload {
        VulkanTexture texture;
        uint64_t completion = 0;
    };

    struct VulkanUploadUsage {
        uint64_t staging_bytes = 0;
        size_t batches = 0, retirements = 0;
    };

    class VulkanUploads {
    public:
        // Device and graphics queue must outlive this owner. Queue access is serialized by the render thread.
        explicit VulkanUploads(const VulkanDevice& device);
        ~VulkanUploads() noexcept;
        VulkanUploads(const VulkanUploads&) = delete;
        auto operator=(const VulkanUploads&) -> VulkanUploads& = delete;

        [[nodiscard]] auto upload_mesh_buffer(std::span<const std::byte> bytes, VkBufferUsageFlags kind)
            -> VulkanBufferUpload;
        [[nodiscard]] auto upload_rgba(const CpuRgbaTextureData& data, TextureEncoding encoding) -> VulkanTextureUpload;
        [[nodiscard]] auto upload_reflection(const ReflectionProbe& probe) -> VulkanTextureUpload;
        void wait(uint64_t completion) const;
        void collect();
        [[nodiscard]] auto dependency() const noexcept -> VkSemaphoreSubmitInfo;
        [[nodiscard]] auto completed() const -> uint64_t;
        // Retained allocations, including completed batches until collect() releases them.
        [[nodiscard]] auto memory_usage() const noexcept -> VulkanUploadUsage;
        // Values belong to this upload timeline; frame/render retirement is a separate lifetime domain.
        void retire(VulkanBuffer&& buffer, uint64_t completion);
        void retire(VulkanTexture&& texture, uint64_t completion);

        [[nodiscard]] auto readback(const VulkanBuffer& buffer) -> std::vector<std::byte>;
        // Mip-major packed RGBA8 for 2D, RGBA32F/six faces for cubes; restores shader-read layout.
        [[nodiscard]] auto readback(const VulkanTexture& texture) -> std::vector<std::byte>;

    private:
        struct State;
        std::unique_ptr<State> state_;
    };
} // namespace RelayCore::render
