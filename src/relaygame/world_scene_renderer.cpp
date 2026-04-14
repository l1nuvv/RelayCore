#include "relaygame/world_scene_renderer.h"

#include "relaycore/core/log.h"
#include "relaycore/core/profiling.h"
#include "relaycore/render/blockout_mesh_builder.h"
#include "relaycore/render/camera.h"
#include "relaycore/render/renderer.h"
#include "relaycore/world/blockout_level.h"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <glm/common.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>
#include <limits>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>

namespace RelayGame
{
    namespace
    {
        constexpr std::size_t kMaxActiveLocalLights = 8;

        [[nodiscard]] auto normalize_plane(const glm::vec4& plane) -> glm::vec4
        {
            RELAYCORE_PROFILE_FUNCTION();
            const float length = glm::length(glm::vec3{plane.x, plane.y, plane.z});
            if (length <= 0.0001F)
            {
                return {};
            }

            return plane / length;
        }

        [[nodiscard]] auto extract_frustum_planes(const glm::mat4& view_projection) -> std::array<glm::vec4, 6>
        {
            RELAYCORE_PROFILE_FUNCTION();
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
            RELAYCORE_PROFILE_FUNCTION();
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
        find_scene_renderer_entity_property(const RelayCore::EntitySpawnData& entity, const std::string_view key)
            -> const std::string*
        {
            RELAYCORE_PROFILE_FUNCTION();
            const auto found = entity.properties.find(std::string{key});
            if (found == entity.properties.end())
            {
                return nullptr;
            }

            return &found->second;
        }

        [[nodiscard]] auto
        parse_scene_renderer_float_property(const RelayCore::EntitySpawnData& entity, const std::string_view key)
            -> std::optional<float>
        {
            RELAYCORE_PROFILE_FUNCTION();
            const std::string* value = find_scene_renderer_entity_property(entity, key);
            if (value == nullptr)
            {
                return std::nullopt;
            }

            float parsed_value = 0.0F;
            const auto [ptr, ec] = std::from_chars(value->data(), value->data() + value->size(), parsed_value);
            if (ec != std::errc{} || ptr != value->data() + value->size())
            {
                throw std::runtime_error("render_light." + std::string(key) + " must be numeric");
            }

            return parsed_value;
        }

        [[nodiscard]] auto
        parse_scene_renderer_vec3_property(const RelayCore::EntitySpawnData& entity, const std::string_view key)
            -> std::optional<glm::vec3>
        {
            RELAYCORE_PROFILE_FUNCTION();
            const std::string* value = find_scene_renderer_entity_property(entity, key);
            if (value == nullptr)
            {
                return std::nullopt;
            }

            std::string normalized = *value;
            std::replace(normalized.begin(), normalized.end(), ',', ' ');

            std::istringstream stream(normalized);
            glm::vec3 parsed_value{};
            if (!(stream >> parsed_value.x >> parsed_value.y >> parsed_value.z))
            {
                throw std::runtime_error("render_light.color must be a 3-component vector string");
            }

            return parsed_value;
        }

        [[nodiscard]] auto format_entity_label(const RelayCore::EntitySpawnData& entity, const std::size_t entity_index)
            -> std::string
        {
            if (const std::string* targetname = find_scene_renderer_entity_property(entity, "targetname");
                targetname != nullptr && !targetname->empty())
            {
                return "render_light '" + *targetname + "' [entity " + std::to_string(entity_index) + "]";
            }

            return "render_light [entity " + std::to_string(entity_index) + "]";
        }

        void assign_warning_source(
            RelayCore::render::AuthoringWarningDiagnostics& warning,
            const std::string& label,
            const RelayCore::AuthoredSourceLocation* source
        )
        {
            warning.primary_label = label;
            if (source == nullptr || !source->valid())
            {
                return;
            }

            warning.primary_file_path = source->has_placement() ? source->placement_file_path : source->file_path;
            warning.primary_line = static_cast<int>(source->has_placement() ? source->placement_line : source->line);
            warning.primary_column =
                static_cast<int>(source->has_placement() ? source->placement_column : source->column);
        }

        [[nodiscard]] auto warning_source_bucket(const RelayCore::AuthoredSourceLocation* source) -> std::string
        {
            if (source == nullptr)
            {
                return "generated";
            }

            return source->has_placement() ? std::string{"instanced"} : std::string{"direct"};
        }

        [[nodiscard]] auto local_light_score(
            const RelayCore::render::LocalLight& light,
            const RelayCore::render::PerspectiveCamera& camera
        ) -> float
        {
            RELAYCORE_PROFILE_FUNCTION();
            const glm::vec3 delta = light.position - camera.position();
            const float distance_squared = glm::dot(delta, delta);
            const float radius = std::max(light.radius, 0.5F);
            return distance_squared / (radius * radius);
        }

        [[nodiscard]] auto color_outside_unit_range(const glm::vec3& color) -> bool
        {
            return color.x < 0.0F || color.y < 0.0F || color.z < 0.0F || color.x > 1.0F || color.y > 1.0F
                   || color.z > 1.0F;
        }

        [[nodiscard]] auto format_light_color(const glm::vec3& color) -> std::string
        {
            return "[" + std::to_string(color.x) + ", " + std::to_string(color.y) + ", " + std::to_string(color.z)
                   + "]";
        }
    } // namespace

    void WorldSceneRenderer::initialize(
        const RelayCore::BlockoutLevel& level,
        const RelayCore::render::RenderAuthoringConfig& authoring_config
    )
    {
        RELAYCORE_PROFILE_FUNCTION();
        const RelayCore::render::BlockoutRenderData render_data =
            RelayCore::render::build_blockout_render_data(level, authoring_config);
        RelayCore::render::CpuColorMeshData door_mesh_data = RelayCore::render::make_color_box_mesh_data();
        RelayCore::render::CpuColorMeshData panel_mesh_data =
            RelayCore::render::make_color_double_sided_quad_mesh_data();
        RelayCore::render::CpuColorMeshData panel_blocked_mesh_data =
            RelayCore::render::make_color_double_sided_quad_mesh_data();
        RelayCore::render::CpuColorMeshData panel_highlight_mesh_data =
            RelayCore::render::make_color_double_sided_quad_mesh_data();
        RelayCore::render::CpuColorMeshData panel_success_mesh_data =
            RelayCore::render::make_color_double_sided_quad_mesh_data();
        RelayCore::render::CpuColorMeshData panel_warning_mesh_data =
            RelayCore::render::make_color_double_sided_quad_mesh_data();
        RelayCore::render::CpuColorMeshData pickup_mesh_data = RelayCore::render::make_color_box_mesh_data();
        RelayCore::render::CpuColorMeshData pickup_highlight_mesh_data = RelayCore::render::make_color_box_mesh_data();
        RelayCore::render::CpuColorMeshData turret_mesh_data = RelayCore::render::make_color_box_mesh_data();
        RelayCore::render::CpuColorMeshData turret_destroyed_mesh_data = RelayCore::render::make_color_box_mesh_data();
        RelayCore::render::CpuColorMeshData turret_alert_mesh_data = RelayCore::render::make_color_box_mesh_data();
        RelayCore::render::CpuColorMeshData inspector_usable_mesh_data = RelayCore::render::make_color_box_mesh_data();
        RelayCore::render::CpuColorMeshData inspector_door_mesh_data = RelayCore::render::make_color_box_mesh_data();
        RelayCore::render::CpuColorMeshData inspector_trigger_mesh_data = RelayCore::render::make_color_box_mesh_data();
        RelayCore::render::CpuColorMeshData inspector_solid_mesh_data = RelayCore::render::make_color_box_mesh_data();
        RelayCore::render::CpuColorMeshData inspector_hit_mesh_data = RelayCore::render::make_color_box_mesh_data();

        tint_mesh_data(panel_mesh_data, {1.80F, 0.72F, 0.34F}, 0.02F);
        tint_mesh_data(panel_blocked_mesh_data, {0.62F, 0.44F, 0.34F}, -0.02F);
        tint_mesh_data(panel_highlight_mesh_data, {2.25F, 1.90F, 0.55F}, 0.10F);
        tint_mesh_data(panel_success_mesh_data, {0.58F, 1.95F, 1.08F}, 0.06F);
        tint_mesh_data(panel_warning_mesh_data, {2.18F, 0.78F, 0.48F}, 0.04F);
        tint_mesh_data(pickup_mesh_data, {0.52F, 1.82F, 1.90F}, 0.06F);
        tint_mesh_data(pickup_highlight_mesh_data, {0.86F, 2.20F, 2.30F}, 0.14F);
        tint_mesh_data(turret_mesh_data, {1.84F, 0.52F, 0.42F}, -0.01F);
        tint_mesh_data(turret_destroyed_mesh_data, {0.48F, 0.42F, 0.40F}, -0.18F);
        tint_mesh_data(turret_alert_mesh_data, {2.55F, 0.72F, 0.62F}, 0.14F);
        tint_mesh_data(inspector_usable_mesh_data, {2.25F, 1.82F, 0.62F}, 0.12F);
        tint_mesh_data(inspector_door_mesh_data, {0.62F, 2.08F, 2.28F}, 0.10F);
        tint_mesh_data(inspector_trigger_mesh_data, {2.28F, 0.92F, 0.58F}, 0.08F);
        tint_mesh_data(inspector_solid_mesh_data, {1.18F, 1.55F, 2.18F}, 0.08F);
        tint_mesh_data(inspector_hit_mesh_data, {2.60F, 2.60F, 2.60F}, 0.16F);

        door_mesh_.initialize(door_mesh_data);
        panel_mesh_.initialize(panel_mesh_data);
        panel_blocked_mesh_.initialize(panel_blocked_mesh_data);
        panel_highlight_mesh_.initialize(panel_highlight_mesh_data);
        panel_success_mesh_.initialize(panel_success_mesh_data);
        panel_warning_mesh_.initialize(panel_warning_mesh_data);
        pickup_mesh_.initialize(pickup_mesh_data);
        pickup_highlight_mesh_.initialize(pickup_highlight_mesh_data);
        turret_mesh_.initialize(turret_mesh_data);
        turret_destroyed_mesh_.initialize(turret_destroyed_mesh_data);
        turret_alert_mesh_.initialize(turret_alert_mesh_data);
        inspector_usable_mesh_.initialize(inspector_usable_mesh_data);
        inspector_door_mesh_.initialize(inspector_door_mesh_data);
        inspector_trigger_mesh_.initialize(inspector_trigger_mesh_data);
        inspector_solid_mesh_.initialize(inspector_solid_mesh_data);
        inspector_hit_mesh_.initialize(inspector_hit_mesh_data);

        render_chunks_.clear();
        render_chunks_.reserve(render_data.chunks.size());
        local_lights_.clear();
        authoring_warnings_.clear();

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

        for (std::size_t entity_index = 0; entity_index < level.entities.size(); ++entity_index)
        {
            const RelayCore::EntitySpawnData& entity = level.entities[entity_index];
            if (entity.classname != "render_light")
            {
                continue;
            }

            const RelayCore::AuthoredSourceLocation* source =
                entity_index < level.entity_sources.size() ? &level.entity_sources[entity_index] : nullptr;
            const std::string entity_label = format_entity_label(entity, entity_index);
            RelayCore::render::LocalLight light{};
            light.position = entity.origin;

            const std::optional<glm::vec3> authored_color = parse_scene_renderer_vec3_property(entity, "color");
            const std::optional<float> authored_intensity = parse_scene_renderer_float_property(entity, "intensity");
            const std::optional<float> authored_radius = parse_scene_renderer_float_property(entity, "radius");

            auto push_warning = [&](const std::string& message) {
                RelayCore::render::AuthoringWarningDiagnostics warning{};
                warning.message = message;
                warning.source_bucket = warning_source_bucket(source);
                assign_warning_source(warning, entity_label, source);
                authoring_warnings_.push_back(std::move(warning));
            };

            glm::vec3 light_color = authored_color.value_or(authoring_config.render_light.default_color);
            const float intensity = std::clamp(
                authored_intensity.value_or(authoring_config.render_light.default_intensity),
                authoring_config.render_light.min_intensity,
                authoring_config.render_light.max_intensity
            );
            const float radius = std::clamp(
                authored_radius.value_or(authoring_config.render_light.default_radius),
                authoring_config.render_light.min_radius,
                authoring_config.render_light.max_radius
            );

            if (!authored_color.has_value())
            {
                push_warning(
                    entity_label + " omits color; using render_light.default_color from render_authoring.toml"
                );
            } else if (color_outside_unit_range(*authored_color))
            {
                push_warning(
                    entity_label + " color " + format_light_color(*authored_color)
                    + " is outside the supported [0, 1] range and will be clamped"
                );
                light_color = glm::clamp(light_color, glm::vec3{0.0F}, glm::vec3{1.0F});
            }
            if (!authored_intensity.has_value())
            {
                push_warning(
                    entity_label + " omits intensity; using render_light.default_intensity from render_authoring.toml"
                );
            } else if (*authored_intensity < authoring_config.render_light.min_intensity
                       || *authored_intensity > authoring_config.render_light.max_intensity)
            {
                push_warning(
                    entity_label + " intensity " + std::to_string(*authored_intensity)
                    + " is outside the supported range and will be clamped"
                );
            }
            if (!authored_radius.has_value())
            {
                push_warning(
                    entity_label + " omits radius; using render_light.default_radius from render_authoring.toml"
                );
            } else if (*authored_radius < authoring_config.render_light.min_radius
                       || *authored_radius > authoring_config.render_light.max_radius)
            {
                push_warning(
                    entity_label + " radius " + std::to_string(*authored_radius)
                    + " is outside the supported range and will be clamped"
                );
            }
            if (std::max({light_color.x, light_color.y, light_color.z}) <= 0.01F && intensity > 0.0F)
            {
                push_warning(
                    entity_label + " resolves to an almost black color and contributes little visible lighting"
                );
            }

            light.color = light_color * intensity;
            light.radius = radius;
            local_lights_.push_back(light);
        }

        if (local_lights_.size() > kMaxActiveLocalLights)
        {
            authoring_warnings_.push_back(
                RelayCore::render::AuthoringWarningDiagnostics{
                    .message = "Map authors " + std::to_string(local_lights_.size())
                               + " render_light entities, but the renderer"
                                 " only evaluates the nearest "
                               + std::to_string(kMaxActiveLocalLights) + " per frame",
                    .source_bucket = "generated",
                }
            );
        }

        diagnostics_.total_solids = static_cast<std::uint32_t>(level.solids.size());
        diagnostics_.total_chunks = static_cast<std::uint32_t>(render_chunks_.size());
        diagnostics_.opaque_total_chunks = static_cast<std::uint32_t>(
            std::count_if(render_chunks_.begin(), render_chunks_.end(), [](const RenderChunk& chunk) {
                return !chunk.transparent;
            })
        );
        diagnostics_.transparent_total_chunks = diagnostics_.total_chunks - diagnostics_.opaque_total_chunks;
        diagnostics_.visible_solids = static_cast<std::uint32_t>(render_chunks_.size());
        diagnostics_.visible_chunks = static_cast<std::uint32_t>(render_chunks_.size());
        diagnostics_.opaque_visible_chunks = diagnostics_.opaque_total_chunks;
        diagnostics_.transparent_visible_chunks = diagnostics_.transparent_total_chunks;
        diagnostics_.draw_calls = diagnostics_.visible_solids;
        diagnostics_.opaque_draw_calls = diagnostics_.opaque_visible_chunks;
        diagnostics_.transparent_draw_calls = diagnostics_.transparent_visible_chunks;
        diagnostics_.triangles = render_data.total_triangles;
        diagnostics_.total_render_lights = static_cast<std::uint32_t>(local_lights_.size());
        diagnostics_.active_local_light_budget = static_cast<std::uint32_t>(kMaxActiveLocalLights);
        diagnostics_.local_light_count =
            static_cast<std::uint32_t>(std::min(local_lights_.size(), kMaxActiveLocalLights));
    }

    void WorldSceneRenderer::shutdown() noexcept
    {
        RELAYCORE_PROFILE_FUNCTION();
        render_queue_.clear();
        render_chunks_.clear();
        render_chunks_.shrink_to_fit();

        door_mesh_ = RelayCore::render::StaticColorMesh{};
        panel_mesh_ = RelayCore::render::StaticColorMesh{};
        panel_blocked_mesh_ = RelayCore::render::StaticColorMesh{};
        panel_highlight_mesh_ = RelayCore::render::StaticColorMesh{};
        panel_success_mesh_ = RelayCore::render::StaticColorMesh{};
        panel_warning_mesh_ = RelayCore::render::StaticColorMesh{};
        pickup_mesh_ = RelayCore::render::StaticColorMesh{};
        pickup_highlight_mesh_ = RelayCore::render::StaticColorMesh{};
        turret_mesh_ = RelayCore::render::StaticColorMesh{};
        turret_destroyed_mesh_ = RelayCore::render::StaticColorMesh{};
        turret_alert_mesh_ = RelayCore::render::StaticColorMesh{};
        inspector_usable_mesh_ = RelayCore::render::StaticColorMesh{};
        inspector_door_mesh_ = RelayCore::render::StaticColorMesh{};
        inspector_trigger_mesh_ = RelayCore::render::StaticColorMesh{};
        inspector_solid_mesh_ = RelayCore::render::StaticColorMesh{};
        inspector_hit_mesh_ = RelayCore::render::StaticColorMesh{};
        local_lights_.clear();
        active_local_lights_.clear();
        authoring_warnings_.clear();
        diagnostics_ = {};
    }

    void WorldSceneRenderer::render(
        RelayCore::Renderer& renderer,
        const RelayCore::render::PerspectiveCamera& camera,
        const float scene_time_seconds,
        std::span<const RelayCore::gameplay::RuntimeDoor> doors,
        std::span<const RelayCore::gameplay::RuntimeTurret> turrets,
        std::span<const RelayCore::gameplay::RuntimeUsable> usables,
        const int focused_usable_index,
        const RelayCore::render::InspectorDiagnostics& inspector,
        const bool inspector_visible
    )
    {
        RELAYCORE_PROFILE_ZONE_N("WorldSceneRenderer::render");

        diagnostics_.visible_solids = 0;
        diagnostics_.visible_chunks = 0;
        diagnostics_.culled_chunks = 0;
        diagnostics_.opaque_visible_chunks = 0;
        diagnostics_.transparent_visible_chunks = 0;
        diagnostics_.draw_calls = 0;
        diagnostics_.opaque_draw_calls = 0;
        diagnostics_.overlay_draw_calls = 0;
        diagnostics_.transparent_draw_calls = 0;
        diagnostics_.door_draw_calls = 0;
        diagnostics_.usable_draw_calls = 0;
        diagnostics_.highlight_draw_calls = 0;
        diagnostics_.feedback_draw_calls = 0;
        diagnostics_.inspector_draw_calls = 0;
        diagnostics_.inspector_marker_draw_calls = 0;
        diagnostics_.triangles = 0;
        diagnostics_.opaque_triangles = 0;
        diagnostics_.overlay_triangles = 0;
        diagnostics_.transparent_triangles = 0;
        diagnostics_.local_light_count = 0;
        render_queue_.clear();
        render_queue_.reserve(render_chunks_.size() + doors.size() + turrets.size() + (usables.size() * 3));
        std::vector<const RenderChunk*> transparent_chunks{};
        transparent_chunks.reserve(render_chunks_.size());

        frustum_planes_ = extract_frustum_planes(camera.view_projection_matrix());
        {
            RELAYCORE_PROFILE_ZONE_N("WorldSceneRenderer::draw_visible_blockout_chunks");
            for (const RenderChunk& chunk : render_chunks_)
            {
                if (!aabb_intersects_frustum(frustum_planes_, chunk.mins, chunk.maxs))
                {
                    ++diagnostics_.culled_chunks;
                    continue;
                }

                record_visible_chunk(chunk.transparent);
                if (chunk.transparent)
                {
                    transparent_chunks.push_back(&chunk);
                    continue;
                }

                record_render_draw(RelayCore::render::RenderPass::Opaque, chunk.triangle_count);
                render_queue_.push(chunk.mesh, glm::mat4{1.0F}, RelayCore::render::RenderPass::Opaque);
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

        render_doors(camera, doors);
        render_turrets(camera, turrets);
        render_interactive_usables(camera, scene_time_seconds, usables, focused_usable_index);
        if (inspector_visible)
        {
            render_inspector_pick(camera, scene_time_seconds, inspector);
        }

        for (const RenderChunk* chunk : transparent_chunks)
        {
            record_render_draw(RelayCore::render::RenderPass::Transparent, chunk->triangle_count);
            render_queue_.push(chunk->mesh, glm::mat4{1.0F}, RelayCore::render::RenderPass::Transparent);
        }

        active_local_lights_.clear();
        if (!local_lights_.empty())
        {
            std::vector<const RelayCore::render::LocalLight*> sorted_lights{};
            sorted_lights.reserve(local_lights_.size());
            for (const RelayCore::render::LocalLight& light : local_lights_)
            {
                sorted_lights.push_back(&light);
            }

            std::ranges::sort(sorted_lights, [&camera](const auto* lhs, const auto* rhs) {
                const float lhs_score = local_light_score(*lhs, camera);
                const float rhs_score = local_light_score(*rhs, camera);
                if (std::abs(lhs_score - rhs_score) <= std::numeric_limits<float>::epsilon())
                {
                    return lhs->radius > rhs->radius;
                }
                return lhs_score < rhs_score;
            });

            const std::size_t active_count = std::min(sorted_lights.size(), kMaxActiveLocalLights);
            active_local_lights_.reserve(active_count);
            for (std::size_t index = 0; index < active_count; ++index)
            {
                active_local_lights_.push_back(*sorted_lights[index]);
            }
        }

        diagnostics_.local_light_count = static_cast<std::uint32_t>(active_local_lights_.size());
        renderer.set_local_lights(active_local_lights_);
        RELAYCORE_PROFILE_PLOT("Render/VisibleChunks", diagnostics_.visible_chunks);
        RELAYCORE_PROFILE_PLOT("Render/TransparentVisibleChunks", diagnostics_.transparent_visible_chunks);
        RELAYCORE_PROFILE_PLOT("Render/DrawCalls", diagnostics_.draw_calls);
        RELAYCORE_PROFILE_PLOT("Render/OpaqueDrawCalls", diagnostics_.opaque_draw_calls);
        RELAYCORE_PROFILE_PLOT("Render/OverlayDrawCalls", diagnostics_.overlay_draw_calls);
        RELAYCORE_PROFILE_PLOT("Render/TransparentDrawCalls", diagnostics_.transparent_draw_calls);
        RELAYCORE_PROFILE_PLOT("Render/InspectorDrawCalls", diagnostics_.inspector_draw_calls);
        RELAYCORE_PROFILE_PLOT("Render/InspectorMarkerDrawCalls", diagnostics_.inspector_marker_draw_calls);
        RELAYCORE_PROFILE_PLOT("Render/Triangles", diagnostics_.triangles);
        RELAYCORE_PROFILE_PLOT("Render/OpaqueTriangles", diagnostics_.opaque_triangles);
        RELAYCORE_PROFILE_PLOT("Render/OverlayTriangles", diagnostics_.overlay_triangles);
        RELAYCORE_PROFILE_PLOT("Render/TransparentTriangles", diagnostics_.transparent_triangles);
        RELAYCORE_PROFILE_PLOT("Render/LocalLightsActive", diagnostics_.local_light_count);
        renderer.draw_render_queue(render_queue_.items());
    }

    auto WorldSceneRenderer::diagnostics() const noexcept -> const RelayCore::render::WorldDiagnostics&
    {
        RELAYCORE_PROFILE_FUNCTION();
        return diagnostics_;
    }

    auto WorldSceneRenderer::authoring_warnings() const noexcept
        -> const std::vector<RelayCore::render::AuthoringWarningDiagnostics>&
    {
        RELAYCORE_PROFILE_FUNCTION();
        return authoring_warnings_;
    }

    void WorldSceneRenderer::record_visible_chunk(const bool transparent) noexcept
    {
        RELAYCORE_PROFILE_FUNCTION();
        ++diagnostics_.visible_solids;
        ++diagnostics_.visible_chunks;
        if (transparent)
        {
            ++diagnostics_.transparent_visible_chunks;
        } else
        {
            ++diagnostics_.opaque_visible_chunks;
        }
    }

    void WorldSceneRenderer::record_render_draw(
        const RelayCore::render::RenderPass pass,
        const std::uint32_t triangles
    ) noexcept
    {
        RELAYCORE_PROFILE_FUNCTION();
        ++diagnostics_.draw_calls;
        diagnostics_.triangles += triangles;

        switch (pass)
        {
            case RelayCore::render::RenderPass::Opaque:
                ++diagnostics_.opaque_draw_calls;
                diagnostics_.opaque_triangles += triangles;
                break;
            case RelayCore::render::RenderPass::Overlay:
                ++diagnostics_.overlay_draw_calls;
                diagnostics_.overlay_triangles += triangles;
                break;
            case RelayCore::render::RenderPass::Transparent:
                ++diagnostics_.transparent_draw_calls;
                diagnostics_.transparent_triangles += triangles;
                break;
        }
    }
} // namespace RelayGame
