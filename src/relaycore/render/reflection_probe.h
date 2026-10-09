#pragma once

#include "relaycore/render/irradiance_probes.h"

namespace RelayCore::render
{
    struct ReflectionProbeSettings {
        glm::vec3 position{}, mins{}, maxs{};
        std::uint32_t resolution = 64;
        std::uint32_t samples = 128;
        auto operator==(const ReflectionProbeSettings&) const -> bool = default;
    };

    // Vulkan face order +X,-X,+Y,-Y,+Z,-Z; linear radiance, mip-major RGBA32F.
    struct ReflectionProbe {
        ReflectionProbeSettings settings;
        std::uint64_t source_signature = 0;
        std::vector<std::vector<glm::vec4>> mips;
    };

    void validate_reflection_settings(const ReflectionProbeSettings& settings);
    void validate_reflection_probe(const ReflectionProbe& probe);
    [[nodiscard]] auto reflection_source_signature(
        const IrradianceBakeScene& scene,
        const ReflectionProbeSettings& settings,
        glm::vec3 ambient
    ) -> std::uint64_t;
    [[nodiscard]] auto reflection_face_direction(std::uint32_t face, glm::vec2 uv) -> glm::vec3;
    [[nodiscard]] auto sample_reflection_probe(const ReflectionProbe& probe, glm::vec3 direction, float roughness)
        -> glm::vec3;
    [[nodiscard]] auto
    bake_reflection_probe(const IrradianceBakeScene& scene, const ReflectionProbeSettings& settings, glm::vec3 ambient)
        -> ReflectionProbe;
    void write_reflection_probe(const std::filesystem::path& path, const ReflectionProbe& probe);
    [[nodiscard]] auto read_reflection_probe(const std::filesystem::path& path) -> ReflectionProbe;
} // namespace RelayCore::render
