#pragma once

#include "relaycore/render/render_queue.h"

#include <array>
#include <filesystem>
#include <glm/vec3.hpp>
#include <vector>

namespace RelayCore::render
{
    constexpr std::size_t max_irradiance_probes = 64;
    using IrradianceSH = std::array<glm::vec3, 9>;

    struct IrradianceProbeSettings {
        glm::vec3 mins{}, maxs{};
        glm::uvec3 dimensions{2};
        std::uint32_t rays = 1024;
    };

    struct IrradianceTriangle {
        glm::vec3 a{}, b{}, c{};
        glm::vec3 diffuse{};
    };

    struct IrradianceBakeScene {
        std::vector<IrradianceTriangle> triangles;
        std::vector<LocalLight> lights;
    };

    // Linear RGB diffuse irradiance / pi, cosine-convolved real SH, x-fastest grid.
    struct IrradianceProbeVolume {
        IrradianceProbeSettings settings;
        std::uint64_t source_signature = 0;
        std::vector<IrradianceSH> probes;
    };

    void validate_irradiance_settings(const IrradianceProbeSettings& settings);
    void validate_irradiance_volume(const IrradianceProbeVolume& volume);
    [[nodiscard]] auto
    irradiance_source_signature(const IrradianceBakeScene& scene, const IrradianceProbeSettings& settings)
        -> std::uint64_t;
    [[nodiscard]] auto bake_irradiance_probes(const IrradianceBakeScene& scene, const IrradianceProbeSettings& settings)
        -> IrradianceProbeVolume;
    [[nodiscard]] auto
    sample_irradiance_probes(const IrradianceProbeVolume& volume, glm::vec3 position, glm::vec3 normal) -> glm::vec3;
    void write_irradiance_probes(const std::filesystem::path& path, const IrradianceProbeVolume& volume);
    [[nodiscard]] auto read_irradiance_probes(const std::filesystem::path& path) -> IrradianceProbeVolume;
} // namespace RelayCore::render
