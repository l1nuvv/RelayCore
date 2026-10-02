#include "relaycore/render/irradiance_probes.h"

#include <algorithm>
#include <bit>
#include <cmath>
#include <fstream>
#include <glm/geometric.hpp>
#include <limits>
#include <numbers>
#include <stdexcept>

namespace RelayCore::render
{
    namespace
    {
        constexpr float pi = std::numbers::pi_v<float>;
        constexpr std::uint32_t magic = 0x42505249; // IRPB, little endian.
        constexpr std::uint32_t version = 1;

        void require(bool condition, const char* message)
        {
            if (!condition)
                throw std::runtime_error(message);
        }

        auto finite(glm::vec3 v) -> bool
        {
            return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
        }

        auto probe_count(const IrradianceProbeSettings& s) -> std::size_t
        {
            return std::size_t{s.dimensions.x} * s.dimensions.y * s.dimensions.z;
        }

        // Two-sided visibility; the facing side is resolved at a radiance hit.
        auto intersect(glm::vec3 origin, glm::vec3 direction, const IrradianceTriangle& t) -> float
        {
            const auto e1 = t.b - t.a, e2 = t.c - t.a;
            const auto p = glm::cross(direction, e2);
            const float determinant = glm::dot(e1, p);
            if (std::abs(determinant) < 1.e-8F)
                return std::numeric_limits<float>::infinity();
            const auto delta = origin - t.a;
            const float u = glm::dot(delta, p) / determinant;
            const auto q = glm::cross(delta, e1);
            const float v = glm::dot(direction, q) / determinant;
            const float distance = glm::dot(e2, q) / determinant;
            if (u < 0 || v < 0 || u + v > 1 || distance <= .001F)
                return std::numeric_limits<float>::infinity();
            return distance;
        }

        void append(std::vector<std::uint8_t>& bytes, std::uint32_t value)
        {
            for (int shift = 0; shift < 32; shift += 8)
                bytes.push_back(static_cast<std::uint8_t>(value >> shift));
        }

        void append_vector(std::vector<std::uint8_t>& bytes, glm::vec3 value)
        {
            for (int axis = 0; axis < 3; ++axis)
                append(bytes, std::bit_cast<std::uint32_t>(value[axis]));
        }
    } // namespace

    void validate_irradiance_settings(const IrradianceProbeSettings& s)
    {
        require(
            finite(s.mins) && finite(s.maxs) && finite(s.maxs - s.mins)
                && glm::all(glm::greaterThan(s.maxs - s.mins, glm::vec3{.3F})),
            "irradiance bounds must be finite, with maxs above mins by more than 0.3 metres"
        );
        require(
            glm::all(glm::greaterThanEqual(s.dimensions, glm::uvec3{2}))
                && glm::all(glm::lessThanEqual(s.dimensions, glm::uvec3{32})),
            "irradiance grid dimensions must be integers in [2,32]"
        );
        require(probe_count(s) <= max_irradiance_probes, "irradiance grid exceeds 64 probes");
        require(s.rays >= 64 && s.rays <= 16384, "irradiance rays must be in [64,16384]");
    }

    void validate_irradiance_volume(const IrradianceProbeVolume& volume)
    {
        validate_irradiance_settings(volume.settings);
        require(volume.probes.size() == probe_count(volume.settings), "irradiance probe count does not match grid");
        for (const auto& probe : volume.probes)
            for (const auto value : probe)
                require(finite(value), "irradiance SH coefficients must be finite");
    }

    static auto irradiance_basis(glm::vec3 n) -> std::array<float, 9>
    {
        return {
            .282094792F,
            .488602512F * n.y,
            .488602512F * n.z,
            .488602512F * n.x,
            1.092548431F * n.x * n.y,
            1.092548431F * n.y * n.z,
            .315391565F * (3 * n.z * n.z - 1),
            1.092548431F * n.x * n.z,
            .546274215F * (n.x * n.x - n.y * n.y)
        };
    }

    auto irradiance_source_signature(const IrradianceBakeScene& scene, const IrradianceProbeSettings& s)
        -> std::uint64_t
    {
        validate_irradiance_settings(s);
        // Content invalidation, not a security/integrity digest. Hash canonical little-endian fields.
        std::uint64_t hash = 14695981039346656037ULL;
        const auto integer = [&](std::uint32_t v) {
            for (int shift = 0; shift < 32; shift += 8)
                hash = (hash ^ static_cast<std::uint8_t>(v >> shift)) * 1099511628211ULL;
        };
        const auto vector = [&](glm::vec3 v) {
            require(finite(v), "irradiance bake input must be finite");
            for (int axis = 0; axis < 3; ++axis)
                integer(std::bit_cast<std::uint32_t>(v[axis]));
        };
        integer(version);
        vector(s.mins);
        vector(s.maxs);
        for (int axis = 0; axis < 3; ++axis)
            integer(s.dimensions[axis]);
        integer(s.rays);
        integer(static_cast<std::uint32_t>(scene.triangles.size()));
        for (const auto& t : scene.triangles)
        {
            vector(t.a);
            vector(t.b);
            vector(t.c);
            vector(t.diffuse);
            require(
                glm::all(glm::greaterThanEqual(t.diffuse, glm::vec3{0}))
                    && glm::all(glm::lessThanEqual(t.diffuse, glm::vec3{1})),
                "irradiance diffuse reflectance must be in [0,1]"
            );
        }
        integer(static_cast<std::uint32_t>(scene.lights.size()));
        for (const auto& light : scene.lights)
        {
            vector(light.position);
            vector(light.color);
            require(
                std::isfinite(light.radius) && light.radius > 0
                    && glm::all(glm::greaterThanEqual(light.color, glm::vec3{0})),
                "irradiance light requires positive finite radius and nonnegative radiance"
            );
            integer(std::bit_cast<std::uint32_t>(light.radius));
        }
        return hash;
    }

    auto bake_irradiance_probes(const IrradianceBakeScene& scene, const IrradianceProbeSettings& s)
        -> IrradianceProbeVolume
    {
        IrradianceProbeVolume volume{s, irradiance_source_signature(scene, s), {}};
        volume.probes.resize(probe_count(s));
        for (std::uint32_t z = 0; z < s.dimensions.z; ++z)
            for (std::uint32_t y = 0; y < s.dimensions.y; ++y)
                for (std::uint32_t x = 0; x < s.dimensions.x; ++x)
                {
                    // Grid centres stay 15 cm inside the authored support volume, away from room shells.
                    const auto position = s.mins + glm::vec3{.15F}
                                          + (s.maxs - s.mins - glm::vec3{.3F}) * glm::vec3{x, y, z}
                                                / glm::vec3{s.dimensions - glm::uvec3{1}};
                    auto& probe = volume.probes[x + s.dimensions.x * (y + s.dimensions.y * z)];
                    std::uint32_t backfaces = 0;
                    for (std::uint32_t ray = 0; ray < s.rays; ++ray)
                    {
                        const float height = 1 - 2 * (static_cast<float>(ray) + .5F) / static_cast<float>(s.rays);
                        const float radius = std::sqrt(std::max(0.F, 1 - height * height));
                        const float angle = static_cast<float>(ray) * 2.39996323F;
                        const glm::vec3 direction{radius * std::cos(angle), height, radius * std::sin(angle)};
                        float distance = std::numeric_limits<float>::infinity();
                        const IrradianceTriangle* hit = nullptr;
                        for (const auto& triangle : scene.triangles)
                        {
                            const float candidate = intersect(position, direction, triangle);
                            if (candidate < distance)
                            {
                                distance = candidate;
                                hit = &triangle;
                            }
                        }
                        if (!hit)
                            continue; // No environment or emission in this one-bounce slice.
                        const auto normal = glm::normalize(glm::cross(hit->b - hit->a, hit->c - hit->a));
                        if (glm::dot(normal, direction) > 0)
                        {
                            ++backfaces;
                            continue;
                        }
                        const auto point = position + direction * distance + normal * .002F;
                        glm::vec3 radiance{};
                        for (const auto& light : scene.lights)
                        {
                            const auto delta = light.position - point;
                            const float length = glm::length(delta);
                            if (length <= .001F || length >= light.radius)
                                continue;
                            const auto toward_light = delta / length;
                            const float cosine = std::max(0.F, glm::dot(normal, toward_light));
                            if (cosine == 0)
                                continue;
                            const bool blocked = std::ranges::any_of(scene.triangles, [&](const auto& t) {
                                return intersect(point, toward_light, t) < length - .002F;
                            });
                            if (!blocked)
                            {
                                const float a = 1 - length / light.radius;
                                radiance += hit->diffuse * light.color * (a * a * (3 - 2 * a) * cosine / pi);
                            }
                        }
                        const auto basis = irradiance_basis(direction);
                        for (std::size_t i = 0; i < probe.size(); ++i)
                        {
                            const float convolution = i == 0 ? 1.F : i < 4 ? 2.F / 3 : .25F;
                            probe[i] += radiance * (basis[i] * 4 * pi / static_cast<float>(s.rays) * convolution);
                        }
                    }
                    require(backfaces < s.rays / 2, "irradiance probe is inside geometry; move grid bounds/dimensions");
                }
        validate_irradiance_volume(volume);
        return volume;
    }

    auto sample_irradiance_probes(const IrradianceProbeVolume& v, glm::vec3 p, glm::vec3 n) -> glm::vec3
    {
        validate_irradiance_volume(v);
        require(
            finite(p) && finite(n) && glm::dot(n, n) > 1.e-8F,
            "irradiance sample requires finite position and normal"
        );
        const auto& s = v.settings;
        if (glm::any(glm::lessThan(p, s.mins)) || glm::any(glm::greaterThan(p, s.maxs)))
            return {};
        const auto grid =
            glm::clamp((p - s.mins - glm::vec3{.15F}) / (s.maxs - s.mins - glm::vec3{.3F}), glm::vec3{0}, glm::vec3{1})
            * glm::vec3{s.dimensions - glm::uvec3{1}};
        const auto cell = glm::min(glm::uvec3{grid}, s.dimensions - glm::uvec3{2});
        const auto fraction = grid - glm::vec3{cell};
        const auto basis = irradiance_basis(glm::normalize(n));
        glm::vec3 result{};
        for (std::uint32_t corner = 0; corner < 8; ++corner)
        {
            const glm::uvec3 offset{corner & 1, (corner >> 1) & 1, (corner >> 2) & 1};
            const auto q = cell + offset;
            const auto w = glm::mix(glm::vec3{1} - fraction, fraction, glm::bvec3{offset});
            const auto& sh = v.probes[q.x + s.dimensions.x * (q.y + s.dimensions.y * q.z)];
            for (std::size_t i = 0; i < sh.size(); ++i)
                result += sh[i] * (basis[i] * w.x * w.y * w.z);
        }
        // Truncated SH can ring below zero even when all incoming radiance is nonnegative.
        return glm::max(result, glm::vec3{0});
    }

    void write_irradiance_probes(const std::filesystem::path& path, const IrradianceProbeVolume& volume)
    {
        validate_irradiance_volume(volume);
        std::vector<std::uint8_t> bytes;
        append(bytes, magic);
        append(bytes, version);
        append(bytes, static_cast<std::uint32_t>(volume.source_signature));
        append(bytes, static_cast<std::uint32_t>(volume.source_signature >> 32));
        append_vector(bytes, volume.settings.mins);
        append_vector(bytes, volume.settings.maxs);
        for (int axis = 0; axis < 3; ++axis)
            append(bytes, volume.settings.dimensions[axis]);
        append(bytes, volume.settings.rays);
        for (const auto& sh : volume.probes)
            for (auto value : sh)
                append_vector(bytes, value);
        const auto temporary = path.string() + ".tmp";
        std::ofstream out{temporary, std::ios::binary | std::ios::trunc};
        out.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
        out.close();
        require(bool(out), "failed to write irradiance product");
        std::filesystem::rename(temporary, path);
    }

    auto read_irradiance_probes(const std::filesystem::path& path) -> IrradianceProbeVolume
    {
        std::ifstream input{path, std::ios::binary | std::ios::ate};
        require(bool(input), "cannot open irradiance product; build relaycore_irradiance_assets");
        const auto size = input.tellg();
        require(
            size >= 56 && size <= 56 + static_cast<std::streamoff>(max_irradiance_probes * 9 * 12),
            "invalid irradiance product size"
        );
        std::vector<std::uint8_t> bytes(static_cast<std::size_t>(size));
        input.seekg(0);
        input.read(reinterpret_cast<char*>(bytes.data()), size);
        require(bool(input), "failed to read irradiance product");
        std::size_t cursor = 0;
        const auto integer = [&]() {
            require(cursor + 4 <= bytes.size(), "truncated irradiance product");
            std::uint32_t value = 0;
            for (int shift = 0; shift < 32; shift += 8)
                value |= std::uint32_t{bytes[cursor++]} << shift;
            return value;
        };
        const auto vector = [&]() {
            const float x = std::bit_cast<float>(integer()), y = std::bit_cast<float>(integer()),
                        z = std::bit_cast<float>(integer());
            return glm::vec3{x, y, z};
        };
        require(integer() == magic, "invalid irradiance product magic");
        require(integer() == version, "unsupported irradiance product version");
        IrradianceProbeVolume volume;
        const auto low = integer(), high = integer();
        volume.source_signature = std::uint64_t{low} | (std::uint64_t{high} << 32);
        volume.settings.mins = vector();
        volume.settings.maxs = vector();
        for (int axis = 0; axis < 3; ++axis)
            volume.settings.dimensions[axis] = integer();
        volume.settings.rays = integer();
        validate_irradiance_settings(volume.settings);
        volume.probes.resize(probe_count(volume.settings));
        for (auto& sh : volume.probes)
            for (auto& value : sh)
                value = vector();
        require(cursor == bytes.size(), "trailing irradiance product data");
        validate_irradiance_volume(volume);
        return volume;
    }
} // namespace RelayCore::render
