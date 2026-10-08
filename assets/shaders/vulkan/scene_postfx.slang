float3 linear_to_srgb(float3 color)
{
    return select(color <= 0.0031308, color * 12.92, 1.055 * pow(max(color, 0), 1.0 / 2.4) - 0.055);
}

float3 srgb_to_linear(float3 color)
{
    return select(color <= 0.04045, color / 12.92, pow((color + 0.055) / 1.055, 2.4));
}

float3 sdr_highlight_rolloff(float3 color)
{
    // Preserve the existing SDR response below the shoulder, including shadows and midtones.
    // At 0.65 the value and slope are continuous; the remaining 0.35 tends to display white.
    float3 highlight = max(color - 0.65, 0);
    return min(color, 0.65) + highlight / (1 + highlight / 0.35);
}

float hash12(float2 p)
{
    float3 p3=frac(float3(p.x,p.y,p.x)*0.1031);
    p3+=dot(p3,p3.yzx+33.33);
    return frac((p3.x+p3.y)*p3.z);
}

float3 scene_postfx(float3 color, float2 fragCoord, float4 viewportTime, float4 postfx, float saturation, bool overlay)
{
    if (overlay) return saturate(color + 0.05);
    float luminance = dot(color, float3(0.2126, 0.7152, 0.0722));
    float2 viewport = max(viewportTime.xy, 1.0);
    float2 screenUV = fragCoord / viewport;
    float time = viewportTime.z;
    float vignette = 1 - smoothstep(0.28, 0.84, distance(screenUV, float2(0.5))) * postfx.x;
    float grain = (hash12(floor(fragCoord * 0.5) + floor(time * 24)) - 0.5)
                * postfx.y * (0.08 + (1 - luminance) * 0.16);
    float wave = sin(screenUV.y * viewport.y * 0.92 + time * 10) * 0.5 + 0.5;
    float scanline = 1 - (0.04 + wave * 0.08) * postfx.z;
    color = lerp(float3(luminance), color, saturation);
    color = (color - 0.5) * postfx.w + 0.5;
    color *= lerp(float3(0.97, 0.99, 1.01), float3(1.01, 1.00, 0.98), smoothstep(0.18, 0.78, luminance));
    return saturate(color * vignette * scanline + grain);
}
