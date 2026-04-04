#include "relaycore/core/profiling.h"
#include "relaycore/world/blockout_level.h"
#include "relaygame/player_controller.h"

#include <algorithm>

namespace RelayGame
{
    void GroundedPlayerController::set_static_collision_world(const RelayCore::BlockoutLevel& level)
    {
        static_collision_world_.clear();
        static_collision_world_.reserve(level.solids.size());

        for (const RelayCore::BlockoutSolid& solid : level.solids)
        {
            static_collision_world_.push_back({solid.mins, solid.maxs});
        }

        static_collision_index_.set_cell_size(kStaticCollisionCellSize);
        static_collision_index_.reset(static_collision_world_.size());

        for (std::size_t index = 0; index < static_collision_world_.size(); ++index)
        {
            static_collision_index_.insert(static_collision_world_[index], static_cast<std::uint32_t>(index));
        }

        collision_world_ = static_collision_world_;
    }

    void GroundedPlayerController::toggle_noclip() noexcept
    {
        if (movement_mode_ == MovementMode::Noclip)
        {
            movement_mode_ = MovementMode::Air;
            grounded_ = false;
        } else
        {
            movement_mode_ = MovementMode::Noclip;
            grounded_ = false;
        }

        velocity_ = glm::vec3{0.0F};
        motion_state_.velocity = glm::vec3{0.0F};
        motion_state_.grounded = false;
    }

    void GroundedPlayerController::update(
        const RelayCore::FrameInfo& frame,
        const RelayCore::BlockoutLevel& level,
        std::span<const RelayCore::physics::Aabb> dynamic_blockers
    )
    {
        RELAYCORE_PROFILE_ZONE_N("GroundedPlayerController::update");

        if (movement_mode_ == MovementMode::Noclip)
        {
            update_noclip(frame);
            return;
        }

        update_grounded(frame, level, dynamic_blockers);
    }

    void GroundedPlayerController::update_grounded(
        const RelayCore::FrameInfo& frame,
        const RelayCore::BlockoutLevel& level,
        std::span<const RelayCore::physics::Aabb> dynamic_blockers
    )
    {
        RELAYCORE_PROFILE_ZONE_N("physics::update_grounded");

        if (static_collision_world_.size() != level.solids.size())
        {
            set_static_collision_world(level);
        }

        refresh_collision_world(dynamic_blockers);
        const auto input = poll_move_input();
        const auto config = build_motion_config();

        motion_state_.position = position_;
        motion_state_.velocity = velocity_;
        motion_state_.grounded = grounded_;

        RelayCore::physics::update_grounded(
            motion_state_,
            input,
            yaw_radians_,
            static_cast<float>(frame.delta_seconds),
            RelayCore::physics::CollisionWorld{collision_world_},
            config
        );

        position_ = motion_state_.position;
        velocity_ = motion_state_.velocity;

        grounded_ = motion_state_.grounded;
        movement_mode_ = grounded_ ? MovementMode::Grounded : MovementMode::Air;
    }

    void GroundedPlayerController::update_noclip(const RelayCore::FrameInfo& frame)
    {
        const auto input = poll_move_input();
        const auto config = build_motion_config();

        motion_state_.position = position_;
        motion_state_.velocity = glm::vec3{0.0F};
        motion_state_.grounded = false;

        RelayCore::physics::update_noclip(
            motion_state_,
            input,
            yaw_radians_,
            pitch_radians_,
            static_cast<float>(frame.delta_seconds),
            config
        );

        position_ = motion_state_.position;
        velocity_ = glm::vec3{0.0F};
        grounded_ = false;
        movement_mode_ = MovementMode::Noclip;
    }

    void GroundedPlayerController::refresh_collision_world(std::span<const RelayCore::physics::Aabb> dynamic_blockers)
    {
        if (static_collision_index_.empty())
        {
            collision_world_.resize(static_collision_world_.size());
            std::copy(static_collision_world_.begin(), static_collision_world_.end(), collision_world_.begin());
            collision_world_.insert(collision_world_.end(), dynamic_blockers.begin(), dynamic_blockers.end());
            return;
        }

        const float query_padding = std::max(config_.move_speed * config_.sprint_multiplier, 2.0F)
                                    + config_.collider_radius + config_.step_height;
        const RelayCore::physics::Aabb query_bounds{
            .mins = {position_.x - query_padding, position_.y - config_.collider_height, position_.z - query_padding},
            .maxs = {position_.x + query_padding, position_.y + config_.collider_height, position_.z + query_padding},
        };
        static_collision_index_.query(query_bounds, static_collision_candidates_);

        collision_world_.clear();
        collision_world_.reserve(static_collision_world_.size() / 4 + dynamic_blockers.size() + 8);

        for (const std::uint32_t index : static_collision_candidates_)
        {
            if (index >= static_collision_world_.size())
            {
                continue;
            }
            collision_world_.push_back(static_collision_world_[index]);
        }

        collision_world_.insert(collision_world_.end(), dynamic_blockers.begin(), dynamic_blockers.end());
    }

    auto GroundedPlayerController::build_motion_config() const -> RelayCore::physics::PlayerMotionConfig
    {
        RelayCore::physics::PlayerMotionConfig motion_config{};
        motion_config.max_speed = config_.move_speed;
        motion_config.sprint_multiplier = config_.sprint_multiplier;
        motion_config.ground_accelerate = config_.ground_acceleration;
        motion_config.air_accelerate = config_.air_acceleration;
        motion_config.ground_friction = config_.ground_friction;
        motion_config.stop_speed = config_.stop_speed;
        motion_config.gravity = config_.gravity;
        motion_config.jump_speed = config_.jump_speed;
        motion_config.air_wishspeed_cap = config_.move_speed * 0.30F;
        motion_config.collision = build_collision_config();
        motion_config.noclip_speed = noclip_speed_;
        motion_config.noclip_fast_multiplier = noclip_fast_multiplier_;
        motion_config.noclip_slow_multiplier = noclip_slow_multiplier_;
        return motion_config;
    }

    auto GroundedPlayerController::build_collision_config() const -> RelayCore::physics::CollisionConfig
    {
        RelayCore::physics::CollisionConfig collision_config{};
        collision_config.radius = config_.collider_radius;
        collision_config.height = config_.collider_height;
        collision_config.epsilon = config_.collision_epsilon;
        collision_config.step_height = config_.step_height;
        collision_config.ground_probe_distance = config_.ground_probe_distance;
        collision_config.max_ground_snap_distance = config_.max_ground_snap_distance;
        return collision_config;
    }
} // namespace RelayGame
