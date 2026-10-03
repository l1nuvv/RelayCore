#include "relaygame/static_props.h"

#include "relaycore/core/paths.h"
#include "relaycore/world/surface_material_utils.h"

#include <algorithm>
#include <cmath>
#include <glm/ext/matrix_transform.hpp>
#include <limits>
#include <sstream>
#include <stdexcept>

namespace RelayGame
{
    auto build_static_props(const RelayCore::BlockoutLevel& level) -> std::vector<StaticProp>
    {
        std::vector<StaticProp> props;
        for (std::size_t i = 0; i < level.entities.size(); ++i)
        {
            const auto& entity = level.entities[i];
            if (entity.classname != "prop_static")
                continue;
            const auto label = (i < level.entity_sources.size() ? level.entity_sources[i].file_path : std::string{})
                               + ": prop_static entity " + std::to_string(i);
            const auto required = [&](const char* key) -> const std::string& {
                const auto found = entity.properties.find(key);
                if (found == entity.properties.end() || found->second.empty())
                    throw std::runtime_error(label + " requires " + key);
                return found->second;
            };
            const auto vector = [&](const char* key) {
                auto text = required(key);
                std::replace(text.begin(), text.end(), ',', ' ');
                std::istringstream input(text);
                glm::vec3 value;
                std::string extra;
                if (!(input >> value.x >> value.y >> value.z) || (input >> extra) || !std::isfinite(value.x)
                    || !std::isfinite(value.y) || !std::isfinite(value.z))
                    throw std::runtime_error(label + "." + key + " requires three finite numbers");
                return value;
            };
            StaticProp prop;
            prop.authored_entity_index = static_cast<int>(i);
            prop.model = RelayCore::core::asset_path(required("model"));
            if (prop.model.extension() == ".rcmodel")
            {
                if (entity.properties.contains("texture"))
                    throw std::runtime_error(label + ": .rcmodel owns its materials; remove texture");
            } else
            {
                prop.texture = RelayCore::core::asset_path(required("texture"));
                if (prop.model.extension() != ".obj" || prop.texture.extension() != ".bmp")
                    throw std::runtime_error(label + ": expected .rcmodel or triangulated .obj with .bmp texture");
            }
            const auto scale = entity.properties.contains("scale") ? vector("scale") : glm::vec3{1};
            if (glm::any(glm::lessThanEqual(scale, glm::vec3{0})))
                throw std::runtime_error(label + ": scale must be positive");
            prop.transform = glm::translate(glm::mat4{1}, entity.origin);
            prop.transform = glm::rotate(prop.transform, glm::radians(entity.angles.x), glm::vec3{0, 1, 0});
            prop.transform = glm::rotate(prop.transform, glm::radians(entity.angles.y), glm::vec3{1, 0, 0});
            prop.transform = glm::rotate(prop.transform, glm::radians(entity.angles.z), glm::vec3{0, 0, 1});
            prop.transform = glm::scale(prop.transform, scale);
            for (int column = 0; column < 4; ++column)
                for (int row = 0; row < 4; ++row)
                    if (!std::isfinite(prop.transform[column][row]))
                        throw std::runtime_error(label + ": transform exceeds finite float range");
            if (entity.properties.contains("collision_mins") || entity.properties.contains("collision_maxs")
                || entity.properties.contains("collision_material"))
            {
                const auto mins = vector("collision_mins"), maxs = vector("collision_maxs");
                if (glm::any(glm::greaterThanEqual(mins, maxs)))
                    throw std::runtime_error(label + ": collision_mins must be below collision_maxs");
                const auto material = RelayCore::try_parse_surface_material_name(required("collision_material"));
                if (!material)
                    throw std::runtime_error(label + ": unknown collision_material");
                RelayCore::BlockoutSolid collider;
                collider.mins = glm::vec3{std::numeric_limits<float>::max()};
                collider.maxs = -collider.mins;
                collider.material = *material;
                for (int corner = 0; corner < 8; ++corner)
                {
                    const auto point = glm::vec3(
                        prop.transform
                        * glm::
                            vec4{(corner & 1) ? maxs.x : mins.x, (corner & 2) ? maxs.y : mins.y, (corner & 4) ? maxs.z : mins.z, 1}
                    );
                    collider.mins = glm::min(collider.mins, point);
                    collider.maxs = glm::max(collider.maxs, point);
                }
                prop.collider = collider;
            }
            props.push_back(std::move(prop));
        }
        return props;
    }
} // namespace RelayGame
