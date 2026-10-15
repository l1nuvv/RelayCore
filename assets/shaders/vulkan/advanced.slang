#define SCREEN_EFFECTS 1
#include "mesh.slang"

[[vk::binding(3, 1)]] Sampler2D giPreviousGeometry;
[[vk::binding(5, 1)]] Sampler2D giPreviousVelocity;
[[vk::binding(11, 1)]] Sampler2D giHistory;
[[vk::binding(12, 1)]] Sampler2D lensSource;

[shader("vertex")]
float4 advanced_vertex(uint id : SV_VulkanVertexID) : SV_Position
{
    return float4(float2((id << 1) & 2, id & 2) * 2 - 1, 0, 1);
}

[shader("fragment")]
float4 gi_fragment(float4 pixel : SV_Position) : SV_Target
{
    int2 size = int2(frame.viewportTimeDebug.xy);
    int2 giSize = int2(float2(draw.cameraY, draw.cameraZ));
    float2 uv = pixel.xy / float2(giSize);
    int2 fullPixel = clamp(int2(uv * size), 0, size - 1);
    float4 nd = effectNormalDepth.Load(int3(fullPixel, 0));
    if (nd.w <= 0) return 0;
    float3 position = effect_world((float2(fullPixel) + .5) / float2(size), nd.w);
    float3 tangent = normalize(cross(nd.xyz, abs(nd.y) < .9 ? float3(0, 1, 0) : float3(1, 0, 0)));
    float3 bitangent = cross(nd.xyz, tangent);
    int samples = frame.lighting.x > 1 ? 6 : 4;
    float3 radiance = 0;
    float coverage = 0;
    float rotation = frac(sin(dot(floor(pixel.xy), float2(12.9898,78.233))) * 43758.5453) * 6.283185;
    // Cosine-weighted deterministic hemisphere: misses preserve the authored
    // diffuse instead of injecting black or recursively feeding prior GI.
    for (int ray = 0; ray < samples; ++ray)
    {
        float r = sqrt((ray + .5) / samples), angle = ray * 2.399963 + rotation;
        float3 direction = tangent * (r * cos(angle)) + bitangent * (r * sin(angle)) + nd.xyz * sqrt(1 - r*r);
        float4 hit = trace_screen(position, nd.xyz, direction, .08, frame.lighting.x > 1 ? 32 : 20, 3, true);
        radiance += min(hit.rgb, float3(8)) * hit.a;
        coverage += hit.a;
    }
    float4 current = float4(coverage > 0 ? radiance / coverage : float3(0), coverage / samples);
    float4 velocity = effectVelocity.Load(int3(fullPixel, 0));
    float2 oldUV = uv + velocity.xy + draw.modelRow0.xy;
    float2 oldPosition = oldUV * float2(giSize) - .5;
    int2 giBase = int2(floor(oldPosition));
    if (draw.cameraX == 0 || velocity.w <= 0 || current.a <= 0
        || any(giBase < 0) || any(giBase + 1 >= giSize)) return current;
    for (int y = 0; y < 2; ++y)
        for (int x = 0; x < 2; ++x)
        {
            int2 p = clamp(int2((float2(giBase + int2(x,y)) + .5) * float2(size) / float2(giSize)), 0, size - 1);
            float4 old = giPreviousGeometry.Load(int3(p,0));
            if (giPreviousVelocity.Load(int3(p,0)).w <= 0 || old.w <= 0
                || abs(old.w - velocity.z) > max(.02, velocity.z * .01) || dot(nd.xyz,old.xyz) < .9) return current;
        }
    // Clip radiance and coverage together; newly invisible emitters cannot leave
    // a bright trail or keep replacing probes after screen coverage disappears.
    float2 f = frac(oldPosition);
    float4 history = lerp(lerp(giHistory.Load(int3(giBase,0)), giHistory.Load(int3(giBase+int2(1,0),0)), f.x),
                          lerp(giHistory.Load(int3(giBase+int2(0,1),0)), giHistory.Load(int3(giBase+1,0)), f.x), f.y);
    float3 bound = max(current.rgb * .2, float3(.02));
    history.rgb = clamp(history.rgb, max(current.rgb - bound, 0), current.rgb + bound);
    history.a = clamp(history.a, max(current.a - .05, 0), current.a + .05);
    return lerp(current, history, lerp(.8, .3, saturate(length(velocity.xy * size) / 8)));
}

[shader("fragment")]
float4 reconstruct_fragment(float4 pixel : SV_Position) : SV_Target
{
    float4 nd = effectNormalDepth.Load(int3(int2(pixel.xy), 0));
    if (nd.w <= 0) return float4(0,0,0,-1);
    return reconstruct_diffuse(pixel.xy / frame.viewportTimeDebug.xy, nd.xyz, nd);
}

[shader("fragment")]
float4 lens_fragment(float4 pixel : SV_Position) : SV_Target
{
    int2 size = int2(frame.viewportTimeDebug.xy), p = int2(pixel.xy);
    float4 source = lensSource.Load(int3(p, 0));
    float4 nd = effectNormalDepth.Load(int3(p, 0)), velocity = effectVelocity.Load(int3(p, 0));
    if (nd.w <= 0 || velocity.w <= 0 || draw.cameraX == 0) return source;
    float radius = frame.lighting.y == 0 ? 0 : saturate(abs(nd.w - frame.lighting.w) / max(frame.lighting.w, .25))
                   * (frame.lighting.y > 1 ? 6 : 3);
    float2 motion = frame.lighting.z == 0 ? float2(0) : velocity.xy * float2(size) * .5;
    float cap = frame.lighting.z > 1 ? 12 : 6;
    motion *= min(1, cap / max(length(motion), .0001));
    if (radius < .5 && length(motion) < .5) return source;
    int samples = max(frame.lighting.y, frame.lighting.z) > 1 ? 8 : 4;
    float3 sum = source.rgb;
    float weight = 1;
    for (int tap = 0; tap < samples; ++tap)
    {
        float t = (tap + .5) / samples, angle = tap * 2.399963;
        float2 offset = float2(cos(angle), sin(angle)) * sqrt(t) * radius + motion * (t - .5);
        int2 q = int2(round(float2(p) + offset));
        if (any(q < 0) || any(q >= size)) continue;
        float4 other = effectNormalDepth.Load(int3(q, 0));
        float4 v = effectVelocity.Load(int3(q, 0));
        if (v.w <= 0 || other.w <= 0 || abs(nd.w-other.w) > max(.1, nd.w * .02)
            || dot(nd.xyz, other.xyz) < .9 || length((v.xy-velocity.xy)*size) > 2) continue;
        sum += lensSource.Load(int3(q, 0)).rgb;
        weight += 1;
    }
    return float4(sum / weight, source.a);
}
