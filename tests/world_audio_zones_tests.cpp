#include "relaycore/core/profiling.h"
#include "relaygame/world_audio_zones.h"

#include <doctest/doctest.h>

namespace
{
    [[nodiscard]] auto make_audio_zone_bounds(const glm::vec3& center, const glm::vec3& half_extents)
        -> RelayCore::physics::Aabb
    {
        return {center - half_extents, center + half_extents};
    }
} // namespace

TEST_CASE("build_audio_zones parses authored audio zone entities")
{
    RelayCore::BlockoutLevel level{};
    level.entities.push_back(
        RelayCore::EntitySpawnData{
            .classname = "audio_zone",
            .origin = {2.0F, 1.5F, -3.0F},
            .properties = {
                {"name", "airlock_hum"},
                {"size", "4 3 6"},
                {"loop", "audio/ambient_airlock_hum.wav"},
                {"volume", "0.65"},
                {"priority", "2"},
            },
        }
    );

    const std::filesystem::path asset_root = "assets";
    const auto zones = RelayGame::build_audio_zones(level, asset_root);

    REQUIRE(zones.size() == 1);
    CHECK(zones[0].name == "airlock_hum");
    CHECK(zones[0].bounds.mins.x == doctest::Approx(0.0F));
    CHECK(zones[0].bounds.maxs.y == doctest::Approx(3.0F));
    CHECK(zones[0].bounds.maxs.z == doctest::Approx(0.0F));
    CHECK(zones[0].gain == doctest::Approx(0.65F));
    CHECK(zones[0].priority == 2);
    CHECK(zones[0].loop_path == asset_root / "audio/ambient_airlock_hum.wav");
}

TEST_CASE("find_active_audio_zone prefers higher priority and keeps current zone on exact ties")
{
    const std::vector<RelayGame::RuntimeAudioZone> zones{
        RelayGame::RuntimeAudioZone{
            .name = "intake",
            .bounds = make_audio_zone_bounds({0.0F, 1.0F, 0.0F}, {2.0F, 1.0F, 2.0F}),
            .loop_path = "assets/audio/ambient_airlock_hum.wav",
            .gain = 0.35F,
            .priority = 0,
        },
        RelayGame::RuntimeAudioZone{
            .name = "airlock",
            .bounds = make_audio_zone_bounds({0.5F, 1.0F, 0.5F}, {2.0F, 1.0F, 2.0F}),
            .loop_path = "assets/audio/ambient_airlock_hum.wav",
            .gain = 0.70F,
            .priority = 2,
        },
        RelayGame::RuntimeAudioZone{
            .name = "corridor",
            .bounds = make_audio_zone_bounds({0.0F, 1.0F, 0.0F}, {2.0F, 1.0F, 2.0F}),
            .loop_path = "assets/audio/ambient_airlock_hum.wav",
            .gain = 0.40F,
            .priority = 2,
        },
    };

    const RelayCore::physics::Aabb listener = make_audio_zone_bounds({0.0F, 1.0F, 0.0F}, {0.3F, 0.9F, 0.3F});

    CHECK(RelayGame::find_active_audio_zone(zones, listener, -1) == 1);
    CHECK(RelayGame::find_active_audio_zone(zones, listener, 1) == 1);
    CHECK(RelayGame::find_active_audio_zone(zones, listener, 2) == 2);
}
