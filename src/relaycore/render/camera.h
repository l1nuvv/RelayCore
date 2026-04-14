/**
 * @file src/relaycore/render/camera.h
 * @brief Perspective camera used by the renderer and fly-camera controller.
 */

#pragma once

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

namespace RelayCore::render
{
    /**
     * @brief Startup configuration for a perspective camera.
     */
    struct PerspectiveCameraConfig {
        glm::vec3 position{1.7F, 1.2F, 2.8F};       ///< Initial camera position.
        glm::vec3 target{0.0F, 0.0F, 0.0F};         ///< Initial look-at target.
        glm::vec3 world_up{0.0F, 1.0F, 0.0F};       ///< World-space up direction.
        float vertical_fov_radians = 1.0471975512F; ///< Vertical field of view in radians.
        float near_plane = 0.1F;                    ///< Near clipping plane distance.
        float far_plane = 100.0F;                   ///< Far clipping plane distance.
    };

    /**
     * @brief Lazily-updated perspective camera with FPS-style controls.
     */
    class PerspectiveCamera {
    public:
        /**
         * @brief Creates a camera from the supplied configuration.
         * @param config Camera startup parameters.
         */
        explicit PerspectiveCamera(const PerspectiveCameraConfig& config = PerspectiveCameraConfig{});

        /**
         * @brief Updates the viewport size used for the projection matrix.
         * @param width Viewport width in pixels.
         * @param height Viewport height in pixels.
         */
        void set_viewport(int width, int height) noexcept;

        /**
         * @brief Sets the camera position in world space.
         * @param position New camera position.
         */
        void set_position(const glm::vec3& position) noexcept;

        /**
         * @brief Reorients the camera so it looks at a target point.
         * @param target World-space target point.
         */
        void set_target(const glm::vec3& target) noexcept;

        /**
         * @brief Sets the camera orientation explicitly.
         * @param yaw_radians Yaw angle in radians.
         * @param pitch_radians Pitch angle in radians.
         */
        void set_orientation(float yaw_radians, float pitch_radians) noexcept;

        /**
         * @brief Applies projection-related settings without resetting the current transform.
         * @param config Updated camera configuration.
         */
        void apply_projection_config(const PerspectiveCameraConfig& config) noexcept;

        /**
         * @brief Applies incremental yaw and pitch deltas to the camera.
         * @param yaw_delta_radians Yaw delta in radians.
         * @param pitch_delta_radians Pitch delta in radians.
         */
        void add_yaw_pitch(float yaw_delta_radians, float pitch_delta_radians) noexcept;

        /**
         * @brief Moves the camera in its local basis vectors.
         * @param forward_delta Movement along the forward axis.
         * @param right_delta Movement along the right axis.
         * @param up_delta Movement along the world-up axis.
         */
        void translate_local(float forward_delta, float right_delta, float up_delta) noexcept;

        /**
         * @brief Returns the camera position.
         * @return Immutable camera position.
         */
        [[nodiscard]] auto position() const noexcept -> const glm::vec3&;

        /**
         * @brief Returns the normalized forward vector.
         * @return Immutable forward vector.
         */
        [[nodiscard]] auto forward() const noexcept -> const glm::vec3&;

        /**
         * @brief Returns the normalized right vector.
         * @return Immutable right vector.
         */
        [[nodiscard]] auto right() const noexcept -> const glm::vec3&;

        /**
         * @brief Returns the normalized up vector.
         * @return Immutable up vector.
         */
        [[nodiscard]] auto up() const noexcept -> const glm::vec3&;

        /**
         * @brief Returns the yaw angle.
         * @return Yaw in radians.
         */
        [[nodiscard]] auto yaw_radians() const noexcept -> float;

        /**
         * @brief Returns the pitch angle.
         * @return Pitch in radians.
         */
        [[nodiscard]] auto pitch_radians() const noexcept -> float;

        /**
         * @brief Returns the cached view matrix.
         * @return Immutable view matrix.
         */
        [[nodiscard]] auto view_matrix() const -> const glm::mat4&;

        /**
         * @brief Returns the cached projection matrix.
         * @return Immutable projection matrix.
         */
        [[nodiscard]] auto projection_matrix() const -> const glm::mat4&;

        /**
         * @brief Returns the cached combined view-projection matrix.
         * @return Immutable view-projection matrix.
         */
        [[nodiscard]] auto view_projection_matrix() const -> const glm::mat4&;

    private:
        /**
         * @brief Initializes yaw and pitch from a world-space target.
         * @param target World-space target point.
         */
        void initialize_orientation_from_target(const glm::vec3& target) noexcept;

        /**
         * @brief Recomputes the camera basis vectors when marked dirty.
         */
        void update_basis_if_needed() const noexcept;

        /**
         * @brief Recomputes view and projection matrices when needed.
         */
        void update_matrices_if_needed() const noexcept;

        /**
         * @brief Marks the transform-derived state as dirty.
         */
        void mark_transform_dirty() noexcept;

        /**
         * @brief Marks the projection-derived state as dirty.
         */
        void mark_projection_dirty() noexcept;

        glm::vec3 position_{};                           ///< Camera origin in world space.
        glm::vec3 world_up_{0.0F, 1.0F, 0.0F};           ///< Up vector used to derive the basis.
        float vertical_fov_radians_ = 1.0471975512F;     ///< Vertical field of view in radians.
        float near_plane_ = 0.1F;                        ///< Near clipping plane distance.
        float far_plane_ = 100.0F;                       ///< Far clipping plane distance.
        float aspect_ratio_ = 16.0F / 9.0F;              ///< Current viewport aspect ratio.
        float yaw_radians_ = -1.5707963268F;             ///< Camera yaw in radians.
        float pitch_radians_ = 0.0F;                     ///< Camera pitch in radians.

        mutable bool basis_dirty_ = true;                ///< True when the basis vectors must be recomputed.
        mutable bool view_dirty_ = true;                 ///< True when the view matrix must be recomputed.
        mutable bool projection_dirty_ = true;           ///< True when the projection matrix must be recomputed.
        mutable bool view_projection_dirty_ = true;      ///< True when the combined matrix must be recomputed.
        mutable glm::vec3 forward_{0.0F, 0.0F, -1.0F};   ///< Cached forward vector.
        mutable glm::vec3 right_{1.0F, 0.0F, 0.0F};      ///< Cached right vector.
        mutable glm::vec3 up_{0.0F, 1.0F, 0.0F};         ///< Cached up vector.
        mutable glm::mat4 view_matrix_{1.0F};            ///< Cached view matrix.
        mutable glm::mat4 projection_matrix_{1.0F};      ///< Cached projection matrix.
        mutable glm::mat4 view_projection_matrix_{1.0F}; ///< Cached combined matrix.
    };
} // namespace RelayCore::render
