#include "relaycore/render/vulkan_device.h"

#include "relaycore/core/log.h"
#include "relaycore/core/profiling.h"
#include "relaycore/core/sdl_error.h"
#include "relaycore/platform/window.h"

#include <SDL3/SDL_vulkan.h>
#include <VkBootstrap.h>
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <span>
#include <stdexcept>
#include <string_view>
#include <utility>
#include <vector>

namespace RelayCore::render
{
    namespace
    {
        constexpr uint32_t cache_schema = 1;
        constexpr size_t max_cache_bytes = 64 * 1024 * 1024;
        struct CacheHeader {
            uint32_t schema = cache_schema;
            uint32_t vendor = 0, device = 0, driver = 0;
            uint8_t uuid[VK_UUID_SIZE]{};
            uint64_t shaders = 0, bytes = 0, checksum = 0;
        };
        static_assert(sizeof(CacheHeader) == 56);

        auto hash_bytes(const std::span<const char> bytes, uint64_t hash = 14695981039346656037ULL) -> uint64_t
        {
            for (const auto byte : bytes)
                hash = (hash ^ static_cast<unsigned char>(byte)) * 1099511628211ULL;
            return hash;
        }

        auto shader_hash(const std::filesystem::path& directory) -> uint64_t
        {
            std::vector<std::filesystem::path> paths;
            for (const auto& entry : std::filesystem::directory_iterator(directory))
                if (entry.is_regular_file() && entry.path().extension() == ".spv")
                    paths.push_back(entry.path());
            std::sort(paths.begin(), paths.end());
            if (paths.empty())
                throw std::runtime_error("Pipeline cache shader directory contains no SPIR-V");
            uint64_t hash = 14695981039346656037ULL;
            for (const auto& path : paths)
            {
                hash = hash_bytes(path.filename().string(), hash);
                std::ifstream stream(path, std::ios::binary);
                std::array<char, 4096> bytes{};
                while (stream.read(bytes.data(), bytes.size()) || stream.gcount())
                    hash = hash_bytes(std::span{bytes.data(), static_cast<size_t>(stream.gcount())}, hash);
                if (!stream.eof())
                    throw std::runtime_error("Cannot read pipeline cache shader: " + path.string());
            }
            return hash;
        }

        auto cache_header(const VkPhysicalDevice physical, const std::filesystem::path& shaders) -> CacheHeader
        {
            VkPhysicalDeviceProperties properties{};
            vkGetPhysicalDeviceProperties(physical, &properties);
            CacheHeader header{};
            header.vendor = properties.vendorID;
            header.device = properties.deviceID;
            header.driver = properties.driverVersion;
            std::memcpy(header.uuid, properties.pipelineCacheUUID, VK_UUID_SIZE);
            header.shaders = shader_hash(shaders);
            return header;
        }

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
            // OBS advertises Vulkan 1.3 even when hooked into a Vulkan 1.4 application.
            if (severity == VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT && data->pMessageIdName != nullptr
                && std::strcmp(data->pMessageIdName, "Loader Message") == 0 && data->pMessage != nullptr
                && std::strcmp(
                       data->pMessage,
                       "Layer VK_LAYER_OBS_HOOK uses API version 1.3 which is older than the application specified "
                       "API version of 1.4. May cause issues."
                   ) == 0)
                return VK_FALSE;

            const auto* id = data->pMessageIdName != nullptr ? data->pMessageIdName : "unnamed";
            const bool error = (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) != 0;
            try
            {
                log::logger().log(
                    error ? spdlog::level::err : spdlog::level::warn,
                    "[renderer] Vulkan validation {} [{}]: {}",
                    error ? "error" : "warning",
                    id,
                    data->pMessage
                );
                return VK_FALSE;
            } catch (...)
            {
                // Vulkan callbacks must also report failures outside the logger lifetime.
            }
            std::fprintf(
                stderr,
                "[renderer] Vulkan validation %s [%s]: %s\n",
                error ? "error" : "warning",
                id,
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
        VkPipelineCache pipeline_cache = VK_NULL_HANDLE;
        VulkanDeviceCapabilities capabilities{};

        ~State() noexcept
        {
            if (device.device != VK_NULL_HANDLE)
            {
                const auto result = vkDeviceWaitIdle(device.device);
                if (result != VK_SUCCESS)
                    log::error_noexcept("[renderer] Vulkan device shutdown failed", static_cast<int>(result));
            }
            if (allocator != nullptr)
                vmaDestroyAllocator(allocator);
            if (pipeline_cache != VK_NULL_HANDLE)
                vkDestroyPipelineCache(device.device, pipeline_cache, nullptr);
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
                "RelayCore requires Vulkan loader API 1.4 or newer; update the Vulkan driver/loader"
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
        state_->instance = require_bootstrap(builder.build(), "Failed to create Vulkan 1.4 instance");
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
            selector.set_minimum_version(1, 4)
                .add_required_extension(VK_KHR_SWAPCHAIN_EXTENSION_NAME)
                .add_required_extension(VK_KHR_SWAPCHAIN_MAINTENANCE_1_EXTENSION_NAME)
                .add_required_extension_features(maintenance)
                .set_required_features_12(features12)
                .set_required_features_13(features13)
                .select(),
            "No suitable Vulkan GPU: require API 1.4, dynamicRendering, synchronization2, timelineSemaphore, "
            "VK_KHR_swapchain, VK_KHR_swapchain_maintenance1 (swapchainMaintenance1) and graphics/present "
            "queues for the SDL surface"
        );
        const bool anisotropy = physical.enable_features_if_present({.samplerAnisotropy = VK_TRUE});
        const bool memory_budget = physical.enable_extension_if_present(VK_EXT_MEMORY_BUDGET_EXTENSION_NAME);
        VkPhysicalDeviceVulkan12Features indexing{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES};
        indexing.runtimeDescriptorArray = VK_TRUE;
        indexing.descriptorBindingPartiallyBound = VK_TRUE;
        indexing.shaderSampledImageArrayNonUniformIndexing = VK_TRUE;
        const auto& limits = physical.properties.limits;
        const auto descriptor_limit = std::min(
            {limits.maxPerStageDescriptorSamplers,
             limits.maxPerStageDescriptorSampledImages,
             limits.maxDescriptorSetSamplers,
             limits.maxDescriptorSetSampledImages,
             limits.maxPerStageResources}
        );
        // Mesh/effect layouts and the indexed surface buffer reserve 33 fragment resources.
        const uint32_t capacity = descriptor_limit > 33 ? std::min(512U, (descriptor_limit - 33) / 5) : 0;
        const bool indexed = capacity >= 128 && !std::getenv("RELAYCORE_VULKAN_DISABLE_DESCRIPTOR_INDEXING")
                             && physical.enable_extension_features_if_present(indexing);
        VkPhysicalDeviceMeshShaderPropertiesEXT mesh_properties{
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_PROPERTIES_EXT
        };
        VkPhysicalDeviceProperties2 properties{
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
            .pNext = &mesh_properties
        };
        const auto available_extensions = physical.get_available_extensions();
        const bool mesh_extension =
            std::find(available_extensions.begin(), available_extensions.end(), VK_EXT_MESH_SHADER_EXTENSION_NAME)
            != available_extensions.end();
        if (mesh_extension)
            vkGetPhysicalDeviceProperties2(physical.physical_device, &properties);
        const VkPhysicalDeviceMeshShaderFeaturesEXT mesh_features{
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_EXT,
            .meshShader = VK_TRUE
        };
        const bool mesh =
            mesh_extension && !std::getenv("RELAYCORE_VULKAN_DISABLE_MESH_SHADERS")
            && limits.maxBoundDescriptorSets >= 5 && mesh_properties.maxMeshWorkGroupInvocations >= 32
            && mesh_properties.maxMeshWorkGroupSize[0] >= 32 && mesh_properties.maxMeshSharedMemorySize >= 4
            && mesh_properties.maxMeshOutputVertices >= 64 && mesh_properties.maxMeshOutputPrimitives >= 124
            && mesh_properties.maxMeshOutputComponents >= 28 && mesh_properties.maxMeshOutputMemorySize >= 12288
            && mesh_properties.maxMeshWorkGroupTotalCount > 0 && mesh_properties.maxMeshWorkGroupCount[0] > 0
            && mesh_properties.maxMeshWorkGroupCount[1] > 0
            && physical.enable_extension_features_if_present(mesh_features)
            && physical.enable_extension_if_present(VK_EXT_MESH_SHADER_EXTENSION_NAME);
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
        uint32_t queue_count = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(physical.physical_device, &queue_count, nullptr);
        std::vector<VkQueueFamilyProperties> queues(queue_count);
        vkGetPhysicalDeviceQueueFamilyProperties(physical.physical_device, &queue_count, queues.data());

        const VkPipelineCacheCreateInfo cache_info{.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO};
        const auto cache_result =
            vkCreatePipelineCache(state_->device.device, &cache_info, nullptr, &state_->pipeline_cache);
        if (cache_result != VK_SUCCESS)
            throw std::runtime_error(
                "Failed to create Vulkan pipeline cache: VkResult " + std::to_string(cache_result)
            );

        const VmaVulkanFunctions functions{
            .vkGetInstanceProcAddr = vkGetInstanceProcAddr,
            .vkGetDeviceProcAddr = state_->device.fp_vkGetDeviceProcAddr,
        };
        const VmaAllocatorCreateInfo allocator_info{
            .flags = memory_budget ? VMA_ALLOCATOR_CREATE_EXT_MEMORY_BUDGET_BIT : 0u,
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
            .loader_api_version = system.instance_api_version,
            .instance_api_version = state_->instance.api_version,
            .physical_api_version = physical.properties.apiVersion,
            .effective_api_version = kVulkanApiVersion,
            .driver_version = physical.properties.driverVersion,
            .graphics_queue_family = graphics.second,
            .present_queue_family = present.second,
            .graphics_compute = (queues[graphics.second].queueFlags & VK_QUEUE_COMPUTE_BIT) != 0,
            .validation_enabled = state_->instance.debug_messenger != VK_NULL_HANDLE,
            .memory_budget_enabled = memory_budget,
            .indexed_material_capacity = indexed ? capacity : 0,
            .mesh_dispatch_limit = mesh ? std::array{mesh_properties.maxMeshWorkGroupCount[0], mesh_properties.maxMeshWorkGroupCount[1], mesh_properties.maxMeshWorkGroupCount[2]} : std::array<uint32_t, 3>{},
            .mesh_dispatch_total = mesh ? mesh_properties.maxMeshWorkGroupTotalCount : 0,
            .max_texture_anisotropy =
                anisotropy ? std::min(16.0F, physical.properties.limits.maxSamplerAnisotropy) : 1.0F,
        };
        log::info(
            "[renderer] Vulkan device created: {} | loader API {}.{}.{} | instance API {}.{} | GPU API {}.{}.{} "
            "| effective API {}.{} | required API 1.4 | driver={} | graphics family={} | present "
            "family={} "
            "| "
            "dynamicRendering/synchronization2/timelineSemaphore/swapchainMaintenance1 enabled | validation={} "
            "| texture anisotropy={}",
            physical.name,
            VK_API_VERSION_MAJOR(system.instance_api_version),
            VK_API_VERSION_MINOR(system.instance_api_version),
            VK_API_VERSION_PATCH(system.instance_api_version),
            VK_API_VERSION_MAJOR(state_->instance.api_version),
            VK_API_VERSION_MINOR(state_->instance.api_version),
            VK_API_VERSION_MAJOR(physical.properties.apiVersion),
            VK_API_VERSION_MINOR(physical.properties.apiVersion),
            VK_API_VERSION_PATCH(physical.properties.apiVersion),
            VK_API_VERSION_MAJOR(kVulkanApiVersion),
            VK_API_VERSION_MINOR(kVulkanApiVersion),
            physical.properties.driverVersion,
            graphics.second,
            present.second,
            state_->capabilities.validation_enabled,
            state_->capabilities.max_texture_anisotropy
        );
        log::info(
            "[renderer] Vulkan descriptor indexing: {} material slots (runtime arrays, nonuniform sampled images; "
            "scene-slot "
            "updates)",
            state_->capabilities.indexed_material_capacity
        );
        log::info(
            "[renderer] Vulkan mesh shaders: {}; dispatch {}x{}x{}, total {} (EXT, 64 vertices/124 triangles, indexed "
            "fallback)",
            mesh,
            state_->capabilities.mesh_dispatch_limit[0],
            state_->capabilities.mesh_dispatch_limit[1],
            state_->capabilities.mesh_dispatch_limit[2],
            state_->capabilities.mesh_dispatch_total
        );
        for (const auto& extension : physical.get_extensions())
            log::debug("[renderer] Vulkan enabled device extension: {}", extension);
        log::info(
            "[renderer] Vulkan optional memory budget: {}; preferred anisotropy: {}",
            memory_budget ? "VK_EXT_memory_budget enabled" : "VMA estimated heap budgets (extension unavailable)",
            anisotropy ? "enabled" : "1x fallback (samplerAnisotropy unavailable)"
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
    void VulkanDevice::recreate_surface(const Window& window) const
    {
        if (state_->surface != VK_NULL_HANDLE)
            SDL_Vulkan_DestroySurface(instance(), state_->surface, nullptr);
        state_->surface = VK_NULL_HANDLE;
        core::require_sdl(
            SDL_Vulkan_CreateSurface(window.handle(), instance(), nullptr, &state_->surface),
            "Failed to recreate SDL Vulkan surface; presentation stopped"
        );
        VkBool32 supported = VK_FALSE;
        const auto result = vkGetPhysicalDeviceSurfaceSupportKHR(
            physical_device(),
            capabilities().present_queue_family,
            surface(),
            &supported
        );
        if (result != VK_SUCCESS || !supported)
            throw std::runtime_error(
                "Recreated SDL Vulkan surface is incompatible with the selected present queue: VkResult "
                + std::to_string(result) + "; restart the application to select a device"
            );
        log::info(
            "[renderer] Vulkan SDL surface recreated; present queue family {} remains compatible",
            capabilities().present_queue_family
        );
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

    auto VulkanDevice::memory_usage() const -> VulkanMemoryUsage
    {
        RELAYCORE_PROFILE_ZONE_N("Vulkan memory snapshot");
        // VMA refreshes driver budgets through this API; keep the query on the explicit snapshot path.
        vmaSetCurrentFrameIndex(allocator(), 0);
        const VkPhysicalDeviceMemoryProperties* properties = nullptr;
        vmaGetMemoryProperties(allocator(), &properties);
        std::array<VmaBudget, VK_MAX_MEMORY_HEAPS> budgets{};
        vmaGetHeapBudgets(allocator(), budgets.data());
        VulkanMemoryUsage result{.driver_budget = capabilities().memory_budget_enabled};
        result.heaps.reserve(properties->memoryHeapCount);
        for (uint32_t i = 0; i < properties->memoryHeapCount; ++i)
        {
            const auto& budget = budgets[i];
            result.heaps.push_back(
                {.usage_bytes = budget.usage,
                 .budget_bytes = budget.budget,
                 .block_bytes = budget.statistics.blockBytes,
                 .allocation_bytes = budget.statistics.allocationBytes,
                 .blocks = budget.statistics.blockCount,
                 .allocations = budget.statistics.allocationCount,
                 .device_local = (properties->memoryHeaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) != 0}
            );
        }
        return result;
    }

    auto VulkanDevice::pipeline_cache() const noexcept -> VkPipelineCache
    {
        return state_->pipeline_cache;
    }

    auto VulkanDevice::select_depth_format(
        const bool sampled_shadow,
        const VkSampleCountFlagBits samples,
        const VkFormat preferred
    ) const -> VkFormat
    {
        const std::array candidates =
            sampled_shadow ? std::array{VK_FORMAT_X8_D24_UNORM_PACK32, VK_FORMAT_D32_SFLOAT, VK_FORMAT_D16_UNORM}
                           : std::array{VK_FORMAT_D32_SFLOAT, VK_FORMAT_X8_D24_UNORM_PACK32, VK_FORMAT_D16_UNORM};
        const VkImageUsageFlags usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT
                                        | (sampled_shadow ? VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT
                                                                | VK_IMAGE_USAGE_TRANSFER_DST_BIT
                                                          : 0u);
        const VkFormatFeatureFlags required =
            VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
            | (sampled_shadow ? VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT | VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT
                                    | VK_FORMAT_FEATURE_TRANSFER_SRC_BIT | VK_FORMAT_FEATURE_TRANSFER_DST_BIT
                              : 0u);
        for (const auto format : candidates)
        {
            if (preferred != VK_FORMAT_UNDEFINED && format != preferred)
                continue;
            VkFormatProperties properties{};
            vkGetPhysicalDeviceFormatProperties(physical_device(), format, &properties);
            VkImageFormatProperties image{};
            if ((properties.optimalTilingFeatures & required) == required
                && vkGetPhysicalDeviceImageFormatProperties(
                       physical_device(),
                       format,
                       VK_IMAGE_TYPE_2D,
                       VK_IMAGE_TILING_OPTIMAL,
                       usage,
                       0,
                       &image
                   ) == VK_SUCCESS
                && (image.sampleCounts & samples) && image.maxArrayLayers >= (sampled_shadow ? 48u : 1u))
                return format;
        }
        throw std::runtime_error(
            std::string("No compatible Vulkan ") + (sampled_shadow ? "sampled shadow" : "scene")
            + " depth format: D32_SFLOAT/X8_D24_UNORM_PACK32/D16_UNORM; require attachment"
            + (sampled_shadow ? "/linear sampling/transfer/48 layers" : "") + ", samples=" + std::to_string(samples)
            + ", preferred=" + std::to_string(preferred)
        );
    }

    auto
    VulkanDevice::load_pipeline_cache(const std::filesystem::path& file, const std::filesystem::path& shaders) const
        -> bool
    {
        const auto started = std::chrono::steady_clock::now();
        size_t initial_bytes = 0;
        const auto finish = [&](const char* outcome, const std::string_view reason, const VkResult result = VK_SUCCESS
                            ) {
            log::info(
                "[renderer] Vulkan pipeline cache load: outcome={} reason='{}' duration_ms={:.3f} "
                "initial_bytes={} vk_result={} file='{}'",
                outcome,
                reason,
                std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started).count(),
                initial_bytes,
                static_cast<int>(result),
                file.string()
            );
            return std::string_view{outcome} == "loaded";
        };
        try
        {
            if (!std::filesystem::exists(file))
                return finish("missing", "file_absent");
            const auto expected = cache_header(physical_device(), shaders);
            std::ifstream stream(file, std::ios::binary | std::ios::ate);
            if (!stream)
                return finish("io_error", "file_open_failed");
            const auto size = stream.tellg();
            if (size < 0)
                return finish("io_error", "file_size_query_failed");
            if (size < static_cast<std::streamoff>(sizeof(CacheHeader))
                || size > static_cast<std::streamoff>(max_cache_bytes))
                return finish("corrupt", "file_size");
            CacheHeader header{};
            stream.seekg(0);
            stream.read(reinterpret_cast<char*>(&header), sizeof(header));
            if (!stream)
                return finish("io_error", "header_read_failed");
            if (header.schema != expected.schema)
                return finish("incompatible", "schema");
            if (header.vendor != expected.vendor || header.device != expected.device)
                return finish("incompatible", "vendor_or_device");
            if (header.driver != expected.driver)
                return finish("incompatible", "driver");
            if (std::memcmp(header.uuid, expected.uuid, VK_UUID_SIZE) != 0)
                return finish("incompatible", "pipeline_cache_uuid");
            if (header.shaders != expected.shaders)
                return finish("incompatible", "shader_hash");
            if (header.bytes != static_cast<size_t>(size) - sizeof(header)
                || header.bytes < sizeof(VkPipelineCacheHeaderVersionOne))
                return finish("corrupt", "payload_size");
            std::vector<char> bytes(static_cast<size_t>(header.bytes));
            stream.read(bytes.data(), bytes.size());
            if (!stream)
                return finish("io_error", "payload_read_failed");
            if (hash_bytes(bytes) != header.checksum)
                return finish("corrupt", "checksum");
            VkPipelineCacheHeaderVersionOne vk_header{};
            std::memcpy(&vk_header, bytes.data(), sizeof(vk_header));
            if (vk_header.headerSize != sizeof(vk_header)
                || vk_header.headerVersion != VK_PIPELINE_CACHE_HEADER_VERSION_ONE
                || vk_header.vendorID != header.vendor || vk_header.deviceID != header.device
                || std::memcmp(vk_header.pipelineCacheUUID, header.uuid, VK_UUID_SIZE) != 0)
                return finish("corrupt", "vulkan_header");
            const VkPipelineCacheCreateInfo info{
                .sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO,
                .initialDataSize = bytes.size(),
                .pInitialData = bytes.data()
            };
            VkPipelineCache loaded = VK_NULL_HANDLE;
            const auto result = vkCreatePipelineCache(handle(), &info, nullptr, &loaded);
            if (result != VK_SUCCESS)
                return finish("vulkan_rejected", "create", result);
            const auto merged = vkMergePipelineCaches(handle(), pipeline_cache(), 1, &loaded);
            vkDestroyPipelineCache(handle(), loaded, nullptr);
            if (merged != VK_SUCCESS)
                return finish("vulkan_rejected", "merge", merged);
            initial_bytes = bytes.size();
            return finish("loaded", "merged_into_live_cache");
        } catch (const std::filesystem::filesystem_error& error)
        {
            return finish("io_error", error.what());
        } catch (const std::runtime_error& error)
        {
            return finish("unavailable", error.what());
        }
    }

    auto
    VulkanDevice::save_pipeline_cache(const std::filesystem::path& file, const std::filesystem::path& shaders) const
        -> bool
    {
        try
        {
            auto header = cache_header(physical_device(), shaders);
            size_t size = 0;
            if (vkGetPipelineCacheData(handle(), pipeline_cache(), &size, nullptr) != VK_SUCCESS
                || size > max_cache_bytes - sizeof(header))
                return false;
            std::vector<char> bytes(size);
            if (vkGetPipelineCacheData(handle(), pipeline_cache(), &size, bytes.data()) != VK_SUCCESS)
                return false;
            bytes.resize(size);
            header.bytes = size;
            header.checksum = hash_bytes(bytes);
            std::filesystem::create_directories(file.parent_path());
            std::ofstream stream(file, std::ios::binary | std::ios::trunc);
            stream.write(reinterpret_cast<const char*>(&header), sizeof(header));
            stream.write(bytes.data(), bytes.size());
            stream.close();
            return !stream.fail();
        } catch (const std::filesystem::filesystem_error&)
        {
            return false;
        } catch (const std::runtime_error&)
        {
            return false;
        }
    }
} // namespace RelayCore::render
