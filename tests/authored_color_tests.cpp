#include "relaycore/render/authored_color.h"

#include <cstddef>
#include <doctest/doctest.h>
#include <glm/common.hpp>
#include <limits>
#include <stdexcept>

namespace
{
    void check_vec3(const glm::vec3& actual, const glm::vec3& expected)
    {
        CHECK(actual.x == doctest::Approx(expected.x));
        CHECK(actual.y == doctest::Approx(expected.y));
        CHECK(actual.z == doctest::Approx(expected.z));
    }

    auto previous_legacy_expression(const glm::vec3& base, const glm::vec3& tint, const float brightness) -> glm::vec3
    {
        return glm::clamp(base * tint * brightness, glm::vec3{0.0F}, glm::vec3{1.0F});
    }
} // namespace

TEST_CASE("authored visual sRGB channels convert to linear references")
{
    CHECK(RelayCore::render::srgb_channel_to_linear(0.0F) == doctest::Approx(0.0F));
    CHECK(RelayCore::render::srgb_channel_to_linear(0.04045F) == doctest::Approx(0.0031308F));
    CHECK(RelayCore::render::srgb_channel_to_linear(127.0F / 255.0F) == doctest::Approx(0.2122F).epsilon(0.0001));
    CHECK(RelayCore::render::srgb_channel_to_linear(0.5F) == doctest::Approx(0.214041F).epsilon(0.0001));
    CHECK(RelayCore::render::srgb_channel_to_linear(1.0F) == doctest::Approx(1.0F));
}

TEST_CASE("authored visual sRGB rejects invalid base colours")
{
    const float infinity = std::numeric_limits<float>::infinity();
    const float nan = std::numeric_limits<float>::quiet_NaN();
    const auto convert_channel = [](const float value) {
        static_cast<void>(RelayCore::render::srgb_channel_to_linear(value));
    };
    const auto convert_rgb = [](const glm::vec3 value) {
        static_cast<void>(RelayCore::render::srgb_visual_rgb_to_linear(value));
    };
    CHECK_THROWS_AS(convert_channel(nan), std::invalid_argument);
    CHECK_THROWS_AS(convert_channel(infinity), std::invalid_argument);
    CHECK_THROWS_AS(convert_channel(-0.01F), std::invalid_argument);
    CHECK_THROWS_AS(convert_channel(1.01F), std::invalid_argument);
    CHECK_THROWS_AS(convert_rgb(glm::vec3{0.5F, infinity, 0.5F}), std::invalid_argument);
}

TEST_CASE("legacy surface composition preserves gamma-space multiplication")
{
    check_vec3(
        RelayCore::render::compose_legacy_surface_color(glm::vec3{0.5F}, glm::vec3{1.0F}, 1.0F),
        glm::vec3{0.5F}
    );
    check_vec3(
        RelayCore::render::compose_legacy_surface_color(glm::vec3{0.5F}, glm::vec3{0.5F}, 1.0F),
        glm::vec3{0.25F}
    );
}

TEST_CASE("surface composition preserves the legacy formula for authored combinations")
{
    const glm::vec3 bases[] = {
        {0.74F, 0.96F, 1.0F},
        {0.5F, 0.5F, 0.5F},
        {0.92F, 0.88F, 0.70F},
    };
    const glm::vec3 tints[] = {
        {1.16F, 0.86F, 0.62F},
        {0.5F, 0.5F, 0.5F},
        {1.04F, 0.92F, 0.82F},
    };
    const float brightnesses[] = {1.20F, 1.0F, 1.04F};

    for (std::size_t index = 0; index < 3; ++index)
    {
        check_vec3(
            RelayCore::render::compose_legacy_surface_color(bases[index], tints[index], brightnesses[index]),
            previous_legacy_expression(bases[index], tints[index], brightnesses[index])
        );
    }
}

TEST_CASE("linear surface composition converts only authored visual base colour")
{
    const glm::vec3 base{0.5F, 0.4F, 0.3F};
    const glm::vec3 tint{1.16F, 0.86F, 0.62F};
    constexpr float brightness = 1.20F;

    const glm::vec3 expected_linear = RelayCore::render::srgb_visual_rgb_to_linear(base) * tint * brightness;
    const glm::vec3 linear = RelayCore::render::compose_linear_surface_color(base, tint, brightness);
    check_vec3(linear, expected_linear);

    const glm::vec3 incorrect =
        RelayCore::render::srgb_visual_rgb_to_linear(previous_legacy_expression(base, tint, brightness));
    CHECK(linear.x != doctest::Approx(incorrect.x));
    CHECK(linear.y != doctest::Approx(incorrect.y));
    CHECK(linear.z != doctest::Approx(incorrect.z));
}

TEST_CASE("surface composition accepts tint above one as a linear multiplier")
{
    const glm::vec3 tint{1.16F, 1.0F, 0.84F};
    const glm::vec3 result = RelayCore::render::compose_linear_surface_color(glm::vec3{0.5F}, tint, 1.0F);
    check_vec3(result, RelayCore::render::srgb_visual_rgb_to_linear(glm::vec3{0.5F}) * tint);

    const glm::vec3 unclamped = RelayCore::render::compose_linear_surface_color(glm::vec3{1.0F}, tint, 1.20F);
    CHECK(unclamped.x == doctest::Approx(1.392F));
}
