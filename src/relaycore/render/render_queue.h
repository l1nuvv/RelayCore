#pragma once

#include "relaycore/render/material.h"
#include "relaycore/render/mesh.h"

#include <cstdint>
#include <glm/mat4x4.hpp>
#include <optional>
#include <span>
#include <vector>

namespace RelayCore::render
{
    class Texture2D;
    enum class RenderPass : std::uint8_t {
        Opaque = 0,
        Overlay,
        Transparent,
    };

    struct LocalLight {
        glm::vec3 position{0.0F};
        glm::vec3 color{0.0F};
        float radius = 1.0F;
    };

    struct RenderItem {
        const StaticColorMesh* mesh = nullptr;
        glm::mat4 model_matrix{1.0F};
        RenderPass pass = RenderPass::Opaque;
        const Texture2D* diffuse_texture = nullptr;
        bool casts_directional_shadow = true;
        const Texture2D* normal_texture = nullptr;
        float normal_scale = 1;
        std::optional<MetallicRoughness> metallic_roughness;
    };

    struct SpotLight {
        glm::vec3 position{};
        glm::vec3 direction{0, 0, -1};
        glm::vec3 color{1.0F, 0.94F, 0.84F};
        float range = 18.0F;
        bool enabled = false;
    };

    class RenderQueue {
    public:
        void clear() noexcept
        {
            items_.clear();
        }

        void reserve(const std::size_t count)
        {
            items_.reserve(count);
        }

        void push(
            const StaticColorMesh& mesh,
            const glm::mat4& model_matrix,
            const RenderPass pass = RenderPass::Opaque,
            const Texture2D* diffuse_texture = nullptr,
            const bool casts_directional_shadow = true,
            const Texture2D* normal_texture = nullptr,
            const float normal_scale = 1,
            const std::optional<MetallicRoughness> metallic_roughness = std::nullopt
        )
        {
            items_.push_back(RenderItem{
                .mesh = &mesh,
                .model_matrix = model_matrix,
                .pass = pass,
                .diffuse_texture = diffuse_texture,
                .casts_directional_shadow = casts_directional_shadow,
                .normal_texture = normal_texture,
                .normal_scale = normal_scale,
                .metallic_roughness = metallic_roughness
            });
        }

        [[nodiscard]] auto items() const noexcept -> std::span<const RenderItem>
        {
            return items_;
        }

    private:
        std::vector<RenderItem> items_{};
    };
} // namespace RelayCore::render
