#include "relaycore/core/profiling.h"
#include "relaycore/render/camera.h"
#include "relaygame/world_scene_renderer.h"

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
            return {bounds.mins - amount, bounds.maxs + amount};
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
                    break;
            }

            model = glm::scale(model, scale);
            return model;
        }

        [[nodiscard]] auto mesh_triangle_count(const RelayCore::render::StaticColorMesh& mesh) -> std::uint32_t
        {
            return static_cast<std::uint32_t>(mesh.index_count() / 3);
        }
    } // namespace

    void WorldSceneRenderer::render_doors(
        const RelayCore::render::PerspectiveCamera&,
        std::span<const RelayCore::gameplay::RuntimeDoor> doors
    )
    {
        for (const RelayCore::gameplay::RuntimeDoor& door : doors)
        {
            const RelayCore::physics::Aabb bounds = RelayCore::gameplay::current_door_bounds(door);
            if (!aabb_intersects_planes(frustum_planes_, bounds.mins, bounds.maxs))
            {
                continue;
            }

            ++diagnostics_.draw_calls;
            diagnostics_.triangles += mesh_triangle_count(door_mesh_);
            render_queue_.push(door_mesh_, make_aabb_model_matrix(bounds));
        }
    }

    void WorldSceneRenderer::render_interactive_usables(
        const RelayCore::render::PerspectiveCamera& camera,
        std::span<const RelayCore::gameplay::RuntimeUsable> usables,
        const int focused_usable_index
    )
    {
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

            const bool is_focused = static_cast<int>(usable_index) == focused_usable_index;
            const bool is_panel = usable.visual_kind == RelayCore::gameplay::UsableVisualKind::Panel;
            const RelayCore::render::StaticColorMesh& base_mesh = is_panel ? panel_mesh_ : pickup_mesh_;
            const RelayCore::render::StaticColorMesh& highlight_mesh =
                is_panel ? panel_highlight_mesh_ : pickup_highlight_mesh_;
            const float highlight_padding = is_panel ? 0.08F : 0.05F;
            const float highlight_offset = is_panel ? 0.012F : 0.014F;
            const float base_offset = is_panel ? 0.006F : 0.010F;
            const float feedback_strength =
                usable.feedback_total_seconds > 0.001F
                    ? glm::clamp(usable.feedback_remaining_seconds / usable.feedback_total_seconds, 0.0F, 1.0F)
                    : 0.0F;

            if (is_panel && feedback_strength > 0.0F
                && usable.feedback_style != RelayCore::gameplay::UsableFeedbackStyle::None)
            {
                const RelayCore::render::StaticColorMesh& feedback_mesh =
                    usable.feedback_style == RelayCore::gameplay::UsableFeedbackStyle::Success ? panel_success_mesh_
                                                                                               : panel_warning_mesh_;
                const float feedback_padding = 0.045F + (feedback_strength * 0.085F);
                const float feedback_offset = 0.010F + (feedback_strength * 0.010F);

                ++diagnostics_.draw_calls;
                diagnostics_.triangles += mesh_triangle_count(feedback_mesh);
                render_queue_.push(
                    feedback_mesh,
                    make_panel_plate_model_matrix(usable.visual_bounds, camera, feedback_padding, feedback_offset)
                );
            }

            if (is_focused)
            {
                ++diagnostics_.draw_calls;
                diagnostics_.triangles += mesh_triangle_count(highlight_mesh);
                if (is_panel)
                {
                    render_queue_.push(
                        highlight_mesh,
                        make_panel_plate_model_matrix(usable.visual_bounds, camera, highlight_padding, highlight_offset)
                    );
                } else
                {
                    render_queue_.push(
                        highlight_mesh,
                        make_aabb_model_matrix(expand_aabb(usable.visual_bounds, glm::vec3{0.008F, 0.008F, 0.004F}))
                    );
                }
            }

            ++diagnostics_.draw_calls;
            diagnostics_.triangles += mesh_triangle_count(base_mesh);
            if (is_panel)
            {
                render_queue_.push(
                    base_mesh,
                    make_panel_plate_model_matrix(usable.visual_bounds, camera, 0.0F, base_offset)
                );
            } else
            {
                render_queue_.push(base_mesh, make_aabb_model_matrix(usable.visual_bounds));
            }
        }
    }
} // namespace RelayGame
