#pragma once

#include <SDL3/SDL.h>
#include <cstdint>
#include <glm/vec3.hpp>

#include "relaycore/app/frame_timer.h"

namespace RelayCore
{
    class EngineServices;
    class Window;
    struct BlockoutLevel;
} // namespace RelayCore

namespace RelayCore::render
{
    class PerspectiveCamera;
}

namespace RelayGame
{
    enum class MovementMode : std::uint8_t {
        Grounded,
        Noclip,
    };

    struct GroundedPlayerControllerConfig {
        float move_speed                = 4.5F;
        float sprint_multiplier         = 1.65F;
        float ground_acceleration       = 42.0F;
        float air_acceleration          = 12.0F;
        float ground_friction           = 10.0F;
        float gravity                   = 24.0F;
        float jump_speed                = 8.5F;
        float mouse_sensitivity_radians = 0.0022F;
        float collider_radius           = 0.30F;
        float collider_height           = 1.80F;
        float eye_height                = 1.62F;
        bool  capture_mouse_on_start    = true;
    };

    class GroundedPlayerController {
    public:
        explicit GroundedPlayerController(
            const GroundedPlayerControllerConfig& config = GroundedPlayerControllerConfig{}
        );

        void initialize(const glm::vec3& spawn_position) noexcept;

        void enable_capture(RelayCore::Window& window) const;

        void on_event(const SDL_Event& event, RelayCore::Window& window, RelayCore::EngineServices& services);

        void update(const RelayCore::FrameInfo& frame, const RelayCore::BlockoutLevel& level);

        void apply_camera(RelayCore::render::PerspectiveCamera& camera) const noexcept;

        [[nodiscard]] auto position() const noexcept -> const glm::vec3&;

        [[nodiscard]] auto velocity() const noexcept -> const glm::vec3&;

        [[nodiscard]] auto grounded() const noexcept -> bool;

        [[nodiscard]] auto movement_mode() const noexcept -> MovementMode;

    private:
        void toggle_noclip() noexcept;

        void update_grounded(const RelayCore::FrameInfo& frame, const RelayCore::BlockoutLevel& level);

        void update_noclip(const RelayCore::FrameInfo& frame);

        [[nodiscard]] auto eye_position() const noexcept -> glm::vec3;

        GroundedPlayerControllerConfig config_{};
        glm::vec3                      position_{0.0F, 0.0F, 0.0F};
        glm::vec3                      velocity_{0.0F, 0.0F, 0.0F};
        float                          yaw_radians_            = -1.5707963268F;
        float                          pitch_radians_          = 0.0F;
        float                          pending_yaw_delta_      = 0.0F;
        float                          pending_pitch_delta_    = 0.0F;
        bool                           grounded_               = false;
        MovementMode                   movement_mode_          = MovementMode::Grounded;
        float                          noclip_speed_           = 8.0F;
        float                          noclip_fast_multiplier_ = 3.0F;
        float                          noclip_slow_multiplier_ = 0.35F;
    };
} // namespace RelayGame
