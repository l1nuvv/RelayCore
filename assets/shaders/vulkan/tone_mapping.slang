[[vk::binding(0, 0)]] Sampler2D hdrScene;
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

[shader("fragment")]
float4 fragment_main(float4 position : SV_Position) : SV_Target
{
    float4 source = hdrScene.Sample(position.xy / tone.viewportTimeExposure.xy);
    float3 exposed = max(source.rgb, 0) * exp2(tone.viewportTimeExposure.w);
    // Reinhard preserves values above 1 until this explicit display mapping.
    float3 mapped = 1 - 1 / (1 + exposed);
    // Existing contrast/grain controls operate on display brightness, after tone mapping.
    float3 display = select(mapped <= 0.0031308, mapped * 12.92, 1.055 * pow(mapped, 1.0 / 2.4) - 0.055);
    float2 fragCoord = float2(position.x, tone.viewportTimeExposure.y - position.y);
    float3 graded = scene_postfx(display, fragCoord, tone.viewportTimeExposure, tone.postfx, tone.grading.x, false);
    float3 color = select(graded <= 0.04045, graded / 12.92, pow((graded + 0.055) / 1.055, 2.4));
    return float4(color, source.a);
}
