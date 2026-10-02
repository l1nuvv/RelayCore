#include "relaycore/render/shadow_math.h"

#include "relaycore/core/engine_config.h"
#include "relaycore/render/camera.h"
#include "relaycore/render/render_queue.h"

#include <algorithm>
#include <cmath>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>

namespace RelayCore::render
{
    auto shadow_frustum_planes(const glm::mat4& view_projection) -> std::array<glm::vec4, 6>
    {
        const glm::vec4
            row0{view_projection[0][0], view_projection[1][0], view_projection[2][0], view_projection[3][0]};
        const glm::vec4
            row1{view_projection[0][1], view_projection[1][1], view_projection[2][1], view_projection[3][1]};
        const glm::vec4
            row2{view_projection[0][2], view_projection[1][2], view_projection[2][2], view_projection[3][2]};
        const glm::vec4
            row3{view_projection[0][3], view_projection[1][3], view_projection[2][3], view_projection[3][3]};
        return {row3 + row0, row3 - row0, row3 + row1, row3 - row1, row3 + row2, row3 - row2};
    }

    auto sphere_intersects_shadow_frustum(
        const std::array<glm::vec4, 6>& planes,
        const glm::vec3& center,
        const float radius
    ) -> bool
    {
        for (const glm::vec4& plane : planes)
        {
            const glm::vec3 normal{plane};
            if (glm::dot(normal, center) + plane.w < -radius * glm::length(normal))
                return false;
        }
        return true;
    }

    auto choose_shadow_up(const glm::vec3& light_direction) -> glm::vec3
    {
        const glm::vec3 world_up{0.0F, 1.0F, 0.0F};
        if (std::abs(glm::dot(light_direction, world_up)) > 0.98F)
        {
            return {0.0F, 0.0F, 1.0F};
        }
        return world_up;
    }

    auto round_to_shadow_texel(const float value, const float texel_world_size) -> float
    {
        if (texel_world_size <= 0.000001F)
        {
            return value;
        }

        return std::round(value / texel_world_size) * texel_world_size;
    }

    auto safe_shadow_direction(const glm::vec3& direction) -> glm::vec3
    {
        const float length = glm::length(direction);
        if (length <= 0.0001F)
        {
            return glm::normalize(glm::vec3{0.28F, -1.0F, 0.22F});
        }

        return direction / length;
    }

    auto shadow_light_view_projection(
        const PerspectiveCamera& camera,
        const RendererConfig& config,
        float focus_distance,
        float radius,
        float depth
    ) -> glm::mat4
    {
        const glm::vec3 light_direction = safe_shadow_direction(config.shadow_light_direction);
        const float cascade_radius = std::max(radius, 1.0F);
        const float cascade_depth = std::max(depth, cascade_radius + 1.0F);
        const float texel_world_size =
            (cascade_radius * 2.0F) / static_cast<float>(std::max(config.shadow_map_resolution, 256));
        const glm::vec3 focus_point = camera.position() + (camera.forward() * std::max(focus_distance, 0.0F));
        const glm::mat4 light_rotation =
            glm::lookAt(glm::vec3{0.0F}, light_direction, choose_shadow_up(light_direction));
        const glm::vec4 focus_in_light_space = light_rotation * glm::vec4(focus_point, 1.0F);
        const float snapped_x = round_to_shadow_texel(focus_in_light_space.x, texel_world_size);
        const float snapped_y = round_to_shadow_texel(focus_in_light_space.y, texel_world_size);
        // Keep the light basis fixed; only the snapped focus may move across shadow texels.
        const glm::mat4 light_view =
            glm::translate(
                glm::mat4{1.0F},
                glm::vec3{-snapped_x, -snapped_y, -cascade_depth * 0.5F - focus_in_light_space.z}
            )
            * light_rotation;
        const glm::mat4 light_projection =
            glm::ortho(-cascade_radius, cascade_radius, -cascade_radius, cascade_radius, 0.5F, cascade_depth);
        return light_projection * light_view;
    }

    auto spot_shadow_view_projection(const SpotLight& light) -> glm::mat4
    {
        return glm::perspective(glm::radians(52.0F), 1.0F, .06F, light.range)
               * glm::lookAt(light.position, light.position + light.direction, choose_shadow_up(light.direction));
    }
    auto point_shadow_view_projection(const LocalLight& light, const int face) -> glm::mat4
    {
        constexpr glm::vec3 directions[6] = {{1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}};
        constexpr glm::vec3 ups[6] = {{0, -1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}, {0, -1, 0}, {0, -1, 0}};
        return glm::perspective(glm::radians(90.0F), 1.0F, .05F, light.radius)
               * glm::lookAt(light.position, light.position + directions[face], ups[face]);
    }
    auto shadow_caster_sphere(const RenderItem& item) -> glm::vec4
    {
        const glm::vec3 center = (item.mesh->bounds_min() + item.mesh->bounds_max()) * .5F;
        const glm::vec3 extent = (item.mesh->bounds_max() - item.mesh->bounds_min()) * .5F;
        const glm::vec3 world_center = glm::vec3(item.model_matrix * glm::vec4(center, 1));
        const float radius = glm::length(glm::vec3(item.model_matrix[0])) * extent.x
                             + glm::length(glm::vec3(item.model_matrix[1])) * extent.y
                             + glm::length(glm::vec3(item.model_matrix[2])) * extent.z;
        return {world_center, radius};
    }
} // namespace RelayCore::render
