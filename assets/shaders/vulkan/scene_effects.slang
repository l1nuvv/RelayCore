#ifndef VOLUME_ONLY
[[vk::binding(1, 1)]] Sampler2D effectNormalDepth;
[[vk::binding(6, 1)]] Sampler2D effectAO;
[[vk::binding(2, 1)]] Sampler2D effectVelocity;
[[vk::binding(7, 1)]] Sampler2D reflectionSource;
[[vk::binding(10, 1)]] Sampler2D diffuseGI;
[[vk::binding(13, 1)]] StructuredBuffer<float2> screenDepthBounds;
[[vk::binding(14, 1)]] Sampler2D reconstructedGI;
[[vk::binding(15, 1)]] Sampler2D traceDepth;

float trace_depth(float2 uv)
{
    return frame.depthHierarchy[0].y != 0 ? traceDepth.SampleLevel(uv, 0).r
                                        : effectNormalDepth.SampleLevel(uv, 0).w;
}

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

int empty_depth_range(float4 firstClip, float4 lastClip, float thickness)
{
    if (frame.depthHierarchy[0].y == 0 || min(firstClip.w, lastClip.w) <= .05) return 0;
    float2 firstUV = effect_uv(firstClip), lastUV = effect_uv(lastClip);
    float2 lowUV = min(firstUV, lastUV), highUV = max(firstUV, lastUV);
    if (any(lowUV <= 0) || any(highUV >= 1)) return 0;
    // Positive clip.w makes both projected coordinates monotone along this ray segment.
    float2 lowPixel = lowUV * frame.viewportTimeDebug.xy;
    float2 highPixel = highUV * frame.viewportTimeDebug.xy;
    uint blocks = uint(ceil(max(highPixel.x - lowPixel.x, highPixel.y - lowPixel.y) / frame.depthHierarchy[0].w));
    uint level = blocks > 1 ? uint(firstbithigh(blocks - 1) + 1) : 0;
    level = min(level, 11u);
    uint4 grid = frame.depthHierarchy[level];
    uint2 first = min(uint2(max(lowPixel - .001, 0) / grid.w), grid.yz - 1);
    uint2 last = min(uint2((highPixel + .001) / grid.w), grid.yz - 1);
    float minimum = 3.402823466e+38, maximum = 0;
    bool complete = true;
    for (uint y = first.y; y <= last.y; ++y)
        for (uint x = first.x; x <= last.x; ++x)
        {
            float2 bounds = screenDepthBounds[grid.x + y * grid.y + x];
            minimum = min(minimum, bounds.x);
            maximum = max(maximum, abs(bounds.y));
            complete = complete && bounds.y > 0;
        }
    if (maximum == 0 || max(firstClip.w, lastClip.w) < minimum - .0005) return -1;
    if (complete && min(firstClip.w, lastClip.w) > maximum + thickness + .0005) return 1;
    return 0;
}

float4 legacy_screen_reflection(float3 position, float3 normal, float3 direction, float roughness)
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
        int lastStep = min(step + 7, 40);
        float lastDistance = .06 * lastStep + .004 * lastStep * lastStep;
        int empty = lastStep > step ? empty_depth_range(clip,
            mul(frame.viewProjection, float4(origin + direction * lastDistance, 1)), .15) : 0;
        if (empty != 0)
        {
            previousDelta = empty;
            previousDistance = lastDistance;
            step = lastStep;
            continue;
        }
        float4 nd = float4(0, 0, 0, trace_depth(uv));
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
                nd = float4(0, 0, 0, trace_depth(uv));
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

float4 trace_screen(float3 position, float3 normal, float3 direction, float thickness, int steps, float range, bool requireEligible = false)
{

    float3 origin = position + normal * .04;
    float previousDelta = -1;
    float previousDistance = 0;
    for (int step = 1; step <= steps; ++step)
    {
        float distance = .06 * step + .004 * step * step;
        if (distance > range) break;
        float4 clip = mul(frame.viewProjection, float4(origin + direction * distance, 1));
        float2 uv = effect_uv(clip);
        if (clip.w <= .05 || any(uv <= 0) || any(uv >= 1)) return 0;
        int lastStep = min(step + 7, steps);
        float lastDistance = .06 * lastStep + .004 * lastStep * lastStep;
        if (lastDistance > range) lastStep = step;
        lastDistance = .06 * lastStep + .004 * lastStep * lastStep;
        int empty = lastStep > step ? empty_depth_range(clip,
            mul(frame.viewProjection, float4(origin + direction * lastDistance, 1)), thickness) : 0;
        if (empty != 0)
        {
            previousDelta = empty;
            previousDistance = lastDistance;
            step = lastStep;
            continue;
        }
        float4 nd = float4(0, 0, 0, trace_depth(uv));
        if (nd.w <= 0) { previousDelta = -1; previousDistance = distance; continue; }
        float delta = clip.w - nd.w;
        if (previousDelta < 0 && delta >= 0 && delta < thickness && distance > .15)
        {
            float low = previousDistance, high = distance;
            for (int refine = 0; refine < 5; ++refine)
            {
                float mid = (low + high) * .5;
                clip = mul(frame.viewProjection, float4(origin + direction * mid, 1));
                uv = effect_uv(clip);
                nd = float4(0, 0, 0, trace_depth(uv));
                if (nd.w > 0 && clip.w >= nd.w) high = mid; else low = mid;
            }
            clip = mul(frame.viewProjection, float4(origin + direction * high, 1));
            uv = effect_uv(clip);
            nd = effectNormalDepth.SampleLevel(uv, 0);
            float2 edge = min(uv, 1 - uv);
            float confidence = saturate(min(edge.x, edge.y) * 12)
                             * saturate(-dot(nd.xyz, direction) * 3) * (1 - smoothstep(range * .55, range, high));
            if (nd.w <= 0 || abs(clip.w - nd.w) > .08
                || (requireEligible && effectVelocity.SampleLevel(uv,0).w <= 0)) return 0;
            return float4(reflectionSource.SampleLevel(uv, 0).rgb, confidence);
        }
        previousDelta = delta;
        previousDistance = distance;
    }
    return 0;
}

float4 screen_reflection(float3 position, float3 normal, float3 direction, float roughness)
{
    if (frame.effects.y == 0 || (draw.flags & (2 | 4 | 32 | 64)) != 0) return 0;
    if (frame.effects.w == 0) return legacy_screen_reflection(position, normal, direction, roughness);
    if (roughness >= .85) return 0;
    float3 tangent = normalize(cross(direction, abs(direction.y) < .9 ? float3(0,1,0) : float3(1,0,0)));
    float3 bitangent = cross(direction, tangent);
    int rays = frame.effects.w > 1 ? 4 : 2;
    float3 radiance = 0;
    float confidence = 0;
    for (int ray = 0; ray < rays; ++ray)
    {
        float angle = ray * 2.399963;
        float cone = roughness * roughness * .35 * sqrt((ray + .5) / rays);
        float3 d = normalize(direction + cone * (tangent * cos(angle) + bitangent * sin(angle)));
        if (dot(d, normal) <= 0) continue;
        float4 hit = trace_screen(position, normal, d, .12, frame.effects.w > 1 ? 56 : 40, 9);
        radiance += hit.rgb * hit.a;
        confidence += hit.a;
    }
    return float4(confidence > 0 ? radiance / confidence : float3(0),
                  confidence / rays * (1 - smoothstep(.45, .85, roughness)));
}

float4 reconstruct_diffuse(float2 uv, float3 normal, float4 nd)
{
    uint width, height;
    diffuseGI.GetDimensions(width,height);
    int2 size = int2(width,height), center = int2(uv * float2(size));
    float3 radiance = 0;
    float coverage = 0, total = 0;
    // Bilateral reconstruction rejects the opposite side of depth/normal edges.
    // Accumulate premultiplied radiance so misses retain fallback energy.
    for (int y=-1; y<=1; ++y)
        for (int x=-1; x<=1; ++x)
        {
            int2 q = center + int2(x,y);
            if (any(q < 0) || any(q >= size)) continue;
            float2 sampleUV = (float2(q)+.5)/float2(size);
            float4 other = effectNormalDepth.SampleLevel(sampleUV,0);
            if (other.w <= 0 || abs(other.w-nd.w) > max(.05,nd.w*.01) || dot(other.xyz,normal) < .9) continue;
            float weight = 1.0 / (1 + x*x + y*y);
            float4 gi = diffuseGI.Load(int3(q,0));
            radiance += gi.rgb * gi.a * weight;
            coverage += gi.a * weight;
            total += weight;
        }
    return total > 0 ? float4(radiance / total, coverage / total) : float4(0, 0, 0, -1);
}

float3 screen_diffuse(float3 fallback, float3 base, float3 position, float3 normal, float2 pixel)
{
    if (frame.lighting.x == 0 || (draw.flags & (2 | 4 | 32 | 64)) != 0) return fallback;
    float4 clip = mul(frame.viewProjection, float4(position,1));
    float2 uv = effect_uv(clip);
    float4 nd = effectNormalDepth.SampleLevel(uv,0);
    if (nd.w <= 0 || abs(nd.w-clip.w) > max(.03, clip.w*.005) || dot(nd.xyz,normal) < .9) return fallback;
    uint width, height;
    diffuseGI.GetDimensions(width, height);
    float2 centerUV = (floor(pixel) + .5) / frame.viewportTimeDebug.xy;
    float4 gi;
    // A different MSAA receiver, sample footprint or mapped normal retains the original reconstruction.
    if (all(int2(uv * frame.viewportTimeDebug.xy) == int2(pixel)) && all(nd.xyz == normal)
        && all(int2(uv * float2(width,height)) == int2(centerUV * float2(width,height))))
        gi = reconstructedGI.Load(int3(int2(pixel), 0));
    else gi = reconstruct_diffuse(uv, normal, nd);
    if (gi.a < 0) return fallback;
    return fallback * (1 - gi.a) + base * gi.rgb;
}

#endif

float3 volumetric_color(float3 surface, float3 position, float3 normal, float fogAmount, bool linearColor, uint lightMask)
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
    float3 directionalColor = linearColor ? mr_linear(materials.directionalColor.rgb) : materials.directionalColor.rgb;
    for (int step = 0; step < 8; ++step)
    {
        float distance = (step + .5) * stepLength;
        float3 p = frame.cameraPosition.xyz + ray * (distance / max(lengthRay, .0001));
        float3 incident = fogColor;
        if (shadows.flags.x != 0)
            incident += directionalColor * materials.direction.w * directional_visibility(normal, p, distance) * .25;
        float3 spotRay;
        incident += spot_radiance(p, normal, spotRay, linearColor) * .25;
        scatter += transmission * segment * incident;
        transmission *= 1 - segment;
    }
    float3 volumeDirection = ray / max(lengthRay, .0001);
    // Integrate scalar light weights before applying the sample-invariant color.
    for (uint mask = lightMask; mask != 0; mask &= mask - 1)
    {
        uint light = firstbitlow(mask);
        float4 positionRadius = lights.positionRadius[light];
        float radius = max(positionRadius.w, .001);
        float3 toLight = positionRadius.xyz - frame.cameraPosition.xyz;
        float closestDistance = clamp(dot(toLight, volumeDirection), 0, lengthVolume);
        float3 closestDelta = toLight - volumeDirection * closestDistance;
        // The full segment encloses all eight samples; the margin retains boundary lights.
        float conservativeRadius = radius + .001;
        if (dot(closestDelta, closestDelta) > conservativeRadius * conservativeRadius) continue;
        bool shadowed = (shadows.flags.w & (1 << light)) != 0;
        float lightScatter = 0;
        float lightTransmission = 1;
        for (int step = 0; step < 8; ++step)
        {
            float distance = (step + .5) * stepLength;
            float3 p = frame.cameraPosition.xyz + ray * (distance / max(lengthRay, .0001));
            float3 delta = positionRadius.xyz - p;
            float d = length(delta);
            float attenuation = saturate(1 - d / radius);
            if (attenuation > 0)
            {
                float visibility = !shadowed || d <= .05 ? 1
                    : float((d - .025) / positionRadius.w <= point_depth(-delta / d, light));
                lightScatter += lightTransmission * segment * attenuation * attenuation * visibility * .25;
            }
            lightTransmission *= 1 - segment;
        }
        float3 color = linearColor ? mr_linear(lights.color[light].rgb) : lights.color[light].rgb;
        scatter += color * lightScatter;
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
