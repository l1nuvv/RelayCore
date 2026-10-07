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
    class PerspectiveCamera;
    struct RenderAuthoringConfig;
    struct RendererConfig;
    struct IrradianceProbeVolume;

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
            ColorPipelineMode pipeline = ColorPipelineMode::legacy_gamma
        );
        ~VulkanMeshRenderer() noexcept;
        void prepare_resources(std::span<const RenderItem> items);
        VulkanMeshRenderer(const VulkanMeshRenderer&) = delete;
        auto operator=(const VulkanMeshRenderer&) -> VulkanMeshRenderer& = delete;

        // Returns top-down RGBA8. A submission fence completes before readback, reload or resource reuse.
        [[nodiscard]] auto render_image(
            std::span<const RenderItem> items,
            const PerspectiveCamera& camera,
            int width,
            int height,
            std::span<const RenderItem> viewmodel = {},
            ImDrawData* ui = nullptr
        ) -> CpuRgbaTextureData;
        // Scene submission completes before UI can replace game-owned mesh data or reload this backend.
        void prepare_frame(
            std::span<const RenderItem> items,
            std::span<const RenderItem> shadow_casters,
            const PerspectiveCamera& camera,
            int width,
            int height,
            std::span<const RenderItem> viewmodel = {}
        );
        void append_ui(ImDrawData* ui);
        [[nodiscard]] auto capture_prepared_image() -> CpuRgbaTextureData;
        [[nodiscard]] auto shadow_preview(size_t cascade) -> uintptr_t;
        [[nodiscard]] auto capture_shadow_depth(size_t cascade) -> std::vector<float>;
        void set_config(const RendererConfig& config);
        // Stages mesh and shadow pipelines; failed reload retains the working set.
        void reload_shaders(const std::filesystem::path& shader_directory);
        // Prepared world lights already carry the selected color-pipeline encoding; preserve their order/budget.
        void set_local_lights(std::span<const LocalLight> lights);
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
        [[nodiscard]] auto prepared_image(VkDevice device) const -> VkImage;
        [[nodiscard]] auto prepared_extent() const -> VkExtent2D;
        struct State;
        std::unique_ptr<State> state_;
    };
} // namespace RelayCore::render
