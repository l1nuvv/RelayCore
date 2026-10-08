#include "relaygame/world_scene_renderer.h"

#include "relaycore/core/log.h"
#include "relaycore/core/paths.h"
#include "relaycore/core/profiling.h"
#include "relaycore/render/authored_color.h"
#include "relaycore/render/blockout_mesh_builder.h"
#include "relaycore/render/camera.h"
#include "relaycore/render/renderer.h"
#include "relaycore/world/blockout_level.h"
#include "relaygame/world_irradiance.h"
#include "relaygame/world_scene_light_order.h"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <glm/common.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>
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

        void tint_mesh_data(
            RelayCore::render::CpuColorMeshData& mesh_data,
            const glm::vec3& tint,
            const float bias,
            const RelayCore::render::ColorPipelineMode color_pipeline_mode
        )
        {
            for (auto& vertex : mesh_data.vertices)
            {
                const glm::vec3 visual_rgb = glm::clamp((vertex.color * tint) + glm::vec3{bias}, 0.0F, 1.0F);
                vertex.color = RelayCore::render::visual_rgb_for_pipeline(visual_rgb, color_pipeline_mode);
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

    auto build_prepared_world_scene_data(
        const RelayCore::BlockoutLevel& level,
        const RelayCore::render::RenderAuthoringConfig& authoring_config,
        const RelayCore::render::ColorPipelineMode color_pipeline_mode
    ) -> PreparedWorldSceneData
    {
        RELAYCORE_PROFILE_FUNCTION();
        PreparedWorldSceneData scene_data{};
        scene_data.color_pipeline_mode = color_pipeline_mode;
        scene_data.blockout =
            RelayCore::render::build_blockout_render_data(level, authoring_config, color_pipeline_mode);
        scene_data.total_solids = static_cast<std::uint32_t>(level.solids.size());
        scene_data.environment = build_world_environment(level);
        scene_data.props = build_static_props(level);
        scene_data.has_enemies = std::ranges::any_of(level.entities, [](const auto& e) {
            return e.classname == "enemy_rover";
        });

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
                scene_data.authoring_warnings.push_back(std::move(warning));
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

            light.color = RelayCore::render::visual_rgb_for_pipeline(light_color, color_pipeline_mode) * intensity;
            light.radius = radius;
            const auto required_target = entity.properties.find("required_target");
            scene_data.local_lights.push_back({
                .light = light,
                .required_target = required_target == entity.properties.end() ? "" : required_target->second,
            });
        }

        if (scene_data.local_lights.size() > kMaxActiveLocalLights)
        {
            scene_data.authoring_warnings.push_back(RelayCore::render::AuthoringWarningDiagnostics{
                .message = "Map authors " + std::to_string(scene_data.local_lights.size())
                           + " render_light entities, but the renderer"
                             " only evaluates the nearest "
                           + std::to_string(kMaxActiveLocalLights) + " per frame",
                .source_bucket = "generated",
            });
        }

        scene_data.irradiance = load_world_irradiance(level, scene_data, authoring_config);
        return scene_data;
    }

    auto WorldSceneRenderer::resource_items() const -> std::vector<RelayCore::render::RenderItem>
    {
        RELAYCORE_PROFILE_FUNCTION();
        RelayCore::render::RenderQueue resources;
        const auto add_mesh = [&](const auto& mesh) {
            if (mesh.cpu_data())
                resources.push(mesh, glm::mat4{1});
        };
        for (const auto* mesh :
             {&door_mesh_,
              &panel_mesh_,
              &panel_housing_mesh_,
              &scientist_thigh_mesh_,
              &scientist_shin_mesh_,
              &scientist_shoe_mesh_,
              &scientist_coat_mesh_,
              &panel_blocked_mesh_,
              &panel_highlight_mesh_,
              &panel_success_mesh_,
              &panel_warning_mesh_,
              &pickup_mesh_,
              &pickup_highlight_mesh_,
              &inspector_usable_mesh_,
              &inspector_door_mesh_,
              &inspector_trigger_mesh_,
              &inspector_solid_mesh_,
              &inspector_hit_mesh_,
              &player_shot_mesh_,
              &pellet_mesh_,
              &turret_shot_mesh_,
              &turret_warning_mesh_,
              &fan_frame_mesh_,
              &fan_rotor_mesh_,
              &steam_mesh_,
              &spark_mesh_})
            add_mesh(*mesh);
        for (const auto& mesh : decal_meshes_)
            add_mesh(mesh);
        for (const auto& mesh : lamp_meshes_)
            add_mesh(mesh);
        for (const auto& chunk : render_chunks_)
            add_mesh(chunk.mesh);
        const auto add_model = [&](const auto& model) {
            if (model)
                resources.push(model->mesh, glm::mat4{1}, RelayCore::render::RenderPass::Opaque, &model->texture);
        };
        for (const auto* model :
             {&enemy_model_,
              &destroyed_enemy_model_,
              &power_fuse_model_,
              &shotgun_body_model_,
              &shotgun_pump_model_,
              &turret_model_,
              &destroyed_turret_model_})
            add_model(*model);
        const auto add_asset = [&](const auto& asset) {
            if (asset)
                for (const auto& part : asset->parts)
                {
                    const auto& surface = asset->materials[part.material];
                    resources.push(
                        asset->meshes[part.mesh],
                        glm::mat4{1},
                        RelayCore::render::RenderPass::Opaque,
                        &surface.base_color,
                        true,
                        surface.normal.cpu_data() ? &surface.normal : nullptr,
                        surface.normal_scale,
                        surface.metallic_roughness
                    );
                }
        };
        add_asset(service_key_model_);
        for (const auto& prop : props_)
        {
            if (prop.asset)
                add_asset(prop.asset);
            else
                add_model(prop.model);
        }
        return {resources.items().begin(), resources.items().end()};
    }

    void WorldSceneRenderer::initialize(
        const PreparedWorldSceneData& scene_data,
        RelayCore::render::StaticModelCache& models
    )
    {
        RELAYCORE_PROFILE_FUNCTION();
        props_.clear();
        enemy_model_.reset();
        destroyed_enemy_model_.reset();
        power_fuse_model_.reset();
        service_key_model_.reset();
        turret_model_.reset();
        destroyed_turret_model_.reset();
        if (scene_data.has_enemies)
        {
            enemy_model_ = models.load(
                RelayCore::core::asset_path("models/security_rover.obj"),
                RelayCore::core::asset_path("models/security_rover.bmp"),
                scene_data.color_pipeline_mode == RelayCore::render::ColorPipelineMode::hardware_srgb
                    ? RelayCore::render::TextureEncoding::srgb
                    : RelayCore::render::TextureEncoding::linear
            );
            destroyed_enemy_model_ = models.load(
                RelayCore::core::asset_path("models/security_rover_destroyed.obj"),
                RelayCore::core::asset_path("models/security_rover.bmp"),
                scene_data.color_pipeline_mode == RelayCore::render::ColorPipelineMode::hardware_srgb
                    ? RelayCore::render::TextureEncoding::srgb
                    : RelayCore::render::TextureEncoding::linear
            );
        }
        turret_model_ = models.load(
            RelayCore::core::asset_path("models/security_turret.obj"),
            RelayCore::core::asset_path("models/security_turret.bmp"),
            scene_data.color_pipeline_mode == RelayCore::render::ColorPipelineMode::hardware_srgb
                ? RelayCore::render::TextureEncoding::srgb
                : RelayCore::render::TextureEncoding::linear
        );
        service_key_model_ = models.load(
            RelayCore::core::asset_path("models/service_key.rcmodel"),
            RelayCore::render::TextureEncoding::srgb
        );
        power_fuse_model_ = models.load(
            RelayCore::core::asset_path("models/power_fuse.obj"),
            RelayCore::core::asset_path("models/power_fuse.bmp"),
            scene_data.color_pipeline_mode == RelayCore::render::ColorPipelineMode::hardware_srgb
                ? RelayCore::render::TextureEncoding::srgb
                : RelayCore::render::TextureEncoding::linear
        );
        destroyed_turret_model_ = models.load(
            RelayCore::core::asset_path("models/security_turret_destroyed.obj"),
            RelayCore::core::asset_path("models/security_turret_destroyed.bmp"),
            scene_data.color_pipeline_mode == RelayCore::render::ColorPipelineMode::hardware_srgb
                ? RelayCore::render::TextureEncoding::srgb
                : RelayCore::render::TextureEncoding::linear
        );
        const auto shotgun_encoding =
            scene_data.color_pipeline_mode == RelayCore::render::ColorPipelineMode::hardware_srgb
                ? RelayCore::render::TextureEncoding::srgb
                : RelayCore::render::TextureEncoding::linear;
        shotgun_body_model_ = models.load(
            RelayCore::core::asset_path("models/shotgun_body.obj"),
            RelayCore::core::asset_path("models/shotgun.bmp"),
            shotgun_encoding
        );
        shotgun_pump_model_ = models.load(
            RelayCore::core::asset_path("models/shotgun_pump.obj"),
            RelayCore::core::asset_path("models/shotgun.bmp"),
            shotgun_encoding
        );
        for (const auto& prop : scene_data.props)
        {
            const auto encoding = scene_data.color_pipeline_mode == RelayCore::render::ColorPipelineMode::hardware_srgb
                                      ? RelayCore::render::TextureEncoding::srgb
                                      : RelayCore::render::TextureEncoding::linear;
            if (prop.model.extension() == ".rcmodel")
            {
                props_.push_back({{}, prop.transform, models.load(prop.model, encoding)});
                continue;
            }
            props_.push_back(
                {models.load(
                     prop.model,
                     prop.texture,
                     scene_data.color_pipeline_mode == RelayCore::render::ColorPipelineMode::hardware_srgb
                         ? RelayCore::render::TextureEncoding::srgb
                         : RelayCore::render::TextureEncoding::linear
                 ),
                 prop.transform}
            );
        }
        RelayCore::render::CpuColorMeshData door_mesh_data = RelayCore::render::make_color_box_mesh_data();
        RelayCore::render::CpuColorMeshData pickup_mesh_data = RelayCore::render::make_color_box_mesh_data();
        RelayCore::render::CpuColorMeshData pickup_highlight_mesh_data = RelayCore::render::make_color_box_mesh_data();
        RelayCore::render::CpuColorMeshData inspector_usable_mesh_data = RelayCore::render::make_color_box_mesh_data();
        RelayCore::render::CpuColorMeshData inspector_door_mesh_data = RelayCore::render::make_color_box_mesh_data();
        RelayCore::render::CpuColorMeshData inspector_trigger_mesh_data = RelayCore::render::make_color_box_mesh_data();
        RelayCore::render::CpuColorMeshData inspector_solid_mesh_data = RelayCore::render::make_color_box_mesh_data();
        RelayCore::render::CpuColorMeshData inspector_hit_mesh_data = RelayCore::render::make_color_box_mesh_data();
        RelayCore::render::CpuColorMeshData player_shot_mesh_data = RelayCore::render::make_color_box_mesh_data();
        RelayCore::render::CpuColorMeshData pellet_mesh_data = RelayCore::render::make_color_box_mesh_data();
        RelayCore::render::CpuColorMeshData turret_shot_mesh_data = RelayCore::render::make_color_box_mesh_data();

        const auto tint = [&scene_data](auto& mesh, const glm::vec3& multiplier, const float bias) {
            tint_mesh_data(mesh, multiplier, bias, scene_data.color_pipeline_mode);
        };
        tint(door_mesh_data, {1.0F, 1.0F, 1.0F}, 0.0F);
        tint(pickup_mesh_data, {0.52F, 1.82F, 1.90F}, 0.06F);
        tint(pickup_highlight_mesh_data, {0.86F, 2.20F, 2.30F}, 0.14F);
        tint(inspector_usable_mesh_data, {2.25F, 1.82F, 0.62F}, 0.12F);
        tint(inspector_door_mesh_data, {0.62F, 2.08F, 2.28F}, 0.10F);
        tint(inspector_trigger_mesh_data, {2.28F, 0.92F, 0.58F}, 0.08F);
        tint(inspector_solid_mesh_data, {1.18F, 1.55F, 2.18F}, 0.08F);
        tint(inspector_hit_mesh_data, {2.60F, 2.60F, 2.60F}, 0.16F);
        tint(player_shot_mesh_data, {2.4F, 1.9F, 0.85F}, 0.20F);
        tint(pellet_mesh_data, {1.6F, 1.5F, 1.3F}, 0.10F);
        tint(turret_shot_mesh_data, {2.5F, 0.75F, 0.35F}, 0.10F);

        door_mesh_.initialize(door_mesh_data);
        pickup_mesh_.initialize(pickup_mesh_data);
        pickup_highlight_mesh_.initialize(pickup_highlight_mesh_data);
        inspector_usable_mesh_.initialize(inspector_usable_mesh_data);
        inspector_door_mesh_.initialize(inspector_door_mesh_data);
        inspector_trigger_mesh_.initialize(inspector_trigger_mesh_data);
        inspector_solid_mesh_.initialize(inspector_solid_mesh_data);
        inspector_hit_mesh_.initialize(inspector_hit_mesh_data);
        player_shot_mesh_.initialize(player_shot_mesh_data);
        pellet_mesh_.initialize(pellet_mesh_data);
        turret_shot_mesh_.initialize(turret_shot_mesh_data);
        initialize_prop_meshes(scene_data.color_pipeline_mode);
        initialize_environment_meshes(scene_data.color_pipeline_mode);
        environment_ = scene_data.environment;
        environment_puffs_.reserve(environment_.size() * 12);

        render_chunks_.clear();
        render_chunks_.reserve(scene_data.blockout.chunks.size());
        local_lights_ = scene_data.local_lights;
        irradiance_ = scene_data.irradiance;
        authoring_warnings_ = scene_data.authoring_warnings;

        for (const auto& chunk_data : scene_data.blockout.chunks)
        {
            RenderChunk chunk{};
            chunk.mesh.initialize(chunk_data.mesh);
            chunk.mins = chunk_data.mins;
            chunk.maxs = chunk_data.maxs;
            chunk.triangle_count = chunk_data.triangle_count;
            chunk.transparent = chunk_data.transparent;
            chunk.casts_directional_shadow = chunk_data.casts_directional_shadow;
            render_chunks_.push_back(std::move(chunk));
        }
        diagnostics_.total_solids = scene_data.total_solids;
        diagnostics_.total_chunks = static_cast<std::uint32_t>(render_chunks_.size());
        diagnostics_.opaque_total_chunks = static_cast<std::uint32_t>(std::count_if(
            render_chunks_.begin(),
            render_chunks_.end(),
            [](const RenderChunk& chunk) {
                return !chunk.transparent;
            }
        ));
        diagnostics_.transparent_total_chunks = diagnostics_.total_chunks - diagnostics_.opaque_total_chunks;
        diagnostics_.visible_solids = static_cast<std::uint32_t>(render_chunks_.size());
        diagnostics_.visible_chunks = static_cast<std::uint32_t>(render_chunks_.size());
        diagnostics_.opaque_visible_chunks = diagnostics_.opaque_total_chunks;
        diagnostics_.transparent_visible_chunks = diagnostics_.transparent_total_chunks;
        diagnostics_.draw_calls = diagnostics_.visible_solids;
        diagnostics_.opaque_draw_calls = diagnostics_.opaque_visible_chunks;
        diagnostics_.transparent_draw_calls = diagnostics_.transparent_visible_chunks;
        diagnostics_.triangles = scene_data.blockout.total_triangles;
        diagnostics_.total_render_lights = static_cast<std::uint32_t>(local_lights_.size());
        diagnostics_.active_local_light_budget = static_cast<std::uint32_t>(kMaxActiveLocalLights);
        diagnostics_.local_light_count =
            static_cast<std::uint32_t>(std::min(local_lights_.size(), kMaxActiveLocalLights));
    }

    void WorldSceneRenderer::shutdown() noexcept
    {
        RELAYCORE_PROFILE_FUNCTION();
        render_queue_.clear();
        shadow_casters_.clear();
        transparent_submissions_.clear();
        render_chunks_.clear();
        render_chunks_.shrink_to_fit();
        props_.clear();
        enemy_model_.reset();
        destroyed_enemy_model_.reset();
        power_fuse_model_.reset();
        shotgun_body_model_.reset();
        shotgun_pump_model_.reset();
        service_key_model_.reset();
        turret_model_.reset();
        destroyed_turret_model_.reset();

        door_mesh_ = RelayCore::render::StaticColorMesh{};
        panel_mesh_ = RelayCore::render::StaticColorMesh{};
        panel_housing_mesh_ = RelayCore::render::StaticColorMesh{};
        scientist_thigh_mesh_ = RelayCore::render::StaticColorMesh{};
        scientist_shin_mesh_ = RelayCore::render::StaticColorMesh{};
        scientist_shoe_mesh_ = RelayCore::render::StaticColorMesh{};
        scientist_coat_mesh_ = RelayCore::render::StaticColorMesh{};
        panel_blocked_mesh_ = RelayCore::render::StaticColorMesh{};
        panel_highlight_mesh_ = RelayCore::render::StaticColorMesh{};
        panel_success_mesh_ = RelayCore::render::StaticColorMesh{};
        panel_warning_mesh_ = RelayCore::render::StaticColorMesh{};
        pickup_mesh_ = RelayCore::render::StaticColorMesh{};
        pickup_highlight_mesh_ = RelayCore::render::StaticColorMesh{};
        inspector_usable_mesh_ = RelayCore::render::StaticColorMesh{};
        inspector_door_mesh_ = RelayCore::render::StaticColorMesh{};
        inspector_trigger_mesh_ = RelayCore::render::StaticColorMesh{};
        inspector_solid_mesh_ = RelayCore::render::StaticColorMesh{};
        inspector_hit_mesh_ = RelayCore::render::StaticColorMesh{};
        player_shot_mesh_ = RelayCore::render::StaticColorMesh{};
        pellet_mesh_ = RelayCore::render::StaticColorMesh{};
        turret_shot_mesh_ = RelayCore::render::StaticColorMesh{};
        turret_warning_mesh_ = RelayCore::render::StaticColorMesh{};
        local_lights_.clear();
        irradiance_.reset();
        environment_.clear();
        environment_puffs_.clear();
        fan_frame_mesh_ = {};
        fan_rotor_mesh_ = {};
        steam_mesh_ = {};
        spark_mesh_ = {};
        for (auto& mesh : decal_meshes_)
            mesh = {};
        for (auto& mesh : lamp_meshes_)
            mesh = {};
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
        const bool inspector_visible,
        const std::span<const std::string> activated_targets,
        const ScientistPose& scientist,
        const std::span<const EnvironmentEffect> environment,
        const std::span<const RuntimeEnemy> enemies,
        const RelayCore::gameplay::PlayerCombatState* player_combat,
        const std::span<const RelayCore::gameplay::CombatShotTrace> pellet_traces
    )
    {
        prepare_render_queue(
            camera,
            scene_time_seconds,
            doors,
            turrets,
            usables,
            focused_usable_index,
            inspector,
            inspector_visible,
            activated_targets,
            scientist,
            environment,
            enemies,
            player_combat,
            pellet_traces
        );
        renderer.set_local_lights(active_local_lights_);
        renderer.set_irradiance_probes(irradiance_);
        renderer.draw_render_queue(render_queue_.items(), shadow_casters_.items());
    }

    void WorldSceneRenderer::prepare_render_queue(
        const RelayCore::render::PerspectiveCamera& camera,
        const float scene_time_seconds,
        std::span<const RelayCore::gameplay::RuntimeDoor> doors,
        std::span<const RelayCore::gameplay::RuntimeTurret> turrets,
        std::span<const RelayCore::gameplay::RuntimeUsable> usables,
        const int focused_usable_index,
        const RelayCore::render::InspectorDiagnostics& inspector,
        const bool inspector_visible,
        const std::span<const std::string> activated_targets,
        const ScientistPose& scientist,
        const std::span<const EnvironmentEffect> environment,
        const std::span<const RuntimeEnemy> enemies,
        const RelayCore::gameplay::PlayerCombatState* player_combat,
        const std::span<const RelayCore::gameplay::CombatShotTrace> pellet_traces
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
        shadow_casters_.clear();
        transparent_submissions_.clear();
        render_queue_.reserve(
            render_chunks_.size() + props_.size() + doors.size() + turrets.size() + enemies.size()
            + (usables.size() * 3)
        );
        shadow_casters_.reserve(
            render_chunks_.size() + props_.size() + doors.size() + turrets.size() + enemies.size() + usables.size() * 2
        );
        for (const auto& prop : props_)
        {
            if (prop.asset)
            {
                for (const auto& part : prop.asset->parts)
                {
                    const auto& mesh = prop.asset->meshes[part.mesh];
                    const auto& surface = prop.asset->materials[part.material];
                    render_queue_.push(
                        mesh,
                        prop.transform * part.transform,
                        RelayCore::render::RenderPass::Opaque,
                        &surface.base_color,
                        true,
                        surface.normal.cpu_data() ? &surface.normal : nullptr,
                        surface.normal_scale,
                        surface.metallic_roughness
                    );
                    record_render_draw(
                        RelayCore::render::RenderPass::Opaque,
                        static_cast<std::uint32_t>(mesh.index_count() / 3)
                    );
                }
                continue;
            }
            render_queue_
                .push(prop.model->mesh, prop.transform, RelayCore::render::RenderPass::Opaque, &prop.model->texture);
            record_render_draw(
                RelayCore::render::RenderPass::Opaque,
                static_cast<std::uint32_t>(prop.model->mesh.index_count() / 3)
            );
        }
        transparent_submissions_.reserve(
            render_chunks_.size() + (environment.empty() ? environment_.size() : environment.size()) * 12
        );

        frustum_planes_ = extract_frustum_planes(camera.view_projection_matrix());
        {
            RELAYCORE_PROFILE_ZONE_N("WorldSceneRenderer::draw_visible_blockout_chunks");
            for (const RenderChunk& chunk : render_chunks_)
            {
                if (!aabb_intersects_frustum(frustum_planes_, chunk.mins, chunk.maxs))
                {
                    ++diagnostics_.culled_chunks;
                    if (!chunk.transparent)
                        shadow_casters_.push(
                            chunk.mesh,
                            glm::mat4{1.0F},
                            RelayCore::render::RenderPass::Opaque,
                            nullptr,
                            chunk.casts_directional_shadow
                        );
                    continue;
                }

                record_visible_chunk(chunk.transparent);
                if (chunk.transparent)
                {
                    const glm::vec3 center = (chunk.mins + chunk.maxs) * 0.5F;
                    const glm::vec3 delta = center - camera.position();
                    transparent_submissions_.push_back(
                        {&chunk.mesh, glm::mat4{1.0F}, glm::dot(delta, camera.forward()), chunk.triangle_count}
                    );
                    continue;
                }

                record_render_draw(RelayCore::render::RenderPass::Opaque, chunk.triangle_count);
                render_queue_.push(
                    chunk.mesh,
                    glm::mat4{1.0F},
                    RelayCore::render::RenderPass::Opaque,
                    nullptr,
                    chunk.casts_directional_shadow
                );
            }
        }

        render_doors(camera, doors);
        render_scientist(scientist);
        render_turrets(camera, turrets);
        if (player_combat != nullptr)
            render_shot_trace(camera, player_combat->shot_trace, false);
        for (const auto& trace : pellet_traces)
            render_shot_trace(camera, trace, false, true);
        for (const auto& turret : turrets)
            render_shot_trace(camera, turret.shot_trace, true);
        if (enemy_model_)
            for (const auto& enemy : enemies)
            {
                const auto& state = enemy.state;
                auto transform = glm::translate(glm::mat4{1}, state.position);
                transform = glm::rotate(transform, glm::radians(state.yaw), glm::vec3{0, 1, 0});
                if (state.mode == EnemyMode::Windup)
                    transform = glm::rotate(
                        transform,
                        -.12F * std::sin((.45F - state.timer) / .45F * 3.14159265F),
                        glm::vec3{1, 0, 0}
                    );
                const auto& model = state.mode == EnemyMode::Dead ? destroyed_enemy_model_ : enemy_model_;
                render_queue_.push(model->mesh, transform, RelayCore::render::RenderPass::Opaque, &model->texture);
                record_render_draw(
                    RelayCore::render::RenderPass::Opaque,
                    static_cast<std::uint32_t>(model->mesh.index_count() / 3)
                );
            }
        render_interactive_usables(camera, scene_time_seconds, usables, focused_usable_index, true);
        render_environment(
            camera,
            environment.empty() ? std::span<const EnvironmentEffect>{environment_} : environment
        );
        if (inspector_visible)
        {
            render_inspector_pick(camera, scene_time_seconds, inspector);
        }

        std::ranges::stable_sort(transparent_submissions_, [](const auto& lhs, const auto& rhs) {
            return lhs.depth > rhs.depth;
        });
        for (const TransparentSubmission& submission : transparent_submissions_)
        {
            record_render_draw(RelayCore::render::RenderPass::Transparent, submission.triangles);
            render_queue_.push(*submission.mesh, submission.model, RelayCore::render::RenderPass::Transparent);
        }

        active_local_lights_.clear();
        if (!local_lights_.empty())
        {
            std::vector<RelayCore::render::LocalLight> sorted_lights{};
            sorted_lights.reserve(local_lights_.size());
            for (const WorldLocalLight& entry : local_lights_)
            {
                if (entry.required_target.empty()
                    || std::ranges::find(activated_targets, entry.required_target) != activated_targets.end())
                {
                    auto light = entry.light;
                    const auto index = static_cast<int>(&entry - local_lights_.data());
                    const auto state = std::ranges::find(environment, index, &EnvironmentEffect::light_index);
                    if (state != environment.end())
                        light.color *= state->strength;
                    sorted_lights.push_back(light);
                }
            }

            for (const auto& effect : environment)
                if (effect.kind == EnvironmentKind::Sparks && effect.strength > 0
                    && std::fmod(effect.age, 3.2F) < 0.08F)
                    sorted_lights.push_back({effect.origin, {1.6F, 0.85F, 0.35F}, 2.1F});
            std::ranges::sort(sorted_lights, [&camera](const auto& lhs, const auto& rhs) {
                const float lhs_score = local_light_score(lhs, camera);
                const float rhs_score = local_light_score(rhs, camera);
                return detail::local_light_precedes(lhs_score, lhs.radius, rhs_score, rhs.radius);
            });

            const std::size_t active_count = std::min(sorted_lights.size(), kMaxActiveLocalLights);
            active_local_lights_.reserve(active_count);
            for (std::size_t index = 0; index < active_count; ++index)
            {
                active_local_lights_.push_back(sorted_lights[index]);
            }
        }

        diagnostics_.local_light_count = static_cast<std::uint32_t>(active_local_lights_.size());
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
        for (const RelayCore::render::RenderItem& item : render_queue_.items())
        {
            if (item.mesh != nullptr && item.pass == RelayCore::render::RenderPass::Opaque)
                shadow_casters_.push(
                    *item.mesh,
                    item.model_matrix,
                    item.pass,
                    item.diffuse_texture,
                    item.casts_directional_shadow
                );
        }
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
