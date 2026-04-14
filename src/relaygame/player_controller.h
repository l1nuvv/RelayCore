#pragma once

#include "relaycore/app/frame_timer.h"
#include "relaycore/core/spatial_index_2d.h"
#include "relaycore/physics/collision.h"
#include "relaycore/physics/player_motion.h"

#include <SDL3/SDL.h>
#include <cstdint>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <optional>
#include <span>
#include <string>
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
        std::uint8_t primary_attack_button = SDL_BUTTON_LEFT;
        std::string primary_attack_label{"Mouse1"};
        KeyBinding use{SDL_SCANCODE_E, SDL_SCANCODE_UNKNOWN};
        KeyBinding toggle_capture{SDL_SCANCODE_F1, SDL_SCANCODE_UNKNOWN};
        KeyBinding toggle_vsync{SDL_SCANCODE_V, SDL_SCANCODE_UNKNOWN};
        KeyBinding toggle_noclip{SDL_SCANCODE_F3, SDL_SCANCODE_UNKNOWN};
        KeyBinding toggle_inspector{SDL_SCANCODE_F4, SDL_SCANCODE_UNKNOWN};
        KeyBinding pin_inspector{SDL_SCANCODE_F5, SDL_SCANCODE_UNKNOWN};
        KeyBinding reload_level{SDL_SCANCODE_F7, SDL_SCANCODE_UNKNOWN};
        KeyBinding open_inspector_source{SDL_SCANCODE_F8, SDL_SCANCODE_UNKNOWN};
        KeyBinding save_runtime_snapshot{SDL_SCANCODE_F9, SDL_SCANCODE_UNKNOWN};
        KeyBinding restore_runtime_snapshot{SDL_SCANCODE_F10, SDL_SCANCODE_UNKNOWN};
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

    struct CollisionQueryDiagnostics {
        std::uint32_t static_world_solids = 0;
        std::uint32_t static_candidate_count = 0;
        std::uint32_t dynamic_blocker_count = 0;
        std::uint32_t collision_world_solids = 0;
        bool ground_probe_hit = false;
        bool ground_snap_applied = false;
        bool attempted_step_move = false;
        bool used_step_move = false;
        bool hit_x = false;
        bool hit_y = false;
        bool hit_z = false;
    };

    class GroundedPlayerController {
    public:
        explicit GroundedPlayerController(
            const GroundedPlayerControllerConfig& config = GroundedPlayerControllerConfig{}
        );

        void set_config(const GroundedPlayerControllerConfig& config) noexcept;

        void
        initialize(const glm::vec3& spawn_position, std::optional<glm::vec2> spawn_view_angles = std::nullopt) noexcept;

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

        [[nodiscard]] auto consume_fire_pressed() noexcept -> bool;

        [[nodiscard]] auto consume_use_pressed() noexcept -> bool;

        [[nodiscard]] auto consume_inspector_pin_pressed() noexcept -> bool;

        [[nodiscard]] auto consume_reload_level_pressed() noexcept -> bool;

        [[nodiscard]] auto consume_open_inspector_source_pressed() noexcept -> bool;

        [[nodiscard]] auto consume_save_runtime_snapshot_pressed() noexcept -> bool;

        [[nodiscard]] auto consume_restore_runtime_snapshot_pressed() noexcept -> bool;

        [[nodiscard]] auto collision_query_diagnostics() const noexcept -> const CollisionQueryDiagnostics&;

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
        bool fire_pressed_ = false;
        bool use_pressed_ = false;
        bool inspector_pin_pressed_ = false;
        bool reload_level_pressed_ = false;
        bool open_inspector_source_pressed_ = false;
        bool save_runtime_snapshot_pressed_ = false;
        bool restore_runtime_snapshot_pressed_ = false;
        std::vector<RelayCore::physics::Aabb> static_collision_world_{};
        std::vector<RelayCore::physics::Aabb> collision_world_{};
        RelayCore::core::SpatialIndex2D<std::uint32_t> static_collision_index_{kStaticCollisionCellSize};
        std::vector<std::uint32_t> static_collision_candidates_{};
        RelayCore::physics::GroundedMoveDiagnostics movement_diagnostics_{};
        CollisionQueryDiagnostics collision_query_diagnostics_{};
    };
} // namespace RelayGame
