#include "relaycore/audio/audio_system.h"

#include <doctest/doctest.h>
#include <filesystem>

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
}
