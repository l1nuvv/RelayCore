#include "relaycore/physics/player_motion.h"

#include <algorithm>
#include <cmath>
#include <glm/geometric.hpp>
#include <glm/trigonometric.hpp>
#include <glm/vec2.hpp>

namespace RelayCore::physics
{
    namespace
    {
        [[nodiscard]] auto make_forward_flat(const float yaw_radians) -> glm::vec3
        {
            return glm::normalize(glm::vec3{std::cos(yaw_radians), 0.0F, std::sin(yaw_radians)});
        }

        [[nodiscard]] auto make_right_flat(const float yaw_radians) -> glm::vec3
        {
            const glm::vec3 forward = make_forward_flat(yaw_radians);
            return glm::normalize(glm::cross(forward, glm::vec3{0.0F, 1.0F, 0.0F}));
        }

        [[nodiscard]] auto make_forward_free(const float yaw_radians, const float pitch_radians) -> glm::vec3
        {
            const float cos_pitch = std::cos(pitch_radians);
            return glm::normalize(
                glm::vec3{
                    std::cos(yaw_radians) * cos_pitch,
                    std::sin(pitch_radians),
                    std::sin(yaw_radians) * cos_pitch,
                }
            );
        }

        [[nodiscard]] auto make_right_free(const float yaw_radians) -> glm::vec3
        {
            return make_right_flat(yaw_radians);
        }

        [[nodiscard]] auto planar_speed(const glm::vec3& velocity) -> float
        {
            return std::sqrt((velocity.x * velocity.x) + (velocity.z * velocity.z));
        }

        void apply_friction(glm::vec3& velocity, const PlayerMotionConfig& config, const float dt)
        {
            const float speed = planar_speed(velocity);
            if (speed <= 0.0F) { return; }

            const float control   = std::max(speed, config.stop_speed);
            const float drop      = control * config.ground_friction * dt;
            const float new_speed = std::max(speed - drop, 0.0F);
            if (new_speed == speed) { return; }

            const float scale = new_speed / speed;
            velocity.x *= scale;
            velocity.z *= scale;
        }

        void accelerate(
            glm::vec3& velocity, const glm::vec3& wishdir, const float wishspeed, const float accel, const float dt
        )
        {
            if (wishspeed <= 0.0F) { return; }

            const glm::vec2 planar_velocity{velocity.x, velocity.z};
            const glm::vec2 planar_wishdir{wishdir.x, wishdir.z};
            const float     current_speed = glm::dot(planar_velocity, planar_wishdir);
            const float     add_speed     = wishspeed - current_speed;
            if (add_speed <= 0.0F) { return; }

            const float accel_speed = std::min(accel * dt * wishspeed, add_speed);
            velocity.x += wishdir.x * accel_speed;
            velocity.z += wishdir.z * accel_speed;
        }
    } // namespace

    void update_grounded(
        PlayerMotionState&        state,
        const MoveInput&          input,
        const float               yaw_radians,
        const float               dt,
        const CollisionWorld      world,
        const PlayerMotionConfig& config
    )
    {
        if (!state.grounded && state.velocity.y <= 0.0F && probe_ground(world, state.position, config.collision))
        {
            state.grounded = true;
            state.position = snap_to_ground(world, state.position, config.collision);
            if (state.velocity.y < 0.0F) { state.velocity.y = 0.0F; }
        }

        const glm::vec3 forward = make_forward_flat(yaw_radians);
        const glm::vec3 right   = make_right_flat(yaw_radians);

        glm::vec3 wishvel   = (forward * input.forward_axis) + (right * input.right_axis);
        float     wishspeed = glm::length(wishvel);
        glm::vec3 wishdir{0.0F};

        if (wishspeed > 0.0F)
        {
            wishdir   = wishvel / wishspeed;
            wishspeed = config.max_speed * std::min(wishspeed, 1.0F);
            if (input.sprint) { wishspeed *= config.sprint_multiplier; }
        }

        if (state.grounded)
        {
            apply_friction(state.velocity, config, dt);
            accelerate(state.velocity, wishdir, wishspeed, config.ground_accelerate, dt);
        } else
        {
            const float capped_wishspeed = std::min(wishspeed, config.air_wishspeed_cap);
            accelerate(state.velocity, wishdir, capped_wishspeed, config.air_accelerate, dt);
        }

        if (state.grounded && input.jump_pressed)
        {
            state.velocity.y = config.jump_speed;
            state.grounded   = false;
        }

        state.velocity.y -= config.gravity * dt * 0.5F;

        const MoveResult move =
            move_and_collide(world, state.position, state.velocity, dt, config.collision, state.grounded);

        state.position = move.position;
        if (move.hit_x) { state.velocity.x = 0.0F; }
        if (move.hit_z) { state.velocity.z = 0.0F; }
        if (move.hit_y) { state.velocity.y = 0.0F; }

        state.velocity.y -= config.gravity * dt * 0.5F;

        state.grounded = move.grounded;
        if (state.grounded && state.velocity.y <= 0.0F)
        {
            state.position = snap_to_ground(world, state.position, config.collision);
            if (state.velocity.y < 0.0F) { state.velocity.y = 0.0F; }
        }
    }

    void update_noclip(
        PlayerMotionState&        state,
        const MoveInput&          input,
        const float               yaw_radians,
        const float               pitch_radians,
        const float               dt,
        const PlayerMotionConfig& config
    )
    {
        const glm::vec3 forward = make_forward_free(yaw_radians, pitch_radians);
        const glm::vec3 right   = make_right_free(yaw_radians);
        const glm::vec3 up{0.0F, 1.0F, 0.0F};

        glm::vec3 move = (forward * input.forward_axis) + (right * input.right_axis) + (up * input.up_axis);
        if (const float length = glm::length(move); length > 0.0F) { move /= length; }

        float speed = config.noclip_speed;
        if (input.sprint) { speed *= config.noclip_fast_multiplier; }
        if (input.slow) { speed *= config.noclip_slow_multiplier; }

        state.position += move * speed * dt;
        state.velocity = glm::vec3{0.0F};
        state.grounded = false;
    }
} // namespace RelayCore::physics
