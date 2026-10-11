#include "relaycore/render/authored_color.h"
#include "relaygame/world_scene_renderer.h"

#include <cmath>
#include <glm/geometric.hpp>
#include <glm/gtc/constants.hpp>

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
            bool open_coat = false,
            int sides = 12
        )
        {
            const auto point = [open_coat, sides](const Ring& ring, float side) {
                const float gap =
                    open_coat ? glm::mix(0.86F, 0.04F, glm::clamp((ring.height - 0.64F) / 0.62F, 0.0F, 1.0F)) : 0.0F;
                const float angle = gap + (glm::two_pi<float>() - gap * 2.0F) * side / sides;
                return glm::vec3{
                    std::sin(angle) * ring.radius.x,
                    ring.height,
                    std::cos(angle) * ring.radius.y + ring.offset_z
                };
            };
            const auto inner = [](Ring ring) {
                ring.radius -= glm::vec2{.003F};
                return ring;
            };
            const auto strip = [&](const Ring& lower, const Ring& upper, int side, bool inside) {
                const auto a = point(lower, float(side)), b = point(lower, float(side + 1));
                const auto c = point(upper, float(side + 1)), d = point(upper, float(side));
                const auto first = mesh.vertices.size();
                const auto tint = inside ? color * .94F : color;
                if (inside)
                {
                    triangle(mesh, c, b, a, tint, material);
                    triangle(mesh, d, c, a, tint, material);
                } else
                {
                    triangle(mesh, a, b, c, tint, material);
                    triangle(mesh, a, c, d, tint, material);
                }
                if (sides > 12)
                    for (auto index = first; index < mesh.vertices.size(); ++index)
                    {
                        auto& vertex = mesh.vertices[index];
                        const bool at_top = vertex.position.y == upper.height;
                        const bool at_right = vertex.position == (at_top ? c : b);
                        const auto& ring = at_top ? upper : lower;
                        const float s = float(side + (at_right ? 1 : 0));
                        const auto tangent = point(ring, s + .01F) - point(ring, s - .01F);
                        const auto vertical = point(upper, s) - point(lower, s);
                        vertex.normal = glm::normalize(glm::cross(tangent, vertical)) * (inside ? -1.F : 1.F);
                    }
            };
            for (auto lower = rings.begin(); lower + 1 != rings.end(); ++lower)
            {
                for (int side = 0; side < sides; ++side)
                {
                    strip(*lower, *(lower + 1), side, false);
                    if (open_coat)
                        strip(inner(*lower), inner(*(lower + 1)), side, true);
                }
                if (open_coat)
                    for (const int side : {0, sides})
                    {
                        const auto a = point(*lower, float(side)), b = point(*(lower + 1), float(side));
                        const auto c = point(inner(*(lower + 1)), float(side)), d = point(inner(*lower), float(side));
                        if (side == 0)
                        {
                            triangle(mesh, a, b, c, color * .9F, material);
                            triangle(mesh, a, c, d, color * .9F, material);
                        } else
                        {
                            triangle(mesh, c, b, a, color * .9F, material);
                            triangle(mesh, d, c, a, color * .9F, material);
                        }
                    }
            }
            if (open_coat)
            {
                for (const auto* ring : {rings.begin(), rings.end() - 1})
                    for (int side = 0; side < sides; ++side)
                    {
                        const auto a = point(*ring, float(side)), b = point(*ring, float(side + 1));
                        const auto c = point(inner(*ring), float(side + 1)), d = point(inner(*ring), float(side));
                        if (ring == rings.begin())
                        {
                            triangle(mesh, c, b, a, color * .9F, material);
                            triangle(mesh, d, c, a, color * .9F, material);
                        } else
                        {
                            triangle(mesh, a, b, c, color * .9F, material);
                            triangle(mesh, a, c, d, color * .9F, material);
                        }
                    }
            } else
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
        const auto upload = [this, pipeline](RelayCore::render::StaticColorMesh& mesh, CpuColorMeshData data) {
            for (auto& vertex : data.vertices)
            {
                vertex.color = RelayCore::render::visual_rgb_for_pipeline(vertex.color, pipeline);
            }
            mesh.initialize(data);
        };
        const auto actuator =
            [&](RelayCore::render::StaticColorMesh& mesh, glm::vec3 color, int lamp, bool pressed = false) {
                CpuColorMeshData data;
                disk(data, {0, -0.11F, pressed ? 0.069F : 0.086F}, 0.22F, 0.033F, color);
                for (auto& vertex : data.vertices)
                    vertex.material_kind = static_cast<float>(RelayCore::SurfaceMaterial::Matte);
                // Fixed left/centre/right positions also distinguish blocked, ready and operated states.
                for (int slot = 0; slot < 3; ++slot)
                {
                    const auto first = data.vertices.size();
                    const bool lit = slot == lamp;
                    box(data,
                        {(slot - 1) * .115F, .30F, .066F},
                        {.095F, .055F, .009F},
                        lit ? color : glm::vec3{.055F, .065F, .06F});
                    for (auto i = first; i < data.vertices.size(); ++i)
                        data.vertices[i].material_kind = static_cast<float>(
                            lit ? RelayCore::SurfaceMaterial::LightStrip : RelayCore::SurfaceMaterial::Matte
                        );
                }
                upload(mesh, std::move(data));
            };
        actuator(panel_mesh_, {0.78F, 0.49F, 0.13F}, 1);
        actuator(panel_blocked_mesh_, {0.46F, 0.14F, 0.09F}, 0);
        actuator(panel_highlight_mesh_, {0.98F, 0.67F, 0.22F}, 1);
        actuator(panel_success_mesh_, {0.26F, 0.70F, 0.38F}, 2, true);
        actuator(panel_warning_mesh_, {0.86F, 0.22F, 0.10F}, 0);
        CpuColorMeshData turret_warning;
        // Cover the existing sensor lens, keeping the warning attached to the housing.
        disk(turret_warning, {-0.25F, 0.24F, 0.396F}, 0.058F, 0.002F, {1.0F, 0.82F, 0.24F});
        for (auto& vertex : turret_warning.vertices)
            vertex.material_kind = static_cast<float>(RelayCore::SurfaceMaterial::LightStrip);
        upload(turret_warning_mesh_, std::move(turret_warning));
        for (std::size_t index = 0; index < rover_signal_meshes_.size(); ++index)
        {
            CpuColorMeshData signal;
            for (const float x : {-.092F, .092F})
                disk(
                    signal,
                    {x, .83F, .215F},
                    .041F,
                    .002F,
                    index == 0 ? glm::vec3{.90F, .16F, .07F} : glm::vec3{1.0F, .65F, .12F}
                );
            for (auto& vertex : signal.vertices)
                vertex.material_kind = static_cast<float>(RelayCore::SurfaceMaterial::LightStrip);
            upload(rover_signal_meshes_[index], std::move(signal));
        }
        CpuColorMeshData thigh;
        loft(
            thigh,
            {{-.43F, {.045F, .049F}},
             {-.40F, {.070F, .075F}},
             {-.35F, {.078F, .084F}},
             {-.20F, {.088F, .096F}},
             {-.02F, {.096F, .104F}},
             {.075F, {.091F, .095F}}},
            {1.0F, 1.0F, 1.0F},
            static_cast<float>(RelayCore::SurfaceMaterial::Twill),
            false,
            24
        );
        upload(scientist_thigh_mesh_, std::move(thigh));
        CpuColorMeshData shin;
        loft(
            shin,
            {{-.35F, {.064F, .068F}},
             {-.33F, {.069F, .073F}},
             {-.20F, {.074F, .084F}, -.008F},
             {-.06F, {.070F, .076F}},
             {.035F, {.062F, .066F}}},
            {1.0F, 1.0F, 1.0F},
            static_cast<float>(RelayCore::SurfaceMaterial::Twill),
            false,
            24
        );
        upload(scientist_shin_mesh_, std::move(shin));
        CpuColorMeshData shoe;
        loft(
            shoe,
            {{-.10F, {.074F, .141F}, .052F}, {-.089F, {.082F, .151F}, .052F}, {-.065F, {.081F, .150F}, .052F}},
            {.65F, .65F, .65F},
            static_cast<float>(RelayCore::SurfaceMaterial::Rubber),
            false,
            24
        );
        loft(
            shoe,
            {{-.064F, {.078F, .145F}, .052F},
             {-.015F, {.076F, .140F}, .048F},
             {.025F, {.071F, .117F}, .031F},
             {.075F, {.064F, .078F}, -.003F},
             {.12F, {.063F, .070F}, -.010F}},
            {.70F, .70F, .70F},
            static_cast<float>(RelayCore::SurfaceMaterial::Leather),
            false,
            24
        );
        for (const float z : {.07F, .09F, .11F, .13F})
        {
            box(shoe,
                {0, .083F - (z - .07F) * .68F, z},
                {.05F, .004F, .006F},
                {0.2F, 0.2F, 0.2F},
                RelayCore::SurfaceMaterial::Cotton);
        }
        upload(scientist_shoe_mesh_, std::move(shoe));
        CpuColorMeshData coat;

        loft(
            coat,
            {{.64F, {.29F, .24F}, -.04F},
             {.657F, {.289F, .239F}, -.04F},
             {.84F, {.264F, .211F}, -.04F},
             {1.06F, {.232F, .174F}, -.045F},
             {1.38F, {.205F, .15F}, -.055F},
             {1.56F, {.205F, .15F}, -.055F}},
            {1.0F, 1.0F, 1.0F},
            static_cast<float>(RelayCore::SurfaceMaterial::Cotton),
            true,
            24
        );
        for (auto& vertex : coat.vertices)
            if (vertex.position.y <= .657F)
                vertex.color *= .88F;
        loft(
            coat,
            {{.84F, {.207F, .14F}, -.09F}, {.96F, {.211F, .13F}, -.09F}, {1.56F, {.19F, .12F}, -.09F}},
            {1, 1, 1},
            static_cast<float>(RelayCore::SurfaceMaterial::Twill),
            false,
            24
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
        const auto body = build_scientist_body_transforms(pose);
        for (std::size_t leg = 0; leg < 2; ++leg)
        {
            draw(scientist_thigh_mesh_, body.thighs[leg]);
            draw(scientist_shin_mesh_, body.shins[leg]);
            draw(scientist_shoe_mesh_, body.shoes[leg]);
        }

        draw(scientist_coat_mesh_, body.coat);
    }
} // namespace RelayGame
