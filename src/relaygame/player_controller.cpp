#include "relaygame/player_controller.h"

#include <cmath>
#include <glm/common.hpp>
#include <glm/geometric.hpp>

#include "relaycore/app/engine_services.h"
#include "relaycore/core/log.h"
#include "relaycore/core/profiling.h"
#include "relaycore/platform/window.h"
#include "relaycore/render/camera.h"
#include "relaycore/world/blockout_level.h"

namespace RelayGame
{
    namespace
    {
        constexpr float kPitchClamp = 1.55334306F;
    } // namespace

    GroundedPlayerController::GroundedPlayerController(const GroundedPlayerControllerConfig& config) : config_(config)
    {}

    void GroundedPlayerController::initialize(const glm::vec3& spawn_position) noexcept
    {
        position_              = spawn_position;
        velocity_              = glm::vec3{0.0F};
        motion_state_.position = spawn_position;
        motion_state_.velocity = glm::vec3{0.0F};
        motion_state_.grounded = false;
        grounded_              = false;
        movement_mode_         = MovementMode::Air;
        use_pressed_           = false;
    }

    void GroundedPlayerController::enable_capture(RelayCore::Window& window) const
    {
        if (!config_.capture_mouse_on_start) { return; }

        window.set_relative_mouse_mode(true);
        RelayCore::log::info(
            "Player capture enabled (WASD move, Space jump/up, Ctrl down in noclip, Shift sprint, E use, F1 mouse, F3 "
            "noclip, Esc quit)"
        );
    }

    void GroundedPlayerController::toggle_noclip() noexcept
    {
        if (movement_mode_ == MovementMode::Noclip)
        {
            movement_mode_ = MovementMode::Air;
            grounded_      = false;
        } else
        {
            movement_mode_ = MovementMode::Noclip;
            grounded_      = false;
        }

        velocity_              = glm::vec3{0.0F};
        motion_state_.velocity = glm::vec3{0.0F};
        motion_state_.grounded = false;
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
                    (void) window.toggle_vsync();
                } else if (event.key.scancode == SDL_SCANCODE_F3)
                {
                    toggle_noclip();
                    const char* label = "grounded";
                    if (movement_mode_ == MovementMode::Air)
                    {
                        label = "air";
                    } else if (movement_mode_ == MovementMode::Noclip)
                    {
                        label = "noclip";
                    }
                    RelayCore::log::info("Movement mode: {}", label);
                } else if (event.key.scancode == SDL_SCANCODE_E)
                {
                    use_pressed_ = true;
                }

                break;
            }

            default: break;
        }
    }

    void GroundedPlayerController::update(
        const RelayCore::FrameInfo&               frame,
        const RelayCore::BlockoutLevel&           level,
        std::span<const RelayCore::physics::Aabb> dynamic_blockers
    )
    {
        RELAYCORE_PROFILE_ZONE_N("GroundedPlayerController::update");

        yaw_radians_ += pending_yaw_delta_;
        pitch_radians_       = glm::clamp(pitch_radians_ + pending_pitch_delta_, -kPitchClamp, kPitchClamp);
        pending_yaw_delta_   = 0.0F;
        pending_pitch_delta_ = 0.0F;

        if (movement_mode_ == MovementMode::Noclip)
        {
            update_noclip(frame);
            return;
        }

        update_grounded(frame, level, dynamic_blockers);
    }

    void GroundedPlayerController::update_grounded(
        const RelayCore::FrameInfo&               frame,
        const RelayCore::BlockoutLevel&           level,
        std::span<const RelayCore::physics::Aabb> dynamic_blockers
    )
    {
        RELAYCORE_PROFILE_ZONE_N("physics::update_grounded");

        const auto world  = build_collision_world(level, dynamic_blockers);
        const auto input  = poll_move_input();
        const auto config = build_motion_config();

        motion_state_.position = position_;
        motion_state_.velocity = velocity_;
        motion_state_.grounded = grounded_;

        RelayCore::physics::update_grounded(
            motion_state_,
            input,
            yaw_radians_,
            static_cast<float>(frame.delta_seconds),
            RelayCore::physics::CollisionWorld{world},
            config
        );

        position_ = motion_state_.position;
        velocity_ = motion_state_.velocity;

        grounded_      = motion_state_.grounded;
        movement_mode_ = grounded_ ? MovementMode::Grounded : MovementMode::Air;
    }

    void GroundedPlayerController::update_noclip(const RelayCore::FrameInfo& frame)
    {
        const auto input  = poll_move_input();
        const auto config = build_motion_config();

        motion_state_.position = position_;
        motion_state_.velocity = glm::vec3{0.0F};
        motion_state_.grounded = false;

        RelayCore::physics::update_noclip(
            motion_state_, input, yaw_radians_, pitch_radians_, static_cast<float>(frame.delta_seconds), config
        );

        position_      = motion_state_.position;
        velocity_      = glm::vec3{0.0F};
        grounded_      = false;
        movement_mode_ = MovementMode::Noclip;
    }

    auto GroundedPlayerController::poll_move_input() const -> RelayCore::physics::MoveInput
    {
        const bool* key_states = SDL_GetKeyboardState(nullptr);

        RelayCore::physics::MoveInput input{};
        if (key_states[SDL_SCANCODE_W]) { input.forward_axis += 1.0F; }
        if (key_states[SDL_SCANCODE_S]) { input.forward_axis -= 1.0F; }
        if (key_states[SDL_SCANCODE_D]) { input.right_axis += 1.0F; }
        if (key_states[SDL_SCANCODE_A]) { input.right_axis -= 1.0F; }
        if (key_states[SDL_SCANCODE_SPACE]) { input.jump_pressed = true; }
        if (key_states[SDL_SCANCODE_LSHIFT] || key_states[SDL_SCANCODE_RSHIFT]) { input.sprint = true; }
        if (key_states[SDL_SCANCODE_LALT] || key_states[SDL_SCANCODE_RALT]) { input.slow = true; }

        if (movement_mode_ == MovementMode::Noclip)
        {
            input.up_axis = 0.0F;
            if (key_states[SDL_SCANCODE_SPACE]) { input.up_axis += 1.0F; }
            if (key_states[SDL_SCANCODE_LCTRL] || key_states[SDL_SCANCODE_RCTRL]) { input.up_axis -= 1.0F; }
            input.jump_pressed = false;
        }

        return input;
    }

    auto GroundedPlayerController::build_collision_world(
        const RelayCore::BlockoutLevel& level, std::span<const RelayCore::physics::Aabb> dynamic_blockers
    ) const -> std::vector<RelayCore::physics::Aabb>
    {
        std::vector<RelayCore::physics::Aabb> world;
        world.reserve(level.solids.size() + dynamic_blockers.size());

        for (const RelayCore::BlockoutSolid& solid: level.solids)
        {
            world.push_back({solid.mins, solid.maxs});
        }

        for (const RelayCore::physics::Aabb& blocker: dynamic_blockers)
        {
            world.push_back(blocker);
        }

        return world;
    }

    auto GroundedPlayerController::build_motion_config() const -> RelayCore::physics::PlayerMotionConfig
    {
        RelayCore::physics::PlayerMotionConfig motion_config{};
        motion_config.max_speed              = config_.move_speed;
        motion_config.sprint_multiplier      = config_.sprint_multiplier;
        motion_config.ground_accelerate      = config_.ground_acceleration;
        motion_config.air_accelerate         = config_.air_acceleration;
        motion_config.ground_friction        = config_.ground_friction;
        motion_config.stop_speed             = config_.stop_speed;
        motion_config.gravity                = config_.gravity;
        motion_config.jump_speed             = config_.jump_speed;
        motion_config.air_wishspeed_cap      = config_.move_speed * 0.30F;
        motion_config.collision              = build_collision_config();
        motion_config.noclip_speed           = noclip_speed_;
        motion_config.noclip_fast_multiplier = noclip_fast_multiplier_;
        motion_config.noclip_slow_multiplier = noclip_slow_multiplier_;
        return motion_config;
    }

    auto GroundedPlayerController::build_collision_config() const -> RelayCore::physics::CollisionConfig
    {
        RelayCore::physics::CollisionConfig collision_config{};
        collision_config.radius                   = config_.collider_radius;
        collision_config.height                   = config_.collider_height;
        collision_config.epsilon                  = config_.collision_epsilon;
        collision_config.step_height              = config_.step_height;
        collision_config.ground_probe_distance    = config_.ground_probe_distance;
        collision_config.max_ground_snap_distance = config_.max_ground_snap_distance;
        return collision_config;
    }

    void GroundedPlayerController::apply_camera(RelayCore::render::PerspectiveCamera& camera) const noexcept
    {
        camera.set_position(eye_position_world());
        camera.set_orientation(yaw_radians_, pitch_radians_);
    }

    auto GroundedPlayerController::position() const noexcept -> const glm::vec3& { return position_; }

    auto GroundedPlayerController::velocity() const noexcept -> const glm::vec3& { return velocity_; }

    auto GroundedPlayerController::grounded() const noexcept -> bool { return grounded_; }

    auto GroundedPlayerController::movement_mode() const noexcept -> MovementMode { return movement_mode_; }

    auto GroundedPlayerController::eye_position_world() const noexcept -> glm::vec3
    {
        return position_ + glm::vec3{0.0F, config_.eye_height, 0.0F};
    }

    auto GroundedPlayerController::view_forward() const noexcept -> glm::vec3
    {
        const float cos_pitch = std::cos(pitch_radians_);
        return glm::normalize(
            glm::vec3{
                std::cos(yaw_radians_) * cos_pitch,
                std::sin(pitch_radians_),
                std::sin(yaw_radians_) * cos_pitch,
            }
        );
    }

    auto GroundedPlayerController::player_bounds() const noexcept -> RelayCore::physics::Aabb
    {
        return RelayCore::physics::make_player_aabb(position_, config_.collider_radius, config_.collider_height);
    }

    auto GroundedPlayerController::consume_use_pressed() noexcept -> bool
    {
        const bool pressed = use_pressed_;
        use_pressed_       = false;
        return pressed;
    }
} // namespace RelayGame
