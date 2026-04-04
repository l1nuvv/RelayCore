#include "relaycore/render/blockout_mesh_builder.h"

#include <algorithm>
#include <cstdint>
#include <stdexcept>
#include <vector>

namespace RelayCore::render
{
    namespace
    {
        constexpr float kEpsilon = 0.0001F;

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

        [[nodiscard]] auto material_base_color(const SurfaceMaterial material) -> glm::vec3
        {
            switch (material)
            {
                case SurfaceMaterial::Concrete: return {0.62F, 0.64F, 0.67F};
                case SurfaceMaterial::Metal: return {0.60F, 0.67F, 0.74F};
                case SurfaceMaterial::Tile: return {0.73F, 0.76F, 0.78F};
                case SurfaceMaterial::Glass: return {0.45F, 0.72F, 0.84F};
                case SurfaceMaterial::Hazard: return {0.92F, 0.78F, 0.18F};

                case SurfaceMaterial::Floor: return {0.36F, 0.36F, 0.38F};
                case SurfaceMaterial::Ceiling: return {0.30F, 0.31F, 0.33F};
                case SurfaceMaterial::Wall: return {0.56F, 0.58F, 0.62F};
                case SurfaceMaterial::Corridor: return {0.42F, 0.46F, 0.52F};

                case SurfaceMaterial::Platform: return {0.52F, 0.52F, 0.58F};
                case SurfaceMaterial::PlatformTop: return {0.78F, 0.72F, 0.30F};
                case SurfaceMaterial::PlatformTop2: return {0.92F, 0.52F, 0.22F};

                case SurfaceMaterial::Pillar: return {0.50F, 0.54F, 0.58F};
                case SurfaceMaterial::Cover: return {0.28F, 0.44F, 0.34F};
                case SurfaceMaterial::Block: return {0.58F, 0.48F, 0.36F};
                case SurfaceMaterial::Beam: return {0.67F, 0.42F, 0.18F};
                case SurfaceMaterial::Divider: return {0.32F, 0.76F, 0.88F};
                case SurfaceMaterial::Crate: return {0.55F, 0.39F, 0.22F};
                case SurfaceMaterial::Post: return {0.82F, 0.82F, 0.86F};
            }

            return {1.0F, 0.0F, 1.0F};
        }

        [[nodiscard]] auto shade(const glm::vec3& color, const float factor) -> glm::vec3 { return color * factor; }

        void append_quad(
            CpuColorMeshData& mesh,
            const glm::vec3&  a,
            const glm::vec3&  b,
            const glm::vec3&  c,
            const glm::vec3&  d,
            const glm::vec3&  color
        )
        {
            const auto base_index = static_cast<std::uint32_t>(mesh.vertices.size());

            mesh.vertices.push_back(ColorVertex{a, color});
            mesh.vertices.push_back(ColorVertex{b, color});
            mesh.vertices.push_back(ColorVertex{c, color});
            mesh.vertices.push_back(ColorVertex{d, color});

            mesh.indices.push_back(base_index + 0);
            mesh.indices.push_back(base_index + 1);
            mesh.indices.push_back(base_index + 2);
            mesh.indices.push_back(base_index + 0);
            mesh.indices.push_back(base_index + 2);
            mesh.indices.push_back(base_index + 3);
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

            for (const Rect2D& rect: visible_rects)
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
                case FaceAxis::NegX: return {solid.mins.z, solid.maxs.z, solid.mins.y, solid.maxs.y};
                case FaceAxis::PosY:
                case FaceAxis::NegY: return {solid.mins.x, solid.maxs.x, solid.mins.z, solid.maxs.z};
                case FaceAxis::PosZ:
                case FaceAxis::NegZ: return {solid.mins.x, solid.maxs.x, solid.mins.y, solid.maxs.y};
            }

            throw std::runtime_error("Unsupported face axis");
        }

        [[nodiscard]] auto face_plane(const BlockoutSolid& solid, const FaceAxis face) -> float
        {
            switch (face)
            {
                case FaceAxis::PosX: return solid.maxs.x;
                case FaceAxis::NegX: return solid.mins.x;
                case FaceAxis::PosY: return solid.maxs.y;
                case FaceAxis::NegY: return solid.mins.y;
                case FaceAxis::PosZ: return solid.maxs.z;
                case FaceAxis::NegZ: return solid.mins.z;
            }

            throw std::runtime_error("Unsupported face axis");
        }

        [[nodiscard]] auto face_shade_factor(const FaceAxis face) -> float
        {
            switch (face)
            {
                case FaceAxis::PosZ: return 1.00F;
                case FaceAxis::NegZ: return 0.72F;
                case FaceAxis::NegX:
                case FaceAxis::PosX: return 0.86F;
                case FaceAxis::PosY: return 1.12F;
                case FaceAxis::NegY: return 0.58F;
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
            const float  plane      = face_plane(solid, face);
            const Rect2D cover_rect = face_cover_rect(solid, other, face);
            if (!rect_valid(cover_rect)) { return false; }

            switch (face)
            {
                case FaceAxis::PosX: return other.mins.x <= plane + kEpsilon && other.maxs.x > plane + kEpsilon;
                case FaceAxis::NegX: return other.maxs.x >= plane - kEpsilon && other.mins.x < plane - kEpsilon;
                case FaceAxis::PosY: return other.mins.y <= plane + kEpsilon && other.maxs.y > plane + kEpsilon;
                case FaceAxis::NegY: return other.maxs.y >= plane - kEpsilon && other.mins.y < plane - kEpsilon;
                case FaceAxis::PosZ: return other.mins.z <= plane + kEpsilon && other.maxs.z > plane + kEpsilon;
                case FaceAxis::NegZ: return other.maxs.z >= plane - kEpsilon && other.mins.z < plane - kEpsilon;
            }

            return false;
        }

        void append_face_rect(
            CpuColorMeshData& mesh, const FaceAxis face, const float plane, const Rect2D& rect, const glm::vec3& color
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
                        color
                    );
                    return;
                case FaceAxis::NegZ:
                    append_quad(
                        mesh,
                        {rect.u1, rect.v0, plane},
                        {rect.u0, rect.v0, plane},
                        {rect.u0, rect.v1, plane},
                        {rect.u1, rect.v1, plane},
                        color
                    );
                    return;
                case FaceAxis::NegX:
                    append_quad(
                        mesh,
                        {plane, rect.v0, rect.u0},
                        {plane, rect.v0, rect.u1},
                        {plane, rect.v1, rect.u1},
                        {plane, rect.v1, rect.u0},
                        color
                    );
                    return;
                case FaceAxis::PosX:
                    append_quad(
                        mesh,
                        {plane, rect.v0, rect.u1},
                        {plane, rect.v0, rect.u0},
                        {plane, rect.v1, rect.u0},
                        {plane, rect.v1, rect.u1},
                        color
                    );
                    return;
                case FaceAxis::PosY:
                    append_quad(
                        mesh,
                        {rect.u0, plane, rect.v1},
                        {rect.u1, plane, rect.v1},
                        {rect.u1, plane, rect.v0},
                        {rect.u0, plane, rect.v0},
                        color
                    );
                    return;
                case FaceAxis::NegY:
                    append_quad(
                        mesh,
                        {rect.u0, plane, rect.v0},
                        {rect.u1, plane, rect.v0},
                        {rect.u1, plane, rect.v1},
                        {rect.u0, plane, rect.v1},
                        color
                    );
                    return;
            }
        }

        void append_visible_face(
            CpuColorMeshData&                 mesh,
            const BlockoutSolid&              solid,
            const std::vector<BlockoutSolid>& solids,
            const std::size_t                 solid_index,
            const FaceAxis                    face,
            const glm::vec3&                  color
        )
        {
            std::vector<Rect2D> visible_rects{face_rect(solid, face)};

            for (std::size_t other_index = 0; other_index < solids.size(); ++other_index)
            {
                if (other_index == solid_index) { continue; }

                const BlockoutSolid& other = solids[other_index];
                if (!solid_covers_face_from_outside(solid, other, face)) { continue; }

                subtract_rect(visible_rects, face_cover_rect(solid, other, face));
                if (visible_rects.empty()) { return; }
            }

            const float plane = face_plane(solid, face);
            for (const Rect2D& rect: visible_rects)
            {
                append_face_rect(mesh, face, plane, rect, color);
            }
        }
    } // namespace

    auto build_blockout_mesh(const BlockoutLevel& level) -> CpuColorMeshData
    {
        CpuColorMeshData mesh{};
        mesh.vertices.reserve(level.solids.size() * 24);
        mesh.indices.reserve(level.solids.size() * 36);

        for (std::size_t solid_index = 0; solid_index < level.solids.size(); ++solid_index)
        {
            const BlockoutSolid& solid = level.solids[solid_index];
            if (solid.mins.x >= solid.maxs.x || solid.mins.y >= solid.maxs.y || solid.mins.z >= solid.maxs.z)
            {
                throw std::runtime_error("Blockout solid has invalid bounds");
            }

            const glm::vec3 base_color = material_base_color(solid.material);

            append_visible_face(
                mesh,
                solid,
                level.solids,
                solid_index,
                FaceAxis::PosZ,
                shade(base_color, face_shade_factor(FaceAxis::PosZ))
            );
            append_visible_face(
                mesh,
                solid,
                level.solids,
                solid_index,
                FaceAxis::NegZ,
                shade(base_color, face_shade_factor(FaceAxis::NegZ))
            );
            append_visible_face(
                mesh,
                solid,
                level.solids,
                solid_index,
                FaceAxis::NegX,
                shade(base_color, face_shade_factor(FaceAxis::NegX))
            );
            append_visible_face(
                mesh,
                solid,
                level.solids,
                solid_index,
                FaceAxis::PosX,
                shade(base_color, face_shade_factor(FaceAxis::PosX))
            );
            append_visible_face(
                mesh,
                solid,
                level.solids,
                solid_index,
                FaceAxis::PosY,
                shade(base_color, face_shade_factor(FaceAxis::PosY))
            );
            append_visible_face(
                mesh,
                solid,
                level.solids,
                solid_index,
                FaceAxis::NegY,
                shade(base_color, face_shade_factor(FaceAxis::NegY))
            );
        }

        return mesh;
    }
} // namespace RelayCore::render
