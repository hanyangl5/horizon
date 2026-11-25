// Cookbook PBR.slang / Lighting.slang / Shadows.slang; see ../Cookbook-LICENSE.txt.
#pragma once

#include "BRDF.slang"

struct LightingFrameData
{
    float4x4 ShadowViewProjection;
    float4   LightDirection;
    float4   LightColor;
    float    EnvironmentIntensity;
    float    ShadowBias;
    uint     ShadowEnabled;
    uint     RTShadowEnabled;
    uint     RTAOEnabled;
    uint     EnvironmentMipCount;
    uint     FrameIndex;
    uint     ShadowFilterRadius;
    // Applied after denoising so the filters average linear occlusion.
    float    RTAOPower;
    uint     LightsIndex;
    uint     LightCount;
    uint     ActiveLightsIndex;
    float4   EnvironmentTint;
    uint     MainDirectionalIndex;
    float    MainDiffuseScale;
    float    MainSpecularScale;
    uint     MainExcludedPasses;
};

struct LightingBindings
{
    uint ShadowMap;
    uint ShadowSampler;
    uint Environment;
    uint Irradiance;
    uint BrdfLut;
    uint EnvironmentSampler;
    uint RtShadowAO;
    uint RtReflection;
};

struct FIndirectLighting
{
    float3 Diffuse;
    float3 Specular;
    float3 SpecularWeight;
};

// Ray traced reflections stop at the maximum roughness and fade to the prefiltered environment from the start value.
static const float RTReflectionFadeStart = 0.3;
static const float RTReflectionMaxRoughness = 0.4;

// glTF metallic-roughness: Lambert diffuse + GGX, correlated Smith visibility, Schlick Fresnel.
float3 PBRFresnel(float3 F0, float CosTheta)
{
    float W = pow(1.0 - saturate(CosTheta), 5.0);
    return F0 + (float3(1.0) - F0) * W;
}

float ShadowVisibility(float3 WorldPosition, LightingFrameData Light, LightingBindings Bindings)
{
    if (Light.ShadowEnabled == 0 || Bindings.ShadowMap == 0xffffffffu || Bindings.ShadowSampler == 0xffffffffu)
    {
        return 1.0;
    }
    const float4 Clip = mul(Light.ShadowViewProjection, float4(WorldPosition, 1));
    if (Clip.w <= 0.0)
    {
        return 1.0;
    }
    const float3 Projected = Clip.xyz / Clip.w;
    const float2 UV = Projected.xy * float2(0.5, -0.5) + 0.5;
    if (any(UV < 0.0) || any(UV > 1.0) || Projected.z < 0.0 || Projected.z > 1.0)
    {
        return 1.0;
    }
    Texture2D<float>       Map = ResourceDescriptorHeap[Bindings.ShadowMap];
    SamplerComparisonState Comparison = SamplerDescriptorHeap[Bindings.ShadowSampler];
    uint                   Width;
    uint                   Height;
    Map.GetDimensions(Width, Height);
    const int Radius = int(min(Light.ShadowFilterRadius, 1u));
    float     Visibility = 0;
    for (int Y = -Radius; Y <= Radius; ++Y)
    {
        for (int X = -Radius; X <= Radius; ++X)
        {
            Visibility += Map.SampleCmpLevelZero(Comparison, UV + float2(X, Y) / float2(Width, Height), Projected.z - Light.ShadowBias);
        }
    }
    return Visibility / float((2 * Radius + 1) * (2 * Radius + 1));
}

FIndirectLighting EvaluateIndirectLighting(float3 DiffuseColor, float3 SpecularColor, float Roughness, float3 N, float3 V, float AO,
                                           LightingFrameData Light, LightingBindings Bindings, bool ComputeSpecularIBL)
{
    const float  NoV = saturate(dot(N, V));
    const float3 Fresnel = PBRFresnel(SpecularColor, NoV);
    FIndirectLighting Lighting;
    Lighting.Diffuse = 0;
    Lighting.Specular = 0;
    Lighting.SpecularWeight = Fresnel;
    if (Bindings.EnvironmentSampler == 0xffffffffu)
    {
        return Lighting;
    }

    const float3 Intensity = max(Light.EnvironmentIntensity, 0.0) * Light.EnvironmentTint.rgb;
    SamplerState Sampler = SamplerDescriptorHeap[Bindings.EnvironmentSampler];
    const bool HasIrradiance = Bindings.Irradiance != 0xffffffffu;
    if (HasIrradiance || Bindings.Environment != 0xffffffffu)
    {
        TextureCube<float4> DiffuseEnvironment = ResourceDescriptorHeap[HasIrradiance ? Bindings.Irradiance : Bindings.Environment];
        const float3        Sky = float3(-N.x, N.y, -N.z);
        const float3        Irradiance = DiffuseEnvironment.SampleLevel(Sampler, Sky, HasIrradiance ? 0 : Light.EnvironmentMipCount - 1).rgb;
        // The irradiance map stores the diffuse convolution (E / pi).
        Lighting.Diffuse = (1.0 - Fresnel) * DiffuseColor * Irradiance * saturate(AO) * Intensity;
    }
    if (ComputeSpecularIBL && Bindings.Environment != 0xffffffffu && Bindings.BrdfLut != 0xffffffffu)
    {
        const float3        R = reflect(-V, N);
        TextureCube<float4> Environment = ResourceDescriptorHeap[Bindings.Environment];
        Texture2D<float4>   BrdfLut = ResourceDescriptorHeap[Bindings.BrdfLut];
        const float         Lod = Roughness * float(max(Light.EnvironmentMipCount, 1u) - 1u);
        const float3        Radiance = Environment.SampleLevel(Sampler, float3(-R.x, R.y, -R.z), Lod).rgb;
        // The LUT already integrates the Fresnel and cosine terms.
        const float2        Brdf = BrdfLut.SampleLevel(Sampler, float2(NoV, Roughness), 0).rg;
        Lighting.SpecularWeight = SpecularColor * Brdf.x + float3(Brdf.y);
        Lighting.Specular = Lighting.SpecularWeight * Radiance * Intensity;
    }
    return Lighting;
}
