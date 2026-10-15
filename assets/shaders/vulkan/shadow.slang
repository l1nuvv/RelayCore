#ifndef POINT_SHADOW
#define POINT_SHADOW 0
#endif
#include "scene_instances.slang"
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
Varying vertex_main(Vertex input, uint instance : SV_VulkanInstanceID)
{
    Varying output;
    float4 p=float4(input.position,1);
    SceneInstance transform;
    if (draw.lightPositionRadius.w < 0)
        transform = sceneInstances[asuint(draw.modelRow0.x) + instance];
    else
    {
        transform.mvp = draw.mvp;
        transform.modelRow0 = draw.modelRow0;
        transform.modelRow1 = draw.modelRow1;
        transform.modelRow2 = draw.modelRow2;
    }
    output.position=mul(transform.mvp,p);
    output.position.z=(output.position.z+output.position.w)*0.5;
    output.worldPosition=float3(dot(transform.modelRow0,p),dot(transform.modelRow1,p),dot(transform.modelRow2,p));
    return output;
}
#if POINT_SHADOW
[shader("fragment")]
float fragment_main(Varying input) : SV_Depth
{
    return length(input.worldPosition-draw.lightPositionRadius.xyz)/abs(draw.lightPositionRadius.w);
}
#else
[shader("fragment")]
void fragment_main() {}
#endif
