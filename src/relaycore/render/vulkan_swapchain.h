#pragma once

#include <array>
#include <memory>
#include <span>
#include <vulkan/vulkan.h>

struct ImDrawData;

namespace RelayCore
{
    class Window;
}

namespace RelayCore::render
{
    class VulkanDevice;
    class VulkanMeshRenderer;
    class PerspectiveCamera;
    struct RenderItem;

    class VulkanSwapchain {
    public:
        // Device, window and optional host allocation callbacks must outlive this owner.
        VulkanSwapchain(
            const VulkanDevice& device,
            const Window& window,
            bool vsync = true,
            VkAllocationCallbacks* host_allocator = nullptr
        );
        ~VulkanSwapchain() noexcept;

        VulkanSwapchain(const VulkanSwapchain&) = delete;
        auto operator=(const VulkanSwapchain&) -> VulkanSwapchain& = delete;

        // False means presentation is suspended or the surface needs recreation on the next frame.
        [[nodiscard]] auto draw_clear(const std::array<float, 4>& color) -> bool;
        // First mesh parity path. Frame fences protect the renderer's offscreen image until the blit finishes.
        [[nodiscard]] auto draw_scene(
            VulkanMeshRenderer& renderer,
            std::span<const RenderItem> items,
            const PerspectiveCamera& camera,
            std::span<const RenderItem> viewmodel = {},
            ImDrawData* ui = nullptr
        ) -> bool;
        [[nodiscard]] auto present_prepared_scene(VulkanMeshRenderer& renderer) -> bool;
        void set_vsync(bool enabled);
        // Explicit recovery after a failed recreation; never acquires from a retired swapchain.
        void recreate();
        [[nodiscard]] auto extent() const noexcept -> VkExtent2D;

    private:
        struct State;
        std::unique_ptr<State> state_;
    };
} // namespace RelayCore::render
