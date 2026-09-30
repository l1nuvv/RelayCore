#pragma once

#include "relaycore/core/engine_config.h"
#include "relaycore/render/render_authoring_config.h"

#include <filesystem>
#include <glm/vec3.hpp>
#include <optional>
#include <string>
#include <unordered_set>
#include <vector>

namespace RelayCore::render
{
    struct RendererAuthoringOverrides {
        std::optional<float> indirect_bounce_strength{};
        std::optional<bool> fog_enabled{};
        std::optional<glm::vec3> fog_color{};
        std::optional<float> fog_start{};
        std::optional<float> fog_end{};
        std::optional<glm::vec3> shadow_light_color{};
        std::optional<float> shadow_light_intensity{};
        std::optional<float> shadow_strength{};
        std::optional<float> shadow_contact_strength{};

        [[nodiscard]] auto empty() const noexcept -> bool;
    };

    struct MaterialAuthoringOverrides {
        RelayCore::SurfaceMaterial material = RelayCore::SurfaceMaterial::Concrete;
        std::string canonical_name{};
        std::optional<glm::vec3> base_color{};
        std::optional<float> ambient_bias{};
        std::optional<float> bounce_mix{};

        [[nodiscard]] auto empty() const noexcept -> bool;
    };

    struct SolidVisualOverrides {
        std::string authoring_id{};
        std::optional<glm::vec3> tint{};
        std::optional<float> brightness{};

        [[nodiscard]] auto empty() const noexcept -> bool;
    };

    struct RenderLightVisualOverrides {
        std::string authoring_id{};
        std::optional<glm::vec3> color{};
        std::optional<float> radius{};
        std::optional<float> intensity{};

        [[nodiscard]] auto empty() const noexcept -> bool;
    };

    struct LevelVisualOverrides {
        std::filesystem::path level_path{};
        std::vector<SolidVisualOverrides> solids{};
        std::vector<RenderLightVisualOverrides> render_lights{};

        [[nodiscard]] auto empty() const noexcept -> bool;
    };

    struct PipelineAuthoringOverlay {
        std::filesystem::path source_path{};
        RendererAuthoringOverrides renderer{};
        std::vector<MaterialAuthoringOverrides> materials{};
        std::vector<LevelVisualOverrides> levels{};

        [[nodiscard]] auto empty() const noexcept -> bool;
    };

    struct LevelAuthoringIdentityIndex {
        std::unordered_set<std::string> solid_ids{};
        std::unordered_set<std::string> render_light_ids{};
    };

    [[nodiscard]] auto load_pipeline_authoring_overlay(const std::filesystem::path& overlay_path)
        -> PipelineAuthoringOverlay;

    void apply_renderer_authoring_overrides(RendererConfig& config, const RendererAuthoringOverrides& overrides);

    void apply_material_authoring_overrides(
        RenderAuthoringConfig& config,
        const std::vector<MaterialAuthoringOverrides>& overrides
    );

    void validate_level_authoring_overlay_targets(
        const LevelVisualOverrides& overrides,
        const LevelAuthoringIdentityIndex& identities
    );
} // namespace RelayCore::render
