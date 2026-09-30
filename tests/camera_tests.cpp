#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "relaycore/core/profiling.h"
#include "relaycore/render/camera.h"

#include <cmath>
#include <doctest/doctest.h>
#include <glm/geometric.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <limits>
#include <stdexcept>

namespace
{
    constexpr float kEpsilon = 1.0e-4F;

    auto nearly_equal(const float lhs, const float rhs, const float epsilon = kEpsilon) -> bool
    {
        return doctest::Approx(lhs).epsilon(epsilon) == rhs;
    }

    auto nearly_equal_vec3(const glm::vec3& lhs, const glm::vec3& rhs, const float epsilon = kEpsilon) -> bool
    {
        return nearly_equal(lhs.x, rhs.x, epsilon) && nearly_equal(lhs.y, rhs.y, epsilon)
               && nearly_equal(lhs.z, rhs.z, epsilon);
    }

    auto nearly_equal_mat4(const glm::mat4& lhs, const glm::mat4& rhs) -> bool
    {
        for (int column = 0; column < 4; ++column)
        {
            for (int row = 0; row < 4; ++row)
            {
                if (!nearly_equal(lhs[column][row], rhs[column][row]))
                {
                    return false;
                }
            }
        }
        return true;
    }

    auto finite_vec3(const glm::vec3& value) -> bool
    {
        return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
    }

    auto finite_mat4(const glm::mat4& matrix) -> bool
    {
        for (int column = 0; column < 4; ++column)
        {
            for (int row = 0; row < 4; ++row)
            {
                if (!std::isfinite(matrix[column][row]))
                {
                    return false;
                }
            }
        }
        return true;
    }

    struct CameraState {
        glm::vec3 position{};
        glm::vec3 forward{};
        glm::vec3 right{};
        glm::vec3 up{};
        glm::mat4 view{1.0F};
        glm::mat4 projection{1.0F};
        glm::mat4 view_projection{1.0F};
    };

    auto snapshot(const RelayCore::render::PerspectiveCamera& camera) -> CameraState
    {
        return {
            .position = camera.position(),
            .forward = camera.forward(),
            .right = camera.right(),
            .up = camera.up(),
            .view = camera.view_matrix(),
            .projection = camera.projection_matrix(),
            .view_projection = camera.view_projection_matrix(),
        };
    }

    void check_unchanged_and_finite(const RelayCore::render::PerspectiveCamera& camera, const CameraState& before)
    {
        CHECK(nearly_equal_vec3(camera.position(), before.position));
        CHECK(nearly_equal_vec3(camera.forward(), before.forward));
        CHECK(nearly_equal_vec3(camera.right(), before.right));
        CHECK(nearly_equal_vec3(camera.up(), before.up));
        CHECK(nearly_equal_mat4(camera.view_matrix(), before.view));
        CHECK(nearly_equal_mat4(camera.projection_matrix(), before.projection));
        CHECK(nearly_equal_mat4(camera.view_projection_matrix(), before.view_projection));
        CHECK(finite_mat4(camera.view_matrix()));
        CHECK(finite_mat4(camera.projection_matrix()));
        CHECK(finite_mat4(camera.view_projection_matrix()));
    }

    auto z_up_camera() -> RelayCore::render::PerspectiveCamera
    {
        RelayCore::render::PerspectiveCameraConfig config{};
        config.position = {0.0F, 0.0F, 0.0F};
        config.target = {-1.0F, 0.0F, 0.0F};
        config.world_up = {0.0F, 0.0F, 1.0F};
        return RelayCore::render::PerspectiveCamera{config};
    }
} // namespace

TEST_CASE("camera valid state is finite")
{
    RelayCore::render::PerspectiveCamera camera;
    CHECK(finite_vec3(camera.forward()));
    CHECK(finite_vec3(camera.right()));
    CHECK(finite_vec3(camera.up()));
    CHECK(nearly_equal(glm::length(camera.forward()), 1.0F));
    CHECK(nearly_equal(glm::length(camera.right()), 1.0F));
    CHECK(nearly_equal(glm::length(camera.up()), 1.0F));
    CHECK(finite_mat4(camera.view_matrix()));
    CHECK(finite_mat4(camera.projection_matrix()));
    CHECK(finite_mat4(camera.view_projection_matrix()));
}

TEST_CASE("camera constructor rejects invalid inputs")
{
    using RelayCore::render::PerspectiveCamera;
    using RelayCore::render::PerspectiveCameraConfig;
    const float nan = std::numeric_limits<float>::quiet_NaN();
    const float infinity = std::numeric_limits<float>::infinity();

    SUBCASE("degenerate view inputs")
    {
        PerspectiveCameraConfig config{};
        config.target = config.position;
        CHECK_THROWS_AS(PerspectiveCamera{config}, std::invalid_argument);

        config = {};
        config.world_up = {};
        CHECK_THROWS_AS(PerspectiveCamera{config}, std::invalid_argument);

        config = {};
        config.position = {};
        config.target = {0.0F, 0.0F, -1.0F};
        config.world_up = {0.0F, 0.0F, -1.0F};
        CHECK_THROWS_AS(PerspectiveCamera{config}, std::invalid_argument);

        config.world_up = {0.0F, 0.0F, 1.0F};
        CHECK_THROWS_AS(PerspectiveCamera{config}, std::invalid_argument);

        config.world_up = {0.0F, 1.0e-5F, -1.0F};
        CHECK_THROWS_AS(PerspectiveCamera{config}, std::invalid_argument);

        config = {};
        config.position = {};
        config.target = {0.0F, 1.0F, 0.0F};
        config.world_up = {0.0F, 1.0F, 0.0F};
        CHECK_THROWS_AS(PerspectiveCamera{config}, std::invalid_argument);

        config.target = {0.0F, -1.0F, 0.0F};
        CHECK_THROWS_AS(PerspectiveCamera{config}, std::invalid_argument);

        config.target = {1.0e-5F, 1.0F, 0.0F};
        CHECK_THROWS_AS(PerspectiveCamera{config}, std::invalid_argument);
    }

    SUBCASE("non-finite and overflow view inputs")
    {
        PerspectiveCameraConfig config{};
        config.position.x = nan;
        CHECK_THROWS_AS(PerspectiveCamera{config}, std::invalid_argument);

        config = {};
        config.target.y = infinity;
        CHECK_THROWS_AS(PerspectiveCamera{config}, std::invalid_argument);

        config = {};
        const float maximum = std::numeric_limits<float>::max();
        config.position = {maximum, 0.0F, 0.0F};
        config.target = {-maximum, 0.0F, 0.0F};
        CHECK_THROWS_AS(PerspectiveCamera{config}, std::invalid_argument);
    }

    SUBCASE("projection inputs")
    {
        PerspectiveCameraConfig config{};
        config.vertical_fov_radians = 0.0F;
        CHECK_THROWS_AS(PerspectiveCamera{config}, std::invalid_argument);

        config.vertical_fov_radians = glm::pi<float>();
        CHECK_THROWS_AS(PerspectiveCamera{config}, std::invalid_argument);

        config.vertical_fov_radians = nan;
        CHECK_THROWS_AS(PerspectiveCamera{config}, std::invalid_argument);

        config.vertical_fov_radians = infinity;
        CHECK_THROWS_AS(PerspectiveCamera{config}, std::invalid_argument);

        config = {};
        config.near_plane = 0.0F;
        CHECK_THROWS_AS(PerspectiveCamera{config}, std::invalid_argument);

        config.near_plane = -0.1F;
        CHECK_THROWS_AS(PerspectiveCamera{config}, std::invalid_argument);

        config = {};
        config.far_plane = config.near_plane;
        CHECK_THROWS_AS(PerspectiveCamera{config}, std::invalid_argument);

        config.far_plane = 0.01F;
        CHECK_THROWS_AS(PerspectiveCamera{config}, std::invalid_argument);

        config = {};
        config.near_plane = nan;
        CHECK_THROWS_AS(PerspectiveCamera{config}, std::invalid_argument);

        config = {};
        config.far_plane = infinity;
        CHECK_THROWS_AS(PerspectiveCamera{config}, std::invalid_argument);
    }
}

TEST_CASE("camera setters reject invalid candidates without changing state")
{
    const float nan = std::numeric_limits<float>::quiet_NaN();
    const float infinity = std::numeric_limits<float>::infinity();

    SUBCASE("set_target")
    {
        RelayCore::render::PerspectiveCamera camera;
        const CameraState before = snapshot(camera);
        CHECK_THROWS_AS(camera.set_target(camera.position()), std::invalid_argument);
        check_unchanged_and_finite(camera, before);
        camera.set_target({0.0F, 0.0F, -1.0F});
    }

    SUBCASE("set_target rejects a parallel resulting forward")
    {
        auto camera = z_up_camera();
        const CameraState before = snapshot(camera);
        CHECK_THROWS_AS(camera.set_target({0.0F, 0.0F, 1.0F}), std::invalid_argument);
        check_unchanged_and_finite(camera, before);
        camera.set_target({0.0F, 1.0F, 0.0F});
    }

    SUBCASE("set_target rejects parallel and antiparallel raw Y-up directions")
    {
        RelayCore::render::PerspectiveCamera camera;
        const CameraState before = snapshot(camera);
        CHECK_THROWS_AS(camera.set_target(camera.position() + glm::vec3{0.0F, 1.0F, 0.0F}), std::invalid_argument);
        CHECK_THROWS_AS(camera.set_target(camera.position() + glm::vec3{0.0F, -1.0F, 0.0F}), std::invalid_argument);
        check_unchanged_and_finite(camera, before);
        camera.set_target({0.0F, 0.0F, -1.0F});
    }

    SUBCASE("set_orientation")
    {
        RelayCore::render::PerspectiveCamera camera;
        const CameraState before = snapshot(camera);
        CHECK_THROWS_AS(camera.set_orientation(nan, 0.0F), std::invalid_argument);
        CHECK_THROWS_AS(camera.set_orientation(0.0F, infinity), std::invalid_argument);
        check_unchanged_and_finite(camera, before);
        camera.set_orientation(0.0F, 0.0F);
    }

    SUBCASE("set_orientation rejects a parallel forward")
    {
        auto camera = z_up_camera();
        const CameraState before = snapshot(camera);
        CHECK_THROWS_AS(camera.set_orientation(glm::half_pi<float>(), 0.0F), std::invalid_argument);
        check_unchanged_and_finite(camera, before);
    }

    SUBCASE("add_yaw_pitch")
    {
        RelayCore::render::PerspectiveCamera camera;
        const CameraState before = snapshot(camera);
        CHECK_THROWS_AS(camera.add_yaw_pitch(nan, 0.0F), std::invalid_argument);
        CHECK_THROWS_AS(camera.add_yaw_pitch(0.0F, infinity), std::invalid_argument);
        check_unchanged_and_finite(camera, before);
        camera.add_yaw_pitch(0.1F, 0.0F);
    }

    SUBCASE("set_position")
    {
        RelayCore::render::PerspectiveCamera camera;
        const CameraState before = snapshot(camera);
        CHECK_THROWS_AS(camera.set_position({nan, 0.0F, 0.0F}), std::invalid_argument);
        check_unchanged_and_finite(camera, before);
        camera.set_position({0.0F, 0.0F, 0.0F});
    }

    SUBCASE("translate_local")
    {
        RelayCore::render::PerspectiveCamera camera;
        const CameraState before = snapshot(camera);
        CHECK_THROWS_AS(camera.translate_local(nan, 0.0F, 0.0F), std::invalid_argument);
        check_unchanged_and_finite(camera, before);
        camera.translate_local(1.0F, 0.0F, 0.0F);
    }

    SUBCASE("apply_projection_config")
    {
        RelayCore::render::PerspectiveCamera camera;
        const CameraState before = snapshot(camera);
        RelayCore::render::PerspectiveCameraConfig config{};
        config.far_plane = config.near_plane;
        CHECK_THROWS_AS(camera.apply_projection_config(config), std::invalid_argument);
        config = {};
        config.world_up = {};
        CHECK_THROWS_AS(camera.apply_projection_config(config), std::invalid_argument);
        check_unchanged_and_finite(camera, before);
        camera.apply_projection_config({});
    }

    SUBCASE("apply_projection_config rejects a parallel world up")
    {
        auto camera = z_up_camera();
        const CameraState before = snapshot(camera);
        RelayCore::render::PerspectiveCameraConfig config{};
        config.world_up = {-1.0F, 0.0F, 0.0F};
        CHECK_THROWS_AS(camera.apply_projection_config(config), std::invalid_argument);
        check_unchanged_and_finite(camera, before);
    }
}

TEST_CASE("camera local translation moves in camera space")
{
    RelayCore::render::PerspectiveCameraConfig config{};
    config.position = {0.0F, 0.0F, 0.0F};
    config.target = {0.0F, 0.0F, -1.0F};

    RelayCore::render::PerspectiveCamera camera{config};
    camera.translate_local(2.0F, 0.0F, 0.0F);

    CHECK(nearly_equal_vec3(camera.position(), {0.0F, 0.0F, -2.0F}));
}

TEST_CASE("camera pitch is clamped")
{
    RelayCore::render::PerspectiveCamera camera;
    camera.set_orientation(0.0F, 10.0F);

    constexpr float pitch_limit = 1.55334306F;
    CHECK(nearly_equal(camera.pitch_radians(), pitch_limit));

    camera.set_orientation(0.0F, -10.0F);
    CHECK(nearly_equal(camera.pitch_radians(), -pitch_limit));
}

TEST_CASE("projection matrix handles a zero viewport and changes with aspect ratio")
{
    RelayCore::render::PerspectiveCamera camera;
    camera.set_viewport(0, 0);
    CHECK(finite_mat4(camera.projection_matrix()));

    camera.set_viewport(1280, 720);
    const glm::mat4 wide = camera.projection_matrix();
    camera.set_viewport(720, 1280);
    const glm::mat4 tall = camera.projection_matrix();

    CHECK_FALSE(nearly_equal(wide[0][0], tall[0][0]));
}
