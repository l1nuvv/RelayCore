#include "relaygame/world_irradiance.h"

#include "relaycore/core/paths.h"
#include "relaycore/render/authored_color.h"
#include "relaycore/render/model_asset.h"
#include "relaygame/world_scene_renderer.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <glm/geometric.hpp>
#include <sstream>
#include <stdexcept>
#include <unordered_map>

namespace RelayGame
{
    auto world_reflection_source(const RelayCore::BlockoutLevel& level) -> std::optional<WorldReflectionSource>
    {
        std::optional<WorldReflectionSource> source;
        for (const auto& entity : level.entities)
        {
            if (entity.classname != "reflection_probe")
                continue;
            if (source)
                throw std::runtime_error("only one reflection_probe is supported per scene");
            const auto property = [&](const char* key) -> const std::string& {
                const auto found = entity.properties.find(key);
                if (found == entity.properties.end() || found->second.empty())
                    throw std::runtime_error(std::string{"reflection_probe requires "} + key);
                return found->second;
            };
            const auto vector = [&](const char* key) {
                auto text = property(key);
                std::replace(text.begin(), text.end(), ',', ' ');
                std::istringstream input{text};
                glm::vec3 value;
                std::string extra;
                if (!(input >> value.x >> value.y >> value.z) || (input >> extra))
                    throw std::runtime_error(std::string{"reflection_probe."} + key + " requires three numbers");
                return value;
            };
            const auto integer = [&](const char* key) {
                std::istringstream input{property(key)};
                std::uint32_t value;
                std::string extra;
                if (!(input >> value) || (input >> extra))
                    throw std::runtime_error(std::string{"reflection_probe."} + key + " requires an integer");
                return value;
            };
            WorldReflectionSource result;
            result.settings.position = entity.origin;
            result.settings.mins = vector("mins");
            result.settings.maxs = vector("maxs");
            result.settings.resolution = integer("resolution");
            result.settings.samples = integer("samples");
            RelayCore::render::validate_reflection_settings(result.settings);
            const std::filesystem::path path{property("product")};
            if (path.is_absolute() || path.extension() != ".rcreflection"
                || std::ranges::any_of(path, [](const auto& part) {
                       return part == "..";
                   }))
                throw std::runtime_error("reflection_probe.product requires a relative .rcreflection path without '..'"
                );
            result.product = RelayCore::core::asset_path(path.generic_string());
            source = std::move(result);
        }
        return source;
    }

    auto world_irradiance_source(const RelayCore::BlockoutLevel& level) -> std::optional<WorldIrradianceSource>
    {
        std::optional<WorldIrradianceSource> source;
        for (const auto& entity : level.entities)
        {
            if (entity.classname != "irradiance_volume")
                continue;
            if (source)
                throw std::runtime_error("only one irradiance_volume is supported per native scene");
            const auto property = [&](const char* key) {
                const auto found = entity.properties.find(key);
                if (found == entity.properties.end() || found->second.empty())
                    throw std::runtime_error(std::string{"irradiance_volume requires "} + key);
                return found->second;
            };
            const auto vector = [&](const char* key) {
                auto text = property(key);
                std::replace(text.begin(), text.end(), ',', ' ');
                std::istringstream input{text};
                glm::vec3 value;
                std::string extra;
                if (!(input >> value.x >> value.y >> value.z) || (input >> extra))
                    throw std::runtime_error(std::string{"irradiance_volume."} + key + " requires three numbers");
                return value;
            };
            WorldIrradianceSource result;
            result.settings.mins = vector("mins");
            result.settings.maxs = vector("maxs");
            const auto dimensions = vector("dimensions");
            for (int axis = 0; axis < 3; ++axis)
            {
                if (!std::isfinite(dimensions[axis]) || dimensions[axis] < 2 || dimensions[axis] > 32
                    || std::floor(dimensions[axis]) != dimensions[axis])
                    throw std::runtime_error("irradiance_volume.dimensions requires integers in [2,32]");
                result.settings.dimensions[axis] = static_cast<std::uint32_t>(dimensions[axis]);
            }
            std::istringstream rays{property("rays")};
            std::string extra;
            if (!(rays >> result.settings.rays) || (rays >> extra))
                throw std::runtime_error("irradiance_volume.rays requires an integer");
            RelayCore::render::validate_irradiance_settings(result.settings);
            const std::filesystem::path path{property("product")};
            if (path.extension() != ".rcprobe" || std::ranges::any_of(path, [](const auto& part) {
                    return part == "..";
                }))
                throw std::runtime_error("irradiance_volume.product requires a .rcprobe path without '..'");
            result.product = RelayCore::core::asset_path(path.generic_string());
            source = std::move(result);
        }
        return source;
    }

    auto build_world_irradiance_scene(
        const PreparedWorldSceneData& scene,
        const RelayCore::render::RenderAuthoringConfig& authoring
    ) -> RelayCore::render::IrradianceBakeScene
    {
        using namespace RelayCore::render;
        IrradianceBakeScene result;
        const auto append = [&](const CpuColorMeshData& mesh, const glm::mat4& transform, glm::vec3 factor) {
            for (std::size_t i = 0; i < mesh.indices.size(); i += 3)
            {
                const auto& a = mesh.vertices.at(mesh.indices.at(i));
                const auto& b = mesh.vertices.at(mesh.indices.at(i + 1));
                const auto& c = mesh.vertices.at(mesh.indices.at(i + 2));
                const auto to_linear = [&](glm::vec3 color) {
                    return scene.color_pipeline_mode == ColorPipelineMode::hardware_srgb
                               ? color
                               : srgb_visual_rgb_to_linear(color);
                };
                // Area-independent per-triangle diffuse approximation; fine texture/normal detail stays at runtime.
                const auto diffuse = (to_linear(a.color) + to_linear(b.color) + to_linear(c.color)) / 3.F * factor;
                result.triangles.push_back(
                    {glm::vec3{transform * glm::vec4{a.position, 1}},
                     glm::vec3{transform * glm::vec4{b.position, 1}},
                     glm::vec3{transform * glm::vec4{c.position, 1}},
                     glm::clamp(diffuse, glm::vec3{0}, glm::vec3{1})}
                );
            }
        };
        for (const auto& chunk : scene.blockout.chunks)
            if (!chunk.transparent)
                append(chunk.mesh, glm::mat4{1}, glm::vec3{1});
        const auto srgb_bytes = [] {
            std::array<float, 256> values{};
            for (std::size_t i = 0; i < values.size(); ++i)
                values[i] = srgb_channel_to_linear(static_cast<float>(i) / 255.F);
            return values;
        }();
        struct BakeModel {
            ModelAssetData asset;
            std::vector<glm::vec3> reflectance;
        };
        // One preparation owns this cache; every new load rereads assets and validates their current signature.
        std::unordered_map<std::filesystem::path, BakeModel> models;
        for (const auto& prop : scene.props)
        {
            if (prop.model.extension() != ".rcmodel")
                throw std::runtime_error("irradiance bake supports compiled .rcmodel static props only");
            auto [found, inserted] = models.try_emplace(prop.model);
            auto& model = found->second;
            if (inserted)
            {
                model.asset = read_model_asset(prop.model);
                model.reflectance.reserve(model.asset.materials.size());
                for (const auto& material : model.asset.materials)
                {
                    float red = 0, green = 0, blue = 0;
                    const auto* lookup = srgb_bytes.data();
                    const auto* end = material.base_color.pixels.data() + material.base_color.pixels.size();
                    for (const auto* pixel = material.base_color.pixels.data(); pixel < end; pixel += 4)
                    {
                        red += lookup[pixel[0]];
                        green += lookup[pixel[1]];
                        blue += lookup[pixel[2]];
                    }
                    // Preserve each channel's float addition order: bake signatures include this mean.
                    glm::vec3 mean{red, green, blue};
                    mean /= static_cast<float>(material.base_color.pixels.size() / 4);
                    if (material.metallic_roughness)
                    {
                        float metallic = material.metallic_roughness->metallic;
                        if (material.metallic_roughness_map)
                        {
                            const auto& pixels = material.metallic_roughness_map->pixels;
                            double sum = 0;
                            for (std::size_t i = 2; i < pixels.size(); i += 4)
                                sum += pixels[i];
                            metallic *= static_cast<float>(sum / (255.0 * static_cast<double>(pixels.size() / 4)));
                        }
                        mean *= .96F * (1 - metallic);
                    }
                    model.reflectance.push_back(mean);
                }
            }
            for (const auto& part : model.asset.parts)
                append(
                    model.asset.meshes[part.mesh].geometry,
                    prop.transform * part.transform,
                    model.reflectance[part.material]
                );
        }
        for (const auto& entry : scene.local_lights)
            if (entry.required_target.empty())
            {
                auto light = entry.light;
                if (scene.color_pipeline_mode == ColorPipelineMode::legacy_gamma)
                    throw std::runtime_error("irradiance bake scene must use hardware_srgb for linear light inputs");
                light.color *= authoring.lighting.local_light_gain;
                result.lights.push_back(light);
            }
        return result;
    }

    auto load_world_irradiance(
        const RelayCore::BlockoutLevel& level,
        const PreparedWorldSceneData& scene,
        const RelayCore::render::RenderAuthoringConfig& authoring
    ) -> std::shared_ptr<const RelayCore::render::IrradianceProbeVolume>
    {
        const auto source = world_irradiance_source(level);
        if (!source)
            return {};
        // Bake signatures always use linear preparation, independent of the current player display mode.
        std::optional<PreparedWorldSceneData> linear_scene;
        if (scene.color_pipeline_mode != RelayCore::render::ColorPipelineMode::hardware_srgb)
        {
            auto linear_level = level;
            std::erase_if(linear_level.entities, [](const auto& e) {
                return e.classname == "irradiance_volume" || e.classname == "reflection_probe";
            });
            linear_scene = build_prepared_world_scene_data(
                linear_level,
                authoring,
                RelayCore::render::ColorPipelineMode::hardware_srgb
            );
        }
        const auto inputs = build_world_irradiance_scene(linear_scene ? *linear_scene : scene, authoring);
        auto volume = RelayCore::render::read_irradiance_probes(source->product);
        if (!RelayCore::render::irradiance_inputs_match(volume, inputs, source->settings))
            throw std::runtime_error(
                "stale irradiance product " + source->product.string()
                + "; rebuild relaycore_irradiance_assets after geometry, static lights or bake settings changes"
            );
        return std::make_shared<const RelayCore::render::IrradianceProbeVolume>(std::move(volume));
    }

    auto load_world_reflection(
        const RelayCore::BlockoutLevel& level,
        const PreparedWorldSceneData& scene,
        const RelayCore::render::RenderAuthoringConfig& authoring
    ) -> std::shared_ptr<const RelayCore::render::ReflectionProbe>
    {
        const auto source = world_reflection_source(level);
        if (!source)
            return {};
        std::optional<PreparedWorldSceneData> linear_scene;
        if (scene.color_pipeline_mode != RelayCore::render::ColorPipelineMode::hardware_srgb)
        {
            auto linear_level = level;
            std::erase_if(linear_level.entities, [](const auto& e) {
                return e.classname == "irradiance_volume" || e.classname == "reflection_probe";
            });
            linear_scene = build_prepared_world_scene_data(
                linear_level,
                authoring,
                RelayCore::render::ColorPipelineMode::hardware_srgb
            );
        }
        const auto inputs = build_world_irradiance_scene(linear_scene ? *linear_scene : scene, authoring);
        auto probe = RelayCore::render::read_reflection_probe(source->product);
        const auto ambient = glm::vec3{authoring.lighting.ambient_floor};
        if (probe.settings != source->settings
            || probe.source_signature
                   != RelayCore::render::reflection_source_signature(inputs, source->settings, ambient))
            throw std::runtime_error(
                "stale reflection product " + source->product.string()
                + "; rebuild relaycore_reflection_assets after geometry, static lights, materials or bake settings "
                  "changes"
            );
        return std::make_shared<const RelayCore::render::ReflectionProbe>(std::move(probe));
    }
} // namespace RelayGame
