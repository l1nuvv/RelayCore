#pragma once

#include "relaycore/render/render_queue.h"
#include "relaycore/render/texture2d.h"

#include <filesystem>
#include <memory>
#include <vulkan/vulkan.h>

namespace RelayCore::render
{
    class VulkanDevice;
    class PerspectiveCamera;
    struct RenderAuthoringConfig;

    class VulkanMeshRenderer {
    public:
        // Isolated migration path: opaque mesh/atlas/OBJ parity, before lighting and UI migration.
        // Device outlives this owner; render-thread access to its graphics queue is serialized.
        VulkanMeshRenderer(
            const VulkanDevice& device,
            const std::filesystem::path& shader_directory,
            const RenderAuthoringConfig& authoring,
            const CpuRgbaTextureData& atlas,
            TextureEncoding encoding
        );
        ~VulkanMeshRenderer() noexcept;
        VulkanMeshRenderer(const VulkanMeshRenderer&) = delete;
        auto operator=(const VulkanMeshRenderer&) -> VulkanMeshRenderer& = delete;

        // Returns top-down RGBA8. A submission fence completes before readback, reload or resource reuse.
        [[nodiscard]] auto render_image(
            std::span<const RenderItem> items,
            const PerspectiveCamera& camera,
            int width,
            int height,
            std::span<const RenderItem> viewmodel = {}
        ) -> CpuRgbaTextureData;
        // Stages both shader modules/pipeline; failed reload retains the previous working pipeline.
        void reload_shaders(const std::filesystem::path& shader_directory);

    private:
        friend class VulkanSwapchain;
        [[nodiscard]] auto render_color_image(
            VkDevice presenting_device,
            std::span<const RenderItem> items,
            const PerspectiveCamera& camera,
            VkExtent2D extent,
            std::span<const RenderItem> viewmodel
        ) -> VkImage;
        struct State;
        std::unique_ptr<State> state_;
    };
} // namespace RelayCore::render
