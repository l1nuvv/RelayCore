#include "relaygame/player_feedback.h"

#include <algorithm>
#include <cmath>
#include <glm/geometric.hpp>
#include <glm/vec2.hpp>
#include <limits>

namespace RelayGame
{
    auto footstep_surface(const RelayCore::BlockoutLevel& level, glm::vec3 feet) -> FootstepSurface
    {
        auto material = RelayCore::SurfaceMaterial::Concrete;
        float nearest = std::numeric_limits<float>::max();
        for (const auto* solids : {&level.solids, &level.prop_colliders})
            for (const auto& solid : *solids)
            {
                const float distance = feet.y - solid.maxs.y;
                if (feet.x < solid.mins.x || feet.x > solid.maxs.x || feet.z < solid.mins.z || feet.z > solid.maxs.z
                    || distance < -0.02F || distance > 0.15F || distance >= nearest)
                    continue;
                material = solid.material;
                nearest = distance;
            }
        using M = RelayCore::SurfaceMaterial;
        switch (material)
        {
            case M::Metal:
            case M::Corridor:
            case M::Platform:
            case M::PlatformTop:
            case M::PlatformTop2:
            case M::Hazard:
            case M::Beam:
            case M::Trim:
            case M::WallPanel:
            case M::Post:
                return FootstepSurface::Metal;
            case M::Tile:
            case M::Floor:
                return FootstepSurface::Tile;
            default:
                return FootstepSurface::Concrete;
        }
    }

    void PlayerFeedback::reset() noexcept
    {
        *this = {};
    }

    auto PlayerFeedback::update(
        float dt,
        glm::vec3 displacement,
        float previous_vertical_speed,
        bool was_grounded,
        bool grounded,
        bool noclip
    ) -> LocomotionFeedback
    {
        LocomotionFeedback result;
        if (noclip)
        {
            reset();
            return result;
        }
        if (dt <= 0.0F)
            return result;
        landing_ *= std::exp(-dt * 12.0F);
        if (!grounded)
            airborne_time_ += dt;
        if (grounded && !was_grounded)
        {
            if (airborne_time_ > 0.12F && previous_vertical_speed < -2.0F)
            {
                result.landed = std::clamp((-previous_vertical_speed - 2.0F) / 8.0F, 0.15F, 1.0F);
                landing_ = result.landed;
                step_timer_ = 0;
            }
            airborne_time_ = 0;
        }
        result.jumped = was_grounded && !grounded && displacement.y > 0.01F;
        const float distance = glm::length(glm::vec2{displacement.x, displacement.z});
        // Teleports and snapshot restores must not produce a burst of steps.
        if (distance > 1.0F)
        {
            reset();
            return {};
        }
        const float speed = distance / dt;
        const bool walking = grounded && speed > 0.15F;
        const float target = walking ? std::min(speed / 3.5F, 1.0F) : 0.0F;
        stride_ += (target - stride_) * (1.0F - std::exp(-dt * 12.0F));
        if (walking)
        {
            const float step_length = 0.85F + std::min(speed, 10.0F) * 0.055F;
            phase_ = std::fmod(phase_ + distance * 3.14159265F / step_length, 6.2831853F);
            const float sprint_amount = std::clamp((speed - 3.5F) / 7.0F, 0.0F, 1.0F);
            const float step_interval = 0.48F + (0.32F - 0.48F) * sprint_amount;
            step_timer_ += dt;
            if (step_timer_ >= step_interval)
            {
                step_timer_ = std::fmod(step_timer_, step_interval);
                result.step = result.landed == 0.0F;
            }
        } else if (grounded)
            step_timer_ = 0.0F;
        return result;
    }

    auto PlayerFeedback::eye_offset(float amount) const noexcept -> float
    {
        return amount * (std::cos(phase_ * 2.0F) * 0.018F * stride_ - landing_ * 0.09F);
    }
} // namespace RelayGame
