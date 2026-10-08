#include "relaycore/render/authored_color.h"
#include "relaycore/render/camera.h"
#include "relaycore/world/surface_material_utils.h"
#include "relaygame/world_scene_renderer.h"

#include <algorithm>
#include <cmath>
#include <glm/gtc/matrix_transform.hpp>

namespace RelayGame
{
    namespace
    {
        using RelayCore::render::CpuColorMeshData;
        using RelayCore::render::RenderPass;

        void face(CpuColorMeshData& mesh, glm::vec3 a, glm::vec3 b, glm::vec3 c, glm::vec3 color, float material)
        {
            const auto first = static_cast<std::uint32_t>(mesh.vertices.size());
            const auto normal = glm::normalize(glm::cross(b - a, c - a));
            for (const auto p : {a, b, c})
                mesh.vertices.push_back({p, color, normal, material});
            mesh.indices.insert(mesh.indices.end(), {first, first + 1, first + 2, first + 2, first + 1, first});
        }
        void rect(CpuColorMeshData& mesh, float x, float y, float w, float h, glm::vec3 color, float material = 22)
        {
            face(
                mesh,
                {x - w / 2, y - h / 2, 0},
                {x + w / 2, y - h / 2, 0},
                {x + w / 2, y + h / 2, 0},
                color,
                material
            );
            face(
                mesh,
                {x - w / 2, y - h / 2, 0},
                {x + w / 2, y + h / 2, 0},
                {x - w / 2, y + h / 2, 0},
                color,
                material
            );
        }
        auto effect_model(const EnvironmentEffect& effect) -> glm::mat4
        {
            auto model = glm::translate(glm::mat4{1}, effect.origin);
            model = glm::rotate(model, glm::radians(effect.angles.x), {0, 1, 0});
            model = glm::rotate(model, glm::radians(effect.angles.y), {1, 0, 0});
            model = glm::rotate(model, glm::radians(effect.angles.z), {0, 0, 1});
            return glm::scale(model, glm::vec3{effect.scale});
        }
    } // namespace

    void WorldSceneRenderer::initialize_environment_meshes(RelayCore::render::ColorPipelineMode pipeline)
    {
        const auto color = [this, pipeline](glm::vec3 rgb) {
            return RelayCore::render::visual_rgb_for_pipeline(rgb, pipeline);
        };
        CpuColorMeshData frame, rotor;
        const auto metal = color({0.38F, 0.42F, 0.39F});
        constexpr int sides = 32;
        for (int i = 0; i < sides; ++i)
        {
            const float a = static_cast<float>(i) * 6.2831853F / sides,
                        b = static_cast<float>(i + 1) * 6.2831853F / sides;
            const auto point = [](float angle, float radius, float z) {
                return glm::vec3{std::cos(angle) * radius, std::sin(angle) * radius, z};
            };
            face(frame, point(a, 0.62F, 0.08F), point(b, 0.62F, 0.08F), point(b, 0.52F, 0.08F), metal, 1);
            face(frame, point(a, 0.62F, 0.08F), point(b, 0.52F, 0.08F), point(a, 0.52F, 0.08F), metal, 1);
            face(frame, point(a, 0.52F, 0.08F), point(b, 0.52F, 0.08F), point(b, 0.52F, -0.12F), metal * 0.65F, 1);
            face(frame, point(a, 0.52F, 0.08F), point(b, 0.52F, -0.12F), point(a, 0.52F, -0.12F), metal * 0.65F, 1);
            face(rotor, {0, 0, 0.015F}, point(a, 0.12F, 0.015F), point(b, 0.12F, 0.015F), metal, 1);
            face(frame, {0, 0, -0.115F}, point(a, 0.52F, -0.115F), point(b, 0.52F, -0.115F), metal * 0.65F, 1);
        }
        for (int blade = 0; blade < 5; ++blade)
        {
            const float angle = static_cast<float>(blade) * 6.2831853F / 5;
            const auto p = [angle](float x, float y, float z) {
                return glm::vec3{
                    x * std::cos(angle) - y * std::sin(angle),
                    x * std::sin(angle) + y * std::cos(angle),
                    z
                };
            };
            face(rotor, p(0.07F, -0.04F, 0), p(0.46F, -0.16F, -0.03F), p(0.49F, 0.09F, 0.04F), metal, 1);
            face(rotor, p(0.07F, -0.04F, 0), p(0.49F, 0.09F, 0.04F), p(0.09F, 0.10F, 0.03F), metal, 1);
        }
        // Cross-brace sits in front of the rotor; ring and blades retain an open silhouette.
        const auto brace_start = frame.vertices.size();
        rect(frame, 0, 0, 1.05F, 0.025F, metal);
        rect(frame, 0, 0, 0.025F, 1.05F, metal);
        for (auto i = brace_start; i < frame.vertices.size(); ++i)
            frame.vertices[i].position.z = 0.065F;
        fan_frame_mesh_.initialize(frame);
        fan_rotor_mesh_.initialize(rotor);
        CpuColorMeshData steam, spark;
        const float steam_material = static_cast<float>(RelayCore::surface_material_count() + 1);
        const float emissive_material = steam_material + 1;
        rect(steam, 0, 0, 1, 1, color({0.65F, 0.68F, 0.66F}), steam_material);
        rect(spark, 0, 0, 0.012F, 0.12F, color({1.0F, 0.59F, 0.17F}), emissive_material);
        steam_mesh_.initialize(steam);
        spark_mesh_.initialize(spark);
        for (int i = 0; i < 16; ++i)
        {
            CpuColorMeshData lamp;
            const float value = 0.055F + static_cast<float>(i) / 15 * 0.88F;
            rect(lamp, 0, 0, 1, 1, color({value * 0.95F, value, value * 0.91F}), i == 0 ? 22 : emissive_material);
            lamp_meshes_[static_cast<std::size_t>(i)].initialize(lamp);
        }
        const auto paint = color({0.72F, 0.65F, 0.37F});
        for (std::size_t i = 0; i < decal_meshes_.size(); ++i)
        {
            CpuColorMeshData decal;
            if (i == 0)
            {
                rect(decal, -0.12F, 0, 0.64F, 0.14F, paint);
                face(decal, {0.16F, -0.29F, 0}, {0.49F, 0, 0}, {0.16F, 0.29F, 0}, paint, 22);
            } else if (i == 1)
            {
                const glm::vec3 a{-0.45F, -0.35F, 0}, b{0.45F, -0.35F, 0}, c{0, 0.43F, 0};
                for (const auto edge : {std::pair{a, b}, std::pair{b, c}, std::pair{c, a}})
                {
                    const auto inner_a = edge.first * 0.77F, inner_b = edge.second * 0.77F;
                    face(decal, edge.first, edge.second, inner_b, paint, 22);
                    face(decal, edge.first, inner_b, inner_a, paint, 22);
                }
                rect(decal, 0, 0.05F, 0.055F, 0.22F, paint);
                rect(decal, 0, -0.17F, 0.06F, 0.06F, paint);
            } else
            {
                // Seven-segment stencils keep sector numbers legible without an image/font dependency.
                const unsigned masks[]{0x3F, 0x06, 0x5B, 0x4F};
                for (int digit = 0; digit < 2; ++digit)
                {
                    const float x = digit == 0 ? -0.25F : 0.25F;
                    const auto mask = masks[digit == 0 ? 0 : i - 1];
                    const float ys[]{0.32F, 0.16F, -0.16F, -0.32F, -0.16F, 0.16F, 0};
                    const float xs[]{0, 0.14F, 0.14F, 0, -0.14F, -0.14F, 0};
                    for (int segment = 0; segment < 7; ++segment)
                        if ((mask & (1U << segment)) != 0)
                            rect(
                                decal,
                                x + xs[segment],
                                ys[segment],
                                segment % 3 == 0 ? 0.24F : 0.055F,
                                segment % 3 == 0 ? 0.055F : 0.25F,
                                paint
                            );
                }
            }
            decal_meshes_[i].initialize(decal);
        }
    }

    void WorldSceneRenderer::render_environment(
        const RelayCore::render::PerspectiveCamera& camera,
        std::span<const EnvironmentEffect> effects
    )
    {
        const auto draw = [&](const auto& mesh, const glm::mat4& model, RenderPass pass = RenderPass::Opaque) {
            render_queue_.push(mesh, model, pass);
            record_render_draw(pass, static_cast<std::uint32_t>(mesh.index_count() / 3));
        };
        auto& puffs = environment_puffs_;
        puffs.clear();
        for (const auto& effect : effects)
        {
            if (glm::distance(effect.origin, camera.position()) > 28)
                continue;
            auto model = effect_model(effect);
            switch (effect.kind)
            {
                case EnvironmentKind::Fan:
                    draw(fan_frame_mesh_, model);
                    draw(fan_rotor_mesh_, glm::rotate(model, effect.phase, {0, 0, 1}));
                    break;
                case EnvironmentKind::Decal:
                {
                    const std::size_t index = effect.mark == "arrow" ? 0
                                              : effect.mark == "warning"
                                                  ? 1
                                                  : static_cast<std::size_t>(effect.mark[1] - '1' + 2);
                    draw(decal_meshes_[index], model);
                    break;
                }
                case EnvironmentKind::Lamp:
                    if (effect.fixture_width > 0)
                    {
                        model = glm::translate(glm::mat4{1}, effect.origin + glm::vec3{0, effect.fixture_offset, 0});
                        model = glm::rotate(model, glm::radians(effect.angles.x), {0, 1, 0});
                        model = glm::rotate(model, 1.5707963F, {1, 0, 0});
                        model = glm::scale(model, glm::vec3{effect.fixture_width, effect.fixture_depth, 1});
                        draw(lamp_meshes_[static_cast<std::size_t>(effect.strength * 15)], model);
                    }
                    break;
                case EnvironmentKind::Steam:
                    if (effect.strength > 0.015F)
                        for (int i = 0; i < 12; ++i)
                        {
                            const float t = std::fmod(effect.age * 0.65F + static_cast<float>(i) / 12, 1.0F);
                            const glm::vec3 local{
                                std::sin(static_cast<float>(i) * 2.4F + t * 3) * t * 0.16F,
                                t * 1.15F,
                                0.15F + t * 0.38F
                            };
                            const auto position = glm::vec3{model * glm::vec4{local, 1}};
                            const float size =
                                (0.10F + t * 0.45F) * std::sin(t * 3.1415927F) * effect.strength * effect.scale;
                            puffs.push_back({position, size});
                        }
                    break;
                case EnvironmentKind::Sparks:
                    if (effect.strength > 0 && std::fmod(effect.age, 3.2F) < 0.38F)
                        for (int i = 0; i < 8; ++i)
                        {
                            const float t = std::fmod(effect.age, 3.2F);
                            const float seed = static_cast<float>(i);
                            auto spark = glm::translate(
                                model,
                                {std::sin(seed * 3.7F) * t * 1.3F, t * (1.1F + seed * 0.13F) - 4 * t * t, t * 0.8F}
                            );
                            draw(spark_mesh_, glm::rotate(spark, seed * 1.2F, {0, 0, 1}));
                        }
                    break;
            }
        }
        const auto forward = camera.forward();
        const auto right = glm::normalize(glm::cross(forward, glm::vec3{0, 1, 0}));
        const auto up = glm::cross(right, forward);
        for (const auto& puff : puffs)
        {
            if (puff.size < 0.001F)
                continue;
            glm::mat4 model{1};
            model[0] = glm::vec4{right * puff.size, 0};
            model[1] = glm::vec4{up * puff.size, 0};
            model[2] = glm::vec4{-forward * puff.size, 0};
            model[3] = glm::vec4{puff.position, 1};
            const glm::vec3 delta = puff.position - camera.position();
            transparent_submissions_.push_back(
                {&steam_mesh_,
                 model,
                 glm::dot(delta, forward),
                 static_cast<std::uint32_t>(steam_mesh_.index_count() / 3)}
            );
        }
    }
} // namespace RelayGame
