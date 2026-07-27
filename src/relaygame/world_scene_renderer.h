#pragma once

#include "relaycore/gameplay/combat_runtime.h"
#include "relaycore/gameplay/interaction_runtime.h"
#include "relaycore/render/blockout_mesh_builder.h"
#include "relaycore/render/mesh.h"
#include "relaycore/render/render_authoring_config.h"
#include "relaycore/render/render_diagnostics.h"
#include "relaycore/render/render_queue.h"

#include <array>
#include <cstdint>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <span>
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
    struct PreparedWorldSceneData {
        RelayCore::render::BlockoutRenderData blockout{};
        std::vector<RelayCore::render::LocalLight> local_lights{};
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
        void initialize(const PreparedWorldSceneData& scene_data);

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
            bool inspector_visible
        );

        [[nodiscard]] auto diagnostics() const noexcept -> const RelayCore::render::WorldDiagnostics&;

        [[nodiscard]] auto authoring_warnings() const noexcept
            -> const std::vector<RelayCore::render::AuthoringWarningDiagnostics>&;

    private:
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
        };

        std::array<glm::vec4, 6> frustum_planes_{};
        RelayCore::render::StaticColorMesh door_mesh_{};
        RelayCore::render::StaticColorMesh panel_mesh_{};
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
        std::vector<RelayCore::render::LocalLight> local_lights_{};
        std::vector<RelayCore::render::LocalLight> active_local_lights_{};
        std::vector<RelayCore::render::AuthoringWarningDiagnostics> authoring_warnings_{};
        RelayCore::render::RenderQueue render_queue_{};
        RelayCore::render::WorldDiagnostics diagnostics_{};
    };
} // namespace RelayGame
