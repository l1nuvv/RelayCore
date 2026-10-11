#ifndef POINT_SHADOW
#define POINT_SHADOW 0
#endif
struct ShadowDraw
{
    column_major float4x4 mvp;
    float4 modelRow0, modelRow1, modelRow2;
    float4 lightPositionRadius;
};
[[vk::push_constant]] ConstantBuffer<ShadowDraw> draw;
struct Vertex { [[vk::location(0)]] float3 position; };
struct Varying { float4 position : SV_Position; float3 worldPosition; };
[shader("vertex")]
Varying vertex_main(Vertex input)
{
    Varying output;
    float4 p=float4(input.position,1);
    output.position=mul(draw.mvp,p);
    output.position.z=(output.position.z+output.position.w)*0.5;
    output.worldPosition=float3(dot(draw.modelRow0,p),dot(draw.modelRow1,p),dot(draw.modelRow2,p));
    return output;
}
#if POINT_SHADOW
[shader("fragment")]
float fragment_main(Varying input) : SV_Depth
{
    return length(input.worldPosition-draw.lightPositionRadius.xyz)/draw.lightPositionRadius.w;
}
#else
[shader("fragment")]
void fragment_main() {}
#endif
