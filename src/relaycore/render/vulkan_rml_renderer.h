#pragma once

#include <RmlUi/Core/RenderInterface.h>
#include <filesystem>
#include <memory>
#include <vulkan/vulkan.h>

namespace RelayCore::render
{
    class VulkanDevice;

    class VulkanRmlRenderer final : public Rml::RenderInterface {
    public:
        VulkanRmlRenderer(const VulkanDevice& device, const std::filesystem::path& shaders);
        ~VulkanRmlRenderer() override;
        void begin_frame(int width, int height);
        void record(VkCommandBuffer commands);
        auto CompileGeometry(Rml::Span<const Rml::Vertex> vertices, Rml::Span<const int> indices)
            -> Rml::CompiledGeometryHandle override;
        void RenderGeometry(Rml::CompiledGeometryHandle geometry, Rml::Vector2f translation, Rml::TextureHandle texture)
            override;
        void ReleaseGeometry(Rml::CompiledGeometryHandle geometry) override;
        auto LoadTexture(Rml::Vector2i& dimensions, const Rml::String& source) -> Rml::TextureHandle override;
        auto GenerateTexture(Rml::Span<const Rml::byte> pixels, Rml::Vector2i dimensions)
            -> Rml::TextureHandle override;
        void ReleaseTexture(Rml::TextureHandle texture) override;
        void EnableScissorRegion(bool enable) override;
        void SetScissorRegion(Rml::Rectanglei region) override;
        void SetTransform(const Rml::Matrix4f* transform) override;

    private:
        struct State;
        std::unique_ptr<State> state_;
    };
} // namespace RelayCore::render
