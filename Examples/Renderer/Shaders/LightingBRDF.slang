// Cookbook PBR.slang / Lighting.slang / Shadows.slang; see ../Cookbook-LICENSE.txt.
#pragma once

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
    uint     Padding0;
    uint     Padding1;
    uint     Padding2;
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

struct PBRLighting
{
    float3 Direct;
    float3 IndirectDiffuse;
    float3 IndirectSpecular;
    float3 Fresnel;
    // Split-sum weight f0 * A + B applied to incident specular radiance.
    float3 SpecularWeight;
};

static const float PBRPi = 3.141592653589793;
// Ray traced reflections stop at the maximum roughness and fade to the prefiltered environment from the start value.
static const float RTReflectionFadeStart = 0.3;
static const float RTReflectionMaxRoughness = 0.4;

// glTF metallic-roughness: Lambert diffuse + GGX, correlated Smith visibility, Schlick Fresnel.
float3 PBRFresnel(float3 F0, float CosTheta)
{
    float W = pow(1.0 - saturate(CosTheta), 5.0);
    return F0 + (float3(1.0) - F0) * W;
}

float3 PBRF0(float3 BaseColor, float Metallic, float Specular = 0.5)
{
    return lerp(float3(0.08 * saturate(Specular)), BaseColor, saturate(Metallic));
}

// Returns BRDF * NdotL. Multiply by incident light radiance and visibility only once.
float3 PBRMetallicRoughness(float3 BaseColor, float Metallic, float Roughness, float3 N, float3 V, float3 L, float3 F0)
{
    float NdotV = saturate(dot(N, V));
    float NdotL = saturate(dot(N, L));
    if (NdotV <= 0.0 || NdotL <= 0.0)
    {
        return float3(0.0);
    }

    float3 H = normalize(V + L);
    float  NdotH = saturate(dot(N, H));
    float  VdotH = saturate(dot(V, H));
    float  Alpha = max(saturate(Roughness) * saturate(Roughness), 0.0014142);
    float  A2 = Alpha * Alpha;
    // This form avoids cancellation for very smooth surfaces when NdotH approaches 1.
    float  DValue = (1.0 - NdotH * NdotH) + NdotH * NdotH * A2;
    float  D = A2 / (PBRPi * DValue * DValue);
    float  Gv = NdotL * sqrt(NdotV * NdotV * (1.0 - A2) + A2);
    float  Gl = NdotV * sqrt(NdotL * NdotL * (1.0 - A2) + A2);
    float  Visibility = 0.5 / (Gv + Gl);
    float3 F = PBRFresnel(F0, VdotH);
    float3 Diffuse = (float3(1.0) - F) * (1.0 - saturate(Metallic)) * BaseColor / PBRPi;
    return (Diffuse + F * D * Visibility) * NdotL;
}

// Irradiance maps in this renderer store the diffuse convolution (E / pi).
float3 PBRIndirectDiffuse(float3 BaseColor, float Metallic, float3 F, float3 DiffuseEnvironment, float AO)
{
    return (float3(1.0) - F) * (1.0 - saturate(Metallic)) * BaseColor * DiffuseEnvironment * saturate(AO);
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

// N,V,L are normalized world directions; L points toward the directional source.
// The renderer converts World irradiance to perpendicular illuminance (lux); HDR assets use an explicit cd/m^2 calibration.
PBRLighting EvaluatePBRLighting(float3 BaseColor, float Metallic, float Roughness, float3 N, float3 V, float3 L, float Visibility, float AO,
                                LightingFrameData Light, LightingBindings Bindings, bool ComputeSpecularIBL = true, float Specular = 0.5)
{
    Metallic = saturate(Metallic);
    Roughness = saturate(Roughness);
    float       NdotV = saturate(dot(N, V));
    float3      F0 = PBRF0(BaseColor, Metallic, Specular);
    PBRLighting Result;
    Result.Fresnel = PBRFresnel(F0, NdotV);
    Result.SpecularWeight = Result.Fresnel;
    Result.Direct = PBRMetallicRoughness(BaseColor, Metallic, Roughness, N, V, L, F0) * saturate(Visibility) *
                    max(Light.LightDirection.w, 0.0) * max(Light.LightColor.rgb, 0.0);
    Result.IndirectDiffuse = 0;
    Result.IndirectSpecular = 0;
    if (Bindings.EnvironmentSampler == 0xffffffffu)
    {
        return Result;
    }
    SamplerState Sampler = SamplerDescriptorHeap[Bindings.EnvironmentSampler];
    if (Bindings.Irradiance != 0xffffffffu)
    {
        TextureCube<float4> DiffuseEnvironment = ResourceDescriptorHeap[Bindings.Irradiance];
        float3              Sky = float3(-N.x, N.y, -N.z);
        float3              Irradiance = DiffuseEnvironment.SampleLevel(Sampler, Sky, 0).rgb;
        Result.IndirectDiffuse =
            PBRIndirectDiffuse(BaseColor, Metallic, Result.Fresnel, Irradiance, AO) * max(Light.EnvironmentIntensity, 0.0);
    }
    if (ComputeSpecularIBL && Bindings.Environment != 0xffffffffu && Bindings.BrdfLut != 0xffffffffu)
    {
        float3              R = reflect(-V, N);
        TextureCube<float4> Environment = ResourceDescriptorHeap[Bindings.Environment];
        Texture2D<float4>   BrdfLut = ResourceDescriptorHeap[Bindings.BrdfLut];
        const float         Lod = Roughness * float(max(Light.EnvironmentMipCount, 1u) - 1u);
        const float3        Radiance = Environment.SampleLevel(Sampler, float3(-R.x, R.y, -R.z), Lod).rgb;
        // The LUT integrates the BRDF over (NdotV, perceptual roughness), so Fresnel and NdotL are not applied again.
        const float2        Brdf = BrdfLut.SampleLevel(Sampler, float2(NdotV, Roughness), 0).rg;
        Result.SpecularWeight = F0 * Brdf.x + float3(Brdf.y);
        Result.IndirectSpecular = Result.SpecularWeight * Radiance * max(Light.EnvironmentIntensity, 0.0);
    }
    return Result;
}
