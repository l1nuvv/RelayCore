[[vk::binding(0, 0)]] Sampler2D hdrScene;
[[vk::binding(1, 0)]] [[vk::image_format("rgba8")]] RWTexture2D<float4> displayTarget;
[[vk::binding(2, 0)]] Sampler2D bloom0;
[[vk::binding(3, 0)]] Sampler2D bloom1;
[[vk::binding(4, 0)]] Sampler2D bloom2;
[[vk::binding(5, 0)]] Sampler2D bloom3;
struct ToneMappingData
{
    float4 postfx;
    float4 viewportTimeExposure;
    float4 grading;
};
[[vk::push_constant]] ConstantBuffer<ToneMappingData> tone;
#include "scene_postfx.slang"

[shader("vertex")]
float4 vertex_main(uint id : SV_VulkanVertexID) : SV_Position
{
    float2 position = float2((id << 1) & 2, id & 2) * 2 - 1;
    return float4(position, 0, 1);
}

float4 tone_mapped_display(float2 position)
{
    float4 source = hdrScene.SampleLevel(position / tone.viewportTimeExposure.xy, 0);
    if (tone.grading.y > 0)
    {
        float2 uv = position / tone.viewportTimeExposure.xy;
        source.rgb += tone.grading.y * (bloom0.SampleLevel(uv, 0).rgb * .4
                     + bloom1.SampleLevel(uv, 0).rgb * .3 + bloom2.SampleLevel(uv, 0).rgb * .2
                     + bloom3.SampleLevel(uv, 0).rgb * .1);
    }
    float3 exposed = max(source.rgb, 0) * exp2(tone.viewportTimeExposure.w);
    float3 mapped = sdr_highlight_rolloff(exposed);
    // Existing contrast/grain controls operate on display brightness, after tone mapping.
    float3 display = linear_to_srgb(mapped);
    float2 fragCoord = float2(position.x, tone.viewportTimeExposure.y - position.y);
    float3 graded = scene_postfx(display, fragCoord, tone.viewportTimeExposure, tone.postfx, tone.grading.x, false);
    return float4(graded, source.a);
}

[shader("fragment")]
float4 bloom_fragment(float4 position : SV_Position) : SV_Target
{
    // Bright extraction precedes reduction, so small emissive surfaces survive averaging.
    float2 uv = position.xy / tone.viewportTimeExposure.xy;
    uint width, height;
    hdrScene.GetDimensions(width, height);
    float3 result = 0;
    for (int y = -1; y <= 1; y += 2)
        for (int x = -1; x <= 1; x += 2)
        {
            float3 color = max(hdrScene.SampleLevel(uv + float2(x, y) * .5 / float2(width, height), 0).rgb, 0);
            float peak = max(color.r, max(color.g, color.b));
            float knee = max(tone.grading.z * .5, .0001);
            float soft = clamp(peak - tone.grading.z + knee, 0, 2 * knee);
            soft = soft * soft / (4 * knee);
            result += color * max(peak - tone.grading.z, soft) / max(peak, .0001);
        }
    return float4(result * .25, 1);
}

[shader("fragment")]
float4 fragment_main(float4 position : SV_Position) : SV_Target
{
    float4 display = tone_mapped_display(position.xy);
    return float4(srgb_to_linear(display.rgb), display.a);
}

[shader("compute")]
[numthreads(8, 8, 1)]
void compute_main(uint3 id : SV_DispatchThreadID)
{
    if (any(id.xy >= uint2(tone.viewportTimeExposure.xy)))
        return;
    // Storage writes use the UNORM view: display encoding is already applied above.
    displayTarget[id.xy] = tone_mapped_display(float2(id.xy) + 0.5);
}
