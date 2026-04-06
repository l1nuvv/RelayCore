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
#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>
#include <sstream>
#include <stdexcept>
#include <string>

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

        enum class PanelPlaneAxis : std::uint8_t {
            X,
            Y,
            Z,
        };

        [[nodiscard]] auto thinnest_panel_axis(const RelayCore::physics::Aabb& bounds) -> PanelPlaneAxis
        {
            const glm::vec3 size = bounds.maxs - bounds.mins;
            if (size.x <= size.y && size.x <= size.z)
            {
                return PanelPlaneAxis::X;
            }
            if (size.y <= size.x && size.y <= size.z)
            {
                return PanelPlaneAxis::Y;
            }
            return PanelPlaneAxis::Z;
        }

        [[nodiscard]] auto make_panel_plate_model_matrix(
            const RelayCore::physics::Aabb& bounds,
            const RelayCore::render::PerspectiveCamera& camera,
            const float in_plane_padding,
            const float camera_offset
        ) -> glm::mat4
        {
            const glm::vec3 center = (bounds.mins + bounds.maxs) * 0.5F;
            const glm::vec3 size = bounds.maxs - bounds.mins;
            const glm::vec3 to_camera = camera.position() - center;
            const glm::vec3 camera_dir =
                glm::dot(to_camera, to_camera) > 0.000001F ? glm::normalize(to_camera) : glm::vec3{0.0F, 0.0F, 1.0F};

            glm::mat4 model{1.0F};
            model = glm::translate(model, center + (camera_dir * camera_offset));

            glm::vec3 scale{size.x + in_plane_padding, size.y + in_plane_padding, 1.0F};
            switch (thinnest_panel_axis(bounds))
            {
                case PanelPlaneAxis::X:
                    model = glm::rotate(model, glm::half_pi<float>(), glm::vec3{0.0F, 1.0F, 0.0F});
                    scale = {size.z + in_plane_padding, size.y + in_plane_padding, 1.0F};
                    break;
                case PanelPlaneAxis::Y:
                    model = glm::rotate(model, -glm::half_pi<float>(), glm::vec3{1.0F, 0.0F, 0.0F});
                    scale = {size.x + in_plane_padding, size.z + in_plane_padding, 1.0F};
                    break;
                case PanelPlaneAxis::Z:
                    scale = {size.x + in_plane_padding, size.y + in_plane_padding, 1.0F};
                    break;
            }

            model = glm::scale(model, scale);
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

        [[nodiscard]] auto mesh_triangle_count(const RelayCore::render::StaticColorMesh& mesh) -> std::uint32_t
        {
            return static_cast<std::uint32_t>(mesh.index_count() / 3);
        }

        [[nodiscard]] auto
        parse_float_property(const RelayCore::EntitySpawnData& entity, const char* key, const float default_value)
            -> float
        {
            const auto found = entity.properties.find(key);
            if (found == entity.properties.end())
            {
                return default_value;
            }

            return std::stof(found->second);
        }

        [[nodiscard]] auto
        parse_vec3_property(const RelayCore::EntitySpawnData& entity, const char* key, const glm::vec3& default_value)
            -> glm::vec3
        {
            const auto found = entity.properties.find(key);
            if (found == entity.properties.end())
            {
                return default_value;
            }

            std::string normalized = found->second;
            std::replace(normalized.begin(), normalized.end(), ',', ' ');

            std::istringstream stream(normalized);
            glm::vec3 value = default_value;
            if (!(stream >> value.x >> value.y >> value.z))
            {
                throw std::runtime_error("render_light.color must be a 3-component vector string");
            }

            return value;
        }
    } // namespace

    void WorldSceneRenderer::initialize(const RelayCore::BlockoutLevel& level)
    {
        const RelayCore::render::BlockoutRenderData render_data = RelayCore::render::build_blockout_render_data(level);
        RelayCore::render::CpuColorMeshData door_mesh_data = RelayCore::render::make_color_box_mesh_data();
        RelayCore::render::CpuColorMeshData panel_mesh_data =
            RelayCore::render::make_color_double_sided_quad_mesh_data();
        RelayCore::render::CpuColorMeshData panel_highlight_mesh_data =
            RelayCore::render::make_color_double_sided_quad_mesh_data();

        tint_mesh_data(panel_mesh_data, {1.80F, 0.72F, 0.34F}, 0.02F);
        tint_mesh_data(panel_highlight_mesh_data, {2.25F, 1.90F, 0.55F}, 0.10F);

        door_mesh_.initialize(door_mesh_data);
        panel_mesh_.initialize(panel_mesh_data);
        panel_highlight_mesh_.initialize(panel_highlight_mesh_data);

        render_chunks_.clear();
        render_chunks_.reserve(render_data.chunks.size());
        local_lights_.clear();

        for (const auto& chunk_data : render_data.chunks)
        {
            RenderChunk chunk{};
            chunk.mesh.initialize(chunk_data.mesh);
            chunk.mins = chunk_data.mins;
            chunk.maxs = chunk_data.maxs;
            chunk.triangle_count = chunk_data.triangle_count;
            render_chunks_.push_back(std::move(chunk));
        }

        for (const RelayCore::EntitySpawnData& entity : level.entities)
        {
            if (entity.classname != "render_light")
            {
                continue;
            }

            RelayCore::render::LocalLight light{};
            light.position = entity.origin;
            light.color = parse_vec3_property(entity, "color", {0.18F, 0.22F, 0.30F})
                          * parse_float_property(entity, "intensity", 1.0F);
            light.radius = std::max(parse_float_property(entity, "radius", 3.5F), 0.5F);
            local_lights_.push_back(light);
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
        local_lights_.clear();
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
                ++diagnostics_.draw_calls;
                diagnostics_.triangles += mesh_triangle_count(panel_highlight_mesh_);
                render_queue_.push(
                    panel_highlight_mesh_,
                    make_panel_plate_model_matrix(usable.visual_bounds, camera, 0.08F, 0.012F)
                );
            }

            ++diagnostics_.draw_calls;
            diagnostics_.triangles += mesh_triangle_count(panel_mesh_);
            render_queue_.push(panel_mesh_, make_panel_plate_model_matrix(usable.visual_bounds, camera, 0.0F, 0.006F));
        }

        renderer.set_local_lights(local_lights_);
        renderer.draw_render_queue(render_queue_.items());
    }

    auto WorldSceneRenderer::diagnostics() const noexcept -> const RelayCore::render::WorldDiagnostics&
    {
        return diagnostics_;
    }
} // namespace RelayGame
