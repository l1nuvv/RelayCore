#pragma once

#include "relaycore/render/irradiance_probes.h"

#include <memory>
#include <optional>

namespace RelayCore
{
    struct BlockoutLevel;
}

namespace RelayCore::render
{
    struct RenderAuthoringConfig;
}

namespace RelayGame
{
    struct PreparedWorldSceneData;
    struct WorldIrradianceSource {
        RelayCore::render::IrradianceProbeSettings settings;
        std::filesystem::path product;
    };

    [[nodiscard]] auto world_irradiance_source(const RelayCore::BlockoutLevel& level)
        -> std::optional<WorldIrradianceSource>;
    [[nodiscard]] auto build_world_irradiance_scene(
        const PreparedWorldSceneData& scene,
        const RelayCore::render::RenderAuthoringConfig& authoring
    ) -> RelayCore::render::IrradianceBakeScene;
    [[nodiscard]] auto load_world_irradiance(
        const RelayCore::BlockoutLevel& level,
        const PreparedWorldSceneData& scene,
        const RelayCore::render::RenderAuthoringConfig& authoring
    ) -> std::shared_ptr<const RelayCore::render::IrradianceProbeVolume>;
} // namespace RelayGame
