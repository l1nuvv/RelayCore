#pragma once

#include "relaycore/core/engine_config.h"
#include "relaycore/gameplay/combat_runtime.h"
#include "relaycore/gameplay/interaction_runtime.h"
#include "relaycore/physics/physics_world.h"
#include "relaycore/render/render_diagnostics.h"
#include "relaycore/world/blockout_level.h"
#include "relaygame/containment_encounter.h"
#include "relaygame/enemy_runtime.h"
#include "relaygame/mission_runtime.h"
#include "relaygame/runtime_model.h"
#include "relaygame/world_audio_zones.h"
#include "relaygame/world_environment.h"

#include <filesystem>
#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <optional>
#include <string_view>
#include <vector>

namespace RelayGame
{
    [[nodiscard]] auto
    resolve_level_exit_destination(const std::filesystem::path& source_map, std::string_view reference)
        -> std::filesystem::path;
    [[nodiscard]] auto
    make_level_exit_reference(const std::filesystem::path& source_map, const std::filesystem::path& destination_map)
        -> std::string;

    struct LevelExit {
        RelayCore::physics::Aabb bounds;
        std::filesystem::path destination;
        std::string entry;
        bool requires_completion = true;
    };
    struct EpisodeEnding {
        glm::vec3 camera_position{};
        glm::vec2 view_angles{};
        std::size_t closing_door = 0;
    };

    struct MissionRetrySpawn {
        std::size_t completed_steps = 0;
        glm::vec3 position{};
        std::optional<glm::vec2> view_angles{};
    };

    struct LevelSession {
        std::optional<ContainmentEncounter> containment;
        std::vector<RuntimeEnemy> enemies;
        std::vector<LevelExit> exits;
        std::optional<EpisodeEnding> ending;
        std::filesystem::path level_path{};
        RelayCore::BlockoutLevel level{};
        std::vector<RelayCore::physics::WorldCollider> world_colliders;
        // Immutable query geometry can be shared by copies of a prepared fixture session.
        std::shared_ptr<const RelayCore::physics::PhysicsWorld> physics_fixture;
        glm::vec3 spawn_position{0.0F, 0.3F, 0.0F};
        std::optional<glm::vec2> spawn_view_angles{}; // x = yaw radians, y = pitch radians
        std::vector<RelayCore::gameplay::RuntimeDoor> doors{};
        std::vector<RelayCore::gameplay::RuntimeTrigger> triggers{};
        std::vector<RelayCore::gameplay::RuntimeUsable> usables{};
        RelayCore::gameplay::InteractionSpatialIndex trigger_spatial_index{};
        RelayCore::gameplay::InteractionSpatialIndex usable_spatial_index{};
        RelayGame::RuntimeObjectRegistry runtime_registry{};
        std::vector<RelayCore::render::AuthoringWarningDiagnostics> authoring_warnings{};
        RelayCore::gameplay::CombatBuildResult combat{};
        RelayGame::MissionDefinition mission_definition{};
        std::vector<MissionRetrySpawn> retry_spawns{};
        std::vector<RelayGame::RuntimeAudioZone> audio_zones{};
        std::vector<EnvironmentEffect> environment{};
    };

    class LevelSessionBuilder {
    public:
        explicit LevelSessionBuilder(const RelayCore::core::GameplayConfig& config) noexcept;

        [[nodiscard]] auto build() const -> LevelSession;
        [[nodiscard]] auto build(const std::filesystem::path& path) const -> LevelSession;
        [[nodiscard]] auto build_at_entry(const std::filesystem::path& path, const std::string& entry) const
            -> LevelSession;

        [[nodiscard]] auto build(RelayCore::BlockoutLevel level, std::filesystem::path path) const -> LevelSession;
        [[nodiscard]] auto build_physics_fixture(
            RelayCore::BlockoutLevel level,
            std::filesystem::path path,
            std::span<const RelayCore::physics::PhysicsMesh> meshes
        ) const -> LevelSession;
        [[nodiscard]] auto build_collision_fixture(
            RelayCore::BlockoutLevel level,
            std::filesystem::path path,
            const std::filesystem::path& collision_product,
            std::string_view model_reference,
            const glm::mat4& transform = glm::mat4{1}
        ) const -> LevelSession;

    private:
        const RelayCore::core::GameplayConfig& config_;
    };
} // namespace RelayGame
