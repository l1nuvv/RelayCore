#include "relaycore/render/reflection_probe.h"

#include <algorithm>
#include <bit>
#include <cmath>
#include <fstream>
#include <glm/geometric.hpp>
#include <limits>
#include <numbers>
#include <numeric>
#include <stdexcept>

namespace RelayCore::render
{
    namespace
    {
        constexpr float pi = std::numbers::pi_v<float>;
        constexpr std::uint32_t magic = 0x46455252; // RREF.
        constexpr std::uint32_t version = 1;
        constexpr std::uint32_t bake_revision = 1;
        auto finite(glm::vec3 v) -> bool
        {
            return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
        }
        void require(bool condition, const char* message)
        {
            if (!condition)
                throw std::runtime_error(message);
        }
        auto intersect(glm::vec3 origin, glm::vec3 direction, const IrradianceTriangle& t) -> float
        {
            const auto e1 = t.b - t.a, e2 = t.c - t.a;
            const auto p = glm::cross(direction, e2);
            const float determinant = glm::dot(e1, p);
            if (std::abs(determinant) < 1.e-8F)
                return std::numeric_limits<float>::infinity();
            const auto delta = origin - t.a, q = glm::cross(delta, e1);
            const float u = glm::dot(delta, p) / determinant, v = glm::dot(direction, q) / determinant;
            const float distance = glm::dot(e2, q) / determinant;
            return u < 0 || v < 0 || u + v > 1 || distance <= .001F ? std::numeric_limits<float>::infinity() : distance;
        }
        // Offline capture casts many rays against the same static triangles.
        class BakeTriangles {
            struct Node {
                glm::vec3 mins{}, maxs{};
                std::size_t begin = 0, end = 0, left = 0, right = 0;
            };
            const std::vector<IrradianceTriangle>& triangles_;
            std::vector<std::size_t> indices_;
            std::vector<Node> nodes_;
            auto build(std::size_t begin, std::size_t end) -> std::size_t
            {
                const auto index = nodes_.size();
                Node node{
                    glm::vec3{std::numeric_limits<float>::max()},
                    glm::vec3{std::numeric_limits<float>::lowest()},
                    begin,
                    end
                };
                for (auto i = begin; i < end; ++i)
                {
                    const auto& t = triangles_[indices_[i]];
                    node.mins = glm::min(node.mins, glm::min(t.a, glm::min(t.b, t.c)));
                    node.maxs = glm::max(node.maxs, glm::max(t.a, glm::max(t.b, t.c)));
                }
                nodes_.push_back(node);
                if (end - begin > 8)
                {
                    const auto extent = node.maxs - node.mins;
                    const int axis =
                        extent.x > extent.y ? (extent.x > extent.z ? 0 : 2) : (extent.y > extent.z ? 1 : 2);
                    const auto middle = begin + (end - begin) / 2;
                    std::nth_element(
                        indices_.begin() + begin,
                        indices_.begin() + middle,
                        indices_.begin() + end,
                        [&](auto a, auto b) {
                            const auto& x = triangles_[a];
                            const auto& y = triangles_[b];
                            return x.a[axis] + x.b[axis] + x.c[axis] < y.a[axis] + y.b[axis] + y.c[axis];
                        }
                    );
                    nodes_[index].left = build(begin, middle);
                    nodes_[index].right = build(middle, end);
                }
                return index;
            }
            void
            hit(std::size_t index,
                glm::vec3 origin,
                glm::vec3 direction,
                float& distance,
                const IrradianceTriangle*& triangle) const
            {
                const auto& node = nodes_[index];
                float near = 0, far = distance;
                for (int axis = 0; axis < 3; ++axis)
                {
                    if (std::abs(direction[axis]) < 1.e-8F)
                    {
                        if (origin[axis] < node.mins[axis] || origin[axis] > node.maxs[axis])
                            return;
                    } else
                    {
                        const float a = (node.mins[axis] - origin[axis]) / direction[axis];
                        const float b = (node.maxs[axis] - origin[axis]) / direction[axis];
                        near = std::max(near, std::min(a, b));
                        far = std::min(far, std::max(a, b));
                        if (near > far)
                            return;
                    }
                }
                if (node.left)
                {
                    hit(node.left, origin, direction, distance, triangle);
                    hit(node.right, origin, direction, distance, triangle);
                } else
                    for (auto i = node.begin; i < node.end; ++i)
                    {
                        const auto& t = triangles_[indices_[i]];
                        const auto candidate = intersect(origin, direction, t);
                        if (candidate < distance)
                        {
                            distance = candidate;
                            triangle = &t;
                        }
                    }
            }

        public:
            explicit BakeTriangles(const std::vector<IrradianceTriangle>& triangles) :
                triangles_(triangles), indices_(triangles.size())
            {
                std::iota(indices_.begin(), indices_.end(), 0);
                if (!indices_.empty())
                    build(0, indices_.size());
            }
            auto hit(glm::vec3 origin, glm::vec3 direction, float& distance) const -> const IrradianceTriangle*
            {
                const IrradianceTriangle* triangle = nullptr;
                if (!nodes_.empty())
                    hit(0, origin, direction, distance, triangle);
                return triangle;
            }
        };

        auto face_uv(glm::vec3 d) -> std::pair<std::uint32_t, glm::vec2>
        {
            const auto a = glm::abs(d);
            std::uint32_t face;
            glm::vec2 uv;
            if (a.x >= a.y && a.x >= a.z)
            {
                face = d.x > 0 ? 0 : 1;
                uv = {d.x > 0 ? -d.z : d.z, -d.y};
                uv /= a.x;
            } else if (a.y >= a.z)
            {
                face = d.y > 0 ? 2 : 3;
                uv = {d.x, d.y > 0 ? d.z : -d.z};
                uv /= a.y;
            } else
            {
                face = d.z > 0 ? 4 : 5;
                uv = {d.z > 0 ? d.x : -d.x, -d.y};
                uv /= a.z;
            }
            return {face, uv * .5F + .5F};
        }
        auto sample_mip(const ReflectionProbe& p, glm::vec3 d, std::size_t mip) -> glm::vec3
        {
            const auto [face, uv] = face_uv(d);
            const auto size = p.settings.resolution >> mip;
            const auto xy = uv * static_cast<float>(size) - .5F;
            const auto cell = glm::ivec2{glm::floor(xy)};
            const auto fraction = xy - glm::vec2{cell};
            glm::vec3 color{};
            for (int y = 0; y < 2; ++y)
                for (int x = 0; x < 2; ++x)
                {
                    const auto q = cell + glm::ivec2{x, y};
                    // Cross-face taps keep the prefilter continuous at cube edges.
                    const auto [tap_face, tap_uv] =
                        face_uv(reflection_face_direction(face, (glm::vec2{q} + .5F) / static_cast<float>(size)));
                    const auto tap = glm::clamp(
                        glm::ivec2{tap_uv * static_cast<float>(size)},
                        glm::ivec2{0},
                        glm::ivec2{static_cast<int>(size) - 1}
                    );
                    const float weight = (x ? fraction.x : 1 - fraction.x) * (y ? fraction.y : 1 - fraction.y);
                    color += glm::vec3{p.mips[mip][tap_face * size * size + tap.y * size + tap.x]} * weight;
                }
            return color;
        }
    } // namespace

    void validate_reflection_settings(const ReflectionProbeSettings& s)
    {
        require(
            finite(s.position) && finite(s.mins) && finite(s.maxs) && finite(s.maxs - s.mins)
                && glm::all(glm::greaterThan(s.maxs - s.mins, glm::vec3{.3F}))
                && glm::all(glm::greaterThan(s.position, s.mins)) && glm::all(glm::lessThan(s.position, s.maxs)),
            "reflection probe position must be inside finite bounds"
        );
        require(
            s.resolution >= 8 && s.resolution <= 256 && std::has_single_bit(s.resolution),
            "reflection resolution must be a power of two in [8,256]"
        );
        require(s.samples >= 32 && s.samples <= 1024, "reflection samples must be in [32,1024]");
    }
    void validate_reflection_probe(const ReflectionProbe& p)
    {
        validate_reflection_settings(p.settings);
        require(
            p.mips.size() == std::bit_width(p.settings.resolution),
            "reflection probe requires a complete mip chain"
        );
        for (std::size_t mip = 0; mip < p.mips.size(); ++mip)
        {
            const auto size = p.settings.resolution >> mip;
            require(p.mips[mip].size() == 6 * size * size, "reflection cube mip requires six complete faces");
            for (const auto texel : p.mips[mip])
                require(
                    finite(glm::vec3{texel}) && glm::all(glm::greaterThanEqual(glm::vec3{texel}, glm::vec3{0}))
                        && texel.w == 1,
                    "reflection texels require finite nonnegative radiance and alpha 1"
                );
        }
    }
    auto
    reflection_source_signature(const IrradianceBakeScene& scene, const ReflectionProbeSettings& s, glm::vec3 ambient)
        -> std::uint64_t
    {
        validate_reflection_settings(s);
        require(
            finite(ambient) && glm::all(glm::greaterThanEqual(ambient, glm::vec3{0})),
            "reflection ambient must be finite and nonnegative"
        );
        auto hash = irradiance_source_signature(scene, {s.mins, s.maxs, {2, 2, 2}, std::max(64u, s.samples)});
        const auto integer = [&](std::uint32_t value) {
            for (int shift = 0; shift < 32; shift += 8)
                hash = (hash ^ static_cast<std::uint8_t>(value >> shift)) * 1099511628211ULL;
        };
        integer(bake_revision);
        integer(s.resolution);
        integer(s.samples);
        for (int axis = 0; axis < 3; ++axis)
        {
            integer(std::bit_cast<std::uint32_t>(s.position[axis]));
            integer(std::bit_cast<std::uint32_t>(ambient[axis]));
        }
        return hash;
    }
    auto reflection_face_direction(std::uint32_t face, glm::vec2 uv) -> glm::vec3
    {
        const auto q = uv * 2.F - 1.F;
        const std::array directions{
            glm::vec3{1, -q.y, -q.x},
            glm::vec3{-1, -q.y, q.x},
            glm::vec3{q.x, 1, q.y},
            glm::vec3{q.x, -1, -q.y},
            glm::vec3{q.x, -q.y, 1},
            glm::vec3{-q.x, -q.y, -1}
        };
        return glm::normalize(directions.at(face));
    }
    auto sample_reflection_probe(const ReflectionProbe& p, glm::vec3 d, float roughness) -> glm::vec3
    {
        require(
            finite(d) && glm::dot(d, d) > 1.e-8F && std::isfinite(roughness) && roughness >= 0 && roughness <= 1,
            "reflection sample requires nonzero finite direction and roughness in [0,1]"
        );
        const float lod = roughness * static_cast<float>(p.mips.size() - 1);
        const auto first = static_cast<std::size_t>(lod), second = std::min(first + 1, p.mips.size() - 1);
        return glm::mix(sample_mip(p, d, first), sample_mip(p, d, second), lod - static_cast<float>(first));
    }
    auto bake_reflection_probe(const IrradianceBakeScene& scene, const ReflectionProbeSettings& s, glm::vec3 ambient)
        -> ReflectionProbe
    {
        ReflectionProbe p{s, reflection_source_signature(scene, s, ambient), {}};
        BakeTriangles triangles{scene.triangles};
        p.mips.resize(std::bit_width(s.resolution));
        p.mips[0].resize(6 * s.resolution * s.resolution);
        std::size_t backfaces = 0;
        for (std::uint32_t face = 0; face < 6; ++face)
            for (std::uint32_t y = 0; y < s.resolution; ++y)
                for (std::uint32_t x = 0; x < s.resolution; ++x)
                {
                    const auto direction =
                        reflection_face_direction(face, (glm::vec2{x, y} + .5F) / static_cast<float>(s.resolution));
                    float distance = std::numeric_limits<float>::infinity();
                    const auto* hit = triangles.hit(s.position, direction, distance);
                    glm::vec3 radiance{};
                    if (hit)
                    {
                        const auto normal = glm::normalize(glm::cross(hit->b - hit->a, hit->c - hit->a));
                        if (glm::dot(normal, direction) > 0)
                            ++backfaces;
                        else
                        {
                            radiance = hit->diffuse * ambient;
                            const auto point = s.position + direction * distance + normal * .002F;
                            for (const auto& light : scene.lights)
                            {
                                const auto delta = light.position - point;
                                const float length = glm::length(delta);
                                if (length <= .001F || length >= light.radius)
                                    continue;
                                const auto toward = delta / length;
                                const float cosine = std::max(0.F, glm::dot(normal, toward));
                                if (cosine == 0)
                                    continue;
                                float shadow_distance = length - .002F;
                                if (triangles.hit(point, toward, shadow_distance))
                                    continue;
                                const float a = 1 - length / light.radius;
                                radiance += hit->diffuse * light.color * (a * a * (3 - 2 * a) * cosine / pi);
                            }
                        }
                    }
                    p.mips[0][face * s.resolution * s.resolution + y * s.resolution + x] = {radiance, 1};
                }
        require(backfaces < p.mips[0].size() / 2, "reflection probe is inside geometry; move its position");
        for (std::size_t mip = 1; mip < p.mips.size(); ++mip)
        {
            const auto size = s.resolution >> mip;
            const float roughness = static_cast<float>(mip) / static_cast<float>(p.mips.size() - 1);
            const float alpha = roughness * roughness;
            auto& pixels = p.mips[mip];
            pixels.resize(6 * size * size);
            for (std::uint32_t face = 0; face < 6; ++face)
                for (std::uint32_t y = 0; y < size; ++y)
                    for (std::uint32_t x = 0; x < size; ++x)
                    {
                        const auto n =
                            reflection_face_direction(face, (glm::vec2{x, y} + .5F) / static_cast<float>(size));
                        const auto t =
                            glm::normalize(glm::cross(std::abs(n.y) < .99F ? glm::vec3{0, 1, 0} : glm::vec3{1, 0, 0}, n)
                            );
                        const auto b = glm::cross(n, t);
                        glm::vec3 sum{};
                        float weight = 0;
                        for (std::uint32_t i = 0; i < s.samples; ++i)
                        {
                            const float u = (static_cast<float>(i) + .5F) / static_cast<float>(s.samples);
                            const float phi = 2 * pi * std::fmod(static_cast<float>(i) * .61803398875F, 1.F);
                            const float cosine = std::sqrt((1 - u) / (1 + (alpha * alpha - 1) * u));
                            const float sine = std::sqrt(std::max(0.F, 1 - cosine * cosine));
                            const auto h = t * (sine * std::cos(phi)) + b * (sine * std::sin(phi)) + n * cosine;
                            const auto l = 2 * glm::dot(n, h) * h - n;
                            const float nl = std::max(0.F, glm::dot(n, l));
                            if (nl > 0)
                            {
                                sum += sample_mip(p, l, 0) * nl;
                                weight += nl;
                            }
                        }
                        pixels[face * size * size + y * size + x] = {sum / weight, 1};
                    }
        }
        validate_reflection_probe(p);
        return p;
    }
    void write_reflection_probe(const std::filesystem::path& path, const ReflectionProbe& p)
    {
        validate_reflection_probe(p);
        std::ofstream output(path, std::ios::binary | std::ios::trunc);
        const auto integer = [&](std::uint32_t v) {
            for (int shift = 0; shift < 32; shift += 8)
                output.put(static_cast<char>(v >> shift));
        };
        const auto vector = [&](glm::vec3 v) {
            for (int axis = 0; axis < 3; ++axis)
                integer(std::bit_cast<std::uint32_t>(v[axis]));
        };
        integer(magic);
        integer(version);
        vector(p.settings.position);
        vector(p.settings.mins);
        vector(p.settings.maxs);
        integer(p.settings.resolution);
        integer(p.settings.samples);
        integer(static_cast<std::uint32_t>(p.source_signature));
        integer(static_cast<std::uint32_t>(p.source_signature >> 32));
        for (const auto& mip : p.mips)
            for (const auto texel : mip)
            {
                vector(glm::vec3{texel});
                integer(std::bit_cast<std::uint32_t>(texel.w));
            }
        output.close();
        require(static_cast<bool>(output), "cannot write reflection product");
    }
    auto read_reflection_probe(const std::filesystem::path& path) -> ReflectionProbe
    {
        try
        {
            std::ifstream input(path, std::ios::binary | std::ios::ate);
            const auto length = input.tellg();
            require(length >= 60 && length <= 9 * 1024 * 1024, "invalid reflection product size");
            input.seekg(0);
            const auto integer = [&]() -> std::uint32_t {
                std::uint32_t value = 0;
                for (int shift = 0; shift < 32; shift += 8)
                {
                    const auto byte = input.get();
                    require(byte != EOF, "truncated reflection product");
                    value |= static_cast<std::uint32_t>(byte) << shift;
                }
                return value;
            };
            const auto vector = [&]() -> glm::vec3 {
                return {
                    std::bit_cast<float>(integer()),
                    std::bit_cast<float>(integer()),
                    std::bit_cast<float>(integer())
                };
            };
            require(integer() == magic, "invalid reflection product magic");
            require(integer() == version, "unsupported reflection product version");
            ReflectionProbe p;
            p.settings.position = vector();
            p.settings.mins = vector();
            p.settings.maxs = vector();
            p.settings.resolution = integer();
            p.settings.samples = integer();
            p.source_signature = integer();
            p.source_signature |= static_cast<std::uint64_t>(integer()) << 32;
            validate_reflection_settings(p.settings);
            std::size_t expected = 60;
            for (auto size = p.settings.resolution; size; size >>= 1)
                expected += 6 * size * size * 16;
            require(
                length == static_cast<std::streamoff>(expected),
                "reflection product payload size differs from dimensions"
            );
            for (auto size = p.settings.resolution; size; size >>= 1)
            {
                auto& mip = p.mips.emplace_back(6 * size * size);
                for (auto& texel : mip)
                {
                    const auto rgb = vector();
                    texel = {rgb, std::bit_cast<float>(integer())};
                }
            }
            validate_reflection_probe(p);
            return p;
        } catch (const std::exception& error)
        {
            throw std::runtime_error(path.string() + ": " + error.what());
        }
    }
} // namespace RelayCore::render
