#pragma once

#include "relaycore/world/blockout_level.h"

#include <span>

namespace RelayGame
{
    enum class EnvironmentKind {
        Fan,
        Steam,
        Sparks,
        Decal,
        Lamp
    };

    struct EnvironmentEffect {
        EnvironmentKind kind = EnvironmentKind::Fan;
        glm::vec3 origin{};
        glm::vec3 angles{};
        float scale = 1.0F;
        std::string required_target;
        std::string stop_target;
        std::string mark = "arrow";
        float delay = 0;
        float fixture_offset = 0.485F;
        bool damaged = false;
        int light_index = -1;
        float elapsed = 0;
        float age = 0;
        float strength = 0;
        float phase = 0;
        float audible_gain = 0;
        bool active = false;
    };

    [[nodiscard]] auto build_world_environment(const RelayCore::BlockoutLevel& level) -> std::vector<EnvironmentEffect>;
    void update_world_environment(
        std::span<EnvironmentEffect> effects,
        std::span<const std::string> activated_targets,
        float dt
    );
    [[nodiscard]] auto environment_sound(EnvironmentKind kind) -> const char*;
} // namespace RelayGame
