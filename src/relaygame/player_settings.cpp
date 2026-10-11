#include "relaygame/player_settings.h"

#include <cmath>
#include <fstream>
#include <stdexcept>
#include <toml++/toml.hpp>

namespace RelayGame
{
    namespace
    {
        void validate(const PlayerSettings& settings)
        {
            const auto range = [](const char* key, float value, float low, float high) {
                if (!std::isfinite(value) || value < low || value > high)
                    throw std::runtime_error(std::string{"player_settings."} + key + " is out of range");
            };
            range("volume", settings.volume, 0, 1);
            range("sensitivity", settings.sensitivity, 0.0003F, 0.008F);
            range("fov", settings.fov, 50, 100);
            range("camera_motion", settings.camera_motion, 0, 1);
        }
    } // namespace

    auto load_player_settings(const std::filesystem::path& path, PlayerSettings defaults) -> PlayerSettings
    {
        if (!std::filesystem::exists(path))
            return defaults;
        const auto table = toml::parse_file(path.string());
        const auto read_float = [&](const char* key, float fallback) {
            if (!table.contains(key))
                return fallback;
            const auto value = table[key].value<double>();
            if (!value)
                throw std::runtime_error(std::string{"player_settings."} + key + " must be a number");
            return static_cast<float>(*value);
        };
        defaults.volume = read_float("volume", defaults.volume);
        defaults.sensitivity = read_float("sensitivity", defaults.sensitivity);
        defaults.fov = read_float("fov", defaults.fov);
        defaults.camera_motion = read_float("camera_motion", defaults.camera_motion);
        if (table.contains("vsync"))
        {
            const auto value = table["vsync"].value<bool>();
            if (!value)
                throw std::runtime_error("player_settings.vsync must be a boolean");
            defaults.vsync = *value;
        }
        if (table.contains("hdr_enabled"))
        {
            const auto value = table["hdr_enabled"].value<bool>();
            if (!value)
                throw std::runtime_error("player_settings.hdr_enabled must be a boolean");
            defaults.hdr_enabled = *value;
        }
        validate(defaults);
        return defaults;
    }

    void save_player_settings(const std::filesystem::path& path, const PlayerSettings& settings)
    {
        validate(settings);
        const toml::table table{
            {"volume", settings.volume},
            {"sensitivity", settings.sensitivity},
            {"fov", settings.fov},
            {"camera_motion", settings.camera_motion},
            {"vsync", settings.vsync},
            {"hdr_enabled", settings.hdr_enabled}
        };
        std::filesystem::create_directories(path.parent_path());
        auto temporary = path;
        temporary += ".tmp";
        {
            std::ofstream output{temporary, std::ios::binary | std::ios::trunc};
            output.exceptions(std::ios::badbit | std::ios::failbit);
            output << table << '\n';
            output.close();
        }
        std::filesystem::rename(temporary, path);
    }
} // namespace RelayGame
