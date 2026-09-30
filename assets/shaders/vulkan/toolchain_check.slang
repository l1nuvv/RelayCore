[shader("vertex")]
float4 vertex_main(float3 position : POSITION) : SV_Position
{
    return float4(position, 1.0);
}
