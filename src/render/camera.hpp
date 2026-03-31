#pragma once

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

namespace RelayCore::render
{
struct PerspectiveCameraConfig
{
    glm::vec3 position { 1.7F, 1.2F, 2.8F };
    glm::vec3 target { 0.0F, 0.0F, 0.0F };
    glm::vec3 world_up { 0.0F, 1.0F, 0.0F };
    float vertical_fov_radians = 1.0471975512F; // 60 degrees
    float near_plane = 0.1F;
    float far_plane = 100.0F;
};

class PerspectiveCamera
{
public:
    explicit PerspectiveCamera(const PerspectiveCameraConfig& config = PerspectiveCameraConfig {});

    void set_viewport(int width, int height) noexcept;
    void set_position(const glm::vec3& position) noexcept;
    void set_target(const glm::vec3& target) noexcept;
    void set_orientation(float yaw_radians, float pitch_radians) noexcept;
    void add_yaw_pitch(float yaw_delta_radians, float pitch_delta_radians) noexcept;
    void translate_local(float forward_delta, float right_delta, float up_delta) noexcept;

    [[nodiscard]] auto position() const noexcept -> const glm::vec3&;
    [[nodiscard]] auto forward() const noexcept -> const glm::vec3&;
    [[nodiscard]] auto right() const noexcept -> const glm::vec3&;
    [[nodiscard]] auto up() const noexcept -> const glm::vec3&;
    [[nodiscard]] auto yaw_radians() const noexcept -> float;
    [[nodiscard]] auto pitch_radians() const noexcept -> float;

    [[nodiscard]] auto view_matrix() const -> const glm::mat4&;
    [[nodiscard]] auto projection_matrix() const -> const glm::mat4&;
    [[nodiscard]] auto view_projection_matrix() const -> const glm::mat4&;

private:
    void initialize_orientation_from_target(const glm::vec3& target) noexcept;
    void update_basis_if_needed() const noexcept;
    void update_matrices_if_needed() const noexcept;
    void mark_transform_dirty() noexcept;
    void mark_projection_dirty() noexcept;

    glm::vec3 position_ {};
    glm::vec3 world_up_ { 0.0F, 1.0F, 0.0F };
    float vertical_fov_radians_ = 1.0471975512F;
    float near_plane_ = 0.1F;
    float far_plane_ = 100.0F;
    float aspect_ratio_ = 16.0F / 9.0F;
    float yaw_radians_ = -1.5707963268F;
    float pitch_radians_ = 0.0F;

    mutable bool basis_dirty_ = true;
    mutable bool view_dirty_ = true;
    mutable bool projection_dirty_ = true;
    mutable bool view_projection_dirty_ = true;
    mutable glm::vec3 forward_ { 0.0F, 0.0F, -1.0F };
    mutable glm::vec3 right_ { 1.0F, 0.0F, 0.0F };
    mutable glm::vec3 up_ { 0.0F, 1.0F, 0.0F };
    mutable glm::mat4 view_matrix_ { 1.0F };
    mutable glm::mat4 projection_matrix_ { 1.0F };
    mutable glm::mat4 view_projection_matrix_ { 1.0F };
};
} // namespace RelayCore::render
