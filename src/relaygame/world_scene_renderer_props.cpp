#include "relaycore/render/authored_color.h"
#include "relaygame/world_scene_renderer.h"

#include <cmath>
#include <glm/geometric.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace RelayGame
{
    namespace
    {
        using RelayCore::render::CpuColorMeshData;
        struct Ring {
            float height;
            glm::vec2 radius;
            float offset_z = 0.0F;
        };

        void triangle(CpuColorMeshData& mesh, glm::vec3 a, glm::vec3 b, glm::vec3 c, glm::vec3 color, float material)
        {
            const auto first = static_cast<std::uint32_t>(mesh.vertices.size());
            const auto normal = glm::normalize(glm::cross(b - a, c - a));
            for (const auto position : {a, b, c})
            {
                mesh.vertices.push_back({position, color, normal, material});
            }
            mesh.indices.insert(mesh.indices.end(), {first, first + 1, first + 2});
        }

        void loft(
            CpuColorMeshData& mesh,
            std::initializer_list<Ring> rings,
            glm::vec3 color,
            float material = static_cast<float>(RelayCore::SurfaceMaterial::Matte),
            bool open_coat = false
        )
        {
            constexpr int sides = 12;

            const auto point = [open_coat](const Ring& ring, int side) {
                const float gap =
                    open_coat ? glm::mix(0.90F, 0.04F, glm::clamp((ring.height - 0.59F) / 0.66F, 0.0F, 1.0F)) : 0.0F;
                const float angle = gap + (glm::two_pi<float>() - gap * 2.0F) * static_cast<float>(side) / sides;
                return glm::vec3{
                    std::sin(angle) * ring.radius.x,
                    ring.height,
                    std::cos(angle) * ring.radius.y + ring.offset_z
                };
            };
            for (auto lower = rings.begin(); lower + 1 != rings.end(); ++lower)
            {
                for (int side = 0; side < sides; ++side)
                {
                    const auto a = point(*lower, side), b = point(*lower, side + 1);
                    const auto c = point(*(lower + 1), side + 1), d = point(*(lower + 1), side);
                    triangle(mesh, a, b, c, color, material);
                    triangle(mesh, a, c, d, color, material);
                    if (open_coat)
                    {
                        triangle(mesh, c, b, a, color * 0.8F, material);
                        triangle(mesh, d, c, a, color * 0.8F, material);
                    }
                }
            }
            if (!open_coat)
            {
                const auto& bottom = *rings.begin();
                const auto& top = *(rings.end() - 1);
                for (int side = 0; side < sides; ++side)
                {
                    triangle(
                        mesh,
                        {0.0F, bottom.height, bottom.offset_z},
                        point(bottom, side + 1),
                        point(bottom, side),
                        color,
                        material
                    );
                    triangle(
                        mesh,
                        {0.0F, top.height, top.offset_z},
                        point(top, side),
                        point(top, side + 1),
                        color,
                        material
                    );
                }
            }
        }

        void
        box(CpuColorMeshData& mesh,
            glm::vec3 center,
            glm::vec3 size,
            glm::vec3 color,
            RelayCore::SurfaceMaterial material = RelayCore::SurfaceMaterial::Metal)
        {
            auto part = RelayCore::render::make_color_box_mesh_data();
            const auto first = static_cast<std::uint32_t>(mesh.vertices.size());
            for (auto vertex : part.vertices)
            {
                vertex.position = center + vertex.position * size;
                vertex.color = color;
                vertex.material_kind = static_cast<float>(material);
                mesh.vertices.push_back(vertex);
            }
            for (const auto index : part.indices)
            {
                mesh.indices.push_back(first + index);
            }
        }

        void disk(CpuColorMeshData& mesh, glm::vec3 center, float radius, float depth, glm::vec3 color)
        {
            CpuColorMeshData part;
            loft(
                part,
                {{0.0F, {radius, radius}}, {depth * 0.7F, {radius, radius}}, {depth, {radius * 0.88F, radius * 0.88F}}},
                color,
                1.0F
            );
            const auto first = static_cast<std::uint32_t>(mesh.vertices.size());
            for (auto vertex : part.vertices)
            {
                vertex.position = center + glm::vec3{vertex.position.x, -vertex.position.z * 0.74F, vertex.position.y};
                vertex.normal = glm::normalize(glm::vec3{vertex.normal.x, -vertex.normal.z / 0.74F, vertex.normal.y});
                mesh.vertices.push_back(vertex);
            }
            for (const auto index : part.indices)
            {
                mesh.indices.push_back(first + index);
            }
        }
    } // namespace

    void WorldSceneRenderer::initialize_prop_meshes(const RelayCore::render::ColorPipelineMode pipeline)
    {
        const auto upload = [pipeline](RelayCore::render::StaticColorMesh& mesh, CpuColorMeshData data) {
            for (auto& vertex : data.vertices)
            {
                vertex.color = RelayCore::render::visual_rgb_for_pipeline(vertex.color, pipeline);
            }
            mesh.initialize(data);
        };
        CpuColorMeshData housing;
        box(housing, {0, 0, -0.025F}, {1, 1, 0.10F}, {0.23F, 0.26F, 0.24F});
        box(housing, {0, 0, 0.033F}, {0.87F, 0.90F, 0.025F}, {0.49F, 0.51F, 0.46F});
        disk(housing, {0, -0.11F, 0.049F}, 0.30F, 0.025F, {0.12F, 0.14F, 0.13F});
        disk(housing, {0, -0.11F, 0.074F}, 0.255F, 0.014F, {0.64F, 0.62F, 0.53F});
        box(housing, {0, 0.30F, 0.05F}, {0.44F, 0.13F, 0.02F}, {0.09F, 0.11F, 0.10F});
        for (const float x : {-0.36F, 0.36F})
        {
            for (const float y : {-0.39F, 0.39F})
            {
                disk(housing, {x, y, 0.05F}, 0.034F, 0.008F, {0.65F, 0.65F, 0.60F});
                box(housing, {x, y, 0.06F}, {0.042F, 0.009F, 0.004F}, {0.12F, 0.13F, 0.12F});
            }
        }
        upload(panel_housing_mesh_, std::move(housing));
        const auto actuator = [&](RelayCore::render::StaticColorMesh& mesh, glm::vec3 color, bool pressed = false) {
            CpuColorMeshData data;
            disk(data, {0, -0.11F, pressed ? 0.069F : 0.086F}, 0.22F, 0.033F, color);
            box(data, {0, 0.30F, 0.066F}, {0.32F, 0.055F, 0.009F}, color * 1.25F);
            for (auto& vertex : data.vertices)
            {
                vertex.material_kind = static_cast<float>(RelayCore::SurfaceMaterial::Matte);
            }
            upload(mesh, std::move(data));
        };
        actuator(panel_mesh_, {0.78F, 0.49F, 0.13F});
        actuator(panel_blocked_mesh_, {0.46F, 0.14F, 0.09F});
        actuator(panel_highlight_mesh_, {0.98F, 0.67F, 0.22F});
        actuator(panel_success_mesh_, {0.26F, 0.70F, 0.38F}, true);
        actuator(panel_warning_mesh_, {0.86F, 0.22F, 0.10F});
        CpuColorMeshData thigh;
        loft(
            thigh,
            {{-0.40F, {0.073F, 0.077F}}, {-0.22F, {0.091F, 0.098F}}, {0.12F, {0.105F, 0.112F}, -0.18F}},
            {1.0F, 1.0F, 1.0F},
            static_cast<float>(RelayCore::SurfaceMaterial::Twill)
        );
        upload(scientist_thigh_mesh_, std::move(thigh));
        CpuColorMeshData shin;
        loft(
            shin,
            {{-0.37F, {0.069F, 0.070F}},
             {-0.32F, {0.076F, 0.077F}},
             {-0.17F, {0.078F, 0.089F}},
             {0.02F, {0.074F, 0.078F}}},
            {1.0F, 1.0F, 1.0F},
            static_cast<float>(RelayCore::SurfaceMaterial::Twill)
        );
        upload(scientist_shin_mesh_, std::move(shin));
        CpuColorMeshData shoe;
        loft(
            shoe,
            {{-0.10F, {0.084F, 0.17F}, 0.06F}, {-0.067F, {0.087F, 0.17F}, 0.06F}},
            {0.65F, 0.65F, 0.65F},
            static_cast<float>(RelayCore::SurfaceMaterial::Rubber)
        );
        loft(
            shoe,
            {{-0.066F, {0.084F, 0.166F}, 0.06F}, {-0.01F, {0.077F, 0.155F}, 0.057F}, {0.045F, {0.065F, 0.095F}, 0.005F}
            },
            {0.70F, 0.70F, 0.70F},
            static_cast<float>(RelayCore::SurfaceMaterial::Leather)
        );
        for (const float z : {0.055F, 0.075F, 0.095F})
        {
            box(shoe,
                {0, 0.028F - (z - 0.055F) * 0.25F, z},
                {0.085F, 0.009F, 0.007F},
                {0.2F, 0.2F, 0.2F},
                RelayCore::SurfaceMaterial::Cotton);
        }
        upload(scientist_shoe_mesh_, std::move(shoe));
        CpuColorMeshData coat;

        loft(
            coat,
            {{0.59F, {0.28F, 0.23F}, -0.05F},
             {0.62F, {0.283F, 0.232F}, -0.05F},
             {0.85F, {0.25F, 0.20F}, -0.05F},
             {1.62F, {0.225F, 0.17F}, -0.06F}},
            {1.0F, 1.0F, 1.0F},
            static_cast<float>(RelayCore::SurfaceMaterial::Cotton),
            true
        );

        upload(scientist_coat_mesh_, std::move(coat));
    }

    void WorldSceneRenderer::render_scientist(const ScientistPose& pose)
    {
        if (!pose.visible)
        {
            return;
        }
        const auto draw = [&](const RelayCore::render::StaticColorMesh& mesh, const glm::mat4& model) {
            record_render_draw(
                RelayCore::render::RenderPass::Opaque,
                static_cast<std::uint32_t>(mesh.index_count() / 3)
            );
            render_queue_.push(mesh, model, RelayCore::render::RenderPass::Opaque);
        };
        auto body = glm::translate(glm::mat4{1.0F}, pose.position);
        body = glm::rotate(body, std::atan2(pose.forward.x, pose.forward.z), glm::vec3{0, 1, 0});
        body = glm::translate(body, glm::vec3{0.0F, 0.0F, -0.12F});
        for (const float side : {-1.0F, 1.0F})
        {
            const float step = std::sin(pose.stride_phase) * side * pose.stride_amount;
            const float swing = step * 0.28F;
            const float bend = std::max(step, 0.0F) * 0.36F + pose.landing * 0.20F;
            auto hip = glm::translate(body, glm::vec3{side * 0.125F, 0.89F, -0.055F});
            hip = glm::rotate(hip, -swing, glm::vec3{1, 0, 0});
            draw(scientist_thigh_mesh_, hip);
            auto knee = glm::translate(hip, glm::vec3{0, -0.40F, 0});
            knee = glm::rotate(knee, bend, glm::vec3{1, 0, 0});
            draw(scientist_shin_mesh_, knee);
            auto ankle = glm::translate(knee, glm::vec3{0, -0.37F, 0});
            ankle = glm::rotate(ankle, swing - bend, glm::vec3{1, 0, 0});
            draw(scientist_shoe_mesh_, ankle);
        }

        draw(scientist_coat_mesh_, body);
    }
} // namespace RelayGame
