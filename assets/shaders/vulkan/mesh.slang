struct DrawData
{
    column_major float4x4 mvp;
    float4 modelRow0;
    float4 modelRow1;
    float4 modelRow2;
    // Model texture, transparent alpha and overlay behavior occupy bits 0, 1 and 2.
    uint flags;
    float cameraX;
    float cameraY;
    float cameraZ;
};
[[vk::push_constant]] ConstantBuffer<DrawData> draw;

struct MaterialData
{
    float4 atlas[32];
    float4 scene;
    float4 response[32];
    float4 ambient, cavity, cool, warm, bounce, sky, local, direction, directionalColor;
    float4 glassTint, fog, postfx, grading, cascadeNearTint, cascadeFarTint;
};
[[vk::binding(0, 0)]] ConstantBuffer<MaterialData> materials;
[[vk::binding(1, 0)]] Sampler2D diffuse;
struct LightData { float4 positionRadius[8]; float4 color[8]; int4 count; };
[[vk::binding(2, 0)]] ConstantBuffer<LightData> lights;
struct FrameData { float4 viewportTimeDebug; };
[[vk::binding(8, 0)]] ConstantBuffer<FrameData> frame;

struct ShadowData
{
    column_major float4x4 viewProjection[3];
    float4 nearParameters, farParameters, strengths, cascade;
    float4 spotPositionRange, spotDirection, spotColor;
    int4 flags;
};
[[vk::binding(3,0)]] ConstantBuffer<ShadowData> shadows;
[[vk::binding(4,0)]] Sampler2D shadowNear;
[[vk::binding(5,0)]] Sampler2D shadowFar;
[[vk::binding(6,0)]] Sampler2D shadowSpot;
[[vk::binding(7,0)]] Sampler2DArray shadowPoints;

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

float panel_seam_mask(float2 uv, float scale, float width)
{
    float2 cell = frac(uv / max(scale, 0.001));
    float edge = min(min(cell.x, 1 - cell.x), min(cell.y, 1 - cell.y));
    float feather = max(fwidth(edge) * 1.5, 0.003);
    return 1 - smoothstep(width, width + feather, edge);
}

float hash12(float2 p)
{
    float3 p3=frac(float3(p.x,p.y,p.x)*0.1031);
    p3+=dot(p3,p3.yzx+33.33);
    return frac((p3.x+p3.y)*p3.z);
}
float2 rotate_vec2(float2 p,float angle)
{
    float c=cos(angle),s=sin(angle);
    return float2(p.x*c-p.y*s,p.x*s+p.y*c);
}
float sample_directional(Sampler2D map, float4x4 projection, float3 normal, float3 position,
                         float4 parameters,int halfWidth,float jitter)
{
    float3 world=position+normal*parameters.y;
    float4 clip=mul(projection,float4(world,1));
    float3 coord=clip.xyz/max(clip.w,0.0001)*0.5+0.5;
    if (any(coord<=0) || any(coord>=1)) return 1;
    float bias=max(parameters.x+(1-saturate(dot(normal,normalize(-materials.direction.xyz))))*shadows.strengths.y,parameters.x*0.25);
    uint width,height;
    map.GetDimensions(width,height);
    float2 texel=1.0/float2(width,height);
    float angle=hash12(floor(world.xz*1.75))*6.2831853;
    float visibility=0,weightSum=0;
    for (int y=-2;y<=2;++y) for (int x=-2;x<=2;++x)
    {
        if (abs(x)>halfWidth || abs(y)>halfWidth) continue;
        float weight=float(halfWidth+1-abs(x))*float(halfWidth+1-abs(y));
        float2 offset=rotate_vec2(float2(x,y),angle*max(jitter,0.0));
        float depth=map.Sample(coord.xy+offset*texel*max(parameters.z,0.0001)).r;
        visibility+=(coord.z-bias<=depth ? 1.0 : 1-shadows.strengths.x)*weight;
        weightSum+=weight;
    }
    return visibility/max(weightSum,1.0);
}
float directional_visibility(float3 normal,float3 position,float fogDistance)
{
    if (shadows.flags.x==0) return 1;
    float a=sample_directional(shadowNear,shadows.viewProjection[0],normal,position,shadows.nearParameters,1,0);
    if (shadows.flags.y==0) return a;
    float b=sample_directional(shadowFar,shadows.viewProjection[1],normal,position,shadows.farParameters,2,1);
    float blend=max(shadows.cascade.y,0.0001);
    return lerp(a,b,smoothstep(shadows.cascade.x-blend*0.5,shadows.cascade.x+blend*0.5,fogDistance));
}
float point_depth(float3 ray,int light)
{
    // GL's reference cube array clamps each face independently. A 2D array retains that non-seamless filter.
    float3 a=abs(ray);
    float2 uv;
    int face;
    if (a.x>=a.y && a.x>=a.z)
    {
        face=ray.x>=0 ? 0 : 1;
        uv=float2(ray.x>=0 ? -ray.z : ray.z,-ray.y)/a.x;
    }
    else if (a.y>=a.z)
    {
        face=ray.y>=0 ? 2 : 3;
        uv=float2(ray.x,ray.y>=0 ? ray.z : -ray.z)/a.y;
    }
    else
    {
        face=ray.z>=0 ? 4 : 5;
        uv=float2(ray.z>=0 ? ray.x : -ray.x,-ray.y)/a.z;
    }
    return shadowPoints.Sample(float3(uv*0.5+0.5,float(light*6+face))).r;
}
float local_visibility(int index,float3 lightVector,float distanceToLight,float radius,float3 normal,float3 direction,float attenuation)
{
    if ((shadows.flags.w & (1<<index))==0 || attenuation<=0 || distanceToLight<=0.05) return 1;
    float3 ray=-lightVector/distanceToLight;
    float bias=0.025+0.045*(1-max(dot(normal,direction),0.0));
    float receiver=(distanceToLight-bias)/radius;
    float3 tangent=normalize(cross(ray,abs(ray.y)>0.9 ? float3(1,0,0) : float3(0,1,0)));
    float3 bitangent=cross(ray,tangent);
    float visibility=0;
    for (int tap=0;tap<4;++tap)
    {
        float2 offset=float2(tap==0 || tap==2 ? -1 : 1,tap<2 ? -1 : 1);
        float3 sampleRay=normalize(ray+(tangent*offset.x+bitangent*offset.y)*(2.0/256));
        visibility+=receiver<=point_depth(sampleRay,index) ? 0.25 : 0;
    }
    return visibility;
}
float3 spot_contribution(float3 color,float3 normal,float3 position)
{
    if (shadows.flags.z==0) return 0;
    float3 delta=position-shadows.spotPositionRange.xyz;
    float distanceToSpot=length(delta);
    float3 ray=delta/max(distanceToSpot,0.001);
    float cone=smoothstep(0.899,0.951,dot(ray,shadows.spotDirection.xyz));
    float falloff=pow(saturate(1-distanceToSpot/shadows.spotPositionRange.w),1.5);
    float4 clip=mul(shadows.viewProjection[2],float4(position,1));
    float3 coord=clip.xyz/max(clip.w,0.001)*0.5+0.5;
    float visibility=0;
    if (clip.w>0 && coord.z>=0 && coord.z<=1 && all(coord.xy>=0) && all(coord.xy<=1))
    {
        uint width,height; shadowSpot.GetDimensions(width,height);
        float2 texel=1.0/float2(width,height);
        float bias=max(0.00005,0.0003*(1-max(dot(normal,-ray),0.0)));
        for (int y=-1;y<=1;++y) for (int x=-1;x<=1;++x)
            visibility+=coord.z-bias<=shadowSpot.Sample(coord.xy+float2(x,y)*texel).r ? 1.0/9 : 0;
    }
    return color*shadows.spotColor.rgb*cone*falloff*visibility*max(dot(normal,-ray),0.0)*2.4;
}

float3 safe_normalize_vec3(float3 value, float3 fallback)
{
    float squared = dot(value, value);
    return squared <= 0.000001 ? fallback : value * rsqrt(squared);
}
float soft_noise(float2 uv, float scale) { return hash12(floor(uv / scale)); }
float soft_panel_volume(float2 uv, float scale)
{
    float2 cell = abs(frac(uv / max(scale, 0.001) + 0.5) - 0.5);
    return 1 - smoothstep(0.28, 0.92, max(cell.x, cell.y) * 2);
}
#include "scene_materials.slang"
float3 scene_postfx(float3 color, float2 fragCoord, bool overlay)
{
    if (overlay) return saturate(color + 0.05);
    float luminance = dot(color, float3(0.2126, 0.7152, 0.0722));
    float2 viewport = max(frame.viewportTimeDebug.xy, 1.0);
    float2 screenUV = fragCoord / viewport;
    float time = frame.viewportTimeDebug.z;
    float vignette = 1 - smoothstep(0.28, 0.84, distance(screenUV, float2(0.5))) * materials.postfx.x;
    float grain = (hash12(floor(fragCoord * 0.5) + floor(time * 24)) - 0.5)
                * materials.postfx.y * (0.08 + (1 - luminance) * 0.16);
    float wave = sin(screenUV.y * viewport.y * 0.92 + time * 10) * 0.5 + 0.5;
    float scanline = 1 - (0.04 + wave * 0.08) * materials.postfx.z;
    color = lerp(float3(luminance), color, materials.grading.x);
    color = (color - 0.5) * materials.postfx.w + 0.5;
    color *= lerp(float3(0.97, 0.99, 1.01), float3(1.01, 1.00, 0.98), smoothstep(0.18, 0.78, luminance));
    return saturate(color * vignette * scanline + grain);
}

float3 lit_color(float3 color, float3 normal, float2 uv, float3 position, uint id, bool clothing, bool model, float fogDistance)
{
    float skyAmbient = saturate(normal.y * 0.5 + 0.5);
    float underside = saturate(-normal.y);
    float cavity = panel_seam_mask(uv + float2(0.13, 0.09), 1.9, 0.032) * materials.cavity.x
                 + panel_seam_mask(uv + float2(0.41, 0.17), 0.55, 0.012) * materials.cavity.y
                 + underside * materials.cavity.z;
    if (clothing) cavity = underside * 0.12;
    if (model) cavity = 0;
    float ambient = clamp((lerp(materials.ambient.x, materials.ambient.y, skyAmbient)
                          - cavity * materials.cavity.w) * materials.response[id].x,
                          materials.ambient.z, materials.ambient.w);
    float3 cool = materials.cool.rgb * (materials.cool.w + skyAmbient * materials.sky.x);
    float3 warm = materials.warm.rgb * (materials.warm.w + underside * materials.sky.y);
    float3 bounceTint = lerp(materials.bounce.rgb, color, materials.response[id].y);
    float3 bounce = bounceTint * (materials.bounce.w + underside * materials.sky.z
                                 + (1 - skyAmbient) * materials.sky.w) * materials.local.w;
    float3 result = color * ambient + color * (cool + warm) * 1.04 + bounce;
    float directional = saturate(dot(normal, normalize(-materials.direction.xyz)));
    float visibility=directional_visibility(normal,position,fogDistance);
    result*=1-(1-visibility)*directional*saturate(normal.y)*shadows.strengths.z;
    result += color * materials.directionalColor.rgb * directional * visibility * materials.direction.w;
    for (int i = 0; i < lights.count.x; ++i)
    {
        float3 delta = lights.positionRadius[i].xyz - position;
        float distanceToLight = length(delta);
        float3 direction = distanceToLight > 0.0001 ? delta / distanceToLight : float3(0, 1, 0);
        float attenuation = saturate(1 - distanceToLight / max(lights.positionRadius[i].w, 0.001));
        attenuation = attenuation * attenuation * (3 - 2 * attenuation);
        float wrapped = saturate(dot(normal, direction) * materials.local.x + materials.local.y);
        result += color * lights.color[i].rgb * attenuation * wrapped * materials.local.z * local_visibility(i,delta,distanceToLight,max(lights.positionRadius[i].w,0.001),normal,direction,attenuation);
    }
    result+=spot_contribution(color,normal,position);
    return result;
}

[shader("fragment")]
float4 fragment_main(Varying input) : SV_Target
{
    uint id = input.material;
    bool overlay = (draw.flags & 4) != 0;
    float fog = saturate((input.fogDistance - materials.scene.x) / max(materials.scene.y - materials.scene.x, 0.001));
    float fogAmount = saturate(fog * (overlay ? 0.18 : 1.0));
    if (id == uint(materials.scene.z) + 1 || id == uint(materials.scene.z) + 2)
    {
        float alpha = id == uint(materials.scene.z) + 1
                    ? (1 - smoothstep(0.05, 0.5, length(input.objectPosition.xy))) * 0.16 : 1;
        return float4(lerp(input.color, materials.fog.rgb, fogAmount), alpha);
    }
    float alpha = 1;
    if ((draw.flags & 2) != 0 && id < 32 && materials.atlas[id].z == 1)
    {
        float3 view = float3(draw.cameraX, draw.cameraY, draw.cameraZ) - input.worldPosition;
        float lengthSquared = dot(view, view);
        view = lengthSquared <= 0.000001 ? float3(0, 0, 1) : view * rsqrt(lengthSquared);
        float fresnel = pow(1 - saturate(abs(dot(normalize(input.worldNormal), view))), 2.6);
        alpha = clamp(0.16 + fresnel * 0.34 + fog * 0.08, 0.14, 0.52);
    }
    if (id >= 32) return float4(input.color, 1);
    bool clothing = id >= 23 && id <= 26;
    float2 uv = clothing ? surface_uv(input.objectPosition, normalize(input.objectNormal))
                        : surface_uv(input.worldPosition, normalize(input.worldNormal));
    bool model = (draw.flags & 1) != 0;
    float3 color;
    if (model) color = diffuse.Sample(input.uv).rgb * input.color;
    else
    {
        float scale = max(materials.atlas[id].x, 0.001);
        float2 tileSize = float2(1.0 / 8, 1.0 / 4);
        float2 origin = float2(id % 8, id / 8) * tileSize;
        float2 gutter = tileSize / 16;
        float2 atlasUV = origin + gutter + frac(uv / scale) * (tileSize - 2 * gutter);
        float2 gradient = float2(0.875 / 8, 0.875 / 4) / scale;
        float3 sample = diffuse.SampleGrad(atlasUV, ddx(uv) * gradient, ddy(uv) * gradient).rgb;
        color = lerp(input.color, input.color * sample, materials.atlas[id].y);
    }
    float3 emissive;
    float3 materialColor = apply_material_surface(color, uv, normalize(input.worldNormal), input.worldPosition, int(id), emissive);
    if (clothing || model) { materialColor = color; emissive = 0; }
    emissive *= overlay ? 1.18 : 1.0;
    color = lerp(saturate(lit_color(materialColor, normalize(input.worldNormal), uv, input.worldPosition,
                                  id, clothing, model, input.fogDistance) + emissive), materials.fog.rgb, fogAmount);
    if (frame.viewportTimeDebug.w != 0)
    {
        float blend = max(shadows.cascade.y, 0.0001);
        float cascadeMix = smoothstep(shadows.cascade.x - blend * 0.5, shadows.cascade.x + blend * 0.5, input.fogDistance);
        color = lerp(color, color * lerp(materials.cascadeNearTint.rgb, materials.cascadeFarTint.rgb, cascadeMix), 0.38);
    }
    // GL postfx use bottom-left pixel coordinates; Vulkan fragment positions are top-down.
    float2 fragCoord = float2(input.position.x, frame.viewportTimeDebug.y - input.position.y);
    return float4(scene_postfx(color, fragCoord, overlay), alpha);
}
