/**
 * @file src/relaycore/audio/audio_system.h
 * @brief Thin wrapper around the miniaudio engine.
 */

#pragma once

#include <filesystem>
#include <memory>
#include <string>

struct ma_engine;
struct ma_sound;

namespace RelayCore
{
    /**
     * @brief Startup configuration for the audio subsystem.
     */
    struct AudioSystemConfig {
        bool enabled = true;            ///< Enables or disables audio initialization entirely.
        float master_volume = 1.0F;     ///< Initial master volume applied to the audio engine.
        std::string startup_sound = ""; ///< Optional startup sound path relative to the asset root.
    };

    /**
     * @brief Owns and controls a miniaudio playback engine.
     */
    class AudioSystem {
    public:
        /**
         * @brief Creates an audio system with the supplied configuration.
         * @param config Audio startup configuration.
         */
        explicit AudioSystem(const AudioSystemConfig& config = AudioSystemConfig{});

        /**
         * @brief Shuts down the audio system.
         */
        ~AudioSystem() noexcept;

        AudioSystem(const AudioSystem&) = delete; ///< AudioSystem is not copyable.

        /**
         * @brief Disables copy assignment.
         * @return Reference to this object.
         */
        auto operator=(const AudioSystem&) -> AudioSystem& = delete;

        /**
         * @brief Moves an audio system.
         * @param other Source audio system.
         */
        AudioSystem(AudioSystem&&) noexcept;

        /**
         * @brief Moves audio-system state into this object.
         * @param other Source audio system.
         * @return Reference to this object.
         */
        auto operator=(AudioSystem&&) noexcept -> AudioSystem&;

        /**
         * @brief Initializes the underlying miniaudio engine.
         */
        void initialize();

        /**
         * @brief Shuts down the underlying miniaudio engine.
         */
        void shutdown() noexcept;

        /**
         * @brief Plays a sound file once if audio is available.
         * @param file_path Path to the sound file to play.
         */
        void play_one_shot(const std::filesystem::path& file_path) const;

        /**
         * @brief Starts or replaces a single looping sound.
         * @param file_path Path to the sound file to loop.
         */
        void play_loop(const std::filesystem::path& file_path, float volume = 1.0F, float fade_seconds = 0.0F);

        /**
         * @brief Stops the currently active loop, if any.
         */
        void stop_loop(float fade_seconds = 0.0F) noexcept;

        /**
         * @brief Advances active loop fades and transitions.
         * @param delta_seconds Simulation/frame delta.
         */
        void update(float delta_seconds) noexcept;

        /**
         * @brief Reports whether the audio engine has been initialized.
         * @return True when the miniaudio engine is live.
         */
        [[nodiscard]] auto is_initialized() const noexcept -> bool;

        /**
         * @brief Returns the immutable audio configuration.
         * @return Audio startup configuration.
         */
        [[nodiscard]] auto config() const noexcept -> const AudioSystemConfig&;

        [[nodiscard]] auto has_active_loop() const noexcept -> bool;

        [[nodiscard]] auto current_loop_path() const noexcept -> const std::filesystem::path&;

        [[nodiscard]] auto current_loop_volume() const noexcept -> float;

    private:
        AudioSystemConfig config_{};                    ///< Stored audio configuration.
        std::unique_ptr<ma_engine> engine_;             ///< Owned miniaudio engine instance.
        std::unique_ptr<ma_sound> loop_sound_{};        ///< Optional looping sound instance.
        std::unique_ptr<ma_sound> fading_loop_sound_{}; ///< Optional fading-out loop instance.
        std::filesystem::path active_loop_path_{};
        std::filesystem::path fading_loop_path_{};
        float active_loop_volume_ = 0.0F;
        float target_loop_volume_ = 0.0F;
        float fading_loop_start_volume_ = 0.0F;
        float fade_in_total_seconds_ = 0.0F;
        float fade_in_remaining_seconds_ = 0.0F;
        float fade_out_total_seconds_ = 0.0F;
        float fade_out_remaining_seconds_ = 0.0F;
    };
} // namespace RelayCore
