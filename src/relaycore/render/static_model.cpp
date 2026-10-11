#include "relaycore/render/static_model.h"

#include <SDL3/SDL.h>
#include <array>
#include <charconv>
#include <cmath>
#include <cstring>
#include <fstream>
#include <glm/common.hpp>
#include <glm/geometric.hpp>
#include <limits>
#include <sstream>
#include <stdexcept>

namespace RelayCore::render
{
    auto load_obj_mesh(const std::filesystem::path& path) -> CpuColorMeshData
    {
        std::ifstream input(path);
        if (!input)
            throw std::runtime_error("Cannot open static model: " + path.string());
        std::vector<glm::vec3> positions, normals;
        std::vector<glm::vec2> uvs;
        CpuColorMeshData result;
        std::unordered_map<std::string, std::uint32_t> vertices;
        std::size_t line_number = 0;
        const auto fail = [&](const std::string& reason) {
            throw std::runtime_error(path.string() + ":" + std::to_string(line_number) + ": " + reason);
        };
        const auto index = [&](std::string_view value, std::size_t count) -> std::size_t {
            int parsed = 0;
            const auto [end, error] = std::from_chars(value.data(), value.data() + value.size(), parsed);
            const auto resolved =
                parsed > 0 ? static_cast<std::int64_t>(parsed) - 1 : static_cast<std::int64_t>(count) + parsed;
            if (error != std::errc{} || end != value.data() + value.size() || parsed == 0 || resolved < 0
                || resolved >= static_cast<std::int64_t>(count))
                fail("invalid OBJ index '" + std::string(value) + "'");
            return static_cast<std::size_t>(resolved);
        };
        std::string line;
        while (std::getline(input, line))
        {
            ++line_number;
            line.resize(line.find('#') == std::string::npos ? line.size() : line.find('#'));
            std::istringstream row(line);
            std::string kind;
            if (!(row >> kind))
                continue;
            if (kind == "v" || kind == "vn")
            {
                glm::vec3 value;
                if (!(row >> value.x >> value.y >> value.z) || !std::isfinite(value.x) || !std::isfinite(value.y)
                    || !std::isfinite(value.z))
                    fail("expected finite xyz");
                if (kind == "vn")
                {
                    if (glm::length(value) < 0.00001F)
                        fail("zero normal");
                    normals.push_back(glm::normalize(value));
                } else
                    positions.push_back(value);
                std::string extra;
                if (row >> extra)
                    fail("only xyz positions/normals are supported");
            } else if (kind == "vt")
            {
                glm::vec2 value;
                if (!(row >> value.x >> value.y) || !std::isfinite(value.x) || !std::isfinite(value.y))
                    fail("expected finite UV");
                uvs.push_back(value);
                std::string extra;
                if (row >> extra)
                    fail("only two-component UVs are supported");
            } else if (kind == "f")
            {
                std::array<std::uint32_t, 3> face{};
                for (auto& vertex_index : face)
                {
                    std::string token;
                    if (!(row >> token))
                        fail("faces must contain three v/vt/vn vertices; triangulate on export");
                    const auto first = token.find('/'), last = token.rfind('/');
                    if (first == std::string::npos || first == last)
                        fail("face requires UVs and normals (v/vt/vn)");
                    const auto p = index(std::string_view(token).substr(0, first), positions.size());
                    const auto t = index(std::string_view(token).substr(first + 1, last - first - 1), uvs.size());
                    const auto n = index(std::string_view(token).substr(last + 1), normals.size());
                    const std::string key = std::to_string(p) + '/' + std::to_string(t) + '/' + std::to_string(n);
                    const auto [found, inserted] =
                        vertices.try_emplace(key, static_cast<std::uint32_t>(result.vertices.size()));
                    if (inserted)
                        result.vertices.push_back({positions[p], glm::vec3{1}, normals[n], 0, uvs[t]});
                    vertex_index = found->second;
                }
                std::string extra;
                if (row >> extra)
                    fail("triangulate OBJ faces on export");
                const auto a = result.vertices[face[1]].position - result.vertices[face[0]].position;
                const auto b = result.vertices[face[2]].position - result.vertices[face[0]].position;
                if (glm::length(glm::cross(a, b)) < 0.0000001F)
                    fail("degenerate triangle");
                result.indices.insert(result.indices.end(), face.begin(), face.end());
            } else if (kind != "o" && kind != "g" && kind != "s" && kind != "mtllib" && kind != "usemtl")
                fail("unsupported OBJ statement: " + kind);
        }
        if (input.bad())
            fail("error reading model");
        if (result.indices.empty())
            fail("model contains no triangles");
        return result;
    }

    auto StaticModelCache::load(
        const std::filesystem::path& mesh,
        const std::filesystem::path& texture,
        const TextureEncoding encoding
    ) -> std::shared_ptr<const StaticModel>
    {
        const auto mesh_path = std::filesystem::canonical(mesh);
        const auto texture_path = std::filesystem::canonical(texture);
        const auto key = mesh_path.generic_string() + '\n' + texture_path.generic_string()
                         + (encoding == TextureEncoding::srgb ? "\nsrgb" : "\nlinear");
        Entry staged{
            std::filesystem::last_write_time(mesh_path),
            std::filesystem::last_write_time(texture_path),
            std::filesystem::file_size(mesh_path),
            std::filesystem::file_size(texture_path),
            {}
        };
        const auto found = entries_.find(key);
        if (found != entries_.end() && found->second.mesh_time == staged.mesh_time
            && found->second.texture_time == staged.texture_time && found->second.mesh_size == staged.mesh_size
            && found->second.texture_size == staged.texture_size)
            return found->second.resource;
        const auto data = load_obj_mesh(mesh_path);
        using Surface = std::unique_ptr<SDL_Surface, decltype(&SDL_DestroySurface)>;
        Surface source(SDL_LoadBMP(texture_path.string().c_str()), SDL_DestroySurface);
        if (!source)
            throw std::runtime_error("Cannot load model texture " + texture_path.string() + ": " + SDL_GetError());
        Surface rgba(SDL_ConvertSurface(source.get(), SDL_PIXELFORMAT_RGBA32), SDL_DestroySurface);
        if (!rgba)
            throw std::runtime_error("Cannot convert model texture " + texture_path.string() + ": " + SDL_GetError());
        const auto row_size = static_cast<std::size_t>(rgba->w) * 4;
        std::vector<std::uint8_t> pixels(row_size * rgba->h);
        // OBJ's UV origin is bottom-left; SDL image rows start at the top.
        for (int y = 0; y < rgba->h; ++y)
            std::memcpy(
                pixels.data() + y * row_size,
                static_cast<const std::uint8_t*>(rgba->pixels) + (rgba->h - 1 - y) * rgba->pitch,
                row_size
            );
        auto resource = std::make_shared<StaticModel>();
        resource->mins = resource->maxs = data.vertices.front().position;
        for (const auto& vertex : data.vertices)
        {
            resource->mins = glm::min(resource->mins, vertex.position);
            resource->maxs = glm::max(resource->maxs, vertex.position);
        }
        resource->mesh.initialize(data);
        resource->texture.initialize_rgba(rgba->w, rgba->h, pixels, encoding);
        staged.resource = resource;
        entries_.insert_or_assign(key, std::move(staged));
        return resource;
    }
    auto StaticModelCache::load(const std::filesystem::path& product, const TextureEncoding encoding)
        -> std::shared_ptr<const ModelAsset>
    {
        const auto path = std::filesystem::canonical(product);
        const auto key = path.generic_string() + (encoding == TextureEncoding::srgb ? "\nsrgb" : "\nlinear");
        ProductEntry staged{std::filesystem::last_write_time(path), std::filesystem::file_size(path), {}};
        const auto found = products_.find(key);
        if (found != products_.end() && found->second.time == staged.time && found->second.size == staged.size)
            return found->second.resource;
        auto data = read_model_asset(path);
        auto resource = std::make_shared<ModelAsset>();
        resource->mins = glm::vec3{std::numeric_limits<float>::max()};
        resource->maxs = -resource->mins;
        for (const auto& part : data.parts)
            for (const auto& vertex : data.meshes[part.mesh].geometry.vertices)
            {
                const auto position = glm::vec3{part.transform * glm::vec4{vertex.position, 1}};
                resource->mins = glm::min(resource->mins, position);
                resource->maxs = glm::max(resource->maxs, position);
            }
        for (const auto& material : data.materials)
        {
            const auto& image = material.base_color;
            resource->materials.emplace_back();
            auto& surface = resource->materials.back();
            surface.base_color.initialize_rgba(image.width, image.height, image.pixels, encoding);
            if (material.normal)
            {
                const auto& normal = *material.normal;
                surface.normal.initialize_rgba(normal.width, normal.height, normal.pixels, TextureEncoding::linear);
            }
            surface.normal_scale = material.normal_scale;
            surface.metallic_roughness = material.metallic_roughness;
            if (material.metallic_roughness_map)
            {
                const auto& map = *material.metallic_roughness_map;
                surface.metallic_roughness_map
                    .initialize_rgba(map.width, map.height, map.pixels, TextureEncoding::linear);
            }
            if (material.occlusion)
            {
                const auto& map = *material.occlusion;
                surface.occlusion.initialize_rgba(map.width, map.height, map.pixels, TextureEncoding::linear);
            }
            surface.occlusion_strength = material.occlusion_strength;
            if (material.emissive)
            {
                const auto& map = *material.emissive;
                surface.emissive.initialize_rgba(map.width, map.height, map.pixels, TextureEncoding::srgb);
            }
            surface.emissive_factor = material.emissive_factor;
        }
        for (auto& mesh : data.meshes)
        {
            // Product vertex colours are linear; legacy rendering works in visual sRGB.
            if (encoding == TextureEncoding::linear)
                for (auto& vertex : mesh.geometry.vertices)
                    for (int channel = 0; channel < 3; ++channel)
                    {
                        const auto value = vertex.color[channel];
                        vertex.color[channel] =
                            value <= .0031308F ? value * 12.92F : 1.055F * std::pow(value, 1.0F / 2.4F) - .055F;
                    }
            resource->meshes.emplace_back();
            resource->meshes.back().initialize(mesh.geometry);
        }
        resource->parts = std::move(data.parts);
        staged.resource = resource;
        products_.insert_or_assign(key, std::move(staged));
        return resource;
    }
} // namespace RelayCore::render
