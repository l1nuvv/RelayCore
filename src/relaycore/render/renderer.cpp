#include "relaycore/render/renderer.h"

#include "relaycore/audio/audio_system.h"
#include "relaycore/core/log.h"
#include "relaycore/core/paths.h"
#include "relaycore/core/profiling.h"
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
#include <stdexcept>

namespace RelayCore
{
    namespace
    {
        constexpr std::size_t kNearShadowCascadeIndex = 0;
        constexpr std::size_t kFarShadowCascadeIndex = 1;

        [[nodiscard]] auto choose_shadow_up(const glm::vec3& light_direction) -> glm::vec3
        {
            const glm::vec3 world_up{0.0F, 1.0F, 0.0F};
            if (std::abs(glm::dot(light_direction, world_up)) > 0.98F)
            {
                return {0.0F, 0.0F, 1.0F};
            }
            return world_up;
        }

        [[nodiscard]] auto round_to_shadow_texel(const float value, const float texel_world_size) -> float
        {
            if (texel_world_size <= 0.000001F)
            {
                return value;
            }

            return std::round(value / texel_world_size) * texel_world_size;
        }

        [[nodiscard]] auto safe_shadow_direction(const glm::vec3& direction) -> glm::vec3
        {
            const float length = glm::length(direction);
            if (length <= 0.0001F)
            {
                return glm::normalize(glm::vec3{0.28F, -1.0F, 0.22F});
            }

            return direction / length;
        }

        void draw_shadow_debug_banner()
        {
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
    } // namespace

    Renderer::Renderer(const render::RendererConfig& config) :
        config_(config), camera_(config.camera), imgui_overlay_(config.imgui)
    {}

    void Renderer::initialize(const Window& window)
    {
        if (initialized_)
        {
            return;
        }

        render::gl::load();
        render::gl_debug::initialize();

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

        shader_program_.load_from_files(core::asset_path("shaders/basic.vert"), core::asset_path("shaders/basic.frag"));
        shadow_program_.load_from_files(
            core::asset_path("shaders/shadow_depth.vert"),
            core::asset_path("shaders/shadow_depth.frag")
        );
        const render::CpuRgbaTextureData atlas = render::build_blockout_texture_atlas();
        blockout_atlas_.initialize_rgba(atlas.width, atlas.height, atlas.pixels);
        initialize_shadow_resources();
        camera_.set_viewport(viewport_width_, viewport_height_);
        imgui_overlay_.initialize(window);

        initialized_ = true;
    }

    void Renderer::process_event(const SDL_Event& event)
    {
        imgui_overlay_.process_event(event);
    }

    void Renderer::on_resize(const int width, const int height) noexcept
    {
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
        {
            RELAYCORE_PROFILE_ZONE_N("glClearColor");
            glClearColor(config_.clear_r, config_.clear_g, config_.clear_b, 1.0F);
        }
        {
            RELAYCORE_PROFILE_ZONE_N("glClear");
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

        if (config_.shadow_enabled
            && (shadow_depth_texture_ids_[kNearShadowCascadeIndex] == 0
                || shadow_framebuffer_ids_[kNearShadowCascadeIndex] == 0
                || shadow_depth_texture_ids_[kFarShadowCascadeIndex] == 0
                || shadow_framebuffer_ids_[kFarShadowCascadeIndex] == 0))
        {
            initialize_shadow_resources();
        }

        world_diagnostics_.shadow_enabled = config_.shadow_enabled;
        world_diagnostics_.shadow_cascade_count =
            config_.shadow_enabled ? static_cast<std::uint32_t>(kShadowCascadeCount) : 0U;
        world_diagnostics_.indirect_bounce_strength = std::max(config_.indirect_bounce_strength, 0.0F);
        world_diagnostics_.shadow_map_resolution =
            config_.shadow_enabled ? static_cast<std::uint32_t>(std::max(config_.shadow_map_resolution, 256)) : 0U;
        world_diagnostics_.shadow_preview_texture_id =
            config_.shadow_enabled ? static_cast<std::uintptr_t>(shadow_depth_texture_ids_[kNearShadowCascadeIndex])
                                   : 0U;
        world_diagnostics_.shadow_far_preview_texture_id =
            config_.shadow_enabled ? static_cast<std::uintptr_t>(shadow_depth_texture_ids_[kFarShadowCascadeIndex])
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

        shader_program_.bind();
        constexpr GLint kMvpLocation = 0;
        constexpr GLint kModelLocation = 1;
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
        const glm::mat4 view_projection = camera_.view_projection_matrix();
        shader_program_.set_vec3(kCameraPositionLocation, camera_.position());
        shader_program_.set_vec3(kFogColorLocation, config_.fog_color);
        shader_program_.set_float(
            kFogStartLocation,
            config_.fog_enabled ? config_.fog_start : config_.camera.far_plane
        );
        shader_program_.set_float(kFogEndLocation, config_.fog_enabled ? config_.fog_end : config_.camera.far_plane);
        shader_program_.set_int(
            kLocalLightCountLocation,
            static_cast<int>(std::min<std::size_t>(local_lights_.size(), kMaxLocalLights))
        );
        shader_program_.set_float(kSceneTimeLocation, scene_time_seconds_);
        shader_program_.set_float(kViewportWidthLocation, static_cast<float>(viewport_width_));
        shader_program_.set_float(kViewportHeightLocation, static_cast<float>(viewport_height_));
        shader_program_.set_float(
            kIndirectBounceStrengthUniformLocation,
            std::max(config_.indirect_bounce_strength, 0.0F)
        );
        shader_program_.set_float(kPostfxVignetteLocation, config_.postfx_vignette_strength);
        shader_program_.set_float(kPostfxGrainLocation, config_.postfx_grain_strength);
        shader_program_.set_float(kPostfxScanlineLocation, config_.postfx_scanline_strength);
        shader_program_.set_float(kPostfxContrastLocation, config_.postfx_contrast);
        shader_program_.set_float(kPostfxSaturationLocation, config_.postfx_saturation);
        shader_program_.set_int(
            kShadowEnabledLocation,
            config_.shadow_enabled && shadow_depth_texture_ids_[kNearShadowCascadeIndex] != 0 ? 1 : 0
        );
        shader_program_.set_mat4(
            kShadowNearLightViewProjectionLocation,
            shadow_light_view_projections_[kNearShadowCascadeIndex]
        );
        shader_program_.set_vec3(kShadowLightDirectionLocation, safe_shadow_direction(config_.shadow_light_direction));
        shader_program_.set_vec3(kShadowLightColorLocation, config_.shadow_light_color);
        shader_program_.set_float(kShadowLightIntensityLocation, config_.shadow_light_intensity);
        shader_program_.set_float(kShadowStrengthLocation, config_.shadow_strength);
        shader_program_.set_float(kShadowNearBiasLocation, config_.shadow_bias_near);
        shader_program_.set_float(kShadowSlopeBiasScaleLocation, config_.shadow_slope_bias_scale);
        shader_program_.set_float(kShadowNearNormalOffsetLocation, config_.shadow_normal_offset_near);
        shader_program_.set_int(
            kShadowFarEnabledLocation,
            config_.shadow_enabled && shadow_depth_texture_ids_[kFarShadowCascadeIndex] != 0 ? 1 : 0
        );
        shader_program_.set_float(
            kShadowCascadeSplitDistanceLocation,
            std::max(config_.shadow_cascade_split_distance, 0.0F)
        );
        shader_program_.set_float(kShadowCascadeBlendRangeLocation, std::max(config_.shadow_cascade_blend_range, 0.0F));
        shader_program_.set_float(kShadowNearPcfRadiusLocation, std::max(config_.shadow_pcf_radius_near, 0.0F));
        shader_program_.set_float(kShadowFarPcfRadiusLocation, std::max(config_.shadow_pcf_radius_far, 0.0F));
        shader_program_.set_float(kShadowContactStrengthLocation, std::max(config_.shadow_contact_strength, 0.0F));
        shader_program_.set_int(kShadowDebugCascadesUniformLocation, config_.shadow_debug_cascades ? 1 : 0);
        shader_program_.set_float(kShadowFarBiasLocation, config_.shadow_bias_far);
        shader_program_.set_float(kShadowFarNormalOffsetLocation, config_.shadow_normal_offset_far);
        shader_program_.set_mat4(
            kShadowFarLightViewProjectionLocation,
            shadow_light_view_projections_[kFarShadowCascadeIndex]
        );
        if (blockout_atlas_.valid())
        {
            blockout_atlas_.bind(0);
            shader_program_.set_int(kBlockoutAtlasLocation, 0);
        }
        if (shadow_depth_texture_ids_[kNearShadowCascadeIndex] != 0)
        {
            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, shadow_depth_texture_ids_[kNearShadowCascadeIndex]);
            shader_program_.set_int(kShadowNearMapLocation, 1);
        }
        if (shadow_depth_texture_ids_[kFarShadowCascadeIndex] != 0)
        {
            glActiveTexture(GL_TEXTURE2);
            glBindTexture(GL_TEXTURE_2D, shadow_depth_texture_ids_[kFarShadowCascadeIndex]);
            shader_program_.set_int(kShadowFarMapLocation, 2);
        }

        for (std::size_t light_index = 0; light_index < std::min<std::size_t>(local_lights_.size(), kMaxLocalLights);
             ++light_index)
        {
            const render::LocalLight& light = local_lights_[light_index];
            shader_program_.set_vec3(kLocalLightPositionBaseLocation + static_cast<int>(light_index), light.position);
            shader_program_.set_vec3(kLocalLightColorBaseLocation + static_cast<int>(light_index), light.color);
            shader_program_.set_float(kLocalLightRadiusBaseLocation + static_cast<int>(light_index), light.radius);
        }

        for (const render::RenderItem& item : items)
        {
            if (item.mesh == nullptr)
            {
                continue;
            }

            if (item.transparent)
            {
                glEnable(GL_BLEND);
                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                glDepthMask(GL_FALSE);
                glDisable(GL_CULL_FACE);
            } else
            {
                glDisable(GL_BLEND);
                glDepthMask(GL_TRUE);
                glEnable(GL_CULL_FACE);
            }

            const glm::mat4 mvp = view_projection * item.model_matrix;
            shader_program_.set_mat4(kMvpLocation, mvp);
            shader_program_.set_mat4(kModelLocation, item.model_matrix);
            item.mesh->draw();
        }

        glDisable(GL_BLEND);
        glDepthMask(GL_TRUE);
        glEnable(GL_CULL_FACE);
    }

    void Renderer::set_local_lights(const std::span<const render::LocalLight> lights)
    {
        local_lights_.assign(lights.begin(), lights.end());
    }

    void Renderer::set_scene_time(const float seconds) noexcept
    {
        scene_time_seconds_ = seconds;
    }

    void Renderer::initialize_shadow_resources()
    {
        if (!config_.shadow_enabled)
        {
            return;
        }

        const int shadow_map_resolution = std::max(config_.shadow_map_resolution, 256);

        for (std::size_t cascade_index = 0; cascade_index < kShadowCascadeCount; ++cascade_index)
        {
            if (shadow_depth_texture_ids_[cascade_index] != 0 || shadow_framebuffer_ids_[cascade_index] != 0)
            {
                continue;
            }

            glGenTextures(1, &shadow_depth_texture_ids_[cascade_index]);
            glBindTexture(GL_TEXTURE_2D, shadow_depth_texture_ids_[cascade_index]);
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

            glGenFramebuffers(1, &shadow_framebuffer_ids_[cascade_index]);
            glBindFramebuffer(GL_FRAMEBUFFER, shadow_framebuffer_ids_[cascade_index]);
            glFramebufferTexture2D(
                GL_FRAMEBUFFER,
                GL_DEPTH_ATTACHMENT,
                GL_TEXTURE_2D,
                shadow_depth_texture_ids_[cascade_index],
                0
            );
            glDrawBuffer(GL_NONE);
            glReadBuffer(GL_NONE);
            if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
            {
                throw std::runtime_error("Directional shadow framebuffer is incomplete");
            }
        }
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    void Renderer::render_shadow_map(const std::span<const render::RenderItem> items)
    {
        if (!config_.shadow_enabled || items.empty())
        {
            return;
        }
        if (shadow_framebuffer_ids_[kNearShadowCascadeIndex] == 0
            || shadow_depth_texture_ids_[kNearShadowCascadeIndex] == 0
            || shadow_framebuffer_ids_[kFarShadowCascadeIndex] == 0
            || shadow_depth_texture_ids_[kFarShadowCascadeIndex] == 0)
        {
            return;
        }

        RELAYCORE_PROFILE_ZONE_N("Renderer::render_shadow_map");

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
            glBindFramebuffer(GL_FRAMEBUFFER, shadow_framebuffer_ids_[cascade_index]);
            glViewport(0, 0, shadow_map_resolution, shadow_map_resolution);
            glClear(GL_DEPTH_BUFFER_BIT);

            for (const render::RenderItem& item : items)
            {
                if (item.mesh == nullptr || item.transparent)
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

    auto Renderer::build_shadow_light_view_projection(
        const float focus_distance,
        const float radius,
        const float depth
    ) const -> glm::mat4
    {
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

    void Renderer::draw_ui(const FrameInfo& frame, const AudioSystem& audio_system)
    {
        if (!imgui_overlay_.enabled())
        {
            return;
        }

        imgui_overlay_.update_diagnostics(frame);
        const bool show_shadow_debug_banner = config_.shadow_debug_cascades;

        if (!imgui_overlay_.should_render() && !game_hud_.should_render(interaction_diagnostics_)
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
                player_diagnostics_,
                interaction_diagnostics_
            );
        }
        if (game_hud_.should_render(interaction_diagnostics_))
        {
            RELAYCORE_PROFILE_ZONE_N("imgui.draw_game_hud");
            game_hud_.draw(interaction_diagnostics_);
        }
        if (show_shadow_debug_banner)
        {
            RELAYCORE_PROFILE_ZONE_N("imgui.draw_shadow_debug_banner");
            draw_shadow_debug_banner();
        }
        {
            RELAYCORE_PROFILE_ZONE_N("imgui.render");
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
} // namespace RelayCore
