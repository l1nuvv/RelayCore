#pragma once

#include "relaycore/world/blockout_level.h"

#include <filesystem>
#include <glm/mat4x4.hpp>
#include <optional>

namespace RelayGame
{
    struct StaticProp {
        std::filesystem::path model, texture, collision_product;
        glm::mat4 transform{1};
        std::optional<RelayCore::BlockoutSolid> collider;
        int authored_entity_index = -1;
        bool casts_shadow = true;
    };

    [[nodiscard]] auto build_static_props(const RelayCore::BlockoutLevel& level) -> std::vector<StaticProp>;
} // namespace RelayGame
