[[vk::binding(0, 0)]] Sampler2D normalDepth;
[[vk::binding(1, 0)]] RWStructuredBuffer<float2> bounds;
[[vk::binding(2, 0)]] [[vk::image_format("r32f")]] RWTexture2D<float> traceDepth;
struct Reduction { uint4 source; uint4 target; };
[[vk::push_constant]] ConstantBuffer<Reduction> reduction;
groupshared float3 tileBounds[64];

[shader("compute")]
[numthreads(8, 8, 1)]
void compute_main(uint3 id : SV_DispatchThreadID, uint3 group : SV_GroupID, uint lane : SV_GroupIndex)
{
    if (reduction.source.w == 0)
    {
        // One lane per source pixel keeps the scalar-depth copy coalesced.
        float3 value = float3(3.402823466e+38, 0, 1);
        if (all(id.xy < reduction.source.yz))
        {
            float depth = normalDepth.Load(int3(id.xy, 0)).w;
            traceDepth[id.xy] = depth;
            value = float3(depth > 0 ? depth : 3.402823466e+38, max(depth, 0), depth > 0 ? 1 : 0);
        }
        tileBounds[lane] = value;
        GroupMemoryBarrierWithGroupSync();
        for (uint stride = 32; stride != 0; stride >>= 1)
        {
            if (lane < stride)
            {
                float3 a = tileBounds[lane], b = tileBounds[lane + stride];
                tileBounds[lane] = float3(min(a.x, b.x), max(a.y, b.y), min(a.z, b.z));
            }
            GroupMemoryBarrierWithGroupSync();
        }
        if (lane == 0)
        {
            float3 result = tileBounds[0];
            bounds[reduction.target.x + group.y * reduction.target.y + group.x] =
                float2(result.x, result.y * (result.z != 0 ? 1 : -1));
        }
        return;
    }
    if (any(id.xy >= reduction.target.yz)) return;
    uint block = 2;
    float2 result = float2(3.402823466e+38, 0);
    bool complete = true;
    for (uint y = 0; y < block; ++y)
        for (uint x = 0; x < block; ++x)
        {
            uint2 p = id.xy * block + uint2(x, y);
            if (any(p >= reduction.source.yz)) continue;
            float2 value = bounds[reduction.source.x + p.y * reduction.source.y + p.x];
            complete = complete && value.y > 0;
            result = float2(min(result.x, value.x), max(result.y, abs(value.y)));
        }
    // The sign retains mixed/empty coverage without another buffer channel.
    result.y *= complete ? 1 : -1;
    bounds[reduction.target.x + id.y * reduction.target.y + id.x] = result;
}
