#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "render/camera.hpp"

#include <glm/geometric.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

namespace
{
    constexpr float kEpsilon = 1.0e-4F;

    auto nearly_equal(const float lhs, const float rhs, const float epsilon = kEpsilon) -> bool
    {
        return doctest::Approx(lhs).epsilon(epsilon) == rhs;
    }

    auto nearly_equal_vec3(const glm::vec3& lhs, const glm::vec3& rhs, const float epsilon = kEpsilon) -> bool
    {
        return nearly_equal(lhs.x, rhs.x, epsilon) && nearly_equal(lhs.y, rhs.y, epsilon) && nearly_equal(
                   lhs.z,
                   rhs.z,
                   epsilon);
    }
} // namespace

TEST_CASE("camera basis vectors stay normalized")
{
    RelayCore::render::PerspectiveCamera camera;
    CHECK(nearly_equal(glm::length(camera.forward()), 1.0F));
    CHECK(nearly_equal(glm::length(camera.right()), 1.0F));
    CHECK(nearly_equal(glm::length(camera.up()), 1.0F));
}

TEST_CASE("camera local translation moves in camera space")
{
    RelayCore::render::PerspectiveCameraConfig config{};
    config.position = {0.0F, 0.0F, 0.0F};
    config.target   = {0.0F, 0.0F, -1.0F};

    RelayCore::render::PerspectiveCamera camera{config};
    camera.translate_local(2.0F, 0.0F, 0.0F);

    CHECK(nearly_equal_vec3(camera.position(), { 0.0F, 0.0F, -2.0F }));
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

TEST_CASE("projection matrix changes when viewport changes")
{
    RelayCore::render::PerspectiveCamera camera;
    camera.set_viewport(1280, 720);
    const glm::mat4 wide = camera.projection_matrix();

    camera.set_viewport(720, 1280);
    const glm::mat4 tall = camera.projection_matrix();

    CHECK_FALSE(nearly_equal(wide[0][0], tall[0][0]));
}
