struct DrawData
{
    column_major float4x4 mvp;
    float4 modelRow0;
    float4 modelRow1;
    float4 modelRow2;
    // Bit 0 selects the model texture; bit 1 enables transparent material alpha.
    uint flags;
    float cameraX;
    float cameraY;
    float cameraZ;
};
[[vk::push_constant]] ConstantBuffer<DrawData> draw;

struct MaterialData { float4 atlas[32]; float4 scene; };
[[vk::binding(0, 0)]] ConstantBuffer<MaterialData> materials;
[[vk::binding(1, 0)]] Sampler2D diffuse;

struct Vertex
{
    [[vk::location(0)]] float3 position;
    [[vk::location(1)]] float3 color;
    [[vk::location(2)]] float3 normal;
    [[vk::location(3)]] float material;
    [[vk::location(4)]] float2 uv;
};
struct Varying
{
    float4 position : SV_Position;
    float3 color;
    float3 worldPosition;
    float3 worldNormal;
    float3 objectPosition;
    float3 objectNormal;
    float2 uv;
    nointerpolation uint material;
    float fogDistance;
};

[shader("vertex")]
Varying vertex_main(Vertex input)
{
    Varying output;
    float4 p = float4(input.position, 1);
    output.position = mul(draw.mvp, p);
    // Preserve the CPU camera's OpenGL clip convention for culling; convert only at the GPU boundary.
    output.position.z = (output.position.z + output.position.w) * 0.5;
    output.color = input.color;
    output.worldPosition = float3(dot(draw.modelRow0, p), dot(draw.modelRow1, p), dot(draw.modelRow2, p));
    float3 a = float3(draw.modelRow0.x, draw.modelRow1.x, draw.modelRow2.x);
    float3 b = float3(draw.modelRow0.y, draw.modelRow1.y, draw.modelRow2.y);
    float3 c = float3(draw.modelRow0.z, draw.modelRow1.z, draw.modelRow2.z);
    float determinant = dot(a, cross(b, c));
    output.worldNormal = normalize((cross(b, c) * input.normal.x + cross(c, a) * input.normal.y
                                  + cross(a, b) * input.normal.z) / determinant);
    output.objectPosition = input.position;
    output.objectNormal = input.normal;
    output.uv = input.uv;
    output.material = uint(input.material + 0.5);
    output.fogDistance = distance(output.worldPosition, float3(draw.cameraX, draw.cameraY, draw.cameraZ));
    return output;
}

float2 surface_uv(float3 position, float3 normal)
{
    float3 n = abs(normal);
    if (n.y >= n.x && n.y >= n.z) return position.xz;
    if (n.x >= n.z) return position.zy;
    return position.xy;
}

[shader("fragment")]
float4 fragment_main(Varying input) : SV_Target
{
    uint id = input.material;
    if (id == uint(materials.scene.z) + 1)
        return float4(input.color, (1 - smoothstep(0.05, 0.5, length(input.objectPosition.xy))) * 0.16);
    float alpha = 1;
    if ((draw.flags & 2) != 0 && id < 32 && materials.atlas[id].z != 0)
    {
        float3 view = float3(draw.cameraX, draw.cameraY, draw.cameraZ) - input.worldPosition;
        float lengthSquared = dot(view, view);
        view = lengthSquared <= 0.000001 ? float3(0, 0, 1) : view * rsqrt(lengthSquared);
        float fresnel = pow(1 - saturate(abs(dot(normalize(input.worldNormal), view))), 2.6);
        float fog = saturate((input.fogDistance - materials.scene.x) / max(materials.scene.y - materials.scene.x, 0.001));
        alpha = clamp(0.16 + fresnel * 0.34 + fog * 0.08, 0.14, 0.52);
    }
    if ((draw.flags & 1) != 0)
        return float4(diffuse.Sample(input.uv).rgb * input.color, alpha);
    if (id >= 32) return float4(input.color, 1);
    bool clothing = id >= 23 && id <= 26;
    float2 uv = clothing ? surface_uv(input.objectPosition, normalize(input.objectNormal))
                        : surface_uv(input.worldPosition, normalize(input.worldNormal));
    float scale = max(materials.atlas[id].x, 0.001);
    float2 tileSize = float2(1.0 / 8, 1.0 / 4);
    float2 origin = float2(id % 8, id / 8) * tileSize;
    float2 gutter = tileSize / 16;
    float2 atlasUV = origin + gutter + frac(uv / scale) * (tileSize - 2 * gutter);
    float2 gradient = float2(0.875 / 8, 0.875 / 4) / scale;
    float3 sample = diffuse.SampleGrad(atlasUV, ddx(uv) * gradient, ddy(uv) * gradient).rgb;
    return float4(lerp(input.color, input.color * sample, materials.atlas[id].y), alpha);
}
