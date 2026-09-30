#pragma once

#include <cstdint>
#include <string_view>

namespace RelayGame
{
    enum class PlayerWeapon : std::uint8_t {
        Precise,
        Shotgun
    };

    inline constexpr std::string_view shotgun_item_id = "weapon_shotgun";
} // namespace RelayGame
