#include "relaycore/physics/collision_asset.h"
#include "relaycore/world/blockout_loader_parse.h"
#include "relaycore/world/surface_material_utils.h"

#include <array>
#include <iostream>
#include <stdexcept>

namespace
{
    auto compile(const std::filesystem::path& source, std::string_view model) -> RelayCore::physics::CollisionAsset
    {
        using namespace RelayCore;
        using namespace RelayCore::physics;
        const auto document = toml::parse_file(source.string());
        if (document["version"].value<int>() != 1 || document["units"].value<std::string>() != "metres"
            || document["axes"].value<std::string>() != "right-handed-y-up"
            || document["winding"].value<std::string>() != "ccw")
            throw std::runtime_error("expected version=1, units='metres', axes='right-handed-y-up', winding='ccw'");
        if (document["model"].value<std::string>() != model)
            throw std::runtime_error("model reference differs from declared collision/model pairing");
        CollisionAsset asset{std::string{model}, {}, {}};
        const auto point = [](const toml::table& table, std::string_view key) {
            return detail::parse_vec3(detail::get_required_node(table, key, "collision"), key);
        };
        const auto surface = [](const toml::table& table) {
            const auto name = table["surface"].value<std::string>();
            const auto parsed = name ? try_parse_surface_material_name(*name) : std::nullopt;
            if (!parsed)
                throw std::runtime_error(
                    "missing or unknown collision surface material: " + name.value_or("<missing>")
                );
            return *parsed;
        };
        if (const auto* node = document.get("boxes"))
        {
            const auto* boxes = node->as_array();
            if (!boxes)
                throw std::runtime_error("boxes must be an array of tables");
            for (const auto& box : *boxes)
            {
                const auto* table = box.as_table();
                if (!table)
                    throw std::runtime_error("box must be a table");
                const auto mins = point(*table, "mins"), maxs = point(*table, "maxs");
                if (!(mins.x < maxs.x && mins.y < maxs.y && mins.z < maxs.z))
                    throw std::runtime_error("box mins must be strictly below maxs");
                const auto material = surface(*table);
                const std::array<glm::vec3, 8> vertices{
                    {{mins.x, mins.y, mins.z},
                     {maxs.x, mins.y, mins.z},
                     {maxs.x, maxs.y, mins.z},
                     {mins.x, maxs.y, mins.z},
                     {mins.x, mins.y, maxs.z},
                     {maxs.x, mins.y, maxs.z},
                     {maxs.x, maxs.y, maxs.z},
                     {mins.x, maxs.y, maxs.z}}
                };
                for (const auto face : std::array<std::array<int, 4>, 6>{
                         {{0, 3, 2, 1}, {4, 5, 6, 7}, {0, 4, 7, 3}, {1, 2, 6, 5}, {0, 1, 5, 4}, {3, 7, 6, 2}}
                     })
                {
                    asset.triangles.push_back({vertices[face[0]], vertices[face[1]], vertices[face[2]], material});
                    asset.triangles.push_back({vertices[face[0]], vertices[face[2]], vertices[face[3]], material});
                }
            }
        }
        if (const auto* node = document.get("triangles"))
        {
            const auto* triangles = node->as_array();
            if (!triangles)
                throw std::runtime_error("triangles must be an array of tables");
            for (const auto& triangle : *triangles)
            {
                const auto* table = triangle.as_table();
                if (!table)
                    throw std::runtime_error("triangle must be a table");
                asset.triangles.push_back({point(*table, "a"), point(*table, "b"), point(*table, "c"), surface(*table)}
                );
            }
        }
        glm::mat4 transform{1};
        if (const auto* node = document.get("transform"))
        {
            const auto* values = node->as_array();
            if (!values || values->size() != 16)
                throw std::runtime_error("transform must contain 16 column-major numbers");
            for (int column = 0; column < 4; ++column)
                for (int row = 0; row < 4; ++row)
                {
                    const auto value = (*values)[column * 4 + row].value<float>();
                    if (!value)
                        throw std::runtime_error("transform must contain numbers");
                    transform[column][row] = *value;
                }
        }
        asset.triangles = transform_collision_triangles(asset.triangles, transform);
        asset.bounds = collision_bounds(asset.triangles);
        validate_collision_asset(asset);
        return asset;
    }
} // namespace

int main(int argc, char** argv)
{
    if (argc != 4)
    {
        std::cerr << "Usage: relaycore_collision_compile source.toml output.rccollision models/name.rcmodel\n";
        return 2;
    }
    try
    {
        if (std::filesystem::path{argv[2]}.extension() != ".rccollision")
            throw std::runtime_error("output must use .rccollision extension");
        const auto asset = compile(argv[1], argv[3]);
        RelayCore::physics::write_collision_asset(argv[2], asset);
        std::cout << "Compiled " << asset.triangles.size() << " collision triangles for " << asset.model << '\n';
        return 0;
    } catch (const std::exception& error)
    {
        std::cerr << argv[1] << ": " << error.what() << '\n';
        return 1;
    }
}
