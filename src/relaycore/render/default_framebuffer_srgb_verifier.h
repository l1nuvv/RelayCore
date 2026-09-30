#pragma once

#include <array>
#include <cstdint>
#include <string>

namespace RelayCore::render
{
    struct DefaultFramebufferSrgbVerification {
        int reported_encoding = 0;
        std::array<std::uint8_t, 4> disabled_pixel{};
        std::array<std::uint8_t, 4> enabled_pixel{};
    };

    [[nodiscard]] auto verify_default_framebuffer_srgb_behavior() -> DefaultFramebufferSrgbVerification;

    [[nodiscard]] auto default_framebuffer_srgb_behavior_passes(const DefaultFramebufferSrgbVerification& verification
    ) noexcept -> bool;

    [[nodiscard]] auto framebuffer_color_encoding_name(int encoding) noexcept -> const char*;

    [[nodiscard]] auto format_rgba8(const std::array<std::uint8_t, 4>& pixel) -> std::string;
} // namespace RelayCore::render
