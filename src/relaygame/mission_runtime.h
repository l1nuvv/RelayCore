#pragma once

#include "relaycore/gameplay/interaction_runtime.h"
#include "relaycore/physics/collision.h"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace RelayCore
{
    struct BlockoutLevel;
}

namespace RelayGame
{
    enum class MissionStepKind : std::uint8_t {
        DoorOpened = 0,
        EnterZone,
        AcquireItem,
        ActivateTarget,
        SignalTarget,
    };

    struct MissionStep {
        MissionStepKind kind = MissionStepKind::EnterZone;
        std::string title{};
        std::string detail{};
        std::string target{};
        RelayCore::physics::Aabb bounds{};
    };

    struct MissionDefinition {
        std::string completion_title{"Задача выполнена"};
        std::string completion_detail{"Фрагмент комплекса пройден."};
        std::vector<MissionStep> steps{};
    };

    struct MissionStatus {
        bool active = false;
        bool completed = false;
        std::uint32_t current_step = 0;
        std::uint32_t total_steps = 0;
        std::string title{};
        std::string detail{};
    };

    struct MissionRuntimeSnapshot {
        bool active = false;
        bool completed = false;
        std::uint32_t current_step = 0;
    };

    class MissionBuilder {
    public:
        explicit MissionBuilder(const RelayCore::BlockoutLevel& level) noexcept;

        [[nodiscard]] auto build() const -> MissionDefinition;

    private:
        const RelayCore::BlockoutLevel& level_;
    };

    class MissionRuntime {
    public:
        void reset(MissionDefinition definition);

        void update(
            const RelayCore::physics::Aabb& player_bounds,
            std::span<const RelayCore::gameplay::RuntimeDoor> doors,
            std::span<const std::string> inventory_items,
            std::span<const std::string> activated_targets,
            std::span<const std::string> signaled_targets
        );

        [[nodiscard]] auto status() const -> MissionStatus;

        [[nodiscard]] auto completed() const noexcept -> bool;

        [[nodiscard]] auto snapshot() const -> MissionRuntimeSnapshot;

        [[nodiscard]] auto restore(const MissionRuntimeSnapshot& snapshot, std::string& out_error) -> bool;

    private:
        [[nodiscard]] auto current_step_complete(
            const RelayCore::physics::Aabb& player_bounds,
            std::span<const RelayCore::gameplay::RuntimeDoor> doors,
            std::span<const std::string> inventory_items,
            std::span<const std::string> activated_targets,
            std::span<const std::string> signaled_targets
        ) const -> bool;

        MissionDefinition definition_{};
        std::size_t current_step_index_ = 0;
        bool completed_ = false;
    };
} // namespace RelayGame
