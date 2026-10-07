#include "relaycore/audio/audio_system.h"
#include "relaycore/core/engine_config.h"
#include "relaycore/core/log.h"
#include "relaycore/core/paths.h"

#include <array>
#include <exception>
#include <filesystem>
#include <iostream>
#include <span>
#include <stdexcept>
#include <utility>

#include "scoped_temp_directory.h"

namespace
{
    void require(const bool condition, const char* message)
    {
        if (!condition)
            throw std::runtime_error(message);
    }
} // namespace

int main()
{
    try
    {
        const RelayCore::test::ScopedTempDirectory temp;
        RelayCore::log::init({.logs_directory = temp.path().string(), .enable_timestamped_log = false});
        int result = 0;
        try
        {
            const auto run = [&]() -> int {
                RelayCore::AudioSystem audio;
                audio.initialize();
                if (!audio.is_initialized())
                {
                    std::cerr << "audio_lifetime=skipped: audio engine unavailable\n";
                    return 77;
                }

                const auto ambient = RelayCore::core::asset_path("audio/ambient_airlock_hum.wav");
                const auto effect = RelayCore::core::asset_path("audio/door_open.wav");
                const auto cues = RelayCore::core::load_default_engine_config().gameplay.audio;
                audio.set_master_volume(0);
                for (const auto& cue : std::array{
                         cues.weapon_fire,
                         std::string{"audio/combat/shotgun_fire.wav"},
                         std::string{"audio/combat/impact_hard.wav"},
                         std::string{"audio/combat/impact_metal.wav"},
                         std::string{"audio/environment/steam.wav"},
                         std::string{"audio/environment/ventilation.wav"},
                         std::string{"audio/environment/electrical.wav"},
                         cues.weapon_hit,
                         cues.threat_windup,
                         cues.rover_attack,
                         cues.turret_fire,
                         cues.turret_destroyed,
                         cues.player_hurt
                     })
                {
                    require(!cue.empty(), "Shipped audio cue is missing");
                    const auto path = RelayCore::core::asset_path(cue);
                    audio.preload_one_shots(std::span{&path, 1});
                    audio.play_one_shot(path);
                    audio.set_listener({0, 1, 0}, {0, 0, -1});
                    audio.play_one_shot(path, glm::vec3{-2, 1, -3});
                    audio.play_one_shot(path, glm::vec3{2, 1, -3});
                    audio.play_one_shot(path);
                    audio.play_loop(path, 0);
                    require(audio.current_loop_path() == path && audio.has_active_loop(), "Audio WAV did not decode");
                    audio.set_paused(true);
                    audio.stop_one_shots();
                    audio.set_paused(false);
                    audio.stop_loop();
                }
                audio.set_master_volume(1);
                audio.preload_one_shots(std::span{&effect, 1});
                audio.play_one_shot(effect);
                const std::array<RelayCore::AudioSystem::SpatialLoop, 1> spatial{{{ambient, {2, 1, 0}}}};
                audio.configure_spatial_loops(spatial);
                audio.play_loop(ambient, 0.5F, 0.1F);
                require(audio.has_active_loop(), "Could not start ambient loop");
                const auto missing = temp.path() / "missing.wav";
                audio.preload_one_shots(std::span{&missing, 1});
                audio.play_one_shot(missing);
                audio.play_loop(missing, 0.5F, 0.1F);
                require(audio.current_loop_path() == ambient, "Failed replacement changed active loop");
                audio.play_loop(effect, 0.4F, 0.1F);
                require(audio.current_loop_path() == effect, "Successful replacement did not change active loop");
                audio.update(0.2F);

                RelayCore::AudioSystem moved{std::move(audio)};
                require(!audio.is_initialized() && moved.has_active_loop(), "Move lost audio ownership");
                moved.stop_loop(0.1F);
                moved.update(0.2F);
                require(!moved.has_active_loop(), "Fade did not stop active loop");
                RelayCore::AudioSystem replacement{RelayCore::AudioSystemConfig{.enabled = false}};
                replacement = std::move(moved);
                require(replacement.is_initialized(), "Move assignment lost audio engine");
                replacement.clear_spatial_loops();
                replacement.shutdown();
                require(!replacement.is_initialized(), "Audio shutdown retained engine");
                return 0;
            };
            result = run();
        } catch (const std::exception& error)
        {
            std::cerr << "audio_lifetime=failed: " << error.what() << '\n';
            result = 1;
        }
        RelayCore::log::shutdown();
        if (result == 0)
            std::cout << "audio_lifetime=passed\n";
        return result;
    } catch (const std::exception& error)
    {
        std::cerr << "audio_lifetime=failed: " << error.what() << '\n';
        return 1;
    }
}
