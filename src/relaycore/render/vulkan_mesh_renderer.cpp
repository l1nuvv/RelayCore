#include "relaycore/render/vulkan_mesh_renderer.h"

#include "relaycore/render/camera.h"
#include "relaycore/render/render_authoring_config.h"
#include "relaycore/render/vulkan_device.h"
#include "relaycore/render/vulkan_uploads.h"

#include <algorithm>
#include <array>
#include <cstdio>
#include <fstream>
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
            uint32_t model_textured;
        };
        static_assert(offsetof(DrawPush, model_rows) == 64);
        static_assert(offsetof(DrawPush, model_textured) == 112);
        static_assert(sizeof(DrawPush) == 116);

        struct MeshImage {
            const VulkanDevice& device;
            VkImage image = VK_NULL_HANDLE;
            VmaAllocation allocation = nullptr;
            VkImageView view = VK_NULL_HANDLE;
            explicit MeshImage(const VulkanDevice& owner) : device(owner) {}
            ~MeshImage() noexcept
            {
                if (view)
                    vkDestroyImageView(device.handle(), view, nullptr);
                if (image)
                    vmaDestroyImage(device.allocator(), image, allocation);
            }
            void initialize(
                const VkExtent2D extent,
                const VkFormat format,
                const VkImageUsageFlags usage,
                const VkImageAspectFlags aspect
            )
            {
                const VkImageCreateInfo info{
                    .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
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
                const VkImageViewCreateInfo view_info{
                    .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                    .image = image,
                    .viewType = VK_IMAGE_VIEW_TYPE_2D,
                    .format = format,
                    .subresourceRange = {aspect, 0, 1, 0, 1},
                };
                require_mesh_vk(
                    vkCreateImageView(device.handle(), &view_info, nullptr, &view),
                    "Failed to create Vulkan mesh target view"
                );
            }
        };

        struct MeshTarget {
            VkExtent2D extent;
            MeshImage color, depth;
            VulkanBuffer readback;
            MeshTarget(const VulkanDevice& device, const VkExtent2D size) :
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
                    VK_IMAGE_ASPECT_COLOR_BIT
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
                const auto diagnostic = path.string() + " (mesh.slang, entry " + entry + ")";
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
        VulkanUploads uploads;
        VulkanBuffer material_data;
        Texture2D atlas_source;
        std::unordered_map<const CpuColorMeshData*, Mesh> meshes;
        std::unordered_map<const CpuRgbaTextureData*, std::unique_ptr<Material>> materials;
        VkDescriptorSetLayout descriptor_layout = VK_NULL_HANDLE;
        VkPipelineLayout layout = VK_NULL_HANDLE;
        VkPipeline pipeline = VK_NULL_HANDLE;
        VkCommandPool pool = VK_NULL_HANDLE;
        VkCommandBuffer commands = VK_NULL_HANDLE;
        VkFence fence = VK_NULL_HANDLE;
        bool pending = false;
        uint32_t max_target_width = 0, max_target_height = 0;
        std::unique_ptr<MeshTarget> target;

        explicit State(const VulkanDevice& owner) :
            device(owner), uploads(owner),
            material_data(owner, sizeof(glm::vec4) * 32, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VulkanBufferMemory::upload)
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
            if (pool)
                vkDestroyCommandPool(device.handle(), pool, nullptr);
            if (fence)
                vkDestroyFence(device.handle(), fence, nullptr);
            materials.clear();
            if (pipeline)
                vkDestroyPipeline(device.handle(), pipeline, nullptr);
            if (layout)
                vkDestroyPipelineLayout(device.handle(), layout, nullptr);
            if (descriptor_layout)
                vkDestroyDescriptorSetLayout(device.handle(), descriptor_layout, nullptr);
        }

        void initialize(
            const RenderAuthoringConfig& authoring,
            const CpuRgbaTextureData& atlas,
            const TextureEncoding encoding
        )
        {
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
            constexpr auto color_features = VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT | VK_FORMAT_FEATURE_BLIT_SRC_BIT
                                            | VK_FORMAT_FEATURE_TRANSFER_SRC_BIT;
            if ((color_properties.optimalTilingFeatures & color_features) != color_features)
                throw std::runtime_error(
                    "Vulkan mesh renderer requires RGBA8 UNORM color attachment/readback/blit support"
                );
            std::array<glm::vec4, 32> parameters{};
            static_assert(surface_material_count() <= 32);
            for (size_t i = 0; i < authoring.materials.size(); ++i)
                parameters[i] = {authoring.materials[i].atlas_world_scale, authoring.materials[i].atlas_mix, 0, 0};
            material_data.write(std::as_bytes(std::span{parameters}));
            atlas_source.initialize_rgba(atlas.width, atlas.height, atlas.pixels, encoding, ResourceUpload::cpu);
            const std::array bindings{
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

        auto create_pipeline(const std::filesystem::path& directory) const -> VkPipeline
        {
            MeshShaders shaders{device.handle()};
            shaders.vertex = shaders.load(directory / "mesh.vert.spv", "vertex_main");
            shaders.fragment = shaders.load(directory / "mesh.frag.spv", "fragment_main");
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
                .vertexAttributeDescriptionCount = static_cast<uint32_t>(attributes.size()),
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
                .cullMode = VK_CULL_MODE_BACK_BIT,
                .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
                .lineWidth = 1,
            };
            const VkPipelineMultisampleStateCreateInfo multisample{
                .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
                .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
            };
            const VkPipelineDepthStencilStateCreateInfo depth{
                .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
                .depthTestEnable = VK_TRUE,
                .depthWriteEnable = VK_TRUE,
                .depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL,
            };
            const VkPipelineColorBlendAttachmentState blend_attachment{
                .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT
                                  | VK_COLOR_COMPONENT_A_BIT,
            };
            const VkPipelineColorBlendStateCreateInfo blend{
                .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
                .attachmentCount = 1,
                .pAttachments = &blend_attachment,
            };
            const std::array dynamics{VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
            const VkPipelineDynamicStateCreateInfo dynamic{
                .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
                .dynamicStateCount = static_cast<uint32_t>(dynamics.size()),
                .pDynamicStates = dynamics.data(),
            };
            const VkFormat color = VK_FORMAT_R8G8B8A8_UNORM;
            const VkPipelineRenderingCreateInfo rendering{
                .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
                .colorAttachmentCount = 1,
                .pColorAttachmentFormats = &color,
                .depthAttachmentFormat = VK_FORMAT_D32_SFLOAT,
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
                require_mesh_vk(result, "Pipeline mesh.slang vertex_main/fragment_main from " + directory.string());
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
                VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1},
                VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1}
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
            const VkDescriptorImageInfo image{
                result->texture.sampler(),
                result->texture.view(),
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
            };
            const std::array writes{
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
            };
            vkUpdateDescriptorSets(device.handle(), static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
            return *materials.emplace(cpu.get(), std::move(result)).first->second;
        }

        void prepare(const std::span<const RenderItem> items)
        {
            for (const auto& item : items)
                if (item.pass == RenderPass::Opaque && item.mesh)
                {
                    static_cast<void>(mesh(*item.mesh));
                    static_cast<void>(material(item.diffuse_texture ? *item.diffuse_texture : atlas_source));
                }
        }
        void draw(const std::span<const RenderItem> items, const PerspectiveCamera& camera)
        {
            for (const auto& item : items)
            {
                if (item.pass != RenderPass::Opaque || !item.mesh)
                    continue;
                const auto& geometry = meshes.at(item.mesh->cpu_data().get());
                const auto& texture = item.diffuse_texture ? *item.diffuse_texture : atlas_source;
                const auto set = materials.at(texture.cpu_data().get())->set;
                const auto vertex = geometry.vertices.handle();
                const VkDeviceSize offset = 0;
                vkCmdBindVertexBuffers(commands, 0, 1, &vertex, &offset);
                vkCmdBindIndexBuffer(commands, geometry.indices.handle(), 0, VK_INDEX_TYPE_UINT32);
                vkCmdBindDescriptorSets(commands, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, 0, 1, &set, 0, nullptr);
                DrawPush push{camera.view_projection_matrix() * item.model_matrix, {}, item.diffuse_texture ? 1U : 0U};
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

        auto render(
            const std::span<const RenderItem> items,
            const PerspectiveCamera& camera,
            const int width,
            const int height,
            const std::span<const RenderItem> viewmodel,
            const bool capture
        ) -> CpuRgbaTextureData
        {
            if (width <= 0 || height <= 0)
                throw std::invalid_argument("Vulkan mesh target requires positive dimensions");
            if (static_cast<uint32_t>(width) > max_target_width || static_cast<uint32_t>(height) > max_target_height)
                throw std::invalid_argument("Vulkan mesh target exceeds the device framebuffer/image limits");
            wait();
            std::erase_if(meshes, [](const auto& entry) {
                return entry.second.source.expired();
            });
            std::erase_if(materials, [](const auto& entry) {
                return entry.second->source.expired();
            });
            prepare(items);
            prepare(viewmodel);
            uploads.collect();
            const VkExtent2D extent{static_cast<uint32_t>(width), static_cast<uint32_t>(height)};
            if (!target || target->extent.width != extent.width || target->extent.height != extent.height)
                target = std::make_unique<MeshTarget>(device, extent);
            CpuRgbaTextureData output{width, height, {}};
            if (capture)
                output.pixels.resize(size_t(width) * height * 4);
            require_mesh_vk(vkResetCommandPool(device.handle(), pool, 0), "Failed to reset mesh commands");
            const VkCommandBufferBeginInfo begin{
                .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
                .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
            };
            require_mesh_vk(vkBeginCommandBuffer(commands, &begin), "Failed to begin mesh frame");
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
                    .dstAccessMask =
                        VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
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
            VkRenderingAttachmentInfo color{
                .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
                .imageView = target->color.view,
                .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
                .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
                .clearValue = {.color = {{0, 0, 0, 1}}}
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
            const VkViewport
                viewport{0, static_cast<float>(height), static_cast<float>(width), -static_cast<float>(height), 0, 1};
            const VkRect2D scissor{{0, 0}, extent};
            vkCmdSetViewport(commands, 0, 1, &viewport);
            vkCmdSetScissor(commands, 0, 1, &scissor);
            vkCmdBindPipeline(commands, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
            vkCmdBeginRendering(commands, &rendering);
            draw(items, camera);
            vkCmdEndRendering(commands);
            if (!viewmodel.empty())
            {
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
                draw(viewmodel, camera);
                vkCmdEndRendering(commands);
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
            {
                const VkBufferImageCopy copy{
                    .imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1},
                    .imageExtent = {extent.width, extent.height, 1}
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
        const TextureEncoding encoding
    ) : state_(std::make_unique<State>(device))
    {
        state_->initialize(authoring, atlas, encoding);
        reload_shaders(shader_directory);
    }
    VulkanMeshRenderer::~VulkanMeshRenderer() noexcept = default;

    void VulkanMeshRenderer::reload_shaders(const std::filesystem::path& directory)
    {
        state_->wait();
        const auto replacement = state_->create_pipeline(directory);
        if (state_->pipeline)
            vkDestroyPipeline(state_->device.handle(), state_->pipeline, nullptr);
        state_->pipeline = replacement;
    }
    auto VulkanMeshRenderer::render_image(
        const std::span<const RenderItem> items,
        const PerspectiveCamera& camera,
        const int width,
        const int height,
        const std::span<const RenderItem> viewmodel
    ) -> CpuRgbaTextureData
    {
        return state_->render(items, camera, width, height, viewmodel, true);
    }
    auto VulkanMeshRenderer::render_color_image(
        const VkDevice presenting_device,
        const std::span<const RenderItem> items,
        const PerspectiveCamera& camera,
        const VkExtent2D extent,
        const std::span<const RenderItem> viewmodel
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
            false
        ));
        return state_->target->color.image;
    }
} // namespace RelayCore::render
