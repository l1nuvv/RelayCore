struct UiPush { float4x4 transform; float2 translation; };
[[vk::push_constant]] ConstantBuffer<UiPush> push;
[[vk::binding(0, 0)]] Sampler2D image;
struct VertexInput {
    [[vk::location(0)]] float2 position : POSITION;
    [[vk::location(1)]] float4 color : COLOR;
    [[vk::location(2)]] float2 uv : TEXCOORD;
};
struct VertexOutput {
    float4 position : SV_Position;
    float4 color : COLOR;
    float2 uv : TEXCOORD;
};
VertexOutput vertex_main(VertexInput input) {
    VertexOutput output;
    output.position = mul(push.transform, float4(input.position + push.translation, 0, 1));
    output.color = input.color;
    output.uv = input.uv;
    return output;
}
float4 fragment_main(VertexOutput input) : SV_Target {
    return input.color * image.Sample(input.uv);
}
