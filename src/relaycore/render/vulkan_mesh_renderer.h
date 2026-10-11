#pragma once

#include "relaycore/render/color_pipeline_mode.h"
#include "relaycore/render/render_queue.h"
#include "relaycore/render/texture2d.h"

#include <filesystem>
#include <memory>
#include <vulkan/vulkan.h>

struct ImDrawData;

namespace RelayCore::render
{
    class VulkanDevice;
    class PlayerUi;
    class PerspectiveCamera;
    struct RenderAuthoringConfig;
    struct RendererConfig;
    struct IrradianceProbeVolume;
    struct ReflectionProbe;

    struct VulkanMaterialDescriptorUsage {
        size_t pools = 0;
        size_t materials = 0;
    };

    struct VulkanDepthFormats {
        VkFormat scene = VK_FORMAT_UNDEFINED;
        VkFormat shadow = VK_FORMAT_UNDEFINED;
    };

    struct VulkanResourceUsage {
        uint64_t staging_bytes = 0;
        size_t upload_batches = 0, upload_retirements = 0;
        size_t scene_retirements = 0, material_retirements = 0;
    };

    class VulkanMeshRenderer {
    public:
        // Device outlives this owner; render-thread access to its graphics queue is serialized.
        VulkanMeshRenderer(
            const VulkanDevice& device,
            const std::filesystem::path& shader_directory,
            const RenderAuthoringConfig& authoring,
            const CpuRgbaTextureData& atlas,
            TextureEncoding encoding,
            const RendererConfig& config,
            ColorPipelineMode pipeline = ColorPipelineMode::legacy_gamma,
            VulkanDepthFormats depth_formats = {}
        );
        ~VulkanMeshRenderer() noexcept;
        void prepare_resources(std::span<const RenderItem> items);
        // Includes retired materials until their final GPU use completes.
        [[nodiscard]] auto material_descriptor_usage() const noexcept -> VulkanMaterialDescriptorUsage;
        [[nodiscard]] auto resource_usage() const noexcept -> VulkanResourceUsage;
        VulkanMeshRenderer(const VulkanMeshRenderer&) = delete;
        auto operator=(const VulkanMeshRenderer&) -> VulkanMeshRenderer& = delete;

        // Returns top-down RGBA8 after GPU completion. Readback and ImGui remain synchronous.
        [[nodiscard]] auto render_image(
            std::span<const RenderItem> items,
            const PerspectiveCamera& camera,
            int width,
            int height,
            std::span<const RenderItem> viewmodel = {},
            ImDrawData* ui = nullptr
        ) -> CpuRgbaTextureData;
        // Uses two scene slots; resource reuse/reload waits for their final GPU use.
        void prepare_frame(
            std::span<const RenderItem> items,
            std::span<const RenderItem> shadow_casters,
            const PerspectiveCamera& camera,
            int width,
            int height,
            std::span<const RenderItem> viewmodel = {}
        );
        void append_ui(ImDrawData* ui);
        void append_player_ui(PlayerUi& ui);
        [[nodiscard]] auto capture_prepared_image() -> CpuRgbaTextureData;
        [[nodiscard]] auto shadow_preview(size_t cascade) -> uintptr_t;
        [[nodiscard]] auto capture_shadow_depth(size_t cascade) -> std::vector<float>;
        void set_config(const RendererConfig& config);
        // Stages mesh and shadow pipelines; failed reload retains the working set.
        void reload_shaders(const std::filesystem::path& shader_directory);
        // Prepared world lights already carry the selected color-pipeline encoding; preserve their order/budget.
        void set_local_lights(std::span<const LocalLight> lights);
        void set_reflection_probe(std::shared_ptr<const ReflectionProbe> probe);
        void set_irradiance_probes(std::shared_ptr<const IrradianceProbeVolume> probes);
        void set_spot_light(const SpotLight& light) noexcept;
        void set_scene_time(float seconds) noexcept;

    private:
        friend class VulkanSwapchain;
        [[nodiscard]] auto render_color_image(
            VkDevice presenting_device,
            std::span<const RenderItem> items,
            const PerspectiveCamera& camera,
            VkExtent2D extent,
            std::span<const RenderItem> viewmodel,
            ImDrawData* ui
        ) -> VkImage;
        void submit_presentation(
            VkCommandBuffer commands,
            VkSemaphoreSubmitInfo acquired,
            VkSemaphoreSubmitInfo rendered,
            VkFence fence
        );
        [[nodiscard]] auto prepared_image(VkDevice device) const -> VkImage;
        [[nodiscard]] auto prepared_extent() const -> VkExtent2D;
        struct State;
        std::unique_ptr<State> state_;
    };
} // namespace RelayCore::render
