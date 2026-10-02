#include "relaycore/render/vulkan_mesh_renderer.h"

#include "relaycore/core/engine_config.h"
#include "relaycore/render/authored_color.h"
#include "relaycore/render/camera.h"
#include "relaycore/render/render_authoring_config.h"
#include "relaycore/render/shadow_math.h"
#include "relaycore/render/vulkan_device.h"
#include "relaycore/render/vulkan_gpu_profiler.h"
#include "relaycore/render/vulkan_uploads.h"

#include <algorithm>
#include <array>
#include <cstdio>
#include <fstream>
#include <imgui_impl_vulkan.h>
#include <limits>
#include <stdexcept>
#include <unordered_map>
#include <utility>

namespace RelayCore::render
{
    namespace
    {
        void require_mesh_vk(const VkResult result, const std::string& operation)
        {
            if (result != VK_SUCCESS)
                throw std::runtime_error(operation + ": VkResult " + std::to_string(result));
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
        };
        static_assert(sizeof(FrameParameters) == 16);

        struct LightParameters {
            std::array<glm::vec4, 8> position_radius;
            std::array<glm::vec4, 8> color;
            glm::ivec4 count;
        };
        static_assert(offsetof(LightParameters, color) == 128);
        static_assert(offsetof(LightParameters, count) == 256);
        static_assert(sizeof(LightParameters) == 272);

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
                constexpr auto required =
                    VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT
                    | VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT | VK_FORMAT_FEATURE_TRANSFER_DST_BIT;
                if ((properties.optimalTilingFeatures & required) != required)
                    throw std::runtime_error(
                        "Vulkan shadows require sampled/linear-filtered D24_UNORM depth attachments"
                    );
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
                const VkFormat attachment_format = VK_FORMAT_UNDEFINED
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
                    .samples = VK_SAMPLE_COUNT_1_BIT,
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
                require_mesh_vk(
                    vkCreateImageView(device.handle(), &view_info, nullptr, &view),
                    "Failed to create Vulkan mesh target view"
                );
                if (srgb_view)
                {
                    view_info.format = format;
                    require_mesh_vk(
                        vkCreateImageView(device.handle(), &view_info, nullptr, &unorm_view),
                        "Failed to create unencoded Vulkan UI target view"
                    );
                }
            }
        };

        struct MeshTarget {
            VkExtent2D extent;
            MeshImage color, depth;
            VulkanBuffer readback;
            MeshTarget(const VulkanDevice& device, const VkExtent2D size, const VkFormat scene_format) :
                extent(size), color(device), depth(device), readback(
                                                                device,
                                                                VkDeviceSize(size.width) * size.height * 4,
                                                                VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                                                VulkanBufferMemory::readback
                                                            )
            {
                color.initialize(
                    size,
                    VK_FORMAT_R8G8B8A8_UNORM,
                    VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
                    VK_IMAGE_ASPECT_COLOR_BIT,
                    scene_format
                );
                depth.initialize(
                    size,
                    VK_FORMAT_D32_SFLOAT,
                    VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                    VK_IMAGE_ASPECT_DEPTH_BIT
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
    } // namespace

    struct VulkanMeshRenderer::State {
        struct Mesh {
            std::weak_ptr<const CpuColorMeshData> source;
            VulkanBuffer vertices, indices;
            uint32_t index_count;
        };
        struct Material {
            VkDevice device;
            std::weak_ptr<const CpuRgbaTextureData> source;
            VulkanTexture texture;
            VkDescriptorPool pool = VK_NULL_HANDLE;
            VkDescriptorSet set = VK_NULL_HANDLE;
            explicit Material(const VkDevice owner) : device(owner) {}
            ~Material() noexcept
            {
                if (pool)
                    vkDestroyDescriptorPool(device, pool, nullptr);
            }
        };

        const VulkanDevice& device;
        VulkanGpuProfiler profiler;
        VulkanUploads uploads;
        VulkanBuffer material_data;
        VulkanBuffer light_data;
        VulkanBuffer frame_data;
        float scene_time = 0;
        LightParameters lights{};
        VulkanBuffer shadow_data;
        RendererConfig renderer_config{};
        MaterialParameters material_parameters{};
        std::array<VkDescriptorSet, 2> shadow_previews{};
        ColorPipelineMode color_pipeline = ColorPipelineMode::legacy_gamma;
        SpotLight spot{};
        ShadowParameters shadow_parameters{};
        std::array<std::unique_ptr<ShadowImage>, 3> shadow_maps;
        std::unique_ptr<ShadowImage> point_shadows;
        std::array<VkPipeline, 3> shadow_pipelines{};
        Texture2D atlas_source;
        std::unordered_map<const CpuColorMeshData*, Mesh> meshes;
        std::unordered_map<const CpuRgbaTextureData*, std::unique_ptr<Material>> materials;
        VkDescriptorSetLayout descriptor_layout = VK_NULL_HANDLE;
        VkPipelineLayout layout = VK_NULL_HANDLE;
        std::array<VkPipeline, 3> pipelines{};
        VkCommandPool pool = VK_NULL_HANDLE;
        VkCommandBuffer commands = VK_NULL_HANDLE;
        VkFence fence = VK_NULL_HANDLE;
        bool pending = false;
        uint32_t max_target_width = 0, max_target_height = 0;
        std::unique_ptr<MeshTarget> target;

        explicit State(const VulkanDevice& owner) :
            device(owner), profiler(owner, "RelayCore Vulkan scene"), uploads(owner),
            material_data(
                owner,
                sizeof(MaterialParameters),
                VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                VulkanBufferMemory::upload
            ),
            light_data(owner, sizeof(LightParameters), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VulkanBufferMemory::upload),
            frame_data(owner, sizeof(FrameParameters), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VulkanBufferMemory::upload),
            shadow_data(owner, sizeof(ShadowParameters), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VulkanBufferMemory::upload)
        {}

        void wait()
        {
            if (!pending)
                return;
            require_mesh_vk(
                vkWaitForFences(device.handle(), 1, &fence, VK_TRUE, std::numeric_limits<uint64_t>::max()),
                "Failed to wait for Vulkan mesh frame"
            );
            pending = false;
            profiler.collect();
        }
        ~State() noexcept
        {
            try
            {
                wait();
            } catch (const std::exception& error)
            {
                std::fprintf(stderr, "Vulkan mesh shutdown failed: %s\n", error.what());
            }
            for (auto preview : shadow_previews)
                if (preview)
                    ImGui_ImplVulkan_RemoveTexture(preview);
            if (pool)
                vkDestroyCommandPool(device.handle(), pool, nullptr);
            if (fence)
                vkDestroyFence(device.handle(), fence, nullptr);
            materials.clear();
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
        }

        auto scene_format() const noexcept -> VkFormat
        {
            return color_pipeline == ColorPipelineMode::hardware_srgb ? VK_FORMAT_R8G8B8A8_SRGB
                                                                      : VK_FORMAT_R8G8B8A8_UNORM;
        }

        void initialize(
            const RenderAuthoringConfig& authoring,
            const CpuRgbaTextureData& atlas,
            const TextureEncoding encoding,
            const RendererConfig& config,
            const ColorPipelineMode pipeline
        )
        {
            renderer_config = config;
            color_pipeline = pipeline;
            VkPhysicalDeviceProperties properties{};
            vkGetPhysicalDeviceProperties(device.physical_device(), &properties);
            max_target_width = std::min(properties.limits.maxFramebufferWidth, properties.limits.maxImageDimension2D);
            max_target_height = std::min(properties.limits.maxFramebufferHeight, properties.limits.maxImageDimension2D);
            VkFormatProperties depth_properties{};
            vkGetPhysicalDeviceFormatProperties(device.physical_device(), VK_FORMAT_D32_SFLOAT, &depth_properties);
            if (!(depth_properties.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT))
                throw std::runtime_error("Vulkan mesh renderer requires D32_SFLOAT depth attachments");
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
            constexpr auto attachment_features =
                VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT | VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BLEND_BIT;
            if ((color_properties.optimalTilingFeatures & attachment_features) != attachment_features)
                throw std::runtime_error(
                    "Vulkan scene target requires the selected UNORM/sRGB attachment and blend support"
                );
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
                    VK_FORMAT_X8_D24_UNORM_PACK32,
                    i == 0   ? "Shadow near cascade D24"
                    : i == 1 ? "Shadow far cascade D24"
                             : "Shadow spot D24"
                );
            }
            point_shadows = std::make_unique<ShadowImage>(device, VkExtent2D{256, 256}, 48);
            point_shadows->initialize(
                VK_FORMAT_X8_D24_UNORM_PACK32,
                "Shadow point faces D24 (eight lights, six faces)"
            );
            atlas_source.initialize_rgba(atlas.width, atlas.height, atlas.pixels, encoding);
            const std::array bindings{
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
            const VkPushConstantRange push{
                VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                0,
                sizeof(DrawPush)
            };
            const VkPipelineLayoutCreateInfo layout_info{
                .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
                .setLayoutCount = 1,
                .pSetLayouts = &descriptor_layout,
                .pushConstantRangeCount = 1,
                .pPushConstantRanges = &push,
            };
            require_mesh_vk(
                vkCreatePipelineLayout(device.handle(), &layout_info, nullptr, &layout),
                "Failed to create Vulkan mesh pipeline layout"
            );
            const VkCommandPoolCreateInfo pool_info{
                .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
                .queueFamilyIndex = device.capabilities().graphics_queue_family,
            };
            require_mesh_vk(
                vkCreateCommandPool(device.handle(), &pool_info, nullptr, &pool),
                "Failed to create mesh commands"
            );
            const VkCommandBufferAllocateInfo command_info{
                .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
                .commandPool = pool,
                .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
                .commandBufferCount = 1,
            };
            require_mesh_vk(
                vkAllocateCommandBuffers(device.handle(), &command_info, &commands),
                "Failed to allocate mesh commands"
            );
            const VkFenceCreateInfo fence_info{.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
            require_mesh_vk(
                vkCreateFence(device.handle(), &fence_info, nullptr, &fence),
                "Failed to create mesh frame fence"
            );
        }

        auto
        create_pipeline(const std::filesystem::path& directory, const RenderPass pass, const int shadow_kind = -1) const
            -> VkPipeline
        {
            MeshShaders shaders{device.handle()};
            const auto prefix = shadow_kind < 0 ? "mesh" : shadow_kind == 2 ? "point_shadow" : "shadow";
            shaders.vertex = shaders.load(directory / (std::string(prefix) + ".vert.spv"), "vertex_main");
            shaders.fragment = shaders.load(directory / (std::string(prefix) + ".frag.spv"), "fragment_main");
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
                    .pName = "fragment_main"
                },
            };
            const VkVertexInputBindingDescription binding{0, sizeof(ColorVertex), VK_VERTEX_INPUT_RATE_VERTEX};
            const std::array attributes{
                VkVertexInputAttributeDescription{0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(ColorVertex, position)},
                VkVertexInputAttributeDescription{1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(ColorVertex, color)},
                VkVertexInputAttributeDescription{2, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(ColorVertex, normal)},
                VkVertexInputAttributeDescription{3, 0, VK_FORMAT_R32_SFLOAT, offsetof(ColorVertex, material_kind)},
                VkVertexInputAttributeDescription{4, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(ColorVertex, uv)},
            };
            const VkPipelineVertexInputStateCreateInfo vertex_info{
                .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
                .vertexBindingDescriptionCount = 1,
                .pVertexBindingDescriptions = &binding,
                .vertexAttributeDescriptionCount = shadow_kind < 0 ? static_cast<uint32_t>(attributes.size()) : 1u,
                .pVertexAttributeDescriptions = attributes.data(),
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
                .cullMode = shadow_kind == 0                                ? VkCullModeFlags{VK_CULL_MODE_FRONT_BIT}
                            : shadow_kind > 0 || pass == RenderPass::Opaque ? VkCullModeFlags{VK_CULL_MODE_BACK_BIT}
                                                                            : VkCullModeFlags{VK_CULL_MODE_NONE},
                .frontFace = shadow_kind < 0 ? VK_FRONT_FACE_COUNTER_CLOCKWISE : VK_FRONT_FACE_CLOCKWISE,
                .lineWidth = 1,
            };
            const VkPipelineMultisampleStateCreateInfo multisample{
                .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
                .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
            };
            const VkPipelineDepthStencilStateCreateInfo depth{
                .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
                .depthTestEnable = VK_TRUE,
                .depthWriteEnable = pass == RenderPass::Opaque ? VK_TRUE : VK_FALSE,
                .depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL,
            };
            const VkPipelineColorBlendAttachmentState blend_attachment{
                .blendEnable = pass == RenderPass::Opaque ? VK_FALSE : VK_TRUE,
                .srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
                .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
                .colorBlendOp = VK_BLEND_OP_ADD,
                .srcAlphaBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
                .dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
                .alphaBlendOp = VK_BLEND_OP_ADD,
                .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT
                                  | VK_COLOR_COMPONENT_A_BIT,
            };
            const VkPipelineColorBlendStateCreateInfo blend{
                .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
                .attachmentCount = shadow_kind < 0 ? 1u : 0u,
                .pAttachments = shadow_kind < 0 ? &blend_attachment : nullptr,
            };
            const std::array dynamics{VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
            const VkPipelineDynamicStateCreateInfo dynamic{
                .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
                .dynamicStateCount = static_cast<uint32_t>(dynamics.size()),
                .pDynamicStates = dynamics.data(),
            };
            const VkFormat color = scene_format();
            const VkPipelineRenderingCreateInfo rendering{
                .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
                .colorAttachmentCount = shadow_kind < 0 ? 1u : 0u,
                .pColorAttachmentFormats = shadow_kind < 0 ? &color : nullptr,
                .depthAttachmentFormat = shadow_kind < 0 ? VK_FORMAT_D32_SFLOAT : VK_FORMAT_X8_D24_UNORM_PACK32,
            };
            const VkGraphicsPipelineCreateInfo info{
                .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
                .pNext = &rendering,
                .stageCount = static_cast<uint32_t>(stages.size()),
                .pStages = stages.data(),
                .pVertexInputState = &vertex_info,
                .pInputAssemblyState = &assembly,
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
                vkCreateGraphicsPipelines(device.handle(), VK_NULL_HANDLE, 1, &info, nullptr, &replacement);
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
            auto vertices =
                uploads.upload_mesh_buffer(std::as_bytes(std::span{cpu->vertices}), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
            uploads.wait(vertices.completion);
            auto indices =
                uploads.upload_mesh_buffer(std::as_bytes(std::span{cpu->indices}), VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
            uploads.wait(indices.completion);
            return meshes
                .emplace(
                    cpu.get(),
                    Mesh{
                        cpu,
                        std::move(vertices.buffer),
                        std::move(indices.buffer),
                        static_cast<uint32_t>(cpu->indices.size())
                    }
                )
                .first->second;
        }

        auto material(const Texture2D& resource) -> Material&
        {
            const auto& cpu = resource.cpu_data();
            if (!cpu)
                throw std::invalid_argument("Vulkan draw requires an initialized CPU texture");
            const auto found = materials.find(cpu.get());
            if (found != materials.end())
                return *found->second;
            auto uploaded = uploads.upload_rgba(*cpu, resource.encoding());
            uploads.wait(uploaded.completion);
            auto result = std::make_unique<Material>(device.handle());
            result->texture = std::move(uploaded.texture);
            result->source = cpu;
            const std::array sizes{
                VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 4},
                VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 5}
            };
            const VkDescriptorPoolCreateInfo pool_info{
                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
                .maxSets = 1,
                .poolSizeCount = static_cast<uint32_t>(sizes.size()),
                .pPoolSizes = sizes.data(),
            };
            require_mesh_vk(
                vkCreateDescriptorPool(device.handle(), &pool_info, nullptr, &result->pool),
                "Failed to create mesh material pool"
            );
            const VkDescriptorSetAllocateInfo allocation{
                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
                .descriptorPool = result->pool,
                .descriptorSetCount = 1,
                .pSetLayouts = &descriptor_layout,
            };
            require_mesh_vk(
                vkAllocateDescriptorSets(device.handle(), &allocation, &result->set),
                "Failed to allocate mesh material descriptors"
            );
            const VkDescriptorBufferInfo buffer{material_data.handle(), 0, material_data.size()};
            const VkDescriptorBufferInfo lighting{light_data.handle(), 0, light_data.size()};
            const VkDescriptorBufferInfo shadow_buffer{shadow_data.handle(), 0, shadow_data.size()};
            const VkDescriptorBufferInfo frame_buffer{frame_data.handle(), 0, frame_data.size()};
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
            const std::array writes{
                VkWriteDescriptorSet{
                    .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                    .dstSet = result->set,
                    .dstBinding = 8,
                    .descriptorCount = 1,
                    .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                    .pBufferInfo = &frame_buffer
                },
                VkWriteDescriptorSet{
                    .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                    .dstSet = result->set,
                    .dstBinding = 0,
                    .descriptorCount = 1,
                    .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                    .pBufferInfo = &buffer
                },
                VkWriteDescriptorSet{
                    .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                    .dstSet = result->set,
                    .dstBinding = 1,
                    .descriptorCount = 1,
                    .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                    .pImageInfo = &image
                },
                VkWriteDescriptorSet{
                    .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                    .dstSet = result->set,
                    .dstBinding = 2,
                    .descriptorCount = 1,
                    .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                    .pBufferInfo = &lighting
                },
                VkWriteDescriptorSet{
                    .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                    .dstSet = result->set,
                    .dstBinding = 3,
                    .descriptorCount = 1,
                    .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                    .pBufferInfo = &shadow_buffer
                },
                VkWriteDescriptorSet{
                    .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                    .dstSet = result->set,
                    .dstBinding = 4,
                    .descriptorCount = 1,
                    .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                    .pImageInfo = &shadow_images[0]
                },
                VkWriteDescriptorSet{
                    .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                    .dstSet = result->set,
                    .dstBinding = 5,
                    .descriptorCount = 1,
                    .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                    .pImageInfo = &shadow_images[1]
                },
                VkWriteDescriptorSet{
                    .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                    .dstSet = result->set,
                    .dstBinding = 6,
                    .descriptorCount = 1,
                    .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                    .pImageInfo = &shadow_images[2]
                },
                VkWriteDescriptorSet{
                    .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                    .dstSet = result->set,
                    .dstBinding = 7,
                    .descriptorCount = 1,
                    .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                    .pImageInfo = &shadow_images[3]
                },
            };
            vkUpdateDescriptorSets(device.handle(), static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
            return *materials.emplace(cpu.get(), std::move(result)).first->second;
        }

        void prepare(const std::span<const RenderItem> items)
        {
            for (const auto& item : items)
                if (item.mesh)
                {
                    static_cast<void>(mesh(*item.mesh));
                    static_cast<void>(material(item.diffuse_texture ? *item.diffuse_texture : atlas_source));
                }
        }
        void draw(
            const std::span<const RenderItem> items,
            const PerspectiveCamera& camera,
            const RenderPass pass,
            const bool viewmodel = false
        )
        {
            vkCmdBindPipeline(commands, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines[static_cast<size_t>(pass)]);
            for (const auto& item : items)
            {
                if ((!viewmodel && item.pass != pass) || !item.mesh)
                    continue;
                const auto& geometry = meshes.at(item.mesh->cpu_data().get());
                const auto& texture = item.diffuse_texture ? *item.diffuse_texture : atlas_source;
                const auto set = materials.at(texture.cpu_data().get())->set;
                const auto vertex = geometry.vertices.handle();
                const VkDeviceSize offset = 0;
                vkCmdBindVertexBuffers(commands, 0, 1, &vertex, &offset);
                vkCmdBindIndexBuffer(commands, geometry.indices.handle(), 0, VK_INDEX_TYPE_UINT32);
                vkCmdBindDescriptorSets(commands, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, 0, 1, &set, 0, nullptr);
                const auto position = camera.position();
                DrawPush push{
                    camera.view_projection_matrix() * item.model_matrix,
                    {},
                    (item.diffuse_texture ? 1U : 0U) | (pass == RenderPass::Transparent ? 2U : 0U)
                        | (pass == RenderPass::Overlay ? 4U : 0U),
                    {position.x, position.y, position.z}
                };
                for (int row = 0; row < 3; ++row)
                    for (int col = 0; col < 4; ++col)
                        push.model_rows[row][col] = item.model_matrix[col][row];
                vkCmdPushConstants(
                    commands,
                    layout,
                    VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                    0,
                    sizeof(push),
                    &push
                );
                vkCmdDrawIndexed(commands, geometry.index_count, 1, 0, 0, 0);
            }
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
            vkCmdPipelineBarrier2(commands, &info);
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
            RELAYCORE_PROFILE_VK_ZONE(profiler, commands, "Shadow depth draws");
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
            vkCmdSetViewport(commands, 0, 1, &viewport);
            vkCmdSetScissor(commands, 0, 1, &scissor);
            vkCmdBindPipeline(commands, VK_PIPELINE_BIND_POINT_GRAPHICS, shadow_pipelines[kind]);
            vkCmdBeginRendering(commands, &info);
            const auto planes = shadow_frustum_planes(view_projection);
            for (const auto& item : items)
            {
                if (!item.mesh || item.pass != RenderPass::Opaque || (kind == 0 && !item.casts_directional_shadow))
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
                auto& geometry = mesh(*item.mesh);
                ShadowPush push{.mvp = view_projection * item.model_matrix};
                for (int row = 0; row < 3; ++row)
                    push.model_rows[row] = {
                        item.model_matrix[0][row],
                        item.model_matrix[1][row],
                        item.model_matrix[2][row],
                        item.model_matrix[3][row]
                    };
                if (light)
                    push.light_position_radius = {light->position, light->radius};
                vkCmdPushConstants(
                    commands,
                    layout,
                    VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                    0,
                    sizeof(push),
                    &push
                );
                const auto buffer = geometry.vertices.handle();
                constexpr VkDeviceSize offset = 0;
                vkCmdBindVertexBuffers(commands, 0, 1, &buffer, &offset);
                vkCmdBindIndexBuffer(commands, geometry.indices.handle(), 0, VK_INDEX_TYPE_UINT32);
                vkCmdDrawIndexed(commands, geometry.index_count, 1, 0, 0, 0);
            }
            vkCmdEndRendering(commands);
        }
        void render_shadows(const std::span<const RenderItem> items, const PerspectiveCamera& camera)
        {
            RELAYCORE_PROFILE_VK_ZONE(profiler, commands, "Shadows");
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
                    commands,
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
                for (int i = 0; i < lights.count.x; ++i)
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
            shadow_data.write(std::as_bytes(std::span{&shadow_parameters, 1}));
        }

        void set_config(const RendererConfig& config)
        {
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
            parameters.grading = {config.postfx_saturation, 0, 0, 0};
            parameters.local.w = std::max(config.indirect_bounce_strength, 0.0F);
            material_data.write(std::as_bytes(std::span{&parameters, 1}));
        }

        void begin_commands()
        {
            wait();
            require_mesh_vk(vkResetCommandPool(device.handle(), pool, 0), "Failed to reset mesh commands");
            const VkCommandBufferBeginInfo begin{
                .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
                .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
            };
            require_mesh_vk(vkBeginCommandBuffer(commands, &begin), "Failed to begin mesh commands");
        }

        void submit_and_wait()
        {
            require_mesh_vk(vkEndCommandBuffer(commands), "Failed to end mesh frame");
            require_mesh_vk(vkResetFences(device.handle(), 1, &fence), "Failed to reset mesh fence");
            const VkCommandBufferSubmitInfo command{
                .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
                .commandBuffer = commands
            };
            const VkSubmitInfo2 submit{
                .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
                .commandBufferInfoCount = 1,
                .pCommandBufferInfos = &command
            };
            require_mesh_vk(vkQueueSubmit2(device.graphics_queue(), 1, &submit, fence), "Failed to submit mesh frame");
            pending = true;
            wait();
        }

        void record_ui(ImDrawData* ui)
        {
            RELAYCORE_PROFILE_VK_ZONE(profiler, commands, "ImGui");
            const VkRenderingAttachmentInfo color{
                .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
                .imageView = target->color.unorm_view ? target->color.unorm_view : target->color.view,
                .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                .loadOp = VK_ATTACHMENT_LOAD_OP_LOAD,
                .storeOp = VK_ATTACHMENT_STORE_OP_STORE
            };
            const VkRenderingInfo rendering{
                .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
                .renderArea = {{0, 0}, target->extent},
                .layerCount = 1,
                .colorAttachmentCount = 1,
                .pColorAttachments = &color
            };
            vkCmdBeginRendering(commands, &rendering);
            ImGui_ImplVulkan_RenderDrawData(ui, commands);
            vkCmdEndRendering(commands);
        }

        void append_ui(ImDrawData* ui)
        {
            if (!target)
                throw std::logic_error("Vulkan UI requires a prepared scene");
            if (!ui)
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
                .image = target->color.image,
                .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}
            };
            const VkDependencyInfo dependency{
                .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
                .imageMemoryBarrierCount = 1,
                .pImageMemoryBarriers = &barrier
            };
            vkCmdPipelineBarrier2(commands, &dependency);
            record_ui(ui);
            barrier.srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
            barrier.srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
            barrier.dstStageMask = VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT;
            barrier.dstAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT;
            barrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            vkCmdPipelineBarrier2(commands, &dependency);
            submit_and_wait();
        }

        void record_readback()
        {
            const VkBufferImageCopy copy{
                .imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1},
                .imageExtent = {target->extent.width, target->extent.height, 1}
            };
            vkCmdCopyImageToBuffer(
                commands,
                target->color.image,
                VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                target->readback.handle(),
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
            vkCmdPipelineBarrier2(commands, &host_info);
        }

        auto capture_shadow_depth(size_t cascade) -> std::vector<float>
        {
            if (cascade >= 2 || !target)
                throw std::logic_error("Shadow capture requires a prepared directional cascade");
            const auto& map = *shadow_maps[cascade];
            VulkanBuffer readback{
                device,
                VkDeviceSize(map.extent.width) * map.extent.height * 4,
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
            vkCmdPipelineBarrier2(commands, &dependency);
            const VkBufferImageCopy copy{
                .imageSubresource = {VK_IMAGE_ASPECT_DEPTH_BIT, 0, 0, 1},
                .imageExtent = {map.extent.width, map.extent.height, 1}
            };
            vkCmdCopyImageToBuffer(
                commands,
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
            vkCmdPipelineBarrier2(commands, &dependency);
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
            vkCmdPipelineBarrier2(commands, &host_info);
            submit_and_wait();
            std::vector<uint32_t> packed(size_t(map.extent.width) * map.extent.height);
            readback.read(std::as_writable_bytes(std::span{packed}));
            std::vector<float> output(packed.size());
            std::transform(packed.begin(), packed.end(), output.begin(), [](uint32_t depth) {
                return float(depth & 0xffffffu) / float(0xffffffu);
            });
            return output;
        }

        auto capture_prepared_image() -> CpuRgbaTextureData
        {
            if (!target)
                throw std::logic_error("Vulkan capture requires a prepared scene");
            begin_commands();
            record_readback();
            submit_and_wait();
            CpuRgbaTextureData output{int(target->extent.width), int(target->extent.height), {}};
            output.pixels.resize(size_t(output.width) * output.height * 4);
            target->readback.read(std::as_writable_bytes(std::span{output.pixels}));
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
            wait();
            light_data.write(std::as_bytes(std::span{&lights, 1}));
            const FrameParameters frame{
                {float(width), float(height), scene_time, renderer_config.shadow_debug_cascades ? 1.0F : 0.0F}
            };
            frame_data.write(std::as_bytes(std::span{&frame, 1}));
            std::erase_if(meshes, [](const auto& entry) {
                return entry.second.source.expired();
            });
            std::erase_if(materials, [](const auto& entry) {
                return entry.second->source.expired();
            });
            prepare(items);
            prepare(shadow_casters);
            prepare(viewmodel);
            uploads.collect();
            const VkExtent2D extent{static_cast<uint32_t>(width), static_cast<uint32_t>(height)};
            if (!target || target->extent.width != extent.width || target->extent.height != extent.height)
                target = std::make_unique<MeshTarget>(device, extent, scene_format());
            CpuRgbaTextureData output{width, height, {}};
            if (capture)
                output.pixels.resize(size_t(width) * height * 4);
            begin_commands();
            {
                RELAYCORE_PROFILE_VK_ZONE(profiler, commands, "Scene frame");
                render_shadows(shadow_casters, camera);
                std::array barriers{
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
                        .image = target->color.image,
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
                        .image = target->depth.image,
                        .subresourceRange = {VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1}
                    },
                };
                VkDependencyInfo dependency{
                    .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
                    .imageMemoryBarrierCount = static_cast<uint32_t>(barriers.size()),
                    .pImageMemoryBarriers = barriers.data()
                };
                vkCmdPipelineBarrier2(commands, &dependency);
                const auto clear_rgb = visual_rgb_for_pipeline(
                    {renderer_config.clear_r, renderer_config.clear_g, renderer_config.clear_b},
                    color_pipeline
                );
                VkRenderingAttachmentInfo color{
                    .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
                    .imageView = target->color.view,
                    .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                    .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
                    .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
                    .clearValue = {.color = {{clear_rgb.x, clear_rgb.y, clear_rgb.z, 1}}}
                };
                const VkRenderingAttachmentInfo depth{
                    .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
                    .imageView = target->depth.view,
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
                    static_cast<float>(height),
                    static_cast<float>(width),
                    -static_cast<float>(height),
                    0,
                    1
                };
                const VkRect2D scissor{{0, 0}, extent};
                vkCmdSetViewport(commands, 0, 1, &viewport);
                vkCmdSetScissor(commands, 0, 1, &scissor);
                vkCmdBeginRendering(commands, &rendering);
                {
                    RELAYCORE_PROFILE_VK_ZONE(profiler, commands, "World passes");
                    for (const auto pass : {RenderPass::Opaque, RenderPass::Overlay, RenderPass::Transparent})
                        draw(items, camera, pass);
                }
                vkCmdEndRendering(commands);
                if (!viewmodel.empty())
                {
                    RELAYCORE_PROFILE_VK_ZONE(profiler, commands, "Viewmodel");
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
                    vkCmdPipelineBarrier2(commands, &reuse_info);
                    color.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
                    vkCmdBeginRendering(commands, &rendering);
                    draw(viewmodel, camera, RenderPass::Opaque, true);
                    vkCmdEndRendering(commands);
                }
                if (ui)
                {
                    RELAYCORE_PROFILE_VK_ZONE(profiler, commands, "ImGui");
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
                    vkCmdPipelineBarrier2(commands, &reuse_info);
                    record_ui(ui);
                }
                auto& transfer = barriers[0];
                transfer.srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
                transfer.srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
                transfer.dstStageMask = VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT;
                transfer.dstAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT;
                transfer.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
                transfer.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
                dependency.imageMemoryBarrierCount = 1;
                vkCmdPipelineBarrier2(commands, &dependency);
                if (capture)
                    record_readback();
            }
            submit_and_wait();
            if (capture)
                target->readback.read(std::as_writable_bytes(std::span{output.pixels}));
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
        const ColorPipelineMode pipeline
    ) : state_(std::make_unique<State>(device))
    {
        state_->initialize(authoring, atlas, encoding, config, pipeline);
        reload_shaders(shader_directory);
    }
    VulkanMeshRenderer::~VulkanMeshRenderer() noexcept = default;

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
    auto VulkanMeshRenderer::capture_prepared_image() -> CpuRgbaTextureData
    {
        return state_->capture_prepared_image();
    }
    auto VulkanMeshRenderer::prepared_image(VkDevice device) const -> VkImage
    {
        if (device != state_->device.handle() || !state_->target)
            throw std::logic_error("Vulkan presentation requires a prepared image on the same device");
        return state_->target->color.image;
    }
    auto VulkanMeshRenderer::prepared_extent() const -> VkExtent2D
    {
        return state_->target ? state_->target->extent : VkExtent2D{};
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
        state_->scene_time = seconds;
    }

    void VulkanMeshRenderer::reload_shaders(const std::filesystem::path& directory)
    {
        state_->wait();
        std::array<VkPipeline, 6> replacement{};
        try
        {
            for (const auto pass : {RenderPass::Opaque, RenderPass::Overlay, RenderPass::Transparent})
                replacement[static_cast<size_t>(pass)] = state_->create_pipeline(directory, pass);
            for (int i = 0; i < 3; ++i)
                replacement[3 + i] = state_->create_pipeline(directory, RenderPass::Opaque, i);
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
        return state_->target->color.image;
    }
} // namespace RelayCore::render
