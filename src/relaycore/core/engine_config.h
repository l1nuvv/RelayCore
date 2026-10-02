/**
 * @file src/relaycore/core/engine_config.h
 * @brief Engine configuration aggregates and loading helpers.
 */

#pragma once

#include "relaycore/audio/audio_system.h"
#include "relaycore/debug/imgui_overlay.h"
#include "relaycore/platform/window.h"
#include "relaycore/render/camera.h"

#include <filesystem>
#include <string>

namespace RelayCore
{
    /**
     * @brief Input tuning loaded into the engine configuration.
     */
    struct FlyCameraControllerConfig {
        float move_speed = 3.5F;                   ///< Base movement speed in world units per second.
        float sprint_multiplier = 3.0F;            ///< Multiplier applied while sprint is held.
        float mouse_sensitivity_radians = 0.0022F; ///< Mouse delta to radians conversion factor.
        bool capture_mouse_on_start = true;        ///< Enables relative mouse mode during startup.
    };
} // namespace RelayCore

namespace RelayCore::render
{
    /**
     * @brief Render-system settings loaded from configuration.
     */
    struct RendererConfig {
        float clear_r = 0.10F;                    ///< Red clear-channel value.
        float clear_g = 0.11F;                    ///< Green clear-channel value.
        float clear_b = 0.14F;                    ///< Blue clear-channel value.
        float indirect_bounce_strength = 0.10F;   ///< Material-tinted ambient bounce to enrich indoor lighting.
        bool fog_enabled = true;                  ///< Enables simple distance fog for world readability.
        glm::vec3 fog_color{0.09F, 0.10F, 0.12F}; ///< Fog color blended over distant geometry.
        float fog_start = 5.5F;                   ///< World-space distance where fog begins.
        float fog_end = 18.0F;                    ///< World-space distance where fog reaches full opacity.
        float postfx_vignette_strength = 0.24F;   ///< Screen-edge darkening used to focus the view.
        float postfx_grain_strength = 0.03F;      ///< Film-grain intensity blended in screen space.
        float postfx_scanline_strength = 0.04F;   ///< Scanline modulation strength for retro presentation.
        float postfx_contrast = 1.06F;            ///< Final color-contrast multiplier around mid grey.
        float postfx_saturation = 0.94F;          ///< Final color saturation after lighting and fog.
        bool hdr_enabled = false;
        float exposure_ev = 0;
        bool shadow_enabled = true;                            ///< Enables scene shadow passes.
        int shadow_map_resolution = 1536;                      ///< Square shadow map size in texels.
        glm::vec3 shadow_light_direction{0.28F, -1.0F, 0.22F}; ///< Light-to-scene direction for the key light.
        glm::vec3 shadow_light_color{0.34F, 0.38F, 0.42F};     ///< Tint of the shadowed directional key light.
        float shadow_light_intensity = 0.95F;                  ///< Intensity multiplier for the key light.
        float shadow_strength = 0.72F;                         ///< How strongly the shadow map darkens the key light.
        float shadow_contact_strength = 0.10F;        ///< Extra darkening on shadowed receivers to improve grounding.
        float shadow_bias_near = 0.0022F;             ///< Base depth bias for the near cascade.
        float shadow_bias_far = 0.0030F;              ///< Base depth bias for the far cascade.
        float shadow_slope_bias_scale = 0.0035F;      ///< Extra bias added on grazing angles to reduce acne.
        float shadow_normal_offset_near = 0.020F;     ///< Near-cascade world-space normal push before shadow lookup.
        float shadow_normal_offset_far = 0.028F;      ///< Far-cascade world-space normal push before shadow lookup.
        float shadow_pcf_radius_near = 1.0F;          ///< PCF tap spacing for the near cascade in texels.
        float shadow_pcf_radius_far = 1.8F;           ///< PCF tap spacing for the far cascade in texels.
        float shadow_cascade_split_distance = 9.0F;   ///< Distance where shading switches from near to far cascade.
        float shadow_cascade_blend_range = 1.5F;      ///< Cross-fade band around cascade split to hide seams.
        bool shadow_debug_cascades = false;           ///< Tints shaded fragments by active cascade for tuning.
        float shadow_focus_distance = 10.0F;          ///< Distance in front of the camera used as shadow focus.
        float shadow_projection_radius = 11.0F;       ///< Half-size of the orthographic shadow frustum.
        float shadow_projection_depth = 24.0F;        ///< Depth span of the directional light frustum.
        float shadow_far_focus_distance = 20.0F;      ///< Focus point distance for the far cascade.
        float shadow_far_projection_radius = 22.0F;   ///< Half-size of the far orthographic shadow frustum.
        float shadow_far_projection_depth = 44.0F;    ///< Depth span of the far directional light frustum.
        PerspectiveCameraConfig camera{};             ///< Camera startup configuration.
        RelayCore::debug::ImGuiOverlayConfig imgui{}; ///< ImGui overlay configuration.
    };
} // namespace RelayCore::render

namespace RelayCore::core
{
    struct GameplayControlsConfig {
        std::string move_forward{"W"};
        std::string move_backward{"S"};
        std::string move_right{"D"};
        std::string move_left{"A"};
        std::string jump{"Space"};
        std::string sprint{"Left Shift"};
        std::string sprint_alt{"Right Shift"};
        std::string slow{"Left Alt"};
        std::string slow_alt{"Right Alt"};
        std::string noclip_down{"Left Ctrl"};
        std::string noclip_down_alt{"Right Ctrl"};
        std::string primary_attack{"Mouse Left"};
        std::string use{"E"};
        std::string toggle_capture{"F1"};
        std::string toggle_vsync{"V"};
        std::string toggle_noclip{"F3"};
        std::string toggle_inspector{"F4"};
        std::string pin_inspector{"F5"};
        std::string reload_level{"F7"};
        std::string open_inspector_source{"F8"};
        std::string save_runtime_snapshot{"F9"};
        std::string restore_runtime_snapshot{"F10"};
        std::string quit{"Escape"};
    };

    struct GameplayAudioConfig {
        std::string ambient_loop{};
        std::string panel_press{};
        std::string pickup{};
        std::string door_open{};
        std::string door_close{};
        std::string access_denied{};
        std::string mission_complete{};
        std::string weapon_fire{};
        std::string weapon_hit{};
        std::string threat_windup{};
        std::string rover_attack{};
        std::string turret_fire{};
        std::string turret_destroyed{};
        std::string player_hurt{};
        std::string player_respawn{};
    };

    /**
     * @brief Demo/gameplay bootstrap settings loaded from configuration.
     */
    struct GameplayConfig {
        std::string level_path{"levels/test_room.toml"}; ///< Relative asset path or absolute path to the level file.
        std::string spawn_classname{"player_spawn"};     ///< Entity classname used as the player spawn point.
        GameplayControlsConfig controls{};               ///< Gameplay action bindings loaded by the demo runtime.
        GameplayAudioConfig audio{};                     ///< Gameplay-facing world audio cues.
    };

    /**
     * @brief Aggregate configuration consumed by the engine runtime.
     */
    struct EngineConfig {
        std::filesystem::path source_path{}; ///< Engine config selected during startup.
        WindowConfig window{};               ///< Window-system settings.
        render::RendererConfig renderer{};   ///< Renderer settings.
        FlyCameraControllerConfig input{};   ///< Input settings consumed by gameplay startup.
        AudioSystemConfig audio{};           ///< Audio-system settings.
        GameplayConfig gameplay{};           ///< Demo/gameplay bootstrap settings.
    };

    /**
     * @brief Loads engine configuration from a TOML file.
     * @param config_path Path to the TOML configuration file.
     * @return Parsed engine configuration.
     */
    [[nodiscard]] auto load_engine_config(const std::filesystem::path& config_path) -> EngineConfig;

    /**
     * @brief Loads the default engine configuration from the assets directory.
     * @return Parsed engine configuration.
     */
    [[nodiscard]] auto load_default_engine_config() -> EngineConfig;
} // namespace RelayCore::core
