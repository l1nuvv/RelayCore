/**
 * @file src/relaycore/render/camera.cpp
 * @brief PerspectiveCamera implementation.
 */

#include "relaycore/render/camera.h"

#include <algorithm>
#include <cmath>
#include <glm/common.hpp>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/geometric.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/trigonometric.hpp>
#include <stdexcept>
#include <string>
#include <utility>

namespace RelayCore::render
{
    namespace
    {
        /**
         * @brief Maximum allowed absolute pitch angle in radians.
         */
        constexpr float kPitchClamp = 1.55334306F; // ~89 degrees
        constexpr float kMinimumVectorLengthSquared = 1.0e-8F;

        struct CameraBasis {
            glm::vec3 forward{};
            glm::vec3 right{};
            glm::vec3 up{};
        };

        auto is_finite(const float value) noexcept -> bool
        {
            return std::isfinite(value);
        }

        auto is_finite(const glm::vec3& value) noexcept -> bool
        {
            return is_finite(value.x) && is_finite(value.y) && is_finite(value.z);
        }

        void require_finite(const float value, const char* const name)
        {
            if (!is_finite(value))
            {
                throw std::invalid_argument("PerspectiveCamera " + std::string(name) + " must be finite");
            }
        }

        void require_finite(const glm::vec3& value, const char* const name)
        {
            if (!is_finite(value))
            {
                throw std::invalid_argument("PerspectiveCamera " + std::string(name) + " must contain finite values");
            }
        }

        auto normalized_non_degenerate(const glm::vec3& value, const char* const name) -> glm::vec3
        {
            require_finite(value, name);
            const float length_squared = glm::dot(value, value);
            if (!is_finite(length_squared) || length_squared <= kMinimumVectorLengthSquared)
            {
                throw std::invalid_argument("PerspectiveCamera " + std::string(name) + " must be non-zero");
            }
            return glm::normalize(value);
        }

        void validate_projection(const float fov_radians, const float near_plane, const float far_plane)
        {
            if (!is_finite(fov_radians) || fov_radians <= 0.0F || fov_radians >= glm::pi<float>())
            {
                throw std::invalid_argument("PerspectiveCamera vertical_fov_radians must be finite and inside (0, pi)");
            }
            if (!is_finite(near_plane) || near_plane <= 0.0F)
            {
                throw std::invalid_argument("PerspectiveCamera near_plane must be finite and greater than zero");
            }
            if (!is_finite(far_plane) || far_plane <= near_plane)
            {
                throw std::invalid_argument("PerspectiveCamera far_plane must be finite and greater than near_plane");
            }
        }

        auto calculate_forward(const float yaw_radians, const float pitch_radians) -> glm::vec3
        {
            const float cos_pitch = std::cos(pitch_radians);
            return normalized_non_degenerate(
                glm::vec3{
                    std::cos(yaw_radians) * cos_pitch,
                    std::sin(pitch_radians),
                    std::sin(yaw_radians) * cos_pitch,
                },
                "forward"
            );
        }

        auto calculate_basis(const glm::vec3& forward, const glm::vec3& world_up) -> CameraBasis
        {
            const glm::vec3 right_cross = glm::cross(forward, world_up);
            const float cross_length_squared = glm::dot(right_cross, right_cross);
            if (!is_finite(cross_length_squared) || cross_length_squared <= kMinimumVectorLengthSquared)
            {
                throw std::invalid_argument("PerspectiveCamera forward and world_up must not be parallel");
            }

            const glm::vec3 right = glm::normalize(right_cross);
            const glm::vec3 up = normalized_non_degenerate(glm::cross(right, forward), "up");
            return {.forward = forward, .right = right, .up = up};
        }

        void validate_view_target(const glm::vec3& position, const glm::vec3& forward)
        {
            const glm::vec3 target = position + forward;
            require_finite(target, "view target");
            if (target == position)
            {
                throw std::invalid_argument("PerspectiveCamera position plus forward must differ from position");
            }
        }

        auto orientation_from_target(
            const glm::vec3& position,
            const glm::vec3& target,
            const glm::vec3& normalized_world_up
        ) -> std::pair<float, float>
        {
            require_finite(target, "target");
            const glm::vec3 direction = target - position;
            require_finite(direction, "target direction");
            const float direction_length_squared = glm::dot(direction, direction);
            if (!is_finite(direction_length_squared) || direction_length_squared <= kMinimumVectorLengthSquared)
            {
                throw std::invalid_argument("PerspectiveCamera target must differ from position");
            }
            const glm::vec3 normalized_direction = glm::normalize(direction);
            calculate_basis(normalized_direction, normalized_world_up);
            const float yaw = std::atan2(normalized_direction.z, normalized_direction.x);
            const float pitch = std::asin(normalized_direction.y);
            require_finite(yaw, "yaw_radians");
            require_finite(pitch, "pitch_radians");
            return {yaw, glm::clamp(pitch, -kPitchClamp, kPitchClamp)};
        }
    } // namespace

    PerspectiveCamera::PerspectiveCamera(const PerspectiveCameraConfig& config)
    {
        require_finite(config.position, "position");
        validate_projection(config.vertical_fov_radians, config.near_plane, config.far_plane);
        const glm::vec3 candidate_world_up = normalized_non_degenerate(config.world_up, "world_up");
        const auto [candidate_yaw, candidate_pitch] =
            orientation_from_target(config.position, config.target, candidate_world_up);
        const glm::vec3 candidate_forward = calculate_forward(candidate_yaw, candidate_pitch);
        calculate_basis(candidate_forward, candidate_world_up);
        validate_view_target(config.position, candidate_forward);

        position_ = config.position;
        world_up_ = candidate_world_up;
        vertical_fov_radians_ = config.vertical_fov_radians;
        near_plane_ = config.near_plane;
        far_plane_ = config.far_plane;
        yaw_radians_ = candidate_yaw;
        pitch_radians_ = candidate_pitch;
    }

    void PerspectiveCamera::set_viewport(const int width, const int height) noexcept
    {
        const int safe_width = std::max(width, 1);
        const int safe_height = std::max(height, 1);
        aspect_ratio_ = static_cast<float>(safe_width) / static_cast<float>(safe_height);
        mark_projection_dirty();
    }

    void PerspectiveCamera::set_position(const glm::vec3& position)
    {
        require_finite(position, "position");
        validate_view_target(position, calculate_forward(yaw_radians_, pitch_radians_));
        position_ = position;
        mark_transform_dirty();
    }

    void PerspectiveCamera::set_target(const glm::vec3& target)
    {
        const auto [candidate_yaw, candidate_pitch] = orientation_from_target(position_, target, world_up_);
        const glm::vec3 candidate_forward = calculate_forward(candidate_yaw, candidate_pitch);
        calculate_basis(candidate_forward, world_up_);
        validate_view_target(position_, candidate_forward);

        yaw_radians_ = candidate_yaw;
        pitch_radians_ = candidate_pitch;
        mark_transform_dirty();
    }

    void PerspectiveCamera::set_orientation(const float yaw_radians, const float pitch_radians)
    {
        require_finite(yaw_radians, "yaw_radians");
        require_finite(pitch_radians, "pitch_radians");
        const float candidate_pitch = glm::clamp(pitch_radians, -kPitchClamp, kPitchClamp);
        const glm::vec3 candidate_forward = calculate_forward(yaw_radians, candidate_pitch);
        calculate_basis(candidate_forward, world_up_);
        validate_view_target(position_, candidate_forward);

        yaw_radians_ = yaw_radians;
        pitch_radians_ = candidate_pitch;
        mark_transform_dirty();
    }

    void PerspectiveCamera::apply_projection_config(const PerspectiveCameraConfig& config)
    {
        validate_projection(config.vertical_fov_radians, config.near_plane, config.far_plane);
        const glm::vec3 candidate_world_up = normalized_non_degenerate(config.world_up, "world_up");
        const glm::vec3 candidate_forward = calculate_forward(yaw_radians_, pitch_radians_);
        calculate_basis(candidate_forward, candidate_world_up);
        validate_view_target(position_, candidate_forward);

        world_up_ = candidate_world_up;
        vertical_fov_radians_ = config.vertical_fov_radians;
        near_plane_ = config.near_plane;
        far_plane_ = config.far_plane;
        basis_dirty_ = true;
        mark_projection_dirty();
        view_dirty_ = true;
    }

    void PerspectiveCamera::add_yaw_pitch(const float yaw_delta_radians, const float pitch_delta_radians)
    {
        require_finite(yaw_delta_radians, "yaw_delta_radians");
        require_finite(pitch_delta_radians, "pitch_delta_radians");
        const float candidate_yaw = yaw_radians_ + yaw_delta_radians;
        const float candidate_pitch_unclamped = pitch_radians_ + pitch_delta_radians;
        require_finite(candidate_yaw, "yaw_radians");
        require_finite(candidate_pitch_unclamped, "pitch_radians");
        const float candidate_pitch = glm::clamp(candidate_pitch_unclamped, -kPitchClamp, kPitchClamp);
        const glm::vec3 candidate_forward = calculate_forward(candidate_yaw, candidate_pitch);
        calculate_basis(candidate_forward, world_up_);
        validate_view_target(position_, candidate_forward);

        yaw_radians_ = candidate_yaw;
        pitch_radians_ = candidate_pitch;
        mark_transform_dirty();
    }

    void PerspectiveCamera::translate_local(const float forward_delta, const float right_delta, const float up_delta)
    {
        require_finite(forward_delta, "forward_delta");
        require_finite(right_delta, "right_delta");
        require_finite(up_delta, "up_delta");
        const glm::vec3 candidate_forward = calculate_forward(yaw_radians_, pitch_radians_);
        const CameraBasis candidate_basis = calculate_basis(candidate_forward, world_up_);
        const glm::vec3 translation =
            candidate_basis.forward * forward_delta + candidate_basis.right * right_delta + world_up_ * up_delta;
        require_finite(translation, "translation");
        const glm::vec3 candidate_position = position_ + translation;
        require_finite(candidate_position, "position");
        validate_view_target(candidate_position, candidate_forward);

        position_ = candidate_position;
        mark_transform_dirty();
    }

    auto PerspectiveCamera::position() const noexcept -> const glm::vec3&
    {
        return position_;
    }

    auto PerspectiveCamera::forward() const noexcept -> const glm::vec3&
    {
        update_basis_if_needed();
        return forward_;
    }

    auto PerspectiveCamera::right() const noexcept -> const glm::vec3&
    {
        update_basis_if_needed();
        return right_;
    }

    auto PerspectiveCamera::up() const noexcept -> const glm::vec3&
    {
        update_basis_if_needed();
        return up_;
    }

    auto PerspectiveCamera::yaw_radians() const noexcept -> float
    {
        return yaw_radians_;
    }

    auto PerspectiveCamera::pitch_radians() const noexcept -> float
    {
        return pitch_radians_;
    }

    auto PerspectiveCamera::view_matrix() const -> const glm::mat4&
    {
        update_matrices_if_needed();
        return view_matrix_;
    }

    auto PerspectiveCamera::projection_matrix() const -> const glm::mat4&
    {
        update_matrices_if_needed();
        return projection_matrix_;
    }

    auto PerspectiveCamera::view_projection_matrix() const -> const glm::mat4&
    {
        update_matrices_if_needed();
        return view_projection_matrix_;
    }

    void PerspectiveCamera::update_basis_if_needed() const noexcept
    {
        if (!basis_dirty_)
        {
            return;
        }

        const CameraBasis basis = calculate_basis(calculate_forward(yaw_radians_, pitch_radians_), world_up_);
        forward_ = basis.forward;
        right_ = basis.right;
        up_ = basis.up;
        basis_dirty_ = false;
    }

    void PerspectiveCamera::update_matrices_if_needed() const noexcept
    {
        update_basis_if_needed();

        if (projection_dirty_)
        {
            projection_matrix_ = glm::perspective(vertical_fov_radians_, aspect_ratio_, near_plane_, far_plane_);
            projection_dirty_ = false;
            view_projection_dirty_ = true;
        }

        if (view_dirty_)
        {
            view_matrix_ = glm::lookAt(position_, position_ + forward_, up_);
            view_dirty_ = false;
            view_projection_dirty_ = true;
        }

        if (view_projection_dirty_)
        {
            view_projection_matrix_ = projection_matrix_ * view_matrix_;
            view_projection_dirty_ = false;
        }
    }

    void PerspectiveCamera::mark_transform_dirty() noexcept
    {
        basis_dirty_ = true;
        view_dirty_ = true;
        view_projection_dirty_ = true;
    }

    void PerspectiveCamera::mark_projection_dirty() noexcept
    {
        projection_dirty_ = true;
        view_projection_dirty_ = true;
    }
} // namespace RelayCore::render
