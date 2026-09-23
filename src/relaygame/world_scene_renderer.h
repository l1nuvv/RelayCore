#pragma once

#include "relaycore/gameplay/combat_runtime.h"
#include "relaycore/gameplay/interaction_runtime.h"
#include "relaycore/render/blockout_mesh_builder.h"
#include "relaycore/render/mesh.h"
#include "relaycore/render/render_authoring_config.h"
#include "relaycore/render/render_diagnostics.h"
#include "relaycore/render/render_queue.h"
#include "relaycore/render/static_model.h"
#include "relaygame/enemy_runtime.h"
#include "relaygame/scientist_pose.h"
#include "relaygame/static_props.h"
#include "relaygame/world_environment.h"

#include <array>
#include <cstdint>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <span>
#include <string>
#include <vector>

namespace RelayCore
{
    struct BlockoutLevel;
    class Renderer;
} // namespace RelayCore

namespace RelayCore::render
{
    class PerspectiveCamera;
}

namespace RelayGame
{
    struct WorldLocalLight {
        RelayCore::render::LocalLight light{};
        std::string required_target{};
    };

    struct PreparedWorldSceneData {
        std::vector<StaticProp> props;
        bool has_enemies = false;
        RelayCore::render::BlockoutRenderData blockout{};
        std::vector<WorldLocalLight> local_lights{};
        std::vector<EnvironmentEffect> environment{};
        std::vector<RelayCore::render::AuthoringWarningDiagnostics> authoring_warnings{};
        std::uint32_t total_solids = 0;
        RelayCore::render::ColorPipelineMode color_pipeline_mode = RelayCore::render::ColorPipelineMode::legacy_gamma;
    };

    [[nodiscard]] auto build_prepared_world_scene_data(
        const RelayCore::BlockoutLevel& level,
        const RelayCore::render::RenderAuthoringConfig& authoring_config,
        RelayCore::render::ColorPipelineMode color_pipeline_mode
    ) -> PreparedWorldSceneData;

    class WorldSceneRenderer {
    public:
        void initialize(const PreparedWorldSceneData& scene_data, RelayCore::render::StaticModelCache& models);

        void shutdown() noexcept;

        void render(
            RelayCore::Renderer& renderer,
            const RelayCore::render::PerspectiveCamera& camera,
            float scene_time_seconds,
            std::span<const RelayCore::gameplay::RuntimeDoor> doors,
            std::span<const RelayCore::gameplay::RuntimeTurret> turrets,
            std::span<const RelayCore::gameplay::RuntimeUsable> usables,
            int focused_usable_index,
            const RelayCore::render::InspectorDiagnostics& inspector,
            bool inspector_visible,
            std::span<const std::string> activated_targets = {},
            const ScientistPose& scientist = {},
            std::span<const EnvironmentEffect> environment = {},
            std::span<const RuntimeEnemy> enemies = {}
        );

        [[nodiscard]] auto diagnostics() const noexcept -> const RelayCore::render::WorldDiagnostics&;

        [[nodiscard]] auto authoring_warnings() const noexcept
            -> const std::vector<RelayCore::render::AuthoringWarningDiagnostics>&;

    private:
        void initialize_prop_meshes(RelayCore::render::ColorPipelineMode pipeline);
        void initialize_environment_meshes(RelayCore::render::ColorPipelineMode pipeline);
        void render_environment(
            const RelayCore::render::PerspectiveCamera& camera,
            std::span<const EnvironmentEffect> effects
        );

        void render_scientist(const ScientistPose& pose);

        void render_doors(
            const RelayCore::render::PerspectiveCamera& camera,
            std::span<const RelayCore::gameplay::RuntimeDoor> doors
        );

        void render_interactive_usables(
            const RelayCore::render::PerspectiveCamera& camera,
            float scene_time_seconds,
            std::span<const RelayCore::gameplay::RuntimeUsable> usables,
            int focused_usable_index,
            bool highlight_visible
        );

        void render_turrets(
            const RelayCore::render::PerspectiveCamera& camera,
            std::span<const RelayCore::gameplay::RuntimeTurret> turrets
        );

        void render_inspector_pick(
            const RelayCore::render::PerspectiveCamera& camera,
            float scene_time_seconds,
            const RelayCore::render::InspectorDiagnostics& inspector
        );

        void record_visible_chunk(bool transparent) noexcept;

        void record_render_draw(RelayCore::render::RenderPass pass, std::uint32_t triangles) noexcept;

        struct RenderChunk {
            RelayCore::render::StaticColorMesh mesh{};
            glm::vec3 mins{};
            glm::vec3 maxs{};
            std::uint32_t triangle_count = 0;
            bool transparent = false;
            bool casts_directional_shadow = true;
        };

        std::array<glm::vec4, 6> frustum_planes_{};
        RelayCore::render::StaticColorMesh door_mesh_{};
        RelayCore::render::StaticColorMesh panel_mesh_{};
        RelayCore::render::StaticColorMesh panel_housing_mesh_{};
        RelayCore::render::StaticColorMesh scientist_thigh_mesh_{};
        RelayCore::render::StaticColorMesh scientist_shin_mesh_{};
        RelayCore::render::StaticColorMesh scientist_shoe_mesh_{};
        RelayCore::render::StaticColorMesh scientist_coat_mesh_{};
        RelayCore::render::StaticColorMesh panel_blocked_mesh_{};
        RelayCore::render::StaticColorMesh panel_highlight_mesh_{};
        RelayCore::render::StaticColorMesh panel_success_mesh_{};
        RelayCore::render::StaticColorMesh panel_warning_mesh_{};
        RelayCore::render::StaticColorMesh pickup_mesh_{};
        RelayCore::render::StaticColorMesh pickup_highlight_mesh_{};
        RelayCore::render::StaticColorMesh turret_mesh_{};
        RelayCore::render::StaticColorMesh turret_destroyed_mesh_{};
        RelayCore::render::StaticColorMesh turret_alert_mesh_{};
        RelayCore::render::StaticColorMesh inspector_usable_mesh_{};
        RelayCore::render::StaticColorMesh inspector_door_mesh_{};
        RelayCore::render::StaticColorMesh inspector_trigger_mesh_{};
        RelayCore::render::StaticColorMesh inspector_solid_mesh_{};
        RelayCore::render::StaticColorMesh inspector_hit_mesh_{};
        std::vector<RenderChunk> render_chunks_{};
        std::shared_ptr<const RelayCore::render::StaticModel> enemy_model_;
        std::vector<WorldLocalLight> local_lights_{};
        std::vector<EnvironmentEffect> environment_{};
        RelayCore::render::StaticColorMesh fan_frame_mesh_, fan_rotor_mesh_, steam_mesh_, spark_mesh_;
        std::array<RelayCore::render::StaticColorMesh, 5> decal_meshes_;
        std::array<RelayCore::render::StaticColorMesh, 16> lamp_meshes_;
        struct EnvironmentPuff {
            glm::vec3 position;
            float size;
        };
        std::vector<EnvironmentPuff> environment_puffs_;
        struct TransparentSubmission {
            const RelayCore::render::StaticColorMesh* mesh = nullptr;
            glm::mat4 model{1.0F};
            float depth = 0.0F;
            std::uint32_t triangles = 0;
        };
        std::vector<TransparentSubmission> transparent_submissions_{};
        std::vector<RelayCore::render::LocalLight> active_local_lights_{};
        std::vector<RelayCore::render::AuthoringWarningDiagnostics> authoring_warnings_{};
        struct PropInstance {
            std::shared_ptr<const RelayCore::render::StaticModel> model;
            glm::mat4 transform{1};
        };
        std::vector<PropInstance> props_;
        RelayCore::render::RenderQueue render_queue_{};
        RelayCore::render::RenderQueue shadow_casters_{};
        RelayCore::render::WorldDiagnostics diagnostics_{};
    };
} // namespace RelayGame
