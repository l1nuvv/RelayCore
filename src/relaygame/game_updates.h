#pragma once

#include <cstdint>
#include <string>

namespace RelayGame
{
    struct UpdateRelease {
        bool available = false;
        std::string version;
        std::uint64_t bytes = 0;
        std::string notes;
    };

    [[nodiscard]] auto check_game_update() -> UpdateRelease;
    void install_game_update(const std::string& version);
    [[nodiscard]] auto can_update_game() -> bool;
    void signal_update_ready();
} // namespace RelayGame
