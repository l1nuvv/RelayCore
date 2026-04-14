#pragma once

#include "relaycore/world/blockout_level.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace RelayCore::detail
{
    struct SolidTemplateData {
        std::vector<BlockoutSolid> solids{};
        std::vector<AuthoredSourceLocation> solid_sources{};
    };

    struct SolidDefaults {
        std::optional<SurfaceMaterial> material{};
        std::optional<glm::vec3> tint{};
        std::optional<glm::vec3> size{};
        std::optional<float> brightness{};
    };

    struct ModuleTemplateData {
        struct Anchor {
            std::string name{};
            glm::vec3 origin{};
            glm::vec3 angles{};
        };

        std::vector<BlockoutSolid> solids{};
        std::vector<AuthoredSourceLocation> solid_sources{};
        std::vector<EntitySpawnData> entities{};
        std::vector<AuthoredSourceLocation> entity_sources{};
        std::vector<Anchor> anchors{};
    };

    struct AnchorPlacement {
        glm::vec3 origin{};
        glm::vec3 angles{};
    };

    enum class QuarterTurn : std::uint8_t {
        Deg0,
        Deg90,
        Deg180,
        Deg270,
    };
} // namespace RelayCore::detail
