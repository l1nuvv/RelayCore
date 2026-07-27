#pragma once

#include <glm/vec3.hpp>

namespace RelayCore::render
{
    [[nodiscard]] auto srgb_channel_to_linear(float value) -> float;

    [[nodiscard]] auto srgb_visual_rgb_to_linear(const glm::vec3& visual_srgb) -> glm::vec3;

    [[nodiscard]] auto
    compose_legacy_surface_color(const glm::vec3& base_color_srgb, const glm::vec3& tint_multiplier, float brightness)
        -> glm::vec3;

    [[nodiscard]] auto
    compose_linear_surface_color(const glm::vec3& base_color_srgb, const glm::vec3& tint_multiplier, float brightness)
        -> glm::vec3;
} // namespace RelayCore::render
