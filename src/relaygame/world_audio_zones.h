#pragma once

#include "relaycore/physics/collision.h"
#include "relaycore/world/blockout_level.h"

#include <filesystem>
#include <span>
#include <string>
#include <vector>

namespace RelayGame
{
    struct RuntimeAudioZone {
        std::string name{};
        RelayCore::physics::Aabb bounds{};
        std::filesystem::path loop_path{};
        float gain = 1.0F;
        int priority = 0;
    };

    [[nodiscard]] auto build_audio_zones(const RelayCore::BlockoutLevel& level, const std::filesystem::path& asset_root)
        -> std::vector<RuntimeAudioZone>;

    [[nodiscard]] auto find_active_audio_zone(
        std::span<const RuntimeAudioZone> zones,
        const RelayCore::physics::Aabb& listener_bounds,
        int current_zone_index = -1
    ) -> int;
} // namespace RelayGame
