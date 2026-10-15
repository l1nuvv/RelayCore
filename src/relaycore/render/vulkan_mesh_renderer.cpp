#include "relaycore/render/vulkan_mesh_renderer.h"

#include "relaycore/core/engine_config.h"
#include "relaycore/core/log.h"
#include "relaycore/render/authored_color.h"
#include "relaycore/render/camera.h"
#include "relaycore/render/irradiance_probes.h"
#include "relaycore/render/player_ui.h"
#include "relaycore/render/reflection_probe.h"
#include "relaycore/render/render_authoring_config.h"
#include "relaycore/render/render_resolution.h"
#include "relaycore/render/shadow_math.h"
#include "relaycore/render/vulkan_device.h"
#include "relaycore/render/vulkan_gpu_profiler.h"
#include "relaycore/render/vulkan_material_table.h"
#include "relaycore/render/vulkan_scene_submissions.h"
#include "relaycore/render/vulkan_uploads.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <imgui_impl_vulkan.h>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>

namespace RelayCore::render
{
    namespace
    {
        void require_mesh_vk(const VkResult result, const std::string_view operation)
        {
            if (result != VK_SUCCESS)
                throw std::runtime_error(std::string(operation) + ": VkResult " + std::to_string(result));
        }

        struct DrawPush {
            glm::mat4 mvp;
            std::array<glm::vec4, 3> model_rows;
            uint32_t flags;
            std::array<float, 3> camera_position;
        };
        static_assert(offsetof(DrawPush, model_rows) == 64);
        static_assert(offsetof(DrawPush, flags) == 112);
        static_assert(offsetof(DrawPush, camera_position) == 116);
        static_assert(sizeof(DrawPush) == 128);

        struct DrawInstance {
            glm::mat4 mvp;
            std::array<glm::vec4, 3> model_rows;
            glm::uvec4 correspondence;
        };
        static_assert(offsetof(DrawInstance, correspondence) == 112);
        static_assert(sizeof(DrawInstance) == 128);

        struct CullBatch {
            glm::uvec4 range;
            glm::vec4 mins, maxs;
        };
        static_assert(offsetof(CullBatch, mins) == 16);
        static_assert(sizeof(CullBatch) == 48);
        static_assert(sizeof(VkDrawIndexedIndirectCommand) == 20);

        struct PreviousDraw {
            glm::mat4 mvp;
            glm::vec4 valid;
        };
        static_assert(sizeof(PreviousDraw) == 80);

        struct MaterialParameters {
            std::array<glm::vec4, 32> atlas;
            glm::vec4 scene;
            std::array<glm::vec4, 32> response;
            glm::vec4 ambient, cavity, cool, warm, bounce, sky, local, direction, directional_color;
            glm::vec4 glass_tint, fog, postfx, grading, cascade_near_tint, cascade_far_tint;
        };
        static_assert(offsetof(MaterialParameters, response) == 528);
        static_assert(offsetof(MaterialParameters, ambient) == 1040);
        static_assert(offsetof(MaterialParameters, glass_tint) == 1184);
        static_assert(sizeof(MaterialParameters) == 1280);
        struct FrameParameters {
            glm::vec4 viewport_time_debug;
            glm::mat4 view_projection, inverse_view_projection, view_matrix;
            glm::vec4 camera_position, effects, lighting;
        };
        static_assert(sizeof(FrameParameters) == 256);

        struct LightParameters {
            std::array<glm::vec4, kMaxLocalLights> position_radius;
            std::array<glm::vec4, kMaxLocalLights> color;
            glm::ivec4 count;
        };
        static_assert(offsetof(LightParameters, color) == 512);
        static_assert(offsetof(LightParameters, count) == 1024);
        static_assert(sizeof(LightParameters) == 1040);

        struct IrradianceParameters {
            glm::vec4 mins{}, maxs{};
            glm::uvec4 dimensions{};
            std::array<glm::vec4, max_irradiance_probes * 9> coefficients{};
        };
        static_assert(offsetof(IrradianceParameters, coefficients) == 48);
        static_assert(sizeof(IrradianceParameters) == 9264);

        struct ShadowParameters {
            std::array<glm::mat4, 3> view_projection;
            glm::vec4 near_parameters, far_parameters, strengths, cascade;
            glm::vec4 spot_position_range, spot_direction, spot_color;
            glm::ivec4 flags;
        };
        static_assert(offsetof(ShadowParameters, near_parameters) == 192);
        static_assert(offsetof(ShadowParameters, flags) == 304);
        static_assert(sizeof(ShadowParameters) == 320);
        struct ShadowPush {
            glm::mat4 mvp;
            std::array<glm::vec4, 3> model_rows;
            // A negative range selects instance storage; its magnitude remains the point-light radius.
            glm::vec4 light_position_radius;
        };
        static_assert(sizeof(ShadowPush) == 128);

        struct ShadowImage {
            const VulkanDevice& device;
            VkExtent2D extent;
            uint32_t layers;
            VkImage image = VK_NULL_HANDLE;
            VmaAllocation allocation = nullptr;
            VkImageView view = VK_NULL_HANDLE;
            VkSampler sampler = VK_NULL_HANDLE;
            std::vector<VkImageView> layer_views;
            ShadowImage(const VulkanDevice& owner, const VkExtent2D size, const uint32_t count) :
                device(owner), extent(size), layers(count)
            {}
            ~ShadowImage() noexcept
            {
                if (sampler)
                    vkDestroySampler(device.handle(), sampler, nullptr);
                for (auto layer : layer_views)
                    if (layer)
                        vkDestroyImageView(device.handle(), layer, nullptr);
                if (view)
                    vkDestroyImageView(device.handle(), view, nullptr);
                if (image)
                    vmaDestroyImage(device.allocator(), image, allocation);
            }
            void initialize(const VkFormat format, const char* name)
            {
                VkFormatProperties properties{};
                vkGetPhysicalDeviceFormatProperties(device.physical_device(), format, &properties);
                constexpr auto required = VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
                                          | VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT
                                          | VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT
                                          | VK_FORMAT_FEATURE_TRANSFER_DST_BIT | VK_FORMAT_FEATURE_TRANSFER_SRC_BIT;
                if ((properties.optimalTilingFeatures & required) != required)
                    throw std::runtime_error(
                        "Vulkan shadows require sampled/linear-filtered depth attachments with transfer support"
                    );
                VkImageFormatProperties image_properties{};
                require_mesh_vk(
                    vkGetPhysicalDeviceImageFormatProperties(
                        device.physical_device(),
                        format,
                        VK_IMAGE_TYPE_2D,
                        VK_IMAGE_TILING_OPTIMAL,
                        VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT
                            | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
                        0,
                        &image_properties
                    ),
                    "Shadow depth image format support"
                );
                if (extent.width > image_properties.maxExtent.width || extent.height > image_properties.maxExtent.height
                    || layers > image_properties.maxArrayLayers)
                    throw std::runtime_error("Shadow depth image exceeds the selected format extent/layer limits");
                const VkImageCreateInfo info{
                    .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
                    .imageType = VK_IMAGE_TYPE_2D,
                    .format = format,
                    .extent = {extent.width, extent.height, 1},
                    .mipLevels = 1,
                    .arrayLayers = layers,
                    .samples = VK_SAMPLE_COUNT_1_BIT,
                    .tiling = VK_IMAGE_TILING_OPTIMAL,
                    .usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT
                             | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT
                };
                const VmaAllocationCreateInfo memory{.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE};
                require_mesh_vk(
                    vmaCreateImage(device.allocator(), &info, &memory, &image, &allocation, nullptr),
                    "Failed to allocate Vulkan shadow image"
                );
                VkImageViewCreateInfo vi{
                    .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                    .image = image,
                    .viewType = layers == 1 ? VK_IMAGE_VIEW_TYPE_2D : VK_IMAGE_VIEW_TYPE_2D_ARRAY,
                    .format = format,
                    .subresourceRange = {VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, layers}
                };
                require_mesh_vk(
                    vkCreateImageView(device.handle(), &vi, nullptr, &view),
                    "Failed to create sampled shadow view"
                );
                layer_views.resize(layers);
                vi.viewType = VK_IMAGE_VIEW_TYPE_2D;
                vi.subresourceRange.layerCount = 1;
                for (uint32_t i = 0; i < layers; ++i)
                {
                    vi.subresourceRange.baseArrayLayer = i;
                    require_mesh_vk(
                        vkCreateImageView(device.handle(), &vi, nullptr, &layer_views[i]),
                        "Failed to create shadow attachment view"
                    );
                }
                const VkSamplerCreateInfo si{
                    .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
                    .magFilter = VK_FILTER_LINEAR,
                    .minFilter = VK_FILTER_LINEAR,
                    .mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST,
                    .addressModeU =
                        layers == 1 ? VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER : VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
                    .addressModeV =
                        layers == 1 ? VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER : VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
                    .addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
                    .borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE
                };
                require_mesh_vk(
                    vkCreateSampler(device.handle(), &si, nullptr, &sampler),
                    "Failed to create shadow sampler"
                );
                const auto label = reinterpret_cast<PFN_vkSetDebugUtilsObjectNameEXT>(
                    vkGetDeviceProcAddr(device.handle(), "vkSetDebugUtilsObjectNameEXT")
                );
                if (label)
                {
                    const VkDebugUtilsObjectNameInfoEXT ni{
                        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
                        .objectType = VK_OBJECT_TYPE_IMAGE,
                        .objectHandle = reinterpret_cast<uint64_t>(image),
                        .pObjectName = name
                    };
                    require_mesh_vk(label(device.handle(), &ni), "Failed to name shadow image");
                }
            }
        };

        struct MeshImage {
            const VulkanDevice& device;
            VkImage image = VK_NULL_HANDLE;
            VmaAllocation allocation = nullptr;
            VkImageView view = VK_NULL_HANDLE;
            VkImageView unorm_view = VK_NULL_HANDLE;
            explicit MeshImage(const VulkanDevice& owner) : device(owner) {}
            ~MeshImage() noexcept
            {
                if (unorm_view)
                    vkDestroyImageView(device.handle(), unorm_view, nullptr);
                if (view)
                    vkDestroyImageView(device.handle(), view, nullptr);
                if (image)
                    vmaDestroyImage(device.allocator(), image, allocation);
            }
            void initialize(
                const VkExtent2D extent,
                const VkFormat format,
                const VkImageUsageFlags usage,
                const VkImageAspectFlags aspect,
                const VkFormat attachment_format = VK_FORMAT_UNDEFINED,
                const VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT
            )
            {
                const bool srgb_view = attachment_format == VK_FORMAT_R8G8B8A8_SRGB;
                const std::array formats{VK_FORMAT_R8G8B8A8_UNORM, VK_FORMAT_R8G8B8A8_SRGB};
                const VkImageFormatListCreateInfo format_list{
                    .sType = VK_STRUCTURE_TYPE_IMAGE_FORMAT_LIST_CREATE_INFO,
                    .viewFormatCount = static_cast<uint32_t>(formats.size()),
                    .pViewFormats = formats.data()
                };
                const VkImageCreateInfo info{
                    .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
                    .pNext = srgb_view ? &format_list : nullptr,
                    .flags = srgb_view ? VkImageCreateFlags{VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT} : 0u,
                    .imageType = VK_IMAGE_TYPE_2D,
                    .format = format,
                    .extent = {extent.width, extent.height, 1},
                    .mipLevels = 1,
                    .arrayLayers = 1,
                    .samples = samples,
                    .tiling = VK_IMAGE_TILING_OPTIMAL,
                    .usage = usage,
                };
                const VmaAllocationCreateInfo memory{.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE};
                require_mesh_vk(
                    vmaCreateImage(device.allocator(), &info, &memory, &image, &allocation, nullptr),
                    "Failed to allocate Vulkan mesh target"
                );
                VkImageViewCreateInfo view_info{
                    .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                    .image = image,
                    .viewType = VK_IMAGE_VIEW_TYPE_2D,
                    .format = attachment_format == VK_FORMAT_UNDEFINED ? format : attachment_format,
                    .subresourceRange = {aspect, 0, 1, 0, 1},
                };
                // The sRGB attachment view cannot inherit the UNORM storage usage.
                const VkImageViewUsageCreateInfo view_usage{
                    .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_USAGE_CREATE_INFO,
                    .usage = usage & ~VK_IMAGE_USAGE_STORAGE_BIT
                };
                if (srgb_view && (usage & VK_IMAGE_USAGE_STORAGE_BIT))
                    view_info.pNext = &view_usage;
                require_mesh_vk(
                    vkCreateImageView(device.handle(), &view_info, nullptr, &view),
                    "Failed to create Vulkan mesh target view"
                );
                if (srgb_view)
                {
                    view_info.pNext = nullptr;
                    view_info.format = format;
                    require_mesh_vk(
                        vkCreateImageView(device.handle(), &view_info, nullptr, &unorm_view),
                        "Failed to create unencoded Vulkan UI target view"
                    );
                }
            }
        };

        struct InputTarget {
            MeshImage normal_depth, velocity, depth, color_history;
            std::unique_ptr<MeshImage> ao, temporal, gi, lens;
            VkExtent2D gi_extent{};
            VulkanBuffer readback;
            InputTarget(
                const VulkanDevice& device,
                const VkExtent2D size,
                const VkFormat depth_format,
                const VkFormat color_format,
                const bool ssao,
                const bool taa,
                const bool ssgi,
                const bool lenses
            ) : normal_depth(device), velocity(device), depth(device), color_history(device), readback()
            {
                gi_extent = {std::max(1u, (size.width + 1) / 2), std::max(1u, (size.height + 1) / 2)};
                for (auto* target : {ssgi ? &gi : nullptr, lenses ? &lens : nullptr})
                    if (target)
                    {
                        *target = std::make_unique<MeshImage>(device);
                        (*target)->initialize(
                            target == &gi ? gi_extent : size,
                            color_format,
                            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                            VK_IMAGE_ASPECT_COLOR_BIT
                        );
                    }
                if (taa)
                {
                    temporal = std::make_unique<MeshImage>(device);
                    temporal->initialize(
                        size,
                        color_format,
                        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                        VK_IMAGE_ASPECT_COLOR_BIT
                    );
                }
                if (ssao)
                {
                    ao = std::make_unique<MeshImage>(device);
                    ao->initialize(
                        size,
                        VK_FORMAT_R32G32B32A32_SFLOAT,
                        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                        VK_IMAGE_ASPECT_COLOR_BIT
                    );
                }
                for (auto* image : {&normal_depth, &velocity})
                    image->initialize(
                        size,
                        VK_FORMAT_R32G32B32A32_SFLOAT,
                        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT
                            | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
                        VK_IMAGE_ASPECT_COLOR_BIT
                    );
                depth.initialize(
                    size,
                    depth_format,
                    VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                    VK_IMAGE_ASPECT_DEPTH_BIT
                );
                color_history.initialize(
                    size,
                    color_format,
                    VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                    VK_IMAGE_ASPECT_COLOR_BIT
                );
            }
        };

        struct MeshTarget {
            VkExtent2D extent, render_extent;
            MeshImage color, depth;
            std::unique_ptr<MeshImage> hdr_scene;
            std::unique_ptr<MeshImage> multisample_scene;
            VulkanBuffer readback;
            MeshTarget(
                const VulkanDevice& device,
                const VkExtent2D size,
                const VkExtent2D internal_size,
                const VkFormat scene_format,
                const VkFormat display_format,
                const VkFormat depth_format,
                const VkSampleCountFlagBits samples,
                const bool compute_tone_mapping,
                const bool input_history
            ) :
                extent(size), render_extent(internal_size), color(device), depth(device),
                readback(
                    device,
                    VkDeviceSize(size.width) * size.height * 4,
                    VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                    VulkanBufferMemory::readback
                )
            {
                color.initialize(
                    size,
                    VK_FORMAT_R8G8B8A8_UNORM,
                    VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT
                        | (compute_tone_mapping ? VK_IMAGE_USAGE_STORAGE_BIT : 0u),
                    VK_IMAGE_ASPECT_COLOR_BIT,
                    display_format
                );
                if (scene_format == VK_FORMAT_R16G16B16A16_SFLOAT)
                {
                    hdr_scene = std::make_unique<MeshImage>(device);
                    hdr_scene->initialize(
                        internal_size,
                        scene_format,
                        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT
                            | (input_history ? VK_IMAGE_USAGE_TRANSFER_SRC_BIT : 0u),
                        VK_IMAGE_ASPECT_COLOR_BIT
                    );
                }
                if (samples != VK_SAMPLE_COUNT_1_BIT)
                {
                    multisample_scene = std::make_unique<MeshImage>(device);
                    multisample_scene->initialize(
                        internal_size,
                        scene_format,
                        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
                        VK_IMAGE_ASPECT_COLOR_BIT,
                        VK_FORMAT_UNDEFINED,
                        samples
                    );
                }
                depth.initialize(
                    internal_size,
                    depth_format,
                    VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                    VK_IMAGE_ASPECT_DEPTH_BIT,
                    VK_FORMAT_UNDEFINED,
                    samples
                );
            }
        };

        struct MeshShaders {
            VkDevice device;
            VkShaderModule vertex = VK_NULL_HANDLE, fragment = VK_NULL_HANDLE;
            explicit MeshShaders(const VkDevice owner) : device(owner) {}
            ~MeshShaders() noexcept
            {
                if (vertex)
                    vkDestroyShaderModule(device, vertex, nullptr);
                if (fragment)
                    vkDestroyShaderModule(device, fragment, nullptr);
            }
            auto load(const std::filesystem::path& path, const char* entry) const -> VkShaderModule
            {
                const auto diagnostic = path.string() + " (entry " + entry + ")";
                std::ifstream stream(path, std::ios::binary | std::ios::ate);
                const auto size = stream.tellg();
                if (!stream || size < 20 || static_cast<size_t>(size) % 4 != 0)
                    throw std::runtime_error("Missing or malformed SPIR-V: " + diagnostic);
                std::vector<uint32_t> words(static_cast<size_t>(size) / 4);
                stream.seekg(0);
                if (!stream.read(reinterpret_cast<char*>(words.data()), size) || words.front() != 0x07230203)
                    throw std::runtime_error("Invalid SPIR-V: " + diagnostic);
                const VkShaderModuleCreateInfo info{
                    .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
                    .codeSize = words.size() * sizeof(uint32_t),
                    .pCode = words.data(),
                };
                VkShaderModule module;
                require_mesh_vk(vkCreateShaderModule(device, &info, nullptr, &module), "Shader module: " + diagnostic);
                return module;
            }
        };
        struct SceneCulling {
            VkDevice device;
            VkPipelineCache cache;
            VkDescriptorSetLayout descriptor_layout = VK_NULL_HANDLE;
            VkPipelineLayout layout = VK_NULL_HANDLE;
            VkDescriptorPool pool = VK_NULL_HANDLE;
            VkPipeline pipeline = VK_NULL_HANDLE;
            std::array<VkDescriptorSet, 2> sets{};
            uint32_t max_dispatch_x;

            SceneCulling(const VulkanDevice& owner, const uint32_t dispatch_limit) :
                device(owner.handle()), cache(owner.pipeline_cache()), max_dispatch_x(dispatch_limit)
            {}
            ~SceneCulling() noexcept
            {
                if (pipeline)
                    vkDestroyPipeline(device, pipeline, nullptr);
                if (pool)
                    vkDestroyDescriptorPool(device, pool, nullptr);
                if (layout)
                    vkDestroyPipelineLayout(device, layout, nullptr);
                if (descriptor_layout)
                    vkDestroyDescriptorSetLayout(device, descriptor_layout, nullptr);
            }
            void initialize()
            {
                std::array<VkDescriptorSetLayoutBinding, 4> bindings{};
                for (uint32_t i = 0; i < bindings.size(); ++i)
                    bindings[i] = {i, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr};
                const VkDescriptorSetLayoutCreateInfo descriptor_info{
                    .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
                    .bindingCount = static_cast<uint32_t>(bindings.size()),
                    .pBindings = bindings.data()
                };
                require_mesh_vk(
                    vkCreateDescriptorSetLayout(device, &descriptor_info, nullptr, &descriptor_layout),
                    "Scene culling descriptor layout"
                );
                const VkPushConstantRange push{VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(uint32_t)};
                const VkPipelineLayoutCreateInfo layout_info{
                    .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
                    .setLayoutCount = 1,
                    .pSetLayouts = &descriptor_layout,
                    .pushConstantRangeCount = 1,
                    .pPushConstantRanges = &push
                };
                require_mesh_vk(vkCreatePipelineLayout(device, &layout_info, nullptr, &layout), "Scene culling layout");
                const VkDescriptorPoolSize size{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 8};
                const VkDescriptorPoolCreateInfo pool_info{
                    .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
                    .maxSets = 2,
                    .poolSizeCount = 1,
                    .pPoolSizes = &size
                };
                require_mesh_vk(vkCreateDescriptorPool(device, &pool_info, nullptr, &pool), "Scene culling pool");
                const std::array layouts{descriptor_layout, descriptor_layout};
                const VkDescriptorSetAllocateInfo allocation{
                    .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
                    .descriptorPool = pool,
                    .descriptorSetCount = 2,
                    .pSetLayouts = layouts.data()
                };
                require_mesh_vk(vkAllocateDescriptorSets(device, &allocation, sets.data()), "Scene culling sets");
            }
            auto create_pipeline(const std::filesystem::path& directory) const -> VkPipeline
            {
                MeshShaders shader{device};
                shader.vertex = shader.load(directory / "scene_culling.comp.spv", "compute_main");
                const VkComputePipelineCreateInfo info{
                    .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
                    .stage =
                        {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                         .stage = VK_SHADER_STAGE_COMPUTE_BIT,
                         .module = shader.vertex,
                         .pName = "compute_main"},
                    .layout = layout
                };
                VkPipeline result = VK_NULL_HANDLE;
                require_mesh_vk(
                    vkCreateComputePipelines(device, cache, 1, &info, nullptr, &result),
                    "Scene culling pipeline"
                );
                return result;
            }
        };

        struct ToneMappingPush {
            glm::vec4 postfx, viewport_time_exposure, grading;
        };
        static_assert(sizeof(ToneMappingPush) == 48);

        struct ToneMapping {
            VkDevice device;
            VkPipelineCache cache;
            VkDescriptorSetLayout descriptor_layout = VK_NULL_HANDLE;
            VkPipelineLayout layout = VK_NULL_HANDLE;
            VkDescriptorPool pool = VK_NULL_HANDLE;
            std::array<VkDescriptorSet, 2> sets{};
            VkSampler sampler = VK_NULL_HANDLE;
            VkPipeline pipeline = VK_NULL_HANDLE;
            bool compute = false;
            const VulkanDevice& owner;
            VkPipeline bloom_pipeline = VK_NULL_HANDLE;
            VkSampler bloom_sampler = VK_NULL_HANDLE;
            std::array<std::vector<std::unique_ptr<MeshImage>>, 2> bloom;
            std::array<VkExtent2D, 2> bloom_extent{};
            explicit ToneMapping(const VulkanDevice& device_owner) :
                device(device_owner.handle()), cache(device_owner.pipeline_cache()), owner(device_owner)
            {
                VkFormatProperties format{};
                vkGetPhysicalDeviceFormatProperties(owner.physical_device(), VK_FORMAT_R8G8B8A8_UNORM, &format);
                compute = owner.capabilities().graphics_compute
                          && (format.optimalTilingFeatures & VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT);
                if (compute)
                {
                    VkImageFormatProperties support{};
                    const auto result = vkGetPhysicalDeviceImageFormatProperties(
                        owner.physical_device(),
                        VK_FORMAT_R8G8B8A8_UNORM,
                        VK_IMAGE_TYPE_2D,
                        VK_IMAGE_TILING_OPTIMAL,
                        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT
                            | VK_IMAGE_USAGE_STORAGE_BIT,
                        VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT,
                        &support
                    );
                    if (result != VK_SUCCESS && result != VK_ERROR_FORMAT_NOT_SUPPORTED)
                        require_mesh_vk(result, "Compute tone mapping target format support");
                    compute = result == VK_SUCCESS;
                }
                if (const auto* disable = std::getenv("RELAYCORE_VULKAN_DISABLE_COMPUTE_TONE_MAPPING"))
                {
                    const std::string_view value{disable};
                    if (value != "0" && value != "1")
                        throw std::invalid_argument("RELAYCORE_VULKAN_DISABLE_COMPUTE_TONE_MAPPING must be 0 or 1");
                    compute = compute && value != "1";
                }
                log::debug(
                    "[renderer] Vulkan HDR tone mapping: {}",
                    compute ? "compute (8x8, graphics queue)" : "raster"
                );
            }
            ~ToneMapping() noexcept
            {
                if (bloom_pipeline)
                    vkDestroyPipeline(device, bloom_pipeline, nullptr);
                if (bloom_sampler)
                    vkDestroySampler(device, bloom_sampler, nullptr);
                if (pipeline)
                    vkDestroyPipeline(device, pipeline, nullptr);
                if (pool)
                    vkDestroyDescriptorPool(device, pool, nullptr);
                if (sampler)
                    vkDestroySampler(device, sampler, nullptr);
                if (layout)
                    vkDestroyPipelineLayout(device, layout, nullptr);
                if (descriptor_layout)
                    vkDestroyDescriptorSetLayout(device, descriptor_layout, nullptr);
            }
            void initialize()
            {
                const VkShaderStageFlags stage =
                    VK_SHADER_STAGE_FRAGMENT_BIT | (compute ? VK_SHADER_STAGE_COMPUTE_BIT : 0u);
                const std::array bindings{
                    VkDescriptorSetLayoutBinding{0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, stage, nullptr},
                    VkDescriptorSetLayoutBinding{1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, stage, nullptr},
                    VkDescriptorSetLayoutBinding{2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, stage, nullptr},
                    VkDescriptorSetLayoutBinding{3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, stage, nullptr},
                    VkDescriptorSetLayoutBinding{4, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, stage, nullptr},
                    VkDescriptorSetLayoutBinding{5, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, stage, nullptr}
                };
                const VkDescriptorSetLayoutCreateInfo descriptor_info{
                    .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
                    .bindingCount = static_cast<uint32_t>(bindings.size()),
                    .pBindings = bindings.data()
                };
                require_mesh_vk(
                    vkCreateDescriptorSetLayout(device, &descriptor_info, nullptr, &descriptor_layout),
                    "HDR descriptor layout"
                );
                const VkPushConstantRange push{stage, 0, sizeof(ToneMappingPush)};
                const VkPipelineLayoutCreateInfo layout_info{
                    .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
                    .setLayoutCount = 1,
                    .pSetLayouts = &descriptor_layout,
                    .pushConstantRangeCount = 1,
                    .pPushConstantRanges = &push
                };
                require_mesh_vk(vkCreatePipelineLayout(device, &layout_info, nullptr, &layout), "HDR pipeline layout");
                const VkSamplerCreateInfo sampler_info{
                    .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
                    .magFilter = VK_FILTER_NEAREST,
                    .minFilter = VK_FILTER_NEAREST,
                    .mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST,
                    .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
                    .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
                    .addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE
                };
                require_mesh_vk(vkCreateSampler(device, &sampler_info, nullptr, &sampler), "HDR scene sampler");
                auto linear = sampler_info;
                linear.magFilter = linear.minFilter = VK_FILTER_LINEAR;
                require_mesh_vk(vkCreateSampler(device, &linear, nullptr, &bloom_sampler), "Bloom sampler");
                const std::array sizes{
                    VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 10},
                    VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 2}
                };
                const VkDescriptorPoolCreateInfo pool_info{
                    .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
                    .maxSets = 2,
                    .poolSizeCount = static_cast<uint32_t>(sizes.size()),
                    .pPoolSizes = sizes.data()
                };
                require_mesh_vk(vkCreateDescriptorPool(device, &pool_info, nullptr, &pool), "HDR descriptor pool");
                const std::array layouts{descriptor_layout, descriptor_layout};
                const VkDescriptorSetAllocateInfo allocate{
                    .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
                    .descriptorPool = pool,
                    .descriptorSetCount = 2,
                    .pSetLayouts = layouts.data()
                };
                require_mesh_vk(vkAllocateDescriptorSets(device, &allocate, sets.data()), "HDR descriptor set");
            }
            void bind(
                const VkImageView view,
                const VkImageView output,
                const size_t slot,
                VkExtent2D extent,
                const bool bloom_enabled
            )
            {
                const VkDescriptorImageInfo image{sampler, view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
                const VkWriteDescriptorSet write{
                    .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                    .dstSet = sets[slot],
                    .dstBinding = 0,
                    .descriptorCount = 1,
                    .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                    .pImageInfo = &image
                };
                vkUpdateDescriptorSets(device, 1, &write, 0, nullptr);
                if (bloom_enabled
                    && (bloom[slot].empty() || bloom_extent[slot].width != extent.width
                        || bloom_extent[slot].height != extent.height))
                {
                    bloom_extent[slot] = extent;
                    bloom[slot].clear();
                    for (int level = 0; level < 4; ++level)
                    {
                        extent = {std::max(1u, (extent.width + 1) / 2), std::max(1u, (extent.height + 1) / 2)};
                        auto bloom_image = std::make_unique<MeshImage>(owner);
                        bloom_image->initialize(
                            extent,
                            VK_FORMAT_R16G16B16A16_SFLOAT,
                            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT
                                | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
                            VK_IMAGE_ASPECT_COLOR_BIT
                        );
                        bloom[slot].push_back(std::move(bloom_image));
                    }
                }
                for (uint32_t level = 0; level < 4; ++level)
                {
                    const VkDescriptorImageInfo info{
                        bloom_enabled ? bloom_sampler : sampler,
                        bloom_enabled ? bloom[slot][level]->view : view,
                        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
                    };
                    auto bloom_write = write;
                    bloom_write.dstBinding = 2 + level;
                    bloom_write.pImageInfo = &info;
                    vkUpdateDescriptorSets(device, 1, &bloom_write, 0, nullptr);
                }
                if (compute)
                {
                    const VkDescriptorImageInfo storage{VK_NULL_HANDLE, output, VK_IMAGE_LAYOUT_GENERAL};
                    auto storage_write = write;
                    storage_write.dstBinding = 1;
                    storage_write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
                    storage_write.pImageInfo = &storage;
                    vkUpdateDescriptorSets(device, 1, &storage_write, 0, nullptr);
                }
            }
            auto create_pipeline(const std::filesystem::path& directory, const bool bright = false) const -> VkPipeline
            {
                MeshShaders shaders{device};
                if (compute && !bright)
                {
                    shaders.vertex = shaders.load(directory / "tone_mapping.comp.spv", "compute_main");
                    const VkComputePipelineCreateInfo info{
                        .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
                        .stage =
                            {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                             .stage = VK_SHADER_STAGE_COMPUTE_BIT,
                             .module = shaders.vertex,
                             .pName = "compute_main"},
                        .layout = layout
                    };
                    VkPipeline replacement = VK_NULL_HANDLE;
                    const auto result = vkCreateComputePipelines(device, cache, 1, &info, nullptr, &replacement);
                    if (result != VK_SUCCESS)
                    {
                        if (replacement)
                            vkDestroyPipeline(device, replacement, nullptr);
                        require_mesh_vk(result, "Compute tone mapping pipeline from " + directory.string());
                    }
                    return replacement;
                }
                shaders.vertex = shaders.load(directory / "tone_mapping.vert.spv", "vertex_main");
                shaders.fragment = shaders.load(
                    directory / (bright ? "tone_mapping.bloom.spv" : "tone_mapping.frag.spv"),
                    bright ? "bloom_fragment" : "fragment_main"
                );
                const std::array stages{
                    VkPipelineShaderStageCreateInfo{
                        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                        .stage = VK_SHADER_STAGE_VERTEX_BIT,
                        .module = shaders.vertex,
                        .pName = "vertex_main"
                    },
                    VkPipelineShaderStageCreateInfo{
                        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                        .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
                        .module = shaders.fragment,
                        .pName = bright ? "bloom_fragment" : "fragment_main"
                    }
                };
                const VkPipelineVertexInputStateCreateInfo vertices{
                    .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO
                };
                const VkPipelineInputAssemblyStateCreateInfo assembly{
                    .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
                    .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST
                };
                const VkPipelineViewportStateCreateInfo viewport{
                    .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
                    .viewportCount = 1,
                    .scissorCount = 1
                };
                const VkPipelineRasterizationStateCreateInfo raster{
                    .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
                    .polygonMode = VK_POLYGON_MODE_FILL,
                    .cullMode = VK_CULL_MODE_NONE,
                    .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
                    .lineWidth = 1
                };
                const VkPipelineMultisampleStateCreateInfo multisample{
                    .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
                    .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT
                };
                const VkPipelineColorBlendAttachmentState attachment{
                    .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT
                                      | VK_COLOR_COMPONENT_A_BIT
                };
                const VkPipelineColorBlendStateCreateInfo blend{
                    .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
                    .attachmentCount = 1,
                    .pAttachments = &attachment
                };
                const std::array dynamics{VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
                const VkPipelineDynamicStateCreateInfo dynamic{
                    .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
                    .dynamicStateCount = uint32_t(dynamics.size()),
                    .pDynamicStates = dynamics.data()
                };
                const VkFormat format = bright ? VK_FORMAT_R16G16B16A16_SFLOAT : VK_FORMAT_R8G8B8A8_SRGB;
                const VkPipelineRenderingCreateInfo rendering{
                    .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
                    .colorAttachmentCount = 1,
                    .pColorAttachmentFormats = &format
                };
                const VkGraphicsPipelineCreateInfo info{
                    .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
                    .pNext = &rendering,
                    .stageCount = uint32_t(stages.size()),
                    .pStages = stages.data(),
                    .pVertexInputState = &vertices,
                    .pInputAssemblyState = &assembly,
                    .pViewportState = &viewport,
                    .pRasterizationState = &raster,
                    .pMultisampleState = &multisample,
                    .pColorBlendState = &blend,
                    .pDynamicState = &dynamic,
                    .layout = layout
                };
                VkPipeline replacement = VK_NULL_HANDLE;
                const auto result = vkCreateGraphicsPipelines(device, cache, 1, &info, nullptr, &replacement);
                if (result != VK_SUCCESS)
                {
                    if (replacement)
                        vkDestroyPipeline(device, replacement, nullptr);
                    require_mesh_vk(result, "Tone mapping pipeline from " + directory.string());
                }
                return replacement;
            }
        };
    } // namespace

    struct VulkanMeshRenderer::State {
        struct MeshletGeometry {
            const VulkanDevice& device;
            VulkanBuffer clusters, vertices, triangles;
            VkDescriptorPool pool = VK_NULL_HANDLE;
            VkDescriptorSet set = VK_NULL_HANDLE;
            uint32_t count = 0;
            explicit MeshletGeometry(const VulkanDevice& owner) : device(owner) {}
            ~MeshletGeometry() noexcept
            {
                if (pool)
                    vkDestroyDescriptorPool(device.handle(), pool, nullptr);
            }
            void initialize(
                VulkanUploads& uploads,
                VkDescriptorSetLayout layout,
                const VulkanBuffer& vertex_buffer,
                const MeshletData& data
            )
            {
                count = uint32_t(data.clusters.size());
                auto upload = uploads.upload_mesh_buffer(
                    std::as_bytes(std::span{data.clusters}),
                    VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
                );
                clusters = std::move(upload.buffer);
                upload = uploads.upload_mesh_buffer(
                    std::as_bytes(std::span{data.vertices}),
                    VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
                );
                vertices = std::move(upload.buffer);
                upload = uploads.upload_mesh_buffer(
                    std::as_bytes(std::span{data.triangles}),
                    VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
                );
                triangles = std::move(upload.buffer);
                const VkDescriptorPoolSize size{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 4};
                const VkDescriptorPoolCreateInfo info{
                    .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
                    .maxSets = 1,
                    .poolSizeCount = 1,
                    .pPoolSizes = &size
                };
                require_mesh_vk(
                    vkCreateDescriptorPool(device.handle(), &info, nullptr, &pool),
                    "Meshlet descriptor pool"
                );
                const VkDescriptorSetAllocateInfo allocation{
                    .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
                    .descriptorPool = pool,
                    .descriptorSetCount = 1,
                    .pSetLayouts = &layout
                };
                require_mesh_vk(vkAllocateDescriptorSets(device.handle(), &allocation, &set), "Meshlet descriptor set");
                const std::array buffers{
                    VkDescriptorBufferInfo{vertex_buffer.handle(), 0, vertex_buffer.size()},
                    VkDescriptorBufferInfo{clusters.handle(), 0, clusters.size()},
                    VkDescriptorBufferInfo{vertices.handle(), 0, vertices.size()},
                    VkDescriptorBufferInfo{triangles.handle(), 0, triangles.size()}
                };
                VkPhysicalDeviceProperties properties{};
                vkGetPhysicalDeviceProperties(device.physical_device(), &properties);
                std::array<VkWriteDescriptorSet, 4> writes{};
                for (uint32_t i = 0; i < writes.size(); ++i)
                {
                    if (buffers[i].range > properties.limits.maxStorageBufferRange)
                        throw std::invalid_argument("Meshlet geometry exceeds the device storage buffer range");
                    writes[i] = {
                        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                        .dstSet = set,
                        .dstBinding = i,
                        .descriptorCount = 1,
                        .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                        .pBufferInfo = &buffers[i]
                    };
                }
                vkUpdateDescriptorSets(device.handle(), uint32_t(writes.size()), writes.data(), 0, nullptr);
            }
        };
        struct RetiredMeshlet {
            uint64_t completion, upload;
            std::unique_ptr<MeshletGeometry> geometry;
        };

        struct Mesh {
            std::weak_ptr<const CpuColorMeshData> source;
            VulkanBuffer vertices, indices;
            uint32_t index_count;
            VulkanSceneCompletion last_use{};
            uint64_t upload_completion = 0;
            uint64_t recording = 0;
            glm::vec3 mins{}, maxs{};
            std::unique_ptr<MeshletGeometry> meshlets;
        };
        struct MaterialKey {
            const CpuRgbaTextureData* color;
            const CpuRgbaTextureData* normal;
            float normal_scale;
            std::optional<MetallicRoughness> metallic_roughness;
            const CpuRgbaTextureData* mr_map;
            const CpuRgbaTextureData* occlusion;
            float occlusion_strength;
            const CpuRgbaTextureData* emissive;
            glm::vec3 emissive_factor;
            auto operator==(const MaterialKey&) const -> bool = default;
        };
        struct MaterialKeyHash {
            auto operator()(const MaterialKey& key) const -> std::size_t
            {
                const auto factors = key.metallic_roughness.value_or(MetallicRoughness{1, 0});
                const auto material_hash = std::hash<float>{}(factors.roughness)
                                           ^ (std::hash<float>{}(factors.metallic) << 1)
                                           ^ std::hash<bool>{}(key.metallic_roughness.has_value());
                return material_hash ^ std::hash<const void*>{}(key.color) ^ (std::hash<const void*>{}(key.normal) << 1)
                       ^ (std::hash<float>{}(key.normal_scale) << 2) ^ std::hash<const void*>{}(key.mr_map)
                       ^ (std::hash<const void*>{}(key.occlusion) << 2) ^ std::hash<float>{}(key.occlusion_strength)
                       ^ std::hash<const void*>{}(key.emissive) ^ std::hash<float>{}(key.emissive_factor.x)
                       ^ (std::hash<float>{}(key.emissive_factor.y) << 1)
                       ^ (std::hash<float>{}(key.emissive_factor.z) << 2);
            }
        };
        struct MaterialDescriptorPool {
            static constexpr uint32_t capacity = 128;
            VkDevice device;
            VkDescriptorPool handle = VK_NULL_HANDLE;
            uint32_t materials = 0;

            explicit MaterialDescriptorPool(const VkDevice owner) : device(owner)
            {
                const std::array sizes{
                    VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 14 * capacity},
                    VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 20 * capacity}
                };
                const VkDescriptorPoolCreateInfo info{
                    .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
                    .flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
                    .maxSets = 2 * capacity,
                    .poolSizeCount = static_cast<uint32_t>(sizes.size()),
                    .pPoolSizes = sizes.data()
                };
                require_mesh_vk(
                    vkCreateDescriptorPool(device, &info, nullptr, &handle),
                    "Failed to create shared mesh material pool"
                );
            }
            ~MaterialDescriptorPool() noexcept
            {
                vkDestroyDescriptorPool(device, handle, nullptr);
            }
        };

        struct Material {
            VulkanSceneCompletion last_use{};
            uint64_t upload_completion = 0;
            uint64_t recording = 0;
            std::weak_ptr<const CpuRgbaTextureData> source;
            std::weak_ptr<const CpuRgbaTextureData> normal_source;
            std::weak_ptr<const CpuRgbaTextureData> mr_source, occlusion_source, emissive_source;
            VulkanTexture texture, normal_texture, mr_texture, occlusion_texture, emissive_texture;
            VulkanBuffer surface_data;
            std::optional<uint32_t> table_index;
            MaterialDescriptorPool* pool = nullptr;
            std::array<VkDescriptorSet, 2> sets{};
            explicit Material(const VulkanDevice& owner) :
                surface_data(
                    owner,
                    sizeof(glm::vec4) * 3,
                    VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                    VulkanBufferMemory::upload
                )
            {}
            ~Material() noexcept
            {
                if (pool)
                {
                    const auto result = vkFreeDescriptorSets(pool->device, pool->handle, 2, sets.data());
                    if (result != VK_SUCCESS)
                        log::error_noexcept("[renderer] Vulkan material descriptor shutdown failed", result);
                    --pool->materials;
                }
            }
        };

        struct BatchKey {
            Mesh* geometry;
            Material* surface;
            uint32_t flags;
            auto operator==(const BatchKey&) const -> bool = default;
        };
        struct DrawBatch {
            BatchKey key;
            uint32_t first_instance, count = 1;
        };

        struct Frame {
            const VulkanDevice& device;
            VulkanGpuProfiler profiler;
            VulkanBuffer material_data, light_data, irradiance_data, frame_data, shadow_data;
            VkCommandPool pool = VK_NULL_HANDLE;
            std::array<VkCommandBuffer, 3> buffers{};
            VkCommandBuffer commands = VK_NULL_HANDLE;
            VkCommandBuffer culling_commands = VK_NULL_HANDLE;
            bool culling_pending = false;
            size_t command_index = 0;
            VulkanSceneCompletion pending{};
            VkQueryPool resolution_queries = VK_NULL_HANDLE;
            bool resolution_sample_pending = false;
            float rendered_scale = 1;
            std::unique_ptr<MeshTarget> target;
            std::unique_ptr<InputTarget> inputs;
            VulkanBuffer instances, indirect_commands;
            VulkanBuffer visible_instances, culling_batches, gpu_commands;
            std::vector<CullBatch> cull_records;
            std::vector<DrawInstance> instance_records;
            std::vector<VkDrawIndexedIndirectCommand> draw_commands;
            std::vector<DrawBatch> batches;
            VulkanDrawUsage draw_usage;
            VulkanBuffer previous_draws;
            std::vector<PreviousDraw> previous_records;
            std::vector<std::shared_ptr<const void>> ui_resources;
            explicit Frame(const VulkanDevice& owner) :
                device(owner), profiler(owner, "RelayCore Vulkan scene"), material_data(
                                                                              owner,
                                                                              sizeof(MaterialParameters),
                                                                              VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                                                              VulkanBufferMemory::upload
                                                                          ),
                light_data(
                    owner,
                    sizeof(LightParameters),
                    VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                    VulkanBufferMemory::upload
                ),
                irradiance_data(
                    owner,
                    sizeof(IrradianceParameters),
                    VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                    VulkanBufferMemory::upload
                ),
                frame_data(
                    owner,
                    sizeof(FrameParameters),
                    VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                    VulkanBufferMemory::upload
                ),
                shadow_data(
                    owner,
                    sizeof(ShadowParameters),
                    VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                    VulkanBufferMemory::upload
                )
            {}
            ~Frame() noexcept
            {
                if (resolution_queries)
                    vkDestroyQueryPool(device.handle(), resolution_queries, nullptr);
                if (pool)
                    vkDestroyCommandPool(device.handle(), pool, nullptr);
            }
        };

        const VulkanDevice& device;
        VulkanUploads uploads;
        VulkanSceneSubmissions submissions;
        VulkanBuffer reflection_data;
        VulkanTexture reflection_texture;
        std::shared_ptr<const ReflectionProbe> reflection;
        std::shared_ptr<const IrradianceProbeVolume> irradiance;
        float scene_time = 0;
        LightParameters lights{};
        RendererConfig renderer_config{};
        MaterialParameters material_parameters{};
        std::array<VkDescriptorSet, 2> shadow_previews{};
        ColorPipelineMode color_pipeline = ColorPipelineMode::legacy_gamma;
        VulkanDepthFormats depth_formats{};
        SpotLight spot{};
        ShadowParameters shadow_parameters{};
        std::array<std::unique_ptr<ShadowImage>, 3> shadow_maps;
        std::unique_ptr<ShadowImage> point_shadows;
        std::array<VkPipeline, 3> shadow_pipelines{};
        Texture2D atlas_source;
        std::unordered_map<const CpuColorMeshData*, Mesh> meshes;
        std::vector<std::unique_ptr<MaterialDescriptorPool>> material_pools;
        std::unordered_map<MaterialKey, std::unique_ptr<Material>, MaterialKeyHash> materials;
        std::unique_ptr<VulkanMaterialTable> material_table;
        Material* global_material = nullptr;
        bool mesh_shaders = false, mesh_pipeline_bound = false;
        VkPipeline meshlet_pipeline = VK_NULL_HANDLE;
        VkDescriptorSetLayout meshlet_layout = VK_NULL_HANDLE, empty_table_layout = VK_NULL_HANDLE;
        PFN_vkCmdDrawMeshTasksEXT cmd_mesh_tasks = nullptr;
        VkShaderStageFlags draw_stages = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
        std::vector<RetiredMeshlet> retired_meshlets;
        VkDescriptorSetLayout descriptor_layout = VK_NULL_HANDLE;
        VkPipelineLayout layout = VK_NULL_HANDLE;
        VkDescriptorSetLayout instance_layout = VK_NULL_HANDLE;
        VkDescriptorPool instance_pool = VK_NULL_HANDLE;
        std::array<VkDescriptorSet, 2> instance_sets{};
        VulkanSceneSubmission scene_submission = VulkanSceneSubmission::instanced;
        uint32_t max_instance_records = 0, instance_storage_limit = 0;
        std::unique_ptr<SceneCulling> culling;
        std::array<VkPipeline, 3> pipelines{};
        uint64_t recording = 0;
        std::vector<Mesh*> used_meshes;
        std::vector<Material*> used_materials;
        std::vector<std::unique_ptr<Material>> retired_materials;
        uint32_t max_target_width = 0, max_target_height = 0;
        std::unique_ptr<ToneMapping> tone_mapping;
        PassInputView input_view = PassInputView::off;
        VkDescriptorSetLayout input_layout = VK_NULL_HANDLE;
        VkDescriptorPool input_pool = VK_NULL_HANDLE;
        VkSampler input_sampler = VK_NULL_HANDLE;
        std::array<VkDescriptorSet, 2> input_sets{};
        std::array<VkPipeline, 4> input_pipelines{};
        VkPipeline ao_pipeline = VK_NULL_HANDLE, temporal_pipeline = VK_NULL_HANDLE;
        VkPipeline gi_pipeline = VK_NULL_HANDLE, lens_pipeline = VK_NULL_HANDLE;
        std::filesystem::path shader_directory;
        glm::mat4 previous_projection{1};
        glm::mat4 current_view_projection{1};
        glm::vec2 jitter_uv{}, previous_jitter_uv{};
        glm::mat4 previous_unjittered_view_projection{1};
        uint32_t jitter_index = 0;
        RenderResolution resolution;
        float gpu_scene_ms = 0, timestamp_period = 0;
        uint32_t timestamp_bits = 0;
        VkExtent2D history_extent{};
        float previous_exposure = 0;
        bool history_valid = false, frame_history_valid = false;
        size_t history_slot = 0;

        std::array<Frame, 2> frames;
        size_t frame_index = 0;
        auto frame() noexcept -> Frame&
        {
            return frames[frame_index];
        }
        auto frame() const noexcept -> const Frame&
        {
            return frames[frame_index];
        }

        explicit State(const VulkanDevice& owner) :
            device(owner), uploads(owner), submissions(owner), reflection_data(
                                                                   owner,
                                                                   sizeof(glm::vec4) * 3,
                                                                   VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                                                   VulkanBufferMemory::upload
                                                               ),
            frames{Frame{owner}, Frame{owner}}
        {}

        void wait_frame(Frame& slot)
        {
            if (slot.pending.value)
            {
                RELAYCORE_PROFILE_ZONE_N("Vulkan scene fence");
                submissions.wait(slot.pending);
                slot.pending = {};
                if (slot.resolution_sample_pending)
                {
                    std::array<uint64_t, 2> times{};
                    require_mesh_vk(
                        vkGetQueryPoolResults(
                            device.handle(),
                            slot.resolution_queries,
                            0,
                            2,
                            sizeof(times),
                            times.data(),
                            sizeof(uint64_t),
                            VK_QUERY_RESULT_64_BIT
                        ),
                        "Completed scene resolution timestamps"
                    );
                    const auto mask = timestamp_bits == 64 ? std::numeric_limits<uint64_t>::max()
                                                           : (uint64_t{1} << timestamp_bits) - 1;
                    gpu_scene_ms = float((times[1] - times[0]) & mask) * timestamp_period / 1e6F;
                    resolution.update(
                        gpu_scene_ms,
                        slot.rendered_scale,
                        renderer_config.render_scale,
                        renderer_config.dynamic_resolution_target_ms
                    );
                    slot.resolution_sample_pending = false;
                }
                slot.profiler.collect();
            }
            slot.ui_resources.clear();
        }
        void wait()
        {
            for (auto& slot : frames)
                wait_frame(slot);
        }
        void select_frame(const bool synchronous)
        {
            if (!synchronous)
                frame_index = (frame_index + 1) % frames.size();
            auto& slot = frame();
            wait_frame(slot);
            require_mesh_vk(vkResetCommandPool(device.handle(), slot.pool, 0), "Failed to reset scene slot");
            slot.command_index = 0;
            slot.culling_pending = false;
        }
        ~State() noexcept
        {
            RELAYCORE_PROFILE_ZONE_N("Vulkan scene shutdown drain");
            try
            {
                wait();
                uploads.wait(uploads.dependency().value);
            } catch (const std::exception& error)
            {
                log::error_noexcept("[renderer] Vulkan mesh shutdown failed", error.what());
            }
            for (auto preview : shadow_previews)
                if (preview)
                    ImGui_ImplVulkan_RemoveTexture(preview);
            materials.clear();
            retired_materials.clear();
            retired_meshlets.clear();
            if (meshlet_pipeline)
                vkDestroyPipeline(device.handle(), meshlet_pipeline, nullptr);
            if (meshlet_layout)
                vkDestroyDescriptorSetLayout(device.handle(), meshlet_layout, nullptr);
            if (empty_table_layout)
                vkDestroyDescriptorSetLayout(device.handle(), empty_table_layout, nullptr);
            for (const auto pipeline : pipelines)
                if (pipeline)
                    vkDestroyPipeline(device.handle(), pipeline, nullptr);
            for (const auto pipeline : shadow_pipelines)
                if (pipeline)
                    vkDestroyPipeline(device.handle(), pipeline, nullptr);
            if (layout)
                vkDestroyPipelineLayout(device.handle(), layout, nullptr);
            if (descriptor_layout)
                vkDestroyDescriptorSetLayout(device.handle(), descriptor_layout, nullptr);
            tone_mapping.reset();
            if (gi_pipeline)
                vkDestroyPipeline(device.handle(), gi_pipeline, nullptr);
            if (lens_pipeline)
                vkDestroyPipeline(device.handle(), lens_pipeline, nullptr);
            if (temporal_pipeline)
                vkDestroyPipeline(device.handle(), temporal_pipeline, nullptr);
            if (ao_pipeline)
                vkDestroyPipeline(device.handle(), ao_pipeline, nullptr);
            for (auto pipeline : input_pipelines)
                if (pipeline)
                    vkDestroyPipeline(device.handle(), pipeline, nullptr);
            if (instance_pool)
                vkDestroyDescriptorPool(device.handle(), instance_pool, nullptr);
            if (instance_layout)
                vkDestroyDescriptorSetLayout(device.handle(), instance_layout, nullptr);
            if (input_pool)
                vkDestroyDescriptorPool(device.handle(), input_pool, nullptr);
            if (input_sampler)
                vkDestroySampler(device.handle(), input_sampler, nullptr);
            if (input_layout)
                vkDestroyDescriptorSetLayout(device.handle(), input_layout, nullptr);
        }

        auto display_format() const noexcept -> VkFormat
        {
            return color_pipeline == ColorPipelineMode::hardware_srgb ? VK_FORMAT_R8G8B8A8_SRGB
                                                                      : VK_FORMAT_R8G8B8A8_UNORM;
        }

        auto needs_inputs() const noexcept -> bool
        {
            return (input_view != PassInputView::off && input_view != PassInputView::volume_transmittance)
                   || renderer_config.ssao_enabled || renderer_config.ssr_enabled || renderer_config.taa_enabled
                   || renderer_config.ssgi_quality || renderer_config.dof_quality
                   || renderer_config.motion_blur_quality;
        }

        auto resolved_source() -> MeshImage&
        {
            return renderer_config.taa_enabled && input_view != PassInputView::temporal_source
                       ? *frame().inputs->temporal
                       : *frame().target->hdr_scene;
        }

        auto tone_source() -> MeshImage&
        {
            if (renderer_config.dof_quality || renderer_config.motion_blur_quality)
                return *frame().inputs->lens;
            return resolved_source();
        }

        auto scene_format() const noexcept -> VkFormat
        {
            return renderer_config.hdr_enabled ? VK_FORMAT_R16G16B16A16_SFLOAT : display_format();
        }

        void initialize(
            const RenderAuthoringConfig& authoring,
            const CpuRgbaTextureData& atlas,
            const TextureEncoding encoding,
            const RendererConfig& config,
            const ColorPipelineMode pipeline,
            const VulkanDepthFormats preferred_depth
        )
        {
            renderer_config = config;
            if (config.descriptor_indexing && device.capabilities().indexed_material_capacity)
            {
                material_table = std::make_unique<VulkanMaterialTable>(device);
                material_table->initialize(device.capabilities().indexed_material_capacity);
            }
            log::debug(
                "[renderer] Vulkan scene materials: {}",
                material_table ? "descriptor indexing" : "pooled descriptor sets"
            );
            mesh_shaders = config.mesh_shaders && device.capabilities().mesh_dispatch_total != 0;
            if (mesh_shaders)
            {
                draw_stages |= VK_SHADER_STAGE_MESH_BIT_EXT;
                cmd_mesh_tasks = reinterpret_cast<PFN_vkCmdDrawMeshTasksEXT>(
                    vkGetDeviceProcAddr(device.handle(), "vkCmdDrawMeshTasksEXT")
                );
                if (!cmd_mesh_tasks)
                    throw std::runtime_error("Enabled VK_EXT_mesh_shader has no vkCmdDrawMeshTasksEXT dispatch");
                std::array<VkDescriptorSetLayoutBinding, 4> bindings{};
                for (uint32_t i = 0; i < bindings.size(); ++i)
                    bindings[i] = {i, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_MESH_BIT_EXT, nullptr};
                const VkDescriptorSetLayoutCreateInfo info{
                    .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
                    .bindingCount = uint32_t(bindings.size()),
                    .pBindings = bindings.data()
                };
                require_mesh_vk(
                    vkCreateDescriptorSetLayout(device.handle(), &info, nullptr, &meshlet_layout),
                    "Meshlet descriptor layout"
                );
                if (!material_table)
                {
                    const VkDescriptorSetLayoutCreateInfo empty{
                        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO
                    };
                    require_mesh_vk(
                        vkCreateDescriptorSetLayout(device.handle(), &empty, nullptr, &empty_table_layout),
                        "Empty material table layout"
                    );
                }
            }
            log::debug(
                "[renderer] Vulkan world geometry: {}",
                mesh_shaders ? "EXT meshlets (opaque camera/source passes)" : "indexed vertices"
            );
            resolution.reset(config.render_scale);
            color_pipeline = pipeline;
            if (config.dynamic_resolution_enabled)
            {
                VkPhysicalDeviceProperties properties{};
                vkGetPhysicalDeviceProperties(device.physical_device(), &properties);
                timestamp_period = properties.limits.timestampPeriod;
                uint32_t count = 0;
                vkGetPhysicalDeviceQueueFamilyProperties(device.physical_device(), &count, nullptr);
                std::vector<VkQueueFamilyProperties> families(count);
                vkGetPhysicalDeviceQueueFamilyProperties(device.physical_device(), &count, families.data());
                timestamp_bits = families[device.capabilities().graphics_queue_family].timestampValidBits;
                if (!timestamp_bits)
                    throw std::runtime_error(
                        "Dynamic resolution requires graphics GPU timestamps; disable render.dynamic_resolution_enabled"
                    );
                const VkQueryPoolCreateInfo query_info{
                    .sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO,
                    .queryType = VK_QUERY_TYPE_TIMESTAMP,
                    .queryCount = 2
                };
                for (auto& slot : frames)
                    require_mesh_vk(
                        vkCreateQueryPool(device.handle(), &query_info, nullptr, &slot.resolution_queries),
                        "Dynamic resolution timestamp pool"
                    );
            }
            if (config.msaa_samples != 1 && config.msaa_samples != 2 && config.msaa_samples != 4)
                throw std::invalid_argument("Vulkan render.msaa_samples must be 1, 2 or 4");
            const auto samples = static_cast<VkSampleCountFlagBits>(config.msaa_samples);
            depth_formats.scene = device.select_depth_format(false, samples, preferred_depth.scene);
            depth_formats.shadow = device.select_depth_format(true, VK_SAMPLE_COUNT_1_BIT, preferred_depth.shadow);
            log::debug(
                "[renderer] Vulkan depth formats: scene={} shadow={} samples={}",
                static_cast<int>(depth_formats.scene),
                static_cast<int>(depth_formats.shadow),
                config.msaa_samples
            );
            for (const auto format : {scene_format(), depth_formats.scene})
            {
                VkImageFormatProperties support{};
                const auto usage = format == depth_formats.scene ? VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT
                                                                 : VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
                require_mesh_vk(
                    vkGetPhysicalDeviceImageFormatProperties(
                        device.physical_device(),
                        format,
                        VK_IMAGE_TYPE_2D,
                        VK_IMAGE_TILING_OPTIMAL,
                        usage,
                        0,
                        &support
                    ),
                    "MSAA attachment format support"
                );
                if (!(support.sampleCounts & samples))
                    throw std::runtime_error(
                        "GPU does not support render.msaa_samples for scene color/depth; use 1 or 2"
                    );
            }
            VkPhysicalDeviceProperties properties{};
            vkGetPhysicalDeviceProperties(device.physical_device(), &properties);
            max_target_width = std::min(properties.limits.maxFramebufferWidth, properties.limits.maxImageDimension2D);
            max_target_height = std::min(properties.limits.maxFramebufferHeight, properties.limits.maxImageDimension2D);
            instance_storage_limit = properties.limits.maxStorageBufferRange / sizeof(DrawInstance);
            bool culling_available = device.capabilities().graphics_compute
                                     && properties.limits.maxComputeWorkGroupInvocations >= 64
                                     && properties.limits.maxComputeWorkGroupSize[0] >= 64
                                     && properties.limits.maxPerStageDescriptorStorageBuffers >= 4;
            if (const auto* override = std::getenv("RELAYCORE_VULKAN_DISABLE_GPU_CULLING"))
            {
                const std::string_view value{override};
                if (value != "0" && value != "1")
                    throw std::invalid_argument("RELAYCORE_VULKAN_DISABLE_GPU_CULLING must be 0 or 1");
                culling_available = culling_available && value != "1";
            }
            if (culling_available)
            {
                culling = std::make_unique<SceneCulling>(device, properties.limits.maxComputeWorkGroupCount[0]);
                culling->initialize();
            }
            log::debug(
                "[renderer] Vulkan scene GPU culling: {}",
                culling ? "available (graphics queue, stable compaction)" : "unavailable; CPU indirect fallback"
            );
            VkFormatProperties color_properties{};
            vkGetPhysicalDeviceFormatProperties(device.physical_device(), VK_FORMAT_R8G8B8A8_UNORM, &color_properties);
            constexpr auto color_features = VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT
                                            | VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BLEND_BIT
                                            | VK_FORMAT_FEATURE_BLIT_SRC_BIT | VK_FORMAT_FEATURE_TRANSFER_SRC_BIT;
            if ((color_properties.optimalTilingFeatures & color_features) != color_features)
                throw std::runtime_error(
                    "Vulkan mesh renderer requires RGBA8 UNORM color attachment/blend/readback/blit support"
                );
            vkGetPhysicalDeviceFormatProperties(device.physical_device(), scene_format(), &color_properties);
            const auto attachment_features = VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT
                                             | VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BLEND_BIT
                                             | (config.hdr_enabled ? VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT : 0u);
            if ((color_properties.optimalTilingFeatures & attachment_features) != attachment_features)
                throw std::runtime_error(
                    "Vulkan scene target requires the selected format attachment/blend and HDR sampling support"
                );
            vkGetPhysicalDeviceFormatProperties(device.physical_device(), display_format(), &color_properties);
            if (!(color_properties.optimalTilingFeatures & VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT))
                throw std::runtime_error("Vulkan display target requires the selected UNORM/sRGB attachment support");
            MaterialParameters parameters{};
            static_assert(surface_material_count() <= 32);
            for (size_t i = 0; i < authoring.materials.size(); ++i)
            {
                parameters.atlas[i] = {
                    authoring.materials[i].atlas_world_scale,
                    authoring.materials[i].atlas_mix,
                    static_cast<float>(authoring.materials[i].render_profile),
                    0
                };
                parameters.response[i] = {
                    authoring.materials[i].ambient_bias,
                    authoring.materials[i].bounce_mix,
                    authoring.materials[i].emissive_strength,
                    authoring.materials[i].emissive_pulse_speed
                };
            }
            const auto& light = authoring.lighting;
            parameters.ambient = {light.ambient_floor, light.ambient_zenith, light.ambient_min, light.ambient_max};
            parameters.cavity = {
                light.cavity_broad_strength,
                light.cavity_micro_strength,
                light.cavity_underside_strength,
                light.cavity_ambient_darken
            };
            parameters.cool = {visual_rgb_for_pipeline(light.cool_ambient_color, pipeline), light.cool_ambient_floor};
            parameters.warm = {visual_rgb_for_pipeline(light.warm_bounce_color, pipeline), light.warm_bounce_floor};
            parameters.bounce = {visual_rgb_for_pipeline(light.bounce_neutral_color, pipeline), light.bounce_floor};
            parameters.sky =
                {light.cool_ambient_zenith, light.warm_bounce_underside, light.bounce_underside, light.bounce_darkness};
            parameters.local = {light.local_light_wrap, light.local_light_fill, light.local_light_gain, 0};
            parameters.glass_tint = {visual_rgb_for_pipeline({.12F, .18F, .22F}, pipeline), 0};
            parameters.cascade_near_tint = {visual_rgb_for_pipeline({.18F, .42F, .95F}, pipeline), 0};
            parameters.cascade_far_tint = {visual_rgb_for_pipeline({.95F, .56F, .14F}, pipeline), 0};
            material_parameters = parameters;
            set_config(config);
            for (size_t i = 0; i < shadow_maps.size(); ++i)
            {
                const uint32_t size =
                    i == 2 ? 1024u : static_cast<uint32_t>(std::max(config.shadow_map_resolution, 256));
                if (size > max_target_width || size > max_target_height)
                    throw std::invalid_argument("Vulkan shadow resolution exceeds the device framebuffer/image limits");
                shadow_maps[i] = std::make_unique<ShadowImage>(device, VkExtent2D{size, size}, 1);
                shadow_maps[i]->initialize(
                    depth_formats.shadow,
                    i == 0   ? "Shadow near cascade depth"
                    : i == 1 ? "Shadow far cascade depth"
                             : "Shadow spot depth"
                );
            }
            point_shadows = std::make_unique<ShadowImage>(device, VkExtent2D{512, 512}, 48);
            point_shadows->initialize(depth_formats.shadow, "Shadow point faces depth (eight lights, six faces)");
            atlas_source.initialize_rgba(atlas.width, atlas.height, atlas.pixels, encoding);
            const std::array bindings{
                VkDescriptorSetLayoutBinding{
                    16,
                    VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                    1,
                    VK_SHADER_STAGE_FRAGMENT_BIT,
                    nullptr
                },
                VkDescriptorSetLayoutBinding{
                    14,
                    VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                    1,
                    VK_SHADER_STAGE_FRAGMENT_BIT,
                    nullptr
                },
                VkDescriptorSetLayoutBinding{
                    15,
                    VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                    1,
                    VK_SHADER_STAGE_FRAGMENT_BIT,
                    nullptr
                },
                VkDescriptorSetLayoutBinding{
                    12,
                    VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                    1,
                    VK_SHADER_STAGE_FRAGMENT_BIT,
                    nullptr
                },
                VkDescriptorSetLayoutBinding{
                    13,
                    VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                    1,
                    VK_SHADER_STAGE_FRAGMENT_BIT,
                    nullptr
                },
                VkDescriptorSetLayoutBinding{
                    11,
                    VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                    1,
                    VK_SHADER_STAGE_FRAGMENT_BIT,
                    nullptr
                },
                VkDescriptorSetLayoutBinding{
                    9,
                    VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                    1,
                    VK_SHADER_STAGE_FRAGMENT_BIT,
                    nullptr
                },
                VkDescriptorSetLayoutBinding{
                    10,
                    VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                    1,
                    VK_SHADER_STAGE_FRAGMENT_BIT,
                    nullptr
                },
                VkDescriptorSetLayoutBinding{
                    8,
                    VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                    1,
                    VK_SHADER_STAGE_FRAGMENT_BIT,
                    nullptr
                },
                VkDescriptorSetLayoutBinding{
                    0,
                    VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                    1,
                    VK_SHADER_STAGE_FRAGMENT_BIT,
                    nullptr
                },
                VkDescriptorSetLayoutBinding{
                    1,
                    VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                    1,
                    VK_SHADER_STAGE_FRAGMENT_BIT,
                    nullptr
                },
                VkDescriptorSetLayoutBinding{
                    2,
                    VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                    1,
                    VK_SHADER_STAGE_FRAGMENT_BIT,
                    nullptr
                },
                VkDescriptorSetLayoutBinding{
                    3,
                    VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                    1,
                    VK_SHADER_STAGE_FRAGMENT_BIT,
                    nullptr
                },
                VkDescriptorSetLayoutBinding{
                    4,
                    VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                    1,
                    VK_SHADER_STAGE_FRAGMENT_BIT,
                    nullptr
                },
                VkDescriptorSetLayoutBinding{
                    5,
                    VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                    1,
                    VK_SHADER_STAGE_FRAGMENT_BIT,
                    nullptr
                },
                VkDescriptorSetLayoutBinding{
                    6,
                    VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                    1,
                    VK_SHADER_STAGE_FRAGMENT_BIT,
                    nullptr
                },
                VkDescriptorSetLayoutBinding{
                    7,
                    VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                    1,
                    VK_SHADER_STAGE_FRAGMENT_BIT,
                    nullptr
                },
            };
            const VkDescriptorSetLayoutCreateInfo descriptor_info{
                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
                .bindingCount = static_cast<uint32_t>(bindings.size()),
                .pBindings = bindings.data(),
            };
            require_mesh_vk(
                vkCreateDescriptorSetLayout(device.handle(), &descriptor_info, nullptr, &descriptor_layout),
                "Failed to create Vulkan mesh descriptor layout"
            );
            std::array<VkDescriptorSetLayoutBinding, 13> input_bindings{};
            input_bindings[0] = {0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, nullptr};
            for (uint32_t i = 1; i < input_bindings.size(); ++i)
                input_bindings[i] =
                    {i, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr};
            const VkDescriptorSetLayoutCreateInfo input_info{
                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
                .bindingCount = static_cast<uint32_t>(input_bindings.size()),
                .pBindings = input_bindings.data()
            };
            require_mesh_vk(
                vkCreateDescriptorSetLayout(device.handle(), &input_info, nullptr, &input_layout),
                "Pass input descriptor layout"
            );
            const VkDescriptorSetLayoutBinding instance_binding{
                0,
                VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                1,
                VkShaderStageFlags{VK_SHADER_STAGE_VERTEX_BIT} | (mesh_shaders ? VK_SHADER_STAGE_MESH_BIT_EXT : 0U),
                nullptr
            };
            const VkDescriptorSetLayoutCreateInfo instance_info{
                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
                .bindingCount = 1,
                .pBindings = &instance_binding
            };
            require_mesh_vk(
                vkCreateDescriptorSetLayout(device.handle(), &instance_info, nullptr, &instance_layout),
                "Scene instance descriptor layout"
            );
            const VkDescriptorPoolSize instance_size{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2};
            const VkDescriptorPoolCreateInfo instance_pool_info{
                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
                .maxSets = 2,
                .poolSizeCount = 1,
                .pPoolSizes = &instance_size
            };
            require_mesh_vk(
                vkCreateDescriptorPool(device.handle(), &instance_pool_info, nullptr, &instance_pool),
                "Scene instance descriptor pool"
            );
            const std::array instance_layouts{instance_layout, instance_layout};
            const VkDescriptorSetAllocateInfo instance_allocation{
                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
                .descriptorPool = instance_pool,
                .descriptorSetCount = 2,
                .pSetLayouts = instance_layouts.data()
            };
            require_mesh_vk(
                vkAllocateDescriptorSets(device.handle(), &instance_allocation, instance_sets.data()),
                "Scene instance descriptor sets"
            );
            std::vector set_layouts{descriptor_layout, input_layout, instance_layout};
            if (material_table)
                set_layouts.push_back(material_table->layout());
            else if (mesh_shaders)
                set_layouts.push_back(empty_table_layout);
            if (mesh_shaders)
                set_layouts.push_back(meshlet_layout);
            const VkPushConstantRange push{draw_stages, 0, sizeof(DrawPush)};
            const VkPipelineLayoutCreateInfo layout_info{
                .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
                .setLayoutCount = static_cast<uint32_t>(set_layouts.size()),
                .pSetLayouts = set_layouts.data(),
                .pushConstantRangeCount = 1,
                .pPushConstantRanges = &push,
            };
            require_mesh_vk(
                vkCreatePipelineLayout(device.handle(), &layout_info, nullptr, &layout),
                "Failed to create Vulkan mesh pipeline layout"
            );
            for (auto& slot : frames)
            {
                const VkCommandPoolCreateInfo pool_info{
                    .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
                    .queueFamilyIndex = device.capabilities().graphics_queue_family
                };
                require_mesh_vk(
                    vkCreateCommandPool(device.handle(), &pool_info, nullptr, &slot.pool),
                    "Failed to create scene slot pool"
                );
                const VkCommandBufferAllocateInfo allocation{
                    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
                    .commandPool = slot.pool,
                    .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
                    .commandBufferCount = static_cast<uint32_t>(slot.buffers.size())
                };
                require_mesh_vk(
                    vkAllocateCommandBuffers(device.handle(), &allocation, slot.buffers.data()),
                    "Failed to allocate scene slot commands"
                );
                auto culling_allocation = allocation;
                culling_allocation.commandBufferCount = 1;
                require_mesh_vk(
                    vkAllocateCommandBuffers(device.handle(), &culling_allocation, &slot.culling_commands),
                    "Failed to allocate scene culling commands"
                );
            }
            if (config.bloom_enabled)
            {
                VkFormatProperties bloom_properties{};
                vkGetPhysicalDeviceFormatProperties(
                    device.physical_device(),
                    VK_FORMAT_R16G16B16A16_SFLOAT,
                    &bloom_properties
                );
                constexpr auto required = VK_FORMAT_FEATURE_BLIT_SRC_BIT | VK_FORMAT_FEATURE_BLIT_DST_BIT
                                          | VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT;
                if ((bloom_properties.optimalTilingFeatures & required) != required)
                    throw std::runtime_error("Bloom requires FP16 linear filtering/blits; disable render.bloom_enabled"
                    );
            }
            if (config.hdr_enabled)
            {
                tone_mapping = std::make_unique<ToneMapping>(device);
                tone_mapping->initialize();
            }
        }

        auto create_pipeline(
            const std::filesystem::path& directory,
            const RenderPass pass,
            const int shadow_kind = -1,
            const bool mesh_shader = false
        ) const -> VkPipeline
        {
            MeshShaders shaders{device.handle()};
            const bool inputs = shadow_kind == -2, ao = shadow_kind == -4, temporal = shadow_kind == -5;
            const bool gi = shadow_kind == -6, lens = shadow_kind == -7;
            const bool advanced = gi || lens;
            const bool debug_inputs = shadow_kind == -3 || ao || temporal || advanced;
            const auto prefix = advanced                 ? "advanced"
                                : temporal               ? "temporal"
                                : inputs || debug_inputs ? "pass_inputs"
                                : shadow_kind < 0        ? "mesh"
                                : shadow_kind == 2       ? "point_shadow"
                                                         : "shadow";
            const auto vertex_entry = mesh_shader    ? "meshlet_main"
                                      : advanced     ? "advanced_vertex"
                                      : temporal     ? "temporal_vertex"
                                      : inputs       ? "input_vertex"
                                      : ao           ? "ao_vertex"
                                      : debug_inputs ? "debug_vertex"
                                                     : "vertex_main";
            const auto fragment_entry = gi             ? "gi_fragment"
                                        : lens         ? "lens_fragment"
                                        : temporal     ? "temporal_fragment"
                                        : inputs       ? "input_fragment"
                                        : ao           ? "ao_fragment"
                                        : debug_inputs ? "debug_fragment"
                                                       : "fragment_main";
            shaders.vertex = shaders.load(
                mesh_shader ? directory / (material_table ? "meshlets.mesh.indexed.spv" : "meshlets.mesh.spv")
                            : directory
                                  / (std::string(prefix) + "."
                                     + (advanced                 ? "vert"
                                        : inputs || debug_inputs ? vertex_entry
                                                                 : "vert")
                                     + (material_table && (shadow_kind == -1 || inputs) ? ".indexed.spv" : ".spv")),
                vertex_entry
            );
            shaders.fragment = shaders.load(
                directory
                    / (std::string(
                           advanced ? (gi ? "advanced_gi" : "advanced_lens")
                           : shadow_kind == -1
                                   && (renderer_config.ssao_enabled || renderer_config.ssr_enabled
                                       || renderer_config.ssgi_quality || renderer_config.volumetrics_enabled)
                               ? (renderer_config.ssao_enabled || renderer_config.ssr_enabled
                                          || renderer_config.ssgi_quality
                                      ? "mesh_effects"
                                      : "mesh_volume")
                               : prefix
                       )
                       + "."
                       + (advanced                 ? "frag"
                          : inputs || debug_inputs ? fragment_entry
                                                   : "frag")
                       + (material_table && (shadow_kind == -1 || inputs) ? ".indexed.spv" : ".spv")),
                fragment_entry
            );
            const std::array stages{
                VkPipelineShaderStageCreateInfo{
                    .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                    .stage = mesh_shader ? VK_SHADER_STAGE_MESH_BIT_EXT : VK_SHADER_STAGE_VERTEX_BIT,
                    .module = shaders.vertex,
                    .pName = vertex_entry
                },
                VkPipelineShaderStageCreateInfo{
                    .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                    .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
                    .module = shaders.fragment,
                    .pName = fragment_entry
                },
            };
            const VkVertexInputBindingDescription binding{0, sizeof(ColorVertex), VK_VERTEX_INPUT_RATE_VERTEX};
            const std::array attributes{
                VkVertexInputAttributeDescription{0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(ColorVertex, position)},
                VkVertexInputAttributeDescription{1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(ColorVertex, color)},
                VkVertexInputAttributeDescription{2, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(ColorVertex, normal)},
                VkVertexInputAttributeDescription{3, 0, VK_FORMAT_R32_SFLOAT, offsetof(ColorVertex, material_kind)},
                VkVertexInputAttributeDescription{4, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(ColorVertex, uv)},
                VkVertexInputAttributeDescription{5, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(ColorVertex, tangent)},
            };
            const std::array input_attributes{attributes[0], attributes[2], attributes[4], attributes[5]};
            const VkPipelineVertexInputStateCreateInfo vertex_info{
                .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
                .vertexBindingDescriptionCount = debug_inputs ? 0u : 1u,
                .pVertexBindingDescriptions = &binding,
                .vertexAttributeDescriptionCount = debug_inputs      ? 0u
                                                   : inputs          ? static_cast<uint32_t>(input_attributes.size())
                                                   : shadow_kind < 0 ? static_cast<uint32_t>(attributes.size())
                                                                     : 1u,
                .pVertexAttributeDescriptions = inputs ? input_attributes.data() : attributes.data(),
            };
            const VkPipelineInputAssemblyStateCreateInfo assembly{
                .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
                .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
            };
            const VkPipelineViewportStateCreateInfo viewport{
                .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
                .viewportCount = 1,
                .scissorCount = 1,
            };
            const VkPipelineRasterizationStateCreateInfo raster{
                .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
                .polygonMode = VK_POLYGON_MODE_FILL,
                .cullMode = debug_inputs                                    ? VkCullModeFlags{VK_CULL_MODE_NONE}
                            : shadow_kind == 0                              ? VkCullModeFlags{VK_CULL_MODE_FRONT_BIT}
                            : shadow_kind > 0 || pass == RenderPass::Opaque ? VkCullModeFlags{VK_CULL_MODE_BACK_BIT}
                                                                            : VkCullModeFlags{VK_CULL_MODE_NONE},
                .frontFace = shadow_kind < 0 ? VK_FRONT_FACE_COUNTER_CLOCKWISE : VK_FRONT_FACE_CLOCKWISE,
                .lineWidth = 1,
            };
            const VkPipelineMultisampleStateCreateInfo multisample{
                .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
                .rasterizationSamples = shadow_kind == -1
                                            ? static_cast<VkSampleCountFlagBits>(renderer_config.msaa_samples)
                                            : VK_SAMPLE_COUNT_1_BIT,
            };
            const VkPipelineDepthStencilStateCreateInfo depth{
                .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
                .depthTestEnable = debug_inputs ? VK_FALSE : VK_TRUE,
                .depthWriteEnable = !debug_inputs && pass == RenderPass::Opaque ? VK_TRUE : VK_FALSE,
                .depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL,
            };
            const VkPipelineColorBlendAttachmentState blend_attachment{
                .blendEnable = pass == RenderPass::Opaque || inputs ? VK_FALSE : VK_TRUE,
                .srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
                .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
                .colorBlendOp = VK_BLEND_OP_ADD,
                .srcAlphaBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
                .dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
                .alphaBlendOp = VK_BLEND_OP_ADD,
                .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT
                                  | VK_COLOR_COMPONENT_A_BIT,
            };
            const std::array blend_attachments{blend_attachment, blend_attachment};
            const VkPipelineColorBlendStateCreateInfo blend{
                .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
                .attachmentCount = inputs            ? 2u
                                   : shadow_kind < 0 ? 1u
                                                     : 0u,
                .pAttachments = shadow_kind < 0 ? blend_attachments.data() : nullptr,
            };
            const std::array dynamics{VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
            const VkPipelineDynamicStateCreateInfo dynamic{
                .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
                .dynamicStateCount = static_cast<uint32_t>(dynamics.size()),
                .pDynamicStates = dynamics.data(),
            };
            const std::array colors{
                inputs                                   ? VK_FORMAT_R32G32B32A32_SFLOAT
                : ao                                     ? VK_FORMAT_R32G32B32A32_SFLOAT
                : debug_inputs && !temporal && !advanced ? VK_FORMAT_R8G8B8A8_UNORM
                                                         : scene_format(),
                VK_FORMAT_R32G32B32A32_SFLOAT
            };
            const VkPipelineRenderingCreateInfo rendering{
                .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
                .colorAttachmentCount = inputs            ? 2u
                                        : shadow_kind < 0 ? 1u
                                                          : 0u,
                .pColorAttachmentFormats = shadow_kind < 0 ? colors.data() : nullptr,
                .depthAttachmentFormat = debug_inputs      ? VK_FORMAT_UNDEFINED
                                         : shadow_kind < 0 ? depth_formats.scene
                                                           : depth_formats.shadow,
            };
            const VkGraphicsPipelineCreateInfo info{
                .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
                .pNext = &rendering,
                .stageCount = static_cast<uint32_t>(stages.size()),
                .pStages = stages.data(),
                .pVertexInputState = mesh_shader ? nullptr : &vertex_info,
                .pInputAssemblyState = mesh_shader ? nullptr : &assembly,
                .pViewportState = &viewport,
                .pRasterizationState = &raster,
                .pMultisampleState = &multisample,
                .pDepthStencilState = &depth,
                .pColorBlendState = &blend,
                .pDynamicState = &dynamic,
                .layout = layout,
            };
            VkPipeline replacement = VK_NULL_HANDLE;
            const auto result =
                vkCreateGraphicsPipelines(device.handle(), device.pipeline_cache(), 1, &info, nullptr, &replacement);
            if (result != VK_SUCCESS)
            {
                if (replacement)
                    vkDestroyPipeline(device.handle(), replacement, nullptr);
                require_mesh_vk(
                    result,
                    "Pipeline " + std::string(prefix) + " vertex_main/fragment_main from " + directory.string()
                );
            }
            return replacement;
        }

        auto mesh(const StaticColorMesh& resource) -> Mesh&
        {
            const auto& cpu = resource.cpu_data();
            if (!cpu)
                throw std::invalid_argument("Vulkan draw requires an initialized CPU mesh");
            auto found = meshes.find(cpu.get());
            if (found != meshes.end())
                return found->second;
            auto entry = meshes.emplace(cpu.get(), Mesh{cpu, {}, {}, static_cast<uint32_t>(cpu->indices.size())}).first;
            auto& result = entry->second;
            result.mins = resource.bounds_min();
            result.maxs = resource.bounds_max();
            try
            {
                auto vertices = uploads.upload_mesh_buffer(
                    std::as_bytes(std::span{cpu->vertices}),
                    mesh_shaders
                        ? VkBufferUsageFlags{VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT}
                        : VkBufferUsageFlags{VK_BUFFER_USAGE_VERTEX_BUFFER_BIT}
                );
                result.vertices = std::move(vertices.buffer);
                auto indices = uploads.upload_mesh_buffer(
                    std::as_bytes(std::span{cpu->indices}),
                    VK_BUFFER_USAGE_INDEX_BUFFER_BIT
                );
                result.indices = std::move(indices.buffer);
                if (mesh_shaders)
                {
                    std::optional<MeshletData> generated;
                    if (cpu->meshlets.clusters.empty())
                        generated = build_meshlets(*cpu);
                    result.meshlets = std::make_unique<MeshletGeometry>(device);
                    result.meshlets
                        ->initialize(uploads, meshlet_layout, result.vertices, generated ? *generated : cpu->meshlets);
                }
                result.upload_completion = uploads.dependency().value;
                return result;
            } catch (...)
            {
                uploads.wait(uploads.dependency().value);
                meshes.erase(entry);
                throw;
            }
        }

        void update_reflection(std::shared_ptr<const ReflectionProbe> probe)
        {
            if (probe == reflection && reflection_texture.view())
                return;
            if (probe)
                validate_reflection_probe(*probe);
            wait();
            ReflectionProbe fallback{{{}, {-1, -1, -1}, {1, 1, 1}, 8, 32}, 0, {}};
            if (!probe)
                for (uint32_t size = 8; size; size >>= 1)
                    fallback.mips.emplace_back(6 * size * size, glm::vec4{0, 0, 0, 1});
            auto uploaded = uploads.upload_reflection(probe ? *probe : fallback);
            uploads.wait(uploaded.completion);
            // Material descriptors reference the shared cube. Frames are complete before replacing it.
            retire_materials();
            if (reflection_texture.image())
                submissions.retire(std::move(reflection_texture), submissions.last_submission());
            collect_retired_resources();
            reflection_texture = std::move(uploaded.texture);
            reflection = std::move(probe);
            const std::array data{
                glm::vec4{reflection ? reflection->settings.mins : glm::vec3{}, reflection ? 1.F : 0.F},
                glm::vec4{
                    reflection ? reflection->settings.maxs : glm::vec3{},
                    static_cast<float>(reflection_texture.mip_levels() - 1)
                },
                glm::vec4{reflection ? reflection->settings.position : glm::vec3{}, 0}
            };
            reflection_data.write(std::as_bytes(std::span{data}));
        }

        auto material_key(
            const Texture2D& resource,
            const Texture2D* normal,
            const float scale,
            const std::optional<MetallicRoughness> factors,
            const Texture2D* mr_map,
            const Texture2D* occlusion,
            const float occlusion_strength,
            const Texture2D* emissive,
            const glm::vec3 emissive_factor
        ) const -> MaterialKey
        {
            return {
                resource.cpu_data().get(),
                normal ? normal->cpu_data().get() : nullptr,
                normal ? scale : 1,
                factors,
                mr_map ? mr_map->cpu_data().get() : nullptr,
                occlusion ? occlusion->cpu_data().get() : nullptr,
                occlusion ? occlusion_strength : 1,
                emissive ? emissive->cpu_data().get() : nullptr,
                emissive_factor
            };
        }

        auto material(
            const Texture2D& resource,
            const Texture2D* normal,
            const float scale,
            const std::optional<MetallicRoughness> factors,
            const Texture2D* mr_map,
            const Texture2D* occlusion,
            const float occlusion_strength,
            const Texture2D* emissive,
            const glm::vec3 emissive_factor
        ) -> Material&
        {
            const auto& cpu = resource.cpu_data();
            if (!cpu)
                throw std::invalid_argument("Vulkan draw requires an initialized CPU texture");
            if (normal
                && (!normal->cpu_data() || normal->encoding() != TextureEncoding::linear || !std::isfinite(scale)))
                throw std::invalid_argument("Vulkan normal map requires an initialized linear texture and finite scale"
                );
            if (factors
                && (!std::isfinite(factors->roughness) || factors->roughness < 0 || factors->roughness > 1
                    || !std::isfinite(factors->metallic) || factors->metallic < 0 || factors->metallic > 1))
                throw std::invalid_argument("Vulkan metallic/roughness factors must be finite and in [0, 1]");
            for (const auto* map : {mr_map, occlusion})
                if (map && (!map->valid() || map->encoding() != TextureEncoding::linear))
                    throw std::invalid_argument("Vulkan material map requires an initialized linear texture");
            if (mr_map && !factors)
                throw std::invalid_argument("Vulkan metallic/roughness map requires factors");
            if (!std::isfinite(occlusion_strength) || occlusion_strength < 0 || occlusion_strength > 1)
                throw std::invalid_argument("Vulkan occlusion strength must be in [0, 1]");
            if (!reflection_texture.view())
                update_reflection({});
            if (emissive && (!emissive->valid() || emissive->encoding() != TextureEncoding::srgb))
                throw std::invalid_argument("Vulkan emissive map requires an initialized sRGB texture");
            for (const auto factor : {emissive_factor.x, emissive_factor.y, emissive_factor.z})
                if (!std::isfinite(factor) || factor < 0 || factor > 1)
                    throw std::invalid_argument("Vulkan emissive factor must be finite and in [0, 1]");
            const auto key = material_key(
                resource,
                normal,
                scale,
                factors,
                mr_map,
                occlusion,
                occlusion_strength,
                emissive,
                emissive_factor
            );
            const auto found = materials.find(key);
            if (found != materials.end())
                return *found->second;
            auto entry = materials.emplace(key, std::make_unique<Material>(device)).first;
            auto* result = entry->second.get();
            try
            {
                auto uploaded = uploads.upload_rgba(*cpu, resource.encoding());
                result->texture = std::move(uploaded.texture);
                result->source = cpu;
                if (normal)
                {
                    auto uploaded_normal = uploads.upload_rgba(*normal->cpu_data(), TextureEncoding::linear);
                    result->normal_texture = std::move(uploaded_normal.texture);
                    result->normal_source = normal->cpu_data();
                }
                if (mr_map)
                {
                    auto uploaded_map = uploads.upload_rgba(*mr_map->cpu_data(), TextureEncoding::linear);
                    result->mr_texture = std::move(uploaded_map.texture);
                    result->mr_source = mr_map->cpu_data();
                }
                if (occlusion)
                {
                    auto uploaded_map = uploads.upload_rgba(*occlusion->cpu_data(), TextureEncoding::linear);
                    result->occlusion_texture = std::move(uploaded_map.texture);
                    result->occlusion_source = occlusion->cpu_data();
                }
                if (emissive)
                {
                    auto uploaded_map = uploads.upload_rgba(*emissive->cpu_data(), TextureEncoding::srgb);
                    result->emissive_texture = std::move(uploaded_map.texture);
                    result->emissive_source = emissive->cpu_data();
                }
                const auto values = factors.value_or(MetallicRoughness{1, 0});
                const std::array surface_parameters{
                    glm::vec4{
                        key.normal_scale,
                        values.roughness,
                        values.metallic,
                        color_pipeline == ColorPipelineMode::hardware_srgb ? 1.0F : 0.0F
                    },
                    glm::vec4{mr_map ? 1.F : 0.F, occlusion ? 1.F : 0.F, key.occlusion_strength, 0},
                    glm::vec4{emissive_factor, emissive ? 1.F : 0.F}
                };
                result->surface_data.write(std::as_bytes(std::span{surface_parameters}));
                if (material_table)
                {
                    const auto image = [](const VulkanTexture& texture) {
                        return VkDescriptorImageInfo{
                            texture.sampler(),
                            texture.view(),
                            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
                        };
                    };
                    result->table_index = material_table->allocate(
                        {image(result->texture),
                         image(normal ? result->normal_texture : result->texture),
                         image(mr_map ? result->mr_texture : result->texture),
                         image(occlusion ? result->occlusion_texture : result->texture),
                         image(emissive ? result->emissive_texture : result->texture)},
                        surface_parameters
                    );
                    if (*result->table_index != 0)
                    {
                        result->upload_completion = uploads.dependency().value;
                        return *result;
                    }
                    global_material = result;
                }
                allocate_material_descriptors(*result);
                for (size_t slot = 0; slot < frames.size(); ++slot)
                {
                    const VkDescriptorBufferInfo buffer{
                        frames[slot].material_data.handle(),
                        0,
                        frames[slot].material_data.size()
                    };
                    const VkDescriptorBufferInfo lighting{
                        frames[slot].light_data.handle(),
                        0,
                        frames[slot].light_data.size()
                    };
                    const VkDescriptorBufferInfo irradiance_buffer{
                        frames[slot].irradiance_data.handle(),
                        0,
                        frames[slot].irradiance_data.size()
                    };
                    const VkDescriptorBufferInfo shadow_buffer{
                        frames[slot].shadow_data.handle(),
                        0,
                        frames[slot].shadow_data.size()
                    };
                    const VkDescriptorBufferInfo frame_buffer{
                        frames[slot].frame_data.handle(),
                        0,
                        frames[slot].frame_data.size()
                    };
                    std::array<VkDescriptorImageInfo, 4> shadow_images{};
                    for (size_t i = 0; i < 4; ++i)
                    {
                        const auto& map = i < 3 ? *shadow_maps[i] : *point_shadows;
                        shadow_images[i] = {map.sampler, map.view, VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL};
                    }
                    const VkDescriptorImageInfo image{
                        result->texture.sampler(),
                        result->texture.view(),
                        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
                    };
                    const auto& normal_image_texture = normal ? result->normal_texture : result->texture;
                    const VkDescriptorImageInfo normal_image{
                        normal_image_texture.sampler(),
                        normal_image_texture.view(),
                        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
                    };
                    const auto& mr_image_texture = mr_map ? result->mr_texture : result->texture;
                    const auto& ao_image_texture = occlusion ? result->occlusion_texture : result->texture;
                    const VkDescriptorImageInfo mr_image{
                        mr_image_texture.sampler(),
                        mr_image_texture.view(),
                        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
                    };
                    const VkDescriptorImageInfo ao_image{
                        ao_image_texture.sampler(),
                        ao_image_texture.view(),
                        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
                    };
                    const VkDescriptorBufferInfo normal_buffer{
                        result->surface_data.handle(),
                        0,
                        result->surface_data.size()
                    };
                    const VkDescriptorImageInfo reflection_image{
                        reflection_texture.sampler(),
                        reflection_texture.view(),
                        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
                    };
                    const VkDescriptorBufferInfo reflection_buffer{reflection_data.handle(), 0, reflection_data.size()};
                    const auto& emissive_image_texture = emissive ? result->emissive_texture : result->texture;
                    const VkDescriptorImageInfo emissive_image{
                        emissive_image_texture.sampler(),
                        emissive_image_texture.view(),
                        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
                    };
                    const std::array writes{
                        VkWriteDescriptorSet{
                            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                            .dstSet = result->sets[slot],
                            .dstBinding = 16,
                            .descriptorCount = 1,
                            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                            .pImageInfo = &emissive_image
                        },
                        VkWriteDescriptorSet{
                            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                            .dstSet = result->sets[slot],
                            .dstBinding = 14,
                            .descriptorCount = 1,
                            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                            .pImageInfo = &reflection_image
                        },
                        VkWriteDescriptorSet{
                            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                            .dstSet = result->sets[slot],
                            .dstBinding = 15,
                            .descriptorCount = 1,
                            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                            .pBufferInfo = &reflection_buffer
                        },
                        VkWriteDescriptorSet{
                            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                            .dstSet = result->sets[slot],
                            .dstBinding = 12,
                            .descriptorCount = 1,
                            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                            .pImageInfo = &mr_image
                        },
                        VkWriteDescriptorSet{
                            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                            .dstSet = result->sets[slot],
                            .dstBinding = 13,
                            .descriptorCount = 1,
                            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                            .pImageInfo = &ao_image
                        },
                        VkWriteDescriptorSet{
                            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                            .dstSet = result->sets[slot],
                            .dstBinding = 11,
                            .descriptorCount = 1,
                            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                            .pBufferInfo = &irradiance_buffer
                        },
                        VkWriteDescriptorSet{
                            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                            .dstSet = result->sets[slot],
                            .dstBinding = 9,
                            .descriptorCount = 1,
                            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                            .pImageInfo = &normal_image
                        },
                        VkWriteDescriptorSet{
                            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                            .dstSet = result->sets[slot],
                            .dstBinding = 10,
                            .descriptorCount = 1,
                            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                            .pBufferInfo = &normal_buffer
                        },
                        VkWriteDescriptorSet{
                            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                            .dstSet = result->sets[slot],
                            .dstBinding = 8,
                            .descriptorCount = 1,
                            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                            .pBufferInfo = &frame_buffer
                        },
                        VkWriteDescriptorSet{
                            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                            .dstSet = result->sets[slot],
                            .dstBinding = 0,
                            .descriptorCount = 1,
                            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                            .pBufferInfo = &buffer
                        },
                        VkWriteDescriptorSet{
                            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                            .dstSet = result->sets[slot],
                            .dstBinding = 1,
                            .descriptorCount = 1,
                            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                            .pImageInfo = &image
                        },
                        VkWriteDescriptorSet{
                            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                            .dstSet = result->sets[slot],
                            .dstBinding = 2,
                            .descriptorCount = 1,
                            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                            .pBufferInfo = &lighting
                        },
                        VkWriteDescriptorSet{
                            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                            .dstSet = result->sets[slot],
                            .dstBinding = 3,
                            .descriptorCount = 1,
                            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                            .pBufferInfo = &shadow_buffer
                        },
                        VkWriteDescriptorSet{
                            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                            .dstSet = result->sets[slot],
                            .dstBinding = 4,
                            .descriptorCount = 1,
                            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                            .pImageInfo = &shadow_images[0]
                        },
                        VkWriteDescriptorSet{
                            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                            .dstSet = result->sets[slot],
                            .dstBinding = 5,
                            .descriptorCount = 1,
                            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                            .pImageInfo = &shadow_images[1]
                        },
                        VkWriteDescriptorSet{
                            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                            .dstSet = result->sets[slot],
                            .dstBinding = 6,
                            .descriptorCount = 1,
                            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                            .pImageInfo = &shadow_images[2]
                        },
                        VkWriteDescriptorSet{
                            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                            .dstSet = result->sets[slot],
                            .dstBinding = 7,
                            .descriptorCount = 1,
                            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                            .pImageInfo = &shadow_images[3]
                        },
                    };
                    vkUpdateDescriptorSets(
                        device.handle(),
                        static_cast<uint32_t>(writes.size()),
                        writes.data(),
                        0,
                        nullptr
                    );
                }
                result->upload_completion = uploads.dependency().value;
                return *result;
            } catch (...)
            {
                uploads.wait(uploads.dependency().value);
                if (result->table_index)
                    material_table->release(*result->table_index);
                if (global_material == result)
                    global_material = nullptr;
                materials.erase(entry);
                throw;
            }
        }

        void allocate_material_descriptors(Material& material)
        {
            const std::array layouts{descriptor_layout, descriptor_layout};
            const auto allocate = [&](MaterialDescriptorPool& pool) {
                std::array<VkDescriptorSet, 2> sets{};
                const VkDescriptorSetAllocateInfo info{
                    .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
                    .descriptorPool = pool.handle,
                    .descriptorSetCount = static_cast<uint32_t>(sets.size()),
                    .pSetLayouts = layouts.data()
                };
                const auto result = vkAllocateDescriptorSets(device.handle(), &info, sets.data());
                if (result == VK_SUCCESS)
                {
                    material.sets = sets;
                    material.pool = &pool;
                    ++pool.materials;
                }
                return result;
            };
            for (auto& pool : material_pools)
                if (pool->materials < MaterialDescriptorPool::capacity)
                {
                    const auto result = allocate(*pool);
                    if (result == VK_SUCCESS)
                        return;
                    if (result != VK_ERROR_OUT_OF_POOL_MEMORY && result != VK_ERROR_FRAGMENTED_POOL)
                        require_mesh_vk(result, "Failed to allocate shared mesh material descriptors");
                }
            material_pools.push_back(std::make_unique<MaterialDescriptorPool>(device.handle()));
            require_mesh_vk(
                allocate(*material_pools.back()),
                "Failed to allocate mesh material descriptors from a new pool"
            );
        }

        void collect_retired_resources()
        {
            RELAYCORE_PROFILE_ZONE_N("Vulkan material retirement collect");
            const auto completed = submissions.completed();
            const auto uploaded = uploads.completed();
            std::erase_if(retired_materials, [this, completed, uploaded](const auto& material) {
                if (material->last_use.value > completed || material->upload_completion > uploaded)
                    return false;
                if (material->table_index)
                    material_table->release(*material->table_index);
                return true;
            });
            // Keep one reusable block; never reclaim a pool holding in-flight descriptors.
            if (!material_pools.empty())
                std::erase_if(material_pools, [first = material_pools.front().get()](const auto& pool) {
                    return pool.get() != first && pool->materials == 0;
                });
            std::erase_if(retired_meshlets, [completed, uploaded](const auto& geometry) {
                return geometry.completion <= completed && geometry.upload <= uploaded;
            });
            submissions.collect();
        }

        void retire_materials()
        {
            global_material = nullptr;
            retired_materials.reserve(retired_materials.size() + materials.size());
            for (auto& [key, material] : materials)
                retired_materials.push_back(std::move(material));
            materials.clear();
        }

        void collect_expired_resources()
        {
            for (auto it = meshes.begin(); it != meshes.end();)
            {
                auto& mesh = it->second;
                if (!mesh.source.expired())
                {
                    ++it;
                    continue;
                }
                if (mesh.last_use.value)
                {
                    submissions.retire(std::move(mesh.vertices), mesh.last_use);
                    submissions.retire(std::move(mesh.indices), mesh.last_use);
                } else
                {
                    uploads.retire(std::move(mesh.vertices), mesh.upload_completion);
                    uploads.retire(std::move(mesh.indices), mesh.upload_completion);
                }
                if (mesh.meshlets)
                    retired_meshlets.push_back({mesh.last_use.value, mesh.upload_completion, std::move(mesh.meshlets)});
                it = meshes.erase(it);
            }
            for (auto it = materials.begin(); it != materials.end();)
            {
                const auto& key = it->first;
                auto& material = it->second;
                if (!material->source.expired() && (!key.normal || !material->normal_source.expired())
                    && (!key.mr_map || !material->mr_source.expired())
                    && (!key.occlusion || !material->occlusion_source.expired())
                    && (!key.emissive || !material->emissive_source.expired()))
                {
                    ++it;
                    continue;
                }
                retired_materials.push_back(std::move(material));
                it = materials.erase(it);
            }
            collect_retired_resources();
        }

        void use(Mesh& mesh)
        {
            if (mesh.recording != recording)
            {
                used_meshes.push_back(&mesh);
                mesh.recording = recording;
            }
        }

        void use(Material& material)
        {
            if (material.recording != recording)
            {
                used_materials.push_back(&material);
                material.recording = recording;
            }
        }

        void prepare(const std::span<const RenderItem> items)
        {
            if (material_table && !global_material)
                static_cast<void>(material(atlas_source, nullptr, 1, {}, nullptr, nullptr, 1, nullptr, {}));
            for (const auto& item : items)
                if (item.mesh)
                {
                    static_cast<void>(mesh(*item.mesh));
                    static_cast<void>(material(
                        item.diffuse_texture ? *item.diffuse_texture : atlas_source,
                        item.normal_texture,
                        item.normal_scale,
                        item.metallic_roughness,
                        item.metallic_roughness_map,
                        item.occlusion_texture,
                        item.occlusion_strength,
                        item.emissive_texture,
                        item.emissive_factor
                    ));
                }
        }
        void prepare_draw_storage(const size_t scene_count, const size_t shadow_count)
        {
            // Color, pass inputs and reflection source; two directional + spot + all point faces.
            const uint64_t shadow_passes =
                (spot.enabled ? 1 : 0)
                + (renderer_config.shadow_enabled ? 2 + 6 * std::min(lights.count.x, int(kMaxPointShadowLights)) : 0);
            const uint64_t count =
                std::max(uint64_t{1}, uint64_t(scene_count) * 3 + uint64_t(shadow_count) * shadow_passes);
            if (count > UINT32_MAX || count > instance_storage_limit)
                throw std::invalid_argument("Scene instance records exceed the device storage buffer limit");
            max_instance_records = static_cast<uint32_t>(count);
            auto& slot = frame();
            slot.instance_records.clear();
            slot.draw_commands.clear();
            slot.cull_records.clear();
            slot.draw_usage = {};
            const VkDeviceSize bytes = count * sizeof(DrawInstance);
            const bool replace_instances = slot.instances.size() < bytes;
            if (replace_instances)
                slot.instances =
                    VulkanBuffer(device, bytes, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VulkanBufferMemory::upload);
            if (scene_submission == VulkanSceneSubmission::indirect
                && slot.indirect_commands.size() < count * sizeof(VkDrawIndexedIndirectCommand))
                slot.indirect_commands = VulkanBuffer(
                    device,
                    count * sizeof(VkDrawIndexedIndirectCommand),
                    VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT,
                    VulkanBufferMemory::upload
                );
            const bool gpu = scene_submission == VulkanSceneSubmission::gpu_culled;
            if (gpu)
            {
                if (slot.visible_instances.size() < bytes)
                    slot.visible_instances =
                        VulkanBuffer(device, bytes, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VulkanBufferMemory::device);
                if (slot.culling_batches.size() < count * sizeof(CullBatch))
                    slot.culling_batches = VulkanBuffer(
                        device,
                        count * sizeof(CullBatch),
                        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                        VulkanBufferMemory::upload
                    );
                if (slot.gpu_commands.size() < count * sizeof(VkDrawIndexedIndirectCommand))
                    slot.gpu_commands = VulkanBuffer(
                        device,
                        count * sizeof(VkDrawIndexedIndirectCommand),
                        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT
                            | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                        VulkanBufferMemory::device
                    );
                const std::array buffers{
                    VkDescriptorBufferInfo{slot.instances.handle(), 0, slot.instances.size()},
                    VkDescriptorBufferInfo{slot.visible_instances.handle(), 0, slot.visible_instances.size()},
                    VkDescriptorBufferInfo{slot.culling_batches.handle(), 0, slot.culling_batches.size()},
                    VkDescriptorBufferInfo{slot.gpu_commands.handle(), 0, slot.gpu_commands.size()}
                };
                std::array<VkWriteDescriptorSet, 4> writes{};
                for (uint32_t i = 0; i < writes.size(); ++i)
                    writes[i] = {
                        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                        .dstSet = culling->sets[frame_index],
                        .dstBinding = i,
                        .descriptorCount = 1,
                        .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                        .pBufferInfo = &buffers[i]
                    };
                vkUpdateDescriptorSets(
                    device.handle(),
                    static_cast<uint32_t>(writes.size()),
                    writes.data(),
                    0,
                    nullptr
                );
            }
            const auto& vertex_instances = gpu ? slot.visible_instances : slot.instances;
            const VkDescriptorBufferInfo buffer{vertex_instances.handle(), 0, vertex_instances.size()};
            const VkWriteDescriptorSet write{
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .dstSet = instance_sets[frame_index],
                .dstBinding = 0,
                .descriptorCount = 1,
                .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                .pBufferInfo = &buffer
            };
            vkUpdateDescriptorSets(device.handle(), 1, &write, 0, nullptr);
        }

        void record_culling()
        {
            auto& slot = frame();
            if (!slot.cull_records.empty())
                slot.culling_batches.write(std::as_bytes(std::span{slot.cull_records}));
            const VkCommandBufferBeginInfo begin{
                .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
                .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
            };
            require_mesh_vk(vkBeginCommandBuffer(slot.culling_commands, &begin), "Begin scene culling commands");
            if (slot.resolution_queries)
            {
                vkCmdResetQueryPool(slot.culling_commands, slot.resolution_queries, 0, 2);
                vkCmdWriteTimestamp2(
                    slot.culling_commands,
                    VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT,
                    slot.resolution_queries,
                    0
                );
            }
            {
                RELAYCORE_PROFILE_VK_ZONE(slot.profiler, slot.culling_commands, "Scene GPU culling");
                if (!slot.cull_records.empty())
                {
                    vkCmdBindPipeline(slot.culling_commands, VK_PIPELINE_BIND_POINT_COMPUTE, culling->pipeline);
                    vkCmdBindDescriptorSets(
                        slot.culling_commands,
                        VK_PIPELINE_BIND_POINT_COMPUTE,
                        culling->layout,
                        0,
                        1,
                        &culling->sets[frame_index],
                        0,
                        nullptr
                    );
                    const auto count = static_cast<uint32_t>(slot.cull_records.size());
                    for (uint32_t first = 0; first < count;)
                    {
                        const auto groups = std::min(count - first, culling->max_dispatch_x);
                        vkCmdPushConstants(
                            slot.culling_commands,
                            culling->layout,
                            VK_SHADER_STAGE_COMPUTE_BIT,
                            0,
                            sizeof(first),
                            &first
                        );
                        vkCmdDispatch(slot.culling_commands, groups, 1, 1);
                        first += groups;
                    }
                    const VkMemoryBarrier2 barrier{
                        .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
                        .srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                        .srcAccessMask = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
                        .dstStageMask = VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT
                                        | VK_PIPELINE_STAGE_2_COPY_BIT,
                        .dstAccessMask = VK_ACCESS_2_SHADER_STORAGE_READ_BIT | VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT
                                         | VK_ACCESS_2_TRANSFER_READ_BIT
                    };
                    const VkDependencyInfo dependency{
                        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
                        .memoryBarrierCount = 1,
                        .pMemoryBarriers = &barrier
                    };
                    vkCmdPipelineBarrier2(slot.culling_commands, &dependency);
                }
            }
            require_mesh_vk(vkEndCommandBuffer(slot.culling_commands), "End scene culling commands");
            slot.culling_pending = true;
        }

        void begin_batches()
        {
            frame().batches.clear();
        }

        static auto draw_instance(const RenderItem& item, const glm::mat4& mvp, const uint32_t correspondence)
            -> DrawInstance
        {
            DrawInstance instance{.mvp = mvp, .correspondence = {correspondence, 0, 0, 0}};
            for (int row = 0; row < 3; ++row)
                for (int col = 0; col < 4; ++col)
                    instance.model_rows[row][col] = item.model_matrix[col][row];
            return instance;
        }

        void add_instance(
            const RenderItem& item,
            Mesh& geometry,
            Material* surface,
            const uint32_t flags,
            const glm::mat4& mvp,
            const uint32_t correspondence,
            const bool group
        )
        {
            auto& slot = frame();
            if (slot.instance_records.size() >= max_instance_records)
                throw std::logic_error("Scene instance recording exceeded its reserved pass count");
            const auto first = static_cast<uint32_t>(slot.instance_records.size());
            auto instance = draw_instance(item, mvp, correspondence);
            if (material_table && surface)
            {
                instance.correspondence.y = *surface->table_index;
                use(*surface);
            }
            slot.instance_records.push_back(instance);
            const BatchKey key{&geometry, surface, flags};
            // Preserve submission order even for equal-depth opaque surfaces; merge only adjacent compatible draws.
            if (group && scene_submission != VulkanSceneSubmission::direct && !slot.batches.empty()
                && slot.batches.back().key.geometry == key.geometry && slot.batches.back().key.flags == key.flags
                && (slot.batches.back().key.surface == key.surface || (material_table && surface)))
                ++slot.batches.back().count;
            else
                slot.batches.push_back({key, first});
        }

        void finish_batches()
        {
            vkCmdBindDescriptorSets(
                frame().commands,
                VK_PIPELINE_BIND_POINT_GRAPHICS,
                layout,
                2,
                1,
                &instance_sets[frame_index],
                0,
                nullptr
            );
        }

        void draw_batch(const DrawBatch& batch, const uint32_t first_instance = 0, const bool cull = false)
        {
            auto& slot = frame();
            auto& geometry = *batch.key.geometry;
            use(geometry);
            const auto vertex = geometry.vertices.handle();
            constexpr VkDeviceSize offset = 0;
            vkCmdBindVertexBuffers(slot.commands, 0, 1, &vertex, &offset);
            vkCmdBindIndexBuffer(slot.commands, geometry.indices.handle(), 0, VK_INDEX_TYPE_UINT32);
            ++slot.draw_usage.draws;
            slot.draw_usage.instances += batch.count;
            // Base instance is supplied in push constants, keeping optional indirect-first-instance disabled.
            if (scene_submission == VulkanSceneSubmission::gpu_culled)
            {
                if (slot.cull_records.size() >= max_instance_records)
                    throw std::logic_error("Scene GPU commands exceeded their reserved count");
                const auto command_offset = slot.cull_records.size() * sizeof(VkDrawIndexedIndirectCommand);
                slot.cull_records.push_back(
                    {{batch.first_instance, batch.count, geometry.index_count, cull ? 1U : 0U},
                     {geometry.mins, 0},
                     {geometry.maxs, 0}}
                );
                slot.draw_usage.culling_draws += cull ? 1 : 0;
                vkCmdDrawIndexedIndirect(
                    slot.commands,
                    slot.gpu_commands.handle(),
                    command_offset,
                    1,
                    sizeof(VkDrawIndexedIndirectCommand)
                );
                ++slot.draw_usage.indirect_draws;
            } else if (scene_submission == VulkanSceneSubmission::indirect)
            {
                if (slot.draw_commands.size() >= max_instance_records)
                    throw std::logic_error("Scene indirect commands exceeded their reserved count");
                const auto command_offset = slot.draw_commands.size() * sizeof(VkDrawIndexedIndirectCommand);
                slot.draw_commands.push_back({geometry.index_count, batch.count, 0, 0, 0});
                vkCmdDrawIndexedIndirect(
                    slot.commands,
                    slot.indirect_commands.handle(),
                    command_offset,
                    1,
                    sizeof(VkDrawIndexedIndirectCommand)
                );
                ++slot.draw_usage.indirect_draws;
            } else
                vkCmdDrawIndexed(slot.commands, geometry.index_count, batch.count, 0, 0, first_instance);
        }

        void emit_meshlet_batch(const DrawBatch& batch, DrawPush push)
        {
            auto& slot = frame();
            auto& geometry = *batch.key.geometry;
            use(geometry);
            const auto& meshlets = *geometry.meshlets;
            vkCmdBindDescriptorSets(
                slot.commands,
                VK_PIPELINE_BIND_POINT_GRAPHICS,
                layout,
                4,
                1,
                &meshlets.set,
                0,
                nullptr
            );
            const auto& caps = device.capabilities();
            const uint32_t x_limit = std::min(caps.mesh_dispatch_limit[0], caps.mesh_dispatch_total);
            const uint32_t x_count = std::min(meshlets.count, x_limit);
            const uint32_t y_limit = meshlets.count > x_limit
                                         ? 1
                                         : std::min(caps.mesh_dispatch_limit[1], caps.mesh_dispatch_total / x_count);
            slot.draw_usage.instances += batch.count;
            // Split large meshes one instance at a time to retain instance/triangle rasterization order.
            for (uint32_t instance = 0; instance < batch.count;)
            {
                const uint32_t instances = std::min(batch.count - instance, y_limit);
                push.model_rows[0].x = glm::uintBitsToFloat(batch.first_instance + instance);
                for (uint32_t first = 0; first < meshlets.count;)
                {
                    const uint32_t groups = std::min(meshlets.count - first, x_limit);
                    push.model_rows[0].y = glm::uintBitsToFloat(first);
                    vkCmdPushConstants(slot.commands, layout, draw_stages, 0, sizeof(push), &push);
                    cmd_mesh_tasks(slot.commands, groups, instances, 1);
                    ++slot.draw_usage.draws;
                    ++slot.draw_usage.mesh_draws;
                    slot.draw_usage.mesh_workgroups += size_t(groups) * instances;
                    first += groups;
                }
                instance += instances;
            }
        }

        void emit_scene_batch(
            const DrawBatch& batch,
            const DrawInstance& instance,
            const PerspectiveCamera& camera,
            const bool inputs,
            const bool mesh_requested = false
        )
        {
            const bool mesh_pass = mesh_requested && batch.key.geometry->meshlets->count > 1;
            if (mesh_requested && mesh_pipeline_bound != mesh_pass)
            {
                vkCmdBindPipeline(
                    frame().commands,
                    VK_PIPELINE_BIND_POINT_GRAPHICS,
                    mesh_pass ? meshlet_pipeline : pipelines[0]
                );
                mesh_pipeline_bound = mesh_pass;
            }

            const auto position = camera.position();
            use(*batch.key.surface);
            if (!material_table)
            {
                const auto set = batch.key.surface->sets[frame_index];
                vkCmdBindDescriptorSets(
                    frame().commands,
                    VK_PIPELINE_BIND_POINT_GRAPHICS,
                    layout,
                    0,
                    1,
                    &set,
                    0,
                    nullptr
                );
                ++frame().draw_usage.material_binds;
            }
            const bool instance_storage = mesh_pass || batch.count > 1
                                          || scene_submission == VulkanSceneSubmission::gpu_culled
                                          || (inputs && scene_submission == VulkanSceneSubmission::indirect);
            DrawPush push{
                instance.mvp,
                instance.model_rows,
                batch.key.flags | (instance_storage ? 2048U : 0U)
                    | (material_table ? *batch.key.surface->table_index << 16 : 0U),
                {position.x, position.y, position.z}
            };
            if (instance_storage)
                push.model_rows[0].x = glm::uintBitsToFloat(batch.first_instance);
            if (mesh_pass)
            {
                emit_meshlet_batch(batch, push);
                return;
            }
            vkCmdPushConstants(frame().commands, layout, draw_stages, 0, sizeof(push), &push);
            draw_batch(
                batch,
                inputs && !instance_storage ? instance.correspondence.x : 0,
                (batch.key.flags & (2U | 4U | 32U)) == 0
            );
        }

        bool has_adjacent_geometry(const std::span<const RenderItem> items, const RenderPass pass) const
        {
            const CpuColorMeshData* previous = nullptr;
            for (const auto& item : items)
                if (item.mesh && item.pass == pass)
                {
                    const auto* geometry = item.mesh->cpu_data().get();
                    if (geometry == previous)
                        return true;
                    previous = geometry;
                }
            return false;
        }

        void draw(
            const std::span<const RenderItem> items,
            const PerspectiveCamera& camera,
            const RenderPass pass,
            const bool viewmodel = false,
            const bool inputs = false,
            const uint32_t first_draw = 0,
            const bool reflection_source = false
        )
        {
            RELAYCORE_PROFILE_ZONE_N("Vulkan scene draw recording");
            const bool mesh_pass = mesh_shaders && !inputs && !viewmodel && pass == RenderPass::Opaque
                                   && scene_submission != VulkanSceneSubmission::gpu_culled;
            vkCmdBindPipeline(
                frame().commands,
                VK_PIPELINE_BIND_POINT_GRAPHICS,
                inputs ? input_pipelines[static_cast<size_t>(pass)] : pipelines[static_cast<size_t>(pass)]
            );
            mesh_pipeline_bound = false;
            if (material_table)
            {
                use(*global_material);
                const auto globals = global_material->sets[frame_index];
                const auto table = material_table->set(frame_index);
                vkCmdBindDescriptorSets(
                    frame().commands,
                    VK_PIPELINE_BIND_POINT_GRAPHICS,
                    layout,
                    0,
                    1,
                    &globals,
                    0,
                    nullptr
                );
                vkCmdBindDescriptorSets(
                    frame().commands,
                    VK_PIPELINE_BIND_POINT_GRAPHICS,
                    layout,
                    3,
                    1,
                    &table,
                    0,
                    nullptr
                );
                ++frame().draw_usage.material_binds;
            }
            const bool batching = mesh_pass || scene_submission == VulkanSceneSubmission::gpu_culled
                                  || scene_submission == VulkanSceneSubmission::indirect
                                  || (scene_submission == VulkanSceneSubmission::instanced && !viewmodel
                                      && pass == RenderPass::Opaque && has_adjacent_geometry(items, pass));
            begin_batches();
            finish_batches();
            for (size_t index = 0; index < items.size(); ++index)
            {
                const auto& item = items[index];
                if ((!viewmodel && item.pass != pass) || !item.mesh)
                    continue;
                auto& geometry = meshes.at(item.mesh->cpu_data().get());
                const auto& texture = item.diffuse_texture ? *item.diffuse_texture : atlas_source;
                auto& surface = *materials.at(material_key(
                    texture,
                    item.normal_texture,
                    item.normal_scale,
                    item.metallic_roughness,
                    item.metallic_roughness_map,
                    item.occlusion_texture,
                    item.occlusion_strength,
                    item.emissive_texture,
                    item.emissive_factor
                ));
                const uint32_t flags =
                    (item.diffuse_texture ? 1U : 0U) | (pass == RenderPass::Transparent ? 2U : 0U)
                    | (pass == RenderPass::Overlay ? 4U : 0U) | (item.normal_texture ? 8U : 0U)
                    | (item.metallic_roughness ? 16U : 0U) | (viewmodel ? 32U : 0U) | (reflection_source ? 64U : 0U)
                    | (!inputs && !reflection_source && input_view == PassInputView::ao ? 128U : 0U)
                    | (!inputs && !reflection_source && input_view == PassInputView::reflection_confidence ? 256U : 0U)
                    | (!inputs && !reflection_source && input_view == PassInputView::ssgi ? 1024U : 0U)
                    | (!inputs && !reflection_source && input_view == PassInputView::volume_transmittance ? 512U : 0U);
                const auto mvp = ((renderer_config.taa_enabled && !viewmodel && pass == RenderPass::Opaque
                                   && item.previous_model_matrix)
                                      ? current_view_projection
                                      : camera.view_projection_matrix())
                                 * item.model_matrix;
                const auto correspondence = first_draw + static_cast<uint32_t>(index);
                if (batching)
                    add_instance(
                        item,
                        geometry,
                        &surface,
                        flags,
                        mvp,
                        correspondence,
                        !viewmodel && pass == RenderPass::Opaque
                    );
                else
                    emit_scene_batch(
                        {{&geometry, &surface, flags}, 0},
                        draw_instance(item, mvp, correspondence),
                        camera,
                        inputs
                    );
            }
            for (const auto& batch : frame().batches)
                emit_scene_batch(batch, frame().instance_records[batch.first_instance], camera, inputs, mesh_pass);
        }

        void shadow_barrier(
            ShadowImage& map,
            const VkImageLayout old_layout,
            const VkImageLayout new_layout,
            const VkPipelineStageFlags2 source_stage,
            const VkAccessFlags2 source_access,
            const VkPipelineStageFlags2 dest_stage,
            const VkAccessFlags2 dest_access
        )
        {
            const VkImageMemoryBarrier2 barrier{
                .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
                .srcStageMask = source_stage,
                .srcAccessMask = source_access,
                .dstStageMask = dest_stage,
                .dstAccessMask = dest_access,
                .oldLayout = old_layout,
                .newLayout = new_layout,
                .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .image = map.image,
                .subresourceRange = {VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, map.layers}
            };
            const VkDependencyInfo info{
                .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
                .imageMemoryBarrierCount = 1,
                .pImageMemoryBarriers = &barrier
            };
            vkCmdPipelineBarrier2(frame().commands, &info);
        }
        void emit_shadow_batch(const DrawBatch& batch, const DrawInstance& instance, const LocalLight* light)
        {
            ShadowPush push{.mvp = instance.mvp, .model_rows = instance.model_rows};
            if (light)
                push.light_position_radius = {light->position, light->radius};
            if (batch.count > 1 || scene_submission == VulkanSceneSubmission::gpu_culled)
            {
                push.model_rows[0].x = glm::uintBitsToFloat(batch.first_instance);
                push.light_position_radius.w = light ? -light->radius : -1.0F;
            }
            vkCmdPushConstants(frame().commands, layout, draw_stages, 0, sizeof(push), &push);
            draw_batch(batch, 0, true);
        }

        void draw_shadow(
            const std::span<const RenderItem> items,
            ShadowImage& map,
            const uint32_t layer,
            const glm::mat4& view_projection,
            const int kind,
            const LocalLight* light = nullptr
        )
        {
            RELAYCORE_PROFILE_VK_ZONE(frame().profiler, frame().commands, "Shadow depth draws");
            const VkRenderingAttachmentInfo depth{
                .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
                .imageView = map.layer_views[layer],
                .imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
                .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
                .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
                .clearValue = {.depthStencil = {1, 0}}
            };
            const VkRenderingInfo info{
                .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
                .renderArea = {{0, 0}, map.extent},
                .layerCount = 1,
                .pDepthAttachment = &depth
            };
            // Positive height preserves GL shadow texture coordinates, including the point-face convention.
            const VkViewport viewport{0, 0, float(map.extent.width), float(map.extent.height), 0, 1};
            const VkRect2D scissor{{0, 0}, map.extent};
            vkCmdSetViewport(frame().commands, 0, 1, &viewport);
            vkCmdSetScissor(frame().commands, 0, 1, &scissor);
            vkCmdBindPipeline(frame().commands, VK_PIPELINE_BIND_POINT_GRAPHICS, shadow_pipelines[kind]);
            vkCmdBeginRendering(frame().commands, &info);
            const auto planes = shadow_frustum_planes(view_projection);
            RELAYCORE_PROFILE_ZONE_N("Vulkan shadow draw recording");
            const bool batching = scene_submission == VulkanSceneSubmission::gpu_culled
                                  || scene_submission == VulkanSceneSubmission::indirect
                                  || (scene_submission == VulkanSceneSubmission::instanced
                                      && has_adjacent_geometry(items, RenderPass::Opaque));
            begin_batches();
            finish_batches();
            for (const auto& item : items)
            {
                if (!item.mesh || item.pass != RenderPass::Opaque || !item.casts_shadow
                    || (kind == 0 && !item.casts_directional_shadow))
                    continue;
                if (light)
                {
                    const auto sphere = shadow_caster_sphere(item);
                    const glm::vec3 delta = glm::vec3(sphere) - light->position;
                    const float range = light->radius + sphere.w;
                    if (glm::dot(delta, delta) > range * range
                        || !sphere_intersects_shadow_frustum(planes, glm::vec3(sphere), sphere.w))
                        continue;
                }
                auto& geometry = meshes.at(item.mesh->cpu_data().get());
                const auto mvp = view_projection * item.model_matrix;
                if (batching)
                    add_instance(item, geometry, nullptr, 0, mvp, 0, true);
                else
                    emit_shadow_batch({{&geometry, nullptr, 0}, 0}, draw_instance(item, mvp, 0), light);
            }
            for (const auto& batch : frame().batches)
                emit_shadow_batch(batch, frame().instance_records[batch.first_instance], light);
            vkCmdEndRendering(frame().commands);
        }
        void render_shadows(const std::span<const RenderItem> items, const PerspectiveCamera& camera)
        {
            RELAYCORE_PROFILE_VK_ZONE(frame().profiler, frame().commands, "Shadows");
            const auto& config = renderer_config;
            shadow_parameters = {};
            shadow_parameters.view_projection[0] = shadow_light_view_projection(
                camera,
                config,
                config.shadow_focus_distance,
                config.shadow_projection_radius,
                config.shadow_projection_depth
            );
            shadow_parameters.view_projection[1] = shadow_light_view_projection(
                camera,
                config,
                config.shadow_far_focus_distance,
                config.shadow_far_projection_radius,
                config.shadow_far_projection_depth
            );
            shadow_parameters.view_projection[2] = spot.enabled ? spot_shadow_view_projection(spot) : glm::mat4{1};
            shadow_parameters.near_parameters =
                {config.shadow_bias_near, config.shadow_normal_offset_near, config.shadow_pcf_radius_near, 0};
            shadow_parameters.far_parameters =
                {config.shadow_bias_far, config.shadow_normal_offset_far, config.shadow_pcf_radius_far, 0};
            shadow_parameters.strengths =
                {config.shadow_strength, config.shadow_slope_bias_scale, config.shadow_contact_strength, 0};
            shadow_parameters.cascade = {config.shadow_cascade_split_distance, config.shadow_cascade_blend_range, 0, 0};
            shadow_parameters.spot_position_range = {spot.position, spot.range};
            shadow_parameters.spot_direction = {spot.direction, 0};
            shadow_parameters.spot_color = {visual_rgb_for_pipeline(spot.color, color_pipeline), 0};
            shadow_parameters.flags = {
                config.shadow_enabled && config.shadow_light_intensity > 0 ? 1 : 0,
                config.shadow_enabled ? 1 : 0,
                spot.enabled ? 1 : 0,
                0
            };
            constexpr auto stages =
                VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;
            const auto begin_map = [&](ShadowImage& map) {
                shadow_barrier(
                    map,
                    VK_IMAGE_LAYOUT_UNDEFINED,
                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                    VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
                    VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT,
                    VK_PIPELINE_STAGE_2_CLEAR_BIT,
                    VK_ACCESS_2_TRANSFER_WRITE_BIT
                );
                const VkClearDepthStencilValue clear{1, 0};
                const VkImageSubresourceRange range{VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, map.layers};
                vkCmdClearDepthStencilImage(
                    frame().commands,
                    map.image,
                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                    &clear,
                    1,
                    &range
                );
                shadow_barrier(
                    map,
                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                    VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
                    VK_PIPELINE_STAGE_2_CLEAR_BIT,
                    VK_ACCESS_2_TRANSFER_WRITE_BIT,
                    stages,
                    VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT
                );
            };
            const auto end_map = [&](ShadowImage& map) {
                shadow_barrier(
                    map,
                    VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
                    VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL,
                    stages | VK_PIPELINE_STAGE_2_CLEAR_BIT,
                    VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_2_TRANSFER_WRITE_BIT,
                    VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                    VK_ACCESS_2_SHADER_SAMPLED_READ_BIT
                );
            };
            for (size_t i = 0; i < 3; ++i)
            {
                auto& map = *shadow_maps[i];
                begin_map(map);
                if (i == 2 ? spot.enabled : shadow_parameters.flags.x != 0)
                    draw_shadow(items, map, 0, shadow_parameters.view_projection[i], i == 2 ? 1 : 0);
                end_map(map);
            }
            begin_map(*point_shadows);
            if (config.shadow_enabled)
            {
                const auto planes = shadow_frustum_planes(camera.view_projection_matrix());
                for (int i = 0; i < std::min(lights.count.x, static_cast<int>(kMaxPointShadowLights)); ++i)
                {
                    const LocalLight light{
                        glm::vec3(lights.position_radius[i]),
                        glm::vec3(lights.color[i]),
                        lights.position_radius[i].w
                    };
                    if (light.radius <= .06F || glm::length(light.color) <= .01F
                        || !sphere_intersects_shadow_frustum(planes, light.position, light.radius))
                        continue;
                    shadow_parameters.flags.w |= 1 << i;
                    for (int face = 0; face < 6; ++face)
                        draw_shadow(
                            items,
                            *point_shadows,
                            uint32_t(i * 6 + face),
                            point_shadow_view_projection(light, face),
                            2,
                            &light
                        );
                }
            }
            end_map(*point_shadows);
            frame().shadow_data.write(std::as_bytes(std::span{&shadow_parameters, 1}));
        }

        void set_config(const RendererConfig& config)
        {
            if (config.mesh_shaders != renderer_config.mesh_shaders)
                throw std::invalid_argument("Changing mesh shaders requires a prepared renderer reload");
            if (config.descriptor_indexing != renderer_config.descriptor_indexing)
                throw std::invalid_argument("Changing descriptor indexing requires a prepared renderer reload");
            if (config.dynamic_resolution_enabled != renderer_config.dynamic_resolution_enabled)
                throw std::invalid_argument("Changing dynamic resolution requires a prepared renderer reload");
            if (!std::isfinite(config.dynamic_resolution_target_ms) || config.dynamic_resolution_target_ms < 4
                || config.dynamic_resolution_target_ms > 50)
                throw std::invalid_argument("Dynamic resolution target must be finite and in [4, 50] ms");
            for (auto quality :
                 {config.ssgi_quality, config.reflection_quality, config.dof_quality, config.motion_blur_quality})
                if (quality < 0 || quality > 2)
                    throw std::invalid_argument("SSGI/reflection/lens quality must be 0, 1 or 2");
            if (!std::isfinite(config.dof_focus_distance) || config.dof_focus_distance < .25F
                || config.dof_focus_distance > 100)
                throw std::invalid_argument("DoF focus distance must be finite and in [0.25, 100]");
            if ((config.ssgi_quality || config.dof_quality || config.motion_blur_quality) && !config.hdr_enabled)
                throw std::invalid_argument("SSGI and lens effects require HDR enabled");
            if (config.reflection_quality && !config.ssr_enabled)
                throw std::invalid_argument("Improved reflections require SSR enabled");
            if (config.ssgi_quality != renderer_config.ssgi_quality
                || config.reflection_quality != renderer_config.reflection_quality
                || config.dof_quality != renderer_config.dof_quality
                || config.motion_blur_quality != renderer_config.motion_blur_quality)
                throw std::invalid_argument("Changing advanced effects requires a prepared renderer reload");
            if (config.dof_focus_distance != renderer_config.dof_focus_distance)
                history_valid = false;
            if (config.taa_enabled != renderer_config.taa_enabled)
                throw std::invalid_argument("Changing TAA requires a prepared renderer reload");
            if (!std::isfinite(config.render_scale) || config.render_scale < .5F || config.render_scale > 1)
                throw std::invalid_argument("Render scale must be finite and in [0.5, 1]");
            if ((config.taa_enabled || config.render_scale != 1 || config.dynamic_resolution_enabled)
                && !config.hdr_enabled)
                throw std::invalid_argument("TAA and scaling require HDR enabled");
            if (config.render_scale != renderer_config.render_scale
                || config.dynamic_resolution_target_ms != renderer_config.dynamic_resolution_target_ms)
            {
                history_valid = false;
                jitter_index = 0;
                resolution.reset(config.render_scale);
            }
            if (config.hdr_enabled != renderer_config.hdr_enabled)
                throw std::invalid_argument("Changing HDR requires a prepared renderer reload");
            if (config.bloom_enabled != renderer_config.bloom_enabled
                || config.ssao_enabled != renderer_config.ssao_enabled
                || config.ssr_enabled != renderer_config.ssr_enabled
                || config.volumetrics_enabled != renderer_config.volumetrics_enabled)
                throw std::invalid_argument("Changing effects requires a prepared renderer reload");
            if ((config.bloom_enabled || config.ssr_enabled) && !config.hdr_enabled)
                throw std::invalid_argument("Bloom and SSR require HDR enabled");
            if (!std::isfinite(config.bloom_strength) || config.bloom_strength < 0 || config.bloom_strength > 1
                || !std::isfinite(config.bloom_threshold) || config.bloom_threshold < 0 || config.bloom_threshold > 10
                || !std::isfinite(config.volumetric_density) || config.volumetric_density < 0
                || config.volumetric_density > .1F)
                throw std::invalid_argument("Invalid bloom strength/threshold or volumetric density");
            if (config.msaa_samples != renderer_config.msaa_samples)
                throw std::invalid_argument("Changing MSAA requires a prepared renderer reload");
            if (config.hdr_enabled && color_pipeline != ColorPipelineMode::hardware_srgb)
                throw std::invalid_argument("HDR requires the hardware_srgb color pipeline");
            if (!std::isfinite(config.exposure_ev) || config.exposure_ev < -10 || config.exposure_ev > 10)
                throw std::invalid_argument("Exposure must be finite and in [-10, 10] stops");
            renderer_config = config;
            auto& parameters = material_parameters;
            parameters.scene = {
                config.fog_enabled ? config.fog_start : config.camera.far_plane,
                config.fog_enabled ? config.fog_end : config.camera.far_plane,
                static_cast<float>(surface_material_count()),
                0
            };
            const float length = glm::length(config.shadow_light_direction);
            const glm::vec3 direction =
                length <= .0001F ? glm::normalize(glm::vec3{.28F, -1, .22F}) : config.shadow_light_direction / length;
            parameters.direction = {direction, config.shadow_light_intensity};
            parameters.directional_color = {visual_rgb_for_pipeline(config.shadow_light_color, color_pipeline), 0};
            parameters.fog = {visual_rgb_for_pipeline(config.fog_color, color_pipeline), 0};
            parameters.postfx = {
                config.postfx_vignette_strength,
                config.postfx_grain_strength,
                config.postfx_scanline_strength,
                config.postfx_contrast
            };
            parameters.grading = {config.postfx_saturation, config.hdr_enabled ? 1.0F : 0.0F, 0, 0};
            parameters.local.w = std::max(config.indirect_bounce_strength, 0.0F);
        }

        void begin_commands()
        {
            RELAYCORE_PROFILE_ZONE_N("Vulkan scene command reuse");
            auto& slot = frame();
            if (slot.command_index == slot.buffers.size())
            {
                wait_frame(slot);
                require_mesh_vk(
                    vkResetCommandPool(device.handle(), slot.pool, 0),
                    "Failed to reset scene slot commands"
                );
                slot.command_index = 0;
            }
            slot.commands = slot.buffers[slot.command_index++];
            ++recording;
            used_meshes.clear();
            used_materials.clear();
            used_meshes.reserve(meshes.size());
            used_materials.reserve(materials.size());
            const VkCommandBufferBeginInfo begin{
                .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
                .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
            };
            require_mesh_vk(vkBeginCommandBuffer(frame().commands, &begin), "Failed to begin mesh commands");
        }

        void submit_and_wait(const bool synchronous = true, const bool resolution_sample = false)
        {
            require_mesh_vk(vkEndCommandBuffer(frame().commands), "Failed to end mesh frame");
            frame().pending = submissions.submit(
                frame().commands,
                uploads.dependency(),
                {},
                VK_NULL_HANDLE,
                frame().culling_pending ? frame().culling_commands : VK_NULL_HANDLE
            );
            frame().culling_pending = false;
            frame().resolution_sample_pending = frame().resolution_sample_pending || resolution_sample;
            // Publish use only after successful submission; failed recording/prepare owns no GPU references.
            for (auto* mesh : used_meshes)
                mesh->last_use = frame().pending;
            for (auto* material : used_materials)
                material->last_use = frame().pending;
            used_meshes.clear();
            used_materials.clear();
            if (synchronous)
                wait_frame(frame());
            uploads.collect();
        }

        void input_barrier(
            MeshImage& image,
            const VkImageLayout old_layout,
            const VkImageLayout new_layout,
            const VkImageAspectFlags aspect = VK_IMAGE_ASPECT_COLOR_BIT
        )
        {
            const VkImageMemoryBarrier2 barrier{
                .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
                .srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
                .srcAccessMask = VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT,
                .dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
                .dstAccessMask = VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT,
                .oldLayout = old_layout,
                .newLayout = new_layout,
                .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .image = image.image,
                .subresourceRange = {aspect, 0, 1, 0, 1}
            };
            const VkDependencyInfo dependency{
                .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
                .imageMemoryBarrierCount = 1,
                .pImageMemoryBarriers = &barrier
            };
            vkCmdPipelineBarrier2(frame().commands, &dependency);
        }

        void set_input_view(const PassInputView view)
        {
            if (view < PassInputView::off || view > PassInputView::ssgi)
                throw std::invalid_argument("Invalid pass input view");
            if (view == PassInputView::ssgi && !renderer_config.ssgi_quality)
                throw std::invalid_argument("SSGI diagnostic requires SSGI enabled");
            if (view == PassInputView::temporal_source && !renderer_config.taa_enabled)
                throw std::invalid_argument("Temporal source diagnostic requires TAA enabled");
            if ((view == PassInputView::ao && !renderer_config.ssao_enabled)
                || (view == PassInputView::reflection_confidence && !renderer_config.ssr_enabled)
                || (view == PassInputView::volume_transmittance && !renderer_config.volumetrics_enabled))
                throw std::invalid_argument("The requested effect diagnostic requires its effect enabled");
            if (view == input_view)
                return;
            wait();
            if (view != PassInputView::off && view != PassInputView::volume_transmittance && !input_pipelines[0])
            {
                VkImageFormatProperties properties{};
                require_mesh_vk(
                    vkGetPhysicalDeviceImageFormatProperties(
                        device.physical_device(),
                        VK_FORMAT_R32G32B32A32_SFLOAT,
                        VK_IMAGE_TYPE_2D,
                        VK_IMAGE_TILING_OPTIMAL,
                        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT
                            | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
                        0,
                        &properties
                    ),
                    "Pass inputs require sampled/transfer RGBA32F attachments"
                );
                std::array<VkPipeline, 4> replacements{};
                try
                {
                    for (auto pass : {RenderPass::Opaque, RenderPass::Overlay, RenderPass::Transparent})
                        replacements[static_cast<size_t>(pass)] = create_pipeline(shader_directory, pass, -2);
                    replacements[3] = create_pipeline(shader_directory, RenderPass::Opaque, -3);
                } catch (...)
                {
                    for (auto pipeline : replacements)
                        if (pipeline)
                            vkDestroyPipeline(device.handle(), pipeline, nullptr);
                    throw;
                }
                input_pipelines = replacements;
            }
            if (view != PassInputView::off && view != PassInputView::volume_transmittance && !input_pool)
            {
                const std::array sizes{
                    VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2},
                    VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 24}
                };
                const VkDescriptorPoolCreateInfo pool{
                    .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
                    .maxSets = 2,
                    .poolSizeCount = static_cast<uint32_t>(sizes.size()),
                    .pPoolSizes = sizes.data()
                };
                require_mesh_vk(
                    vkCreateDescriptorPool(device.handle(), &pool, nullptr, &input_pool),
                    "Pass input pool"
                );
                const std::array layouts{input_layout, input_layout};
                const VkDescriptorSetAllocateInfo allocation{
                    .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
                    .descriptorPool = input_pool,
                    .descriptorSetCount = 2,
                    .pSetLayouts = layouts.data()
                };
                require_mesh_vk(
                    vkAllocateDescriptorSets(device.handle(), &allocation, input_sets.data()),
                    "Pass input sets"
                );
                const VkSamplerCreateInfo sampler{
                    .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
                    .magFilter = VK_FILTER_NEAREST,
                    .minFilter = VK_FILTER_NEAREST,
                    .mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST,
                    .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
                    .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
                    .addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE
                };
                require_mesh_vk(
                    vkCreateSampler(device.handle(), &sampler, nullptr, &input_sampler),
                    "Pass input sampler"
                );
            }
            if (view == PassInputView::off || input_view == PassInputView::off || view == PassInputView::temporal_source
                || input_view == PassInputView::temporal_source)
                for (auto& slot : frames)
                {
                    slot.target.reset();
                    slot.inputs.reset();
                    slot.previous_draws = {};
                }
            history_valid = false;
            input_view = view;
        }

        void prepare_inputs(
            const std::span<const RenderItem> items,
            const std::span<const RenderItem> viewmodel,
            const PerspectiveCamera& camera,
            const VkExtent2D extent
        )
        {
            frame_history_valid = history_valid && history_slot != frame_index && history_extent.width == extent.width
                                  && history_extent.height == extent.height
                                  && previous_projection == camera.projection_matrix()
                                  && previous_exposure == renderer_config.exposure_ev;
            if (history_extent.width != extent.width || history_extent.height != extent.height)
            {
                wait();
                for (auto& slot : frames)
                    slot.inputs.reset();
                frame_history_valid = false;
            }
            if (!frame().inputs)
                frame().inputs = std::make_unique<InputTarget>(
                    device,
                    extent,
                    depth_formats.scene,
                    scene_format(),
                    renderer_config.ssao_enabled,
                    renderer_config.taa_enabled,
                    renderer_config.ssgi_quality != 0,
                    renderer_config.dof_quality || renderer_config.motion_blur_quality
                );
            const size_t count = std::max(size_t{1}, items.size() + viewmodel.size());
            VkPhysicalDeviceProperties properties{};
            vkGetPhysicalDeviceProperties(device.physical_device(), &properties);
            if (count > std::numeric_limits<uint32_t>::max()
                || count * sizeof(PreviousDraw) > properties.limits.maxStorageBufferRange)
                throw std::invalid_argument("Pass input draw correspondence exceeds storage buffer limits");
            auto& previous = frame().previous_records;
            previous.resize(count);
            for (size_t i = 0; i < items.size(); ++i)
            {
                const auto& item = items[i];
                previous[i] = {
                    frame_history_valid
                        ? previous_unjittered_view_projection * item.previous_model_matrix.value_or(item.model_matrix)
                        : camera.view_projection_matrix() * item.model_matrix,
                    {item.previous_model_matrix ? 1.0F : 0.0F, jitter_uv.x, jitter_uv.y, 0}
                };
            }
            for (size_t i = 0; i < viewmodel.size(); ++i)
                previous[items.size() + i] = {
                    camera.view_projection_matrix() * viewmodel[i].model_matrix,
                    {0, 0, 0, 0}
                };
            const auto bytes = std::as_bytes(std::span{previous});
            if (frame().previous_draws.size() < bytes.size())
                frame().previous_draws =
                    VulkanBuffer(device, bytes.size(), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VulkanBufferMemory::upload);
            frame().previous_draws.write(bytes);
            auto& current = *frame().inputs;
            auto& old = frame_history_valid ? *frames[history_slot].inputs : current;
            const std::array images{
                VkDescriptorImageInfo{
                    input_sampler,
                    current.normal_depth.view,
                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
                },
                VkDescriptorImageInfo{input_sampler, current.velocity.view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
                VkDescriptorImageInfo{input_sampler, old.normal_depth.view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
                VkDescriptorImageInfo{
                    input_sampler,
                    frame_history_valid ? old.color_history.view : current.normal_depth.view,
                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
                },
                VkDescriptorImageInfo{input_sampler, old.velocity.view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
                VkDescriptorImageInfo{
                    input_sampler,
                    current.ao ? current.ao->view : current.normal_depth.view,
                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
                },
                VkDescriptorImageInfo{
                    input_sampler,
                    (renderer_config.ssr_enabled || renderer_config.ssgi_quality) ? current.color_history.view
                                                                                  : current.normal_depth.view,
                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
                },
                VkDescriptorImageInfo{
                    input_sampler,
                    renderer_config.taa_enabled ? frame().target->hdr_scene->view : current.normal_depth.view,
                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
                },
                VkDescriptorImageInfo{
                    input_sampler,
                    renderer_config.taa_enabled && frame_history_valid ? old.temporal->view : current.normal_depth.view,
                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
                },
                VkDescriptorImageInfo{
                    input_sampler,
                    current.gi ? current.gi->view : current.normal_depth.view,
                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
                },
                VkDescriptorImageInfo{
                    input_sampler,
                    current.gi && frame_history_valid ? old.gi->view : current.normal_depth.view,
                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
                },
                VkDescriptorImageInfo{
                    input_sampler,
                    current.lens ? resolved_source().view : current.normal_depth.view,
                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
                }
            };
            const VkDescriptorBufferInfo buffer{frame().previous_draws.handle(), 0, bytes.size()};
            std::array<VkWriteDescriptorSet, 13> writes{};
            writes[0] = {
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .dstSet = input_sets[frame_index],
                .dstBinding = 0,
                .descriptorCount = 1,
                .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                .pBufferInfo = &buffer
            };
            for (uint32_t i = 1; i < writes.size(); ++i)
                writes[i] = {
                    .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                    .dstSet = input_sets[frame_index],
                    .dstBinding = i,
                    .descriptorCount = 1,
                    .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                    .pImageInfo = &images[i - 1]
                };
            vkUpdateDescriptorSets(device.handle(), static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
        }

        void record_inputs(
            const std::span<const RenderItem> items,
            const std::span<const RenderItem> viewmodel,
            const PerspectiveCamera& camera
        )
        {
            RELAYCORE_PROFILE_VK_ZONE(frame().profiler, frame().commands, "Pass inputs");
            auto& target = *frame().inputs;
            for (auto* image : {&target.normal_depth, &target.velocity})
                input_barrier(*image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
            input_barrier(
                target.depth,
                VK_IMAGE_LAYOUT_UNDEFINED,
                VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
                VK_IMAGE_ASPECT_DEPTH_BIT
            );
            std::array<VkRenderingAttachmentInfo, 2> colors{};
            const std::array views{target.normal_depth.view, target.velocity.view};
            for (size_t i = 0; i < colors.size(); ++i)
                colors[i] = {
                    .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
                    .imageView = views[i],
                    .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                    .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
                    .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
                    .clearValue = {.color = {{0, 0, 0, 0}}}
                };
            const VkRenderingAttachmentInfo depth{
                .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
                .imageView = target.depth.view,
                .imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
                .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
                .storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
                .clearValue = {.depthStencil = {1, 0}}
            };
            const VkRenderingInfo rendering{
                .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
                .renderArea = {{0, 0}, frame().target->render_extent},
                .layerCount = 1,
                .colorAttachmentCount = 2,
                .pColorAttachments = colors.data(),
                .pDepthAttachment = &depth
            };
            vkCmdBindDescriptorSets(
                frame().commands,
                VK_PIPELINE_BIND_POINT_GRAPHICS,
                layout,
                1,
                1,
                &input_sets[frame_index],
                0,
                nullptr
            );
            vkCmdBeginRendering(frame().commands, &rendering);
            for (auto pass : {RenderPass::Opaque, RenderPass::Overlay, RenderPass::Transparent})
                draw(items, camera, pass, false, true);
            draw(viewmodel, camera, RenderPass::Opaque, true, true, static_cast<uint32_t>(items.size()));
            vkCmdEndRendering(frame().commands);
            for (auto* image : {&target.normal_depth, &target.velocity})
                input_barrier(
                    *image,
                    VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
                );
        }

        void record_color_history()
        {
            RELAYCORE_PROFILE_VK_ZONE(frame().profiler, frame().commands, "Color history");
            auto& source = frame().target->hdr_scene ? *frame().target->hdr_scene : frame().target->color;
            auto& destination = frame().inputs->color_history;
            input_barrier(source, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
            input_barrier(destination, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
            const VkImageCopy copy{
                .srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1},
                .dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1},
                .extent = {frame().target->render_extent.width, frame().target->render_extent.height, 1}
            };
            vkCmdCopyImage(
                frame().commands,
                source.image,
                VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                destination.image,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                1,
                &copy
            );
            input_barrier(source, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
            input_barrier(destination, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        }

        void record_input_view()
        {
            if (input_view == PassInputView::scene || input_view >= PassInputView::ao)
                return;
            RELAYCORE_PROFILE_VK_ZONE(frame().profiler, frame().commands, "Pass input visualizer");
            input_barrier(
                frame().target->color,
                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
            );
            const VkRenderingAttachmentInfo color{
                .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
                .imageView =
                    frame().target->color.unorm_view ? frame().target->color.unorm_view : frame().target->color.view,
                .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                .loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                .storeOp = VK_ATTACHMENT_STORE_OP_STORE
            };
            const VkRenderingInfo rendering{
                .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
                .renderArea = {{0, 0}, frame().target->extent},
                .layerCount = 1,
                .colorAttachmentCount = 1,
                .pColorAttachments = &color
            };
            const auto extent = frame().target->extent;
            const VkViewport viewport{0, 0, float(extent.width), float(extent.height), 0, 1};
            const VkRect2D scissor{{0, 0}, extent};
            vkCmdSetViewport(frame().commands, 0, 1, &viewport);
            vkCmdSetScissor(frame().commands, 0, 1, &scissor);
            vkCmdBeginRendering(frame().commands, &rendering);
            vkCmdBindPipeline(frame().commands, VK_PIPELINE_BIND_POINT_GRAPHICS, input_pipelines[3]);
            vkCmdBindDescriptorSets(
                frame().commands,
                VK_PIPELINE_BIND_POINT_GRAPHICS,
                layout,
                1,
                1,
                &input_sets[frame_index],
                0,
                nullptr
            );
            const DrawPush push{
                .mvp = glm::mat4{1},
                .model_rows = {glm::vec4{previous_jitter_uv - jitter_uv, 0, 0}, glm::vec4{}, glm::vec4{}},
                .flags = static_cast<uint32_t>(input_view),
                .camera_position =
                    {frame_history_valid ? 1.0F : 0.0F,
                     float(frame().target->extent.width),
                     float(frame().target->extent.height)}
            };
            vkCmdPushConstants(frame().commands, layout, draw_stages, 0, sizeof(push), &push);
            vkCmdDraw(frame().commands, 3, 1, 0, 0);
            vkCmdEndRendering(frame().commands);
        }

        void record_ao()
        {
            RELAYCORE_PROFILE_VK_ZONE(frame().profiler, frame().commands, "SSAO");
            auto& target = *frame().inputs->ao;
            input_barrier(target, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
            const VkRenderingAttachmentInfo color{
                .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
                .imageView = target.view,
                .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                .loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                .storeOp = VK_ATTACHMENT_STORE_OP_STORE
            };
            const VkRenderingInfo rendering{
                .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
                .renderArea = {{0, 0}, frame().target->render_extent},
                .layerCount = 1,
                .colorAttachmentCount = 1,
                .pColorAttachments = &color
            };
            vkCmdBeginRendering(frame().commands, &rendering);
            vkCmdBindPipeline(frame().commands, VK_PIPELINE_BIND_POINT_GRAPHICS, ao_pipeline);
            const auto set = (material_table ? global_material : materials.begin()->second.get())->sets[frame_index];
            vkCmdBindDescriptorSets(frame().commands, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, 0, 1, &set, 0, nullptr);
            vkCmdDraw(frame().commands, 3, 1, 0, 0);
            vkCmdEndRendering(frame().commands);
            input_barrier(target, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        }

        void record_bloom()
        {
            RELAYCORE_PROFILE_VK_ZONE(frame().profiler, frame().commands, "Bloom");
            auto extent = frame().target->render_extent;
            auto& source = tone_source();
            input_barrier(source, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
            auto& levels = tone_mapping->bloom[frame_index];
            for (size_t level = 0; level < levels.size(); ++level)
            {
                auto next = VkExtent2D{std::max(1u, (extent.width + 1) / 2), std::max(1u, (extent.height + 1) / 2)};
                auto& image = *levels[level];
                if (level == 0)
                {
                    input_barrier(image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
                    const VkRenderingAttachmentInfo color{
                        .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
                        .imageView = image.view,
                        .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                        .loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                        .storeOp = VK_ATTACHMENT_STORE_OP_STORE
                    };
                    const VkRenderingInfo rendering{
                        .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
                        .renderArea = {{0, 0}, next},
                        .layerCount = 1,
                        .colorAttachmentCount = 1,
                        .pColorAttachments = &color
                    };
                    vkCmdBeginRendering(frame().commands, &rendering);
                    vkCmdBindPipeline(frame().commands, VK_PIPELINE_BIND_POINT_GRAPHICS, tone_mapping->bloom_pipeline);
                    vkCmdBindDescriptorSets(
                        frame().commands,
                        VK_PIPELINE_BIND_POINT_GRAPHICS,
                        tone_mapping->layout,
                        0,
                        1,
                        &tone_mapping->sets[frame_index],
                        0,
                        nullptr
                    );
                    const ToneMappingPush push{
                        {},
                        {float(next.width), float(next.height), 0, 0},
                        {0, 0, renderer_config.bloom_threshold, 0}
                    };
                    vkCmdPushConstants(
                        frame().commands,
                        tone_mapping->layout,
                        VK_SHADER_STAGE_FRAGMENT_BIT | (tone_mapping->compute ? VK_SHADER_STAGE_COMPUTE_BIT : 0u),
                        0,
                        sizeof(push),
                        &push
                    );
                    const VkViewport viewport{0, 0, float(next.width), float(next.height), 0, 1};
                    const VkRect2D scissor{{0, 0}, next};
                    vkCmdSetViewport(frame().commands, 0, 1, &viewport);
                    vkCmdSetScissor(frame().commands, 0, 1, &scissor);
                    vkCmdDraw(frame().commands, 3, 1, 0, 0);
                    vkCmdEndRendering(frame().commands);
                    input_barrier(
                        image,
                        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL
                    );
                } else
                {
                    input_barrier(image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
                    const VkImageBlit region{
                        .srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1},
                        .srcOffsets = {{0, 0, 0}, {int(extent.width), int(extent.height), 1}},
                        .dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1},
                        .dstOffsets = {{0, 0, 0}, {int(next.width), int(next.height), 1}}
                    };
                    vkCmdBlitImage(
                        frame().commands,
                        levels[level - 1]->image,
                        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                        image.image,
                        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                        1,
                        &region,
                        VK_FILTER_LINEAR
                    );
                    input_barrier(image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
                    input_barrier(
                        *levels[level - 1],
                        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
                    );
                }
                extent = next;
            }
            input_barrier(
                *levels.back(),
                VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
            );
            input_barrier(source, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
        }

        void record_advanced(MeshImage& target, const VkPipeline pipeline, const bool lens)
        {
            if (lens)
                input_barrier(
                    resolved_source(),
                    VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
                );
            input_barrier(target, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
            const VkRenderingAttachmentInfo color{
                .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
                .imageView = target.view,
                .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                .loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                .storeOp = VK_ATTACHMENT_STORE_OP_STORE
            };
            const auto extent = lens ? frame().target->render_extent : frame().inputs->gi_extent;
            const VkRenderingInfo rendering{
                .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
                .renderArea = {{0, 0}, extent},
                .layerCount = 1,
                .colorAttachmentCount = 1,
                .pColorAttachments = &color
            };
            const VkViewport viewport{0, 0, float(extent.width), float(extent.height), 0, 1};
            const VkRect2D scissor{{0, 0}, extent};
            vkCmdSetViewport(frame().commands, 0, 1, &viewport);
            vkCmdSetScissor(frame().commands, 0, 1, &scissor);
            vkCmdBeginRendering(frame().commands, &rendering);
            vkCmdBindPipeline(frame().commands, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
            auto& surface = *(material_table ? global_material : materials.begin()->second.get());
            use(surface);
            const std::array sets{surface.sets[frame_index], input_sets[frame_index]};
            vkCmdBindDescriptorSets(
                frame().commands,
                VK_PIPELINE_BIND_POINT_GRAPHICS,
                layout,
                0,
                2,
                sets.data(),
                0,
                nullptr
            );
            DrawPush push{
                .mvp = glm::mat4{1},
                .model_rows = {},
                .flags = 0,
                .camera_position = {frame_history_valid ? 1.0F : 0.0F, float(extent.width), float(extent.height)}
            };
            push.model_rows[0] = {previous_jitter_uv - jitter_uv, 0, 0};
            vkCmdPushConstants(frame().commands, layout, draw_stages, 0, sizeof(push), &push);
            vkCmdDraw(frame().commands, 3, 1, 0, 0);
            vkCmdEndRendering(frame().commands);
            if (!lens)
                input_barrier(
                    target,
                    VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
                );
            else if (!renderer_config.taa_enabled)
                input_barrier(
                    resolved_source(),
                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                    VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
                );
        }

        void record_temporal()
        {
            RELAYCORE_PROFILE_VK_ZONE(frame().profiler, frame().commands, "TAA resolve");
            auto& target = *frame().inputs->temporal;
            input_barrier(
                *frame().target->hdr_scene,
                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
            );
            input_barrier(target, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
            const VkRenderingAttachmentInfo color{
                .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
                .imageView = target.view,
                .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                .loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                .storeOp = VK_ATTACHMENT_STORE_OP_STORE
            };
            const VkRenderingInfo rendering{
                .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
                .renderArea = {{0, 0}, frame().target->render_extent},
                .layerCount = 1,
                .colorAttachmentCount = 1,
                .pColorAttachments = &color
            };
            const auto extent = frame().target->render_extent;
            const VkViewport viewport{0, 0, float(extent.width), float(extent.height), 0, 1};
            const VkRect2D scissor{{0, 0}, extent};
            vkCmdSetViewport(frame().commands, 0, 1, &viewport);
            vkCmdSetScissor(frame().commands, 0, 1, &scissor);
            vkCmdBeginRendering(frame().commands, &rendering);
            vkCmdBindPipeline(frame().commands, VK_PIPELINE_BIND_POINT_GRAPHICS, temporal_pipeline);
            vkCmdBindDescriptorSets(
                frame().commands,
                VK_PIPELINE_BIND_POINT_GRAPHICS,
                layout,
                1,
                1,
                &input_sets[frame_index],
                0,
                nullptr
            );
            const DrawPush push{
                .mvp = glm::mat4{1},
                .model_rows = {},
                .flags = 0,
                .camera_position =
                    {frame_history_valid ? 1.0F : 0.0F,
                     previous_jitter_uv.x - jitter_uv.x,
                     previous_jitter_uv.y - jitter_uv.y}
            };
            vkCmdPushConstants(frame().commands, layout, draw_stages, 0, sizeof(push), &push);
            vkCmdDraw(frame().commands, 3, 1, 0, 0);
            vkCmdEndRendering(frame().commands);
            input_barrier(
                *frame().target->hdr_scene,
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
            );
            if (input_view == PassInputView::temporal_source)
                input_barrier(
                    target,
                    VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
                );
            // Tone mapping transitions the selected source to sampling. Previous-slot
            // history remains sampled until queue-ordered reuse of that slot.
        }

        void record_tone_mapping()
        {
            RELAYCORE_PROFILE_VK_ZONE(frame().profiler, frame().commands, "HDR tone mapping");
            const bool compute = tone_mapping->compute;
            const std::array barriers{
                VkImageMemoryBarrier2{
                    .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
                    .srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                    .srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                    .dstStageMask =
                        compute ? VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT : VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                    .dstAccessMask = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
                    .oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                    .newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                    .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                    .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                    .image = tone_source().image,
                    .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}
                },
                VkImageMemoryBarrier2{
                    .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
                    .srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
                    .srcAccessMask = VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT,
                    .dstStageMask = compute ? VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT
                                            : VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                    .dstAccessMask =
                        compute ? VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT : VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                    .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                    .newLayout = compute ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                    .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                    .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                    .image = frame().target->color.image,
                    .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}
                }
            };
            const VkDependencyInfo dependency{
                .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
                .imageMemoryBarrierCount = uint32_t(barriers.size()),
                .pImageMemoryBarriers = barriers.data()
            };
            vkCmdPipelineBarrier2(frame().commands, &dependency);
            const VkRenderingAttachmentInfo color{
                .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
                .imageView = frame().target->color.view,
                .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                .loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                .storeOp = VK_ATTACHMENT_STORE_OP_STORE
            };
            const VkRenderingInfo rendering{
                .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
                .renderArea = {{0, 0}, frame().target->extent},
                .layerCount = 1,
                .colorAttachmentCount = 1,
                .pColorAttachments = &color
            };
            if (!compute)
                vkCmdBeginRendering(frame().commands, &rendering);
            const auto bind_point = compute ? VK_PIPELINE_BIND_POINT_COMPUTE : VK_PIPELINE_BIND_POINT_GRAPHICS;
            vkCmdBindPipeline(frame().commands, bind_point, tone_mapping->pipeline);
            vkCmdBindDescriptorSets(
                frame().commands,
                bind_point,
                tone_mapping->layout,
                0,
                1,
                &tone_mapping->sets[frame_index],
                0,
                nullptr
            );
            const ToneMappingPush push{
                material_parameters.postfx,
                {float(frame().target->extent.width),
                 float(frame().target->extent.height),
                 scene_time,
                 renderer_config.exposure_ev},
                {material_parameters.grading.x,
                 renderer_config.bloom_enabled ? renderer_config.bloom_strength : 0,
                 renderer_config.bloom_threshold,
                 0}
            };
            vkCmdPushConstants(
                frame().commands,
                tone_mapping->layout,
                VK_SHADER_STAGE_FRAGMENT_BIT | (compute ? VK_SHADER_STAGE_COMPUTE_BIT : 0u),
                0,
                sizeof(push),
                &push
            );
            if (compute)
            {
                vkCmdDispatch(
                    frame().commands,
                    (frame().target->extent.width + 7) / 8,
                    (frame().target->extent.height + 7) / 8,
                    1
                );
                const VkImageMemoryBarrier2 ready{
                    .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
                    .srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                    .srcAccessMask = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
                    .dstStageMask =
                        VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT,
                    .dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT
                                     | VK_ACCESS_2_TRANSFER_READ_BIT,
                    .oldLayout = VK_IMAGE_LAYOUT_GENERAL,
                    .newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                    .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                    .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                    .image = frame().target->color.image,
                    .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}
                };
                const VkDependencyInfo ready_info{
                    .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
                    .imageMemoryBarrierCount = 1,
                    .pImageMemoryBarriers = &ready
                };
                vkCmdPipelineBarrier2(frame().commands, &ready_info);
            } else
            {
                const VkViewport
                    viewport{0, 0, float(frame().target->extent.width), float(frame().target->extent.height), 0, 1};
                const VkRect2D scissor{{0, 0}, frame().target->extent};
                vkCmdSetViewport(frame().commands, 0, 1, &viewport);
                vkCmdSetScissor(frame().commands, 0, 1, &scissor);
                vkCmdDraw(frame().commands, 3, 1, 0, 0);
                vkCmdEndRendering(frame().commands);
            }
        }

        void record_ui(ImDrawData* ui)
        {
            RELAYCORE_PROFILE_VK_ZONE(frame().profiler, frame().commands, "ImGui");
            const VkRenderingAttachmentInfo color{
                .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
                .imageView =
                    frame().target->color.unorm_view ? frame().target->color.unorm_view : frame().target->color.view,
                .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                .loadOp = VK_ATTACHMENT_LOAD_OP_LOAD,
                .storeOp = VK_ATTACHMENT_STORE_OP_STORE
            };
            const VkRenderingInfo rendering{
                .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
                .renderArea = {{0, 0}, frame().target->extent},
                .layerCount = 1,
                .colorAttachmentCount = 1,
                .pColorAttachments = &color
            };
            vkCmdBeginRendering(frame().commands, &rendering);
            ImGui_ImplVulkan_RenderDrawData(ui, frame().commands);
            vkCmdEndRendering(frame().commands);
        }

        void append_ui(ImDrawData* ui, PlayerUi* player_ui = nullptr)
        {
            if (!frame().target)
                throw std::logic_error("Vulkan UI requires a prepared scene");
            if (!ui && !player_ui)
                return;
            begin_commands();
            VkImageMemoryBarrier2 barrier{
                .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
                .srcStageMask = VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT,
                .srcAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT,
                .dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                .dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                .newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .image = frame().target->color.image,
                .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}
            };
            const VkDependencyInfo dependency{
                .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
                .imageMemoryBarrierCount = 1,
                .pImageMemoryBarriers = &barrier
            };
            vkCmdPipelineBarrier2(frame().commands, &dependency);
            if (player_ui)
            {
                RELAYCORE_PROFILE_VK_ZONE(frame().profiler, frame().commands, "RmlUi");
                const VkRenderingAttachmentInfo color{
                    .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
                    .imageView = frame().target->color.unorm_view ? frame().target->color.unorm_view
                                                                  : frame().target->color.view,
                    .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                    .loadOp = VK_ATTACHMENT_LOAD_OP_LOAD,
                    .storeOp = VK_ATTACHMENT_STORE_OP_STORE
                };
                const VkRenderingInfo rendering{
                    .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
                    .renderArea = {{0, 0}, frame().target->extent},
                    .layerCount = 1,
                    .colorAttachmentCount = 1,
                    .pColorAttachments = &color
                };
                vkCmdBeginRendering(frame().commands, &rendering);
                player_ui->record(frame().commands);
                vkCmdEndRendering(frame().commands);
            } else
                record_ui(ui);
            barrier.srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
            barrier.srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
            barrier.dstStageMask = VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT;
            barrier.dstAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT;
            barrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            vkCmdPipelineBarrier2(frame().commands, &dependency);
            if (player_ui)
                frame().ui_resources.push_back(player_ui->retain_render_resources());
            submit_and_wait(!player_ui);
        }

        void record_readback()
        {
            const VkBufferImageCopy copy{
                .imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1},
                .imageExtent = {frame().target->extent.width, frame().target->extent.height, 1}
            };
            vkCmdCopyImageToBuffer(
                frame().commands,
                frame().target->color.image,
                VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                frame().target->readback.handle(),
                1,
                &copy
            );
            const VkMemoryBarrier2 host{
                .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
                .srcStageMask = VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT,
                .srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
                .dstStageMask = VK_PIPELINE_STAGE_2_HOST_BIT,
                .dstAccessMask = VK_ACCESS_2_HOST_READ_BIT
            };
            const VkDependencyInfo host_info{
                .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
                .memoryBarrierCount = 1,
                .pMemoryBarriers = &host
            };
            vkCmdPipelineBarrier2(frame().commands, &host_info);
        }

        auto capture_shadow_depth(size_t cascade) -> std::vector<float>
        {
            if (cascade >= 2 || !frame().target)
                throw std::logic_error("Shadow capture requires a prepared directional cascade");
            const auto& map = *shadow_maps[cascade];
            VulkanBuffer readback{
                device,
                VkDeviceSize(map.extent.width) * map.extent.height
                    * (depth_formats.shadow == VK_FORMAT_D16_UNORM ? 2 : 4),
                VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                VulkanBufferMemory::readback
            };
            begin_commands();
            VkImageMemoryBarrier2 barrier{
                .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
                .srcStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                .srcAccessMask = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
                .dstStageMask = VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT,
                .dstAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT,
                .oldLayout = VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL,
                .newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .image = map.image,
                .subresourceRange = {VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1}
            };
            const VkDependencyInfo dependency{
                .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
                .imageMemoryBarrierCount = 1,
                .pImageMemoryBarriers = &barrier
            };
            vkCmdPipelineBarrier2(frame().commands, &dependency);
            const VkBufferImageCopy copy{
                .imageSubresource = {VK_IMAGE_ASPECT_DEPTH_BIT, 0, 0, 1},
                .imageExtent = {map.extent.width, map.extent.height, 1}
            };
            vkCmdCopyImageToBuffer(
                frame().commands,
                map.image,
                VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                readback.handle(),
                1,
                &copy
            );
            barrier.srcStageMask = VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT;
            barrier.srcAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT;
            barrier.dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
            barrier.dstAccessMask = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT;
            barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            barrier.newLayout = VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL;
            vkCmdPipelineBarrier2(frame().commands, &dependency);
            const VkMemoryBarrier2 host{
                .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
                .srcStageMask = VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT,
                .srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
                .dstStageMask = VK_PIPELINE_STAGE_2_HOST_BIT,
                .dstAccessMask = VK_ACCESS_2_HOST_READ_BIT
            };
            const VkDependencyInfo host_info{
                .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
                .memoryBarrierCount = 1,
                .pMemoryBarriers = &host
            };
            vkCmdPipelineBarrier2(frame().commands, &host_info);
            submit_and_wait();
            std::vector<float> output(size_t(map.extent.width) * map.extent.height);
            if (depth_formats.shadow == VK_FORMAT_D32_SFLOAT)
                readback.read(std::as_writable_bytes(std::span{output}));
            else if (depth_formats.shadow == VK_FORMAT_D16_UNORM)
            {
                std::vector<uint16_t> packed(output.size());
                readback.read(std::as_writable_bytes(std::span{packed}));
                std::transform(packed.begin(), packed.end(), output.begin(), [](uint16_t depth) {
                    return float(depth) / 65535.0F;
                });
            } else
            {
                std::vector<uint32_t> packed(output.size());
                readback.read(std::as_writable_bytes(std::span{packed}));
                std::transform(packed.begin(), packed.end(), output.begin(), [](uint32_t depth) {
                    return float(depth & 0xffffffu) / float(0xffffffu);
                });
            }
            return output;
        }

        auto capture_prepared_image() -> CpuRgbaTextureData
        {
            if (!frame().target)
                throw std::logic_error("Vulkan capture requires a prepared scene");
            begin_commands();
            record_readback();
            submit_and_wait();
            CpuRgbaTextureData output{int(frame().target->extent.width), int(frame().target->extent.height), {}};
            output.pixels.resize(size_t(output.width) * output.height * 4);
            frame().target->readback.read(std::as_writable_bytes(std::span{output.pixels}));
            return output;
        }

        auto render(
            const std::span<const RenderItem> items,
            const PerspectiveCamera& camera,
            const int width,
            const int height,
            const std::span<const RenderItem> viewmodel,
            const bool capture,
            ImDrawData* ui,
            const std::span<const RenderItem> shadow_casters
        ) -> CpuRgbaTextureData
        {
            if (width <= 0 || height <= 0)
                throw std::invalid_argument("Vulkan mesh target requires positive dimensions");
            if (static_cast<uint32_t>(width) > max_target_width || static_cast<uint32_t>(height) > max_target_height)
                throw std::invalid_argument("Vulkan mesh target exceeds the device framebuffer/image limits");
            select_frame(!needs_inputs() && (capture || ui));
            const float scale =
                renderer_config.dynamic_resolution_enabled ? resolution.scale : renderer_config.render_scale;
            frame().rendered_scale = scale;
            const VkExtent2D extent{
                std::max(1u, static_cast<uint32_t>(std::lround(width * scale))),
                std::max(1u, static_cast<uint32_t>(std::lround(height * scale)))
            };
            const VkExtent2D display_extent{static_cast<uint32_t>(width), static_cast<uint32_t>(height)};
            if (!history_valid || history_extent.width != extent.width || history_extent.height != extent.height
                || previous_projection != camera.projection_matrix()
                || previous_exposure != renderer_config.exposure_ev)
                jitter_index = 0;
            jitter_uv = {};
            if (renderer_config.taa_enabled)
            {
                constexpr std::array<glm::vec2, 8> halton{
                    {{0, -1.0F / 6},
                     {-.25F, 1.0F / 6},
                     {.25F, -7.0F / 18},
                     {-.375F, -1.0F / 18},
                     {.125F, 5.0F / 18},
                     {-.125F, -5.0F / 18},
                     {.375F, 1.0F / 18},
                     {-.4375F, 7.0F / 18}}
                };
                jitter_uv = halton[jitter_index % halton.size()] / glm::vec2{extent.width, extent.height};
            }
            glm::mat4 jitter{1};
            jitter[3][0] = 2 * jitter_uv.x;
            jitter[3][1] = -2 * jitter_uv.y;
            current_view_projection = jitter * camera.view_projection_matrix();
            frame().material_data.write(std::as_bytes(std::span{&material_parameters, 1}));
            frame().light_data.write(std::as_bytes(std::span{&lights, 1}));
            IrradianceParameters indirect{};
            if (irradiance)
            {
                indirect.mins = {irradiance->settings.mins, 0};
                indirect.maxs = {irradiance->settings.maxs, 0};
                indirect.dimensions = {irradiance->settings.dimensions, 1};
                for (std::size_t probe = 0; probe < irradiance->probes.size(); ++probe)
                    for (std::size_t sh = 0; sh < 9; ++sh)
                        indirect.coefficients[probe * 9 + sh] = {irradiance->probes[probe][sh], 0};
            }
            frame().irradiance_data.write(std::as_bytes(std::span{&indirect, 1}));
            const FrameParameters frame_parameters{
                {float(extent.width),
                 float(extent.height),
                 scene_time,
                 renderer_config.shadow_debug_cascades ? 1.0F : 0.0F},
                current_view_projection,
                glm::inverse(current_view_projection),
                camera.view_matrix(),
                {camera.position(), 0},
                {renderer_config.ssao_enabled ? 1.0F : 0.0F,
                 renderer_config.ssr_enabled ? 1.0F : 0.0F,
                 renderer_config.volumetrics_enabled ? renderer_config.volumetric_density : 0.0F,
                 float(renderer_config.reflection_quality)},
                {float(renderer_config.ssgi_quality),
                 float(renderer_config.dof_quality),
                 float(renderer_config.motion_blur_quality),
                 renderer_config.dof_focus_distance}
            };
            frame().frame_data.write(std::as_bytes(std::span{&frame_parameters, 1}));
            collect_expired_resources();
            prepare(items);
            if ((renderer_config.ssao_enabled || renderer_config.ssgi_quality || renderer_config.dof_quality
                 || renderer_config.motion_blur_quality)
                && materials.empty())
                static_cast<void>(material(atlas_source, nullptr, 1, {}, nullptr, nullptr, 1, nullptr, {}));
            // Opaque shadow pipelines consume geometry only.
            for (const auto& caster : shadow_casters)
                if (caster.mesh)
                    static_cast<void>(mesh(*caster.mesh));
            prepare(viewmodel);
            uploads.collect();
            if (material_table)
                material_table->update(frame_index);
            const bool replace_target = !frame().target || frame().target->extent.width != display_extent.width
                                        || frame().target->extent.height != display_extent.height
                                        || frame().target->render_extent.width != extent.width
                                        || frame().target->render_extent.height != extent.height;
            const bool replace_inputs =
                needs_inputs()
                && (!frame().inputs || history_extent.width != extent.width || history_extent.height != extent.height);
            if (replace_target)
            {
                frame().target = std::make_unique<MeshTarget>(
                    device,
                    display_extent,
                    extent,
                    scene_format(),
                    display_format(),
                    depth_formats.scene,
                    static_cast<VkSampleCountFlagBits>(renderer_config.msaa_samples),
                    tone_mapping && tone_mapping->compute,
                    needs_inputs()
                );
            }
            if (needs_inputs())
            {
                prepare_inputs(items, viewmodel, camera, extent);
                history_valid = false;
            }
            if (tone_mapping && (replace_target || replace_inputs))
            {
                const auto source_view = tone_source().view;
                tone_mapping->bind(
                    source_view,
                    frame().target->color.unorm_view,
                    frame_index,
                    extent,
                    renderer_config.bloom_enabled
                );
            }
            CpuRgbaTextureData output{width, height, {}};
            if (capture)
                output.pixels.resize(size_t(width) * height * 4);
            auto& scene_image = frame().target->hdr_scene ? *frame().target->hdr_scene : frame().target->color;
            prepare_draw_storage(items.size() + viewmodel.size(), shadow_casters.size());
            begin_commands();
            {
                RELAYCORE_PROFILE_VK_ZONE(frame().profiler, frame().commands, "Scene frame");
                if (frame().resolution_queries && scene_submission != VulkanSceneSubmission::gpu_culled)
                {
                    vkCmdResetQueryPool(frame().commands, frame().resolution_queries, 0, 2);
                    vkCmdWriteTimestamp2(
                        frame().commands,
                        VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT,
                        frame().resolution_queries,
                        0
                    );
                }
                render_shadows(shadow_casters, camera);
                std::array<VkImageMemoryBarrier2, 3> barriers{
                    VkImageMemoryBarrier2{
                        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
                        .srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
                        .srcAccessMask = VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT,
                        .dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                        .dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                        .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                        .newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                        .image = scene_image.image,
                        .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}
                    },
                    VkImageMemoryBarrier2{
                        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
                        .srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
                        .srcAccessMask = VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT,
                        .dstStageMask =
                            VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
                        .dstAccessMask = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT
                                         | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                        .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                        .newLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
                        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                        .image = frame().target->depth.image,
                        .subresourceRange = {VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1}
                    },
                };
                if (frame().target->multisample_scene)
                {
                    barriers[2] = barriers[0];
                    barriers[2].image = frame().target->multisample_scene->image;
                }
                VkDependencyInfo dependency{
                    .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
                    .imageMemoryBarrierCount = frame().target->multisample_scene ? 3u : 2u,
                    .pImageMemoryBarriers = barriers.data()
                };
                vkCmdPipelineBarrier2(frame().commands, &dependency);
                const auto clear_rgb = visual_rgb_for_pipeline(
                    {renderer_config.clear_r, renderer_config.clear_g, renderer_config.clear_b},
                    color_pipeline
                );
                VkRenderingAttachmentInfo color{
                    .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
                    .imageView =
                        frame().target->multisample_scene ? frame().target->multisample_scene->view : scene_image.view,
                    .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                    .resolveMode =
                        frame().target->multisample_scene ? VK_RESOLVE_MODE_AVERAGE_BIT : VK_RESOLVE_MODE_NONE,
                    .resolveImageView = frame().target->multisample_scene ? scene_image.view : VK_NULL_HANDLE,
                    .resolveImageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                    .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
                    .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
                    .clearValue = {.color = {{clear_rgb.x, clear_rgb.y, clear_rgb.z, 1}}}
                };
                const VkRenderingAttachmentInfo depth{
                    .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
                    .imageView = frame().target->depth.view,
                    .imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
                    .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
                    .storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
                    .clearValue = {.depthStencil = {1, 0}}
                };
                const VkRenderingInfo rendering{
                    .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
                    .renderArea = {{0, 0}, extent},
                    .layerCount = 1,
                    .colorAttachmentCount = 1,
                    .pColorAttachments = &color,
                    .pDepthAttachment = &depth
                };
                // Negative viewport height maps the existing Y-up camera to top-down Vulkan images without changing winding.
                const VkViewport viewport{
                    0,
                    static_cast<float>(extent.height),
                    static_cast<float>(extent.width),
                    -static_cast<float>(extent.height),
                    0,
                    1
                };
                const VkRect2D scissor{{0, 0}, extent};
                vkCmdSetViewport(frame().commands, 0, 1, &viewport);
                vkCmdSetScissor(frame().commands, 0, 1, &scissor);
                if (needs_inputs())
                    record_inputs(items, viewmodel, camera);
                if (renderer_config.ssao_enabled)
                    record_ao();
                if (renderer_config.ssr_enabled || renderer_config.ssgi_quality)
                {
                    RELAYCORE_PROFILE_VK_ZONE(frame().profiler, frame().commands, "SSR source");
                    auto& history = frame().inputs->color_history;
                    input_barrier(history, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
                    const VkClearColorValue black{{0, 0, 0, 0}};
                    const VkImageSubresourceRange range{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
                    vkCmdClearColorImage(
                        frame().commands,
                        history.image,
                        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                        &black,
                        1,
                        &range
                    );
                    input_barrier(
                        history,
                        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
                    );
                    if (frame().inputs->gi)
                        input_barrier(
                            *frame().inputs->gi,
                            VK_IMAGE_LAYOUT_UNDEFINED,
                            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
                        );
                    vkCmdBeginRendering(frame().commands, &rendering);
                    draw(items, camera, RenderPass::Opaque, false, false, 0, true);
                    vkCmdEndRendering(frame().commands);
                    record_color_history();
                    input_barrier(
                        frame().target->depth,
                        VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
                        VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
                        VK_IMAGE_ASPECT_DEPTH_BIT
                    );
                    if (frame().target->multisample_scene)
                        input_barrier(
                            *frame().target->multisample_scene,
                            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
                        );
                }
                if (renderer_config.ssgi_quality)
                {
                    RELAYCORE_PROFILE_VK_ZONE(frame().profiler, frame().commands, "SSGI");
                    record_advanced(*frame().inputs->gi, gi_pipeline, false);
                    vkCmdSetViewport(frame().commands, 0, 1, &viewport);
                    vkCmdSetScissor(frame().commands, 0, 1, &scissor);
                }
                vkCmdBeginRendering(frame().commands, &rendering);
                {
                    RELAYCORE_PROFILE_VK_ZONE(frame().profiler, frame().commands, "World passes");
                    for (const auto pass : {RenderPass::Opaque, RenderPass::Overlay, RenderPass::Transparent})
                        draw(items, camera, pass);
                }
                vkCmdEndRendering(frame().commands);
                if (!viewmodel.empty())
                {
                    RELAYCORE_PROFILE_VK_ZONE(frame().profiler, frame().commands, "Viewmodel");
                    VkMemoryBarrier2 reuse{
                        .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
                        .srcStageMask = VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT,
                        .srcAccessMask =
                            VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                        .dstStageMask = VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT,
                        .dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT
                                         | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT
                                         | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT
                    };
                    const VkDependencyInfo reuse_info{
                        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
                        .memoryBarrierCount = 1,
                        .pMemoryBarriers = &reuse
                    };
                    vkCmdPipelineBarrier2(frame().commands, &reuse_info);
                    color.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
                    vkCmdBeginRendering(frame().commands, &rendering);
                    draw(viewmodel, camera, RenderPass::Opaque, true);
                    vkCmdEndRendering(frame().commands);
                }
                if (input_view != PassInputView::off && input_view != PassInputView::volume_transmittance)
                    record_color_history();
                if (renderer_config.taa_enabled)
                    record_temporal();
                if (renderer_config.dof_quality || renderer_config.motion_blur_quality)
                {
                    RELAYCORE_PROFILE_VK_ZONE(frame().profiler, frame().commands, "Lens effects");
                    record_advanced(*frame().inputs->lens, lens_pipeline, true);
                }
                if (renderer_config.bloom_enabled)
                    record_bloom();
                if (tone_mapping)
                    record_tone_mapping();
                if (input_view != PassInputView::off)
                    record_input_view();
                if (frame().resolution_queries)
                    vkCmdWriteTimestamp2(
                        frame().commands,
                        VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT,
                        frame().resolution_queries,
                        1
                    );
                if (ui)
                {
                    RELAYCORE_PROFILE_VK_ZONE(frame().profiler, frame().commands, "ImGui");
                    const VkMemoryBarrier2 reuse{
                        .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
                        .srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                        .srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                        .dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                        .dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT
                    };
                    const VkDependencyInfo reuse_info{
                        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
                        .memoryBarrierCount = 1,
                        .pMemoryBarriers = &reuse
                    };
                    vkCmdPipelineBarrier2(frame().commands, &reuse_info);
                    record_ui(ui);
                }
                auto& transfer = barriers[0];
                transfer.image = frame().target->color.image;
                transfer.srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
                transfer.srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
                transfer.dstStageMask = VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT;
                transfer.dstAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT;
                transfer.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
                transfer.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
                dependency.imageMemoryBarrierCount = 1;
                vkCmdPipelineBarrier2(frame().commands, &dependency);
                if (capture)
                    record_readback();
            }
            if (!frame().instance_records.empty())
                frame().instances.write(std::as_bytes(std::span{frame().instance_records}));
            if (!frame().draw_commands.empty())
                frame().indirect_commands.write(std::as_bytes(std::span{frame().draw_commands}));
            if (scene_submission == VulkanSceneSubmission::gpu_culled)
                record_culling();
            submit_and_wait(capture || ui, frame().resolution_queries != VK_NULL_HANDLE);
            if (needs_inputs())
            {
                previous_projection = camera.projection_matrix();
                previous_unjittered_view_projection = camera.view_projection_matrix();
                previous_jitter_uv = jitter_uv;
                previous_exposure = renderer_config.exposure_ev;
                history_extent = extent;
                history_slot = frame_index;
                history_valid = true;
                ++jitter_index;
            }
            if (capture)
                frame().target->readback.read(std::as_writable_bytes(std::span{output.pixels}));
            return output;
        }
    };

    VulkanMeshRenderer::VulkanMeshRenderer(
        const VulkanDevice& device,
        const std::filesystem::path& shader_directory,
        const RenderAuthoringConfig& authoring,
        const CpuRgbaTextureData& atlas,
        const TextureEncoding encoding,
        const RendererConfig& config,
        const ColorPipelineMode pipeline,
        const VulkanDepthFormats depth_formats
    ) : state_(std::make_unique<State>(device))
    {
        state_->initialize(authoring, atlas, encoding, config, pipeline, depth_formats);
        reload_shaders(shader_directory);
        if (config.ssao_enabled || config.ssr_enabled || config.taa_enabled || config.ssgi_quality || config.dof_quality
            || config.motion_blur_quality)
        {
            state_->set_input_view(PassInputView::scene);
            state_->input_view = PassInputView::off;
        }
        if (const auto* value = std::getenv("RELAYCORE_VULKAN_SCENE_SUBMISSION"))
        {
            const std::array names{"direct", "instanced", "indirect", "gpu_culled"};
            const auto found = std::find(names.begin(), names.end(), std::string_view{value});
            if (found == names.end())
                throw std::invalid_argument(
                    "RELAYCORE_VULKAN_SCENE_SUBMISSION must be direct, instanced, indirect or gpu_culled"
                );
            set_scene_submission(static_cast<VulkanSceneSubmission>(found - names.begin()));
        }
        if (const auto* value = std::getenv("RELAYCORE_VULKAN_PASS_INPUT_VIEW"))
        {
            const std::array names{
                "off",
                "scene",
                "depth",
                "normals",
                "velocity",
                "history",
                "previous_color",
                "ao",
                "reflection_confidence",
                "volume_transmittance",
                "temporal_source",
                "ssgi"
            };
            const auto found = std::find(names.begin(), names.end(), std::string_view{value});
            if (found == names.end())
                throw std::invalid_argument("RELAYCORE_VULKAN_PASS_INPUT_VIEW must be off, scene, depth, normals, "
                                            "velocity, history, previous_color, ao, reflection_confidence, "
                                            "volume_transmittance, temporal_source or ssgi");
            set_pass_input_view(static_cast<PassInputView>(found - names.begin()));
        }
    }

    auto VulkanMeshRenderer::resource_usage() const noexcept -> VulkanResourceUsage
    {
        const auto uploads = state_->uploads.memory_usage();
        return {
            .staging_bytes = uploads.staging_bytes,
            .upload_batches = uploads.batches,
            .upload_retirements = uploads.retirements,
            .scene_retirements = state_->submissions.pending_retirements(),
            .material_retirements = state_->retired_materials.size()
        };
    }
    void VulkanMeshRenderer::set_scene_submission(const VulkanSceneSubmission mode) noexcept
    {
        state_->scene_submission =
            mode == VulkanSceneSubmission::gpu_culled && !state_->culling ? VulkanSceneSubmission::indirect : mode;
    }

    auto VulkanMeshRenderer::supports_gpu_culling() const noexcept -> bool
    {
        return state_->culling != nullptr;
    }

    auto VulkanMeshRenderer::scene_submission() const noexcept -> VulkanSceneSubmission
    {
        return state_->scene_submission;
    }

    auto VulkanMeshRenderer::draw_usage() const noexcept -> VulkanDrawUsage
    {
        return state_->frame().draw_usage;
    }

    auto VulkanMeshRenderer::capture_draw_usage() -> VulkanDrawUsage
    {
        auto& slot = state_->frame();
        auto usage = slot.draw_usage;
        if (slot.cull_records.empty())
            return usage;
        state_->wait_frame(slot);
        std::vector<VkDrawIndexedIndirectCommand> commands(slot.cull_records.size());
        VulkanBuffer readback{
            state_->device,
            commands.size() * sizeof(commands.front()),
            VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VulkanBufferMemory::readback
        };
        state_->begin_commands();
        const VkBufferCopy copy{0, 0, readback.size()};
        vkCmdCopyBuffer(slot.commands, slot.gpu_commands.handle(), readback.handle(), 1, &copy);
        state_->submit_and_wait();
        readback.read(std::as_writable_bytes(std::span{commands}));
        usage.instances = 0;
        for (size_t i = 0; i < commands.size(); ++i)
        {
            const auto& command = commands[i];
            const auto& batch = slot.cull_records[i].range;
            if (command.indexCount != batch.z || command.instanceCount > batch.y || command.firstIndex
                || command.vertexOffset || command.firstInstance)
                throw std::runtime_error("GPU scene culling produced an invalid indirect command");
            usage.instances += command.instanceCount;
        }
        usage.culled_instances = slot.draw_usage.instances - usage.instances;
        return usage;
    }

    auto VulkanMeshRenderer::resolution_status() const noexcept -> VulkanResolutionStatus
    {
        const auto& slot = state_->frame();
        return {
            slot.target ? int(slot.target->render_extent.width) : 0,
            slot.target ? int(slot.target->render_extent.height) : 0,
            slot.rendered_scale,
            state_->gpu_scene_ms
        };
    }
    VulkanMeshRenderer::~VulkanMeshRenderer() noexcept = default;

    auto VulkanMeshRenderer::material_descriptor_usage() const noexcept -> VulkanMaterialDescriptorUsage
    {
        VulkanMaterialDescriptorUsage usage{.pools = state_->material_pools.size()};
        for (const auto& pool : state_->material_pools)
            usage.materials += pool->materials;
        if (state_->material_table)
        {
            usage.materials = state_->materials.size() + state_->retired_materials.size();
            usage.indexed_capacity = state_->material_table->capacity();
        }
        return usage;
    }

    auto VulkanMeshRenderer::uses_mesh_shaders() const noexcept -> bool
    {
        return state_->mesh_shaders;
    }

    auto VulkanMeshRenderer::uses_descriptor_indexing() const noexcept -> bool
    {
        return state_->material_table != nullptr;
    }

    void VulkanMeshRenderer::prepare_resources(const std::span<const RenderItem> items)
    {
        RELAYCORE_PROFILE_ZONE_N("Vulkan resource preparation");
        state_->wait();
        state_->collect_expired_resources();
        state_->prepare(items);
        state_->uploads.collect();
    }

    auto VulkanMeshRenderer::shadow_preview(size_t cascade) -> uintptr_t
    {
        if (cascade >= state_->shadow_previews.size())
            throw std::out_of_range("Directional shadow preview cascade index");
        auto& descriptor = state_->shadow_previews[cascade];
        const auto& map = *state_->shadow_maps[cascade];
        if (!descriptor)
            descriptor = ImGui_ImplVulkan_AddTexture(map.sampler, map.view, VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL);
        return reinterpret_cast<uintptr_t>(descriptor);
    }
    auto VulkanMeshRenderer::capture_shadow_depth(size_t cascade) -> std::vector<float>
    {
        return state_->capture_shadow_depth(cascade);
    }

    void VulkanMeshRenderer::set_config(const RendererConfig& config)
    {
        state_->set_config(config);
    }
    void VulkanMeshRenderer::prepare_frame(
        std::span<const RenderItem> items,
        std::span<const RenderItem> shadow_casters,
        const PerspectiveCamera& camera,
        int width,
        int height,
        std::span<const RenderItem> viewmodel
    )
    {
        static_cast<void>(state_->render(items, camera, width, height, viewmodel, false, nullptr, shadow_casters));
    }
    void VulkanMeshRenderer::append_ui(ImDrawData* ui)
    {
        state_->append_ui(ui);
    }

    void VulkanMeshRenderer::append_player_ui(PlayerUi& ui)
    {
        const auto extent = state_->frame().target->extent;
        ui.prepare(int(extent.width), int(extent.height));
        state_->append_ui(nullptr, &ui);
    }
    auto VulkanMeshRenderer::capture_prepared_image() -> CpuRgbaTextureData
    {
        return state_->capture_prepared_image();
    }
    void VulkanMeshRenderer::submit_presentation(
        const VkCommandBuffer commands,
        const VkSemaphoreSubmitInfo acquired,
        const VkSemaphoreSubmitInfo rendered,
        const VkFence fence
    )
    {
        // Copy completion extends this slot's source-image and UI-resource lifetime.
        state_->frame().pending = state_->submissions.submit(commands, acquired, rendered, fence);
    }
    auto VulkanMeshRenderer::prepared_image(VkDevice device) const -> VkImage
    {
        if (device != state_->device.handle() || !state_->frame().target)
            throw std::logic_error("Vulkan presentation requires a prepared image on the same device");
        return state_->frame().target->color.image;
    }
    auto VulkanMeshRenderer::prepared_extent() const -> VkExtent2D
    {
        return state_->frame().target ? state_->frame().target->extent : VkExtent2D{};
    }

    void VulkanMeshRenderer::set_reflection_probe(std::shared_ptr<const ReflectionProbe> probe)
    {
        state_->update_reflection(std::move(probe));
    }

    void VulkanMeshRenderer::set_irradiance_probes(std::shared_ptr<const IrradianceProbeVolume> probes)
    {
        if (probes)
            validate_irradiance_volume(*probes);
        state_->irradiance = std::move(probes);
    }

    void VulkanMeshRenderer::set_local_lights(const std::span<const LocalLight> lights)
    {
        state_->lights = {};
        const auto count = std::min(lights.size(), state_->lights.position_radius.size());
        state_->lights.count.x = static_cast<int>(count);
        for (size_t i = 0; i < count; ++i)
        {
            state_->lights.position_radius[i] = {lights[i].position, lights[i].radius};
            state_->lights.color[i] = {lights[i].color, 0};
        }
    }

    void VulkanMeshRenderer::set_spot_light(const SpotLight& light) noexcept
    {
        state_->spot = light;
    }

    void VulkanMeshRenderer::set_scene_time(const float seconds) noexcept
    {
        if (seconds < state_->scene_time)
            reset_history();
        state_->scene_time = seconds;
    }

    void VulkanMeshRenderer::set_pass_input_view(const PassInputView view)
    {
        state_->set_input_view(view);
    }

    void VulkanMeshRenderer::reset_history() noexcept
    {
        state_->history_valid = false;
        state_->jitter_index = 0;
    }

    auto VulkanMeshRenderer::capture_pass_inputs() -> VulkanPassInputCapture
    {
        if (!state_->frame().inputs)
            throw std::logic_error("Pass inputs require an enabled, prepared frame");
        state_->wait();
        state_->begin_commands();
        auto& target = *state_->frame().inputs;
        const auto extent = state_->frame().target->render_extent;
        const size_t pixels = size_t(extent.width) * extent.height;
        if (!target.readback.handle())
            target.readback = VulkanBuffer(
                state_->device,
                pixels * sizeof(glm::vec4) * 2,
                VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                VulkanBufferMemory::readback
            );
        const std::array images{&target.normal_depth, &target.velocity};
        for (size_t i = 0; i < images.size(); ++i)
        {
            state_->input_barrier(
                *images[i],
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL
            );
            const VkBufferImageCopy copy{
                .bufferOffset = pixels * sizeof(glm::vec4) * i,
                .imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1},
                .imageExtent = {extent.width, extent.height, 1}
            };
            vkCmdCopyImageToBuffer(
                state_->frame().commands,
                images[i]->image,
                VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                target.readback.handle(),
                1,
                &copy
            );
            state_->input_barrier(
                *images[i],
                VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
            );
        }
        state_->submit_and_wait();
        VulkanPassInputCapture capture{
            int(extent.width),
            int(extent.height),
            std::vector<glm::vec4>(pixels),
            std::vector<glm::vec4>(pixels),
            state_->frame_history_valid,
            state_->jitter_uv
        };
        target.readback.read(std::as_writable_bytes(std::span{capture.normal_depth}));
        target.readback.read(std::as_writable_bytes(std::span{capture.velocity}), pixels * sizeof(glm::vec4));
        return capture;
    }

    void VulkanMeshRenderer::reload_shaders(const std::filesystem::path& directory)
    {
        const auto started = std::chrono::steady_clock::now();
        log::debug(
            "[renderer] Pipeline preparation begin: operation={} reason={}",
            state_->pipelines[0] ? "recreate" : "initialize",
            state_->pipelines[0] ? "shader_reload" : "backend_construction"
        );
        state_->wait();
        auto staged_directory = directory;
        std::array<VkPipeline, 18> replacement{};
        try
        {
            for (const auto pass : {RenderPass::Opaque, RenderPass::Overlay, RenderPass::Transparent})
                replacement[static_cast<size_t>(pass)] = state_->create_pipeline(directory, pass);
            for (int i = 0; i < 3; ++i)
                replacement[3 + i] = state_->create_pipeline(directory, RenderPass::Opaque, i);
            if (state_->tone_mapping)
            {
                replacement[6] = state_->tone_mapping->create_pipeline(directory);
                if (state_->renderer_config.bloom_enabled)
                    replacement[11] = state_->tone_mapping->create_pipeline(directory, true);
            }
            if (state_->renderer_config.ssgi_quality)
                replacement[14] = state_->create_pipeline(directory, RenderPass::Opaque, -6);
            if (state_->renderer_config.dof_quality || state_->renderer_config.motion_blur_quality)
                replacement[15] = state_->create_pipeline(directory, RenderPass::Opaque, -7);
            if (state_->renderer_config.taa_enabled)
                replacement[13] = state_->create_pipeline(directory, RenderPass::Opaque, -5);
            if (state_->renderer_config.ssao_enabled)
                replacement[12] = state_->create_pipeline(directory, RenderPass::Opaque, -4);
            if (state_->input_pipelines[0])
            {
                for (auto pass : {RenderPass::Opaque, RenderPass::Overlay, RenderPass::Transparent})
                    replacement[7 + static_cast<size_t>(pass)] = state_->create_pipeline(directory, pass, -2);
                replacement[10] = state_->create_pipeline(directory, RenderPass::Opaque, -3);
            }
            if (state_->mesh_shaders)
                replacement[17] = state_->create_pipeline(directory, RenderPass::Opaque, -1, true);
            if (state_->culling)
                replacement[16] = state_->culling->create_pipeline(directory);
        } catch (...)
        {
            for (const auto pipeline : replacement)
                if (pipeline)
                    vkDestroyPipeline(state_->device.handle(), pipeline, nullptr);
            throw;
        }
        for (const auto pipeline : state_->pipelines)
            if (pipeline)
                vkDestroyPipeline(state_->device.handle(), pipeline, nullptr);
        for (const auto pipeline : state_->shadow_pipelines)
            if (pipeline)
                vkDestroyPipeline(state_->device.handle(), pipeline, nullptr);
        std::copy_n(replacement.begin(), 3, state_->pipelines.begin());
        std::copy_n(replacement.begin() + 3, 3, state_->shadow_pipelines.begin());
        if (state_->tone_mapping)
        {
            if (state_->tone_mapping->pipeline)
                vkDestroyPipeline(state_->device.handle(), state_->tone_mapping->pipeline, nullptr);
            state_->tone_mapping->pipeline = replacement[6];
            if (state_->tone_mapping->bloom_pipeline)
                vkDestroyPipeline(state_->device.handle(), state_->tone_mapping->bloom_pipeline, nullptr);
            state_->tone_mapping->bloom_pipeline = replacement[11];
        }
        for (size_t i = 0; i < state_->input_pipelines.size(); ++i)
        {
            if (state_->input_pipelines[i])
                vkDestroyPipeline(state_->device.handle(), state_->input_pipelines[i], nullptr);
            state_->input_pipelines[i] = replacement[7 + i];
        }
        if (state_->ao_pipeline)
            vkDestroyPipeline(state_->device.handle(), state_->ao_pipeline, nullptr);
        state_->ao_pipeline = replacement[12];
        if (state_->temporal_pipeline)
            vkDestroyPipeline(state_->device.handle(), state_->temporal_pipeline, nullptr);
        state_->temporal_pipeline = replacement[13];
        if (state_->gi_pipeline)
            vkDestroyPipeline(state_->device.handle(), state_->gi_pipeline, nullptr);
        state_->gi_pipeline = replacement[14];
        if (state_->lens_pipeline)
            vkDestroyPipeline(state_->device.handle(), state_->lens_pipeline, nullptr);
        state_->lens_pipeline = replacement[15];
        if (state_->culling)
        {
            if (state_->culling->pipeline)
                vkDestroyPipeline(state_->device.handle(), state_->culling->pipeline, nullptr);
            state_->culling->pipeline = replacement[16];
        }
        if (state_->meshlet_pipeline)
            vkDestroyPipeline(state_->device.handle(), state_->meshlet_pipeline, nullptr);
        state_->meshlet_pipeline = replacement[17];
        log::debug(
            "[renderer] Pipelines ready: duration_ms={:.3f} pipeline_cache=attached (hits not measured)",
            std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started).count()
        );
        state_->shader_directory = std::move(staged_directory);
        reset_history();
    }
    auto VulkanMeshRenderer::render_image(
        const std::span<const RenderItem> items,
        const PerspectiveCamera& camera,
        const int width,
        const int height,
        const std::span<const RenderItem> viewmodel,
        ImDrawData* ui
    ) -> CpuRgbaTextureData
    {
        return state_->render(items, camera, width, height, viewmodel, true, ui, items);
    }
    auto VulkanMeshRenderer::render_color_image(
        const VkDevice presenting_device,
        const std::span<const RenderItem> items,
        const PerspectiveCamera& camera,
        const VkExtent2D extent,
        const std::span<const RenderItem> viewmodel,
        ImDrawData* ui
    ) -> VkImage
    {
        if (presenting_device != state_->device.handle())
            throw std::invalid_argument("Vulkan mesh renderer and swapchain must use the same device");
        static_cast<void>(state_->render(
            items,
            camera,
            static_cast<int>(extent.width),
            static_cast<int>(extent.height),
            viewmodel,
            false,
            ui,
            items
        ));
        return state_->frame().target->color.image;
    }
} // namespace RelayCore::render
