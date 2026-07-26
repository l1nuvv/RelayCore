#include "relaygame/player_controller.h"

#include "relaycore/app/engine_services.h"
#include "relaycore/core/log.h"
#include "relaycore/core/profiling.h"
#include "relaycore/platform/window.h"
#include "relaycore/render/camera.h"
#include "relaycore/render/renderer.h"

#include <cmath>
#include <glm/common.hpp>
#include <glm/geometric.hpp>

namespace RelayGame
{
    namespace
    {
        constexpr float kPitchClamp = 1.55334306F;
        constexpr float kDefaultYaw = -1.5707963268F;

        [[nodiscard]] auto binding_matches(const KeyBinding& binding, const SDL_Scancode scancode) -> bool
        {
            RELAYCORE_PROFILE_FUNCTION();
            return scancode == binding.primary || scancode == binding.secondary;
        }

        [[nodiscard]] auto binding_pressed(const KeyBinding& binding, const bool* key_states) -> bool
        {
            RELAYCORE_PROFILE_FUNCTION();
            const bool primary_down = binding.primary != SDL_SCANCODE_UNKNOWN && key_states[binding.primary];
            const bool secondary_down = binding.secondary != SDL_SCANCODE_UNKNOWN && key_states[binding.secondary];
            return primary_down || secondary_down;
        }
    } // namespace

    GroundedPlayerController::GroundedPlayerController(const GroundedPlayerControllerConfig& config) : config_(config)
    {
        RELAYCORE_PROFILE_FUNCTION();
    }

    void GroundedPlayerController::set_config(const GroundedPlayerControllerConfig& config) noexcept
    {
        RELAYCORE_PROFILE_FUNCTION();
        config_ = config;
    }

    void GroundedPlayerController::initialize(
        const glm::vec3& spawn_position,
        const std::optional<glm::vec2> spawn_view_angles
    ) noexcept
    {
        RELAYCORE_PROFILE_FUNCTION();
        position_ = spawn_position;
        velocity_ = glm::vec3{0.0F};
        motion_state_.position = spawn_position;
        motion_state_.velocity = glm::vec3{0.0F};
        motion_state_.grounded = false;
        yaw_radians_ = kDefaultYaw;
        pitch_radians_ = 0.0F;
        if (spawn_view_angles.has_value())
        {
            yaw_radians_ = spawn_view_angles->x;
            pitch_radians_ = glm::clamp(spawn_view_angles->y, -kPitchClamp, kPitchClamp);
        }
        grounded_ = false;
        movement_mode_ = MovementMode::Air;
        fire_pressed_ = false;
        use_pressed_ = false;
        inspector_pin_pressed_ = false;
        reload_level_pressed_ = false;
        open_inspector_source_pressed_ = false;
        save_runtime_snapshot_pressed_ = false;
        restore_runtime_snapshot_pressed_ = false;
        imgui_keyboard_captured_ = false;
        imgui_mouse_captured_ = false;
        window_has_focus_ = true;
        window_minimized_ = false;
        pending_yaw_delta_ = 0.0F;
        pending_pitch_delta_ = 0.0F;
        movement_diagnostics_ = {};
        collision_query_diagnostics_ = {};
    }

    void GroundedPlayerController::enable_capture(RelayCore::Window& window) const
    {
        RELAYCORE_PROFILE_FUNCTION();
        if (!config_.capture_mouse_on_start)
        {
            return;
        }

        window.set_relative_mouse_mode(true);
        RelayCore::log::info("Player capture enabled (movement bindings loaded from config)");
    }

    void GroundedPlayerController::set_imgui_capture(const bool keyboard_captured, const bool mouse_captured) noexcept
    {
        RELAYCORE_PROFILE_FUNCTION();
        if (!imgui_keyboard_captured_ && keyboard_captured)
        {
            clear_keyboard_transient_input();
        }
        if (!imgui_mouse_captured_ && mouse_captured)
        {
            clear_mouse_transient_input();
        }

        imgui_keyboard_captured_ = keyboard_captured;
        imgui_mouse_captured_ = mouse_captured;
    }

    void GroundedPlayerController::on_event(
        const SDL_Event& event,
        RelayCore::Window& window,
        RelayCore::EngineServices& services
    )
    {
        RELAYCORE_PROFILE_FUNCTION();
        switch (event.type)
        {
            case SDL_EVENT_WINDOW_FOCUS_LOST:
                window_has_focus_ = false;
                clear_focus_loss_input();
                break;

            case SDL_EVENT_WINDOW_FOCUS_GAINED:
                window_has_focus_ = true;
                break;

            case SDL_EVENT_WINDOW_MINIMIZED:
                window_minimized_ = true;
                clear_focus_loss_input();
                break;

            case SDL_EVENT_WINDOW_RESTORED:
                window_minimized_ = false;
                break;

            case SDL_EVENT_MOUSE_MOTION:
            {
                if (gameplay_mouse_input_enabled() && window.relative_mouse_mode())
                {
                    pending_yaw_delta_ += event.motion.xrel * config_.mouse_sensitivity_radians;
                    pending_pitch_delta_ -= event.motion.yrel * config_.mouse_sensitivity_radians;
                }
                break;
            }

            case SDL_EVENT_MOUSE_BUTTON_DOWN:
            {
                if (gameplay_mouse_input_enabled() && window.relative_mouse_mode()
                    && event.button.button == config_.bindings.primary_attack_button)
                {
                    fire_pressed_ = true;
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
                } else if (binding_matches(config_.bindings.toggle_inspector, event.key.scancode))
                {
                    services.renderer().toggle_inspector_visualization_enabled();
                    RelayCore::log::info(
                        "Inspector highlight {}",
                        services.renderer().inspector_visualization_enabled() ? "enabled" : "disabled"
                    );
                } else if (binding_matches(config_.bindings.pin_inspector, event.key.scancode))
                {
                    inspector_pin_pressed_ = true;
                } else if (binding_matches(config_.bindings.reload_level, event.key.scancode))
                {
                    reload_level_pressed_ = true;
                } else if (binding_matches(config_.bindings.open_inspector_source, event.key.scancode))
                {
                    open_inspector_source_pressed_ = true;
                } else if (binding_matches(config_.bindings.save_runtime_snapshot, event.key.scancode))
                {
                    save_runtime_snapshot_pressed_ = true;
                } else if (binding_matches(config_.bindings.restore_runtime_snapshot, event.key.scancode))
                {
                    restore_runtime_snapshot_pressed_ = true;
                } else if (gameplay_keyboard_input_enabled()
                           && binding_matches(config_.bindings.use, event.key.scancode))
                {
                    use_pressed_ = true;
                }

                break;
            }

            default:
                break;
        }
    }

    void GroundedPlayerController::clear_keyboard_transient_input() noexcept
    {
        use_pressed_ = false;
    }

    void GroundedPlayerController::clear_mouse_transient_input() noexcept
    {
        fire_pressed_ = false;
        pending_yaw_delta_ = 0.0F;
        pending_pitch_delta_ = 0.0F;
    }

    void GroundedPlayerController::clear_transient_input() noexcept
    {
        clear_keyboard_transient_input();
        clear_mouse_transient_input();
    }

    void GroundedPlayerController::clear_focus_loss_input() noexcept
    {
        clear_transient_input();
        inspector_pin_pressed_ = false;
        reload_level_pressed_ = false;
        open_inspector_source_pressed_ = false;
        save_runtime_snapshot_pressed_ = false;
        restore_runtime_snapshot_pressed_ = false;
    }

    auto GroundedPlayerController::gameplay_keyboard_input_enabled() const noexcept -> bool
    {
        return window_has_focus_ && !window_minimized_ && !imgui_keyboard_captured_;
    }

    auto GroundedPlayerController::gameplay_mouse_input_enabled() const noexcept -> bool
    {
        return window_has_focus_ && !window_minimized_ && !imgui_mouse_captured_;
    }

    void GroundedPlayerController::flush_pending_look() noexcept
    {
        RELAYCORE_PROFILE_FUNCTION();
        yaw_radians_ += pending_yaw_delta_;
        pitch_radians_ = glm::clamp(pitch_radians_ + pending_pitch_delta_, -kPitchClamp, kPitchClamp);
        pending_yaw_delta_ = 0.0F;
        pending_pitch_delta_ = 0.0F;
    }

    auto GroundedPlayerController::poll_move_input() const -> RelayCore::physics::MoveInput
    {
        RELAYCORE_PROFILE_FUNCTION();
        if (!gameplay_keyboard_input_enabled())
        {
            return {};
        }
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
        RELAYCORE_PROFILE_FUNCTION();
        camera.set_position(eye_position_world());
        camera.set_orientation(yaw_radians_, pitch_radians_);
    }

    auto GroundedPlayerController::position() const noexcept -> const glm::vec3&
    {
        RELAYCORE_PROFILE_FUNCTION();
        return position_;
    }

    auto GroundedPlayerController::velocity() const noexcept -> const glm::vec3&
    {
        RELAYCORE_PROFILE_FUNCTION();
        return velocity_;
    }

    auto GroundedPlayerController::grounded() const noexcept -> bool
    {
        RELAYCORE_PROFILE_FUNCTION();
        return grounded_;
    }

    auto GroundedPlayerController::movement_mode() const noexcept -> MovementMode
    {
        RELAYCORE_PROFILE_FUNCTION();
        return movement_mode_;
    }

    auto GroundedPlayerController::eye_position_world() const noexcept -> glm::vec3
    {
        RELAYCORE_PROFILE_FUNCTION();
        return position_ + glm::vec3{0.0F, config_.eye_height, 0.0F};
    }

    auto GroundedPlayerController::view_forward() const noexcept -> glm::vec3
    {
        RELAYCORE_PROFILE_FUNCTION();
        const float cos_pitch = std::cos(pitch_radians_);
        return glm::normalize(glm::vec3{
            std::cos(yaw_radians_) * cos_pitch,
            std::sin(pitch_radians_),
            std::sin(yaw_radians_) * cos_pitch,
        });
    }

    auto GroundedPlayerController::player_bounds() const noexcept -> RelayCore::physics::Aabb
    {
        RELAYCORE_PROFILE_FUNCTION();
        return RelayCore::physics::make_player_aabb(position_, config_.collider_radius, config_.collider_height);
    }

    auto GroundedPlayerController::consume_use_pressed() noexcept -> bool
    {
        RELAYCORE_PROFILE_FUNCTION();
        const bool pressed = use_pressed_;
        use_pressed_ = false;
        return pressed;
    }

    auto GroundedPlayerController::consume_fire_pressed() noexcept -> bool
    {
        RELAYCORE_PROFILE_FUNCTION();
        const bool pressed = fire_pressed_;
        fire_pressed_ = false;
        return pressed;
    }

    auto GroundedPlayerController::consume_inspector_pin_pressed() noexcept -> bool
    {
        RELAYCORE_PROFILE_FUNCTION();
        const bool pressed = inspector_pin_pressed_;
        inspector_pin_pressed_ = false;
        return pressed;
    }

    auto GroundedPlayerController::consume_reload_level_pressed() noexcept -> bool
    {
        RELAYCORE_PROFILE_FUNCTION();
        const bool pressed = reload_level_pressed_;
        reload_level_pressed_ = false;
        return pressed;
    }

    auto GroundedPlayerController::consume_open_inspector_source_pressed() noexcept -> bool
    {
        RELAYCORE_PROFILE_FUNCTION();
        const bool pressed = open_inspector_source_pressed_;
        open_inspector_source_pressed_ = false;
        return pressed;
    }

    auto GroundedPlayerController::consume_save_runtime_snapshot_pressed() noexcept -> bool
    {
        RELAYCORE_PROFILE_FUNCTION();
        const bool pressed = save_runtime_snapshot_pressed_;
        save_runtime_snapshot_pressed_ = false;
        return pressed;
    }

    auto GroundedPlayerController::consume_restore_runtime_snapshot_pressed() noexcept -> bool
    {
        RELAYCORE_PROFILE_FUNCTION();
        const bool pressed = restore_runtime_snapshot_pressed_;
        restore_runtime_snapshot_pressed_ = false;
        return pressed;
    }

    auto GroundedPlayerController::collision_query_diagnostics() const noexcept -> const CollisionQueryDiagnostics&
    {
        RELAYCORE_PROFILE_FUNCTION();
        return collision_query_diagnostics_;
    }
} // namespace RelayGame
