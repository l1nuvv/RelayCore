#include "mesh.slang"

struct PreviousDraw { column_major float4x4 mvp; float4 valid; };
[[vk::binding(0, 1)]] StructuredBuffer<PreviousDraw> previousDraws;
[[vk::binding(1, 1)]] Sampler2D currentNormalDepth;
[[vk::binding(2, 1)]] Sampler2D currentVelocity;
[[vk::binding(3, 1)]] Sampler2D previousNormalDepth;
[[vk::binding(4, 1)]] Sampler2D previousColor;
[[vk::binding(5, 1)]] Sampler2D previousVelocity;

struct InputVarying
{
    float4 position : SV_Position;
    float3 worldNormal;
    float4 worldTangent;
    float2 uv;
    float4 currentClip;
    float4 previousClip;
    nointerpolation float valid;
};

[shader("vertex")]
InputVarying input_vertex(Vertex input, uint instance : SV_VulkanInstanceID)
{
    InputVarying output;
    Varying surface = vertex_main(input);
    output.position = surface.position;
    output.worldNormal = surface.worldNormal;
    output.worldTangent = surface.worldTangent;
    output.uv = surface.uv;
    output.currentClip = mul(draw.mvp, float4(input.position, 1));
    output.previousClip = mul(previousDraws[instance].mvp, float4(input.position, 1));
    output.valid = previousDraws[instance].valid.x;
    return output;
}

struct InputOutput { float4 normalDepth : SV_Target0; float4 velocity : SV_Target1; };
[shader("fragment")]
InputOutput input_fragment(InputVarying input)
{
    InputOutput output;
    Varying surface;
    surface.worldNormal = input.worldNormal;
    surface.worldTangent = input.worldTangent;
    surface.uv = input.uv;
    output.normalDepth = float4(surface_normal(surface), input.currentClip.w);
    float2 currentUV = input.currentClip.xy / input.currentClip.w * float2(.5, -.5) + .5;
    float2 previousUV = input.previousClip.xy / max(input.previousClip.w, .0001) * float2(.5, -.5) + .5;
    output.velocity = float4(previousUV - currentUV, input.previousClip.w,
                             input.valid * float(input.previousClip.w > 0));
    // Transparent/overlay/viewmodel pixels cannot reuse opaque history.
    if ((draw.flags & (2 | 4 | 32)) != 0)
    {
        output.normalDepth = 0;
        output.velocity.w = 0;
    }
    if (output.velocity.w == 0) output.velocity = float4(0, 0, input.currentClip.w, 0);
    return output;
}

[shader("vertex")]
float4 debug_vertex(uint id : SV_VulkanVertexID) : SV_Position
{
    return float4(float2((id << 1) & 2, id & 2) * 2 - 1, 0, 1);
}

[shader("fragment")]
float4 debug_fragment(float4 position : SV_Position) : SV_Target
{
    uint width, height;
    currentNormalDepth.GetDimensions(width, height);
    int2 pixel = int2(position.xy);
    float4 nd = currentNormalDepth.Load(int3(pixel, 0));
    float4 velocity = currentVelocity.Load(int3(pixel, 0));
    if (draw.flags == 2) return float4((nd.w / (1 + nd.w)).xxx, 1);
    if (draw.flags == 3) return float4(nd.w > 0 ? nd.xyz * .5 + .5 : float3(0), 1);
    if (draw.flags == 4) return float4(.5 + velocity.xy * 8, velocity.w, 1);
    float2 uv = position.xy / float2(width, height) + velocity.xy;
    bool valid = draw.cameraX != 0 && nd.w > 0 && velocity.w > 0 && all(uv >= 0) && all(uv < 1);
    float4 old = previousNormalDepth.Load(int3(int2(clamp(uv, 0, .999999) * float2(width, height)), 0));
    float oldEligibility = previousVelocity.Load(int3(int2(clamp(uv, 0, .999999) * float2(width, height)), 0)).w;
    valid = valid && oldEligibility > 0 && old.w > 0 && abs(old.w - velocity.z) <= max(.02, velocity.z * .01)
                  && dot(old.xyz, nd.xyz) >= .9;
    // Exercise the color-history read only after geometry rejection. UI never enters this texture.
    float3 history = valid ? previousColor.Load(int3(int2(uv * float2(width, height)), 0)).rgb : float3(0);
    if (draw.flags == 6) return float4(history, 1);
    return float4(valid ? float3(0, 1, 0) : float3(1, 0, 0), 1);
}
