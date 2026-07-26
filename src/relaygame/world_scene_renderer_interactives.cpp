#include "relaycore/core/profiling.h"
#include "relaycore/render/camera.h"
#include "relaygame/world_scene_renderer.h"

#include <cmath>
#include <glm/common.hpp>
#include <glm/geometric.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace RelayGame
{
    namespace
    {
        [[nodiscard]] auto
        aabb_intersects_planes(const std::array<glm::vec4, 6>& planes, const glm::vec3& mins, const glm::vec3& maxs)
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

        [[nodiscard]] auto make_aabb_model_matrix(const RelayCore::physics::Aabb& bounds) -> glm::mat4
        {
            RELAYCORE_PROFILE_FUNCTION();
            const glm::vec3 center = (bounds.mins + bounds.maxs) * 0.5F;
            const glm::vec3 size = bounds.maxs - bounds.mins;

            glm::mat4 model{1.0F};
            model = glm::translate(model, center);
            model = glm::scale(model, size);
            return model;
        }

        [[nodiscard]] auto expand_aabb(const RelayCore::physics::Aabb& bounds, const glm::vec3& amount)
            -> RelayCore::physics::Aabb
        {
            RELAYCORE_PROFILE_FUNCTION();
            return {bounds.mins - amount, bounds.maxs + amount};
        }

        [[nodiscard]] auto expanded_pickup_visual_bounds(const RelayCore::physics::Aabb& bounds)
            -> RelayCore::physics::Aabb
        {
            RELAYCORE_PROFILE_FUNCTION();
            const glm::vec3 center = (bounds.mins + bounds.maxs) * 0.5F;
            const glm::vec3 half_extents = (bounds.maxs - bounds.mins) * 0.5F;
            const glm::vec3 visual_half_extents{
                std::max(half_extents.x, 0.10F),
                std::max(half_extents.y, 0.06F),
                std::max(half_extents.z, 0.03F),
            };
            return {center - visual_half_extents, center + visual_half_extents};
        }

        enum class PanelPlaneAxis : std::uint8_t {
            X,
            Y,
            Z,
        };

        [[nodiscard]] auto thinnest_panel_axis(const RelayCore::physics::Aabb& bounds) -> PanelPlaneAxis
        {
            RELAYCORE_PROFILE_FUNCTION();
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

        [[nodiscard]] auto panel_plane_normal(
            const RelayCore::gameplay::RuntimeUsable& usable,
            const RelayCore::render::PerspectiveCamera& camera
        ) -> glm::vec3
        {
            RELAYCORE_PROFILE_FUNCTION();
            switch (usable.panel_mount_axis)
            {
                case RelayCore::gameplay::PanelMountAxis::PositiveX:
                    return {1.0F, 0.0F, 0.0F};
                case RelayCore::gameplay::PanelMountAxis::NegativeX:
                    return {-1.0F, 0.0F, 0.0F};
                case RelayCore::gameplay::PanelMountAxis::PositiveZ:
                    return {0.0F, 0.0F, 1.0F};
                case RelayCore::gameplay::PanelMountAxis::NegativeZ:
                    return {0.0F, 0.0F, -1.0F};
                case RelayCore::gameplay::PanelMountAxis::Centered:
                    break;
            }

            const RelayCore::physics::Aabb& bounds = usable.visual_bounds;
            const glm::vec3 center = (bounds.mins + bounds.maxs) * 0.5F;
            const glm::vec3 to_camera = camera.position() - center;

            switch (thinnest_panel_axis(bounds))
            {
                case PanelPlaneAxis::X:
                    return {to_camera.x >= 0.0F ? 1.0F : -1.0F, 0.0F, 0.0F};
                case PanelPlaneAxis::Y:
                    return {0.0F, to_camera.y >= 0.0F ? 1.0F : -1.0F, 0.0F};
                case PanelPlaneAxis::Z:
                    return {0.0F, 0.0F, to_camera.z >= 0.0F ? 1.0F : -1.0F};
            }

            return {0.0F, 0.0F, 1.0F};
        }

        [[nodiscard]] auto make_panel_plate_model_matrix(
            const RelayCore::gameplay::RuntimeUsable& usable,
            const RelayCore::render::PerspectiveCamera& camera,
            const float in_plane_padding,
            const float camera_offset
        ) -> glm::mat4
        {
            RELAYCORE_PROFILE_FUNCTION();
            const RelayCore::physics::Aabb& bounds = usable.visual_bounds;
            const glm::vec3 center = (bounds.mins + bounds.maxs) * 0.5F;
            const glm::vec3 size = bounds.maxs - bounds.mins;
            const glm::vec3 plane_normal = panel_plane_normal(usable, camera);

            glm::mat4 model{1.0F};
            model = glm::translate(model, center + (plane_normal * camera_offset));

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
                    break;
            }

            model = glm::scale(model, scale);
            return model;
        }

        [[nodiscard]] auto mesh_triangle_count(const RelayCore::render::StaticColorMesh& mesh) -> std::uint32_t
        {
            RELAYCORE_PROFILE_FUNCTION();
            return static_cast<std::uint32_t>(mesh.index_count() / 3);
        }

        [[nodiscard]] auto make_centered_box_model_matrix(const glm::vec3& center, const glm::vec3& half_extents)
            -> glm::mat4
        {
            RELAYCORE_PROFILE_FUNCTION();
            glm::mat4 model{1.0F};
            model = glm::translate(model, center);
            model = glm::scale(model, half_extents * 2.0F);
            return model;
        }

        [[nodiscard]] auto inspector_expanded_bounds(
            const RelayCore::render::InspectorDiagnostics& inspector,
            const float scene_time_seconds
        ) -> RelayCore::physics::Aabb
        {
            RELAYCORE_PROFILE_FUNCTION();
            const float pulse = 0.5F + (0.5F * std::sin(scene_time_seconds * 6.0F));

            glm::vec3 expansion{0.035F + (pulse * 0.012F)};
            if (inspector.object_kind == "solid")
            {
                expansion = glm::vec3{0.025F + (pulse * 0.010F)};
            } else if (inspector.object_kind == "trigger")
            {
                expansion = glm::vec3{0.045F + (pulse * 0.016F)};
            } else if (inspector.object_kind == "usable")
            {
                expansion = glm::vec3{0.030F + (pulse * 0.010F)};
            }

            return {
                .mins = inspector.bounds_mins - expansion,
                .maxs = inspector.bounds_maxs + expansion,
            };
        }

    } // namespace

    void WorldSceneRenderer::render_doors(
        const RelayCore::render::PerspectiveCamera&,
        std::span<const RelayCore::gameplay::RuntimeDoor> doors
    )
    {
        RELAYCORE_PROFILE_FUNCTION();
        for (const RelayCore::gameplay::RuntimeDoor& door : doors)
        {
            const RelayCore::physics::Aabb bounds = RelayCore::gameplay::current_door_bounds(door);
            if (!aabb_intersects_planes(frustum_planes_, bounds.mins, bounds.maxs))
            {
                continue;
            }

            ++diagnostics_.door_draw_calls;
            record_render_draw(RelayCore::render::RenderPass::Opaque, mesh_triangle_count(door_mesh_));
            render_queue_.push(door_mesh_, make_aabb_model_matrix(bounds), RelayCore::render::RenderPass::Opaque);
        }
    }

    void WorldSceneRenderer::render_interactive_usables(
        const RelayCore::render::PerspectiveCamera& camera,
        const float scene_time_seconds,
        std::span<const RelayCore::gameplay::RuntimeUsable> usables,
        const int focused_usable_index,
        const bool highlight_visible
    )
    {
        RELAYCORE_PROFILE_FUNCTION();
        const float focus_pulse = 0.5F + (0.5F * std::sin(scene_time_seconds * 7.5F));

        for (std::size_t usable_index = 0; usable_index < usables.size(); ++usable_index)
        {
            const RelayCore::gameplay::RuntimeUsable& usable = usables[usable_index];
            if (!usable.enabled || !usable.has_visual_bounds
                || (usable.visual_kind != RelayCore::gameplay::UsableVisualKind::Panel
                    && usable.visual_kind != RelayCore::gameplay::UsableVisualKind::Pickup))
            {
                continue;
            }

            if (!aabb_intersects_planes(frustum_planes_, usable.visual_bounds.mins, usable.visual_bounds.maxs))
            {
                continue;
            }

            const bool is_focused = highlight_visible && static_cast<int>(usable_index) == focused_usable_index;
            const bool is_panel = usable.visual_kind == RelayCore::gameplay::UsableVisualKind::Panel;
            const bool is_blocked = usable.locked || !usable.requirement_met;
            const RelayCore::render::StaticColorMesh& base_mesh =
                is_panel ? (is_blocked ? panel_blocked_mesh_ : panel_mesh_) : pickup_mesh_;
            const RelayCore::render::StaticColorMesh& highlight_mesh =
                is_panel ? (is_blocked ? panel_warning_mesh_ : panel_highlight_mesh_) : pickup_highlight_mesh_;
            const float highlight_padding = is_panel ? (0.08F + (focus_pulse * 0.045F)) : 0.05F;
            const float highlight_offset = is_panel ? (0.014F + (focus_pulse * 0.006F)) : 0.014F;
            const float base_offset = is_panel ? 0.020F : 0.010F;
            const float feedback_strength =
                usable.feedback_total_seconds > 0.001F
                    ? glm::clamp(usable.feedback_remaining_seconds / usable.feedback_total_seconds, 0.0F, 1.0F)
                    : 0.0F;
            const RelayCore::physics::Aabb pickup_bounds =
                is_panel ? usable.visual_bounds : expanded_pickup_visual_bounds(usable.visual_bounds);

            if (is_panel && feedback_strength > 0.0F
                && usable.feedback_style != RelayCore::gameplay::UsableFeedbackStyle::None)
            {
                const RelayCore::render::StaticColorMesh& feedback_mesh =
                    usable.feedback_style == RelayCore::gameplay::UsableFeedbackStyle::Success ? panel_success_mesh_
                                                                                               : panel_warning_mesh_;
                const float feedback_padding = 0.045F + (feedback_strength * 0.085F);
                const float feedback_offset = 0.012F + (feedback_strength * 0.014F);

                ++diagnostics_.feedback_draw_calls;
                record_render_draw(RelayCore::render::RenderPass::Overlay, mesh_triangle_count(feedback_mesh));
                render_queue_.push(
                    feedback_mesh,
                    make_panel_plate_model_matrix(usable, camera, feedback_padding, feedback_offset),
                    RelayCore::render::RenderPass::Overlay
                );
            }

            if (is_focused)
            {
                ++diagnostics_.highlight_draw_calls;
                record_render_draw(RelayCore::render::RenderPass::Overlay, mesh_triangle_count(highlight_mesh));
                if (is_panel)
                {
                    render_queue_.push(
                        highlight_mesh,
                        make_panel_plate_model_matrix(usable, camera, highlight_padding, highlight_offset),
                        RelayCore::render::RenderPass::Overlay
                    );
                } else
                {
                    const glm::vec3 pulse_extent = glm::vec3{
                        0.014F + (focus_pulse * 0.010F),
                        0.014F + (focus_pulse * 0.010F),
                        0.010F + (focus_pulse * 0.006F),
                    };
                    render_queue_.push(
                        highlight_mesh,
                        make_aabb_model_matrix(expand_aabb(pickup_bounds, pulse_extent)),
                        RelayCore::render::RenderPass::Overlay
                    );
                }
            }

            ++diagnostics_.usable_draw_calls;
            const RelayCore::render::RenderPass base_pass =
                is_panel ? RelayCore::render::RenderPass::Overlay : RelayCore::render::RenderPass::Opaque;
            record_render_draw(base_pass, mesh_triangle_count(base_mesh));
            if (is_panel)
            {
                render_queue_.push(
                    base_mesh,
                    make_panel_plate_model_matrix(usable, camera, 0.040F, base_offset),
                    RelayCore::render::RenderPass::Overlay
                );
            } else
            {
                render_queue_
                    .push(base_mesh, make_aabb_model_matrix(pickup_bounds), RelayCore::render::RenderPass::Opaque);
            }
        }
    }

    void WorldSceneRenderer::render_turrets(
        const RelayCore::render::PerspectiveCamera&,
        const std::span<const RelayCore::gameplay::RuntimeTurret> turrets
    )
    {
        RELAYCORE_PROFILE_FUNCTION();
        for (const RelayCore::gameplay::RuntimeTurret& turret : turrets)
        {
            if (!aabb_intersects_planes(frustum_planes_, turret.bounds.mins, turret.bounds.maxs))
            {
                continue;
            }

            const RelayCore::render::StaticColorMesh& base_mesh =
                turret.destroyed ? turret_destroyed_mesh_ : turret_mesh_;
            record_render_draw(RelayCore::render::RenderPass::Opaque, mesh_triangle_count(base_mesh));
            render_queue_.push(base_mesh, make_aabb_model_matrix(turret.bounds), RelayCore::render::RenderPass::Opaque);

            const float alert_strength =
                std::max(turret.hit_flash_remaining_seconds * 4.5F, turret.fire_flash_remaining_seconds * 5.5F);
            if (turret.destroyed || alert_strength <= 0.001F)
            {
                continue;
            }

            const RelayCore::physics::Aabb alert_bounds =
                expand_aabb(turret.bounds, glm::vec3{0.025F + std::min(alert_strength, 0.06F)});
            record_render_draw(RelayCore::render::RenderPass::Overlay, mesh_triangle_count(turret_alert_mesh_));
            render_queue_
                .push(turret_alert_mesh_, make_aabb_model_matrix(alert_bounds), RelayCore::render::RenderPass::Overlay);
        }
    }

    void WorldSceneRenderer::render_inspector_pick(
        const RelayCore::render::PerspectiveCamera&,
        const float scene_time_seconds,
        const RelayCore::render::InspectorDiagnostics& inspector
    )
    {
        RELAYCORE_PROFILE_FUNCTION();
        if (!inspector.has_pick)
        {
            return;
        }

        const RelayCore::physics::Aabb expanded_bounds = inspector_expanded_bounds(inspector, scene_time_seconds);
        if (!aabb_intersects_planes(frustum_planes_, expanded_bounds.mins, expanded_bounds.maxs))
        {
            return;
        }

        RelayCore::render::StaticColorMesh* highlight_mesh = &inspector_solid_mesh_;
        if (inspector.object_kind == "usable")
        {
            highlight_mesh = &inspector_usable_mesh_;
        } else if (inspector.object_kind == "door")
        {
            highlight_mesh = &inspector_door_mesh_;
        } else if (inspector.object_kind == "trigger")
        {
            highlight_mesh = &inspector_trigger_mesh_;
        }

        ++diagnostics_.inspector_draw_calls;
        record_render_draw(RelayCore::render::RenderPass::Overlay, mesh_triangle_count(*highlight_mesh));
        render_queue_
            .push(*highlight_mesh, make_aabb_model_matrix(expanded_bounds), RelayCore::render::RenderPass::Overlay);

        const glm::vec3 hit_half_extents{
            inspector.object_kind == "solid" ? 0.045F : 0.055F,
            inspector.object_kind == "solid" ? 0.045F : 0.055F,
            inspector.object_kind == "solid" ? 0.045F : 0.055F,
        };
        const RelayCore::physics::Aabb hit_bounds{
            .mins = inspector.hit_point - hit_half_extents,
            .maxs = inspector.hit_point + hit_half_extents,
        };
        if (!aabb_intersects_planes(frustum_planes_, hit_bounds.mins, hit_bounds.maxs))
        {
            return;
        }

        ++diagnostics_.inspector_marker_draw_calls;
        record_render_draw(RelayCore::render::RenderPass::Overlay, mesh_triangle_count(inspector_hit_mesh_));
        render_queue_.push(
            inspector_hit_mesh_,
            make_centered_box_model_matrix(inspector.hit_point, hit_half_extents),
            RelayCore::render::RenderPass::Overlay
        );
    }
} // namespace RelayGame
