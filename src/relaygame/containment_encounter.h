#pragma once

#include "relaycore/gameplay/combat_runtime.h"
#include "relaycore/gameplay/interaction_runtime.h"

#include <optional>

namespace RelayGame
{
    enum class GuardianMode : std::uint8_t {
        Watching,
        Pursuing,
        Windup,
        Recovery,
        Suppressed,
        Capturing,
        Contained
    };

    struct ContainmentState {
        glm::vec3 position{};
        glm::vec3 strike_target{};
        float timer = 0;
        float grip_fraction = 0;
        float capture_fraction = 0;
        GuardianMode mode = GuardianMode::Watching;
    };

    struct ContainmentEncounter {
        int entity_index = -1;
        glm::vec3 home{};
        RelayCore::physics::Aabb trap{};
        ContainmentState state;

        [[nodiscard]] auto bounds() const -> RelayCore::physics::Aabb;
        [[nodiscard]] auto collider() const -> RelayCore::physics::WorldCollider;
        [[nodiscard]] auto capture_available(std::span<const std::string> activated) const -> bool;
        void sync_usables(std::span<RelayCore::gameplay::RuntimeUsable> usables, std::span<const std::string> activated)
            const;
        void update(
            RelayCore::physics::WorldQuery world,
            const RelayCore::physics::Aabb& player_bounds,
            const glm::vec3& eye,
            RelayCore::gameplay::PlayerCombatState& player,
            float dt,
            std::vector<RelayCore::gameplay::RuntimeDoor>& doors,
            std::vector<std::string>& activated,
            std::span<const RelayCore::physics::WorldCollider> actors = {}
        );
        void suppress();
        void reset_threat_after_death();
        [[nodiscard]] auto validate_state(const ContainmentState& saved, std::span<const std::string> activated) const
            -> bool;
        [[nodiscard]] auto status_text(std::span<const std::string> activated) const -> std::string;
    };

    [[nodiscard]] auto build_containment_encounter(
        const RelayCore::BlockoutLevel& level,
        std::span<const RelayCore::gameplay::RuntimeUsable> usables,
        std::span<const RelayCore::gameplay::RuntimeDoor> doors
    ) -> std::optional<ContainmentEncounter>;
} // namespace RelayGame
