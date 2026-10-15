#include "relaycore/render/model_asset.h"

#include "relaycore/core/profiling.h"

#include <bit>
#include <cmath>
#include <fstream>
#include <glm/geometric.hpp>
#include <glm/matrix.hpp>
#include <limits>
#include <stdexcept>

namespace RelayCore::render
{
    namespace
    {
        constexpr std::uint32_t kMagic = 0x4c444d52; // RMDL, little endian.
        constexpr std::uint32_t kVersion = 6;
        constexpr std::size_t kMaxBytes = 256 * 1024 * 1024;

        void require(const bool condition, const char* message)
        {
            if (!condition)
                throw std::runtime_error(message);
        }

        auto finite(const glm::vec3 value) -> bool
        {
            return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
        }

        void append(std::vector<std::uint8_t>& bytes, const std::uint32_t value)
        {
            for (int shift = 0; shift < 32; shift += 8)
                bytes.push_back(static_cast<std::uint8_t>(value >> shift));
        }

        void append_float(std::vector<std::uint8_t>& bytes, const float value)
        {
            append(bytes, std::bit_cast<std::uint32_t>(value));
        }

        class ProductReader {
        public:
            explicit ProductReader(const std::span<const std::uint8_t> bytes) : bytes_(bytes) {}

            auto take(const std::size_t size) -> std::span<const std::uint8_t>
            {
                require(size <= bytes_.size(), "truncated model product");
                const auto result = bytes_.first(size);
                bytes_ = bytes_.subspan(size);
                return result;
            }

            auto integer() -> std::uint32_t
            {
                const auto value = take(4);
                return value[0] | (std::uint32_t{value[1]} << 8) | (std::uint32_t{value[2]} << 16)
                       | (std::uint32_t{value[3]} << 24);
            }

            auto number() -> float
            {
                return std::bit_cast<float>(integer());
            }
            auto vector() -> glm::vec3
            {
                return {number(), number(), number()};
            }
            auto empty() const -> bool
            {
                return bytes_.empty();
            }

        private:
            std::span<const std::uint8_t> bytes_;
        };
    } // namespace

    void validate_model_asset(const ModelAssetData& model)
    {
        require(!model.meshes.empty() && model.meshes.size() <= 4096, "model requires 1..4096 meshes");
        require(!model.materials.empty() && model.materials.size() <= 4096, "model requires 1..4096 materials");
        require(!model.parts.empty() && model.parts.size() <= 65536, "model requires 1..65536 parts");
        std::size_t bytes = 20 + model.parts.size() * 72;
        for (const auto& mesh : model.meshes)
        {
            const auto& geometry = mesh.geometry;
            require(!geometry.vertices.empty() && geometry.vertices.size() <= 2000000, "invalid model vertex count");
            require(
                !geometry.indices.empty() && geometry.indices.size() <= 6000000 && geometry.indices.size() % 3 == 0,
                "model mesh requires triangle indices"
            );
            for (const auto& vertex : geometry.vertices)
            {
                require(
                    finite(vertex.position) && finite(vertex.color) && finite(vertex.normal)
                        && std::isfinite(glm::length(vertex.normal)) && glm::length(vertex.normal) > 0.00001F
                        && glm::all(glm::greaterThanEqual(vertex.color, glm::vec3{0}))
                        && glm::all(glm::lessThanEqual(vertex.color, glm::vec3{1})) && std::isfinite(vertex.uv.x)
                        && std::isfinite(vertex.uv.y) && vertex.material_kind == 0,
                    "model vertex requires finite position/color/UV, nonzero normal and material_kind 0"
                );
                require(
                    finite(glm::vec3{vertex.tangent}) && std::isfinite(vertex.tangent.w),
                    "nonfinite model tangent"
                );
                if (mesh.has_tangents)
                    require(
                        std::abs(glm::length(glm::vec3{vertex.tangent}) - 1) < .01F
                            && std::abs(glm::dot(glm::normalize(vertex.normal), glm::vec3{vertex.tangent})) < .01F
                            && (vertex.tangent.w == 1 || vertex.tangent.w == -1),
                        "model tangent must be unit, perpendicular to normal, with handedness +/-1"
                    );
            }
            for (std::size_t i = 0; i < geometry.indices.size(); i += 3)
            {
                const auto a = geometry.indices[i], b = geometry.indices[i + 1], c = geometry.indices[i + 2];
                require(
                    a < geometry.vertices.size() && b < geometry.vertices.size() && c < geometry.vertices.size(),
                    "model triangle index is outside its mesh"
                );
                if (mesh.has_tangents)
                    require(
                        geometry.vertices[a].tangent.w == geometry.vertices[b].tangent.w
                            && geometry.vertices[a].tangent.w == geometry.vertices[c].tangent.w,
                        "triangle tangent handedness must agree"
                    );
                require(
                    glm::length(glm::cross(
                        geometry.vertices[b].position - geometry.vertices[a].position,
                        geometry.vertices[c].position - geometry.vertices[a].position
                    )) > 0.0000001F,
                    "model contains a degenerate triangle"
                );
            }
            bytes += 12 + geometry.vertices.size() * 64 + geometry.indices.size() * 4;
        }
        for (const auto& material : model.materials)
        {
            const auto& image = material.base_color;
            require(
                image.width > 0 && image.width <= 4096 && image.height > 0 && image.height <= 4096,
                "model base-color dimensions must be 1..4096"
            );
            require(
                image.pixels.size() == static_cast<std::size_t>(image.width) * image.height * 4,
                "model base-color RGBA byte count differs from dimensions"
            );
            const auto* image_end = image.pixels.data() + image.pixels.size();
            for (const auto* pixel = image.pixels.data(); pixel < image_end; pixel += 4)
                if (pixel[3] != 255)
                    throw std::runtime_error("model base-color must be opaque");
            bytes += 72 + image.pixels.size();
            for (const auto factor :
                 {material.emissive_factor.x, material.emissive_factor.y, material.emissive_factor.z})
                require(
                    std::isfinite(factor) && factor >= 0 && factor <= 1,
                    "emissive factor must be finite and in [0, 1]"
                );
            require(
                std::isfinite(material.occlusion_strength) && material.occlusion_strength >= 0
                    && material.occlusion_strength <= 1,
                "occlusion strength must be finite and in [0, 1]"
            );
            require(
                !material.metallic_roughness_map || material.metallic_roughness.has_value(),
                "metallic/roughness map requires metallic/roughness factors"
            );
            for (const auto* map : {&material.metallic_roughness_map, &material.occlusion, &material.emissive})
                if (*map)
                {
                    require(
                        (*map)->width > 0 && (*map)->width <= 4096 && (*map)->height > 0 && (*map)->height <= 4096,
                        "material map dimensions must be 1..4096"
                    );
                    require(
                        (*map)->pixels.size() == static_cast<std::size_t>((*map)->width) * (*map)->height * 4,
                        "material map RGBA byte count differs from dimensions"
                    );
                    bytes += (*map)->pixels.size();
                }
            if (material.metallic_roughness)
            {
                const auto& factors = *material.metallic_roughness;
                require(
                    std::isfinite(factors.roughness) && factors.roughness >= 0 && factors.roughness <= 1
                        && std::isfinite(factors.metallic) && factors.metallic >= 0 && factors.metallic <= 1,
                    "metallic/roughness factors must be finite and in [0, 1]"
                );
            }
            require(std::isfinite(material.normal_scale), "normal scale must be finite");
            if (material.normal)
            {
                const auto& normal = *material.normal;
                require(
                    normal.width > 0 && normal.width <= 4096 && normal.height > 0 && normal.height <= 4096,
                    "model normal-map dimensions must be 1..4096"
                );
                require(
                    normal.pixels.size() == static_cast<std::size_t>(normal.width) * normal.height * 4,
                    "model normal-map RGBA byte count differs from dimensions"
                );
                const auto* normal_end = normal.pixels.data() + normal.pixels.size();
                for (const auto* pixel = normal.pixels.data(); pixel < normal_end; pixel += 4)
                {
                    const int x = 2 * pixel[0] - 255, y = 2 * pixel[1] - 255, z = 2 * pixel[2] - 255;
                    // Decoded n = (2*RGB - 255)/255: |n| > 1/2 iff 4*dot(numerator,numerator) > 255^2.
                    if (4 * (x * x + y * y + z * z) <= 255 * 255 || z <= 0)
                        throw std::runtime_error("normal-map texel must encode a nonzero +Z tangent-space normal");
                }
                bytes += normal.pixels.size();
            }
        }
        require(bytes <= kMaxBytes, "model product exceeds 256 MiB");
        for (const auto& part : model.parts)
        {
            require(
                part.mesh < model.meshes.size() && part.material < model.materials.size(),
                "model part references a missing mesh/material"
            );
            require(
                !model.materials[part.material].normal || model.meshes[part.mesh].has_tangents,
                "normal-mapped part requires tangents"
            );
            for (int column = 0; column < 4; ++column)
                for (int row = 0; row < 4; ++row)
                    require(std::isfinite(part.transform[column][row]), "model part transform must be finite");
            require(
                part.transform[0][3] == 0 && part.transform[1][3] == 0 && part.transform[2][3] == 0
                    && part.transform[3][3] == 1 && std::isfinite(glm::determinant(glm::mat3{part.transform}))
                    && glm::determinant(glm::mat3{part.transform}) > 0,
                "model part requires an invertible affine transform with positive determinant"
            );
            for (const auto& vertex : model.meshes[part.mesh].geometry.vertices)
                require(
                    finite(glm::vec3{part.transform * glm::vec4{vertex.position, 1}}),
                    "model transformed position exceeds finite float range"
                );
        }
    }

    void write_model_asset(const std::filesystem::path& path, const ModelAssetData& model)
    {
        validate_model_asset(model);
        std::vector<std::uint8_t> bytes;
        append(bytes, kMagic);
        append(bytes, kVersion);
        append(bytes, static_cast<std::uint32_t>(model.meshes.size()));
        append(bytes, static_cast<std::uint32_t>(model.materials.size()));
        append(bytes, static_cast<std::uint32_t>(model.parts.size()));
        for (const auto& mesh : model.meshes)
        {
            append(bytes, mesh.has_tangents ? 1u : 0u);
            append(bytes, static_cast<std::uint32_t>(mesh.geometry.vertices.size()));
            append(bytes, static_cast<std::uint32_t>(mesh.geometry.indices.size()));
            for (const auto& vertex : mesh.geometry.vertices)
            {
                for (const auto value :
                     {vertex.position.x,
                      vertex.position.y,
                      vertex.position.z,
                      vertex.color.x,
                      vertex.color.y,
                      vertex.color.z,
                      vertex.normal.x,
                      vertex.normal.y,
                      vertex.normal.z,
                      vertex.material_kind,
                      vertex.uv.x,
                      vertex.uv.y,
                      vertex.tangent.x,
                      vertex.tangent.y,
                      vertex.tangent.z,
                      vertex.tangent.w})
                    append_float(bytes, value);
            }
            for (const auto index : mesh.geometry.indices)
                append(bytes, index);
        }
        for (const auto& material : model.materials)
        {
            append(bytes, static_cast<std::uint32_t>(material.base_color.width));
            append(bytes, static_cast<std::uint32_t>(material.base_color.height));
            bytes.insert(bytes.end(), material.base_color.pixels.begin(), material.base_color.pixels.end());
            append(bytes, material.normal ? static_cast<std::uint32_t>(material.normal->width) : 0u);
            append(bytes, material.normal ? static_cast<std::uint32_t>(material.normal->height) : 0u);
            append_float(bytes, material.normal_scale);
            if (material.normal)
                bytes.insert(bytes.end(), material.normal->pixels.begin(), material.normal->pixels.end());
            append(bytes, material.metallic_roughness ? 1u : 0u);
            append_float(bytes, material.metallic_roughness ? material.metallic_roughness->roughness : 1);
            append_float(bytes, material.metallic_roughness ? material.metallic_roughness->metallic : 0);
            for (const auto* map : {&material.metallic_roughness_map, &material.occlusion})
            {
                append(bytes, *map ? static_cast<std::uint32_t>((*map)->width) : 0u);
                append(bytes, *map ? static_cast<std::uint32_t>((*map)->height) : 0u);
                if (*map)
                    bytes.insert(bytes.end(), (*map)->pixels.begin(), (*map)->pixels.end());
            }
            append_float(bytes, material.occlusion_strength);
            append(bytes, material.emissive ? static_cast<std::uint32_t>(material.emissive->width) : 0u);
            append(bytes, material.emissive ? static_cast<std::uint32_t>(material.emissive->height) : 0u);
            if (material.emissive)
                bytes.insert(bytes.end(), material.emissive->pixels.begin(), material.emissive->pixels.end());
            for (const auto factor :
                 {material.emissive_factor.x, material.emissive_factor.y, material.emissive_factor.z})
                append_float(bytes, factor);
        }
        for (const auto& part : model.parts)
        {
            append(bytes, part.mesh);
            append(bytes, part.material);
            for (int column = 0; column < 4; ++column)
                for (int row = 0; row < 4; ++row)
                    append_float(bytes, part.transform[column][row]);
        }
        for (const auto& mesh : model.meshes)
        {
            const auto data = build_meshlets(mesh.geometry);
            append(bytes, uint32_t(data.clusters.size()));
            append(bytes, uint32_t(data.vertices.size()));
            append(bytes, uint32_t(data.triangles.size()));
            for (const auto& cluster : data.clusters)
            {
                for (uint32_t i = 0; i < 4; ++i)
                    append(bytes, cluster.range[i]);
                for (uint32_t i = 0; i < 4; ++i)
                    append_float(bytes, cluster.mins[i]);
                for (uint32_t i = 0; i < 4; ++i)
                    append_float(bytes, cluster.maxs[i]);
            }
            for (auto index : data.vertices)
                append(bytes, index);
            for (auto triangle : data.triangles)
                append(bytes, triangle);
        }
        require(bytes.size() <= kMaxBytes, "model product exceeds 256 MiB including meshlets");
        auto temporary = path;
        temporary += ".tmp";
        std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
        output.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
        output.close();
        if (!output)
        {
            std::filesystem::remove(temporary);
            throw std::runtime_error("Cannot write model product: " + path.string());
        }
        std::filesystem::rename(temporary, path);
    }

    auto read_model_asset(const std::filesystem::path& path) -> ModelAssetData
    {
        try
        {
            std::vector<std::uint8_t> bytes;
            {
                RELAYCORE_PROFILE_ZONE_N("Native model file read");
                std::ifstream input(path, std::ios::binary | std::ios::ate);
                require(static_cast<bool>(input), "cannot open model product");
                const auto size = input.tellg();
                require(size >= 20 && size <= static_cast<std::streamoff>(kMaxBytes), "invalid model product size");
                bytes.resize(static_cast<std::size_t>(size));
                input.seekg(0);
                input.read(reinterpret_cast<char*>(bytes.data()), size);
                require(static_cast<bool>(input), "cannot read model product");
            }
            RELAYCORE_PROFILE_ZONE_N("Native model decode");
            ProductReader reader{bytes};
            require(reader.integer() == kMagic, "invalid model product magic");
            const auto version = reader.integer();
            require(version >= 1 && version <= kVersion, "unsupported model product version (supported: 1..6)");
            const auto meshes = reader.integer(), materials = reader.integer(), parts = reader.integer();
            require(
                meshes > 0 && meshes <= 4096 && materials > 0 && materials <= 4096 && parts > 0 && parts <= 65536,
                "invalid model product counts"
            );
            ModelAssetData result;
            for (std::uint32_t i = 0; i < meshes; ++i)
            {
                MeshAsset mesh;
                if (version >= 2)
                {
                    const auto flags = reader.integer();
                    require(flags <= 1, "unsupported mesh flags");
                    mesh.has_tangents = flags == 1;
                }
                const auto vertices = reader.integer(), indices = reader.integer();
                require(
                    vertices > 0 && vertices <= 2000000 && indices > 0 && indices <= 6000000 && indices % 3 == 0,
                    "invalid mesh counts"
                );
                // Prove the complete payload exists before allocating from authored counts.
                auto payload = reader.take(
                    static_cast<std::size_t>(vertices) * (version == 1 ? 48 : 64)
                    + static_cast<std::size_t>(indices) * 4
                );
                ProductReader geometry{payload};
                mesh.geometry.vertices.reserve(vertices);
                for (std::uint32_t j = 0; j < vertices; ++j)
                {
                    ColorVertex vertex;
                    vertex.position = geometry.vector();
                    vertex.color = geometry.vector();
                    vertex.normal = geometry.vector();
                    vertex.material_kind = geometry.number();
                    vertex.uv = {geometry.number(), geometry.number()};
                    if (version >= 2)
                        vertex.tangent = {geometry.number(), geometry.number(), geometry.number(), geometry.number()};
                    mesh.geometry.vertices.push_back(vertex);
                }
                mesh.geometry.indices.reserve(indices);
                for (std::uint32_t j = 0; j < indices; ++j)
                    mesh.geometry.indices.push_back(geometry.integer());
                result.meshes.push_back(std::move(mesh));
            }
            for (std::uint32_t i = 0; i < materials; ++i)
            {
                const auto width = reader.integer(), height = reader.integer();
                require(width > 0 && width <= 4096 && height > 0 && height <= 4096, "invalid base-color dimensions");
                const auto pixels = reader.take(static_cast<std::size_t>(width) * height * 4);
                ModelMaterial material{
                    {static_cast<int>(width), static_cast<int>(height), {pixels.begin(), pixels.end()}}
                };
                if (version >= 2)
                {
                    const auto normal_width = reader.integer(), normal_height = reader.integer();
                    material.normal_scale = reader.number();
                    require(
                        normal_width <= 4096 && normal_height <= 4096 && ((normal_width == 0) == (normal_height == 0)),
                        "invalid normal-map dimensions"
                    );
                    if (normal_width)
                    {
                        const auto normal_pixels =
                            reader.take(static_cast<std::size_t>(normal_width) * normal_height * 4);
                        material.normal = CpuRgbaTextureData{
                            static_cast<int>(normal_width),
                            static_cast<int>(normal_height),
                            {normal_pixels.begin(), normal_pixels.end()}
                        };
                    }
                }
                if (version >= 3)
                {
                    const auto flags = reader.integer();
                    require(flags <= 1, "unsupported material flags");
                    const MetallicRoughness factors{reader.number(), reader.number()};
                    if (flags == 1)
                        material.metallic_roughness = factors;
                    else
                        require(factors.roughness == 1 && factors.metallic == 0, "invalid legacy material factors");
                }
                if (version >= 4)
                {
                    for (auto* map : {&material.metallic_roughness_map, &material.occlusion})
                    {
                        const auto map_width = reader.integer(), map_height = reader.integer();
                        require(
                            map_width <= 4096 && map_height <= 4096 && ((map_width == 0) == (map_height == 0)),
                            "invalid material-map dimensions"
                        );
                        if (map_width)
                        {
                            const auto data = reader.take(static_cast<std::size_t>(map_width) * map_height * 4);
                            *map = CpuRgbaTextureData{
                                static_cast<int>(map_width),
                                static_cast<int>(map_height),
                                {data.begin(), data.end()}
                            };
                        }
                    }
                    material.occlusion_strength = reader.number();
                }
                if (version >= 5)
                {
                    const auto emissive_width = reader.integer(), emissive_height = reader.integer();
                    require(
                        emissive_width <= 4096 && emissive_height <= 4096
                            && ((emissive_width == 0) == (emissive_height == 0)),
                        "invalid emissive-map dimensions"
                    );
                    if (emissive_width)
                    {
                        const auto emissive_pixels =
                            reader.take(static_cast<std::size_t>(emissive_width) * emissive_height * 4);
                        material.emissive = CpuRgbaTextureData{
                            static_cast<int>(emissive_width),
                            static_cast<int>(emissive_height),
                            {emissive_pixels.begin(), emissive_pixels.end()}
                        };
                    }
                    material.emissive_factor = reader.vector();
                }
                result.materials.push_back(std::move(material));
            }
            for (std::uint32_t i = 0; i < parts; ++i)
            {
                ModelPart part;
                part.mesh = reader.integer();
                part.material = reader.integer();
                for (int column = 0; column < 4; ++column)
                    for (int row = 0; row < 4; ++row)
                        part.transform[column][row] = reader.number();
                result.parts.push_back(part);
            }
            if (version >= 6)
                for (auto& mesh : result.meshes)
                {
                    auto& data = mesh.geometry.meshlets;
                    const auto clusters = reader.integer(), vertices = reader.integer(), triangles = reader.integer();
                    require(
                        clusters > 0 && clusters <= mesh.geometry.indices.size() / 3 && vertices > 0
                            && vertices <= mesh.geometry.indices.size()
                            && triangles == mesh.geometry.indices.size() / 3,
                        "invalid meshlet product counts"
                    );
                    auto payload = reader.take(size_t(clusters) * 48 + size_t(vertices) * 4 + size_t(triangles) * 4);
                    ProductReader packed{payload};
                    data.clusters.resize(clusters);
                    for (auto& cluster : data.clusters)
                    {
                        for (uint32_t i = 0; i < 4; ++i)
                            cluster.range[i] = packed.integer();
                        for (uint32_t i = 0; i < 4; ++i)
                            cluster.mins[i] = packed.number();
                        for (uint32_t i = 0; i < 4; ++i)
                            cluster.maxs[i] = packed.number();
                    }
                    for (uint32_t i = 0; i < vertices; ++i)
                        data.vertices.push_back(packed.integer());
                    for (uint32_t i = 0; i < triangles; ++i)
                        data.triangles.push_back(packed.integer());
                    validate_meshlets(mesh.geometry, data);
                }
            require(reader.empty(), "unexpected bytes after model product");
            validate_model_asset(result);
            return result;
        } catch (const std::exception& error)
        {
            throw std::runtime_error(path.string() + ": " + error.what());
        }
    }
} // namespace RelayCore::render
