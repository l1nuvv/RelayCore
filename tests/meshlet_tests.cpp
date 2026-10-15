#include "relaycore/render/mesh.h"

#include <doctest/doctest.h>
#include <limits>

using namespace RelayCore::render;

TEST_CASE("meshlets preserve ordered triangles across vertex and primitive limits")
{
    auto mesh = make_color_box_mesh_data();
    const auto vertices = mesh.vertices;
    const auto indices = mesh.indices;
    for (uint32_t copy = 1; copy < 20; ++copy)
    {
        for (auto vertex : vertices)
        {
            vertex.position.x += float(copy) * 2;
            mesh.vertices.push_back(vertex);
        }
        for (auto index : indices)
            mesh.indices.push_back(index + copy * uint32_t(vertices.size()));
    }
    auto packed = build_meshlets(mesh);
    REQUIRE(packed.clusters.size() > 1);
    CHECK_NOTHROW(validate_meshlets(mesh, packed));
    const auto repeat = build_meshlets(mesh);
    CHECK(repeat.vertices == packed.vertices);
    CHECK(repeat.triangles == packed.triangles);
    REQUIRE(repeat.clusters.size() == packed.clusters.size());
    for (size_t i = 0; i < packed.clusters.size(); ++i)
    {
        CHECK(repeat.clusters[i].range == packed.clusters[i].range);
        CHECK(repeat.clusters[i].mins == packed.clusters[i].mins);
        CHECK(repeat.clusters[i].maxs == packed.clusters[i].maxs);
    }
    mesh.indices.clear();
    for (int i = 0; i < 250; ++i)
        mesh.indices.insert(mesh.indices.end(), {0, 1, 2});
    packed = build_meshlets(mesh);
    REQUIRE(packed.clusters.size() == 3);
    CHECK(packed.clusters[0].range.w == kMeshletTriangles);
    CHECK(packed.clusters[2].range.w == 2);
    CHECK_NOTHROW(validate_meshlets(mesh, packed));
}

TEST_CASE("meshlet validation rejects false bounds ranges and remapped topology")
{
    auto mesh = make_color_box_mesh_data();
    const auto packed = build_meshlets(mesh);
    auto bad = packed;
    bad.clusters[0].mins.x += 1;
    CHECK_THROWS(validate_meshlets(mesh, bad));
    bad = packed;
    bad.clusters[0].range.x = UINT32_MAX;
    CHECK_THROWS(validate_meshlets(mesh, bad));
    bad = packed;
    bad.clusters[0].range.z = kMeshletVertices + 1;
    CHECK_THROWS(validate_meshlets(mesh, bad));
    bad = packed;
    bad.triangles[0] = UINT32_MAX;
    CHECK_THROWS(validate_meshlets(mesh, bad));
    bad = packed;
    bad.vertices[0] = uint32_t(mesh.vertices.size());
    CHECK_THROWS(validate_meshlets(mesh, bad));
    bad = packed;
    bad.vertices[0] = bad.vertices[1];
    CHECK_THROWS(validate_meshlets(mesh, bad));
    bad = packed;
    bad.triangles.push_back(0);
    CHECK_THROWS(validate_meshlets(mesh, bad));
    mesh.indices[0] = UINT32_MAX;
    CHECK_THROWS(static_cast<void>(build_meshlets(mesh)));
    mesh.indices[0] = 0;
    mesh.vertices[0].position.x = std::numeric_limits<float>::quiet_NaN();
    CHECK_THROWS(static_cast<void>(build_meshlets(mesh)));
}
