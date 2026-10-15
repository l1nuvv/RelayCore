#include "mesh.slang"

struct Meshlet { uint4 range; float4 mins, maxs; };
[[vk::binding(0, 4)]] ByteAddressBuffer meshVertices;
[[vk::binding(1, 4)]] StructuredBuffer<Meshlet> meshlets;
[[vk::binding(2, 4)]] StructuredBuffer<uint> meshletVertices;
[[vk::binding(3, 4)]] StructuredBuffer<uint> meshletTriangles;

Vertex load_mesh_vertex(uint index)
{
    uint offset = index * 64;
    Vertex v;
    v.position = asfloat(meshVertices.Load3(offset));
    v.color = asfloat(meshVertices.Load3(offset + 12));
    v.normal = asfloat(meshVertices.Load3(offset + 24));
    v.material = asfloat(meshVertices.Load(offset + 36));
    v.uv = asfloat(meshVertices.Load2(offset + 40));
    v.tangent = asfloat(meshVertices.Load4(offset + 48));
    return v;
}

bool meshlet_visible(Meshlet m, SceneInstance transform)
{
    uint outside = 63;
    for (uint corner = 0; corner < 8; ++corner) {
        float3 p = float3((corner & 1) != 0 ? m.maxs.x : m.mins.x,
                         (corner & 2) != 0 ? m.maxs.y : m.mins.y,
                         (corner & 4) != 0 ? m.maxs.z : m.mins.z);
        float4 clip = mul(transform.mvp, float4(p, 1));
        float tolerance = 1e-5 * (abs(clip.w) + 1);
        uint planes = (clip.x < -clip.w - tolerance ? 1 : 0) | (clip.x > clip.w + tolerance ? 2 : 0)
                    | (clip.y < -clip.w - tolerance ? 4 : 0) | (clip.y > clip.w + tolerance ? 8 : 0)
                    | (clip.z < -clip.w - tolerance ? 16 : 0) | (clip.z > clip.w + tolerance ? 32 : 0);
        outside &= planes;
    }
    return outside == 0;
}

groupshared uint visible;
[shader("mesh")]
[numthreads(32, 1, 1)]
[outputtopology("triangle")]
void meshlet_main(uint thread : SV_GroupIndex, uint3 group : SV_GroupID,
    out vertices Varying vertices[64], out indices uint3 triangles[124])
{
    Meshlet m = meshlets[group.x + asuint(draw.modelRow0.y)];
    uint instance = group.y;
    SceneInstance transform = scene_transform(instance);
    if (thread == 0) visible = meshlet_visible(m, transform) ? 1 : 0;
    GroupMemoryBarrierWithGroupSync();
    SetMeshOutputCounts(visible != 0 ? m.range.z : 0, visible != 0 ? m.range.w : 0);
    if (visible == 0) return;
    for (uint i = thread; i < m.range.z; i += 32)
        vertices[i] = vertex_main(load_mesh_vertex(meshletVertices[m.range.x + i]), instance);
    for (uint i = thread; i < m.range.w; i += 32) {
        uint packed = meshletTriangles[m.range.y + i];
        triangles[i] = uint3(packed & 255, (packed >> 8) & 255, (packed >> 16) & 255);
    }
}
