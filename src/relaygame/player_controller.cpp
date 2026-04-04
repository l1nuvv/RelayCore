#include "relaygame/player_controller.h"

#include "relaycore/app/engine_services.h"
#include "relaycore/core/log.h"
#include "relaycore/platform/window.h"
#include "relaycore/render/camera.h"

#include <cmath>
#include <glm/common.hpp>
#include <glm/geometric.hpp>

namespace RelayGame
{
    namespace
    {
        constexpr float kPitchClamp = 1.55334306F;

        [[nodiscard]] auto binding_matches(const KeyBinding& binding, const SDL_Scancode scancode) -> bool
        {
            return scancode == binding.primary || scancode == binding.secondary;
        }

        [[nodiscard]] auto binding_pressed(const KeyBinding& binding, const bool* key_states) -> bool
        {
            const bool primary_down = binding.primary != SDL_SCANCODE_UNKNOWN && key_states[binding.primary];
            const bool secondary_down = binding.secondary != SDL_SCANCODE_UNKNOWN && key_states[binding.secondary];
            return primary_down || secondary_down;
        }
    } // namespace

    GroundedPlayerController::GroundedPlayerController(const GroundedPlayerControllerConfig& config) : config_(config)
    {}

    void GroundedPlayerController::set_config(const GroundedPlayerControllerConfig& config) noexcept
    {
        config_ = config;
    }

    void GroundedPlayerController::initialize(const glm::vec3& spawn_position) noexcept
    {
        position_ = spawn_position;
        velocity_ = glm::vec3{0.0F};
        motion_state_.position = spawn_position;
        motion_state_.velocity = glm::vec3{0.0F};
        motion_state_.grounded = false;
        grounded_ = false;
        movement_mode_ = MovementMode::Air;
        use_pressed_ = false;
    }

    void GroundedPlayerController::enable_capture(RelayCore::Window& window) const
    {
        if (!config_.capture_mouse_on_start)
        {
            return;
        }

        window.set_relative_mouse_mode(true);
        RelayCore::log::info("Player capture enabled (movement bindings loaded from config)");
    }

    void GroundedPlayerController::on_event(
        const SDL_Event& event,
        RelayCore::Window& window,
        RelayCore::EngineServices& services
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
                if (event.key.repeat)
                {
                    break;
                }

                if (binding_matches(config_.bindings.quit, event.key.scancode))
                {
                    services.request_quit();
                } else if (binding_matches(config_.bindings.toggle_capture, event.key.scancode))
                {
                    const bool enabled = !window.relative_mouse_mode();
                    window.set_relative_mouse_mode(enabled);
                    RelayCore::log::info("Relative mouse mode {}", enabled ? "enabled" : "disabled");
                } else if (binding_matches(config_.bindings.toggle_vsync, event.key.scancode))
                {
                    (void) window.toggle_vsync();
                } else if (binding_matches(config_.bindings.toggle_noclip, event.key.scancode))
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
                } else if (binding_matches(config_.bindings.use, event.key.scancode))
                {
                    use_pressed_ = true;
                }

                break;
            }

            default:
                break;
        }
    }

    void GroundedPlayerController::flush_pending_look() noexcept
    {
        yaw_radians_ += pending_yaw_delta_;
        pitch_radians_ = glm::clamp(pitch_radians_ + pending_pitch_delta_, -kPitchClamp, kPitchClamp);
        pending_yaw_delta_ = 0.0F;
        pending_pitch_delta_ = 0.0F;
    }

    auto GroundedPlayerController::poll_move_input() const -> RelayCore::physics::MoveInput
    {
        const bool* key_states = SDL_GetKeyboardState(nullptr);

        RelayCore::physics::MoveInput input{};
        if (binding_pressed(config_.bindings.move_forward, key_states))
        {
            input.forward_axis += 1.0F;
        }
        if (binding_pressed(config_.bindings.move_backward, key_states))
        {
            input.forward_axis -= 1.0F;
        }
        if (binding_pressed(config_.bindings.move_right, key_states))
        {
            input.right_axis += 1.0F;
        }
        if (binding_pressed(config_.bindings.move_left, key_states))
        {
            input.right_axis -= 1.0F;
        }
        if (binding_pressed(config_.bindings.jump, key_states))
        {
            input.jump_pressed = true;
        }
        if (binding_pressed(config_.bindings.sprint, key_states))
        {
            input.sprint = true;
        }
        if (binding_pressed(config_.bindings.slow, key_states))
        {
            input.slow = true;
        }

        if (movement_mode_ == MovementMode::Noclip)
        {
            input.up_axis = 0.0F;
            if (binding_pressed(config_.bindings.jump, key_states))
            {
                input.up_axis += 1.0F;
            }
            if (binding_pressed(config_.bindings.noclip_down, key_states))
            {
                input.up_axis -= 1.0F;
            }
            input.jump_pressed = false;
        }

        return input;
    }

    void GroundedPlayerController::apply_camera(RelayCore::render::PerspectiveCamera& camera) const noexcept
    {
        camera.set_position(eye_position_world());
        camera.set_orientation(yaw_radians_, pitch_radians_);
    }

    auto GroundedPlayerController::position() const noexcept -> const glm::vec3&
    {
        return position_;
    }

    auto GroundedPlayerController::velocity() const noexcept -> const glm::vec3&
    {
        return velocity_;
    }

    auto GroundedPlayerController::grounded() const noexcept -> bool
    {
        return grounded_;
    }

    auto GroundedPlayerController::movement_mode() const noexcept -> MovementMode
    {
        return movement_mode_;
    }

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
        use_pressed_ = false;
        return pressed;
    }
} // namespace RelayGame
