#include "relaycore/audio/audio_system.h"

#include <array>
#include <doctest/doctest.h>
#include <filesystem>
#include <utility>

TEST_CASE("audio system disabled mode is safe and remains uninitialized")
{
    RelayCore::AudioSystem audio{RelayCore::AudioSystemConfig{.enabled = false}};

    CHECK_NOTHROW(audio.initialize());
    CHECK_FALSE(audio.is_initialized());

    CHECK_NOTHROW(audio.play_one_shot(std::filesystem::path{"audio/test.wav"}));
    CHECK_NOTHROW(audio.play_loop(std::filesystem::path{"audio/test.wav"}));
    CHECK_NOTHROW(audio.stop_loop());
    CHECK_NOTHROW(audio.update(0.016F));
    CHECK_FALSE(audio.has_active_loop());
    const std::array<RelayCore::AudioSystem::SpatialLoop, 1> sources{{{"missing.wav", {2, 1, 0}}}};
    CHECK_NOTHROW(audio.configure_spatial_loops(sources));
    CHECK_NOTHROW(audio.set_listener({0, 1, 0}, {0, 0, 1}));
    CHECK_NOTHROW(audio.set_spatial_loop_volume(0, 0.25F));
    RelayCore::AudioSystem moved{std::move(audio)};
    CHECK_NOTHROW(moved.set_spatial_loop_volume(0, 0.5F));
    CHECK_NOTHROW(moved.clear_spatial_loops());
}
