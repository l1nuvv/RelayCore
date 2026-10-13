#pragma once

#include <filesystem>

namespace RelayGame
{
    enum class GraphicsPreset {
        Custom,
        Low,
        Balanced,
        High
    };

    struct PlayerSettings {
        float volume = 1.0F;
        float sensitivity = 0.0022F;
        float fov = 70.0F;
        float camera_motion = 0.6F;
        bool vsync = true;
        bool hdr_enabled = false;
        bool bloom_enabled = false;
        bool ssao_enabled = false;
        bool ssr_enabled = false;
        bool volumetrics_enabled = false;
        int ssgi_quality = 0;
        int reflection_quality = 0;
        int dof_quality = 0;
        int motion_blur_quality = 0;
        bool taa_enabled = false;
        float render_scale = 1.0F;
        bool dynamic_resolution_enabled = false;
    };

    void apply_graphics_preset(PlayerSettings& settings, GraphicsPreset preset);
    [[nodiscard]] auto graphics_preset(const PlayerSettings& settings) -> GraphicsPreset;

    [[nodiscard]] auto load_player_settings(const std::filesystem::path& path, PlayerSettings defaults)
        -> PlayerSettings;
    void save_player_settings(const std::filesystem::path& path, const PlayerSettings& settings);
} // namespace RelayGame
