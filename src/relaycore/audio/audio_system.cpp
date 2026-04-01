#include "relaycore/audio/audio_system.hpp"

#include <memory>
#include <miniaudio.h>
#include <stdexcept>
#include <utility>

#include "relaycore/core/log.hpp"

namespace RelayCore
{
    AudioSystem::AudioSystem(const AudioSystemConfig& config) : config_(config) {}

    AudioSystem::~AudioSystem() noexcept { shutdown(); }

    AudioSystem::AudioSystem(AudioSystem&& other) noexcept :
        config_(std::move(other.config_)), engine_(std::move(other.engine_))
    {}

    auto AudioSystem::operator=(AudioSystem&& other) noexcept -> AudioSystem&
    {
        if (this == &other) { return *this; }

        shutdown();
        config_ = std::move(other.config_);
        engine_ = std::move(other.engine_);
        return *this;
    }

    void AudioSystem::initialize()
    {
        if (!config_.enabled || engine_) { return; }

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
        if (!engine_) { return; }

        ma_engine_uninit(engine_.get());
        engine_.reset();
    }

    void AudioSystem::play_one_shot(const std::filesystem::path& file_path) const
    {
        if (!engine_ || file_path.empty()) { return; }

        const std::string utf8_path = file_path.string();
        if (ma_engine_play_sound(engine_.get(), utf8_path.c_str(), nullptr) != MA_SUCCESS)
        {
            log::warn("Failed to play sound: {}", utf8_path);
        }
    }

    auto AudioSystem::is_initialized() const noexcept -> bool { return static_cast<bool>(engine_); }

    auto AudioSystem::config() const noexcept -> const AudioSystemConfig& { return config_; }
} // namespace RelayCore
