#include "relaygame/world_scene_renderer.h"

#include "relaycore/core/log.h"
#include "relaycore/core/profiling.h"
#include "relaycore/render/blockout_mesh_builder.h"
#include "relaycore/render/camera.h"
#include "relaycore/render/renderer.h"
#include "relaycore/world/blockout_level.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <glm/common.hpp>
#include <glm/geometric.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>

namespace RelayGame
{
    namespace
    {
        struct FrustumPlane {
            glm::vec3 normal{0.0F, 0.0F, 1.0F};
            float distance = 0.0F;
        };

        struct ViewFrustum {
            std::array<FrustumPlane, 6> planes{};
        };

        [[nodiscard]] auto normalize_plane(const glm::vec4& plane) -> FrustumPlane
        {
            const glm::vec3 normal{plane.x, plane.y, plane.z};
            const float length = glm::length(normal);
            if (length <= 0.0001F)
            {
                return {};
            }

            return {normal / length, plane.w / length};
        }

        [[nodiscard]] auto extract_frustum(const glm::mat4& view_projection) -> ViewFrustum
        {
            const glm::vec4
                row0{view_projection[0][0], view_projection[1][0], view_projection[2][0], view_projection[3][0]};
            const glm::vec4
                row1{view_projection[0][1], view_projection[1][1], view_projection[2][1], view_projection[3][1]};
            const glm::vec4
                row2{view_projection[0][2], view_projection[1][2], view_projection[2][2], view_projection[3][2]};
            const glm::vec4
                row3{view_projection[0][3], view_projection[1][3], view_projection[2][3], view_projection[3][3]};

            ViewFrustum frustum{};
            frustum.planes[0] = normalize_plane(row3 + row0);
            frustum.planes[1] = normalize_plane(row3 - row0);
            frustum.planes[2] = normalize_plane(row3 + row1);
            frustum.planes[3] = normalize_plane(row3 - row1);
            frustum.planes[4] = normalize_plane(row3 + row2);
            frustum.planes[5] = normalize_plane(row3 - row2);
            return frustum;
        }

        [[nodiscard]] auto
        aabb_intersects_frustum(const ViewFrustum& frustum, const glm::vec3& mins, const glm::vec3& maxs) -> bool
        {
            for (const FrustumPlane& plane : frustum.planes)
            {
                const glm::vec3 positive_vertex{
                    plane.normal.x >= 0.0F ? maxs.x : mins.x,
                    plane.normal.y >= 0.0F ? maxs.y : mins.y,
                    plane.normal.z >= 0.0F ? maxs.z : mins.z,
                };

                if (glm::dot(plane.normal, positive_vertex) + plane.distance < 0.0F)
                {
                    return false;
                }
            }

            return true;
        }

        [[nodiscard]] auto make_aabb_model_matrix(const RelayCore::physics::Aabb& bounds) -> glm::mat4
        {
            const glm::vec3 center = (bounds.mins + bounds.maxs) * 0.5F;
            const glm::vec3 size = (bounds.maxs - bounds.mins);

            glm::mat4 model{1.0F};
            model = glm::translate(model, center);
            model = glm::scale(model, size);
            return model;
        }

        void
        tint_mesh_data(RelayCore::render::CpuColorMeshData& mesh_data, const glm::vec3& tint, const float bias = 0.0F)
        {
            for (auto& vertex : mesh_data.vertices)
            {
                vertex.color = glm::clamp((vertex.color * tint) + glm::vec3{bias}, 0.0F, 1.0F);
            }
        }

        [[nodiscard]] auto expanded_aabb(const RelayCore::physics::Aabb& bounds, const glm::vec3& expansion)
            -> RelayCore::physics::Aabb
        {
            return {
                .mins = bounds.mins - expansion,
                .maxs = bounds.maxs + expansion,
            };
        }

        [[nodiscard]] auto mesh_triangle_count(const RelayCore::render::StaticColorMesh& mesh) -> std::uint32_t
        {
            return static_cast<std::uint32_t>(mesh.index_count() / 3);
        }
    } // namespace

    void WorldSceneRenderer::initialize(const RelayCore::BlockoutLevel& level)
    {
        const RelayCore::render::BlockoutRenderData render_data = RelayCore::render::build_blockout_render_data(level);
        RelayCore::render::CpuColorMeshData door_mesh_data = RelayCore::render::make_color_box_mesh_data();
        RelayCore::render::CpuColorMeshData panel_mesh_data = RelayCore::render::make_color_box_mesh_data();
        RelayCore::render::CpuColorMeshData panel_highlight_mesh_data = RelayCore::render::make_color_box_mesh_data();

        tint_mesh_data(panel_mesh_data, {1.80F, 0.72F, 0.34F}, 0.02F);
        tint_mesh_data(panel_highlight_mesh_data, {2.25F, 1.90F, 0.55F}, 0.10F);

        door_mesh_.initialize(door_mesh_data);
        panel_mesh_.initialize(panel_mesh_data);
        panel_highlight_mesh_.initialize(panel_highlight_mesh_data);

        render_chunks_.clear();
        render_chunks_.reserve(render_data.chunks.size());

        for (const auto& chunk_data : render_data.chunks)
        {
            RenderChunk chunk{};
            chunk.mesh.initialize(chunk_data.mesh);
            chunk.mins = chunk_data.mins;
            chunk.maxs = chunk_data.maxs;
            chunk.triangle_count = chunk_data.triangle_count;
            render_chunks_.push_back(std::move(chunk));
        }

        diagnostics_.total_solids = static_cast<std::uint32_t>(level.solids.size());
        diagnostics_.visible_solids = static_cast<std::uint32_t>(render_chunks_.size());
        diagnostics_.draw_calls = diagnostics_.visible_solids;
        diagnostics_.triangles = render_data.total_triangles;
    }

    void WorldSceneRenderer::shutdown() noexcept
    {
        render_queue_.clear();
        render_chunks_.clear();
        render_chunks_.shrink_to_fit();

        door_mesh_ = RelayCore::render::StaticColorMesh{};
        panel_mesh_ = RelayCore::render::StaticColorMesh{};
        panel_highlight_mesh_ = RelayCore::render::StaticColorMesh{};
        diagnostics_ = {};
    }

    void WorldSceneRenderer::render(
        RelayCore::Renderer& renderer,
        const RelayCore::render::PerspectiveCamera& camera,
        std::span<const RelayCore::gameplay::RuntimeDoor> doors,
        std::span<const RelayCore::gameplay::RuntimeUsable> usables,
        const int focused_usable_index
    )
    {
        RELAYCORE_PROFILE_ZONE_N("WorldSceneRenderer::render");

        diagnostics_.visible_solids = 0;
        diagnostics_.draw_calls = 0;
        diagnostics_.triangles = 0;
        render_queue_.clear();
        render_queue_.reserve(render_chunks_.size() + doors.size() + (usables.size() * 2));

        const ViewFrustum frustum = extract_frustum(camera.view_projection_matrix());
        {
            RELAYCORE_PROFILE_ZONE_N("WorldSceneRenderer::draw_visible_blockout_chunks");
            for (const RenderChunk& chunk : render_chunks_)
            {
                if (!aabb_intersects_frustum(frustum, chunk.mins, chunk.maxs))
                {
                    continue;
                }

                ++diagnostics_.visible_solids;
                ++diagnostics_.draw_calls;
                diagnostics_.triangles += chunk.triangle_count;
                render_queue_.push(chunk.mesh, glm::mat4{1.0F});
            }
        }

        for (const RelayCore::gameplay::RuntimeDoor& door : doors)
        {
            const RelayCore::physics::Aabb bounds = RelayCore::gameplay::current_door_bounds(door);
            if (!aabb_intersects_frustum(frustum, bounds.mins, bounds.maxs))
            {
                continue;
            }

            ++diagnostics_.draw_calls;
            diagnostics_.triangles += mesh_triangle_count(door_mesh_);
            render_queue_.push(door_mesh_, make_aabb_model_matrix(bounds));
        }

        for (std::size_t usable_index = 0; usable_index < usables.size(); ++usable_index)
        {
            const RelayCore::gameplay::RuntimeUsable& usable = usables[usable_index];
            if (!usable.enabled || usable.visual_kind != RelayCore::gameplay::UsableVisualKind::Panel
                || !usable.has_visual_bounds)
            {
                continue;
            }

            if (!aabb_intersects_frustum(frustum, usable.visual_bounds.mins, usable.visual_bounds.maxs))
            {
                continue;
            }

            if (static_cast<int>(usable_index) == focused_usable_index)
            {
                const RelayCore::physics::Aabb highlight_bounds =
                    expanded_aabb(usable.visual_bounds, {0.03F, 0.03F, 0.03F});
                ++diagnostics_.draw_calls;
                diagnostics_.triangles += mesh_triangle_count(panel_highlight_mesh_);
                render_queue_.push(panel_highlight_mesh_, make_aabb_model_matrix(highlight_bounds));
            }

            ++diagnostics_.draw_calls;
            diagnostics_.triangles += mesh_triangle_count(panel_mesh_);
            render_queue_.push(panel_mesh_, make_aabb_model_matrix(usable.visual_bounds));
        }

        renderer.draw_render_queue(render_queue_.items());
    }

    auto WorldSceneRenderer::diagnostics() const noexcept -> const RelayCore::render::WorldDiagnostics&
    {
        return diagnostics_;
    }
} // namespace RelayGame
