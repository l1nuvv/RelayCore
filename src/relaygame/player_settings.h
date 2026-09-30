#pragma once

#include <filesystem>

namespace RelayGame
{
    struct PlayerSettings {
        float volume = 1.0F;
        float sensitivity = 0.0022F;
        float fov = 70.0F;
        float camera_motion = 0.6F;
        bool vsync = true;
    };

    [[nodiscard]] auto load_player_settings(const std::filesystem::path& path, PlayerSettings defaults)
        -> PlayerSettings;
    void save_player_settings(const std::filesystem::path& path, const PlayerSettings& settings);
} // namespace RelayGame
