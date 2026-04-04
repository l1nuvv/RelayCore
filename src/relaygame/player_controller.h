#pragma once

#include "relaycore/app/frame_timer.h"
#include "relaycore/core/spatial_index_2d.h"
#include "relaycore/physics/collision.h"
#include "relaycore/physics/player_motion.h"

#include <SDL3/SDL.h>
#include <cstdint>
#include <glm/vec3.hpp>
#include <span>
#include <vector>

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
    struct KeyBinding {
        SDL_Scancode primary = SDL_SCANCODE_UNKNOWN;
        SDL_Scancode secondary = SDL_SCANCODE_UNKNOWN;
    };

    struct GroundedPlayerBindingsConfig {
        KeyBinding move_forward{SDL_SCANCODE_W, SDL_SCANCODE_UNKNOWN};
        KeyBinding move_backward{SDL_SCANCODE_S, SDL_SCANCODE_UNKNOWN};
        KeyBinding move_right{SDL_SCANCODE_D, SDL_SCANCODE_UNKNOWN};
        KeyBinding move_left{SDL_SCANCODE_A, SDL_SCANCODE_UNKNOWN};
        KeyBinding jump{SDL_SCANCODE_SPACE, SDL_SCANCODE_UNKNOWN};
        KeyBinding sprint{SDL_SCANCODE_LSHIFT, SDL_SCANCODE_RSHIFT};
        KeyBinding slow{SDL_SCANCODE_LALT, SDL_SCANCODE_RALT};
        KeyBinding noclip_down{SDL_SCANCODE_LCTRL, SDL_SCANCODE_RCTRL};
        KeyBinding use{SDL_SCANCODE_E, SDL_SCANCODE_UNKNOWN};
        KeyBinding toggle_capture{SDL_SCANCODE_F1, SDL_SCANCODE_UNKNOWN};
        KeyBinding toggle_vsync{SDL_SCANCODE_V, SDL_SCANCODE_UNKNOWN};
        KeyBinding toggle_noclip{SDL_SCANCODE_F3, SDL_SCANCODE_UNKNOWN};
        KeyBinding quit{SDL_SCANCODE_ESCAPE, SDL_SCANCODE_UNKNOWN};
    };

    enum class MovementMode : std::uint8_t {
        Grounded = 0,
        Air = 1,
        Noclip = 2,
    };

    struct GroundedPlayerControllerConfig {
        float move_speed = 4.5F;
        float sprint_multiplier = 1.0F;
        float ground_acceleration = 10.0F;
        float air_acceleration = 10.0F;
        float ground_friction = 4.0F;
        float stop_speed = 1.5F;
        float gravity = 24.0F;
        float jump_speed = 8.5F;
        float mouse_sensitivity_radians = 0.0022F;
        float collider_radius = 0.30F;
        float collider_height = 1.80F;
        float eye_height = 1.62F;
        float step_height = 0.35F;
        float ground_probe_distance = 0.08F;
        float max_ground_snap_distance = 0.10F;
        float collision_epsilon = 0.001F;
        bool capture_mouse_on_start = true;
        GroundedPlayerBindingsConfig bindings{};
    };

    class GroundedPlayerController {
    public:
        explicit GroundedPlayerController(
            const GroundedPlayerControllerConfig& config = GroundedPlayerControllerConfig{}
        );

        void set_config(const GroundedPlayerControllerConfig& config) noexcept;

        void initialize(const glm::vec3& spawn_position) noexcept;

        void set_static_collision_world(const RelayCore::BlockoutLevel& level);

        void enable_capture(RelayCore::Window& window) const;

        void on_event(const SDL_Event& event, RelayCore::Window& window, RelayCore::EngineServices& services);

        void flush_pending_look() noexcept;

        void update(
            const RelayCore::FrameInfo& frame,
            const RelayCore::BlockoutLevel& level,
            std::span<const RelayCore::physics::Aabb> dynamic_blockers = {}
        );

        void apply_camera(RelayCore::render::PerspectiveCamera& camera) const noexcept;

        [[nodiscard]] auto position() const noexcept -> const glm::vec3&;

        [[nodiscard]] auto velocity() const noexcept -> const glm::vec3&;

        [[nodiscard]] auto grounded() const noexcept -> bool;

        [[nodiscard]] auto movement_mode() const noexcept -> MovementMode;

        [[nodiscard]] auto eye_position_world() const noexcept -> glm::vec3;

        [[nodiscard]] auto view_forward() const noexcept -> glm::vec3;

        [[nodiscard]] auto player_bounds() const noexcept -> RelayCore::physics::Aabb;

        [[nodiscard]] auto consume_use_pressed() noexcept -> bool;

    private:
        static constexpr float kStaticCollisionCellSize = 4.0F;

        void toggle_noclip() noexcept;

        void update_grounded(
            const RelayCore::FrameInfo& frame,
            const RelayCore::BlockoutLevel& level,
            std::span<const RelayCore::physics::Aabb> dynamic_blockers
        );

        void update_noclip(const RelayCore::FrameInfo& frame);

        [[nodiscard]] auto poll_move_input() const -> RelayCore::physics::MoveInput;

        void refresh_collision_world(std::span<const RelayCore::physics::Aabb> dynamic_blockers);

        [[nodiscard]] auto build_motion_config() const -> RelayCore::physics::PlayerMotionConfig;

        [[nodiscard]] auto build_collision_config() const -> RelayCore::physics::CollisionConfig;

        GroundedPlayerControllerConfig config_{};
        glm::vec3 position_{0.0F, 0.0F, 0.0F};
        glm::vec3 velocity_{0.0F, 0.0F, 0.0F};
        RelayCore::physics::PlayerMotionState motion_state_{};
        float yaw_radians_ = -1.5707963268F;
        float pitch_radians_ = 0.0F;
        float pending_yaw_delta_ = 0.0F;
        float pending_pitch_delta_ = 0.0F;
        bool grounded_ = false;
        MovementMode movement_mode_ = MovementMode::Air;
        float noclip_speed_ = 8.0F;
        float noclip_fast_multiplier_ = 3.0F;
        float noclip_slow_multiplier_ = 0.35F;
        bool use_pressed_ = false;
        std::vector<RelayCore::physics::Aabb> static_collision_world_{};
        std::vector<RelayCore::physics::Aabb> collision_world_{};
        RelayCore::core::SpatialIndex2D<std::uint32_t> static_collision_index_{kStaticCollisionCellSize};
        std::vector<std::uint32_t> static_collision_candidates_{};
    };
} // namespace RelayGame
