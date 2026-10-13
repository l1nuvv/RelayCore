// Host layout: DrawInstance, 128 bytes; correspondence.x indexes the original temporal record.
struct SceneInstance
{
    column_major float4x4 mvp;
    float4 modelRow0, modelRow1, modelRow2;
    uint4 correspondence;
};
#ifndef SCENE_CULLING_SHADER
[[vk::binding(0, 2)]] StructuredBuffer<SceneInstance> sceneInstances;
#endif
