#include "relaycore/world/world_collision.h"

#include <array>
#include <fstream>
#include <iostream>

#include "physics_fixture_data.h"

int main(int argc, char** argv)
{
    if (argc > 2)
    {
        std::cerr << "Usage: relaycore_physics_fixture [diagnostics.svg]\n";
        return 2;
    }
    try
    {
        using namespace RelayCore;
        using namespace RelayCore::physics;
        const auto triangles = tool::physics_fixture_doorway();
        const std::array meshes{PhysicsMesh{triangles, {WorldObjectKind::CollisionMesh, 0}}};
        const auto boxes = build_static_world_colliders(tool::physics_fixture_level());
        const PhysicsWorld physics{boxes, meshes};
        std::ofstream svg;
        if (argc == 2)
        {
            svg.open(argv[1]);
            if (!svg)
                throw std::runtime_error("Cannot open diagnostic SVG");
            svg << "<svg xmlns='http://www.w3.org/2000/svg' width='900' height='390' viewBox='0 0 900 390'>"
                << "<rect width='900' height='390' fill='#111827'/>"
                << "<g font-family='sans-serif' fill='#e5e7eb' font-size='15'>"
                << "<text x='45' y='28'>Jolt static fixture: front (X/Y)</text>"
                << "<text x='500' y='28'>Side (Z/Y): hit point and outward normal</text>"
                << "<text x='45' y='365'>Metres, +Y up; CCW panels at Z=3. Doorway width 1.2, height 2.</text></g>";
            for (const auto& triangle : triangles)
            {
                const auto* colour = triangle.surface == SurfaceMaterial::Metal  ? "#64748b"
                                     : triangle.surface == SurfaceMaterial::Tile ? "#b8ad82"
                                                                                 : "#78746d";
                svg << "<polygon fill='" << colour << "' stroke='#cbd5e1' points='";
                for (const auto point : {triangle.a, triangle.b, triangle.c})
                    svg << 210 + point.x * 45 << ',' << 300 - point.y * 70 << ' ';
                svg << "'/>";
            }
            svg << "<path d='M700 90V300 M560 300H840' stroke='#cbd5e1' fill='none'/>";
        }
        for (const auto origin : std::array<glm::vec3, 4>{{{0, 1, 0}, {-1, 1, 0}, {1, 1, 0}, {0, 2.5F, 0}}})
        {
            const auto hit = WorldQuery{physics}.raycast(origin, {0, 0, 1}, 6);
            std::cout << "ray origin=" << origin.x << ',' << origin.y << ',' << origin.z;
            if (!hit)
                std::cout << " clear\n";
            else
                std::cout << " distance=" << hit->distance << " point=" << hit->position.x << ',' << hit->position.y
                          << ',' << hit->position.z << " normal=" << hit->normal.x << ',' << hit->normal.y << ','
                          << hit->normal.z << " surface=" << static_cast<int>(hit->surface)
                          << " authored=" << hit->object.authored_entity_index << '\n';
            if (svg.is_open())
            {
                const auto end = hit ? hit->position : origin + glm::vec3{0, 0, 6};
                const auto* colour = hit ? "#fb923c" : "#4ade80";
                svg << "<path stroke='" << colour << "' stroke-width='2' d='M" << 580 + origin.z * 40 << ' '
                    << 300 - origin.y * 70 << " L" << 580 + end.z * 40 << ' ' << 300 - end.y * 70 << "'/>";
                if (hit)
                    svg << "<circle fill='#fb923c' r='5' cx='" << 210 + end.x * 45 << "' cy='" << 300 - end.y * 70
                        << "'/><path stroke='#38bdf8' stroke-width='4' d='M" << 580 + end.z * 40 << ' '
                        << 300 - end.y * 70 << " l" << hit->normal.z * 30 << ' ' << -hit->normal.y * 30 << "'/>";
            }
        }
        const auto cast = physics.cast_box({1, 1, 0}, {.3F, .8F, .3F}, {0, 0, 6});
        if (!cast)
            throw std::runtime_error("Fixture wall cast unexpectedly clear");
        std::cout << "box cast distance=" << cast->distance << " fraction=" << cast->fraction
                  << " normal=" << cast->normal.x << ',' << cast->normal.y << ',' << cast->normal.z << '\n';
        if (svg.is_open())
        {
            svg << "</svg>\n";
            svg.close();
            if (!svg)
                throw std::runtime_error("Cannot write diagnostic SVG");
        }
        return 0;
    } catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
