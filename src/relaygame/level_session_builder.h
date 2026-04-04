#pragma once

#include "relaycore/core/engine_config.h"
#include "relaycore/gameplay/interaction_runtime.h"
#include "relaycore/world/blockout_level.h"

#include <glm/vec3.hpp>
#include <vector>

namespace RelayGame
{
    struct LevelSession {
        RelayCore::BlockoutLevel level{};
        glm::vec3 spawn_position{0.0F, 0.3F, 0.0F};
        std::vector<RelayCore::gameplay::RuntimeDoor> doors{};
        std::vector<RelayCore::gameplay::RuntimeTrigger> triggers{};
        std::vector<RelayCore::gameplay::RuntimeUsable> usables{};
        RelayCore::gameplay::InteractionSpatialIndex trigger_spatial_index{};
        RelayCore::gameplay::InteractionSpatialIndex usable_spatial_index{};
    };

    class LevelSessionBuilder {
    public:
        explicit LevelSessionBuilder(const RelayCore::core::GameplayConfig& config) noexcept;

        [[nodiscard]] auto build() const -> LevelSession;

    private:
        const RelayCore::core::GameplayConfig& config_;
    };
} // namespace RelayGame
