#pragma once

#include "relaygame/enemy_runtime.h"
#include "relaygame/player_controller.h"
#include "relaygame/runtime_model.h"

#include <filesystem>

namespace RelayGame
{
    inline constexpr std::int64_t saved_game_format_version = 5;
    struct SavedGame {
        RelayGame::RuntimeLevelSnapshot world;
        GroundedPlayerController::State player;
        std::uint64_t level_fingerprint = 0;
        float played_seconds = 0;
        bool flashlight = false;
        // One flat snapshot per inactive map; nested histories are rejected by the file format.
        std::vector<SavedGame> visited_levels;
        std::vector<EnemyState> enemies;
    };

    [[nodiscard]] auto fingerprint_level(const RelayCore::BlockoutLevel& level) -> std::uint64_t;
    [[nodiscard]] auto load_saved_game(const std::filesystem::path& path) -> SavedGame;
    void write_saved_game(const std::filesystem::path& path, const SavedGame& save);
} // namespace RelayGame
