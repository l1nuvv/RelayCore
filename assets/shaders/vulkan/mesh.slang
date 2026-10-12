struct DrawData
{
    column_major float4x4 mvp;
    float4 modelRow0;
    float4 modelRow1;
    float4 modelRow2;
    // Bits 0..5: model color, transparent alpha, overlay, normal map, metallic/roughness, viewmodel.
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
[[vk::binding(9, 0)]] Sampler2D normalMap;
[[vk::binding(12, 0)]] Sampler2D metallicRoughnessMap;
[[vk::binding(13, 0)]] Sampler2D occlusionMap;
[[vk::binding(16, 0)]] Sampler2D emissiveMap;
struct SurfaceData { float4 parameters; float4 maps; float4 emission; };
[[vk::binding(10, 0)]] ConstantBuffer<SurfaceData> surfaceData;
[[vk::binding(14, 0)]] SamplerCube reflectionMap;
struct ReflectionData { float4 mins, maxs, position; };
[[vk::binding(15, 0)]] ConstantBuffer<ReflectionData> reflectionData;
struct IrradianceData
{
    float4 mins, maxs;
    uint4 dimensions;
    float4 coefficients[576];
};
[[vk::binding(11, 0)]] ConstantBuffer<IrradianceData> irradiance;

// Matches the positive real basis used by the CPU baker; coefficients already contain cosine / pi.
float3 probe_indirect(float3 position, float3 normal, float reflectionRoughness = -1)
{
    if (irradiance.dimensions.w == 0 || (draw.flags & 36) != 0
        || any(position < irradiance.mins.xyz) || any(position > irradiance.maxs.xyz)) return 0;
    float3 grid = saturate((position-irradiance.mins.xyz-.15)
        / (irradiance.maxs.xyz-irradiance.mins.xyz-.3)) * float3(irradiance.dimensions.xyz-1);
    uint3 cell = min(uint3(grid), irradiance.dimensions.xyz-2);
    float3 fraction = grid-float3(cell);
    float basis[9] = { .282094792, .488602512*normal.y, .488602512*normal.z, .488602512*normal.x,
        1.092548431*normal.x*normal.y, 1.092548431*normal.y*normal.z,
        .315391565*(3*normal.z*normal.z-1), 1.092548431*normal.x*normal.z,
        .546274215*(normal.x*normal.x-normal.y*normal.y) };
    // Recover low-frequency incident radiance from cosine/pi-convolved SH for reflections.
    // A roughness window suppresses high bands; these probes cannot represent sharp reflections.
    if (reflectionRoughness >= 0)
        for (uint sh=1; sh<9; ++sh)
        {
            float band = sh < 4 ? 2.0 : 6.0;
            basis[sh] *= exp(-band*reflectionRoughness*reflectionRoughness) / (sh < 4 ? 2.0/3.0 : .25);
        }
    float3 result = 0;
    for (uint corner=0; corner<8; ++corner)
    {
        uint3 offset = uint3(corner&1, (corner>>1)&1, (corner>>2)&1);
        uint3 q = cell+offset;
        float3 weight = lerp(1-fraction, fraction, float3(offset));
        uint index = (q.x+irradiance.dimensions.x*(q.y+irradiance.dimensions.y*q.z))*9;
        for (uint sh=0; sh<9; ++sh)
            result += irradiance.coefficients[index+sh].rgb * (basis[sh]*weight.x*weight.y*weight.z);
    }
    return max(result, 0);
}
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
    [[vk::location(5)]] float4 tangent;
};
struct Varying
{
    float4 position : SV_Position;
    float3 color;
    float3 worldPosition;
    float3 worldNormal;
    float4 worldTangent;
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
    output.worldTangent = float4(a * input.tangent.x + b * input.tangent.y + c * input.tangent.z,
                                 input.tangent.w * sign(determinant));
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

#include "scene_postfx.slang"
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
float local_visibility(int index,float3 lightVector,float distanceToLight,float radius,float3 normal,float3 receiverPlaneNormal,float3 direction,float attenuation)
{
    if ((shadows.flags.w & (1<<index))==0 || attenuation<=0 || distanceToLight<=0.05) return 1;
    float3 ray=-lightVector/distanceToLight;
    float bias=0.025+0.045*(1-max(dot(normal,direction),0.0));
    float3 tangent=normalize(cross(ray,abs(ray.y)>0.9 ? float3(1,0,0) : float3(0,1,0)));
    float3 bitangent=cross(ray,tangent);
    uint shadowWidth, shadowHeight, shadowLayers;
    shadowPoints.GetDimensions(shadowWidth,shadowHeight,shadowLayers);
    float visibility=0;
    for (int tap=0;tap<9;++tap)
    {
        float2 offset=float2(tap%3-1,tap/3-1);
        float3 sampleRay=normalize(ray+(tangent*offset.x+bitangent*offset.y)*(2.0/shadowWidth));
        // Each PCF ray intersects a different point on the geometric receiver plane.
        // Comparing every tap to the centre distance self-shadows grazing surfaces.
        float planeCosine=dot(receiverPlaneNormal,sampleRay);
        float sampleDistance=abs(planeCosine)>0.0001
            ? distanceToLight*dot(receiverPlaneNormal,ray)/planeCosine : distanceToLight;
        float receiver=(sampleDistance-bias)/radius;
        visibility+=receiver<=point_depth(sampleRay,index) ? 1.0/9 : 0;
    }
    return visibility;
}
float3 spot_radiance(float3 position,float3 geometricNormal,out float3 ray, bool linearColor = false)
{
    ray = float3(0,0,-1);
    if (shadows.flags.z==0) return 0;
    float3 delta=position-shadows.spotPositionRange.xyz;
    float distanceToSpot=length(delta);
    ray=delta/max(distanceToSpot,0.001);
    float cone=smoothstep(0.899,0.951,dot(ray,shadows.spotDirection.xyz));
    float falloff=pow(saturate(1-distanceToSpot/shadows.spotPositionRange.w),1.5);
    float4 clip=mul(shadows.viewProjection[2],float4(position,1));
    float3 coord=clip.xyz/max(clip.w,0.001)*0.5+0.5;
    float visibility=0;
    if (clip.w>0 && coord.z>=0 && coord.z<=1 && all(coord.xy>=0) && all(coord.xy<=1))
    {
        uint width,height; shadowSpot.GetDimensions(width,height);
        float2 texel=1.0/float2(width,height);
        float bias=max(0.00005,0.0003*(1-max(dot(geometricNormal,-ray),0.0)));
        for (int y=-1;y<=1;++y) for (int x=-1;x<=1;++x)
            visibility+=coord.z-bias<=shadowSpot.Sample(coord.xy+float2(x,y)*texel).r ? 1.0/9 : 0;
    }
    float3 color = shadows.spotColor.rgb;
    if (linearColor && surfaceData.parameters.w == 0)
        color = select(color <= 0.04045, color / 12.92, pow((color + 0.055) / 1.055, 2.4));
    return color*cone*falloff*visibility*2.4;
}
float3 spot_contribution(float3 color,float3 normal,float3 position,float3 geometricNormal)
{
    float3 ray;
    float3 radiance = spot_radiance(position,geometricNormal,ray);
    return color*radiance*max(dot(normal,-ray),0.0);
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

float3 lit_color(float3 color, float3 normal, float3 geometricNormal, float3 receiverPlaneNormal, float2 uv, float3 position, uint id, bool clothing, bool model, float fogDistance, float ao)
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
    float3 indirect = probe_indirect(position, normal);
    if (surfaceData.parameters.w == 0)
        indirect = select(indirect <= 0.0031308, indirect * 12.92, 1.055 * pow(indirect, 1.0/2.4) - .055);
    result += color * indirect;
    result *= ao;
    float directional = saturate(dot(normal, normalize(-materials.direction.xyz)));
    float visibility=directional_visibility(geometricNormal,position,fogDistance);
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
        result += color * lights.color[i].rgb * attenuation * wrapped * materials.local.z * local_visibility(i,delta,distanceToLight,max(lights.positionRadius[i].w,0.001),geometricNormal,receiverPlaneNormal,direction,attenuation);
    }
    result+=spot_contribution(color,normal,position,geometricNormal);
    return result;
}

float3 mr_linear(float3 color)
{
    if (surfaceData.parameters.w != 0) return color;
    return srgb_to_linear(color);
}

// glTF Appendix B: GGX, separable Smith visibility, Schlick, Lambert diffuse.
float3 metallic_roughness_brdf(float3 base, float3 normal, float3 view, float3 light, float roughness, float metallic)
{
    float nl = saturate(dot(normal, light)), nv = saturate(dot(normal, view));
    if (nl <= 0 || nv <= 0) return 0;
    float3 halfVector = safe_normalize_vec3(view + light, normal);
    float nh = saturate(dot(normal, halfVector)), vh = saturate(dot(view, halfVector));
    // The valid roughness=0 singularity uses a documented finite lobe (roughness >= 0.05).
    float alpha = max(roughness * roughness, 0.0025);
    float a2 = alpha * alpha;
    float denominator = (1 - nh * nh) + a2 * nh * nh;
    float distribution = a2 / (3.14159265 * denominator * denominator);
    float visibility = 1 / ((nl + sqrt(a2 + (1-a2)*nl*nl)) * (nv + sqrt(a2 + (1-a2)*nv*nv)));
    float3 f0 = lerp(float3(0.04), base, metallic);
    float3 fresnel = f0 + (1-f0) * pow(1-vh, 5);
    return ((1-fresnel) * (1-metallic) * base / 3.14159265 + fresnel * distribution * visibility) * nl;
}
float3 environment_brdf(float3 f0, float roughness, float nv)
{
    // Karis/Lazarov analytic split-sum fit: unrealengine.com/blog/physically-based-shading-on-mobile
    float4 r = roughness*float4(-1,-.0275,-.572,.022) + float4(1,.0425,1.04,-.04);
    float a = min(r.x*r.x, exp2(-9.28*nv))*r.x+r.y;
    float2 integrated = float2(-1.04,1.04)*a+r.zw;
    return max(f0*integrated.x+integrated.y, 0);
}
float3 metallic_roughness_color(float3 color, float3 normal, float3 geometricNormal, float3 receiverPlaneNormal, float3 position, float fogDistance, float roughness, float metallic, float ao)
{
    float3 base = mr_linear(color);
    float3 view = safe_normalize_vec3(float3(draw.cameraX,draw.cameraY,draw.cameraZ)-position,normal);
    float ambient = lerp(materials.ambient.x,materials.ambient.y,saturate(normal.y*.5+.5));
    float3 result = base * (1-metallic) * 0.96 * ambient;
    result += base * (1-metallic) * .96 * probe_indirect(position,normal);
    float3 reflected = reflect(-view, normal);
    float reflectedAmbient = lerp(materials.ambient.x,materials.ambient.y,saturate(reflected.y*.5+.5));
    float3 environment = float3(reflectedAmbient) + probe_indirect(position,reflected,roughness);
    if (reflectionData.mins.w != 0 && (draw.flags & 36) == 0
        && all(position >= reflectionData.mins.xyz) && all(position <= reflectionData.maxs.xyz))
    {
        // Box projection anchors room reflections to the receiver's position, rather than the camera.
        float3 safeDirection = select(abs(reflected) < .00001, select(reflected < 0, -.00001, .00001), reflected);
        float3 plane = select(safeDirection > 0, reflectionData.maxs.xyz, reflectionData.mins.xyz);
        float3 distances = (plane-position)/safeDirection;
        float distance = min(distances.x, min(distances.y,distances.z));
        float3 lookup = position + reflected*distance - reflectionData.position.xyz;
        environment = reflectionMap.SampleLevel(lookup, roughness*reflectionData.maxs.w).rgb;
    }
    float3 f0 = lerp(float3(.04),base,metallic);
    result += environment * environment_brdf(f0,roughness,saturate(dot(normal,view)));
    result *= ao;
    float3 direction = normalize(-materials.direction.xyz);
    result += metallic_roughness_brdf(base,normal,view,direction,roughness,metallic) * mr_linear(materials.directionalColor.rgb)
              * materials.direction.w * directional_visibility(geometricNormal,position,fogDistance);
    for (int i=0;i<lights.count.x;++i)
    {
        float3 delta = lights.positionRadius[i].xyz-position;
        float distanceToLight = length(delta);
        float3 lightDirection = distanceToLight > 0.0001 ? delta/distanceToLight : float3(0,1,0);
        float attenuation = saturate(1-distanceToLight/max(lights.positionRadius[i].w,0.001));
        attenuation = attenuation*attenuation*(3-2*attenuation);
        float visibility = local_visibility(i,delta,distanceToLight,max(lights.positionRadius[i].w,0.001),geometricNormal,receiverPlaneNormal,lightDirection,attenuation);
        result += metallic_roughness_brdf(base,normal,view,lightDirection,roughness,metallic) * mr_linear(lights.color[i].rgb)
                  * attenuation * materials.local.z * visibility;
    }
    float3 ray;
    float3 spot = spot_radiance(position,geometricNormal,ray,true);
    result += metallic_roughness_brdf(base,normal,view,-ray,roughness,metallic) * spot;
    return result;
}

float3 surface_normal(Varying input)
{
    float3 geometricNormal = normalize(input.worldNormal);
    if ((draw.flags & 8) == 0) return geometricNormal;
    float3 tangent = normalize(input.worldTangent.xyz - geometricNormal * dot(geometricNormal, input.worldTangent.xyz));
    float3 bitangent = cross(geometricNormal, tangent) * input.worldTangent.w;
    float3 sampled = normalMap.Sample(input.uv).rgb * 2 - 1;
    sampled.xy *= surfaceData.parameters.x;
    sampled /= max(max(abs(sampled.x), abs(sampled.y)), abs(sampled.z));
    return normalize(tangent * sampled.x + bitangent * sampled.y + geometricNormal * sampled.z);
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
    float3 geometricNormal = normalize(input.worldNormal);
    // Interpolated mesh normals describe shading, not the rasterized triangle plane.
    float3 receiverPlaneNormal = normalize(cross(ddx(input.worldPosition), ddy(input.worldPosition)));
    float3 shadingNormal = surface_normal(input);
    float3 emissive;
    float3 materialColor = apply_material_surface(color, uv, normalize(input.worldNormal), input.worldPosition, int(id), emissive);
    if (clothing || model) { materialColor = color; emissive = 0; }
    emissive *= overlay ? 1.18 : 1.0;
    float2 mr = surfaceData.parameters.yz;
    if (surfaceData.maps.x != 0) mr *= metallicRoughnessMap.Sample(input.uv).gb;
    float ao = surfaceData.maps.y != 0 ? lerp(1.0, occlusionMap.Sample(input.uv).r, surfaceData.maps.z) : 1.0;
    bool metallicRoughness = (draw.flags & 16) != 0;
    if (metallicRoughness && (draw.flags & 8) != 0)
    {
        // Broaden GGX only where mapped normals vary across a pixel footprint.
        float3 dx = ddx(shadingNormal), dy = ddy(shadingNormal);
        float kernel = min(.25 * (dot(dx, dx) + dot(dy, dy)), .04);
        mr.x = pow(saturate(pow(mr.x, 4) + kernel), .25);
    }
    float3 nativeEmission = surfaceData.emission.rgb;
    if (surfaceData.emission.w != 0) nativeEmission *= emissiveMap.Sample(input.uv).rgb;
    // Native emission is linear even on the legacy visual-sRGB lighting path.
    if (!metallicRoughness && surfaceData.parameters.w == 0) nativeEmission = linear_to_srgb(nativeEmission);
    emissive += nativeEmission;
    float3 lighting = metallicRoughness
        ? metallic_roughness_color(materialColor, shadingNormal, geometricNormal, receiverPlaneNormal, input.worldPosition, input.fogDistance, mr.x, mr.y, ao)
        : lit_color(materialColor, shadingNormal, geometricNormal, receiverPlaneNormal, uv, input.worldPosition,
                    id, clothing, model, input.fogDistance, ao);
    float3 fogColor = metallicRoughness ? mr_linear(materials.fog.rgb) : materials.fog.rgb;
    bool hdr = materials.grading.y != 0;
    color = lerp(max(lighting + emissive, 0), fogColor, fogAmount);
    if (frame.viewportTimeDebug.w != 0)
    {
        float blend = max(shadows.cascade.y, 0.0001);
        float cascadeMix = smoothstep(shadows.cascade.x - blend * 0.5, shadows.cascade.x + blend * 0.5, input.fogDistance);
        float3 nearTint = metallicRoughness ? mr_linear(materials.cascadeNearTint.rgb) : materials.cascadeNearTint.rgb;
        float3 farTint = metallicRoughness ? mr_linear(materials.cascadeFarTint.rgb) : materials.cascadeFarTint.rgb;
        color = lerp(color, color * lerp(nearTint, farTint, cascadeMix), 0.38);
    }
    if (hdr) return float4(color, alpha);
    // Roll off SDR highlights before postfx clamps them, retaining lit surface detail.
    color = sdr_highlight_rolloff(color);
    // Postfx controls describe display brightness in every mode. The sRGB attachment
    // still expects linear output and performs the final encoding once.
    bool linearColor = metallicRoughness || surfaceData.parameters.w != 0;
    if (linearColor) color = linear_to_srgb(color);
    // GL postfx use bottom-left pixel coordinates; Vulkan fragment positions are top-down.
    float2 fragCoord = float2(input.position.x, frame.viewportTimeDebug.y - input.position.y);
    float3 post = scene_postfx(color, fragCoord, frame.viewportTimeDebug, materials.postfx, materials.grading.x, overlay);
    return float4(linearColor && surfaceData.parameters.w != 0 ? srgb_to_linear(post) : post, alpha);
}
