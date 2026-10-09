#include "relaycore/render/vulkan_rml_renderer.h"

#include "relaycore/core/log.h"
#include "relaycore/core/profiling.h"
#include "relaycore/render/vulkan_device.h"
#include "relaycore/render/vulkan_uploads.h"

#include <RmlUi/Core/Matrix4.h>
#include <algorithm>
#include <array>
#include <fstream>
#include <glm/gtc/type_ptr.hpp>
#include <glm/mat4x4.hpp>
#include <stdexcept>
#include <unordered_map>
#include <vector>

namespace RelayCore::render
{
    namespace
    {
        void require_ui_vk(VkResult result, const char* operation)
        {
            if (result != VK_SUCCESS)
                throw std::runtime_error(std::string(operation) + ": VkResult " + std::to_string(result));
        }
        struct UiPush {
            glm::mat4 transform;
            glm::vec2 translation;
        };
    } // namespace
    struct VulkanRmlRenderer::State {
        const VulkanDevice& device;
        VulkanUploads uploads;
        VkDescriptorSetLayout descriptor_layout{};
        VkDescriptorPool pool{};
        VkPipelineLayout layout{};
        VkPipeline pipeline{};
        struct Geometry {
            VulkanBuffer vertices, indices;
            uint32_t count;
        };
        struct Texture {
            VulkanTexture image;
            VkDescriptorSet set;
        };
        struct Draw {
            std::shared_ptr<Geometry> geometry;
            std::shared_ptr<Texture> texture;
            UiPush push;
            VkRect2D clip;
        };
        std::unordered_map<uintptr_t, std::shared_ptr<Geometry>> geometries;
        std::unordered_map<uintptr_t, std::shared_ptr<Texture>> textures;
        std::vector<Draw> draws;
        uintptr_t next_handle = 1;
        uintptr_t white = 0;
        VkExtent2D extent{};
        VkRect2D scissor{};
        bool clipping = false;
        glm::mat4 transform{1};
        explicit State(const VulkanDevice& owner) : device(owner), uploads(owner) {}
        ~State()
        {
            draws.clear();
            geometries.clear();
            textures.clear();
            if (pipeline)
                vkDestroyPipeline(device.handle(), pipeline, nullptr);
            if (layout)
                vkDestroyPipelineLayout(device.handle(), layout, nullptr);
            if (pool)
                vkDestroyDescriptorPool(device.handle(), pool, nullptr);
            if (descriptor_layout)
                vkDestroyDescriptorSetLayout(device.handle(), descriptor_layout, nullptr);
        }
        void initialize(const std::filesystem::path& shaders)
        {
            const VkDescriptorSetLayoutBinding
                binding{0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT};
            const VkDescriptorSetLayoutCreateInfo descriptors{
                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
                .bindingCount = 1,
                .pBindings = &binding
            };
            require_ui_vk(
                vkCreateDescriptorSetLayout(device.handle(), &descriptors, nullptr, &descriptor_layout),
                "RmlUi descriptor layout"
            );
            const VkDescriptorPoolSize pool_size{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1024};
            const VkDescriptorPoolCreateInfo pool_info{
                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
                .flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
                .maxSets = 1024,
                .poolSizeCount = 1,
                .pPoolSizes = &pool_size
            };
            require_ui_vk(vkCreateDescriptorPool(device.handle(), &pool_info, nullptr, &pool), "RmlUi descriptor pool");
            const VkPushConstantRange range{VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(UiPush)};
            const VkPipelineLayoutCreateInfo layout_info{
                .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
                .setLayoutCount = 1,
                .pSetLayouts = &descriptor_layout,
                .pushConstantRangeCount = 1,
                .pPushConstantRanges = &range
            };
            require_ui_vk(
                vkCreatePipelineLayout(device.handle(), &layout_info, nullptr, &layout),
                "RmlUi pipeline layout"
            );
            std::array<VkShaderModule, 2> modules{};
            try
            {
                for (size_t i = 0; i < modules.size(); ++i)
                {
                    const auto file = shaders / (i == 0 ? "player_ui.vert.spv" : "player_ui.frag.spv");
                    std::ifstream stream(file, std::ios::binary | std::ios::ate);
                    if (!stream || stream.tellg() <= 0 || size_t(stream.tellg()) % 4)
                        throw std::runtime_error("Cannot load UI SPIR-V: " + file.string());
                    std::vector<uint32_t> code(size_t(stream.tellg()) / 4);
                    stream.seekg(0);
                    if (!stream.read(reinterpret_cast<char*>(code.data()), std::streamsize(code.size() * 4)))
                        throw std::runtime_error("Cannot read UI SPIR-V: " + file.string());
                    const VkShaderModuleCreateInfo info{
                        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
                        .codeSize = code.size() * 4,
                        .pCode = code.data()
                    };
                    require_ui_vk(vkCreateShaderModule(device.handle(), &info, nullptr, &modules[i]), "RmlUi shader");
                }
                const std::array stages{
                    VkPipelineShaderStageCreateInfo{
                        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                        .stage = VK_SHADER_STAGE_VERTEX_BIT,
                        .module = modules[0],
                        .pName = "vertex_main"
                    },
                    VkPipelineShaderStageCreateInfo{
                        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                        .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
                        .module = modules[1],
                        .pName = "fragment_main"
                    }
                };
                const VkVertexInputBindingDescription vertex_binding{
                    0,
                    sizeof(Rml::Vertex),
                    VK_VERTEX_INPUT_RATE_VERTEX
                };
                const std::array attributes{
                    VkVertexInputAttributeDescription{0, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(Rml::Vertex, position)},
                    VkVertexInputAttributeDescription{1, 0, VK_FORMAT_R8G8B8A8_UNORM, offsetof(Rml::Vertex, colour)},
                    VkVertexInputAttributeDescription{2, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(Rml::Vertex, tex_coord)}
                };
                const VkPipelineVertexInputStateCreateInfo vertices{
                    .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
                    .vertexBindingDescriptionCount = 1,
                    .pVertexBindingDescriptions = &vertex_binding,
                    .vertexAttributeDescriptionCount = uint32_t(attributes.size()),
                    .pVertexAttributeDescriptions = attributes.data()
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
                const VkPipelineMultisampleStateCreateInfo samples{
                    .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
                    .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT
                };
                // RmlUi supplies encoded, premultiplied colors. Compose after scene display conversion on its UNORM view.
                const VkPipelineColorBlendAttachmentState attachment{
                    .blendEnable = VK_TRUE,
                    .srcColorBlendFactor = VK_BLEND_FACTOR_ONE,
                    .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
                    .colorBlendOp = VK_BLEND_OP_ADD,
                    .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
                    .dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
                    .alphaBlendOp = VK_BLEND_OP_ADD,
                    .colorWriteMask = 15
                };
                const VkPipelineColorBlendStateCreateInfo blend{
                    .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
                    .attachmentCount = 1,
                    .pAttachments = &attachment
                };
                const std::array dynamic_states{VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
                const VkPipelineDynamicStateCreateInfo dynamic{
                    .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
                    .dynamicStateCount = uint32_t(dynamic_states.size()),
                    .pDynamicStates = dynamic_states.data()
                };
                const VkFormat format = VK_FORMAT_R8G8B8A8_UNORM;
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
                    .pMultisampleState = &samples,
                    .pColorBlendState = &blend,
                    .pDynamicState = &dynamic,
                    .layout = layout
                };
                require_ui_vk(
                    vkCreateGraphicsPipelines(device.handle(), {}, 1, &info, nullptr, &pipeline),
                    "RmlUi pipeline"
                );
            } catch (...)
            {
                for (auto module : modules)
                    if (module)
                        vkDestroyShaderModule(device.handle(), module, nullptr);
                throw;
            }
            for (auto module : modules)
                vkDestroyShaderModule(device.handle(), module, nullptr);
        }
    };
    VulkanRmlRenderer::VulkanRmlRenderer(const VulkanDevice& device, const std::filesystem::path& shaders) :
        state_(std::make_unique<State>(device))
    {
        state_->initialize(shaders);
        const std::array<Rml::byte, 4> white{255, 255, 255, 255};
        state_->white = GenerateTexture({white.data(), white.size()}, {1, 1});
    }
    VulkanRmlRenderer::~VulkanRmlRenderer() = default;
    void VulkanRmlRenderer::begin_frame(int width, int height)
    {
        state_->draws.clear();
        state_->extent = {uint32_t(width), uint32_t(height)};
        state_->scissor = {{0, 0}, state_->extent};
        state_->clipping = false;
        state_->transform = glm::mat4{1};
    }
    auto VulkanRmlRenderer::CompileGeometry(Rml::Span<const Rml::Vertex> vertices, Rml::Span<const int> indices)
        -> Rml::CompiledGeometryHandle
    {
        RELAYCORE_PROFILE_ZONE_N("RmlUi geometry upload");
        auto geometry = std::make_shared<State::Geometry>();
        geometry->vertices = VulkanBuffer(
            state_->device,
            vertices.size() * sizeof(Rml::Vertex),
            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
            VulkanBufferMemory::upload
        );
        geometry->indices = VulkanBuffer(
            state_->device,
            indices.size() * sizeof(int),
            VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
            VulkanBufferMemory::upload
        );
        geometry->vertices.write(std::as_bytes(std::span{vertices.data(), vertices.size()}));
        geometry->indices.write(std::as_bytes(std::span{indices.data(), indices.size()}));
        geometry->count = uint32_t(indices.size());
        const auto handle = state_->next_handle++;
        state_->geometries.emplace(handle, std::move(geometry));
        return handle;
    }
    void VulkanRmlRenderer::RenderGeometry(
        Rml::CompiledGeometryHandle geometry,
        Rml::Vector2f translation,
        Rml::TextureHandle texture
    )
    {
        glm::mat4 projection{1};
        projection[0][0] = 2.0F / float(state_->extent.width);
        projection[1][1] = 2.0F / float(state_->extent.height);
        projection[3][0] = projection[3][1] = -1;
        state_->draws.push_back(
            {state_->geometries.at(geometry),
             state_->textures.at(texture ? texture : state_->white),
             {projection * state_->transform, {translation.x, translation.y}},
             state_->clipping ? state_->scissor : VkRect2D{{0, 0}, state_->extent}}
        );
    }
    void VulkanRmlRenderer::ReleaseGeometry(Rml::CompiledGeometryHandle geometry)
    {
        state_->geometries.erase(geometry);
    }
    auto VulkanRmlRenderer::LoadTexture(Rml::Vector2i&, const Rml::String& source) -> Rml::TextureHandle
    {
        log::error("RmlUi image loading is unsupported by the current authored UI: {}", source);
        return 0;
    }
    auto VulkanRmlRenderer::GenerateTexture(Rml::Span<const Rml::byte> pixels, Rml::Vector2i dimensions)
        -> Rml::TextureHandle
    {
        RELAYCORE_PROFILE_ZONE_N("RmlUi atlas upload");
        CpuRgbaTextureData data{dimensions.x, dimensions.y, {pixels.begin(), pixels.end()}};
        auto upload = state_->uploads.upload_rgba(data, TextureEncoding::linear);
        state_->uploads.wait(upload.completion);
        state_->uploads.collect();
        auto texture = std::make_shared<State::Texture>();
        texture->image = std::move(upload.texture);
        const VkDescriptorSetAllocateInfo allocate{
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
            .descriptorPool = state_->pool,
            .descriptorSetCount = 1,
            .pSetLayouts = &state_->descriptor_layout
        };
        require_ui_vk(
            vkAllocateDescriptorSets(state_->device.handle(), &allocate, &texture->set),
            "RmlUi texture descriptor"
        );
        const VkDescriptorImageInfo image{
            texture->image.sampler(),
            texture->image.view(),
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
        };
        const VkWriteDescriptorSet write{
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = texture->set,
            .dstBinding = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .pImageInfo = &image
        };
        vkUpdateDescriptorSets(state_->device.handle(), 1, &write, 0, nullptr);
        const auto handle = state_->next_handle++;
        state_->textures.emplace(handle, std::move(texture));
        return handle;
    }
    void VulkanRmlRenderer::ReleaseTexture(Rml::TextureHandle texture)
    {
        const auto it = state_->textures.find(texture);
        if (it != state_->textures.end())
        {
            // The mesh UI submission is synchronous; the previous frame has completed before document updates.
            require_ui_vk(
                vkFreeDescriptorSets(state_->device.handle(), state_->pool, 1, &it->second->set),
                "RmlUi texture release"
            );
            state_->textures.erase(it);
        }
    }
    void VulkanRmlRenderer::EnableScissorRegion(bool enable)
    {
        state_->clipping = enable;
    }
    void VulkanRmlRenderer::SetScissorRegion(Rml::Rectanglei region)
    {
        const auto min = region.TopLeft(), max = region.BottomRight();
        const int x = std::clamp(min.x, 0, int(state_->extent.width)),
                  y = std::clamp(min.y, 0, int(state_->extent.height));
        state_->scissor = {
            {x, y},
            {uint32_t(std::max(0, std::min(max.x, int(state_->extent.width)) - x)),
             uint32_t(std::max(0, std::min(max.y, int(state_->extent.height)) - y))}
        };
    }
    void VulkanRmlRenderer::SetTransform(const Rml::Matrix4f* transform)
    {
        state_->transform = glm::mat4{1};
        if (transform)
            for (int column = 0; column < 4; ++column)
                for (int row = 0; row < 4; ++row)
                    state_->transform[column][row] = transform->GetColumn(column)[row];
    }
    void VulkanRmlRenderer::record(VkCommandBuffer commands)
    {
        const VkViewport viewport{0, 0, float(state_->extent.width), float(state_->extent.height), 0, 1};
        vkCmdSetViewport(commands, 0, 1, &viewport);
        vkCmdBindPipeline(commands, VK_PIPELINE_BIND_POINT_GRAPHICS, state_->pipeline);
        for (const auto& draw : state_->draws)
        {
            if (!draw.clip.extent.width || !draw.clip.extent.height)
                continue;
            vkCmdSetScissor(commands, 0, 1, &draw.clip);
            const VkBuffer buffer = draw.geometry->vertices.handle();
            const VkDeviceSize offset = 0;
            vkCmdBindVertexBuffers(commands, 0, 1, &buffer, &offset);
            vkCmdBindIndexBuffer(commands, draw.geometry->indices.handle(), 0, VK_INDEX_TYPE_UINT32);
            vkCmdBindDescriptorSets(
                commands,
                VK_PIPELINE_BIND_POINT_GRAPHICS,
                state_->layout,
                0,
                1,
                &draw.texture->set,
                0,
                nullptr
            );
            vkCmdPushConstants(commands, state_->layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(UiPush), &draw.push);
            vkCmdDrawIndexed(commands, draw.geometry->count, 1, 0, 0, 0);
        }
    }
} // namespace RelayCore::render
