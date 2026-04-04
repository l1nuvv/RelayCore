#include "relaygame/player_controller.h"

#include <algorithm>
#include <cmath>
#include <glm/common.hpp>
#include <glm/geometric.hpp>

#include "relaycore/app/engine_services.h"
#include "relaycore/core/log.h"
#include "relaycore/platform/window.h"
#include "relaycore/render/camera.h"
#include "relaycore/world/blockout_level.h"

namespace RelayGame
{
    namespace
    {
        constexpr float kPitchClamp = 1.55334306F;
        constexpr float kEpsilon    = 0.0001F;

        struct Aabb {
            glm::vec3 mins{};
            glm::vec3 maxs{};
        };

        struct AxisMoveResult {
            float position_axis = 0.0F;
            bool  hit           = false;
        };

        [[nodiscard]] auto make_player_aabb(const glm::vec3& position, const GroundedPlayerControllerConfig& config)
            -> Aabb
        {
            return {
                {position.x - config.collider_radius, position.y, position.z - config.collider_radius},
                {position.x + config.collider_radius,
                 position.y + config.collider_height,
                 position.z + config.collider_radius},
            };
        }

        [[nodiscard]] auto make_solid_aabb(const RelayCore::BlockoutSolid& solid) -> Aabb
        {
            return {solid.mins, solid.maxs};
        }

        [[nodiscard]] auto overlaps(const Aabb& a, const Aabb& b) -> bool
        {
            return a.mins.x < b.maxs.x - kEpsilon && a.maxs.x > b.mins.x + kEpsilon && a.mins.y < b.maxs.y - kEpsilon
                   && a.maxs.y > b.mins.y + kEpsilon && a.mins.z < b.maxs.z - kEpsilon
                   && a.maxs.z > b.mins.z + kEpsilon;
        }

        [[nodiscard]] auto accelerate_towards(const glm::vec3& current, const glm::vec3& target, const float max_delta)
            -> glm::vec3
        {
            const glm::vec3 delta        = target - current;
            const float     delta_length = glm::length(delta);
            if (delta_length <= kEpsilon) { return target; }
            if (delta_length <= max_delta) { return target; }
            return current + (delta / delta_length) * max_delta;
        }

        void apply_ground_friction(glm::vec3& velocity, const float friction, const float dt)
        {
            glm::vec2   planar{velocity.x, velocity.z};
            const float speed = glm::length(planar);
            if (speed <= kEpsilon)
            {
                velocity.x = 0.0F;
                velocity.z = 0.0F;
                return;
            }

            const float drop      = speed * friction * dt;
            const float new_speed = std::max(speed - drop, 0.0F);
            if (new_speed <= kEpsilon)
            {
                velocity.x = 0.0F;
                velocity.z = 0.0F;
                return;
            }

            planar *= new_speed / speed;
            velocity.x = planar.x;
            velocity.z = planar.y;
        }

        [[nodiscard]] auto move_axis(
            const RelayCore::BlockoutLevel&       level,
            const glm::vec3&                      start_position,
            const GroundedPlayerControllerConfig& config,
            const int                             axis,
            const float                           delta
        ) -> AxisMoveResult
        {
            glm::vec3 candidate = start_position;
            candidate[axis] += delta;

            AxisMoveResult result{};
            result.position_axis = candidate[axis];

            for (const RelayCore::BlockoutSolid& solid: level.solids)
            {
                const Aabb player_aabb = make_player_aabb(candidate, config);
                const Aabb solid_aabb  = make_solid_aabb(solid);
                if (!overlaps(player_aabb, solid_aabb)) { continue; }

                result.hit = true;

                const float player_min_offset = player_aabb.mins[axis] - candidate[axis];
                const float player_max_offset = player_aabb.maxs[axis] - candidate[axis];

                if (delta > 0.0F)
                {
                    result.position_axis = std::min(result.position_axis, solid_aabb.mins[axis] - player_max_offset);
                } else if (delta < 0.0F)
                {
                    result.position_axis = std::max(result.position_axis, solid_aabb.maxs[axis] - player_min_offset);
                }

                candidate[axis] = result.position_axis;
            }

            return result;
        }

        [[nodiscard]] auto horizontal_forward_from_yaw(const float yaw_radians) -> glm::vec3
        {
            return glm::normalize(glm::vec3{std::cos(yaw_radians), 0.0F, std::sin(yaw_radians)});
        }

        [[nodiscard]] auto right_from_yaw(const float yaw_radians) -> glm::vec3
        {
            const glm::vec3 forward = horizontal_forward_from_yaw(yaw_radians);
            return glm::normalize(glm::cross(forward, glm::vec3{0.0F, 1.0F, 0.0F}));
        }

        [[nodiscard]] auto full_forward_from_angles(const float yaw_radians, const float pitch_radians) -> glm::vec3
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
    } // namespace

    GroundedPlayerController::GroundedPlayerController(const GroundedPlayerControllerConfig& config) : config_(config)
    {}

    void GroundedPlayerController::initialize(const glm::vec3& spawn_position) noexcept
    {
        position_      = spawn_position;
        velocity_      = glm::vec3{0.0F};
        grounded_      = false;
        movement_mode_ = MovementMode::Grounded;
    }

    void GroundedPlayerController::enable_capture(RelayCore::Window& window) const
    {
        if (!config_.capture_mouse_on_start) { return; }

        window.set_relative_mouse_mode(true);
        RelayCore::log::info(
            "Player capture enabled (WASD move, Space jump/up, Ctrl down in noclip, Shift sprint, F1 mouse, F3 noclip, "
            "Esc quit)"
        );
    }

    void GroundedPlayerController::toggle_noclip() noexcept
    {
        movement_mode_ = (movement_mode_ == MovementMode::Grounded) ? MovementMode::Noclip : MovementMode::Grounded;
        velocity_      = glm::vec3{0.0F};
        grounded_      = false;
    }

    void GroundedPlayerController::on_event(
        const SDL_Event& event, RelayCore::Window& window, RelayCore::EngineServices& services
    )
    {
        switch (event.type)
        {
            case SDL_EVENT_MOUSE_MOTION:
            {
                if (window.relative_mouse_mode())
                {
                    pending_yaw_delta_ += event.motion.xrel * config_.mouse_sensitivity_radians;
                    pending_pitch_delta_ -= event.motion.yrel * config_.mouse_sensitivity_radians;
                }
                break;
            }

            case SDL_EVENT_KEY_DOWN:
            {
                if (event.key.repeat) { break; }

                if (event.key.scancode == SDL_SCANCODE_ESCAPE)
                {
                    services.request_quit();
                } else if (event.key.scancode == SDL_SCANCODE_F1)
                {
                    const bool enabled = !window.relative_mouse_mode();
                    window.set_relative_mouse_mode(enabled);
                    RelayCore::log::info("Relative mouse mode {}", enabled ? "enabled" : "disabled");
                } else if (event.key.scancode == SDL_SCANCODE_V)
                {
                    window.toggle_vsync();
                } else if (event.key.scancode == SDL_SCANCODE_F3)
                {
                    toggle_noclip();
                    RelayCore::log::info(
                        "Movement mode: {}", movement_mode_ == MovementMode::Noclip ? "noclip" : "grounded"
                    );
                }

                break;
            }

            default: break;
        }
    }

    void GroundedPlayerController::update(const RelayCore::FrameInfo& frame, const RelayCore::BlockoutLevel& level)
    {
        yaw_radians_ += pending_yaw_delta_;
        pitch_radians_       = glm::clamp(pitch_radians_ + pending_pitch_delta_, -kPitchClamp, kPitchClamp);
        pending_yaw_delta_   = 0.0F;
        pending_pitch_delta_ = 0.0F;

        if (movement_mode_ == MovementMode::Noclip)
        {
            update_noclip(frame);
            return;
        }

        update_grounded(frame, level);
    }

    void
    GroundedPlayerController::update_grounded(const RelayCore::FrameInfo& frame, const RelayCore::BlockoutLevel& level)
    {
        const float dt         = static_cast<float>(frame.delta_seconds);
        const bool* key_states = SDL_GetKeyboardState(nullptr);

        float speed = config_.move_speed;
        if (key_states[SDL_SCANCODE_LSHIFT] || key_states[SDL_SCANCODE_RSHIFT]) { speed *= config_.sprint_multiplier; }

        float forward_axis = 0.0F;
        float right_axis   = 0.0F;

        if (key_states[SDL_SCANCODE_W]) { forward_axis += 1.0F; }
        if (key_states[SDL_SCANCODE_S]) { forward_axis -= 1.0F; }
        if (key_states[SDL_SCANCODE_D]) { right_axis += 1.0F; }
        if (key_states[SDL_SCANCODE_A]) { right_axis -= 1.0F; }

        const glm::vec3 forward = horizontal_forward_from_yaw(yaw_radians_);
        const glm::vec3 right   = right_from_yaw(yaw_radians_);

        glm::vec3 desired_direction = forward * forward_axis + right * right_axis;
        if (glm::dot(desired_direction, desired_direction) > 0.0F)
        {
            desired_direction = glm::normalize(desired_direction);
        }

        if (grounded_ && glm::dot(desired_direction, desired_direction) <= 0.0F)
        {
            apply_ground_friction(velocity_, config_.ground_friction, dt);
        }

        const glm::vec3 desired_planar_velocity = desired_direction * speed;
        glm::vec3       current_planar_velocity{velocity_.x, 0.0F, velocity_.z};
        current_planar_velocity = accelerate_towards(
            current_planar_velocity,
            desired_planar_velocity,
            (grounded_ ? config_.ground_acceleration : config_.air_acceleration) * dt
        );
        velocity_.x = current_planar_velocity.x;
        velocity_.z = current_planar_velocity.z;

        if (grounded_ && key_states[SDL_SCANCODE_SPACE])
        {
            velocity_.y = config_.jump_speed;
            grounded_   = false;
        }

        velocity_.y -= config_.gravity * dt;

        const AxisMoveResult move_x = move_axis(level, position_, config_, 0, velocity_.x * dt);
        position_.x                 = move_x.position_axis;
        if (move_x.hit) { velocity_.x = 0.0F; }

        const AxisMoveResult move_z = move_axis(level, position_, config_, 2, velocity_.z * dt);
        position_.z                 = move_z.position_axis;
        if (move_z.hit) { velocity_.z = 0.0F; }

        const bool           moving_down = velocity_.y <= 0.0F;
        const AxisMoveResult move_y      = move_axis(level, position_, config_, 1, velocity_.y * dt);
        position_.y                      = move_y.position_axis;
        if (move_y.hit)
        {
            velocity_.y = 0.0F;
            grounded_   = moving_down;
        } else
        {
            grounded_ = false;
        }
    }

    void GroundedPlayerController::update_noclip(const RelayCore::FrameInfo& frame)
    {
        const float dt         = static_cast<float>(frame.delta_seconds);
        const bool* key_states = SDL_GetKeyboardState(nullptr);

        float speed = noclip_speed_;
        if (key_states[SDL_SCANCODE_LSHIFT] || key_states[SDL_SCANCODE_RSHIFT]) { speed *= noclip_fast_multiplier_; }
        if (key_states[SDL_SCANCODE_LALT] || key_states[SDL_SCANCODE_RALT]) { speed *= noclip_slow_multiplier_; }

        float forward_axis = 0.0F;
        float right_axis   = 0.0F;
        float up_axis      = 0.0F;

        if (key_states[SDL_SCANCODE_W]) { forward_axis += 1.0F; }
        if (key_states[SDL_SCANCODE_S]) { forward_axis -= 1.0F; }
        if (key_states[SDL_SCANCODE_D]) { right_axis += 1.0F; }
        if (key_states[SDL_SCANCODE_A]) { right_axis -= 1.0F; }
        if (key_states[SDL_SCANCODE_SPACE]) { up_axis += 1.0F; }
        if (key_states[SDL_SCANCODE_LCTRL] || key_states[SDL_SCANCODE_RCTRL]) { up_axis -= 1.0F; }

        const glm::vec3 forward = full_forward_from_angles(yaw_radians_, pitch_radians_);
        const glm::vec3 right   = right_from_yaw(yaw_radians_);
        const glm::vec3 up{0.0F, 1.0F, 0.0F};

        glm::vec3 move = forward * forward_axis + right * right_axis + up * up_axis;
        if (glm::dot(move, move) > 0.0F) { move = glm::normalize(move); }

        position_ += move * speed * dt;
        velocity_ = glm::vec3{0.0F};
        grounded_ = false;
    }

    void GroundedPlayerController::apply_camera(RelayCore::render::PerspectiveCamera& camera) const noexcept
    {
        camera.set_position(eye_position());
        camera.set_orientation(yaw_radians_, pitch_radians_);
    }

    auto GroundedPlayerController::position() const noexcept -> const glm::vec3& { return position_; }

    auto GroundedPlayerController::velocity() const noexcept -> const glm::vec3& { return velocity_; }

    auto GroundedPlayerController::grounded() const noexcept -> bool { return grounded_; }

    auto GroundedPlayerController::movement_mode() const noexcept -> MovementMode { return movement_mode_; }

    auto GroundedPlayerController::eye_position() const noexcept -> glm::vec3
    {
        return position_ + glm::vec3{0.0F, config_.eye_height, 0.0F};
    }
} // namespace RelayGame
