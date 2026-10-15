[[vk::binding(0, 0)]] Sampler2D normalDepth;
[[vk::binding(1, 0)]] RWStructuredBuffer<float2> bounds;
[[vk::binding(2, 0)]] [[vk::image_format("r32f")]] RWTexture2D<float> traceDepth;
struct Reduction { uint4 source; uint4 target; };
[[vk::push_constant]] ConstantBuffer<Reduction> reduction;

[shader("compute")]
[numthreads(8, 8, 1)]
void compute_main(uint3 id : SV_DispatchThreadID)
{
    if (any(id.xy >= reduction.target.yz)) return;
    uint block = reduction.source.w == 0 ? 8 : 2;
    float2 result = float2(3.402823466e+38, 0);
    bool complete = true;
    for (uint y = 0; y < block; ++y)
        for (uint x = 0; x < block; ++x)
        {
            uint2 p = id.xy * block + uint2(x, y);
            if (any(p >= reduction.source.yz)) continue;
            float2 value;
            if (reduction.source.w == 0)
            {
                float depth = normalDepth.Load(int3(p, 0)).w;
                traceDepth[p] = depth;
                value = float2(depth > 0 ? depth : 3.402823466e+38, max(depth, 0));
            }
            else value = bounds[reduction.source.x + p.y * reduction.source.y + p.x];
            complete = complete && value.y > 0;
            result = float2(min(result.x, value.x), max(result.y, abs(value.y)));
        }
    // The sign retains mixed/empty coverage without another buffer channel.
    result.y *= complete ? 1 : -1;
    bounds[reduction.target.x + id.y * reduction.target.y + id.x] = result;
}
