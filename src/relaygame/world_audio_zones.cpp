#include "relaygame/world_audio_zones.h"

#include "relaycore/core/log.h"
#include "relaycore/core/profiling.h"

#include <algorithm>
#include <charconv>
#include <optional>
#include <sstream>
#include <string_view>

namespace RelayGame
{
    namespace
    {
        [[nodiscard]] auto parse_float_property(const RelayCore::EntitySpawnData& entity, const std::string_view key)
            -> std::optional<float>
        {
            RELAYCORE_PROFILE_FUNCTION();
            const auto found = entity.properties.find(std::string{key});
            if (found == entity.properties.end())
            {
                return std::nullopt;
            }

            const std::string& value = found->second;
            float result = 0.0F;
            const auto [ptr, ec] = std::from_chars(value.data(), value.data() + value.size(), result);
            if (ec != std::errc{} || ptr != value.data() + value.size())
            {
                return std::nullopt;
            }

            return result;
        }

        [[nodiscard]] auto parse_int_property(const RelayCore::EntitySpawnData& entity, const std::string_view key)
            -> std::optional<int>
        {
            RELAYCORE_PROFILE_FUNCTION();
            const auto found = entity.properties.find(std::string{key});
            if (found == entity.properties.end())
            {
                return std::nullopt;
            }

            const std::string& value = found->second;
            int result = 0;
            const auto [ptr, ec] = std::from_chars(value.data(), value.data() + value.size(), result);
            if (ec != std::errc{} || ptr != value.data() + value.size())
            {
                return std::nullopt;
            }

            return result;
        }

        [[nodiscard]] auto parse_vec3_property(const RelayCore::EntitySpawnData& entity, const std::string_view key)
            -> std::optional<glm::vec3>
        {
            RELAYCORE_PROFILE_FUNCTION();
            const auto found = entity.properties.find(std::string{key});
            if (found == entity.properties.end())
            {
                return std::nullopt;
            }

            std::istringstream stream(found->second);
            glm::vec3 value{};
            if (!(stream >> value.x >> value.y >> value.z))
            {
                return std::nullopt;
            }

            return value;
        }

        [[nodiscard]] auto parse_string_property(const RelayCore::EntitySpawnData& entity, const std::string_view key)
            -> std::string
        {
            RELAYCORE_PROFILE_FUNCTION();
            const auto found = entity.properties.find(std::string{key});
            return found == entity.properties.end() ? std::string{} : found->second;
        }

        [[nodiscard]] auto overlap_volume(const RelayCore::physics::Aabb& a, const RelayCore::physics::Aabb& b) -> float
        {
            RELAYCORE_PROFILE_FUNCTION();
            const glm::vec3 mins{
                std::max(a.mins.x, b.mins.x),
                std::max(a.mins.y, b.mins.y),
                std::max(a.mins.z, b.mins.z),
            };
            const glm::vec3 maxs{
                std::min(a.maxs.x, b.maxs.x),
                std::min(a.maxs.y, b.maxs.y),
                std::min(a.maxs.z, b.maxs.z),
            };
            const glm::vec3 size = maxs - mins;
            if (size.x <= 0.0F || size.y <= 0.0F || size.z <= 0.0F)
            {
                return 0.0F;
            }

            return size.x * size.y * size.z;
        }
    } // namespace

    auto build_audio_zones(const RelayCore::BlockoutLevel& level, const std::filesystem::path& asset_root)
        -> std::vector<RuntimeAudioZone>
    {
        RELAYCORE_PROFILE_FUNCTION();
        std::vector<RuntimeAudioZone> zones{};

        for (const RelayCore::EntitySpawnData& entity : level.entities)
        {
            if (entity.classname != "audio_zone")
            {
                continue;
            }

            const std::optional<glm::vec3> size = parse_vec3_property(entity, "size");
            if (!size.has_value() || size->x <= 0.0F || size->y <= 0.0F || size->z <= 0.0F)
            {
                RelayCore::log::warn(
                    "Skipping audio_zone at ({}, {}, {}): invalid or missing size",
                    entity.origin.x,
                    entity.origin.y,
                    entity.origin.z
                );
                continue;
            }

            const std::string loop = [&entity]() {
                const std::string primary = parse_string_property(entity, "loop");
                return primary.empty() ? parse_string_property(entity, "ambient_loop") : primary;
            }();
            if (loop.empty())
            {
                RelayCore::log::warn(
                    "Skipping audio_zone at ({}, {}, {}): missing loop property",
                    entity.origin.x,
                    entity.origin.y,
                    entity.origin.z
                );
                continue;
            }

            RuntimeAudioZone zone{};
            zone.name = [&entity, &zones]() {
                std::string authored_name = parse_string_property(entity, "name");
                if (authored_name.empty())
                {
                    authored_name = parse_string_property(entity, "targetname");
                }
                if (authored_name.empty())
                {
                    authored_name = "audio_zone_" + std::to_string(zones.size());
                }
                return authored_name;
            }();
            zone.bounds = {
                .mins = entity.origin - (*size * 0.5F),
                .maxs = entity.origin + (*size * 0.5F),
            };
            zone.loop_path = asset_root / loop;
            zone.gain = std::clamp(parse_float_property(entity, "volume").value_or(1.0F), 0.0F, 1.0F);
            zone.priority = parse_int_property(entity, "priority").value_or(0);
            zones.push_back(std::move(zone));
        }

        return zones;
    }

    auto find_active_audio_zone(
        const std::span<const RuntimeAudioZone> zones,
        const RelayCore::physics::Aabb& listener_bounds,
        const int current_zone_index
    ) -> int
    {
        RELAYCORE_PROFILE_FUNCTION();
        int best_index = -1;
        int best_priority = 0;
        float best_overlap = 0.0F;

        for (std::size_t index = 0; index < zones.size(); ++index)
        {
            const RuntimeAudioZone& zone = zones[index];
            if (!RelayCore::physics::overlaps(zone.bounds, listener_bounds, 0.001F))
            {
                continue;
            }

            const float zone_overlap = overlap_volume(zone.bounds, listener_bounds);
            const bool is_current_zone = static_cast<int>(index) == current_zone_index;
            const bool best_is_current_zone = best_index == current_zone_index;

            if (best_index < 0 || zone.priority > best_priority
                || (zone.priority == best_priority && is_current_zone && !best_is_current_zone)
                || (zone.priority == best_priority && is_current_zone == best_is_current_zone
                    && zone_overlap > best_overlap + 0.0001F))
            {
                best_index = static_cast<int>(index);
                best_priority = zone.priority;
                best_overlap = zone_overlap;
            }
        }

        return best_index;
    }
} // namespace RelayGame
