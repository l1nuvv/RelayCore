#pragma once

#include "relaycore/world/blockout_level.h"

namespace RelayGame
{
    enum class FootstepSurface {
        Concrete,
        Metal,
        Tile
    };

    [[nodiscard]] auto footstep_surface(const RelayCore::BlockoutLevel& level, glm::vec3 feet) -> FootstepSurface;

    struct LocomotionFeedback {
        bool step = false;
        bool jumped = false;
        float landed = 0.0F;
    };

    class PlayerFeedback {
    public:
        void reset() noexcept;
        [[nodiscard]] auto update(
            float dt,
            glm::vec3 displacement,
            float previous_vertical_speed,
            bool was_grounded,
            bool grounded,
            bool noclip
        ) -> LocomotionFeedback;
        [[nodiscard]] auto eye_offset(float amount) const noexcept -> float;
        [[nodiscard]] auto phase() const noexcept -> float
        {
            return phase_;
        }
        [[nodiscard]] auto stride() const noexcept -> float
        {
            return stride_;
        }
        [[nodiscard]] auto landing() const noexcept -> float
        {
            return landing_;
        }

    private:
        float phase_ = 0.0F;
        float stride_ = 0.0F;
        float step_timer_ = 0.0F;
        float landing_ = 0.0F;
        float airborne_time_ = 0.0F;
    };
} // namespace RelayGame
