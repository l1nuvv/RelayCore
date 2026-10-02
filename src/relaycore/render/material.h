#pragma once

namespace RelayCore::render
{
    struct MetallicRoughness {
        float roughness = 1;
        float metallic = 1;
        auto operator==(const MetallicRoughness&) const -> bool = default;
    };
} // namespace RelayCore::render
