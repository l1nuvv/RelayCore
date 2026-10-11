#pragma once

#include <array>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

namespace RelayCore::render
{
    class PerspectiveCamera;
    struct RendererConfig;
    struct LocalLight;
    struct SpotLight;
    struct RenderItem;
    [[nodiscard]] auto safe_shadow_direction(const glm::vec3& direction) -> glm::vec3;
    [[nodiscard]] auto choose_shadow_up(const glm::vec3& direction) -> glm::vec3;
    [[nodiscard]] auto round_to_shadow_texel(float value, float texel_world_size) -> float;
    [[nodiscard]] auto shadow_frustum_planes(const glm::mat4& view_projection) -> std::array<glm::vec4, 6>;
    [[nodiscard]] auto
    sphere_intersects_shadow_frustum(const std::array<glm::vec4, 6>& planes, const glm::vec3& center, float radius)
        -> bool;
    [[nodiscard]] auto shadow_light_view_projection(
        const PerspectiveCamera& camera,
        const RendererConfig& config,
        float focus_distance,
        float radius,
        float depth
    ) -> glm::mat4;
    [[nodiscard]] auto spot_shadow_view_projection(const SpotLight& light) -> glm::mat4;
    [[nodiscard]] auto point_shadow_view_projection(const LocalLight& light, int face) -> glm::mat4;
    [[nodiscard]] auto shadow_caster_sphere(const RenderItem& item) -> glm::vec4;
} // namespace RelayCore::render
