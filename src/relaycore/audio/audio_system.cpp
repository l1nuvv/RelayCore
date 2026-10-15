/**
 * @file src/relaycore/audio/audio_system.cpp
 * @brief AudioSystem implementation backed by miniaudio.
 */

#include "relaycore/audio/audio_system.h"

#include "relaycore/core/log.h"
#include "relaycore/core/profiling.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <miniaudio.h>
#include <stdexcept>
#include <utility>

namespace RelayCore
{
    void InitializedSoundDeleter::operator()(ma_sound* sound) const noexcept
    {
        if (sound == nullptr)
            return;
        ma_sound_stop(sound);
        ma_sound_uninit(sound);
        delete sound;
    }

    namespace
    {
        [[nodiscard]] auto create_loop_sound(ma_engine& engine, const std::filesystem::path& file_path)
            -> InitializedSound
        {
            RELAYCORE_PROFILE_FUNCTION();
            if (!std::filesystem::is_regular_file(file_path))
            {
                log::warn("[audio] Looping sound file does not exist: {}", file_path.string());
                return {};
            }
            auto loop_sound = std::make_unique<ma_sound>();
            const std::string utf8_path = file_path.string();
            if (ma_sound_init_from_file(&engine, utf8_path.c_str(), 0, nullptr, nullptr, loop_sound.get())
                != MA_SUCCESS)
            {
                log::warn("[audio] Failed to initialize looping sound: {}", utf8_path);
                return {};
            }

            ma_sound_set_looping(loop_sound.get(), MA_TRUE);
            return InitializedSound{loop_sound.release()};
        }
    } // namespace

    AudioSystem::AudioSystem(const AudioSystemConfig& config) : config_(config) {}

    AudioSystem::~AudioSystem() noexcept
    {
        RELAYCORE_PROFILE_FUNCTION();
        shutdown();
    }

    AudioSystem::AudioSystem(AudioSystem&& other) noexcept :
        config_(std::move(other.config_)), engine_(std::move(other.engine_)), one_shots_(std::move(other.one_shots_)),
        spatial_loops_(std::move(other.spatial_loops_)), loop_sound_(std::move(other.loop_sound_)),
        fading_loop_sound_(std::move(other.fading_loop_sound_)), active_loop_path_(std::move(other.active_loop_path_)),
        fading_loop_path_(std::move(other.fading_loop_path_)), active_loop_volume_(other.active_loop_volume_),
        target_loop_volume_(other.target_loop_volume_), fading_loop_start_volume_(other.fading_loop_start_volume_),
        fade_in_total_seconds_(other.fade_in_total_seconds_),
        fade_in_remaining_seconds_(other.fade_in_remaining_seconds_),
        fade_out_total_seconds_(other.fade_out_total_seconds_),
        fade_out_remaining_seconds_(other.fade_out_remaining_seconds_)
    {
        RELAYCORE_PROFILE_FUNCTION();
    }

    auto AudioSystem::operator=(AudioSystem&& other) noexcept -> AudioSystem&
    {
        RELAYCORE_PROFILE_FUNCTION();
        if (this == &other)
        {
            return *this;
        }

        shutdown();
        config_ = std::move(other.config_);
        engine_ = std::move(other.engine_);
        one_shots_ = std::move(other.one_shots_);
        spatial_loops_ = std::move(other.spatial_loops_);
        loop_sound_ = std::move(other.loop_sound_);
        fading_loop_sound_ = std::move(other.fading_loop_sound_);
        active_loop_path_ = std::move(other.active_loop_path_);
        fading_loop_path_ = std::move(other.fading_loop_path_);
        active_loop_volume_ = other.active_loop_volume_;
        target_loop_volume_ = other.target_loop_volume_;
        fading_loop_start_volume_ = other.fading_loop_start_volume_;
        fade_in_total_seconds_ = other.fade_in_total_seconds_;
        fade_in_remaining_seconds_ = other.fade_in_remaining_seconds_;
        fade_out_total_seconds_ = other.fade_out_total_seconds_;
        fade_out_remaining_seconds_ = other.fade_out_remaining_seconds_;
        return *this;
    }

    void AudioSystem::initialize()
    {
        RELAYCORE_PROFILE_FUNCTION();
        if (!config_.enabled || engine_)
        {
            return;
        }

        auto engine = std::make_unique<ma_engine>();
        const ma_result result = ma_engine_init(nullptr, engine.get());
        if (result != MA_SUCCESS)
        {
            log::warn(
                "[audio] Failed to initialize miniaudio engine (ma_result={}): {}. Audio disabled.",
                static_cast<int>(result),
                ma_result_description(result)
            );
            return;
        }

        ma_engine_set_volume(engine.get(), config_.master_volume);
        engine_ = std::move(engine);
        log::info("[audio] Audio initialized (master_volume={:.2f})", config_.master_volume);
    }

    void AudioSystem::shutdown() noexcept
    {
        RELAYCORE_PROFILE_FUNCTION();
        stop_loop();
        clear_spatial_loops();
        one_shots_.clear();
        if (!engine_)
        {
            return;
        }

        ma_engine_uninit(engine_.get());
        engine_.reset();
    }

    void AudioSystem::play_one_shot(const std::filesystem::path& file_path, const std::optional<glm::vec3> position)
    {
        RELAYCORE_PROFILE_FUNCTION();
        if (!engine_ || file_path.empty())
        {
            return;
        }

        if (std::ranges::find(one_shots_, file_path, &CachedOneShot::path) == one_shots_.end())
            preload_one_shots(std::span{&file_path, 1});
        if (const auto found = std::ranges::find(one_shots_, file_path, &CachedOneShot::path);
            found != one_shots_.end())
        {
            ma_sound_set_spatialization_enabled(found->sound.get(), position.has_value());
            if (position)
            {
                ma_sound_set_position(found->sound.get(), position->x, position->y, position->z);
                ma_sound_set_attenuation_model(found->sound.get(), ma_attenuation_model_linear);
                ma_sound_set_min_distance(found->sound.get(), 1.0F);
                ma_sound_set_max_distance(found->sound.get(), 24.0F);
            }
            ma_sound_seek_to_pcm_frame(found->sound.get(), 0);
            ma_sound_start(found->sound.get());
            return;
        }
    }

    void AudioSystem::stop_one_shots() noexcept
    {
        for (auto& cached : one_shots_)
            ma_sound_stop(cached.sound.get());
    }

    void AudioSystem::preload_one_shots(std::span<const std::filesystem::path> paths)
    {
        if (!engine_)
            return;
        one_shots_.reserve(one_shots_.size() + paths.size());
        for (const auto& path : paths)
        {
            if (std::ranges::find(one_shots_, path, &CachedOneShot::path) != one_shots_.end())
                continue;
            if (!std::filesystem::is_regular_file(path))
            {
                log::warn("[audio] One-shot sound file does not exist: {}", path.string());
                continue;
            }
            auto sound = std::make_unique<ma_sound>();
            const auto result = ma_sound_init_from_file(
                engine_.get(),
                path.string().c_str(),
                MA_SOUND_FLAG_DECODE | MA_SOUND_FLAG_NO_SPATIALIZATION,
                nullptr,
                nullptr,
                sound.get()
            );
            if (result != MA_SUCCESS)
            {
                log::warn(
                    "[audio] Could not preload player sound {}: {}",
                    path.string(),
                    ma_result_description(result)
                );
                continue;
            }
            InitializedSound initialized{sound.release()};
            one_shots_.push_back(CachedOneShot{path, std::move(initialized)});
        }
    }

    void AudioSystem::configure_spatial_loops(std::span<const SpatialLoop> loops)
    {
        clear_spatial_loops();
        spatial_loops_.resize(loops.size());
        if (!engine_)
            return;
        for (std::size_t index = 0; index < loops.size(); ++index)
        {
            const auto& source = loops[index];
            if (source.path.empty())
                continue;
            auto sound = create_loop_sound(*engine_, source.path);
            if (!sound)
                continue;
            ma_sound_set_position(sound.get(), source.position.x, source.position.y, source.position.z);
            ma_sound_set_attenuation_model(sound.get(), ma_attenuation_model_linear);
            ma_sound_set_min_distance(sound.get(), 0.8F);
            ma_sound_set_max_distance(sound.get(), 9.0F);
            ma_sound_set_volume(sound.get(), 0);
            ma_sound_start(sound.get());
            spatial_loops_[index] = std::move(sound);
        }
    }

    void AudioSystem::clear_spatial_loops() noexcept
    {
        spatial_loops_.clear();
    }

    void AudioSystem::set_spatial_loop_volume(std::size_t index, float volume)
    {
        auto& sound = spatial_loops_.at(index);
        if (sound)
            ma_sound_set_volume(sound.get(), volume);
    }

    void AudioSystem::set_listener(glm::vec3 position, glm::vec3 forward) noexcept
    {
        if (!engine_)
            return;
        ma_engine_listener_set_position(engine_.get(), 0, position.x, position.y, position.z);
        ma_engine_listener_set_direction(engine_.get(), 0, forward.x, forward.y, forward.z);
        ma_engine_listener_set_world_up(engine_.get(), 0, 0, 1, 0);
    }

    void AudioSystem::set_master_volume(float volume)
    {
        if (!std::isfinite(volume) || volume < 0 || volume > 1)
            throw std::invalid_argument("Audio volume must be between 0 and 1");
        config_.master_volume = volume;
        if (engine_)
            ma_engine_set_volume(engine_.get(), volume);
    }

    void AudioSystem::set_paused(bool paused)
    {
        if (!engine_)
            return;
        if (paused)
            ma_engine_stop(engine_.get());
        else
            ma_engine_start(engine_.get());
    }

    void AudioSystem::play_loop(const std::filesystem::path& file_path, const float volume, const float fade_seconds)
    {
        RELAYCORE_PROFILE_FUNCTION();
        if (!engine_ || file_path.empty())
        {
            return;
        }

        const float clamped_volume = std::clamp(volume, 0.0F, 1.0F);
        const float clamped_fade = std::max(fade_seconds, 0.0F);

        if (loop_sound_ && active_loop_path_ == file_path)
        {
            if (std::abs(target_loop_volume_ - clamped_volume) <= 0.0001F)
            {
                return;
            }

            target_loop_volume_ = clamped_volume;
            if (clamped_fade <= 0.0001F)
            {
                fade_in_total_seconds_ = 0.0F;
                fade_in_remaining_seconds_ = 0.0F;
                active_loop_volume_ = clamped_volume;
                ma_sound_set_volume(loop_sound_.get(), active_loop_volume_);
            }
            return;
        }

        auto next_loop_sound = create_loop_sound(*engine_, file_path);
        if (!next_loop_sound)
        {
            return;
        }
        ma_sound_set_spatialization_enabled(next_loop_sound.get(), MA_FALSE);

        fading_loop_sound_.reset();

        if (loop_sound_)
        {
            if (clamped_fade > 0.0001F)
            {
                fading_loop_sound_ = std::move(loop_sound_);
                fading_loop_path_ = active_loop_path_;
                fading_loop_start_volume_ = active_loop_volume_;
                fade_out_total_seconds_ = clamped_fade;
                fade_out_remaining_seconds_ = clamped_fade;
            } else
            {
                loop_sound_.reset();
            }
        }

        active_loop_path_ = file_path;
        target_loop_volume_ = clamped_volume;
        active_loop_volume_ = clamped_fade > 0.0001F ? 0.0F : clamped_volume;
        fade_in_total_seconds_ = clamped_fade;
        fade_in_remaining_seconds_ = clamped_fade;

        ma_sound_set_volume(next_loop_sound.get(), active_loop_volume_);
        if (ma_sound_start(next_loop_sound.get()) != MA_SUCCESS)
        {
            log::warn("[audio] Failed to start looping sound: {}", file_path.string());
            return;
        }

        loop_sound_ = std::move(next_loop_sound);
    }

    void AudioSystem::stop_loop(const float fade_seconds) noexcept
    {
        RELAYCORE_PROFILE_FUNCTION();
        const float clamped_fade = std::max(fade_seconds, 0.0F);
        if (!loop_sound_ && !fading_loop_sound_)
        {
            return;
        }

        if (clamped_fade <= 0.0001F || !loop_sound_)
        {
            loop_sound_.reset();
            fading_loop_sound_.reset();
            active_loop_path_.clear();
            fading_loop_path_.clear();
            active_loop_volume_ = 0.0F;
            target_loop_volume_ = 0.0F;
            fading_loop_start_volume_ = 0.0F;
            fade_in_total_seconds_ = 0.0F;
            fade_in_remaining_seconds_ = 0.0F;
            fade_out_total_seconds_ = 0.0F;
            fade_out_remaining_seconds_ = 0.0F;
            return;
        }

        fading_loop_sound_.reset();
        fading_loop_sound_ = std::move(loop_sound_);
        fading_loop_path_ = std::move(active_loop_path_);
        fading_loop_start_volume_ = active_loop_volume_;
        fade_out_total_seconds_ = clamped_fade;
        fade_out_remaining_seconds_ = clamped_fade;
        active_loop_path_.clear();
        active_loop_volume_ = 0.0F;
        target_loop_volume_ = 0.0F;
        fade_in_total_seconds_ = 0.0F;
        fade_in_remaining_seconds_ = 0.0F;
    }

    void AudioSystem::update(const float delta_seconds) noexcept
    {
        RELAYCORE_PROFILE_FUNCTION();
        const float step = std::max(delta_seconds, 0.0F);

        if (loop_sound_)
        {
            if (fade_in_remaining_seconds_ > 0.0F && fade_in_total_seconds_ > 0.0001F)
            {
                fade_in_remaining_seconds_ = std::max(0.0F, fade_in_remaining_seconds_ - step);
                const float alpha = 1.0F - (fade_in_remaining_seconds_ / fade_in_total_seconds_);
                active_loop_volume_ = std::lerp(0.0F, target_loop_volume_, std::clamp(alpha, 0.0F, 1.0F));
                ma_sound_set_volume(loop_sound_.get(), active_loop_volume_);
            } else if (std::abs(active_loop_volume_ - target_loop_volume_) > 0.0001F)
            {
                active_loop_volume_ = target_loop_volume_;
                ma_sound_set_volume(loop_sound_.get(), active_loop_volume_);
            }
        }

        if (fading_loop_sound_)
        {
            if (fade_out_remaining_seconds_ > 0.0F && fade_out_total_seconds_ > 0.0001F)
            {
                fade_out_remaining_seconds_ = std::max(0.0F, fade_out_remaining_seconds_ - step);
                const float alpha = fade_out_remaining_seconds_ / fade_out_total_seconds_;
                ma_sound_set_volume(
                    fading_loop_sound_.get(),
                    fading_loop_start_volume_ * std::clamp(alpha, 0.0F, 1.0F)
                );
            }

            if (fade_out_remaining_seconds_ <= 0.0F)
            {
                fading_loop_sound_.reset();
                fading_loop_path_.clear();
                fading_loop_start_volume_ = 0.0F;
                fade_out_total_seconds_ = 0.0F;
            }
        }
    }

    auto AudioSystem::is_initialized() const noexcept -> bool
    {
        return static_cast<bool>(engine_);
    }

    auto AudioSystem::config() const noexcept -> const AudioSystemConfig&
    {
        return config_;
    }

    auto AudioSystem::has_active_loop() const noexcept -> bool
    {
        return static_cast<bool>(loop_sound_);
    }

    auto AudioSystem::current_loop_path() const noexcept -> const std::filesystem::path&
    {
        return active_loop_path_;
    }

    auto AudioSystem::current_loop_volume() const noexcept -> float
    {
        return active_loop_volume_;
    }
} // namespace RelayCore
