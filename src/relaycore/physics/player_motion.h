#pragma once

#include "relaycore/physics/collision.h"

#include <glm/vec3.hpp>

namespace RelayCore::physics
{
    struct PlayerMotionConfig {
        float max_speed = 4.5F;
        float sprint_multiplier = 1.0F;
        float ground_accelerate = 10.0F;
        float air_accelerate = 10.0F;
        float ground_friction = 4.0F;
        float stop_speed = 1.5F;
        float gravity = 24.0F;
        float jump_speed = 8.5F;
        float air_wishspeed_cap = 1.0F;
        CollisionConfig collision{};
        float noclip_speed = 8.0F;
        float noclip_fast_multiplier = 3.0F;
        float noclip_slow_multiplier = 0.35F;
    };

    struct MoveInput {
        float forward_axis = 0.0F;
        float right_axis = 0.0F;
        float up_axis = 0.0F;
        bool jump_pressed = false;
        bool sprint = false;
        bool slow = false;
    };

    struct PlayerMotionState {
        glm::vec3 position{};
        glm::vec3 velocity{};
        bool grounded = false;
    };

    struct GroundedMoveDiagnostics {
        bool started_grounded = false;
        bool ended_grounded = false;
        bool jumped = false;
        bool ground_probe_hit = false;
        bool ground_snap_applied = false;
        bool attempted_step_move = false;
        bool used_step_move = false;
        bool hit_x = false;
        bool hit_y = false;
        bool hit_z = false;
    };

    void update_grounded(
        PlayerMotionState& state,
        const MoveInput& input,
        float yaw_radians,
        float dt,
        CollisionWorld world,
        const PlayerMotionConfig& config,
        GroundedMoveDiagnostics* diagnostics = nullptr
    );

    void update_noclip(
        PlayerMotionState& state,
        const MoveInput& input,
        float yaw_radians,
        float pitch_radians,
        float dt,
        const PlayerMotionConfig& config,
        GroundedMoveDiagnostics* diagnostics = nullptr
    );
} // namespace RelayCore::physics
