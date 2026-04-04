#include "relaygame/player_controller_config.h"

#include "relaycore/core/engine_config.h"
#include "relaycore/core/log.h"

#include <SDL3/SDL.h>
#include <string>
#include <string_view>

namespace RelayGame
{
    namespace
    {
        [[nodiscard]] auto
        resolve_scancode(std::string_view configured_name, std::string_view action_name, const SDL_Scancode fallback)
            -> SDL_Scancode
        {
            if (configured_name.empty())
            {
                return fallback;
            }

            const SDL_Scancode scancode = SDL_GetScancodeFromName(std::string{configured_name}.c_str());
            if (scancode != SDL_SCANCODE_UNKNOWN)
            {
                return scancode;
            }

            RelayCore::log::warn(
                "Unknown scancode '{}' for gameplay action '{}'. Using fallback binding.",
                std::string{configured_name},
                std::string{action_name}
            );
            return fallback;
        }
    } // namespace

    auto build_player_controller_config(const RelayCore::core::EngineConfig& config) -> GroundedPlayerControllerConfig
    {
        GroundedPlayerControllerConfig player_config{};
        player_config.move_speed = config.input.move_speed;
        player_config.sprint_multiplier = config.input.sprint_multiplier;
        player_config.mouse_sensitivity_radians = config.input.mouse_sensitivity_radians;
        player_config.capture_mouse_on_start = config.input.capture_mouse_on_start;

        const auto& controls = config.gameplay.controls;
        player_config.bindings.move_forward.primary =
            resolve_scancode(controls.move_forward, "move_forward", SDL_SCANCODE_W);
        player_config.bindings.move_backward.primary =
            resolve_scancode(controls.move_backward, "move_backward", SDL_SCANCODE_S);
        player_config.bindings.move_right.primary = resolve_scancode(controls.move_right, "move_right", SDL_SCANCODE_D);
        player_config.bindings.move_left.primary = resolve_scancode(controls.move_left, "move_left", SDL_SCANCODE_A);
        player_config.bindings.jump.primary = resolve_scancode(controls.jump, "jump", SDL_SCANCODE_SPACE);
        player_config.bindings.sprint.primary = resolve_scancode(controls.sprint, "sprint", SDL_SCANCODE_LSHIFT);
        player_config.bindings.sprint.secondary =
            resolve_scancode(controls.sprint_alt, "sprint_alt", SDL_SCANCODE_RSHIFT);
        player_config.bindings.slow.primary = resolve_scancode(controls.slow, "slow", SDL_SCANCODE_LALT);
        player_config.bindings.slow.secondary = resolve_scancode(controls.slow_alt, "slow_alt", SDL_SCANCODE_RALT);
        player_config.bindings.noclip_down.primary =
            resolve_scancode(controls.noclip_down, "noclip_down", SDL_SCANCODE_LCTRL);
        player_config.bindings.noclip_down.secondary =
            resolve_scancode(controls.noclip_down_alt, "noclip_down_alt", SDL_SCANCODE_RCTRL);
        player_config.bindings.use.primary = resolve_scancode(controls.use, "use", SDL_SCANCODE_E);
        player_config.bindings.toggle_capture.primary =
            resolve_scancode(controls.toggle_capture, "toggle_capture", SDL_SCANCODE_F1);
        player_config.bindings.toggle_vsync.primary =
            resolve_scancode(controls.toggle_vsync, "toggle_vsync", SDL_SCANCODE_V);
        player_config.bindings.toggle_noclip.primary =
            resolve_scancode(controls.toggle_noclip, "toggle_noclip", SDL_SCANCODE_F3);
        player_config.bindings.quit.primary = resolve_scancode(controls.quit, "quit", SDL_SCANCODE_ESCAPE);
        return player_config;
    }
} // namespace RelayGame
