#include "Common.slang"

cbuffer RootConstant0
{
    uint frameIndex;
    uint gbuffer0Index;
    uint gbuffer1Index;
    uint gbuffer2Index;
    uint depthIndex;
    uint lightsIndex;
    uint activeLightsIndex;
    uint lightCount;
};

struct PSInput
{
    float4 position: SV_Position;
    float2 uv: TEXCOORD0;
};

PSInput VSMain(uint vertexId : SV_VertexID)
{
    float2  uv = float2((vertexId << 1) & 2, vertexId & 2);
    PSInput output;
    output.position = float4(uv * float2(2.0, -2.0) + float2(-1.0, 1.0), 0.0, 1.0);
    output.uv = uv;
    return output;
}

float3 reconstructWorldPosition(float2 uv, float depth)
{
    StructuredBuffer<FrameData> Frame = ResourceDescriptorHeap[frameIndex];
    float4                      clip = float4(uv * float2(2.0, -2.0) + float2(-1.0, 1.0), depth, 1.0);
    float4                      world = mul(Frame[0].inverseViewProjection, clip);
    return world.xyz / world.w;
}

float distributionGGX(float NoH, float roughness)
{
    float alpha = roughness * roughness;
    float alphaSquared = alpha * alpha;
    float denominator = NoH * NoH * (alphaSquared - 1.0) + 1.0;
    return alphaSquared / max(PI * denominator * denominator, 1e-6);
}

float visibilitySmithGGX(float NoV, float NoL, float roughness)
{
    float alpha = roughness * roughness;
    float alphaSquared = alpha * alpha;
    float lambdaV = NoL * sqrt(max(NoV * NoV * (1.0 - alphaSquared) + alphaSquared, 0.0));
    float lambdaL = NoV * sqrt(max(NoL * NoL * (1.0 - alphaSquared) + alphaSquared, 0.0));
    return 0.5 / max(lambdaV + lambdaL, 1e-6);
}

float3 fresnelSchlick(float VoH, float3 f0)
{
    float factor = pow(1.0 - VoH, 5.0);
    return f0 + (1.0 - f0) * factor;
}

float4 PSMain(PSInput input): SV_Target0
{
    Texture2D<float4>           GBuffer0 = ResourceDescriptorHeap[gbuffer0Index];
    Texture2D<float4>           GBuffer1 = ResourceDescriptorHeap[gbuffer1Index];
    Texture2D<float4>           GBuffer2 = ResourceDescriptorHeap[gbuffer2Index];
    Texture2D<float>            SceneDepth = ResourceDescriptorHeap[depthIndex];
    StructuredBuffer<FrameData> Frame = ResourceDescriptorHeap[frameIndex];
    int2                        pixel = int2(input.position.xy);
    float                       depth = SceneDepth.Load(int3(pixel, 0));
    if (depth >= 1.0)
        return float4(0.0, 0.0, 0.0, 1.0);

    float3 emissive = GBuffer0.Load(int3(pixel, 0)).rgb;
    float4 normalMaterial = GBuffer1.Load(int3(pixel, 0));
    float4 baseColorMetallic = GBuffer2.Load(int3(pixel, 0));
    float3 normal = decodeOctNormal(normalMaterial.rg);
    float2 material = unpackMaterialProperties(normalMaterial.ba);
    float  roughness = material.x;
    float  specular = material.y;
    float3 baseColor = baseColorMetallic.rgb;
    float  metallic = baseColorMetallic.a;

    float3 worldPosition = reconstructWorldPosition(input.uv, depth);
    float3 view = normalize(Frame[0].eye.xyz - worldPosition);
    float  NoV = max(dot(normal, view), 1e-4);
    float3 f0 = lerp((0.08 * specular).xxx, baseColor, metallic);
    float3 color = emissive;
    for (uint i = 0; i < lightCount; ++i)
    {
        ByteAddressBuffer lights = ResourceDescriptorHeap[lightsIndex];
        ByteAddressBuffer active = ResourceDescriptorHeap[activeLightsIndex];
        const uint offset = active.Load(i * 4) * 192;
        // This sample currently shades directional lights; all shapes remain in the scene table.
        if (lights.Load(offset + 184) == 0 || lights.Load(offset + 96) != 0)
            continue;
        // Local +Z points towards a directional source; the light emits along -Z.
        float3 light = normalize(asfloat(lights.Load3(offset + 64)));
        float3 halfVector = normalize(view + light);
        float NoL = saturate(dot(normal, light));
        float NoH = saturate(dot(normal, halfVector));
        float VoH = saturate(dot(view, halfVector));
        float3 fresnel = fresnelSchlick(VoH, f0);
        float distribution = distributionGGX(NoH, roughness);
        float visibility = visibilitySmithGGX(NoV, NoL, roughness);
        float3 specularBrdf = distribution * visibility * fresnel;
        float3 diffuseBrdf = (1.0 - fresnel) * (1.0 - metallic) * baseColor / PI;
        float3 radiance = asfloat(lights.Load3(offset + 100)) * asfloat(lights.Load(offset + 112)) *
                          exp2(asfloat(lights.Load(offset + 128)));
        color += (diffuseBrdf + specularBrdf) * radiance * NoL;
    }

    return float4(color, 1.0);
}
