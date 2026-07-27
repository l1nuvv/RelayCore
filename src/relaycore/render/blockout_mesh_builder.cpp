#include "relaycore/render/blockout_mesh_builder.h"

#include "relaycore/core/profiling.h"
#include "relaycore/render/authored_color.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <glm/geometric.hpp>
#include <stdexcept>
#include <vector>

namespace RelayCore::render
{
    namespace
    {
        constexpr float kEpsilon = 0.0001F;

        [[nodiscard]] auto nearly_equal(const float a, const float b) -> bool
        {
            return std::abs(a - b) <= kEpsilon;
        }

        enum class FaceAxis : std::uint8_t {
            PosX,
            NegX,
            PosY,
            NegY,
            PosZ,
            NegZ,
        };

        struct Rect2D {
            float u0;
            float u1;
            float v0;
            float v1;
        };

        [[nodiscard]] auto
        material_base_color(const SurfaceMaterial material, const RenderAuthoringConfig& authoring_config) -> glm::vec3
        {
            return material_authoring(authoring_config, material).base_color;
        }

        [[nodiscard]] auto shade(const glm::vec3& color, const float factor) -> glm::vec3
        {
            return color * factor;
        }

        [[nodiscard]] auto same_surface_appearance(const BlockoutSolid& a, const BlockoutSolid& b) -> bool
        {
            return a.material == b.material && nearly_equal(a.brightness, b.brightness)
                   && nearly_equal(a.tint.x, b.tint.x) && nearly_equal(a.tint.y, b.tint.y)
                   && nearly_equal(a.tint.z, b.tint.z);
        }

        [[nodiscard]] auto face_normal(const FaceAxis face) -> glm::vec3
        {
            switch (face)
            {
                case FaceAxis::PosX:
                    return {1.0F, 0.0F, 0.0F};
                case FaceAxis::NegX:
                    return {-1.0F, 0.0F, 0.0F};
                case FaceAxis::PosY:
                    return {0.0F, 1.0F, 0.0F};
                case FaceAxis::NegY:
                    return {0.0F, -1.0F, 0.0F};
                case FaceAxis::PosZ:
                    return {0.0F, 0.0F, 1.0F};
                case FaceAxis::NegZ:
                    return {0.0F, 0.0F, -1.0F};
            }

            return {0.0F, 0.0F, 1.0F};
        }

        [[nodiscard]] auto face_depth(const BlockoutSolid& solid, const FaceAxis face) -> float
        {
            switch (face)
            {
                case FaceAxis::PosX:
                case FaceAxis::NegX:
                    return solid.maxs.x - solid.mins.x;
                case FaceAxis::PosY:
                case FaceAxis::NegY:
                    return solid.maxs.y - solid.mins.y;
                case FaceAxis::PosZ:
                case FaceAxis::NegZ:
                    return solid.maxs.z - solid.mins.z;
            }

            return 0.0F;
        }

        [[nodiscard]] auto
        material_uses_bevel(const SurfaceMaterial material, const RenderAuthoringConfig& authoring_config) -> bool
        {
            return material_authoring(authoring_config, material).bevel_enabled;
        }

        [[nodiscard]] auto solid_uses_bevel(const BlockoutSolid& solid, const RenderAuthoringConfig& authoring_config)
            -> bool
        {
            if (!material_uses_bevel(solid.material, authoring_config))
            {
                return false;
            }

            const glm::vec3 extents = solid.maxs - solid.mins;
            std::array<float, 3> dimensions{extents.x, extents.y, extents.z};
            std::ranges::sort(dimensions);

            return dimensions[2] >= 2.0F && dimensions[1] >= 1.0F && dimensions[0] >= 0.18F;
        }

        [[nodiscard]] auto bevel_size(
            const BlockoutSolid& solid,
            const FaceAxis face,
            const Rect2D& rect,
            const RenderAuthoringConfig& authoring_config
        ) -> float
        {
            if (!solid_uses_bevel(solid, authoring_config))
            {
                return 0.0F;
            }

            const float rect_width = rect.u1 - rect.u0;
            const float rect_height = rect.v1 - rect.v0;
            const float depth = face_depth(solid, face);
            if (rect_width <= kEpsilon || rect_height <= kEpsilon || depth <= kEpsilon)
            {
                return 0.0F;
            }

            return std::min({0.06F, rect_width * 0.12F, rect_height * 0.12F, depth * 0.22F});
        }

        enum class ThinAxis : std::uint8_t {
            X,
            Y,
            Z,
        };

        [[nodiscard]] auto thinnest_axis(const BlockoutSolid& solid) -> ThinAxis
        {
            const glm::vec3 extents = solid.maxs - solid.mins;
            if (extents.x <= extents.y && extents.x <= extents.z)
            {
                return ThinAxis::X;
            }

            if (extents.y <= extents.x && extents.y <= extents.z)
            {
                return ThinAxis::Y;
            }

            return ThinAxis::Z;
        }

        void append_quad(
            CpuColorMeshData& mesh,
            const glm::vec3& a,
            const glm::vec3& b,
            const glm::vec3& c,
            const glm::vec3& d,
            const glm::vec3& color,
            const glm::vec3& normal,
            const float material_kind
        )
        {
            const auto base_index = static_cast<std::uint32_t>(mesh.vertices.size());

            mesh.vertices.push_back(ColorVertex{a, color, normal, material_kind});
            mesh.vertices.push_back(ColorVertex{b, color, normal, material_kind});
            mesh.vertices.push_back(ColorVertex{c, color, normal, material_kind});
            mesh.vertices.push_back(ColorVertex{d, color, normal, material_kind});

            mesh.indices.push_back(base_index + 0);
            mesh.indices.push_back(base_index + 1);
            mesh.indices.push_back(base_index + 2);
            mesh.indices.push_back(base_index + 0);
            mesh.indices.push_back(base_index + 2);
            mesh.indices.push_back(base_index + 3);
        }

        void append_oriented_quad(
            CpuColorMeshData& mesh,
            const glm::vec3& a,
            const glm::vec3& b,
            const glm::vec3& c,
            const glm::vec3& d,
            const glm::vec3& color,
            const glm::vec3& desired_normal,
            const float material_kind
        )
        {
            const glm::vec3 normal = glm::normalize(desired_normal);
            const glm::vec3 computed = glm::cross(b - a, c - a);
            if (glm::dot(computed, normal) < 0.0F)
            {
                append_quad(mesh, a, d, c, b, color, normal, material_kind);
                return;
            }

            append_quad(mesh, a, b, c, d, color, normal, material_kind);
        }

        void append_glass_panel(CpuColorMeshData& mesh, const BlockoutSolid& solid, const glm::vec3& color)
        {
            const float material_kind = static_cast<float>(static_cast<std::uint8_t>(solid.material));
            const ThinAxis axis = thinnest_axis(solid);

            if (axis == ThinAxis::X)
            {
                const float x = (solid.mins.x + solid.maxs.x) * 0.5F;
                const glm::vec3 a{x, solid.mins.y, solid.mins.z};
                const glm::vec3 b{x, solid.maxs.y, solid.mins.z};
                const glm::vec3 c{x, solid.maxs.y, solid.maxs.z};
                const glm::vec3 d{x, solid.mins.y, solid.maxs.z};
                append_oriented_quad(mesh, a, b, c, d, color, {1.0F, 0.0F, 0.0F}, material_kind);
                return;
            }

            if (axis == ThinAxis::Y)
            {
                const float y = (solid.mins.y + solid.maxs.y) * 0.5F;
                const glm::vec3 a{solid.mins.x, y, solid.mins.z};
                const glm::vec3 b{solid.maxs.x, y, solid.mins.z};
                const glm::vec3 c{solid.maxs.x, y, solid.maxs.z};
                const glm::vec3 d{solid.mins.x, y, solid.maxs.z};
                append_oriented_quad(mesh, a, b, c, d, color, {0.0F, 1.0F, 0.0F}, material_kind);
                return;
            }

            const float z = (solid.mins.z + solid.maxs.z) * 0.5F;
            const glm::vec3 a{solid.mins.x, solid.mins.y, z};
            const glm::vec3 b{solid.maxs.x, solid.mins.y, z};
            const glm::vec3 c{solid.maxs.x, solid.maxs.y, z};
            const glm::vec3 d{solid.mins.x, solid.maxs.y, z};
            append_oriented_quad(mesh, a, b, c, d, color, {0.0F, 0.0F, 1.0F}, material_kind);
        }

        [[nodiscard]] auto rect_valid(const Rect2D& rect) -> bool
        {
            return (rect.u1 - rect.u0) > kEpsilon && (rect.v1 - rect.v0) > kEpsilon;
        }

        [[nodiscard]] auto rect_intersection(const Rect2D& a, const Rect2D& b) -> Rect2D
        {
            return {
                std::max(a.u0, b.u0),
                std::min(a.u1, b.u1),
                std::max(a.v0, b.v0),
                std::min(a.v1, b.v1),
            };
        }

        void subtract_rect(std::vector<Rect2D>& visible_rects, const Rect2D& cutter)
        {
            std::vector<Rect2D> next_rects;
            next_rects.reserve(visible_rects.size() * 4);

            for (const Rect2D& rect : visible_rects)
            {
                const Rect2D overlap = rect_intersection(rect, cutter);
                if (!rect_valid(overlap))
                {
                    next_rects.push_back(rect);
                    continue;
                }

                if ((overlap.v0 - rect.v0) > kEpsilon)
                {
                    next_rects.push_back(Rect2D{rect.u0, rect.u1, rect.v0, overlap.v0});
                }

                if ((rect.v1 - overlap.v1) > kEpsilon)
                {
                    next_rects.push_back(Rect2D{rect.u0, rect.u1, overlap.v1, rect.v1});
                }

                if ((overlap.u0 - rect.u0) > kEpsilon)
                {
                    next_rects.push_back(Rect2D{rect.u0, overlap.u0, overlap.v0, overlap.v1});
                }

                if ((rect.u1 - overlap.u1) > kEpsilon)
                {
                    next_rects.push_back(Rect2D{overlap.u1, rect.u1, overlap.v0, overlap.v1});
                }
            }

            visible_rects = std::move(next_rects);
        }

        [[nodiscard]] auto face_rect(const BlockoutSolid& solid, const FaceAxis face) -> Rect2D
        {
            switch (face)
            {
                case FaceAxis::PosX:
                case FaceAxis::NegX:
                    return {solid.mins.z, solid.maxs.z, solid.mins.y, solid.maxs.y};
                case FaceAxis::PosY:
                case FaceAxis::NegY:
                    return {solid.mins.x, solid.maxs.x, solid.mins.z, solid.maxs.z};
                case FaceAxis::PosZ:
                case FaceAxis::NegZ:
                    return {solid.mins.x, solid.maxs.x, solid.mins.y, solid.maxs.y};
            }

            throw std::runtime_error("Unsupported face axis");
        }

        [[nodiscard]] auto face_plane(const BlockoutSolid& solid, const FaceAxis face) -> float
        {
            switch (face)
            {
                case FaceAxis::PosX:
                    return solid.maxs.x;
                case FaceAxis::NegX:
                    return solid.mins.x;
                case FaceAxis::PosY:
                    return solid.maxs.y;
                case FaceAxis::NegY:
                    return solid.mins.y;
                case FaceAxis::PosZ:
                    return solid.maxs.z;
                case FaceAxis::NegZ:
                    return solid.mins.z;
            }

            throw std::runtime_error("Unsupported face axis");
        }

        [[nodiscard]] auto face_shade_factor(const FaceAxis face) -> float
        {
            switch (face)
            {
                case FaceAxis::PosZ:
                    return 1.00F;
                case FaceAxis::NegZ:
                    return 0.72F;
                case FaceAxis::NegX:
                case FaceAxis::PosX:
                    return 0.86F;
                case FaceAxis::PosY:
                    return 1.12F;
                case FaceAxis::NegY:
                    return 0.58F;
            }

            return 1.0F;
        }

        [[nodiscard]] auto face_cover_rect(const BlockoutSolid& solid, const BlockoutSolid& other, const FaceAxis face)
            -> Rect2D
        {
            switch (face)
            {
                case FaceAxis::PosX:
                case FaceAxis::NegX:
                    return {
                        std::max(solid.mins.z, other.mins.z),
                        std::min(solid.maxs.z, other.maxs.z),
                        std::max(solid.mins.y, other.mins.y),
                        std::min(solid.maxs.y, other.maxs.y),
                    };
                case FaceAxis::PosY:
                case FaceAxis::NegY:
                    return {
                        std::max(solid.mins.x, other.mins.x),
                        std::min(solid.maxs.x, other.maxs.x),
                        std::max(solid.mins.z, other.mins.z),
                        std::min(solid.maxs.z, other.maxs.z),
                    };
                case FaceAxis::PosZ:
                case FaceAxis::NegZ:
                    return {
                        std::max(solid.mins.x, other.mins.x),
                        std::min(solid.maxs.x, other.maxs.x),
                        std::max(solid.mins.y, other.mins.y),
                        std::min(solid.maxs.y, other.maxs.y),
                    };
            }

            throw std::runtime_error("Unsupported face axis");
        }

        [[nodiscard]] auto
        solid_covers_face_from_outside(const BlockoutSolid& solid, const BlockoutSolid& other, const FaceAxis face)
            -> bool
        {
            const float plane = face_plane(solid, face);
            const Rect2D cover_rect = face_cover_rect(solid, other, face);
            if (!rect_valid(cover_rect))
            {
                return false;
            }

            switch (face)
            {
                case FaceAxis::PosX:
                    return other.mins.x <= plane + kEpsilon && other.maxs.x > plane + kEpsilon;
                case FaceAxis::NegX:
                    return other.maxs.x >= plane - kEpsilon && other.mins.x < plane - kEpsilon;
                case FaceAxis::PosY:
                    return other.mins.y <= plane + kEpsilon && other.maxs.y > plane + kEpsilon;
                case FaceAxis::NegY:
                    return other.maxs.y >= plane - kEpsilon && other.mins.y < plane - kEpsilon;
                case FaceAxis::PosZ:
                    return other.mins.z <= plane + kEpsilon && other.maxs.z > plane + kEpsilon;
                case FaceAxis::NegZ:
                    return other.maxs.z >= plane - kEpsilon && other.mins.z < plane - kEpsilon;
            }

            return false;
        }

        [[nodiscard]] auto
        solid_has_coplanar_face(const BlockoutSolid& solid, const BlockoutSolid& other, const FaceAxis face) -> bool
        {
            const float plane = face_plane(solid, face);
            const Rect2D cover_rect = face_cover_rect(solid, other, face);
            if (!rect_valid(cover_rect))
            {
                return false;
            }

            switch (face)
            {
                case FaceAxis::PosX:
                    return nearly_equal(other.maxs.x, plane) && other.mins.x < plane - kEpsilon;
                case FaceAxis::NegX:
                    return nearly_equal(other.mins.x, plane) && other.maxs.x > plane + kEpsilon;
                case FaceAxis::PosY:
                    return nearly_equal(other.maxs.y, plane) && other.mins.y < plane - kEpsilon;
                case FaceAxis::NegY:
                    return nearly_equal(other.mins.y, plane) && other.maxs.y > plane + kEpsilon;
                case FaceAxis::PosZ:
                    return nearly_equal(other.maxs.z, plane) && other.mins.z < plane - kEpsilon;
                case FaceAxis::NegZ:
                    return nearly_equal(other.mins.z, plane) && other.maxs.z > plane + kEpsilon;
            }

            return false;
        }

        [[nodiscard]] auto coplanar_face_is_overridden(
            const BlockoutSolid& solid,
            const std::size_t solid_index,
            const BlockoutSolid& other,
            const std::size_t other_index,
            const FaceAxis face
        ) -> bool
        {
            if (!solid_has_coplanar_face(solid, other, face))
            {
                return false;
            }

            const float solid_depth = face_depth(solid, face);
            const float other_depth = face_depth(other, face);
            if (other_depth + kEpsilon < solid_depth)
            {
                return true;
            }
            if (solid_depth + kEpsilon < other_depth)
            {
                return false;
            }

            // Later authored solids win exact coplanar ties so one visible owner remains.
            return other_index > solid_index;
        }

        void append_face_rect(
            CpuColorMeshData& mesh,
            const FaceAxis face,
            const float plane,
            const Rect2D& rect,
            const glm::vec3& color,
            const float material_kind
        )
        {
            switch (face)
            {
                case FaceAxis::PosZ:
                    append_quad(
                        mesh,
                        {rect.u0, rect.v0, plane},
                        {rect.u1, rect.v0, plane},
                        {rect.u1, rect.v1, plane},
                        {rect.u0, rect.v1, plane},
                        color,
                        face_normal(face),
                        material_kind
                    );
                    return;
                case FaceAxis::NegZ:
                    append_quad(
                        mesh,
                        {rect.u1, rect.v0, plane},
                        {rect.u0, rect.v0, plane},
                        {rect.u0, rect.v1, plane},
                        {rect.u1, rect.v1, plane},
                        color,
                        face_normal(face),
                        material_kind
                    );
                    return;
                case FaceAxis::NegX:
                    append_quad(
                        mesh,
                        {plane, rect.v0, rect.u0},
                        {plane, rect.v0, rect.u1},
                        {plane, rect.v1, rect.u1},
                        {plane, rect.v1, rect.u0},
                        color,
                        face_normal(face),
                        material_kind
                    );
                    return;
                case FaceAxis::PosX:
                    append_quad(
                        mesh,
                        {plane, rect.v0, rect.u1},
                        {plane, rect.v0, rect.u0},
                        {plane, rect.v1, rect.u0},
                        {plane, rect.v1, rect.u1},
                        color,
                        face_normal(face),
                        material_kind
                    );
                    return;
                case FaceAxis::PosY:
                    append_quad(
                        mesh,
                        {rect.u0, plane, rect.v1},
                        {rect.u1, plane, rect.v1},
                        {rect.u1, plane, rect.v0},
                        {rect.u0, plane, rect.v0},
                        color,
                        face_normal(face),
                        material_kind
                    );
                    return;
                case FaceAxis::NegY:
                    append_quad(
                        mesh,
                        {rect.u0, plane, rect.v0},
                        {rect.u1, plane, rect.v0},
                        {rect.u1, plane, rect.v1},
                        {rect.u0, plane, rect.v1},
                        color,
                        face_normal(face),
                        material_kind
                    );
                    return;
            }
        }

        [[nodiscard]] auto face_corners(const FaceAxis face, const float plane, const Rect2D& rect)
            -> std::array<glm::vec3, 4>
        {
            switch (face)
            {
                case FaceAxis::PosZ:
                    return {{
                        {rect.u0, rect.v0, plane},
                        {rect.u1, rect.v0, plane},
                        {rect.u1, rect.v1, plane},
                        {rect.u0, rect.v1, plane},
                    }};
                case FaceAxis::NegZ:
                    return {{
                        {rect.u1, rect.v0, plane},
                        {rect.u0, rect.v0, plane},
                        {rect.u0, rect.v1, plane},
                        {rect.u1, rect.v1, plane},
                    }};
                case FaceAxis::NegX:
                    return {{
                        {plane, rect.v0, rect.u0},
                        {plane, rect.v0, rect.u1},
                        {plane, rect.v1, rect.u1},
                        {plane, rect.v1, rect.u0},
                    }};
                case FaceAxis::PosX:
                    return {{
                        {plane, rect.v0, rect.u1},
                        {plane, rect.v0, rect.u0},
                        {plane, rect.v1, rect.u0},
                        {plane, rect.v1, rect.u1},
                    }};
                case FaceAxis::PosY:
                    return {{
                        {rect.u0, plane, rect.v1},
                        {rect.u1, plane, rect.v1},
                        {rect.u1, plane, rect.v0},
                        {rect.u0, plane, rect.v0},
                    }};
                case FaceAxis::NegY:
                    return {{
                        {rect.u0, plane, rect.v0},
                        {rect.u1, plane, rect.v0},
                        {rect.u1, plane, rect.v1},
                        {rect.u0, plane, rect.v1},
                    }};
            }

            throw std::runtime_error("Unsupported face axis");
        }

        [[nodiscard]] auto edge_adjacent_normal(const FaceAxis face, const std::size_t edge_index) -> glm::vec3
        {
            switch (face)
            {
                case FaceAxis::PosZ:
                    switch (edge_index)
                    {
                        case 0:
                            return {0.0F, -1.0F, 0.0F};
                        case 1:
                            return {1.0F, 0.0F, 0.0F};
                        case 2:
                            return {0.0F, 1.0F, 0.0F};
                        default:
                            return {-1.0F, 0.0F, 0.0F};
                    }
                case FaceAxis::NegZ:
                    switch (edge_index)
                    {
                        case 0:
                            return {0.0F, -1.0F, 0.0F};
                        case 1:
                            return {-1.0F, 0.0F, 0.0F};
                        case 2:
                            return {0.0F, 1.0F, 0.0F};
                        default:
                            return {1.0F, 0.0F, 0.0F};
                    }
                case FaceAxis::PosX:
                    switch (edge_index)
                    {
                        case 0:
                            return {0.0F, -1.0F, 0.0F};
                        case 1:
                            return {0.0F, 0.0F, -1.0F};
                        case 2:
                            return {0.0F, 1.0F, 0.0F};
                        default:
                            return {0.0F, 0.0F, 1.0F};
                    }
                case FaceAxis::NegX:
                    switch (edge_index)
                    {
                        case 0:
                            return {0.0F, -1.0F, 0.0F};
                        case 1:
                            return {0.0F, 0.0F, 1.0F};
                        case 2:
                            return {0.0F, 1.0F, 0.0F};
                        default:
                            return {0.0F, 0.0F, -1.0F};
                    }
                case FaceAxis::PosY:
                    switch (edge_index)
                    {
                        case 0:
                            return {0.0F, 0.0F, 1.0F};
                        case 1:
                            return {1.0F, 0.0F, 0.0F};
                        case 2:
                            return {0.0F, 0.0F, -1.0F};
                        default:
                            return {-1.0F, 0.0F, 0.0F};
                    }
                case FaceAxis::NegY:
                    switch (edge_index)
                    {
                        case 0:
                            return {0.0F, 0.0F, -1.0F};
                        case 1:
                            return {1.0F, 0.0F, 0.0F};
                        case 2:
                            return {0.0F, 0.0F, 1.0F};
                        default:
                            return {-1.0F, 0.0F, 0.0F};
                    }
            }

            return {0.0F, 1.0F, 0.0F};
        }

        void append_beveled_face_rect(
            CpuColorMeshData& mesh,
            const FaceAxis face,
            const float plane,
            const Rect2D& rect,
            const glm::vec3& color,
            const float material_kind,
            const float bevel
        )
        {
            if (bevel <= kEpsilon || (rect.u1 - rect.u0) <= (bevel * 2.0F + kEpsilon)
                || (rect.v1 - rect.v0) <= (bevel * 2.0F + kEpsilon))
            {
                append_face_rect(mesh, face, plane, rect, color, material_kind);
                return;
            }

            Rect2D inner_rect{
                rect.u0 + bevel,
                rect.u1 - bevel,
                rect.v0 + bevel,
                rect.v1 - bevel,
            };

            const glm::vec3 face_dir = face_normal(face);
            const float inner_plane = plane - bevel * (face_dir.x + face_dir.y + face_dir.z);
            const auto outer = face_corners(face, plane, rect);
            const auto inner = face_corners(face, inner_plane, inner_rect);

            append_oriented_quad(mesh, inner[0], inner[1], inner[2], inner[3], color, face_dir, material_kind);

            for (std::size_t edge_index = 0; edge_index < 4; ++edge_index)
            {
                const std::size_t next_index = (edge_index + 1) % 4;
                const glm::vec3 bevel_normal = glm::normalize(face_dir + edge_adjacent_normal(face, edge_index));
                append_oriented_quad(
                    mesh,
                    outer[edge_index],
                    outer[next_index],
                    inner[next_index],
                    inner[edge_index],
                    shade(color, 0.92F),
                    bevel_normal,
                    material_kind
                );
            }
        }

        void append_visible_face(
            CpuColorMeshData& mesh,
            const BlockoutSolid& solid,
            const std::vector<BlockoutSolid>& solids,
            const std::size_t solid_index,
            const FaceAxis face,
            const glm::vec3& color,
            const RenderAuthoringConfig& authoring_config
        )
        {
            const Rect2D original_rect = face_rect(solid, face);
            std::vector<Rect2D> visible_rects{original_rect};
            bool face_was_cut = false;

            for (std::size_t other_index = 0; other_index < solids.size(); ++other_index)
            {
                if (other_index == solid_index)
                {
                    continue;
                }

                const BlockoutSolid& other = solids[other_index];

                const bool covered_from_outside = solid_covers_face_from_outside(solid, other, face);

                // Для копланарных дубликатов нужен детерминированный "владелец" overlap.
                // Иначе два solids вычтут друг друга и можно получить дырку.
                const bool covered_by_coplanar_face =
                    coplanar_face_is_overridden(solid, solid_index, other, other_index, face);

                if (!covered_from_outside && !covered_by_coplanar_face)
                {
                    continue;
                }

                subtract_rect(visible_rects, face_cover_rect(solid, other, face));
                face_was_cut = true;
                if (visible_rects.empty())
                {
                    return;
                }
            }

            const float plane = face_plane(solid, face);
            const float material_kind = static_cast<float>(static_cast<std::uint8_t>(solid.material));
            for (const Rect2D& rect : visible_rects)
            {
                // Bevel only intact faces. Applying it to rects carved by overlaps creates visible trenches
                // around props and panel inserts that read as gaps in the world.
                const float bevel = face_was_cut ? 0.0F : bevel_size(solid, face, rect, authoring_config);
                if (bevel > 0.0F)
                {
                    append_beveled_face_rect(mesh, face, plane, rect, color, material_kind, bevel);
                    continue;
                }

                append_face_rect(mesh, face, plane, rect, color, material_kind);
            }
        }
    } // namespace

    [[nodiscard]] auto build_blockout_mesh_for_solid(
        const BlockoutLevel& level,
        const std::size_t solid_index,
        const RenderAuthoringConfig& authoring_config
    ) -> CpuColorMeshData
    {
        const BlockoutSolid& solid = level.solids[solid_index];
        if (solid.mins.x >= solid.maxs.x || solid.mins.y >= solid.maxs.y || solid.mins.z >= solid.maxs.z)
        {
            throw std::runtime_error("Blockout solid has invalid bounds");
        }

        CpuColorMeshData mesh{};
        mesh.vertices.reserve(24);
        mesh.indices.reserve(36);

        const auto& material = material_authoring(authoring_config, solid.material);
        const glm::vec3 base_color = compose_legacy_surface_color(material.base_color, solid.tint, solid.brightness);

        if (material.transparent)
        {
            append_glass_panel(mesh, solid, shade(base_color, 1.0F));
            return mesh;
        }

        append_visible_face(
            mesh,
            solid,
            level.solids,
            solid_index,
            FaceAxis::PosZ,
            shade(base_color, face_shade_factor(FaceAxis::PosZ)),
            authoring_config
        );
        append_visible_face(
            mesh,
            solid,
            level.solids,
            solid_index,
            FaceAxis::NegZ,
            shade(base_color, face_shade_factor(FaceAxis::NegZ)),
            authoring_config
        );
        append_visible_face(
            mesh,
            solid,
            level.solids,
            solid_index,
            FaceAxis::NegX,
            shade(base_color, face_shade_factor(FaceAxis::NegX)),
            authoring_config
        );
        append_visible_face(
            mesh,
            solid,
            level.solids,
            solid_index,
            FaceAxis::PosX,
            shade(base_color, face_shade_factor(FaceAxis::PosX)),
            authoring_config
        );
        append_visible_face(
            mesh,
            solid,
            level.solids,
            solid_index,
            FaceAxis::PosY,
            shade(base_color, face_shade_factor(FaceAxis::PosY)),
            authoring_config
        );
        append_visible_face(
            mesh,
            solid,
            level.solids,
            solid_index,
            FaceAxis::NegY,
            shade(base_color, face_shade_factor(FaceAxis::NegY)),
            authoring_config
        );

        return mesh;
    }

    auto build_blockout_mesh(const BlockoutLevel& level, const RenderAuthoringConfig& authoring_config)
        -> CpuColorMeshData
    {
        RELAYCORE_PROFILE_ZONE_N("build_blockout_mesh");

        CpuColorMeshData mesh{};
        mesh.vertices.reserve(level.solids.size() * 24);
        mesh.indices.reserve(level.solids.size() * 36);

        for (std::size_t solid_index = 0; solid_index < level.solids.size(); ++solid_index)
        {
            CpuColorMeshData solid_mesh = build_blockout_mesh_for_solid(level, solid_index, authoring_config);

            const std::uint32_t base_index = static_cast<std::uint32_t>(mesh.vertices.size());
            mesh.vertices.insert(mesh.vertices.end(), solid_mesh.vertices.begin(), solid_mesh.vertices.end());

            for (const std::uint32_t index : solid_mesh.indices)
            {
                mesh.indices.push_back(base_index + index);
            }
        }

        return mesh;
    }

    auto build_blockout_mesh(const BlockoutLevel& level) -> CpuColorMeshData
    {
        return build_blockout_mesh(level, default_render_authoring_config());
    }

    auto build_blockout_render_data(const BlockoutLevel& level, const RenderAuthoringConfig& authoring_config)
        -> BlockoutRenderData
    {
        RELAYCORE_PROFILE_ZONE_N("build_blockout_render_data");

        BlockoutRenderData render_data{};
        render_data.chunks.reserve(level.solids.size());

        for (std::size_t solid_index = 0; solid_index < level.solids.size(); ++solid_index)
        {
            const BlockoutSolid& solid = level.solids[solid_index];
            const auto& material = material_authoring(authoring_config, solid.material);

            BlockoutRenderChunkData chunk{};
            chunk.mesh = build_blockout_mesh_for_solid(level, solid_index, authoring_config);
            if (chunk.mesh.indices.empty())
            {
                continue;
            }

            chunk.mins = solid.mins;
            chunk.maxs = solid.maxs;
            chunk.triangle_count = static_cast<std::uint32_t>(chunk.mesh.indices.size() / 3);
            chunk.transparent = material.transparent;

            render_data.total_triangles += chunk.triangle_count;
            render_data.chunks.push_back(std::move(chunk));
        }

        return render_data;
    }

    auto build_blockout_render_data(const BlockoutLevel& level) -> BlockoutRenderData
    {
        return build_blockout_render_data(level, default_render_authoring_config());
    }
} // namespace RelayCore::render
