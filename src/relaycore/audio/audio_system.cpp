/**
 * @file src/relaycore/audio/audio_system.cpp
 * @brief AudioSystem implementation backed by miniaudio.
 */

#include "relaycore/audio/audio_system.h"

#include "relaycore/core/log.h"

#include <memory>
#include <miniaudio.h>
#include <stdexcept>
#include <utility>

namespace RelayCore
{
    AudioSystem::AudioSystem(const AudioSystemConfig& config) : config_(config) {}

    AudioSystem::~AudioSystem() noexcept
    {
        shutdown();
    }

    AudioSystem::AudioSystem(AudioSystem&& other) noexcept :
        config_(std::move(other.config_)), engine_(std::move(other.engine_)), loop_sound_(std::move(other.loop_sound_))
    {}

    auto AudioSystem::operator=(AudioSystem&& other) noexcept -> AudioSystem&
    {
        if (this == &other)
        {
            return *this;
        }

        shutdown();
        config_ = std::move(other.config_);
        engine_ = std::move(other.engine_);
        loop_sound_ = std::move(other.loop_sound_);
        return *this;
    }

    void AudioSystem::initialize()
    {
        if (!config_.enabled || engine_)
        {
            return;
        }

        auto engine = std::make_unique<ma_engine>();
        if (ma_engine_init(nullptr, engine.get()) != MA_SUCCESS)
        {
            throw std::runtime_error("Failed to initialize miniaudio engine");
        }

        ma_engine_set_volume(engine.get(), config_.master_volume);
        engine_ = std::move(engine);
        log::info("Audio initialized (master_volume={:.2f})", config_.master_volume);
    }

    void AudioSystem::shutdown() noexcept
    {
        stop_loop();
        if (!engine_)
        {
            return;
        }

        ma_engine_uninit(engine_.get());
        engine_.reset();
    }

    void AudioSystem::play_one_shot(const std::filesystem::path& file_path) const
    {
        if (!engine_ || file_path.empty())
        {
            return;
        }

        const std::string utf8_path = file_path.string();
        if (ma_engine_play_sound(engine_.get(), utf8_path.c_str(), nullptr) != MA_SUCCESS)
        {
            log::warn("Failed to play sound: {}", utf8_path);
        }
    }

    void AudioSystem::play_loop(const std::filesystem::path& file_path)
    {
        if (!engine_ || file_path.empty())
        {
            return;
        }

        stop_loop();

        const std::string utf8_path = file_path.string();
        auto loop_sound = std::make_unique<ma_sound>();
        if (ma_sound_init_from_file(engine_.get(), utf8_path.c_str(), 0, nullptr, nullptr, loop_sound.get())
            != MA_SUCCESS)
        {
            log::warn("Failed to initialize looping sound: {}", utf8_path);
            return;
        }

        ma_sound_set_looping(loop_sound.get(), MA_TRUE);
        if (ma_sound_start(loop_sound.get()) != MA_SUCCESS)
        {
            log::warn("Failed to start looping sound: {}", utf8_path);
            ma_sound_uninit(loop_sound.get());
            return;
        }

        loop_sound_ = std::move(loop_sound);
    }

    void AudioSystem::stop_loop() noexcept
    {
        if (!loop_sound_)
        {
            return;
        }

        ma_sound_stop(loop_sound_.get());
        ma_sound_uninit(loop_sound_.get());
        loop_sound_.reset();
    }

    auto AudioSystem::is_initialized() const noexcept -> bool
    {
        return static_cast<bool>(engine_);
    }

    auto AudioSystem::config() const noexcept -> const AudioSystemConfig&
    {
        return config_;
    }
} // namespace RelayCore
