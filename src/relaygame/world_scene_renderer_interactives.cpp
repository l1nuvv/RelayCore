#include "relaycore/core/profiling.h"
#include "relaycore/render/camera.h"
#include "relaycore/render/renderer.h"
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

    void WorldSceneRenderer::render_pistol_viewmodel(
        RelayCore::Renderer& renderer,
        const RelayCore::render::PerspectiveCamera& camera,
        const float cycle_remaining_seconds,
        const float refire_seconds
    )
    {
        const auto items = prepare_pistol_viewmodel(camera, cycle_remaining_seconds, refire_seconds);
        for (const auto& item : items)
            record_render_draw(RelayCore::render::RenderPass::Overlay, mesh_triangle_count(*item.mesh));
        renderer.draw_viewmodel(items);
    }

    auto WorldSceneRenderer::prepare_pistol_viewmodel(
        const RelayCore::render::PerspectiveCamera& camera,
        const float cycle_remaining_seconds,
        const float refire_seconds
    ) -> std::span<const RelayCore::render::RenderItem>
    {
        const float elapsed = refire_seconds - cycle_remaining_seconds;
        const float recoil =
            cycle_remaining_seconds > 0
                ? std::sin(glm::pi<float>() * glm::clamp(elapsed / glm::min(refire_seconds, .18F), 0.0F, 1.0F))
                : 0;
        const float slide =
            cycle_remaining_seconds > 0
                ? .028F * std::sin(glm::pi<float>() * glm::clamp(elapsed / glm::min(refire_seconds, .10F), 0.0F, 1.0F))
                : 0;
        glm::mat4 basis{1};
        basis[0] = glm::vec4{camera.right(), 0};
        basis[1] = glm::vec4{camera.up(), 0};
        basis[2] = glm::vec4{-camera.forward(), 0};
        basis[3] = glm::vec4{camera.position(), 1};
        auto model = glm::translate(basis, {.12F, -.13F + recoil * .012F, -.34F + recoil * .020F});
        // The resting bore converges on the eye ray at 12 m; hitscan stays at the eye.
        model = glm::rotate(model, std::atan(.12F / 11.66F), glm::vec3{0, 1, 0});
        model = glm::rotate(model, std::atan(.083F / 11.66F) + recoil * .13F, glm::vec3{1, 0, 0});
        pistol_viewmodel_.clear();
        for (std::size_t index = 0; index < pistol_models_.size(); ++index)
        {
            const auto& asset = *pistol_models_[index];
            const auto part_model = index == 1 ? glm::translate(model, {0, 0, slide}) : model;
            for (const auto& part : asset.parts)
            {
                const auto& surface = asset.materials[part.material];
                pistol_viewmodel_.push(
                    asset.meshes[part.mesh],
                    part_model * part.transform,
                    RelayCore::render::RenderPass::Opaque,
                    &surface.base_color,
                    false,
                    surface.normal.cpu_data() ? &surface.normal : nullptr,
                    surface.normal_scale,
                    surface.metallic_roughness
                );
            }
        }
        return pistol_viewmodel_.items();
    }

    void WorldSceneRenderer::render_shotgun_viewmodel(
        RelayCore::Renderer& renderer,
        const RelayCore::render::PerspectiveCamera& camera,
        float cycle_remaining_seconds
    )
    {
        const auto items = prepare_shotgun_viewmodel(camera, cycle_remaining_seconds);
        for (const auto& item : items)
            record_render_draw(RelayCore::render::RenderPass::Overlay, mesh_triangle_count(*item.mesh));
        renderer.draw_viewmodel(items);
    }

    auto WorldSceneRenderer::prepare_shotgun_viewmodel(
        const RelayCore::render::PerspectiveCamera& camera,
        float cycle_remaining_seconds
    ) -> std::span<const RelayCore::render::RenderItem>
    {
        const float elapsed = .85F - cycle_remaining_seconds;
        const float recoil =
            cycle_remaining_seconds > 0 ? std::sin(glm::pi<float>() * glm::clamp(elapsed / .14F, 0.0F, 1.0F)) : 0;
        const float pump = cycle_remaining_seconds > 0
                               ? .07F * std::sin(glm::pi<float>() * glm::clamp((elapsed - .18F) / .54F, 0.0F, 1.0F))
                               : 0;
        glm::mat4 basis{1};
        basis[0] = glm::vec4{camera.right(), 0};
        basis[1] = glm::vec4{camera.up(), 0};
        basis[2] = glm::vec4{-camera.forward(), 0};
        basis[3] = glm::vec4{camera.position(), 1};
        auto model = glm::translate(basis, {.19F, -.18F + recoil * .008F, -.60F + recoil * .035F});
        // The bore converges on the eye ray at 12 m, independently of the hitscan origin.
        model = glm::rotate(model, std::atan(.19F / 11.4F), glm::vec3{0, 1, 0});
        model = glm::rotate(model, std::atan(.19645F / 11.4F) + recoil * .055F, glm::vec3{1, 0, 0});
        shotgun_viewmodel_.clear();
        for (std::size_t index = 0; index < shotgun_view_models_.size(); ++index)
        {
            const auto& asset = *shotgun_view_models_[index];
            const auto part_model = index == 1 ? glm::translate(model, {0, 0, pump}) : model;
            for (const auto& part : asset.parts)
            {
                const auto& surface = asset.materials[part.material];
                shotgun_viewmodel_.push(
                    asset.meshes[part.mesh],
                    part_model * part.transform,
                    RelayCore::render::RenderPass::Opaque,
                    &surface.base_color,
                    false,
                    surface.normal.cpu_data() ? &surface.normal : nullptr,
                    surface.normal_scale,
                    surface.metallic_roughness
                );
            }
        }
        return shotgun_viewmodel_.items();
    }

    void WorldSceneRenderer::queue_interactive_model(
        const RelayCore::render::ModelAsset& asset,
        const glm::mat4& model,
        const bool visible
    )
    {
        for (const auto& part : asset.parts)
        {
            const auto& mesh = asset.meshes[part.mesh];
            if (!visible)
            {
                shadow_casters_.push(mesh, model * part.transform);
                continue;
            }
            const auto& surface = asset.materials[part.material];
            record_render_draw(RelayCore::render::RenderPass::Opaque, mesh_triangle_count(mesh));
            render_queue_.push(
                mesh,
                model * part.transform,
                RelayCore::render::RenderPass::Opaque,
                &surface.base_color,
                true,
                surface.normal.cpu_data() ? &surface.normal : nullptr,
                surface.normal_scale,
                surface.metallic_roughness
            );
        }
    }

    void WorldSceneRenderer::render_doors(
        const RelayCore::render::PerspectiveCamera&,
        std::span<const RelayCore::gameplay::RuntimeDoor> doors
    )
    {
        RELAYCORE_PROFILE_FUNCTION();
        for (const RelayCore::gameplay::RuntimeDoor& door : doors)
        {
            const RelayCore::physics::Aabb bounds = RelayCore::gameplay::current_door_bounds(door);
            const auto size = bounds.maxs - bounds.mins;
            const bool faces_x = size.x < size.z;
            const bool lift =
                std::abs(door.open_offset.y) >= std::max(std::abs(door.open_offset.x), std::abs(door.open_offset.z));
            auto model = glm::translate(glm::mat4{1}, (bounds.mins + bounds.maxs) * .5F);
            if (faces_x)
                model = glm::rotate(model, -glm::half_pi<float>(), glm::vec3{0, 1, 0});
            model = glm::scale(model, faces_x ? glm::vec3{size.z, size.y, size.x} : size);
            const bool visible = aabb_intersects_planes(frustum_planes_, bounds.mins, bounds.maxs);
            if (visible)
                ++diagnostics_.door_draw_calls;
            queue_interactive_model(*door_models_[lift ? 0 : 1], model, visible);

            // Frame returns sit beyond both depth faces of the entire swept leaf.
            // The opening and the moving collision AABB remain exactly authored.
            const auto sweep_min = glm::min(door.closed_bounds.mins, door.closed_bounds.mins + door.open_offset);
            const auto sweep_max = glm::max(door.closed_bounds.maxs, door.closed_bounds.maxs + door.open_offset);
            const auto frame_box = [&](const glm::vec3& center, const glm::vec3& extent) {
                const auto transform = make_centered_box_model_matrix(center, extent * .5F);
                if (aabb_intersects_planes(frustum_planes_, center - extent * .5F, center + extent * .5F))
                {
                    record_render_draw(RelayCore::render::RenderPass::Opaque, mesh_triangle_count(door_guide_mesh_));
                    render_queue_.push(door_guide_mesh_, transform, RelayCore::render::RenderPass::Opaque);
                } else
                    shadow_casters_.push(door_guide_mesh_, transform);
            };
            const int width_axis = faces_x ? 2 : 0;
            const int depth_axis = faces_x ? 0 : 2;
            const auto closed_center = (door.closed_bounds.mins + door.closed_bounds.maxs) * .5F;
            for (const float side : {-1.0F, 1.0F})
            {
                auto rail_center = (sweep_min + sweep_max) * .5F;
                rail_center[depth_axis] = side < 0 ? sweep_min[depth_axis] - .045F : sweep_max[depth_axis] + .045F;
                auto rail_size = glm::vec3{.06F};
                if (lift)
                {
                    rail_size.y = sweep_max.y - sweep_min.y + .08F;
                    for (const float edge : {-1.0F, 1.0F})
                    {
                        rail_center[width_axis] = closed_center[width_axis] + edge * (size[width_axis] * .5F + .035F);
                        frame_box(rail_center, rail_size);
                    }
                } else
                {
                    rail_size[width_axis] = sweep_max[width_axis] - sweep_min[width_axis] + .16F;
                    rail_size.y = .10F;
                    rail_center.y = sweep_max.y + .055F;
                    frame_box(rail_center, rail_size);
                }
                auto drive_center = rail_center;
                drive_center[width_axis] = (sweep_min[width_axis] + sweep_max[width_axis]) * .5F;
                drive_center.y = door.closed_bounds.maxs.y + .14F;
                auto drive_size = glm::vec3{.12F};
                drive_size[width_axis] = .48F;
                drive_size[depth_axis] = .07F;
                frame_box(drive_center, drive_size);
            }
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
            if ((!usable.enabled
                 && !(usable.consumed && usable.visual_kind == RelayCore::gameplay::UsableVisualKind::Panel))
                || !usable.has_visual_bounds
                || (usable.visual_kind != RelayCore::gameplay::UsableVisualKind::Panel
                    && usable.visual_kind != RelayCore::gameplay::UsableVisualKind::Pickup))
            {
                continue;
            }

            using RelayCore::render::RenderPass;
            const bool camera_visible =
                aabb_intersects_planes(frustum_planes_, usable.visual_bounds.mins, usable.visual_bounds.maxs);
            const bool focused = static_cast<int>(usable_index) == focused_usable_index;
            if (usable.visual_kind == RelayCore::gameplay::UsableVisualKind::Panel)
            {
                using RelayCore::gameplay::UsableFeedbackStyle;
                const auto normal = panel_plane_normal(usable, camera);
                const auto size = usable.visual_bounds.maxs - usable.visual_bounds.mins;
                const glm::vec3 up = std::abs(normal.y) > 0.5F ? glm::vec3{0, 0, 1} : glm::vec3{0, 1, 0};
                const auto right = glm::cross(up, normal);
                const auto axis = thinnest_panel_axis(usable.visual_bounds);
                glm::mat4 model{1.0F};
                model[0] = glm::vec4{right * (axis == PanelPlaneAxis::X ? size.z : size.x), 0};
                model[1] = glm::vec4{glm::cross(normal, right) * (axis == PanelPlaneAxis::Y ? size.z : size.y), 0};
                model[2] = glm::vec4{normal, 0};
                model[3] = glm::vec4{(usable.visual_bounds.mins + usable.visual_bounds.maxs) * 0.5F, 1};
                const bool feedback = usable.feedback_remaining_seconds > 0.0F;
                const bool success =
                    usable.consumed || (feedback && usable.feedback_style == UsableFeedbackStyle::Success);
                const bool warning = feedback && usable.feedback_style == UsableFeedbackStyle::Warning;
                const auto& actuator = success                                      ? panel_success_mesh_
                                       : warning                                    ? panel_warning_mesh_
                                       : (usable.locked || !usable.requirement_met) ? panel_blocked_mesh_
                                       : focused                                    ? panel_highlight_mesh_
                                                                                    : panel_mesh_;
                const auto presentation =
                    usable.authored_entity_index >= 0
                            && static_cast<std::size_t>(usable.authored_entity_index) < panel_presentations_.size()
                        ? panel_presentations_[static_cast<std::size_t>(usable.authored_entity_index)]
                        : PanelPresentation::Door;
                const auto& housing = *panel_models_[static_cast<std::size_t>(presentation)];
                queue_interactive_model(housing, model, camera_visible);
                if (!camera_visible)
                {
                    shadow_casters_.push(actuator, model);
                    continue;
                }
                ++diagnostics_.usable_draw_calls;
                record_render_draw(RenderPass::Opaque, mesh_triangle_count(actuator));
                render_queue_.push(actuator, model, RenderPass::Opaque);
                if (feedback)
                {
                    ++diagnostics_.feedback_draw_calls;
                }
                if (focused)
                {
                    ++diagnostics_.highlight_draw_calls;
                }
            } else
            {
                auto bounds = expanded_pickup_visual_bounds(usable.visual_bounds);
                glm::mat4 pickup_model = make_aabb_model_matrix(bounds);
                const bool is_power_fuse = usable.item_id == "power_fuse";
                const bool is_shotgun = usable.item_id == shotgun_item_id;
                const bool is_service_key = usable.item_id == "service_key";
                const auto entity_index = static_cast<std::size_t>(usable.authored_entity_index);
                const auto surface_y = entity_index < pickup_surfaces_.size()
                                           ? pickup_surfaces_[entity_index].value_or(bounds.mins.y)
                                           : bounds.mins.y;
                if (is_shotgun)
                {
                    const auto center = (shotgun_world_model_->mins + shotgun_world_model_->maxs) * .5F;
                    auto rotation = glm::rotate(glm::mat4{1}, glm::half_pi<float>(), glm::vec3{0, 1, 0});
                    rotation = glm::rotate(rotation, glm::half_pi<float>(), glm::vec3{0, 0, 1});
                    const auto half = (shotgun_world_model_->maxs - shotgun_world_model_->mins) * .5F;
                    const auto rotated_half = glm::abs(glm::vec3{rotation[0]}) * half.x
                                              + glm::abs(glm::vec3{rotation[1]}) * half.y
                                              + glm::abs(glm::vec3{rotation[2]}) * half.z;
                    auto origin = (bounds.mins + bounds.maxs) * .5F;
                    origin.y = surface_y + rotated_half.y + .002F;
                    bounds = {origin - rotated_half, origin + rotated_half};
                    pickup_model = glm::translate(glm::mat4{1}, origin) * rotation;
                    pickup_model = glm::translate(pickup_model, -center);
                } else if (is_power_fuse)
                {
                    const auto size = bounds.maxs - bounds.mins;
                    auto origin = (bounds.mins + bounds.maxs) * .5F;
                    origin.y = surface_y + .002F - power_fuse_model_->mins.y * size.y;
                    pickup_model = glm::scale(glm::translate(glm::mat4{1}, origin), size);
                    bounds = {origin + power_fuse_model_->mins * size, origin + power_fuse_model_->maxs * size};
                }
                if (!camera_visible)
                {
                    if (is_service_key)
                    {
                        for (const auto& part : service_key_model_->parts)
                            shadow_casters_.push(service_key_model_->meshes[part.mesh], pickup_model * part.transform);
                    } else if (is_shotgun)
                    {
                        queue_interactive_model(*shotgun_world_model_, pickup_model, false);
                    } else if (is_power_fuse)
                        shadow_casters_.push(power_fuse_model_->mesh, pickup_model);
                    else
                        shadow_casters_.push(pickup_mesh_, pickup_model);
                    continue;
                }
                ++diagnostics_.usable_draw_calls;
                if (is_service_key)
                {
                    queue_interactive_model(*service_key_model_, pickup_model, true);
                } else if (is_shotgun)
                {
                    queue_interactive_model(*shotgun_world_model_, pickup_model, true);
                } else if (is_power_fuse)
                {
                    record_render_draw(
                        RenderPass::Opaque,
                        static_cast<std::uint32_t>(power_fuse_model_->mesh.index_count() / 3)
                    );
                    render_queue_
                        .push(power_fuse_model_->mesh, pickup_model, RenderPass::Opaque, &power_fuse_model_->texture);
                } else
                {
                    record_render_draw(RenderPass::Opaque, mesh_triangle_count(pickup_mesh_));
                    render_queue_.push(pickup_mesh_, pickup_model, RenderPass::Opaque);
                }
                if (focused && highlight_visible)
                {
                    if (is_service_key || is_shotgun || is_power_fuse)
                    {
                        const auto frame =
                            expand_aabb(bounds, {0.02F + focus_pulse * 0.01F, 0, 0.02F + focus_pulse * 0.01F});
                        const auto frame_size = frame.maxs - frame.mins;
                        const auto center = (frame.mins + frame.maxs) * .5F;
                        for (const int axis : {0, 2})
                            for (const float side : {-1.0F, 1.0F})
                            {
                                auto bar_center = center;
                                bar_center.y = bounds.mins.y + .012F;
                                bar_center[axis] += side * frame_size[axis] * .5F;
                                auto bar_size = frame_size;
                                bar_size.y = .008F;
                                bar_size[axis] = .008F;
                                ++diagnostics_.highlight_draw_calls;
                                record_render_draw(RenderPass::Overlay, mesh_triangle_count(pickup_highlight_mesh_));
                                render_queue_.push(
                                    pickup_highlight_mesh_,
                                    glm::scale(glm::translate(glm::mat4{1}, bar_center), bar_size),
                                    RenderPass::Overlay
                                );
                            }
                        continue;
                    }
                    ++diagnostics_.highlight_draw_calls;
                    record_render_draw(RenderPass::Overlay, mesh_triangle_count(pickup_highlight_mesh_));
                    render_queue_.push(
                        pickup_highlight_mesh_,
                        make_aabb_model_matrix(expand_aabb(bounds, glm::vec3{0.02F + focus_pulse * 0.01F})),
                        RenderPass::Overlay
                    );
                }
            }
        }
    }
    void WorldSceneRenderer::render_turrets(
        const RelayCore::render::PerspectiveCamera& camera,
        const std::span<const RelayCore::gameplay::RuntimeTurret> turrets
    )
    {
        RELAYCORE_PROFILE_FUNCTION();
        for (const RelayCore::gameplay::RuntimeTurret& turret : turrets)
        {
            const glm::vec3 size = turret.bounds.maxs - turret.bounds.mins;
            const auto& turret_model = turret.destroyed ? destroyed_turret_model_ : turret_model_;
            auto model = glm::translate(glm::mat4{1.0F}, turret.origin);
            model = glm::rotate(model, glm::radians(turret.yaw), glm::vec3{0, 1, 0});
            model = glm::scale(model, size);
            if (!aabb_intersects_planes(frustum_planes_, turret.bounds.mins, turret.bounds.maxs))
            {
                shadow_casters_.push(turret_model->mesh, model);
                continue;
            }

            record_render_draw(
                RelayCore::render::RenderPass::Opaque,
                static_cast<std::uint32_t>(turret_model->mesh.index_count() / 3)
            );
            render_queue_
                .push(turret_model->mesh, model, RelayCore::render::RenderPass::Opaque, &turret_model->texture);
            if (turret.enabled && !turret.destroyed && turret.warming_up
                && std::sin(turret.warmup_remaining_seconds * 24.0F) >= 0.0F)
            {
                record_render_draw(RelayCore::render::RenderPass::Opaque, mesh_triangle_count(turret_warning_mesh_));
                render_queue_.push(turret_warning_mesh_, model, RelayCore::render::RenderPass::Opaque);
            }
            if (turret.destroyed && turret.hit_flash_remaining_seconds > 0.0F)
            {
                const float yaw = glm::radians(turret.yaw);
                const glm::vec3 head =
                    turret.origin
                    + glm::vec3{std::sin(yaw) * size.z * 0.42F, -size.y * 0.25F, std::cos(yaw) * size.z * 0.42F};
                const float remaining = std::min(turret.hit_flash_remaining_seconds, 0.25F);
                render_shot_trace(camera, {head, head + glm::vec3{-0.35F, 0.30F, 0.10F}, remaining}, true);
                render_shot_trace(camera, {head, head + glm::vec3{0.30F, 0.15F, 0.30F}, remaining}, true);
                render_shot_trace(camera, {head, head + glm::vec3{0.12F, -0.28F, -0.25F}, remaining}, true);
            }
        }
    }

    void WorldSceneRenderer::render_shot_trace(
        const RelayCore::render::PerspectiveCamera& camera,
        const RelayCore::gameplay::CombatShotTrace& trace,
        const bool hostile,
        const bool pellet
    )
    {
        if (trace.remaining_seconds <= 0.0F)
            return;
        const glm::vec3 visual_start =
            hostile ? trace.start
                    : trace.start + camera.right() * 0.22F - camera.up() * 0.17F + camera.forward() * 0.32F;
        const glm::vec3 delta = trace.end - visual_start;
        const float distance = glm::length(delta);
        if (distance <= 0.05F)
            return;

        const glm::vec3 forward = delta / distance;
        const float progress = glm::clamp(1.0F - trace.remaining_seconds / (pellet ? .06F : .25F), 0.0F, 1.0F);
        const float front = std::max(progress, pellet ? .18F : .08F);
        const float back = std::max(0.0F, front - 0.18F);
        const float end_distance = hostile ? distance : front * distance;
        const float start_distance =
            hostile ? 0.0F : (pellet ? std::max(.05F, end_distance - .08F) : std::max(0.05F, back * distance));
        if (end_distance <= start_distance)
            return;

        const glm::vec3 up = std::abs(forward.y) > 0.95F ? glm::vec3{0, 0, 1} : glm::vec3{0, 1, 0};
        const glm::vec3 right = glm::normalize(glm::cross(up, forward));
        const glm::vec3 normal = glm::cross(forward, right);
        glm::mat4 model{1.0F};
        const float width =
            hostile ? 0.018F * glm::clamp(trace.remaining_seconds / 0.08F, 0.0F, 1.0F) : (pellet ? 0.008F : 0.045F);
        model[0] = glm::vec4{right * width, 0.0F};
        model[1] = glm::vec4{normal * width, 0.0F};
        model[2] = glm::vec4{forward * (end_distance - start_distance), 0.0F};
        model[3] = glm::vec4{visual_start + forward * ((start_distance + end_distance) * 0.5F), 1.0F};
        const auto& mesh = hostile ? turret_shot_mesh_ : (pellet ? pellet_mesh_ : player_shot_mesh_);
        record_render_draw(RelayCore::render::RenderPass::Overlay, mesh_triangle_count(mesh));
        render_queue_.push(mesh, model, RelayCore::render::RenderPass::Overlay, nullptr, false);
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
