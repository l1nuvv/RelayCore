#include "relaycore/render/vulkan_swapchain.h"

#include "relaycore/core/profiling.h"
#include "relaycore/platform/window.h"
#include "relaycore/render/vulkan_device.h"
#include "relaycore/render/vulkan_gpu_profiler.h"
#include "relaycore/render/vulkan_mesh_renderer.h"

#include <VkBootstrap.h>
#include <cstdio>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace RelayCore::render
{
    namespace
    {
        void require_vk(const VkResult result, const char* operation)
        {
            if (result != VK_SUCCESS)
                throw std::runtime_error(std::string(operation) + ": VkResult " + std::to_string(result));
        }

        template<typename T>
        auto require_swapchain_result(vkb::Result<T> result, const char* operation) -> T
        {
            if (!result)
                throw std::runtime_error(
                    std::string(operation) + ": " + result.error().message() + " (VkResult "
                    + std::to_string(result.vk_result()) + ")"
                );
            return std::move(result).value();
        }

        struct SwapchainImages {
            vkb::Swapchain swapchain{};
            std::vector<VkImage> images;
            std::vector<VkImageView> views;

            ~SwapchainImages() noexcept
            {
                for (const auto view : views)
                    if (view != VK_NULL_HANDLE)
                        vkDestroyImageView(swapchain.device, view, nullptr);
                if (swapchain.swapchain != VK_NULL_HANDLE)
                    vkb::destroy_swapchain(swapchain);
            }
        };

        struct Frame {
            VkCommandPool pool = VK_NULL_HANDLE;
            VkCommandBuffer commands = VK_NULL_HANDLE;
            VkSemaphore acquired = VK_NULL_HANDLE;
            VkSemaphore rendered = VK_NULL_HANDLE;
            VkFence acquire_fence = VK_NULL_HANDLE;
            VkFence render_fence = VK_NULL_HANDLE;
            VkFence present_fence = VK_NULL_HANDLE;
            bool acquire_pending = false;
            bool render_pending = false;
            bool present_pending = false;
        };
    } // namespace

    struct VulkanSwapchain::State {
        const VulkanDevice& device;
        VulkanGpuProfiler profiler;
        const Window& window;
        bool vsync;
        VkAllocationCallbacks* host_allocator;
        std::unique_ptr<SwapchainImages> active;
        std::array<Frame, 2> frames{};
        size_t frame_index = 0;
        PixelExtent requested_extent{};
        bool needs_recreation = true;
        bool stopped = false;
        bool reset_frames = false;
        PFN_vkCmdBeginDebugUtilsLabelEXT begin_label = nullptr;
        PFN_vkCmdEndDebugUtilsLabelEXT end_label = nullptr;

        State(
            const VulkanDevice& device_in,
            const Window& window_in,
            const bool vsync_in,
            VkAllocationCallbacks* allocator_in
        ) :
            device(device_in), profiler(device_in, "RelayCore Vulkan presentation"), window(window_in), vsync(vsync_in),
            host_allocator(allocator_in)
        {}

        void wait_fence(const VkFence fence, bool& pending)
        {
            if (!pending)
                return;
            require_vk(
                vkWaitForFences(device.handle(), 1, &fence, VK_TRUE, std::numeric_limits<uint64_t>::max()),
                "Failed to wait for Vulkan frame"
            );
            pending = false;
        }

        void wait_frame(Frame& frame)
        {
            RELAYCORE_PROFILE_ZONE_N("Vulkan frame fences");
            {
                RELAYCORE_PROFILE_ZONE_N("Vulkan acquire fence wait");
                wait_fence(frame.acquire_fence, frame.acquire_pending);
            }
            {
                RELAYCORE_PROFILE_ZONE_N("Vulkan presentation copy fence wait");
                wait_fence(frame.render_fence, frame.render_pending);
            }
            // A submission fence alone does not protect presentation's semaphore references.
            {
                RELAYCORE_PROFILE_ZONE_N("Vulkan present fence wait");
                wait_fence(frame.present_fence, frame.present_pending);
            }
            profiler.collect();
        }

        void drain()
        {
            RELAYCORE_PROFILE_ZONE_N("Vulkan swapchain drain");
            for (auto& frame : frames)
                wait_frame(frame);
        }

        void destroy_frames() noexcept
        {
            for (auto& frame : frames)
            {
                if (frame.pool != VK_NULL_HANDLE)
                    vkDestroyCommandPool(device.handle(), frame.pool, nullptr);
                if (frame.acquired != VK_NULL_HANDLE)
                    vkDestroySemaphore(device.handle(), frame.acquired, nullptr);
                if (frame.rendered != VK_NULL_HANDLE)
                    vkDestroySemaphore(device.handle(), frame.rendered, nullptr);
                for (const auto fence : {frame.acquire_fence, frame.render_fence, frame.present_fence})
                    if (fence != VK_NULL_HANDLE)
                        vkDestroyFence(device.handle(), fence, nullptr);
                frame = {};
            }
        }

        void create_frames()
        {
            for (auto& frame : frames)
            {
                const VkCommandPoolCreateInfo pool_info{
                    .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
                    .queueFamilyIndex = device.capabilities().graphics_queue_family,
                };
                require_vk(
                    vkCreateCommandPool(device.handle(), &pool_info, nullptr, &frame.pool),
                    "Failed to create Vulkan frame command pool"
                );
                const VkCommandBufferAllocateInfo commands_info{
                    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
                    .commandPool = frame.pool,
                    .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
                    .commandBufferCount = 1,
                };
                require_vk(
                    vkAllocateCommandBuffers(device.handle(), &commands_info, &frame.commands),
                    "Failed to allocate Vulkan frame commands"
                );
                const VkSemaphoreCreateInfo semaphore_info{.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
                require_vk(
                    vkCreateSemaphore(device.handle(), &semaphore_info, nullptr, &frame.acquired),
                    "Failed to create Vulkan acquire semaphore"
                );
                require_vk(
                    vkCreateSemaphore(device.handle(), &semaphore_info, nullptr, &frame.rendered),
                    "Failed to create Vulkan render semaphore"
                );
                const VkFenceCreateInfo fence_info{.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
                for (auto* fence : {&frame.acquire_fence, &frame.render_fence, &frame.present_fence})
                    require_vk(
                        vkCreateFence(device.handle(), &fence_info, nullptr, fence),
                        "Failed to create Vulkan frame fence"
                    );
            }
        }

        ~State() noexcept
        {
            try
            {
                drain();
            } catch (const std::exception& error)
            {
                std::fprintf(stderr, "Vulkan presentation shutdown failed: %s\n", error.what());
            }
            active.reset();
            destroy_frames();
        }

        void recreate()
        {
            RELAYCORE_PROFILE_ZONE_N("Vulkan swapchain recreation");
            if (!can_present(window.presentation_state()))
            {
                needs_recreation = true;
                return;
            }
            stopped = true;
            drain();
            if (reset_frames)
            {
                destroy_frames();
                create_frames();
                reset_frames = false;
            }
            const auto pixels = window.pixel_extent();
            auto replacement = std::make_unique<SwapchainImages>();
            auto previous = std::move(active);
            vkb::SwapchainBuilder builder{
                device.physical_device(),
                device.handle(),
                device.surface(),
                device.capabilities().graphics_queue_family,
                device.capabilities().present_queue_family
            };
            // Even failed creation retires oldSwapchain. Dropping this owner cannot roll that back.
            replacement->swapchain = require_swapchain_result(
                builder.set_old_swapchain(previous ? previous->swapchain.swapchain : VK_NULL_HANDLE)
                    .set_desired_extent(static_cast<uint32_t>(pixels.width), static_cast<uint32_t>(pixels.height))
                    .set_desired_format({VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR})
                    .add_fallback_format({VK_FORMAT_R8G8B8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR})
                    .set_desired_present_mode(vsync ? VK_PRESENT_MODE_FIFO_KHR : VK_PRESENT_MODE_IMMEDIATE_KHR)
                    .add_fallback_present_mode(VK_PRESENT_MODE_FIFO_KHR)
                    .set_desired_min_image_count(3)
                    .set_image_usage_flags(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT)
                    .set_allocation_callbacks(host_allocator)
                    .build(),
                "Failed to recreate Vulkan swapchain; presentation stopped, call recreate to retry"
            );
            auto& swapchain = replacement->swapchain;
            if ((swapchain.image_format != VK_FORMAT_B8G8R8A8_UNORM
                 && swapchain.image_format != VK_FORMAT_R8G8B8A8_UNORM)
                || swapchain.color_space != VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
                throw std::runtime_error("Vulkan presentation requires an SDR BGRA8/RGBA8 UNORM surface");
            VkFormatProperties format_properties{};
            vkGetPhysicalDeviceFormatProperties(device.physical_device(), swapchain.image_format, &format_properties);
            if (!(format_properties.optimalTilingFeatures & VK_FORMAT_FEATURE_BLIT_DST_BIT))
                throw std::runtime_error("Vulkan mesh presentation requires swapchain format blit-destination support");
            replacement->images = require_swapchain_result(swapchain.get_images(), "Failed to query swapchain images");
            replacement->views.resize(replacement->images.size(), VK_NULL_HANDLE);
            for (size_t i = 0; i < replacement->images.size(); ++i)
            {
                const VkImageViewCreateInfo view_info{
                    .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                    .image = replacement->images[i],
                    .viewType = VK_IMAGE_VIEW_TYPE_2D,
                    .format = swapchain.image_format,
                    .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1},
                };
                require_vk(
                    vkCreateImageView(device.handle(), &view_info, nullptr, &replacement->views[i]),
                    "Failed to create Vulkan swapchain image view; presentation stopped"
                );
            }
            active = std::move(replacement);
            requested_extent = pixels;
            needs_recreation = false;
            stopped = false;
        }

        void record(
            Frame& frame,
            const uint32_t image_index,
            const std::array<float, 4>& color,
            const VkImage scene_image,
            const VkExtent2D source_extent
        )
        {
            RELAYCORE_PROFILE_ZONE_N("Vulkan record clear");
            require_vk(vkResetCommandPool(device.handle(), frame.pool, 0), "Failed to reset Vulkan frame commands");
            const VkCommandBufferBeginInfo begin_info{
                .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
                .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
            };
            require_vk(vkBeginCommandBuffer(frame.commands, &begin_info), "Failed to begin Vulkan frame commands");
            {
                RELAYCORE_PROFILE_VK_ZONE(profiler, frame.commands, "Presentation copy/clear");
                if (begin_label != nullptr)
                {
                    const VkDebugUtilsLabelEXT label{
                        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT,
                        .pLabelName = "RelayCore swapchain clear",
                        .color = {0.2F, 0.6F, 0.9F, 1.0F},
                    };
                    begin_label(frame.commands, &label);
                }
                const auto stage = scene_image ? VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT
                                               : VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
                const auto access =
                    scene_image ? VK_ACCESS_2_TRANSFER_WRITE_BIT : VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
                const auto image_layout =
                    scene_image ? VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL : VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
                VkImageMemoryBarrier2 barrier{
                    .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
                    // Chain the acquire semaphore wait into the layout transition, even when discarding contents.
                    .srcStageMask = stage,
                    .dstStageMask = stage,
                    .dstAccessMask = access,
                    .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                    .newLayout = image_layout,
                    .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                    .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                    .image = active->images[image_index],
                    .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1},
                };
                const VkDependencyInfo dependency{
                    .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
                    .imageMemoryBarrierCount = 1,
                    .pImageMemoryBarriers = &barrier,
                };
                vkCmdPipelineBarrier2(frame.commands, &dependency);
                const VkRenderingAttachmentInfo attachment{
                    .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
                    .imageView = active->views[image_index],
                    .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                    .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
                    .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
                    .clearValue = {.color = {{color[0], color[1], color[2], color[3]}}},
                };
                const VkRenderingInfo rendering{
                    .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
                    .renderArea = {{0, 0}, active->swapchain.extent},
                    .layerCount = 1,
                    .colorAttachmentCount = 1,
                    .pColorAttachments = &attachment,
                };
                if (scene_image)
                {
                    const auto size = active->swapchain.extent;
                    const VkImageBlit2 region{
                        .sType = VK_STRUCTURE_TYPE_IMAGE_BLIT_2,
                        .srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1},
                        .srcOffsets =
                            {{0, 0, 0},
                             {static_cast<int32_t>(source_extent.width), static_cast<int32_t>(source_extent.height), 1}
                            },
                        .dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1},
                        .dstOffsets =
                            {{0, 0, 0}, {static_cast<int32_t>(size.width), static_cast<int32_t>(size.height), 1}}
                    };
                    const VkBlitImageInfo2 blit{
                        .sType = VK_STRUCTURE_TYPE_BLIT_IMAGE_INFO_2,
                        .srcImage = scene_image,
                        .srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                        .dstImage = active->images[image_index],
                        .dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                        .regionCount = 1,
                        .pRegions = &region,
                        .filter = VK_FILTER_NEAREST
                    };
                    vkCmdBlitImage2(frame.commands, &blit);
                } else
                {
                    vkCmdBeginRendering(frame.commands, &rendering);
                    vkCmdEndRendering(frame.commands);
                }
                barrier.srcStageMask = stage;
                barrier.srcAccessMask = access;
                barrier.dstStageMask = VK_PIPELINE_STAGE_2_NONE;
                barrier.dstAccessMask = VK_ACCESS_2_NONE;
                barrier.oldLayout = image_layout;
                barrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
                vkCmdPipelineBarrier2(frame.commands, &dependency);
                if (end_label != nullptr)
                    end_label(frame.commands);
            }
            require_vk(vkEndCommandBuffer(frame.commands), "Failed to end Vulkan frame commands");
        }

        auto draw_clear(
            const std::array<float, 4>& color,
            VulkanMeshRenderer* renderer = nullptr,
            const std::span<const RenderItem> items = {},
            const PerspectiveCamera* camera = nullptr,
            const std::span<const RenderItem> viewmodel = {},
            ImDrawData* ui = nullptr,
            bool prepared = false
        ) -> bool
        {
            RELAYCORE_PROFILE_ZONE_N("Vulkan frame");
            if (stopped)
                throw std::runtime_error("Vulkan presentation stopped after failure; call recreate to retry");
            if (!can_present(window.presentation_state()))
                return false;
            const auto pixels = window.pixel_extent();
            if (needs_recreation || pixels.width != requested_extent.width || pixels.height != requested_extent.height)
                recreate();
            if (!active)
                return false;
            const auto scene_image = renderer ? (prepared ? renderer->prepared_image(device.handle())
                                                          : renderer->render_color_image(
                                                                device.handle(),
                                                                items,
                                                                *camera,
                                                                active->swapchain.extent,
                                                                viewmodel,
                                                                ui
                                                            ))
                                              : VK_NULL_HANDLE;
            const auto source_extent = renderer ? renderer->prepared_extent() : VkExtent2D{};
            auto& frame = frames[frame_index];
            wait_frame(frame);
            require_vk(vkResetFences(device.handle(), 1, &frame.acquire_fence), "Failed to reset Vulkan acquire fence");
            uint32_t image_index = 0;
            VkResult acquisition;
            {
                RELAYCORE_PROFILE_ZONE_N("Vulkan acquire");
                acquisition = vkAcquireNextImageKHR(
                    device.handle(),
                    active->swapchain.swapchain,
                    std::numeric_limits<uint64_t>::max(),
                    frame.acquired,
                    frame.acquire_fence,
                    &image_index
                );
            }
            if (acquisition == VK_ERROR_OUT_OF_DATE_KHR)
            {
                needs_recreation = true;
                return false;
            }
            if (acquisition != VK_SUCCESS && acquisition != VK_SUBOPTIMAL_KHR)
                require_vk(acquisition, "Failed to acquire Vulkan swapchain image");
            frame.acquire_pending = true;
            record(frame, image_index, color, scene_image, source_extent);
            require_vk(vkResetFences(device.handle(), 1, &frame.render_fence), "Failed to reset Vulkan render fence");
            const VkSemaphoreSubmitInfo wait{
                .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
                .semaphore = frame.acquired,
                .stageMask = scene_image ? VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT
                                         : VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
            };
            const VkSemaphoreSubmitInfo signal{
                .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
                .semaphore = frame.rendered,
                .stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
            };
            const VkCommandBufferSubmitInfo commands{
                .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
                .commandBuffer = frame.commands,
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
            {
                RELAYCORE_PROFILE_ZONE_N("Vulkan submit");
                require_vk(
                    vkQueueSubmit2(device.graphics_queue(), 1, &submission, frame.render_fence),
                    "Failed to submit Vulkan frame"
                );
            }
            frame.render_pending = true;
            require_vk(vkResetFences(device.handle(), 1, &frame.present_fence), "Failed to reset Vulkan present fence");
            const VkSwapchainPresentFenceInfoKHR present_fence{
                .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_PRESENT_FENCE_INFO_KHR,
                .swapchainCount = 1,
                .pFences = &frame.present_fence,
            };
            const VkPresentInfoKHR present_info{
                .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
                .pNext = &present_fence,
                .waitSemaphoreCount = 1,
                .pWaitSemaphores = &frame.rendered,
                .swapchainCount = 1,
                .pSwapchains = &active->swapchain.swapchain,
                .pImageIndices = &image_index,
            };
            VkResult presentation;
            {
                RELAYCORE_PROFILE_ZONE_N("Vulkan present");
                presentation = vkQueuePresentKHR(device.present_queue(), &present_info);
            }
            frame.present_pending = presentation == VK_SUCCESS || presentation == VK_SUBOPTIMAL_KHR
                                    || presentation == VK_ERROR_OUT_OF_DATE_KHR
                                    || presentation == VK_ERROR_SURFACE_LOST_KHR;
            if (presentation != VK_SUCCESS && presentation != VK_SUBOPTIMAL_KHR
                && presentation != VK_ERROR_OUT_OF_DATE_KHR)
                require_vk(presentation, "Failed to present Vulkan frame");
            needs_recreation = acquisition == VK_SUBOPTIMAL_KHR || presentation != VK_SUCCESS;
            // Return ownership of the source image only after the GPU blit. Presentation owns only swapchain images.
            if (scene_image)
            {
                RELAYCORE_PROFILE_ZONE_N("Vulkan presentation source image wait");
                wait_fence(frame.render_fence, frame.render_pending);
            }
            frame_index = (frame_index + 1) % frames.size();
            return presentation != VK_ERROR_OUT_OF_DATE_KHR;
        }
    };

    VulkanSwapchain::VulkanSwapchain(
        const VulkanDevice& device,
        const Window& window,
        const bool vsync,
        VkAllocationCallbacks* host_allocator
    ) : state_(std::make_unique<State>(device, window, vsync, host_allocator))
    {
        if (device.capabilities().validation_enabled)
        {
            state_->begin_label = reinterpret_cast<PFN_vkCmdBeginDebugUtilsLabelEXT>(
                vkGetDeviceProcAddr(device.handle(), "vkCmdBeginDebugUtilsLabelEXT")
            );
            state_->end_label = reinterpret_cast<PFN_vkCmdEndDebugUtilsLabelEXT>(
                vkGetDeviceProcAddr(device.handle(), "vkCmdEndDebugUtilsLabelEXT")
            );
            if (!state_->begin_label || !state_->end_label)
                throw std::runtime_error("Vulkan debug labels unavailable after enabling VK_EXT_debug_utils");
        }
        state_->create_frames();
        state_->recreate();
    }

    VulkanSwapchain::~VulkanSwapchain() noexcept = default;

    auto VulkanSwapchain::draw_clear(const std::array<float, 4>& color) -> bool
    {
        try
        {
            return state_->draw_clear(color);
        } catch (...)
        {
            state_->stopped = true;
            state_->reset_frames = true;
            throw;
        }
    }

    void VulkanSwapchain::recreate()
    {
        state_->recreate();
    }

    auto VulkanSwapchain::draw_scene(
        VulkanMeshRenderer& renderer,
        const std::span<const RenderItem> items,
        const PerspectiveCamera& camera,
        const std::span<const RenderItem> viewmodel,
        ImDrawData* ui
    ) -> bool
    {
        try
        {
            return state_->draw_clear({0, 0, 0, 1}, &renderer, items, &camera, viewmodel, ui);
        } catch (...)
        {
            state_->stopped = true;
            state_->reset_frames = true;
            // A failed present can follow a successful submit referencing the renderer's source image.
            state_->drain();
            throw;
        }
    }

    auto VulkanSwapchain::present_prepared_scene(VulkanMeshRenderer& renderer) -> bool
    {
        try
        {
            return state_->draw_clear({0, 0, 0, 1}, &renderer, {}, nullptr, {}, nullptr, true);
        } catch (...)
        {
            state_->stopped = true;
            state_->reset_frames = true;
            state_->drain();
            throw;
        }
    }

    void VulkanSwapchain::set_vsync(bool enabled)
    {
        if (state_->vsync != enabled)
        {
            state_->vsync = enabled;
            state_->needs_recreation = true;
        }
    }

    auto VulkanSwapchain::extent() const noexcept -> VkExtent2D
    {
        return state_->active ? state_->active->swapchain.extent : VkExtent2D{};
    }
} // namespace RelayCore::render
