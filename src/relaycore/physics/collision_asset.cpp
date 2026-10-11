#include "relaycore/physics/collision_asset.h"

#include "relaycore/world/surface_material_utils.h"

#include <bit>
#include <cmath>
#include <fstream>
#include <glm/geometric.hpp>
#include <glm/matrix.hpp>
#include <stdexcept>

namespace RelayCore::physics
{
    namespace
    {
        constexpr std::uint32_t kMagic = 0x4c4f4352; // RCOL
        constexpr std::uint32_t kVersion = 1;
        constexpr std::size_t kMaxTriangles = 1000000;

        void require(bool condition, const char* message)
        {
            if (!condition)
                throw std::runtime_error(message);
        }

        auto finite(glm::vec3 value) -> bool
        {
            return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
        }

        void append(std::vector<std::uint8_t>& bytes, std::uint32_t value)
        {
            for (int shift = 0; shift < 32; shift += 8)
                bytes.push_back(static_cast<std::uint8_t>(value >> shift));
        }

        void append_vector(std::vector<std::uint8_t>& bytes, glm::vec3 value)
        {
            for (float component : {value.x, value.y, value.z})
                append(bytes, std::bit_cast<std::uint32_t>(component));
        }
    } // namespace

    auto collision_bounds(std::span<const PhysicsTriangle> triangles) -> Aabb
    {
        require(!triangles.empty(), "collision geometry is empty");
        Aabb bounds{triangles.front().a, triangles.front().a};
        for (const auto& triangle : triangles)
            for (auto point : {triangle.a, triangle.b, triangle.c})
            {
                require(finite(point), "collision vertex must be finite");
                bounds.mins = glm::min(bounds.mins, point);
                bounds.maxs = glm::max(bounds.maxs, point);
            }
        return bounds;
    }

    void validate_collision_asset(const CollisionAsset& asset)
    {
        const auto path = std::filesystem::path{asset.model};
        require(
            asset.model.size() <= 256 && asset.model.starts_with("models/") && path.extension() == ".rcmodel"
                && path.parent_path().generic_string() == "models"
                && path.stem().string().find_first_not_of(
                       "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-"
                   ) == std::string::npos
                && !path.stem().empty(),
            "collision model must reference models/<name>.rcmodel"
        );
        require(asset.triangles.size() <= kMaxTriangles, "too many collision triangles");
        const auto bounds = collision_bounds(asset.triangles);
        require(
            bounds.mins == asset.bounds.mins && bounds.maxs == asset.bounds.maxs,
            "collision bounds differ from geometry"
        );
        for (const auto& triangle : asset.triangles)
        {
            const auto normal = glm::cross(triangle.b - triangle.a, triangle.c - triangle.a);
            const auto length_squared = glm::dot(normal, normal);
            require(
                finite(normal) && std::isfinite(length_squared) && length_squared > 1e-12F,
                "degenerate collision triangle"
            );
            require(
                static_cast<std::size_t>(triangle.surface) < surface_material_count(),
                "invalid collision surface material"
            );
        }
    }

    auto transform_collision_triangles(std::span<const PhysicsTriangle> triangles, const glm::mat4& transform)
        -> std::vector<PhysicsTriangle>
    {
        for (int column = 0; column < 4; ++column)
            for (int row = 0; row < 4; ++row)
                require(std::isfinite(transform[column][row]), "collision transform must be finite");
        const auto determinant = glm::determinant(glm::mat3{transform});
        require(
            transform[0][3] == 0 && transform[1][3] == 0 && transform[2][3] == 0 && transform[3][3] == 1
                && std::isfinite(determinant) && determinant != 0,
            "collision transform must be invertible and affine"
        );
        std::vector<PhysicsTriangle> result;
        result.reserve(triangles.size());
        for (const auto& triangle : triangles)
        {
            PhysicsTriangle baked{
                glm::vec3{transform * glm::vec4{triangle.a, 1}},
                glm::vec3{transform * glm::vec4{triangle.b, 1}},
                glm::vec3{transform * glm::vec4{triangle.c, 1}},
                triangle.surface
            };
            if (determinant < 0)
                std::swap(baked.b, baked.c);
            result.push_back(baked);
        }
        return result;
    }

    void write_collision_asset(const std::filesystem::path& path, const CollisionAsset& asset)
    {
        validate_collision_asset(asset);
        std::vector<std::uint8_t> bytes;
        append(bytes, kMagic);
        append(bytes, kVersion);
        append(bytes, static_cast<std::uint32_t>(asset.model.size()));
        append(bytes, static_cast<std::uint32_t>(asset.triangles.size()));
        append_vector(bytes, asset.bounds.mins);
        append_vector(bytes, asset.bounds.maxs);
        bytes.insert(bytes.end(), asset.model.begin(), asset.model.end());
        for (const auto& triangle : asset.triangles)
        {
            append_vector(bytes, triangle.a);
            append_vector(bytes, triangle.b);
            append_vector(bytes, triangle.c);
            append(bytes, static_cast<std::uint32_t>(triangle.surface));
        }
        auto temporary = path;
        temporary += ".tmp";
        std::ofstream output{temporary, std::ios::binary | std::ios::trunc};
        output.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
        output.close();
        if (!output)
        {
            std::filesystem::remove(temporary);
            throw std::runtime_error("cannot write collision product: " + path.string());
        }
        std::filesystem::rename(temporary, path);
    }

    auto read_collision_asset(const std::filesystem::path& path, std::string_view expected_model) -> CollisionAsset
    {
        try
        {
            std::ifstream input{path, std::ios::binary | std::ios::ate};
            require(static_cast<bool>(input), "cannot open collision product; rebuild assets");
            const auto size = input.tellg();
            require(
                size >= 40 && size <= static_cast<std::streamoff>(40 + 256 + kMaxTriangles * 40),
                "invalid collision product size"
            );
            input.seekg(0);
            const auto integer = [&] {
                std::uint32_t value = 0;
                for (int shift = 0; shift < 32; shift += 8)
                {
                    const int byte = input.get();
                    require(byte != std::char_traits<char>::eof(), "truncated collision product");
                    value |= static_cast<std::uint32_t>(byte) << shift;
                }
                return value;
            };
            const auto vector = [&] {
                const auto x = std::bit_cast<float>(integer());
                const auto y = std::bit_cast<float>(integer());
                const auto z = std::bit_cast<float>(integer());
                return glm::vec3{x, y, z};
            };
            require(integer() == kMagic, "invalid collision product magic");
            require(integer() == kVersion, "unsupported collision version; rebuild assets");
            const auto model_size = integer(), count = integer();
            require(
                model_size > 0 && model_size <= 256 && count > 0 && count <= kMaxTriangles,
                "invalid collision counts"
            );
            require(
                size == static_cast<std::streamoff>(40 + model_size + static_cast<std::size_t>(count) * 40),
                "collision payload size mismatch"
            );
            CollisionAsset asset;
            asset.bounds.mins = vector();
            asset.bounds.maxs = vector();
            asset.model.resize(model_size);
            input.read(asset.model.data(), model_size);
            require(asset.model == expected_model, "collision/model pairing differs; rebuild assets or fix reference");
            asset.triangles.reserve(count);
            for (std::uint32_t i = 0; i < count; ++i)
            {
                PhysicsTriangle triangle;
                triangle.a = vector();
                triangle.b = vector();
                triangle.c = vector();
                const auto surface = integer();
                require(surface < surface_material_count(), "invalid collision surface material");
                triangle.surface = static_cast<SurfaceMaterial>(surface);
                asset.triangles.push_back(triangle);
            }
            validate_collision_asset(asset);
            return asset;
        } catch (const std::exception& error)
        {
            throw std::runtime_error(path.string() + ": " + error.what());
        }
    }
} // namespace RelayCore::physics
