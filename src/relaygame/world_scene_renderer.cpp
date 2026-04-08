#include "relaygame/world_scene_renderer.h"

#include "relaycore/core/log.h"
#include "relaycore/core/profiling.h"
#include "relaycore/render/blockout_mesh_builder.h"
#include "relaycore/render/camera.h"
#include "relaycore/render/renderer.h"
#include "relaycore/world/blockout_level.h"

#include <algorithm>
#include <cstdint>
#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>
#include <sstream>
#include <stdexcept>
#include <string>

namespace RelayGame
{
    namespace
    {
        [[nodiscard]] auto normalize_plane(const glm::vec4& plane) -> glm::vec4
        {
            const float length = glm::length(glm::vec3{plane.x, plane.y, plane.z});
            if (length <= 0.0001F)
            {
                return {};
            }

            return plane / length;
        }

        [[nodiscard]] auto extract_frustum_planes(const glm::mat4& view_projection) -> std::array<glm::vec4, 6>
        {
            const glm::vec4
                row0{view_projection[0][0], view_projection[1][0], view_projection[2][0], view_projection[3][0]};
            const glm::vec4
                row1{view_projection[0][1], view_projection[1][1], view_projection[2][1], view_projection[3][1]};
            const glm::vec4
                row2{view_projection[0][2], view_projection[1][2], view_projection[2][2], view_projection[3][2]};
            const glm::vec4
                row3{view_projection[0][3], view_projection[1][3], view_projection[2][3], view_projection[3][3]};

            return {
                normalize_plane(row3 + row0),
                normalize_plane(row3 - row0),
                normalize_plane(row3 + row1),
                normalize_plane(row3 - row1),
                normalize_plane(row3 + row2),
                normalize_plane(row3 - row2),
            };
        }

        [[nodiscard]] auto
        aabb_intersects_frustum(const std::array<glm::vec4, 6>& planes, const glm::vec3& mins, const glm::vec3& maxs)
            -> bool
        {
            for (const glm::vec4& plane : planes)
            {
                const glm::vec3 normal{plane.x, plane.y, plane.z};
                const glm::vec3 positive_vertex{
                    normal.x >= 0.0F ? maxs.x : mins.x,
                    normal.y >= 0.0F ? maxs.y : mins.y,
                    normal.z >= 0.0F ? maxs.z : mins.z,
                };

                if (glm::dot(normal, positive_vertex) + plane.w < 0.0F)
                {
                    return false;
                }
            }

            return true;
        }

        void
        tint_mesh_data(RelayCore::render::CpuColorMeshData& mesh_data, const glm::vec3& tint, const float bias = 0.0F)
        {
            for (auto& vertex : mesh_data.vertices)
            {
                vertex.color = glm::clamp((vertex.color * tint) + glm::vec3{bias}, 0.0F, 1.0F);
            }
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
        RelayCore::render::CpuColorMeshData panel_success_mesh_data =
            RelayCore::render::make_color_double_sided_quad_mesh_data();
        RelayCore::render::CpuColorMeshData panel_warning_mesh_data =
            RelayCore::render::make_color_double_sided_quad_mesh_data();
        RelayCore::render::CpuColorMeshData pickup_mesh_data = RelayCore::render::make_color_box_mesh_data();
        RelayCore::render::CpuColorMeshData pickup_highlight_mesh_data = RelayCore::render::make_color_box_mesh_data();

        tint_mesh_data(panel_mesh_data, {1.80F, 0.72F, 0.34F}, 0.02F);
        tint_mesh_data(panel_highlight_mesh_data, {2.25F, 1.90F, 0.55F}, 0.10F);
        tint_mesh_data(panel_success_mesh_data, {0.58F, 1.95F, 1.08F}, 0.06F);
        tint_mesh_data(panel_warning_mesh_data, {2.18F, 0.78F, 0.48F}, 0.04F);
        tint_mesh_data(pickup_mesh_data, {0.52F, 1.82F, 1.90F}, 0.06F);
        tint_mesh_data(pickup_highlight_mesh_data, {0.86F, 2.20F, 2.30F}, 0.14F);

        door_mesh_.initialize(door_mesh_data);
        panel_mesh_.initialize(panel_mesh_data);
        panel_highlight_mesh_.initialize(panel_highlight_mesh_data);
        panel_success_mesh_.initialize(panel_success_mesh_data);
        panel_warning_mesh_.initialize(panel_warning_mesh_data);
        pickup_mesh_.initialize(pickup_mesh_data);
        pickup_highlight_mesh_.initialize(pickup_highlight_mesh_data);

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
            chunk.transparent = chunk_data.transparent;
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
        panel_success_mesh_ = RelayCore::render::StaticColorMesh{};
        panel_warning_mesh_ = RelayCore::render::StaticColorMesh{};
        pickup_mesh_ = RelayCore::render::StaticColorMesh{};
        pickup_highlight_mesh_ = RelayCore::render::StaticColorMesh{};
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
        std::vector<const RenderChunk*> transparent_chunks{};
        transparent_chunks.reserve(render_chunks_.size());

        frustum_planes_ = extract_frustum_planes(camera.view_projection_matrix());
        {
            RELAYCORE_PROFILE_ZONE_N("WorldSceneRenderer::draw_visible_blockout_chunks");
            for (const RenderChunk& chunk : render_chunks_)
            {
                if (!aabb_intersects_frustum(frustum_planes_, chunk.mins, chunk.maxs))
                {
                    continue;
                }

                ++diagnostics_.visible_solids;
                ++diagnostics_.draw_calls;
                diagnostics_.triangles += chunk.triangle_count;
                if (chunk.transparent)
                {
                    transparent_chunks.push_back(&chunk);
                    continue;
                }

                render_queue_.push(chunk.mesh, glm::mat4{1.0F});
            }
        }

        std::ranges::sort(transparent_chunks, [&camera](const RenderChunk* lhs, const RenderChunk* rhs) {
            const glm::vec3 lhs_center = (lhs->mins + lhs->maxs) * 0.5F;
            const glm::vec3 rhs_center = (rhs->mins + rhs->maxs) * 0.5F;
            const glm::vec3 lhs_delta = lhs_center - camera.position();
            const glm::vec3 rhs_delta = rhs_center - camera.position();
            const float lhs_distance2 = glm::dot(lhs_delta, lhs_delta);
            const float rhs_distance2 = glm::dot(rhs_delta, rhs_delta);
            return lhs_distance2 > rhs_distance2;
        });

        for (const RenderChunk* chunk : transparent_chunks)
        {
            render_queue_.push(chunk->mesh, glm::mat4{1.0F}, true);
        }

        render_doors(camera, doors);
        render_interactive_usables(camera, usables, focused_usable_index);

        renderer.set_local_lights(local_lights_);
        renderer.draw_render_queue(render_queue_.items());
    }

    auto WorldSceneRenderer::diagnostics() const noexcept -> const RelayCore::render::WorldDiagnostics&
    {
        return diagnostics_;
    }
} // namespace RelayGame
