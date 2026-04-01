#pragma once

#include <filesystem>
#include <memory>
#include <string>

struct ma_engine;

namespace RelayCore
{
    struct AudioSystemConfig {
        bool        enabled       = true;
        float       master_volume = 1.0F;
        std::string startup_sound = "";
    };

    class AudioSystem {
    public:
        explicit AudioSystem(const AudioSystemConfig& config = AudioSystemConfig{});

        ~AudioSystem() noexcept;

        AudioSystem(const AudioSystem&) = delete;

        auto operator=(const AudioSystem&) -> AudioSystem& = delete;

        AudioSystem(AudioSystem&&) noexcept;

        auto operator=(AudioSystem&&) noexcept -> AudioSystem&;

        void initialize();

        void shutdown() noexcept;

        void play_one_shot(const std::filesystem::path& file_path) const;

        [[nodiscard]] auto is_initialized() const noexcept -> bool;

        [[nodiscard]] auto config() const noexcept -> const AudioSystemConfig&;

    private:
        AudioSystemConfig          config_{};
        std::unique_ptr<ma_engine> engine_;
    };
} // namespace RelayCore
