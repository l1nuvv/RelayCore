#ifndef VOLUME_ONLY
[[vk::binding(1, 1)]] Sampler2D effectNormalDepth;
[[vk::binding(6, 1)]] Sampler2D effectAO;
[[vk::binding(7, 1)]] Sampler2D reflectionSource;

float3 effect_world(float2 uv, float depth)
{
    float4 farPoint = mul(frame.inverseViewProjection, float4(uv * float2(2, -2) + float2(-1, 1), 1, 1));
    float3 ray = farPoint.xyz / farPoint.w - frame.cameraPosition.xyz;
    return frame.cameraPosition.xyz + ray * (depth / max(-mul(frame.viewMatrix, float4(ray, 0)).z, .0001));
}

float2 effect_uv(float4 clip)
{
    return clip.xy / max(clip.w, .0001) * float2(.5, -.5) + .5;
}

float screen_ao(float2 pixel, float3 normal, float depth)
{
    float total = 0, result = 0;
    for (int y = -1; y <= 1; ++y)
        for (int x = -1; x <= 1; ++x)
        {
            int2 p = clamp(int2(pixel) + int2(x, y), int2(0), int2(frame.viewportTimeDebug.xy) - 1);
            float4 nd = effectNormalDepth.Load(int3(p, 0));
            float weight = float(nd.w > 0 && abs(nd.w - depth) < max(.05, depth * .005) && dot(nd.xyz, normal) > .8);
            result += effectAO.Load(int3(p, 0)).r * weight;
            total += weight;
        }
    return total > 0 ? result / total : 1;
}

float4 screen_reflection(float3 position, float3 normal, float3 direction, float roughness)
{
    if (frame.effects.y == 0 || (draw.flags & (2 | 4 | 32 | 64)) != 0 || roughness >= .65) return 0;
    float3 origin = position + normal * .04;
    float previousDelta = -1;
    float previousDistance = 0;
    for (int step = 1; step <= 40; ++step)
    {
        float distance = .06 * step + .004 * step * step;
        float4 clip = mul(frame.viewProjection, float4(origin + direction * distance, 1));
        float2 uv = effect_uv(clip);
        if (clip.w <= .05 || any(uv <= 0) || any(uv >= 1)) return 0;
        float4 nd = effectNormalDepth.SampleLevel(uv, 0);
        if (nd.w <= 0) { previousDelta = -1; previousDistance = distance; continue; }
        float delta = clip.w - nd.w;
        if (previousDelta < 0 && delta >= 0 && delta < .15 && distance > .15)
        {
            float low = previousDistance, high = distance;
            for (int refine = 0; refine < 5; ++refine)
            {
                float mid = (low + high) * .5;
                clip = mul(frame.viewProjection, float4(origin + direction * mid, 1));
                uv = effect_uv(clip);
                nd = effectNormalDepth.SampleLevel(uv, 0);
                if (nd.w > 0 && clip.w >= nd.w) high = mid; else low = mid;
            }
            clip = mul(frame.viewProjection, float4(origin + direction * high, 1));
            uv = effect_uv(clip);
            nd = effectNormalDepth.SampleLevel(uv, 0);
            float2 edge = min(uv, 1 - uv);
            float confidence = saturate(min(edge.x, edge.y) * 12) * (1 - smoothstep(.25, .65, roughness))
                             * saturate(-dot(nd.xyz, direction) * 3) * (1 - smoothstep(5, 9, high));
            if (nd.w <= 0 || abs(clip.w - nd.w) > .08) return 0;
            return float4(reflectionSource.SampleLevel(uv, 0).rgb, confidence);
        }
        previousDelta = delta;
        previousDistance = distance;
    }
    return 0;
}

#endif

float3 volumetric_color(float3 surface, float3 position, float3 normal, float fogAmount, bool linearColor)
{
    float3 fogColor = linearColor ? mr_linear(materials.fog.rgb) : materials.fog.rgb;
    if (frame.effects.z == 0 || (draw.flags & (2 | 4 | 32)) != 0) return lerp(surface, fogColor, fogAmount);
    float3 ray = position - frame.cameraPosition.xyz;
    float lengthRay = length(ray);
    float lengthVolume = min(lengthRay, 24);
    float stepLength = lengthVolume / 8;
    float segment = 1 - exp(-frame.effects.z * stepLength);
    float transmission = 1;
    float3 scatter = 0;
    for (int step = 0; step < 8; ++step)
    {
        float distance = (step + .5) * stepLength;
        float3 p = frame.cameraPosition.xyz + ray * (distance / max(lengthRay, .0001));
        float3 incident = fogColor;
        if (shadows.flags.x != 0)
            incident += (linearColor ? mr_linear(materials.directionalColor.rgb) : materials.directionalColor.rgb) * materials.direction.w
                      * directional_visibility(normal, p, distance) * .25;
        for (int light = 0; light < lights.count.x; ++light)
        {
            float3 delta = lights.positionRadius[light].xyz - p;
            float d = length(delta);
            float attenuation = saturate(1 - d / max(lights.positionRadius[light].w, .001));
            float visibility = (shadows.flags.w & (1 << light)) == 0 || d <= .05 ? 1
                : float((d - .025) / lights.positionRadius[light].w <= point_depth(-delta / d, light));
            incident += (linearColor ? mr_linear(lights.color[light].rgb) : lights.color[light].rgb) * attenuation * attenuation * visibility * .25;
        }
        float3 spotRay;
        incident += spot_radiance(p, normal, spotRay, linearColor) * .25;
        scatter += transmission * segment * incident;
        transmission *= 1 - segment;
    }
    return surface * transmission + scatter;
}

#ifndef VOLUME_ONLY
[shader("vertex")]
float4 ao_vertex(uint id : SV_VulkanVertexID) : SV_Position
{
    return float4(float2((id << 1) & 2, id & 2) * 2 - 1, 0, 1);
}

[shader("fragment")]
float4 ao_fragment(float4 pixel : SV_Position) : SV_Target
{
    float2 uv = pixel.xy / frame.viewportTimeDebug.xy;
    float4 nd = effectNormalDepth.SampleLevel(uv, 0);
    if (nd.w <= 0) return float4(1);
    float3 p = effect_world(uv, nd.w);
    float3 tangent = normalize(cross(nd.xyz, abs(nd.y) < .9 ? float3(0, 1, 0) : float3(1, 0, 0)));
    float3 bitangent = cross(nd.xyz, tangent);
    float occlusion = 0;
    for (int sample = 0; sample < 12; ++sample)
    {
        float angle = sample * 2.399963;
        float z = .2 + .6 * (sample + .5) / 12;
        float radial = sqrt(1 - z * z);
        float3 offset = (tangent * cos(angle) * radial + bitangent * sin(angle) * radial + nd.xyz * z)
                      * (.12 + .33 * (sample + .5) / 12);
        float4 clip = mul(frame.viewProjection, float4(p + offset, 1));
        float2 sampleUV = effect_uv(clip);
        if (clip.w <= 0 || any(sampleUV <= 0) || any(sampleUV >= 1)) continue;
        float4 other = effectNormalDepth.SampleLevel(sampleUV, 0);
        if (other.w <= 0 || other.w >= clip.w - .025) continue;
        float3 delta = effect_world(sampleUV, other.w) - p;
        float d = length(delta);
        if (d > .025 && dot(nd.xyz, delta) > .02)
            occlusion += (1 - smoothstep(.2, .5, d));
    }
    return float4((1 - .8 * occlusion / 12).xxx, 1);
}
#endif
