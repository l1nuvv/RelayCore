#include "relaycore/render/renderer.h"

#include "relaycore/audio/audio_system.h"
#include "relaycore/core/log.h"
#include "relaycore/core/paths.h"
#include "relaycore/core/profiling_opengl.h"
#include "relaycore/platform/window.h"
#include "relaycore/render/blockout_texture_atlas.h"
#include "relaycore/render/gl_debug.h"
#include "relaycore/render/gl_loader.h"

#include <algorithm>
#include <cmath>
#include <glad/gl.h>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/geometric.hpp>
#include <glm/mat4x4.hpp>
#include <imgui.h>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace RelayCore
{
    namespace
    {
        constexpr std::size_t kNearShadowCascadeIndex = 0;
        constexpr std::size_t kFarShadowCascadeIndex = 1;

        class ShadowResourceBindingGuard {
        public:
            ShadowResourceBindingGuard()
            {
                glGetIntegerv(GL_ACTIVE_TEXTURE, &active_texture_);
                glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &draw_framebuffer_);
                glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &read_framebuffer_);
                glGetIntegerv(GL_TEXTURE_BINDING_2D, &texture_2d_);
            }

            ~ShadowResourceBindingGuard() noexcept
            {
                glBindFramebuffer(GL_DRAW_FRAMEBUFFER, static_cast<unsigned int>(draw_framebuffer_));
                glBindFramebuffer(GL_READ_FRAMEBUFFER, static_cast<unsigned int>(read_framebuffer_));
                glActiveTexture(static_cast<unsigned int>(active_texture_));
                glBindTexture(GL_TEXTURE_2D, static_cast<unsigned int>(texture_2d_));
            }

            ShadowResourceBindingGuard(const ShadowResourceBindingGuard&) = delete;

            auto operator=(const ShadowResourceBindingGuard&) -> ShadowResourceBindingGuard& = delete;

        private:
            GLint active_texture_ = GL_TEXTURE0;
            GLint draw_framebuffer_ = 0;
            GLint read_framebuffer_ = 0;
            GLint texture_2d_ = 0;
        };

        void apply_render_pass_state(const render::RenderPass pass)
        {
            RELAYCORE_PROFILE_FUNCTION();
            switch (pass)
            {
                case render::RenderPass::Opaque:
                    glDisable(GL_BLEND);
                    glDepthMask(GL_TRUE);
                    glEnable(GL_CULL_FACE);
                    break;
                case render::RenderPass::Overlay:
                    glEnable(GL_BLEND);
                    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                    glDepthMask(GL_FALSE);
                    glDisable(GL_CULL_FACE);
                    break;
                case render::RenderPass::Transparent:
                    glEnable(GL_BLEND);
                    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                    glDepthMask(GL_FALSE);
                    glDisable(GL_CULL_FACE);
                    break;
            }
        }

        [[nodiscard]] auto choose_shadow_up(const glm::vec3& light_direction) -> glm::vec3
        {
            RELAYCORE_PROFILE_FUNCTION();
            const glm::vec3 world_up{0.0F, 1.0F, 0.0F};
            if (std::abs(glm::dot(light_direction, world_up)) > 0.98F)
            {
                return {0.0F, 0.0F, 1.0F};
            }
            return world_up;
        }

        [[nodiscard]] auto round_to_shadow_texel(const float value, const float texel_world_size) -> float
        {
            RELAYCORE_PROFILE_FUNCTION();
            if (texel_world_size <= 0.000001F)
            {
                return value;
            }

            return std::round(value / texel_world_size) * texel_world_size;
        }

        [[nodiscard]] auto safe_shadow_direction(const glm::vec3& direction) -> glm::vec3
        {
            RELAYCORE_PROFILE_FUNCTION();
            const float length = glm::length(direction);
            if (length <= 0.0001F)
            {
                return glm::normalize(glm::vec3{0.28F, -1.0F, 0.22F});
            }

            return direction / length;
        }

        void draw_shadow_debug_banner()
        {
            RELAYCORE_PROFILE_FUNCTION();
            ImGuiViewport* viewport = ImGui::GetMainViewport();
            ImDrawList* draw_list = ImGui::GetForegroundDrawList();

            const char* label = "SHADOW CASCADE DEBUG";
            const char* detail = "Cascade tint is enabled. Disable it in Diagnostics -> Shadow Tuning.";
            const ImVec2 label_size = ImGui::CalcTextSize(label);
            const ImVec2 detail_size = ImGui::CalcTextSize(detail);
            const float width = std::max(label_size.x, detail_size.x) + 40.0F;
            const float height = label_size.y + detail_size.y + 28.0F;
            const ImVec2 min{
                viewport->WorkPos.x + (viewport->WorkSize.x - width) * 0.5F,
                viewport->WorkPos.y + 18.0F,
            };
            const ImVec2 max{min.x + width, min.y + height};

            draw_list
                ->AddRectFilled(min, max, ImGui::ColorConvertFloat4ToU32(ImVec4(0.17F, 0.09F, 0.05F, 0.92F)), 10.0F);
            draw_list
                ->AddRect(min, max, ImGui::ColorConvertFloat4ToU32(ImVec4(0.98F, 0.66F, 0.24F, 0.85F)), 10.0F, 0, 1.5F);
            draw_list->AddText(
                ImVec2(min.x + 20.0F, min.y + 10.0F),
                ImGui::ColorConvertFloat4ToU32(ImVec4(1.0F, 0.93F, 0.76F, 1.0F)),
                label
            );
            draw_list->AddText(
                ImVec2(min.x + 20.0F, min.y + 10.0F + label_size.y + 6.0F),
                ImGui::ColorConvertFloat4ToU32(ImVec4(0.96F, 0.84F, 0.68F, 1.0F)),
                detail
            );
        }

        [[nodiscard]] auto format_glsl_float(const float value) -> std::string
        {
            std::ostringstream stream;
            stream << std::fixed << std::setprecision(6) << value;
            return stream.str();
        }

        [[nodiscard]] auto format_glsl_vec3(const glm::vec3& value) -> std::string
        {
            return "vec3(" + format_glsl_float(value.x) + ", " + format_glsl_float(value.y) + ", "
                   + format_glsl_float(value.z) + ")";
        }

        [[nodiscard]] auto profile_shader_id(const render::MaterialRenderProfile profile) -> int
        {
            return static_cast<int>(profile);
        }

        [[nodiscard]] auto build_shader_contract_source(const render::RenderAuthoringConfig& authoring_config)
            -> std::string
        {
            std::ostringstream stream;
            const std::size_t material_count = RelayCore::surface_material_count();

            stream << "const int RC_MATERIAL_PROFILE_DEFAULT = 0;\n";
            stream << "const int RC_MATERIAL_PROFILE_GLASS = 1;\n";
            stream << "const int RC_MATERIAL_PROFILE_FLOOR_GRID = 2;\n";
            stream << "const int RC_MATERIAL_PROFILE_CEILING_GRID = 3;\n";
            stream << "const int RC_MATERIAL_PROFILE_PANEL = 4;\n";
            stream << "const int RC_MATERIAL_PROFILE_PANEL_INSET = 5;\n";
            stream << "const int RC_MATERIAL_PROFILE_PANEL_SERVICE = 6;\n";
            stream << "const int RC_MATERIAL_PROFILE_PLATFORM_TOP = 7;\n";
            stream << "const int RC_MATERIAL_PROFILE_STRUCTURAL = 8;\n";
            stream << "const int RC_MATERIAL_PROFILE_DIVIDER = 9;\n";
            stream << "const int RC_MATERIAL_PROFILE_LIGHT_STRIP = 10;\n";
            stream << "const int RC_SURFACE_MATERIAL_COUNT = " << material_count << ";\n";

            const auto emit_float_array =
                [&stream, material_count, &authoring_config](const char* name, const auto& value_fn) {
                    stream << "const float " << name << "[" << material_count << "] = float[" << material_count << "](";
                    for (std::size_t index = 0; index < material_count; ++index)
                    {
                        if (index > 0)
                        {
                            stream << ", ";
                        }
                        stream << format_glsl_float(value_fn(authoring_config.materials[index]));
                    }
                    stream << ");\n";
                };

            const auto emit_int_array =
                [&stream, material_count, &authoring_config](const char* name, const auto& value_fn) {
                    stream << "const int " << name << "[" << material_count << "] = int[" << material_count << "](";
                    for (std::size_t index = 0; index < material_count; ++index)
                    {
                        if (index > 0)
                        {
                            stream << ", ";
                        }
                        stream << value_fn(authoring_config.materials[index]);
                    }
                    stream << ");\n";
                };

            emit_float_array("rc_material_atlas_scales", [](const render::MaterialAuthoringConfig& material) {
                return material.atlas_world_scale;
            });
            emit_float_array("rc_material_atlas_mix_values", [](const render::MaterialAuthoringConfig& material) {
                return material.atlas_mix;
            });
            emit_float_array("rc_material_ambient_bias_values", [](const render::MaterialAuthoringConfig& material) {
                return material.ambient_bias;
            });
            emit_float_array("rc_material_bounce_mix_values", [](const render::MaterialAuthoringConfig& material) {
                return material.bounce_mix;
            });
            emit_float_array(
                "rc_material_emissive_strength_values",
                [](const render::MaterialAuthoringConfig& material) {
                    return material.emissive_strength;
                }
            );
            emit_float_array(
                "rc_material_emissive_pulse_speed_values",
                [](const render::MaterialAuthoringConfig& material) {
                    return material.emissive_pulse_speed;
                }
            );
            emit_int_array("rc_material_profile_values", [](const render::MaterialAuthoringConfig& material) {
                return profile_shader_id(material.render_profile);
            });

            stream << "int rc_material_index(int material_id) { return clamp(material_id, 0, RC_SURFACE_MATERIAL_COUNT "
                      "- 1); }\n";
            stream << "float rc_material_atlas_world_scale(int material_id) { return "
                      "rc_material_atlas_scales[rc_material_index(material_id)]; }\n";
            stream << "float rc_material_atlas_mix(int material_id) { return "
                      "rc_material_atlas_mix_values[rc_material_index(material_id)]; }\n";
            stream << "float rc_material_ambient_bias(int material_id) { return "
                      "rc_material_ambient_bias_values[rc_material_index(material_id)]; }\n";
            stream << "float rc_material_bounce_mix(int material_id) { return "
                      "rc_material_bounce_mix_values[rc_material_index(material_id)]; }\n";
            stream << "float rc_material_emissive_strength(int material_id) { return "
                      "rc_material_emissive_strength_values[rc_material_index(material_id)]; }\n";
            stream << "float rc_material_emissive_pulse_speed(int material_id) { return "
                      "rc_material_emissive_pulse_speed_values[rc_material_index(material_id)]; }\n";
            stream << "int rc_material_profile(int material_id) { return "
                      "rc_material_profile_values[rc_material_index(material_id)]; }\n";

            stream << "vec3 rc_cool_ambient_color() { return "
                   << format_glsl_vec3(authoring_config.lighting.cool_ambient_color) << "; }\n";
            stream << "float rc_cool_ambient_floor() { return "
                   << format_glsl_float(authoring_config.lighting.cool_ambient_floor) << "; }\n";
            stream << "float rc_cool_ambient_zenith() { return "
                   << format_glsl_float(authoring_config.lighting.cool_ambient_zenith) << "; }\n";
            stream << "vec3 rc_warm_bounce_color() { return "
                   << format_glsl_vec3(authoring_config.lighting.warm_bounce_color) << "; }\n";
            stream << "float rc_warm_bounce_floor() { return "
                   << format_glsl_float(authoring_config.lighting.warm_bounce_floor) << "; }\n";
            stream << "float rc_warm_bounce_underside() { return "
                   << format_glsl_float(authoring_config.lighting.warm_bounce_underside) << "; }\n";
            stream << "vec3 rc_bounce_neutral_color() { return "
                   << format_glsl_vec3(authoring_config.lighting.bounce_neutral_color) << "; }\n";
            stream << "float rc_bounce_floor() { return " << format_glsl_float(authoring_config.lighting.bounce_floor)
                   << "; }\n";
            stream << "float rc_bounce_underside() { return "
                   << format_glsl_float(authoring_config.lighting.bounce_underside) << "; }\n";
            stream << "float rc_bounce_darkness() { return "
                   << format_glsl_float(authoring_config.lighting.bounce_darkness) << "; }\n";
            stream << "float rc_ambient_floor() { return " << format_glsl_float(authoring_config.lighting.ambient_floor)
                   << "; }\n";
            stream << "float rc_ambient_zenith() { return "
                   << format_glsl_float(authoring_config.lighting.ambient_zenith) << "; }\n";
            stream << "float rc_ambient_min() { return " << format_glsl_float(authoring_config.lighting.ambient_min)
                   << "; }\n";
            stream << "float rc_ambient_max() { return " << format_glsl_float(authoring_config.lighting.ambient_max)
                   << "; }\n";
            stream << "float rc_cavity_broad_strength() { return "
                   << format_glsl_float(authoring_config.lighting.cavity_broad_strength) << "; }\n";
            stream << "float rc_cavity_micro_strength() { return "
                   << format_glsl_float(authoring_config.lighting.cavity_micro_strength) << "; }\n";
            stream << "float rc_cavity_underside_strength() { return "
                   << format_glsl_float(authoring_config.lighting.cavity_underside_strength) << "; }\n";
            stream << "float rc_cavity_ambient_darken() { return "
                   << format_glsl_float(authoring_config.lighting.cavity_ambient_darken) << "; }\n";
            stream << "float rc_local_light_wrap() { return "
                   << format_glsl_float(authoring_config.lighting.local_light_wrap) << "; }\n";
            stream << "float rc_local_light_fill() { return "
                   << format_glsl_float(authoring_config.lighting.local_light_fill) << "; }\n";
            stream << "float rc_local_light_gain() { return "
                   << format_glsl_float(authoring_config.lighting.local_light_gain) << "; }\n";

            return stream.str();
        }
    } // namespace

    Renderer::Renderer(const render::RendererConfig& config) :
        config_(config), camera_(config.camera), imgui_overlay_(config.imgui)
    {
        RELAYCORE_PROFILE_FUNCTION();
    }

    Renderer::~Renderer() noexcept
    {
        release_shadow_resources();
    }

    void Renderer::initialize(const Window& window)
    {
        RELAYCORE_PROFILE_FUNCTION();
        if (initialized_)
        {
            return;
        }

        render::gl::load();
        render::gl_debug::initialize();
        RELAYCORE_PROFILE_GPU_CONTEXT();
        RELAYCORE_PROFILE_GPU_CONTEXT_NAME("RelayCore::Renderer");

        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LEQUAL);
        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);
        glFrontFace(GL_CCW);

        const char* vendor = reinterpret_cast<const char*>(glGetString(GL_VENDOR));
        const char* renderer = reinterpret_cast<const char*>(glGetString(GL_RENDERER));
        const char* version = reinterpret_cast<const char*>(glGetString(GL_VERSION));
        const char* glsl_version = reinterpret_cast<const char*>(glGetString(GL_SHADING_LANGUAGE_VERSION));

        log::info("GL vendor: {}", vendor != nullptr ? vendor : "unknown");
        log::info("GL renderer: {}", renderer != nullptr ? renderer : "unknown");
        log::info("GL version: {}", version != nullptr ? version : "unknown");
        log::info("GLSL version: {}", glsl_version != nullptr ? glsl_version : "unknown");
        log::debug("Asset root: {}", core::asset_root_path().string());

        commit_live_content_reload(prepare_live_content_reload(config_, render::load_default_render_authoring_config())
        );
        camera_.set_viewport(viewport_width_, viewport_height_);
        imgui_overlay_.initialize(window);

        initialized_ = true;
    }

    auto Renderer::prepare_live_content_reload(
        const render::RendererConfig& config,
        const render::RenderAuthoringConfig& authoring_config
    ) -> PreparedLiveContentReload
    {
        RELAYCORE_PROFILE_FUNCTION();
        PreparedLiveContentReload staged_reload{};
        staged_reload.config = config;
        staged_reload.authoring_config = authoring_config;

        const std::string fragment_contract = build_shader_contract_source(authoring_config);
        const render::ShaderFileSourcePatches scene_source_patches{
            .fragment_injected_source = fragment_contract,
        };

        staged_reload.opaque_shader_program.load_from_files(
            core::asset_path("shaders/scene_opaque.vert"),
            core::asset_path("shaders/scene_opaque.frag"),
            scene_source_patches
        );
        staged_reload.overlay_shader_program.load_from_files(
            core::asset_path("shaders/scene_overlay.vert"),
            core::asset_path("shaders/scene_overlay.frag"),
            scene_source_patches
        );
        staged_reload.transparent_shader_program.load_from_files(
            core::asset_path("shaders/scene_transparent.vert"),
            core::asset_path("shaders/scene_transparent.frag"),
            scene_source_patches
        );
        staged_reload.shadow_shader_program.load_from_files(
            core::asset_path("shaders/shadow_depth.vert"),
            core::asset_path("shaders/shadow_depth.frag")
        );

        const std::filesystem::path atlas_path = core::asset_path("textures/blockout_atlas.bmp");
        render::CpuRgbaTextureData atlas{};
        if (const auto loaded_atlas = render::load_blockout_texture_atlas(atlas_path); loaded_atlas.has_value())
        {
            atlas = *loaded_atlas;
            staged_reload.atlas_source_label = atlas_path.string();
        } else
        {
            atlas = render::build_blockout_texture_atlas(authoring_config);
            staged_reload.atlas_source_label = "generated fallback";
        }

        staged_reload.blockout_atlas.initialize_rgba(atlas.width, atlas.height, atlas.pixels);
        return staged_reload;
    }

    void Renderer::commit_live_content_reload(PreparedLiveContentReload staged_reload)
    {
        RELAYCORE_PROFILE_FUNCTION();
        ShadowResources staged_shadow_resources = create_shadow_resources(staged_reload.config);

        opaque_shader_program_ = std::move(staged_reload.opaque_shader_program);
        overlay_shader_program_ = std::move(staged_reload.overlay_shader_program);
        transparent_shader_program_ = std::move(staged_reload.transparent_shader_program);
        shadow_program_ = std::move(staged_reload.shadow_shader_program);
        blockout_atlas_ = std::move(staged_reload.blockout_atlas);
        config_ = std::move(staged_reload.config);
        authoring_config_ = std::move(staged_reload.authoring_config);
        shadow_resources_ = std::move(staged_shadow_resources);

        camera_.apply_projection_config(config_.camera);
        camera_.set_viewport(viewport_width_, viewport_height_);
        imgui_overlay_.apply_config(config_.imgui);
    }

    void Renderer::process_event(const SDL_Event& event)
    {
        RELAYCORE_PROFILE_FUNCTION();
        imgui_overlay_.process_event(event);
    }

    void Renderer::on_resize(const int width, const int height) noexcept
    {
        RELAYCORE_PROFILE_FUNCTION();
        viewport_width_ = width;
        viewport_height_ = height;
        camera_.set_viewport(width, height);
        if (initialized_)
        {
            glViewport(0, 0, width, height);
        }
    }

    void Renderer::begin_frame()
    {
        RELAYCORE_PROFILE_ZONE_N("Renderer::begin_frame");
        RELAYCORE_PROFILE_GPU_COLLECT();
        {
            RELAYCORE_PROFILE_ZONE_N("Renderer::clear");
            RELAYCORE_PROFILE_GPU_ZONE_N("Renderer::clear");
            glClearColor(config_.clear_r, config_.clear_g, config_.clear_b, 1.0F);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        }
    }

    void Renderer::draw_render_queue(const std::span<const render::RenderItem> items)
    {
        RELAYCORE_PROFILE_ZONE_N("Renderer::draw_render_queue");
        if (items.empty())
        {
            return;
        }

        if (config_.shadow_enabled && !shadow_resources_.complete())
        {
            RELAYCORE_PROFILE_FUNCTION();
            initialize_shadow_resources();
        }

        world_diagnostics_.shadow_enabled = config_.shadow_enabled;
        world_diagnostics_.shadow_cascade_count =
            config_.shadow_enabled ? static_cast<std::uint32_t>(kShadowCascadeCount) : 0U;
        world_diagnostics_.indirect_bounce_strength = std::max(config_.indirect_bounce_strength, 0.0F);
        world_diagnostics_.shadow_map_resolution =
            config_.shadow_enabled ? static_cast<std::uint32_t>(std::max(config_.shadow_map_resolution, 256)) : 0U;
        world_diagnostics_.shadow_preview_texture_id =
            config_.shadow_enabled
                ? static_cast<std::uintptr_t>(shadow_resources_.depth_texture_ids[kNearShadowCascadeIndex])
                : 0U;
        world_diagnostics_.shadow_far_preview_texture_id =
            config_.shadow_enabled
                ? static_cast<std::uintptr_t>(shadow_resources_.depth_texture_ids[kFarShadowCascadeIndex])
                : 0U;
        world_diagnostics_.shadow_contact_strength =
            config_.shadow_enabled ? std::max(config_.shadow_contact_strength, 0.0F) : 0.0F;
        world_diagnostics_.shadow_bias_near = config_.shadow_enabled ? std::max(config_.shadow_bias_near, 0.0F) : 0.0F;
        world_diagnostics_.shadow_bias_far = config_.shadow_enabled ? std::max(config_.shadow_bias_far, 0.0F) : 0.0F;
        world_diagnostics_.shadow_normal_offset_near =
            config_.shadow_enabled ? std::max(config_.shadow_normal_offset_near, 0.0F) : 0.0F;
        world_diagnostics_.shadow_normal_offset_far =
            config_.shadow_enabled ? std::max(config_.shadow_normal_offset_far, 0.0F) : 0.0F;
        world_diagnostics_.shadow_pcf_radius_near =
            config_.shadow_enabled ? std::max(config_.shadow_pcf_radius_near, 0.0F) : 0.0F;
        world_diagnostics_.shadow_pcf_radius_far =
            config_.shadow_enabled ? std::max(config_.shadow_pcf_radius_far, 0.0F) : 0.0F;
        world_diagnostics_.shadow_cascade_split_distance =
            config_.shadow_enabled ? std::max(config_.shadow_cascade_split_distance, 0.0F) : 0.0F;
        world_diagnostics_.shadow_cascade_blend_range =
            config_.shadow_enabled ? std::max(config_.shadow_cascade_blend_range, 0.0F) : 0.0F;
        world_diagnostics_.shadow_projection_radius =
            config_.shadow_enabled ? std::max(config_.shadow_projection_radius, 1.0F) : 0.0F;
        world_diagnostics_.shadow_far_projection_radius =
            config_.shadow_enabled ? std::max(config_.shadow_far_projection_radius, 1.0F) : 0.0F;
        world_diagnostics_.shadow_focus_distance =
            config_.shadow_enabled ? std::max(config_.shadow_focus_distance, 0.0F) : 0.0F;
        world_diagnostics_.shadow_far_focus_distance =
            config_.shadow_enabled ? std::max(config_.shadow_far_focus_distance, 0.0F) : 0.0F;
        world_diagnostics_.shadow_texel_world_size =
            config_.shadow_enabled ? (world_diagnostics_.shadow_projection_radius * 2.0F)
                                         / static_cast<float>(std::max(config_.shadow_map_resolution, 256))
                                   : 0.0F;
        world_diagnostics_.shadow_far_texel_world_size =
            config_.shadow_enabled ? (world_diagnostics_.shadow_far_projection_radius * 2.0F)
                                         / static_cast<float>(std::max(config_.shadow_map_resolution, 256))
                                   : 0.0F;

        render_shadow_map(items);

        {
            RELAYCORE_PROFILE_ZONE_N("Renderer::main_scene");
            RELAYCORE_PROFILE_GPU_ZONE_N("Renderer::main_scene");

            constexpr GLint kMvpLocation = 0;
            constexpr GLint kModelLocation = 1;
            const glm::mat4 view_projection = camera_.view_projection_matrix();

            const auto draw_pass_items = [&](const render::RenderPass pass) {
                bool has_items_for_pass = false;
                for (const render::RenderItem& item : items)
                {
                    if (item.mesh != nullptr && item.pass == pass)
                    {
                        has_items_for_pass = true;
                        break;
                    }
                }

                if (!has_items_for_pass)
                {
                    return;
                }

                apply_render_pass_state(pass);
                render::ShaderProgram& program = scene_shader_program(pass);
                bind_scene_program_uniforms(program);
                if (pass == render::RenderPass::Opaque)
                {
                    RELAYCORE_PROFILE_ZONE_N("Renderer::opaque_pass");
                    RELAYCORE_PROFILE_GPU_ZONE_N("Renderer::opaque_pass");
                } else if (pass == render::RenderPass::Overlay)
                {
                    RELAYCORE_PROFILE_ZONE_N("Renderer::overlay_pass");
                    RELAYCORE_PROFILE_GPU_ZONE_N("Renderer::overlay_pass");
                } else
                {
                    RELAYCORE_PROFILE_ZONE_N("Renderer::transparent_pass");
                    RELAYCORE_PROFILE_GPU_ZONE_N("Renderer::transparent_pass");
                }

                for (const render::RenderItem& item : items)
                {
                    if (item.mesh == nullptr || item.pass != pass)
                    {
                        continue;
                    }

                    const glm::mat4 mvp = view_projection * item.model_matrix;
                    program.set_mat4(kMvpLocation, mvp);
                    program.set_mat4(kModelLocation, item.model_matrix);
                    item.mesh->draw();
                }
            };

            {
                RELAYCORE_PROFILE_ZONE_N("Renderer::opaque_passes");
                draw_pass_items(render::RenderPass::Opaque);
                draw_pass_items(render::RenderPass::Overlay);
                draw_pass_items(render::RenderPass::Transparent);
            }

            glDisable(GL_BLEND);
            glDepthMask(GL_TRUE);
            glEnable(GL_CULL_FACE);
        }
    }

    void Renderer::set_local_lights(const std::span<const render::LocalLight> lights)
    {
        local_lights_.assign(lights.begin(), lights.end());
    }

    void Renderer::set_scene_time(const float seconds) noexcept
    {
        scene_time_seconds_ = seconds;
    }

    Renderer::ShadowResources::~ShadowResources() noexcept
    {
        reset();
    }

    Renderer::ShadowResources::ShadowResources(ShadowResources&& other) noexcept :
        framebuffer_ids(std::exchange(other.framebuffer_ids, {})),
        depth_texture_ids(std::exchange(other.depth_texture_ids, {}))
    {}

    auto Renderer::ShadowResources::operator=(ShadowResources&& other) noexcept -> ShadowResources&
    {
        if (this != &other)
        {
            reset();
            framebuffer_ids = std::exchange(other.framebuffer_ids, {});
            depth_texture_ids = std::exchange(other.depth_texture_ids, {});
        }

        return *this;
    }

    void Renderer::ShadowResources::reset() noexcept
    {
        for (std::size_t cascade_index = 0; cascade_index < kShadowCascadeCount; ++cascade_index)
        {
            if (framebuffer_ids[cascade_index] != 0)
            {
                glDeleteFramebuffers(1, &framebuffer_ids[cascade_index]);
                framebuffer_ids[cascade_index] = 0;
            }
            if (depth_texture_ids[cascade_index] != 0)
            {
                glDeleteTextures(1, &depth_texture_ids[cascade_index]);
                depth_texture_ids[cascade_index] = 0;
            }
        }
    }

    auto Renderer::ShadowResources::complete() const noexcept -> bool
    {
        for (std::size_t cascade_index = 0; cascade_index < kShadowCascadeCount; ++cascade_index)
        {
            if (framebuffer_ids[cascade_index] == 0 || depth_texture_ids[cascade_index] == 0)
            {
                return false;
            }
        }

        return true;
    }

    auto Renderer::create_shadow_resources(const render::RendererConfig& config) -> ShadowResources
    {
        RELAYCORE_PROFILE_FUNCTION();
        ShadowResourceBindingGuard binding_guard{};
        ShadowResources resources{};
        if (!config.shadow_enabled)
        {
            return resources;
        }

        const int shadow_map_resolution = std::max(config.shadow_map_resolution, 256);

        for (std::size_t cascade_index = 0; cascade_index < kShadowCascadeCount; ++cascade_index)
        {
            glGenTextures(1, &resources.depth_texture_ids[cascade_index]);
            glBindTexture(GL_TEXTURE_2D, resources.depth_texture_ids[cascade_index]);
            glTexImage2D(
                GL_TEXTURE_2D,
                0,
                GL_DEPTH_COMPONENT24,
                shadow_map_resolution,
                shadow_map_resolution,
                0,
                GL_DEPTH_COMPONENT,
                GL_FLOAT,
                nullptr
            );
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_R, GL_RED);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_G, GL_RED);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_B, GL_RED);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_A, GL_ONE);
            constexpr float border_color[4] = {1.0F, 1.0F, 1.0F, 1.0F};
            glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, border_color);

            glGenFramebuffers(1, &resources.framebuffer_ids[cascade_index]);
            glBindFramebuffer(GL_FRAMEBUFFER, resources.framebuffer_ids[cascade_index]);
            glFramebufferTexture2D(
                GL_FRAMEBUFFER,
                GL_DEPTH_ATTACHMENT,
                GL_TEXTURE_2D,
                resources.depth_texture_ids[cascade_index],
                0
            );
            glDrawBuffer(GL_NONE);
            glReadBuffer(GL_NONE);
            if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
            {
                throw std::runtime_error("Directional shadow framebuffer is incomplete");
            }
        }
        return resources;
    }

    void Renderer::initialize_shadow_resources()
    {
        RELAYCORE_PROFILE_FUNCTION();
        if (!config_.shadow_enabled || shadow_resources_.complete())
        {
            return;
        }

        shadow_resources_ = create_shadow_resources(config_);
    }

    void Renderer::release_shadow_resources() noexcept
    {
        RELAYCORE_PROFILE_FUNCTION();
        shadow_resources_.reset();
    }

    void Renderer::render_shadow_map(const std::span<const render::RenderItem> items)
    {
        RELAYCORE_PROFILE_FUNCTION();
        if (!config_.shadow_enabled || items.empty())
        {
            return;
        }
        if (!shadow_resources_.complete())
        {
            return;
        }

        RELAYCORE_PROFILE_ZONE_N("Renderer::render_shadow_map");
        RELAYCORE_PROFILE_GPU_ZONE_N("Renderer::render_shadow_map");

        shadow_light_view_projections_[kNearShadowCascadeIndex] = build_shadow_light_view_projection(
            config_.shadow_focus_distance,
            config_.shadow_projection_radius,
            config_.shadow_projection_depth
        );
        shadow_light_view_projections_[kFarShadowCascadeIndex] = build_shadow_light_view_projection(
            config_.shadow_far_focus_distance,
            config_.shadow_far_projection_radius,
            config_.shadow_far_projection_depth
        );

        const int shadow_map_resolution = std::max(config_.shadow_map_resolution, 256);
        glCullFace(GL_FRONT);
        glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
        shadow_program_.bind();
        constexpr GLint kShadowMvpLocation = 0;
        for (std::size_t cascade_index = 0; cascade_index < kShadowCascadeCount; ++cascade_index)
        {
            glBindFramebuffer(GL_FRAMEBUFFER, shadow_resources_.framebuffer_ids[cascade_index]);
            glViewport(0, 0, shadow_map_resolution, shadow_map_resolution);
            glClear(GL_DEPTH_BUFFER_BIT);

            for (const render::RenderItem& item : items)
            {
                if (item.mesh == nullptr || item.pass != render::RenderPass::Opaque)
                {
                    continue;
                }

                shadow_program_.set_mat4(
                    kShadowMvpLocation,
                    shadow_light_view_projections_[cascade_index] * item.model_matrix
                );
                item.mesh->draw();
            }
        }

        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
        glCullFace(GL_BACK);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, viewport_width_, viewport_height_);
    }

    auto Renderer::build_shadow_light_view_projection(const float focus_distance, const float radius, const float depth)
        const -> glm::mat4
    {
        RELAYCORE_PROFILE_FUNCTION();
        const glm::vec3 light_direction = safe_shadow_direction(config_.shadow_light_direction);
        const float cascade_radius = std::max(radius, 1.0F);
        const float cascade_depth = std::max(depth, cascade_radius + 1.0F);
        const float texel_world_size =
            (cascade_radius * 2.0F) / static_cast<float>(std::max(config_.shadow_map_resolution, 256));
        const glm::vec3 focus_point = camera_.position() + (camera_.forward() * std::max(focus_distance, 0.0F));
        const glm::vec3 light_position = focus_point - (light_direction * cascade_depth * 0.5F);
        glm::mat4 light_view = glm::lookAt(light_position, focus_point, choose_shadow_up(light_direction));
        const glm::vec4 focus_in_light_space = light_view * glm::vec4(focus_point, 1.0F);
        const float snapped_x = round_to_shadow_texel(focus_in_light_space.x, texel_world_size);
        const float snapped_y = round_to_shadow_texel(focus_in_light_space.y, texel_world_size);
        const glm::vec3 snap_delta{
            snapped_x - focus_in_light_space.x,
            snapped_y - focus_in_light_space.y,
            0.0F,
        };
        light_view = glm::translate(glm::mat4{1.0F}, snap_delta) * light_view;
        const glm::mat4 light_projection =
            glm::ortho(-cascade_radius, cascade_radius, -cascade_radius, cascade_radius, 0.5F, cascade_depth);
        return light_projection * light_view;
    }

    auto Renderer::scene_shader_program(const render::RenderPass pass) noexcept -> render::ShaderProgram&
    {
        switch (pass)
        {
            case render::RenderPass::Opaque:
                return opaque_shader_program_;
            case render::RenderPass::Overlay:
                return overlay_shader_program_;
            case render::RenderPass::Transparent:
                return transparent_shader_program_;
        }

        return opaque_shader_program_;
    }

    auto Renderer::scene_shader_program(const render::RenderPass pass) const noexcept -> const render::ShaderProgram&
    {
        switch (pass)
        {
            case render::RenderPass::Opaque:
                return opaque_shader_program_;
            case render::RenderPass::Overlay:
                return overlay_shader_program_;
            case render::RenderPass::Transparent:
                return transparent_shader_program_;
        }

        return opaque_shader_program_;
    }

    void Renderer::bind_scene_program_uniforms(render::ShaderProgram& program) const
    {
        RELAYCORE_PROFILE_FUNCTION();
        constexpr GLint kCameraPositionLocation = 2;
        constexpr GLint kFogColorLocation = 3;
        constexpr GLint kFogStartLocation = 4;
        constexpr GLint kFogEndLocation = 5;
        constexpr GLint kLocalLightCountLocation = 6;
        constexpr GLint kLocalLightPositionBaseLocation = 7;
        constexpr GLint kLocalLightColorBaseLocation = 15;
        constexpr GLint kLocalLightRadiusBaseLocation = 23;
        constexpr GLint kSceneTimeLocation = 31;
        constexpr GLint kBlockoutAtlasLocation = 32;
        constexpr GLint kViewportWidthLocation = 33;
        constexpr GLint kViewportHeightLocation = 34;
        constexpr GLint kPostfxVignetteLocation = 35;
        constexpr GLint kPostfxGrainLocation = 36;
        constexpr GLint kPostfxScanlineLocation = 37;
        constexpr GLint kPostfxContrastLocation = 38;
        constexpr GLint kPostfxSaturationLocation = 39;
        constexpr GLint kShadowEnabledLocation = 40;
        constexpr GLint kShadowNearLightViewProjectionLocation = 41;
        constexpr GLint kShadowLightDirectionLocation = 45;
        constexpr GLint kShadowLightColorLocation = 46;
        constexpr GLint kShadowLightIntensityLocation = 47;
        constexpr GLint kShadowNearMapLocation = 48;
        constexpr GLint kShadowStrengthLocation = 49;
        constexpr GLint kShadowNearBiasLocation = 50;
        constexpr GLint kShadowSlopeBiasScaleLocation = 51;
        constexpr GLint kShadowNearNormalOffsetLocation = 52;
        constexpr GLint kShadowFarEnabledLocation = 53;
        constexpr GLint kShadowFarMapLocation = 54;
        constexpr GLint kShadowCascadeSplitDistanceLocation = 55;
        constexpr GLint kShadowFarLightViewProjectionLocation = 56;
        constexpr GLint kShadowCascadeBlendRangeLocation = 60;
        constexpr GLint kShadowNearPcfRadiusLocation = 61;
        constexpr GLint kShadowFarPcfRadiusLocation = 62;
        constexpr GLint kShadowContactStrengthLocation = 63;
        constexpr GLint kIndirectBounceStrengthUniformLocation = 64;
        constexpr GLint kShadowDebugCascadesUniformLocation = 65;
        constexpr GLint kShadowFarBiasLocation = 66;
        constexpr GLint kShadowFarNormalOffsetLocation = 67;
        constexpr std::size_t kMaxLocalLights = 8;

        program.bind();
        program.set_vec3(kCameraPositionLocation, camera_.position());
        program.set_vec3(kFogColorLocation, config_.fog_color);
        program.set_float(kFogStartLocation, config_.fog_enabled ? config_.fog_start : config_.camera.far_plane);
        program.set_float(kFogEndLocation, config_.fog_enabled ? config_.fog_end : config_.camera.far_plane);
        program.set_int(
            kLocalLightCountLocation,
            static_cast<int>(std::min<std::size_t>(local_lights_.size(), kMaxLocalLights))
        );
        program.set_float(kSceneTimeLocation, scene_time_seconds_);
        program.set_float(kViewportWidthLocation, static_cast<float>(viewport_width_));
        program.set_float(kViewportHeightLocation, static_cast<float>(viewport_height_));
        program.set_float(kIndirectBounceStrengthUniformLocation, std::max(config_.indirect_bounce_strength, 0.0F));
        program.set_float(kPostfxVignetteLocation, config_.postfx_vignette_strength);
        program.set_float(kPostfxGrainLocation, config_.postfx_grain_strength);
        program.set_float(kPostfxScanlineLocation, config_.postfx_scanline_strength);
        program.set_float(kPostfxContrastLocation, config_.postfx_contrast);
        program.set_float(kPostfxSaturationLocation, config_.postfx_saturation);
        program.set_int(
            kShadowEnabledLocation,
            config_.shadow_enabled && shadow_resources_.depth_texture_ids[kNearShadowCascadeIndex] != 0 ? 1 : 0
        );
        program.set_mat4(
            kShadowNearLightViewProjectionLocation,
            shadow_light_view_projections_[kNearShadowCascadeIndex]
        );
        program.set_vec3(kShadowLightDirectionLocation, safe_shadow_direction(config_.shadow_light_direction));
        program.set_vec3(kShadowLightColorLocation, config_.shadow_light_color);
        program.set_float(kShadowLightIntensityLocation, config_.shadow_light_intensity);
        program.set_float(kShadowStrengthLocation, config_.shadow_strength);
        program.set_float(kShadowNearBiasLocation, config_.shadow_bias_near);
        program.set_float(kShadowSlopeBiasScaleLocation, config_.shadow_slope_bias_scale);
        program.set_float(kShadowNearNormalOffsetLocation, config_.shadow_normal_offset_near);
        program.set_int(
            kShadowFarEnabledLocation,
            config_.shadow_enabled && shadow_resources_.depth_texture_ids[kFarShadowCascadeIndex] != 0 ? 1 : 0
        );
        program.set_float(kShadowCascadeSplitDistanceLocation, std::max(config_.shadow_cascade_split_distance, 0.0F));
        program.set_float(kShadowCascadeBlendRangeLocation, std::max(config_.shadow_cascade_blend_range, 0.0F));
        program.set_float(kShadowNearPcfRadiusLocation, std::max(config_.shadow_pcf_radius_near, 0.0F));
        program.set_float(kShadowFarPcfRadiusLocation, std::max(config_.shadow_pcf_radius_far, 0.0F));
        program.set_float(kShadowContactStrengthLocation, std::max(config_.shadow_contact_strength, 0.0F));
        program.set_int(kShadowDebugCascadesUniformLocation, config_.shadow_debug_cascades ? 1 : 0);
        program.set_float(kShadowFarBiasLocation, config_.shadow_bias_far);
        program.set_float(kShadowFarNormalOffsetLocation, config_.shadow_normal_offset_far);
        program.set_mat4(kShadowFarLightViewProjectionLocation, shadow_light_view_projections_[kFarShadowCascadeIndex]);

        if (blockout_atlas_.valid())
        {
            blockout_atlas_.bind(0);
            program.set_int(kBlockoutAtlasLocation, 0);
        }
        if (shadow_resources_.depth_texture_ids[kNearShadowCascadeIndex] != 0)
        {
            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, shadow_resources_.depth_texture_ids[kNearShadowCascadeIndex]);
            program.set_int(kShadowNearMapLocation, 1);
        }
        if (shadow_resources_.depth_texture_ids[kFarShadowCascadeIndex] != 0)
        {
            glActiveTexture(GL_TEXTURE2);
            glBindTexture(GL_TEXTURE_2D, shadow_resources_.depth_texture_ids[kFarShadowCascadeIndex]);
            program.set_int(kShadowFarMapLocation, 2);
        }
        glActiveTexture(GL_TEXTURE0);

        for (std::size_t light_index = 0; light_index < std::min<std::size_t>(local_lights_.size(), kMaxLocalLights);
             ++light_index)
        {
            const render::LocalLight& light = local_lights_[light_index];
            program.set_vec3(kLocalLightPositionBaseLocation + static_cast<int>(light_index), light.position);
            program.set_vec3(kLocalLightColorBaseLocation + static_cast<int>(light_index), light.color);
            program.set_float(kLocalLightRadiusBaseLocation + static_cast<int>(light_index), light.radius);
        }
    }

    void Renderer::draw_ui(const FrameInfo& frame, const AudioSystem& audio_system)
    {
        RELAYCORE_PROFILE_FUNCTION();
        if (!imgui_overlay_.enabled())
        {
            return;
        }

        imgui_overlay_.update_diagnostics(frame);
        const bool show_shadow_debug_banner = config_.shadow_debug_cascades;

        if (!imgui_overlay_.should_render() && !game_hud_.should_render(player_diagnostics_, interaction_diagnostics_)
            && !show_shadow_debug_banner)
        {
            return;
        }
        {
            RELAYCORE_PROFILE_ZONE_N("imgui.begin_frame");
            imgui_overlay_.begin_frame();
        }
        if (imgui_overlay_.diagnostics_visible())
        {
            RELAYCORE_PROFILE_ZONE_N("imgui.draw_diagnostics");
            imgui_overlay_.draw_diagnostics(
                frame,
                camera_,
                audio_system,
                config_,
                world_diagnostics_,
                runtime_diagnostics_,
                player_diagnostics_,
                interaction_diagnostics_,
                inspector_diagnostics_,
                audio_diagnostics_,
                iteration_diagnostics_
            );
        }
        if (game_hud_.should_render(player_diagnostics_, interaction_diagnostics_))
        {
            RELAYCORE_PROFILE_ZONE_N("imgui.draw_game_hud");
            game_hud_.draw(player_diagnostics_, interaction_diagnostics_);
        }
        if (show_shadow_debug_banner)
        {
            RELAYCORE_PROFILE_ZONE_N("imgui.draw_shadow_debug_banner");
            draw_shadow_debug_banner();
        }
        {
            RELAYCORE_PROFILE_ZONE_N("imgui.render");
            RELAYCORE_PROFILE_GPU_ZONE_N("Renderer::ui");
            imgui_overlay_.render();
        }
    }

    void Renderer::set_world_diagnostics(const render::WorldDiagnostics& diagnostics) noexcept
    {
        world_diagnostics_ = diagnostics;
    }

    auto Renderer::world_diagnostics() const noexcept -> const render::WorldDiagnostics&
    {
        return world_diagnostics_;
    }

    void Renderer::set_runtime_diagnostics(const render::RuntimeDiagnostics& diagnostics) noexcept
    {
        runtime_diagnostics_ = diagnostics;
    }

    auto Renderer::runtime_diagnostics() const noexcept -> const render::RuntimeDiagnostics&
    {
        return runtime_diagnostics_;
    }

    void Renderer::set_player_diagnostics(const render::PlayerDiagnostics& diagnostics) noexcept
    {
        player_diagnostics_ = diagnostics;
    }

    auto Renderer::player_diagnostics() const noexcept -> const render::PlayerDiagnostics&
    {
        return player_diagnostics_;
    }

    void Renderer::set_interaction_diagnostics(const render::InteractionDiagnostics& diagnostics) noexcept
    {
        interaction_diagnostics_ = diagnostics;
    }

    auto Renderer::interaction_diagnostics() const noexcept -> const render::InteractionDiagnostics&
    {
        return interaction_diagnostics_;
    }

    void Renderer::set_inspector_diagnostics(const render::InspectorDiagnostics& diagnostics) noexcept
    {
        inspector_diagnostics_ = diagnostics;
    }

    auto Renderer::inspector_diagnostics() const noexcept -> const render::InspectorDiagnostics&
    {
        return inspector_diagnostics_;
    }

    void Renderer::set_audio_diagnostics(const render::AudioDiagnostics& diagnostics) noexcept
    {
        audio_diagnostics_ = diagnostics;
    }

    auto Renderer::audio_diagnostics() const noexcept -> const render::AudioDiagnostics&
    {
        return audio_diagnostics_;
    }

    void Renderer::set_iteration_diagnostics(const render::IterationDiagnostics& diagnostics) noexcept
    {
        iteration_diagnostics_ = diagnostics;
    }

    auto Renderer::iteration_diagnostics() const noexcept -> const render::IterationDiagnostics&
    {
        return iteration_diagnostics_;
    }

    auto Renderer::authoring_config() const noexcept -> const render::RenderAuthoringConfig&
    {
        return authoring_config_;
    }

    auto Renderer::camera() noexcept -> render::PerspectiveCamera&
    {
        return camera_;
    }

    auto Renderer::camera() const noexcept -> const render::PerspectiveCamera&
    {
        return camera_;
    }

    auto Renderer::diagnostics_visible() const noexcept -> bool
    {
        return imgui_overlay_.diagnostics_visible();
    }

    void Renderer::set_diagnostics_visible(const bool visible) noexcept
    {
        imgui_overlay_.set_diagnostics_visible(visible);
    }

    void Renderer::toggle_diagnostics_visible() noexcept
    {
        imgui_overlay_.toggle_diagnostics_visible();
    }

    auto Renderer::advanced_diagnostics_visible() const noexcept -> bool
    {
        return imgui_overlay_.advanced_diagnostics_visible();
    }

    void Renderer::set_advanced_diagnostics_visible(const bool visible) noexcept
    {
        imgui_overlay_.set_advanced_diagnostics_visible(visible);
    }

    void Renderer::toggle_advanced_diagnostics_visible() noexcept
    {
        imgui_overlay_.toggle_advanced_diagnostics_visible();
    }

    auto Renderer::inspector_visualization_enabled() const noexcept -> bool
    {
        return inspector_visualization_enabled_;
    }

    void Renderer::set_inspector_visualization_enabled(const bool enabled) noexcept
    {
        inspector_visualization_enabled_ = enabled;
    }

    void Renderer::toggle_inspector_visualization_enabled() noexcept
    {
        inspector_visualization_enabled_ = !inspector_visualization_enabled_;
    }

    auto Renderer::consume_iteration_command_requests() noexcept -> IterationCommandRequests
    {
        const debug::ImGuiOverlay::IterationCommandRequests overlay_requests =
            imgui_overlay_.consume_iteration_command_requests();
        IterationCommandRequests requests{};
        requests.save_runtime_snapshot = overlay_requests.save_runtime_snapshot;
        requests.restore_runtime_snapshot = overlay_requests.restore_runtime_snapshot;
        requests.open_first_authoring_warning = overlay_requests.open_first_authoring_warning;
        requests.dump_authoring_warnings = overlay_requests.dump_authoring_warnings;
        requests.open_first_runtime_registry = overlay_requests.open_first_runtime_registry;
        requests.dump_runtime_registry = overlay_requests.dump_runtime_registry;
        requests.open_first_runtime_binding = overlay_requests.open_first_runtime_binding;
        requests.dump_runtime_bindings = overlay_requests.dump_runtime_bindings;
        requests.dump_render_authoring_contract = overlay_requests.dump_render_authoring_contract;
        requests.authoring_warning_source_filter = overlay_requests.authoring_warning_source_filter;
        requests.runtime_registry_source_filter = overlay_requests.runtime_registry_source_filter;
        requests.runtime_binding_source_filter = overlay_requests.runtime_binding_source_filter;
        return requests;
    }
} // namespace RelayCore
