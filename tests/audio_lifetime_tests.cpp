#include "relaycore/audio/audio_system.h"
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
                audio.preload_one_shots(std::span{&effect, 1});
                audio.play_one_shot(effect);
                const std::array<RelayCore::AudioSystem::SpatialLoop, 1> spatial{{{ambient, {2, 1, 0}}}};
                audio.configure_spatial_loops(spatial);
                audio.play_loop(ambient, 0.5F, 0.1F);
                require(audio.has_active_loop(), "Could not start ambient loop");
                audio.play_loop(temp.path() / "missing.wav", 0.5F, 0.1F);
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
