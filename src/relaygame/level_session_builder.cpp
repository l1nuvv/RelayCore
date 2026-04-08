#include "relaygame/level_session_builder.h"

#include "relaycore/core/paths.h"
#include "relaycore/world/blockout_loader.h"

#include <algorithm>
#include <filesystem>
#include <glm/trigonometric.hpp>

namespace RelayGame
{
    namespace
    {
        [[nodiscard]] auto resolve_level_path(const RelayCore::core::GameplayConfig& config) -> std::filesystem::path
        {
            const std::filesystem::path configured = config.level_path;
            if (configured.empty())
            {
                return RelayCore::core::asset_path("levels/test_room.toml");
            }
            if (configured.is_absolute())
            {
                return configured;
            }
            return RelayCore::core::asset_path(configured.generic_string());
        }

        [[nodiscard]] auto spawn_view_angles_from_entity(const RelayCore::EntitySpawnData& entity)
            -> std::optional<glm::vec2>
        {
            if (!entity.has_angles)
            {
                return std::nullopt;
            }

            // Level-authored spawn angles are [yaw, pitch, roll] in degrees with yaw=0 facing +Z.
            return glm::vec2{
                glm::radians(90.0F - entity.angles.x),
                glm::radians(entity.angles.y),
            };
        }
    } // namespace

    LevelSessionBuilder::LevelSessionBuilder(const RelayCore::core::GameplayConfig& config) noexcept : config_(config)
    {}

    auto LevelSessionBuilder::build() const -> LevelSession
    {
        LevelSession session{};
        session.level = RelayCore::load_blockout_level(resolve_level_path(config_));

        const auto spawn_it = std::find_if(
            session.level.entities.begin(),
            session.level.entities.end(),
            [this](const RelayCore::EntitySpawnData& entity) {
                return entity.classname == config_.spawn_classname;
            }
        );
        if (spawn_it != session.level.entities.end())
        {
            session.spawn_position = spawn_it->origin;
            session.spawn_view_angles = spawn_view_angles_from_entity(*spawn_it);
        }

        RelayCore::gameplay::build_runtime_interactions(
            session.level,
            session.doors,
            session.triggers,
            session.usables
        );
        RelayCore::gameplay::rebuild_trigger_spatial_index(session.triggers, session.trigger_spatial_index);
        RelayCore::gameplay::rebuild_usable_spatial_index(session.usables, session.usable_spatial_index);
        return session;
    }
} // namespace RelayGame
