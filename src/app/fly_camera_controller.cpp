#include "app/fly_camera_controller.hpp"

#include <SDL3/SDL.h>
#include <glm/geometric.hpp>

#include "core/log.hpp"
#include "platform/window.hpp"
#include "render/camera.hpp"

namespace RelayCore
{
    FlyCameraController::FlyCameraController(const FlyCameraControllerConfig& config) : config_(config) {}

    void FlyCameraController::enable_capture(Window& window) const
    {
        if (!config_.capture_mouse_on_start) { return; }

        window.set_relative_mouse_mode(true);
        log::info("Fly camera capture enabled (WASD + mouse, Shift sprint, Q/E vertical, F1 toggle mouse, Esc quit)");
    }

    void FlyCameraController::on_event(const SDL_Event& event, Window& window, bool& application_running)
    {
        switch (event.type)
        {
            case SDL_EVENT_MOUSE_MOTION:
                if (window.relative_mouse_mode())
                {
                    pending_yaw_delta_ += event.motion.xrel * config_.mouse_sensitivity_radians;
                    pending_pitch_delta_ -= event.motion.yrel * config_.mouse_sensitivity_radians;
                }
                break;

            case SDL_EVENT_KEY_DOWN:
                if (event.key.repeat) { break; }

                if (event.key.scancode == SDL_SCANCODE_ESCAPE)
                {
                    application_running = false;
                } else if (event.key.scancode == SDL_SCANCODE_F1) {
                    const bool enabled = !window.relative_mouse_mode();
                    window.set_relative_mouse_mode(enabled);
                    log::info("Relative mouse mode {}", enabled ? "enabled" : "disabled");
                }
                break;

            default: break;
        }
    }

    void FlyCameraController::update(const FrameInfo& frame, render::PerspectiveCamera& camera)
    {
        camera.add_yaw_pitch(pending_yaw_delta_, pending_pitch_delta_);

        const bool* key_states = SDL_GetKeyboardState(nullptr);
        float       speed      = config_.move_speed;
        if (key_states[SDL_SCANCODE_LSHIFT] || key_states[SDL_SCANCODE_RSHIFT]) { speed *= config_.sprint_multiplier; }

        const float movement_step = speed * static_cast<float>(frame.delta_seconds);

        float forward_axis = 0.0F;
        float right_axis   = 0.0F;
        float up_axis      = 0.0F;

        if (key_states[SDL_SCANCODE_W]) { forward_axis += 1.0F; }
        if (key_states[SDL_SCANCODE_S]) { forward_axis -= 1.0F; }
        if (key_states[SDL_SCANCODE_D]) { right_axis += 1.0F; }
        if (key_states[SDL_SCANCODE_A]) { right_axis -= 1.0F; }
        if (key_states[SDL_SCANCODE_E]) { up_axis += 1.0F; }
        if (key_states[SDL_SCANCODE_Q]) { up_axis -= 1.0F; }

        glm::vec3 movement{right_axis, up_axis, forward_axis};
        if (glm::dot(movement, movement) > 0.0F)
        {
            movement = glm::normalize(movement) * movement_step;
            camera.translate_local(movement.z, movement.x, movement.y);
        }

        pending_yaw_delta_   = 0.0F;
        pending_pitch_delta_ = 0.0F;
    }

    auto FlyCameraController::config() const noexcept -> const FlyCameraControllerConfig& { return config_; }
} // namespace RelayCore
