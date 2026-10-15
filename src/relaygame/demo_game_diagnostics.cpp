#include "relaycore/app/engine_services.h"
#include "relaycore/core/log.h"
#include "relaycore/platform/window.h"
#include "relaycore/render/renderer.h"
#include "relaygame/demo_game.h"

namespace RelayGame
{
    void DemoGame::update_frame_context(RelayCore::EngineServices& services)
    {
        const char* mode = screen_ == Screen::Playing    ? "game"
                           : screen_ == Screen::Paused   ? "pause"
                           : screen_ == Screen::MainMenu ? "menu"
                           : screen_ == Screen::Editor   ? "editor"
                           : screen_ == Screen::Finale   ? "finale"
                                                         : "complete";
        services.frame_diagnostics().set_context(
            mode,
            editor_.active() ? editor_.save_path().string() : runtime_.level_path()
        );
    }

    void DemoGame::log_effective_settings(RelayCore::EngineServices& services)
    {
        const auto& renderer = services.renderer();
        const auto& config = renderer.config();
        const auto extent = services.window().pixel_extent();
        PlayerSettings effective{};
        effective.vsync = services.window().vsync_enabled();
        effective.hdr_enabled = config.hdr_enabled;
        effective.bloom_enabled = config.bloom_enabled;
        effective.ssao_enabled = config.ssao_enabled;
        effective.ssr_enabled = config.ssr_enabled;
        effective.volumetrics_enabled = config.volumetrics_enabled;
        effective.ssgi_quality = config.ssgi_quality;
        effective.reflection_quality = config.reflection_quality;
        effective.dof_quality = config.dof_quality;
        effective.motion_blur_quality = config.motion_blur_quality;
        effective.taa_enabled = config.taa_enabled;
        effective.render_scale = config.render_scale;
        effective.dynamic_resolution_enabled = config.dynamic_resolution_enabled;
        if (logged_settings_ && *logged_settings_ == effective && logged_width_ == extent.width
            && logged_height_ == extent.height && logged_pipeline_ == renderer.color_pipeline_mode()
            && logged_presentation_mode_ == renderer.presentation_mode_name())
            return;
        const auto preset = graphics_preset(effective);
        const char* preset_name = preset == GraphicsPreset::Low        ? "low"
                                  : preset == GraphicsPreset::Balanced ? "balanced"
                                  : preset == GraphicsPreset::High     ? "high"
                                                                       : "custom";
        RelayCore::log::info(
            "[renderer] Effective settings {}: pixels={}x{} vsync_requested={} present_mode={} preset={} hdr_scene={} "
            "output=SDR "
            "render_scale={:.3f} dynamic_resolution={} color_pipeline={} validation={} "
            "bloom={} ssao={} ssr={} volumetrics={} ssgi={} reflections={} dof={} motion_blur={} taa={}",
            logged_settings_ ? "changed" : "session",
            extent.width,
            extent.height,
            effective.vsync,
            renderer.presentation_mode_name(),
            preset_name,
            effective.hdr_enabled,
            effective.render_scale,
            effective.dynamic_resolution_enabled,
            RelayCore::render::color_pipeline_mode_name(renderer.color_pipeline_mode()),
            renderer.validation_enabled(),
            effective.bloom_enabled,
            effective.ssao_enabled,
            effective.ssr_enabled,
            effective.volumetrics_enabled,
            effective.ssgi_quality,
            effective.reflection_quality,
            effective.dof_quality,
            effective.motion_blur_quality,
            effective.taa_enabled
        );
        logged_settings_ = effective;
        logged_width_ = extent.width;
        logged_height_ = extent.height;
        logged_pipeline_ = renderer.color_pipeline_mode();
        logged_presentation_mode_ = renderer.presentation_mode_name();
    }
} // namespace RelayGame
