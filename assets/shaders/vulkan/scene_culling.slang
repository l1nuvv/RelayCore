#define SCENE_CULLING_SHADER 1
#include "scene_instances.slang"

// Host layout: CullBatch, 48 bytes; range = first record, count, index count, cull enabled.
struct CullBatch { uint4 range; float4 mins, maxs; };
struct IndexedCommand
{
    uint indexCount, instanceCount, firstIndex;
    int vertexOffset;
    uint firstInstance;
};
[[vk::binding(0, 0)]] StructuredBuffer<SceneInstance> sourceInstances;
[[vk::binding(1, 0)]] RWStructuredBuffer<SceneInstance> visibleInstances;
[[vk::binding(2, 0)]] StructuredBuffer<CullBatch> batches;
[[vk::binding(3, 0)]] RWStructuredBuffer<IndexedCommand> commands;
struct CullPush { uint firstBatch; };
[[vk::push_constant]] ConstantBuffer<CullPush> cull;
groupshared uint prefix[64];
groupshared uint kept;

bool intersects_frustum(SceneInstance instance, CullBatch batch)
{
    uint outside = 63;
    for (uint corner = 0; corner < 8; ++corner)
    {
        float3 position = float3((corner & 1) != 0 ? batch.maxs.x : batch.mins.x,
                                 (corner & 2) != 0 ? batch.maxs.y : batch.mins.y,
                                 (corner & 4) != 0 ? batch.maxs.z : batch.mins.z);
        float4 clip = mul(instance.mvp, float4(position, 1));
        for (uint plane = 0; plane < 6; ++plane)
        {
            float component = clip[plane / 2];
            float tolerance = 0.0001 * (1 + abs(clip.w) + abs(component));
            // The CPU projection uses -w..w depth; mesh/shadow vertices convert it to Vulkan afterwards.
            if (clip.w + ((plane & 1) != 0 ? -component : component) >= -tolerance)
                outside &= ~(1u << plane);
        }
    }
    return outside == 0;
}

[shader("compute")]
[numthreads(64, 1, 1)]
void compute_main(uint3 group : SV_GroupID, uint lane : SV_GroupIndex)
{
    uint command = cull.firstBatch + group.x;
    CullBatch batch = batches[command];
    if (lane == 0) kept = 0;
    GroupMemoryBarrierWithGroupSync();
    // One workgroup owns a batch's reserved span. Block scans preserve authored order without atomics.
    for (uint block = 0; block < batch.range.y; block += 64)
    {
        uint offset = block + lane;
        bool present = offset < batch.range.y;
        SceneInstance instance;
        bool visible = false;
        if (present)
        {
            instance = sourceInstances[batch.range.x + offset];
            visible = batch.range.w == 0 || intersects_frustum(instance, batch);
        }
        prefix[lane] = visible ? 1 : 0;
        GroupMemoryBarrierWithGroupSync();
        for (uint step = 1; step < 64; step *= 2)
        {
            uint preceding = lane >= step ? prefix[lane - step] : 0;
            GroupMemoryBarrierWithGroupSync();
            prefix[lane] += preceding;
            GroupMemoryBarrierWithGroupSync();
        }
        if (visible) visibleInstances[batch.range.x + kept + prefix[lane] - 1] = instance;
        GroupMemoryBarrierWithGroupSync();
        if (lane == 0) kept += prefix[63];
        GroupMemoryBarrierWithGroupSync();
    }
    if (lane == 0)
    {
        IndexedCommand output;
        output.indexCount = batch.range.z;
        output.instanceCount = kept;
        output.firstIndex = 0;
        output.vertexOffset = 0;
        output.firstInstance = 0;
        commands[command] = output;
    }
}
