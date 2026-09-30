#include "relaycore/render/authored_color.h"

#include <cmath>
#include <glm/common.hpp>
#include <stdexcept>

namespace RelayCore::render
{
    namespace
    {
        void validate_visual_srgb_channel(const float value)
        {
            if (!std::isfinite(value) || value < 0.0F || value > 1.0F)
            {
                throw std::invalid_argument("visual sRGB channel must be finite and in [0, 1]");
            }
        }

        void validate_visual_srgb(const glm::vec3& visual_srgb)
        {
            validate_visual_srgb_channel(visual_srgb.x);
            validate_visual_srgb_channel(visual_srgb.y);
            validate_visual_srgb_channel(visual_srgb.z);
        }

        void validate_multiplier(const glm::vec3& tint_multiplier, const float brightness)
        {
            if (!std::isfinite(tint_multiplier.x) || !std::isfinite(tint_multiplier.y)
                || !std::isfinite(tint_multiplier.z) || !std::isfinite(brightness))
            {
                throw std::invalid_argument("surface colour multipliers must be finite");
            }
        }
    } // namespace

    auto srgb_channel_to_linear(const float value) -> float
    {
        validate_visual_srgb_channel(value);
        if (value <= 0.04045F)
        {
            return value / 12.92F;
        }
        return std::pow((value + 0.055F) / 1.055F, 2.4F);
    }

    auto srgb_visual_rgb_to_linear(const glm::vec3& visual_srgb) -> glm::vec3
    {
        validate_visual_srgb(visual_srgb);
        return {
            srgb_channel_to_linear(visual_srgb.x),
            srgb_channel_to_linear(visual_srgb.y),
            srgb_channel_to_linear(visual_srgb.z),
        };
    }

    auto visual_rgb_for_pipeline(const glm::vec3& visual_srgb, const ColorPipelineMode mode) -> glm::vec3
    {
        return mode == ColorPipelineMode::hardware_srgb ? srgb_visual_rgb_to_linear(visual_srgb) : visual_srgb;
    }

    auto compose_legacy_surface_color(
        const glm::vec3& base_color_srgb,
        const glm::vec3& tint_multiplier,
        const float brightness
    ) -> glm::vec3
    {
        validate_visual_srgb(base_color_srgb);
        validate_multiplier(tint_multiplier, brightness);
        return glm::clamp(base_color_srgb * tint_multiplier * brightness, glm::vec3{0.0F}, glm::vec3{1.0F});
    }

    auto compose_linear_surface_color(
        const glm::vec3& base_color_srgb,
        const glm::vec3& tint_multiplier,
        const float brightness
    ) -> glm::vec3
    {
        validate_multiplier(tint_multiplier, brightness);
        return srgb_visual_rgb_to_linear(base_color_srgb) * tint_multiplier * brightness;
    }
} // namespace RelayCore::render
