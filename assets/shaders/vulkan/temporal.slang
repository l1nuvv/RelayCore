[[vk::binding(1, 1)]] Sampler2D currentNormalDepth;
[[vk::binding(2, 1)]] Sampler2D currentVelocity;
[[vk::binding(3, 1)]] Sampler2D previousNormalDepth;
[[vk::binding(5, 1)]] Sampler2D previousVelocity;
[[vk::binding(8, 1)]] Sampler2D currentColor;
[[vk::binding(9, 1)]] Sampler2D resolvedHistory;

struct DrawData
{
    column_major float4x4 mvp;
    float4 model0, model1, model2;
    uint flags;
    float cameraX, cameraY, cameraZ;
};
[[vk::push_constant]] ConstantBuffer<DrawData> draw;

[shader("vertex")]
float4 temporal_vertex(uint id : SV_VulkanVertexID) : SV_Position
{
    return float4(float2((id << 1) & 2, id & 2) * 2 - 1, 0, 1);
}

float3 history_color(float2 uv, int2 size)
{
    float2 p = uv * float2(size) - .5;
    int2 base = int2(floor(p));
    float2 f = frac(p);
    float3 a = resolvedHistory.Load(int3(clamp(base, 0, size - 1), 0)).rgb;
    float3 b = resolvedHistory.Load(int3(clamp(base + int2(1, 0), 0, size - 1), 0)).rgb;
    float3 c = resolvedHistory.Load(int3(clamp(base + int2(0, 1), 0, size - 1), 0)).rgb;
    float3 d = resolvedHistory.Load(int3(clamp(base + 1, 0, size - 1), 0)).rgb;
    return lerp(lerp(a, b, f.x), lerp(c, d, f.x), f.y);
}

[shader("fragment")]
float4 temporal_fragment(float4 position : SV_Position) : SV_Target
{
    uint width, height;
    currentColor.GetDimensions(width, height);
    int2 size = int2(width, height), pixel = int2(position.xy);
    float4 source = currentColor.Load(int3(pixel, 0));
    float4 nd = currentNormalDepth.Load(int3(pixel, 0));
    float4 velocity = currentVelocity.Load(int3(pixel, 0));
    if (draw.cameraX == 0 || nd.w <= 0 || velocity.w <= 0) return source;
    float2 uv = position.xy / float2(size) + velocity.xy + float2(draw.cameraY, draw.cameraZ);
    // Reject the entire bilinear footprint: a valid center alone can sample a
    // moving weapon or disoccluded neighbor into stationary world history.
    float2 oldPosition = uv * float2(size) - .5;
    int2 base = int2(floor(oldPosition));
    if (any(base < 0) || any(base + 1 >= size)) return source;
    for (int y = 0; y < 2; ++y)
        for (int x = 0; x < 2; ++x)
        {
            int2 p = base + int2(x, y);
            float4 old = previousNormalDepth.Load(int3(p, 0));
            if (previousVelocity.Load(int3(p, 0)).w <= 0 || old.w <= 0
                || abs(old.w - velocity.z) > max(.02, velocity.z * .01)
                || dot(old.xyz, nd.xyz) < .9) return source;
        }
    float3 low = source.rgb, high = source.rgb;
    for (int y = -1; y <= 1; ++y)
        for (int x = -1; x <= 1; ++x)
        {
            int2 p = clamp(pixel + int2(x, y), 0, size - 1);
            if (currentVelocity.Load(int3(p, 0)).w <= 0) return source;
            float3 color = currentColor.Load(int3(p, 0)).rgb;
            low = min(low, color);
            high = max(high, color);
        }
    float3 history = clamp(history_color(uv, size), low, high);
    // Limit persistence during motion; abrupt lighting/emission changes are
    // bounded by the current neighborhood instead of retaining bright trails.
    float motion = length(velocity.xy * float2(size));
    float weight = lerp(.9, .5, saturate(motion / 8));
    return float4(lerp(source.rgb, history, weight), source.a);
}
