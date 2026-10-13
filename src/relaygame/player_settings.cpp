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
            if ((settings.bloom_enabled || settings.ssr_enabled) && !settings.hdr_enabled)
                throw std::runtime_error("player_settings: bloom and SSR require HDR enabled");
            if (settings.ssgi_quality < 0 || settings.ssgi_quality > 2)
                throw std::runtime_error("player_settings.ssgi_quality must be 0, 1 or 2");
            if (settings.reflection_quality < 0 || settings.reflection_quality > 2)
                throw std::runtime_error("player_settings.reflection_quality must be 0, 1 or 2");
            if (settings.dof_quality < 0 || settings.dof_quality > 2)
                throw std::runtime_error("player_settings.dof_quality must be 0, 1 or 2");
            if (settings.motion_blur_quality < 0 || settings.motion_blur_quality > 2)
                throw std::runtime_error("player_settings.motion_blur_quality must be 0, 1 or 2");
            if ((settings.ssgi_quality || settings.dof_quality || settings.motion_blur_quality)
                && !settings.hdr_enabled)
                throw std::runtime_error("player_settings: SSGI and lens effects require HDR enabled");
            if (settings.reflection_quality && !settings.ssr_enabled)
                throw std::runtime_error("player_settings: improved reflections require SSR enabled");
            range("render_scale", settings.render_scale, .5F, 1);
            if ((settings.taa_enabled || settings.render_scale != 1 || settings.dynamic_resolution_enabled)
                && !settings.hdr_enabled)
                throw std::runtime_error("player_settings: TAA and scaling require HDR enabled");
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
        defaults.render_scale = read_float("render_scale", defaults.render_scale);
        if (table.contains("ssgi_quality"))
        {
            const auto value = table["ssgi_quality"].value<int>();
            if (!value)
                throw std::runtime_error("player_settings.ssgi_quality must be an integer");
            defaults.ssgi_quality = *value;
        }
        if (table.contains("reflection_quality"))
        {
            const auto value = table["reflection_quality"].value<int>();
            if (!value)
                throw std::runtime_error("player_settings.reflection_quality must be an integer");
            defaults.reflection_quality = *value;
        }
        if (table.contains("dof_quality"))
        {
            const auto value = table["dof_quality"].value<int>();
            if (!value)
                throw std::runtime_error("player_settings.dof_quality must be an integer");
            defaults.dof_quality = *value;
        }
        if (table.contains("motion_blur_quality"))
        {
            const auto value = table["motion_blur_quality"].value<int>();
            if (!value)
                throw std::runtime_error("player_settings.motion_blur_quality must be an integer");
            defaults.motion_blur_quality = *value;
        }

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
        if (table.contains("bloom_enabled"))
        {
            const auto value = table["bloom_enabled"].value<bool>();
            if (!value)
                throw std::runtime_error("player_settings.bloom_enabled must be a boolean");
            defaults.bloom_enabled = *value;
        }
        if (table.contains("ssao_enabled"))
        {
            const auto value = table["ssao_enabled"].value<bool>();
            if (!value)
                throw std::runtime_error("player_settings.ssao_enabled must be a boolean");
            defaults.ssao_enabled = *value;
        }
        if (table.contains("ssr_enabled"))
        {
            const auto value = table["ssr_enabled"].value<bool>();
            if (!value)
                throw std::runtime_error("player_settings.ssr_enabled must be a boolean");
            defaults.ssr_enabled = *value;
        }
        if (table.contains("volumetrics_enabled"))
        {
            const auto value = table["volumetrics_enabled"].value<bool>();
            if (!value)
                throw std::runtime_error("player_settings.volumetrics_enabled must be a boolean");
            defaults.volumetrics_enabled = *value;
        }
        if (table.contains("taa_enabled"))
        {
            const auto value = table["taa_enabled"].value<bool>();
            if (!value)
                throw std::runtime_error("player_settings.taa_enabled must be a boolean");
            defaults.taa_enabled = *value;
        }
        if (table.contains("dynamic_resolution_enabled"))
        {
            const auto value = table["dynamic_resolution_enabled"].value<bool>();
            if (!value)
                throw std::runtime_error("player_settings.dynamic_resolution_enabled must be a boolean");
            defaults.dynamic_resolution_enabled = *value;
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
            {"hdr_enabled", settings.hdr_enabled},
            {"bloom_enabled", settings.bloom_enabled},
            {"ssao_enabled", settings.ssao_enabled},
            {"ssr_enabled", settings.ssr_enabled},
            {"volumetrics_enabled", settings.volumetrics_enabled},
            {"ssgi_quality", settings.ssgi_quality},
            {"reflection_quality", settings.reflection_quality},
            {"dof_quality", settings.dof_quality},
            {"motion_blur_quality", settings.motion_blur_quality},
            {"taa_enabled", settings.taa_enabled},
            {"render_scale", settings.render_scale},
            {"dynamic_resolution_enabled", settings.dynamic_resolution_enabled}
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
