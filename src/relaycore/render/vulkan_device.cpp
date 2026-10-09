#include "relaycore/render/vulkan_device.h"

#include "relaycore/core/log.h"
#include "relaycore/core/sdl_error.h"
#include "relaycore/platform/window.h"

#include <SDL3/SDL_vulkan.h>
#include <VkBootstrap.h>
#include <algorithm>
#include <cstdio>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace RelayCore::render
{
    namespace
    {
        template<typename T>
        auto require_bootstrap(vkb::Result<T> result, const std::string_view operation) -> T
        {
            if (!result)
            {
                std::string message = std::string(operation) + ": " + result.error().message() + " (VkResult "
                                      + std::to_string(result.vk_result()) + ")";
                for (const auto& reason : result.detailed_failure_reasons())
                    message += "\n" + reason;
                throw std::runtime_error(message);
            }
            return std::move(result).value();
        }

#if RELAYCORE_VULKAN_VALIDATION
        auto VKAPI_CALL
        validation_callback(const VkDebugUtilsMessageSeverityFlagBitsEXT severity, VkDebugUtilsMessageTypeFlagsEXT, const VkDebugUtilsMessengerCallbackDataEXT* data, void*) noexcept
            -> VkBool32
        {
            // Startup and teardown diagnostics must also work before/after the logger lifetime.
            std::fprintf(
                stderr,
                "Vulkan validation %s [%s]: %s\n",
                severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT ? "error" : "warning",
                data->pMessageIdName != nullptr ? data->pMessageIdName : "unnamed",
                data->pMessage
            );
            return VK_FALSE;
        }
#endif
    } // namespace

    struct VulkanDevice::State {
        vkb::Instance instance{};
        VkSurfaceKHR surface = VK_NULL_HANDLE;
        vkb::Device device{};
        VkQueue graphics_queue = VK_NULL_HANDLE;
        VkQueue present_queue = VK_NULL_HANDLE;
        VmaAllocator allocator = nullptr;
        VulkanDeviceCapabilities capabilities{};

        ~State() noexcept
        {
            if (device.device != VK_NULL_HANDLE)
            {
                const auto result = vkDeviceWaitIdle(device.device);
                if (result != VK_SUCCESS)
                    std::fprintf(stderr, "Vulkan device shutdown failed: VkResult %d\n", static_cast<int>(result));
            }
            if (allocator != nullptr)
                vmaDestroyAllocator(allocator);
            if (device.device != VK_NULL_HANDLE)
                vkb::destroy_device(device);
            if (surface != VK_NULL_HANDLE)
                SDL_Vulkan_DestroySurface(instance.instance, surface, nullptr);
            if (instance.instance != VK_NULL_HANDLE)
                vkb::destroy_instance(instance);
        }
    };

    VulkanDevice::VulkanDevice(const Window& window) : state_(std::make_unique<State>())
    {
        const auto extensions = window.vulkan_instance_extensions();
        const auto system = require_bootstrap(
            vkb::SystemInfo::get_system_info(vkGetInstanceProcAddr),
            "Failed to query the Vulkan loader"
        );
        if (system.instance_api_version < kVulkanApiVersion)
            throw std::runtime_error(
                "RelayCore requires Vulkan loader API 1.3 or newer; update the Vulkan driver/loader"
            );

#if RELAYCORE_VULKAN_VALIDATION
        if (!system.is_layer_available("VK_LAYER_KHRONOS_validation")
            || !system.is_extension_available(VK_EXT_DEBUG_UTILS_EXTENSION_NAME))
            throw std::runtime_error(
                "Vulkan development builds require VK_LAYER_KHRONOS_validation and VK_EXT_debug_utils; "
                "install the Vulkan SDK validation layers or disable RELAYCORE_ENABLE_VULKAN_VALIDATION"
            );
#endif

        vkb::InstanceBuilder builder{vkGetInstanceProcAddr};
        for (const auto extension :
             {VK_KHR_GET_SURFACE_CAPABILITIES_2_EXTENSION_NAME, VK_KHR_SURFACE_MAINTENANCE_1_EXTENSION_NAME})
        {
            if (!system.is_extension_available(extension))
                throw std::runtime_error(
                    std::string("Vulkan presentation requires ") + extension + "; update the Vulkan driver/loader"
                );
            builder.enable_extension(extension);
        }
        builder.set_app_name("RelayCore")
            .set_engine_name("RelayCore")
            .require_api_version(kVulkanApiVersion)
            .enable_extensions(extensions.size(), extensions.data());
#if RELAYCORE_VULKAN_VALIDATION
        builder.enable_validation_layers()
            .add_validation_feature_enable(VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT)
            .enable_extension(VK_EXT_DEBUG_UTILS_EXTENSION_NAME)
            .set_debug_callback(validation_callback)
            .set_debug_messenger_severity(
                VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT
            );
#endif
        state_->instance = require_bootstrap(builder.build(), "Failed to create Vulkan 1.3 instance");
        core::require_sdl(
            SDL_Vulkan_CreateSurface(window.handle(), state_->instance.instance, nullptr, &state_->surface),
            "Failed to create SDL Vulkan surface"
        );

        VkPhysicalDeviceVulkan12Features features12{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES};
        features12.timelineSemaphore = VK_TRUE;
#if defined(TRACY_ENABLE)
        features12.hostQueryReset = VK_TRUE;
#endif
        VkPhysicalDeviceVulkan13Features features13{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES};
        features13.dynamicRendering = VK_TRUE;
        features13.synchronization2 = VK_TRUE;
        VkPhysicalDeviceSwapchainMaintenance1FeaturesKHR maintenance{
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SWAPCHAIN_MAINTENANCE_1_FEATURES_KHR,
            .swapchainMaintenance1 = VK_TRUE,
        };
        vkb::PhysicalDeviceSelector selector{state_->instance, state_->surface};
        auto physical = require_bootstrap(
            selector.set_minimum_version(1, 3)
                .add_required_extension(VK_KHR_SWAPCHAIN_EXTENSION_NAME)
                .add_required_extension(VK_KHR_SWAPCHAIN_MAINTENANCE_1_EXTENSION_NAME)
                .add_required_extension_features(maintenance)
                .set_required_features_12(features12)
                .set_required_features_13(features13)
                .select(),
            "No suitable Vulkan GPU: require API 1.3, dynamicRendering, synchronization2, timelineSemaphore, "
            "VK_KHR_swapchain, VK_KHR_swapchain_maintenance1 (swapchainMaintenance1) and graphics/present "
            "queues for the SDL surface"
        );
        const bool anisotropy = physical.enable_features_if_present({.samplerAnisotropy = VK_TRUE});
        state_->device = require_bootstrap(vkb::DeviceBuilder{physical}.build(), "Failed to create Vulkan device");
        const auto graphics = require_bootstrap(
            state_->device.get_queue_and_index(vkb::QueueType::graphics),
            "Vulkan device has no graphics queue"
        );
        const auto present = require_bootstrap(
            state_->device.get_queue_and_index(vkb::QueueType::present),
            "Vulkan device has no surface presentation queue"
        );
        state_->graphics_queue = graphics.first;
        state_->present_queue = present.first;

        const VmaVulkanFunctions functions{
            .vkGetInstanceProcAddr = vkGetInstanceProcAddr,
            .vkGetDeviceProcAddr = state_->device.fp_vkGetDeviceProcAddr,
        };
        const VmaAllocatorCreateInfo allocator_info{
            .physicalDevice = physical.physical_device,
            .device = state_->device.device,
            .pVulkanFunctions = &functions,
            .instance = state_->instance.instance,
            .vulkanApiVersion = kVulkanApiVersion,
        };
        const auto allocation_result = vmaCreateAllocator(&allocator_info, &state_->allocator);
        if (allocation_result != VK_SUCCESS)
            throw std::runtime_error(
                "Failed to create Vulkan memory allocator: VkResult " + std::to_string(allocation_result)
            );

        state_->capabilities = {
            .device_name = physical.name,
            .physical_api_version = physical.properties.apiVersion,
            .driver_version = physical.properties.driverVersion,
            .graphics_queue_family = graphics.second,
            .present_queue_family = present.second,
            .validation_enabled = state_->instance.debug_messenger != VK_NULL_HANDLE,
            .max_texture_anisotropy =
                anisotropy ? std::min(16.0F, physical.properties.limits.maxSamplerAnisotropy) : 1.0F,
        };
        log::info(
            "Vulkan device created: {} | GPU API {}.{}.{} | required API 1.3 | graphics family={} | present family={} "
            "| "
            "dynamicRendering/synchronization2/timelineSemaphore/swapchainMaintenance1 enabled | validation={} "
            "| texture anisotropy={}",
            physical.name,
            VK_API_VERSION_MAJOR(physical.properties.apiVersion),
            VK_API_VERSION_MINOR(physical.properties.apiVersion),
            VK_API_VERSION_PATCH(physical.properties.apiVersion),
            graphics.second,
            present.second,
            state_->capabilities.validation_enabled,
            state_->capabilities.max_texture_anisotropy
        );
    }

    VulkanDevice::~VulkanDevice() noexcept = default;
    VulkanDevice::VulkanDevice(VulkanDevice&&) noexcept = default;
    auto VulkanDevice::operator=(VulkanDevice&&) noexcept -> VulkanDevice& = default;

    auto VulkanDevice::capabilities() const noexcept -> const VulkanDeviceCapabilities&
    {
        return state_->capabilities;
    }

    auto VulkanDevice::instance() const noexcept -> VkInstance
    {
        return state_->instance.instance;
    }
    auto VulkanDevice::physical_device() const noexcept -> VkPhysicalDevice
    {
        return state_->device.physical_device.physical_device;
    }
    auto VulkanDevice::handle() const noexcept -> VkDevice
    {
        return state_->device.device;
    }
    auto VulkanDevice::surface() const noexcept -> VkSurfaceKHR
    {
        return state_->surface;
    }
    auto VulkanDevice::graphics_queue() const noexcept -> VkQueue
    {
        return state_->graphics_queue;
    }
    auto VulkanDevice::present_queue() const noexcept -> VkQueue
    {
        return state_->present_queue;
    }
    auto VulkanDevice::allocator() const noexcept -> VmaAllocator
    {
        return state_->allocator;
    }
} // namespace RelayCore::render
