#pragma once

namespace RelayCore::render
{
    enum class ColorPipelineMode {
        legacy_gamma,
        hardware_srgb,
    };

    [[nodiscard]] constexpr auto color_pipeline_mode_name(const ColorPipelineMode mode) noexcept -> const char*
    {
        return mode == ColorPipelineMode::hardware_srgb ? "hardware_srgb" : "legacy_gamma";
    }
} // namespace RelayCore::render
