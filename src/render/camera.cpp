#include "render/camera.hpp"

#include <algorithm>
#include <cmath>
#include <glm/common.hpp>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/geometric.hpp>

namespace RelayCore::render
{
    namespace
    {
        constexpr float kPitchClamp = 1.55334306F; // ~89 degrees
    } // namespace

    PerspectiveCamera::PerspectiveCamera(const PerspectiveCameraConfig& config) :
        position_(config.position),
        world_up_(glm::normalize(config.world_up)),
        vertical_fov_radians_(config.vertical_fov_radians),
        near_plane_(config.near_plane),
        far_plane_(config.far_plane)
    {
        initialize_orientation_from_target(config.target);
    }

    void PerspectiveCamera::set_viewport(const int width, const int height) noexcept
    {
        const int safe_width  = std::max(width, 1);
        const int safe_height = std::max(height, 1);
        aspect_ratio_         = static_cast<float>(safe_width) / static_cast<float>(safe_height);
        mark_projection_dirty();
    }

    void PerspectiveCamera::set_position(const glm::vec3& position) noexcept
    {
        position_ = position;
        mark_transform_dirty();
    }

    void PerspectiveCamera::set_target(const glm::vec3& target) noexcept { initialize_orientation_from_target(target); }

    void PerspectiveCamera::set_orientation(const float yaw_radians, const float pitch_radians) noexcept
    {
        yaw_radians_   = yaw_radians;
        pitch_radians_ = glm::clamp(pitch_radians, -kPitchClamp, kPitchClamp);
        mark_transform_dirty();
    }

    void PerspectiveCamera::add_yaw_pitch(const float yaw_delta_radians, const float pitch_delta_radians) noexcept
    {
        yaw_radians_ += yaw_delta_radians;
        pitch_radians_ = glm::clamp(pitch_radians_ + pitch_delta_radians, -kPitchClamp, kPitchClamp);
        mark_transform_dirty();
    }

    void PerspectiveCamera::translate_local(
        const float forward_delta, const float right_delta, const float up_delta
    ) noexcept
    {
        update_basis_if_needed();
        position_ += forward_ * forward_delta;
        position_ += right_ * right_delta;
        position_ += world_up_ * up_delta;
        mark_transform_dirty();
    }

    auto PerspectiveCamera::position() const noexcept -> const glm::vec3& { return position_; }

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

    auto PerspectiveCamera::yaw_radians() const noexcept -> float { return yaw_radians_; }

    auto PerspectiveCamera::pitch_radians() const noexcept -> float { return pitch_radians_; }

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

    void PerspectiveCamera::initialize_orientation_from_target(const glm::vec3& target) noexcept
    {
        const glm::vec3 direction = glm::normalize(target - position_);
        const float     pitch     = std::asin(direction.y);
        const float     yaw       = std::atan2(direction.z, direction.x);
        set_orientation(yaw, pitch);
    }

    void PerspectiveCamera::update_basis_if_needed() const noexcept
    {
        if (!basis_dirty_) { return; }

        const float cos_pitch = std::cos(pitch_radians_);
        forward_              = glm::normalize(
            glm::vec3{
                std::cos(yaw_radians_) * cos_pitch,
                std::sin(pitch_radians_),
                std::sin(yaw_radians_) * cos_pitch,
            }
        );
        right_       = glm::normalize(glm::cross(forward_, world_up_));
        up_          = glm::normalize(glm::cross(right_, forward_));
        basis_dirty_ = false;
    }

    void PerspectiveCamera::update_matrices_if_needed() const noexcept
    {
        update_basis_if_needed();

        if (projection_dirty_)
        {
            projection_matrix_     = glm::perspective(vertical_fov_radians_, aspect_ratio_, near_plane_, far_plane_);
            projection_dirty_      = false;
            view_projection_dirty_ = true;
        }

        if (view_dirty_)
        {
            view_matrix_           = glm::lookAt(position_, position_ + forward_, up_);
            view_dirty_            = false;
            view_projection_dirty_ = true;
        }

        if (view_projection_dirty_)
        {
            view_projection_matrix_ = projection_matrix_ * view_matrix_;
            view_projection_dirty_  = false;
        }
    }

    void PerspectiveCamera::mark_transform_dirty() noexcept
    {
        basis_dirty_           = true;
        view_dirty_            = true;
        view_projection_dirty_ = true;
    }

    void PerspectiveCamera::mark_projection_dirty() noexcept
    {
        projection_dirty_      = true;
        view_projection_dirty_ = true;
    }
} // namespace RelayCore::render
