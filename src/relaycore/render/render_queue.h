#pragma once

#include "relaycore/render/material.h"
#include "relaycore/render/mesh.h"

#include <cstddef>
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

    inline constexpr std::size_t kMaxLocalLights = 32;
    inline constexpr std::size_t kMaxPointShadowLights = 8;

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
        bool casts_shadow = true;
        const Texture2D* normal_texture = nullptr;
        float normal_scale = 1;
        std::optional<MetallicRoughness> metallic_roughness;
        const Texture2D* metallic_roughness_map = nullptr;
        const Texture2D* occlusion_texture = nullptr;
        float occlusion_strength = 1;
        const Texture2D* emissive_texture = nullptr;
        glm::vec3 emissive_factor{0};
        // Missing correspondence is rejected by temporal consumers, never inferred from draw order.
        std::optional<glm::mat4> previous_model_matrix;
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
            const std::optional<MetallicRoughness> metallic_roughness = std::nullopt,
            const Texture2D* metallic_roughness_map = nullptr,
            const Texture2D* occlusion_texture = nullptr,
            const float occlusion_strength = 1,
            const Texture2D* emissive_texture = nullptr,
            const glm::vec3 emissive_factor = glm::vec3{0},
            const bool casts_shadow = true
        )
        {
            items_.push_back(RenderItem{
                .mesh = &mesh,
                .model_matrix = model_matrix,
                .pass = pass,
                .diffuse_texture = diffuse_texture,
                .casts_directional_shadow = casts_directional_shadow,
                .casts_shadow = casts_shadow,
                .normal_texture = normal_texture,
                .normal_scale = normal_scale,
                .metallic_roughness = metallic_roughness,
                .metallic_roughness_map = metallic_roughness_map,
                .occlusion_texture = occlusion_texture,
                .occlusion_strength = occlusion_strength,
                .emissive_texture = emissive_texture,
                .emissive_factor = emissive_factor
            });
        }

        [[nodiscard]] auto items() const noexcept -> std::span<const RenderItem>
        {
            return items_;
        }

        [[nodiscard]] auto items() noexcept -> std::span<RenderItem>
        {
            return items_;
        }

    private:
        std::vector<RenderItem> items_{};
    };
} // namespace RelayCore::render
