// Cookbook PBR.slang / Lighting.slang / Shadows.slang; see ../Cookbook-LICENSE.txt.
#pragma once

struct LightingFrameData
{
    float4x4 shadowViewProjection;
    float4   lightDirection;
    float4   lightColor;
    float    environmentIntensity;
    float    shadowBias;
    uint     shadowEnabled;
    uint     rtShadowEnabled;
    uint     rtAOEnabled;
    uint     environmentMipCount;
    uint     frameIndex;
    uint     shadowFilterRadius;
};

struct LightingBindings
{
    uint shadowMap;
    uint shadowSampler;
    uint environment;
    uint irradiance;
    uint brdfLut;
    uint environmentSampler;
    uint rtShadowAO;
    uint rtReflection;
};

struct PBRLighting
{
    float3 direct;
    float3 indirectDiffuse;
    float3 indirectSpecular;
    float3 fresnel;
};

static const float kPbrPi = 3.141592653589793;

// glTF metallic-roughness: Lambert diffuse + GGX, correlated Smith visibility, Schlick Fresnel.
float3 pbrFresnel(float3 f0, float cosTheta)
{
    float w = pow(1.0 - saturate(cosTheta), 5.0);
    return f0 + (float3(1.0) - f0) * w;
}

float3 pbrF0(float3 baseColor, float metallic, float specular = 0.5)
{
    return lerp(float3(0.08 * saturate(specular)), baseColor, saturate(metallic));
}

// Returns BRDF * NdotL. Multiply by incident light radiance and visibility only once.
float3 pbrMetallicRoughness(float3 baseColor, float metallic, float roughness, float3 N, float3 V, float3 L, float3 f0)
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
    float  alpha = max(saturate(roughness) * saturate(roughness), 0.0014142);
    float  a2 = alpha * alpha;
    // This form avoids cancellation for very smooth surfaces when NdotH approaches 1.
    float  d = (1.0 - NdotH * NdotH) + NdotH * NdotH * a2;
    float  D = a2 / (kPbrPi * d * d);
    float  gv = NdotL * sqrt(NdotV * NdotV * (1.0 - a2) + a2);
    float  gl = NdotV * sqrt(NdotL * NdotL * (1.0 - a2) + a2);
    float  visibility = 0.5 / (gv + gl);
    float3 F = pbrFresnel(f0, VdotH);
    float3 diffuse = (float3(1.0) - F) * (1.0 - saturate(metallic)) * baseColor / kPbrPi;
    return (diffuse + F * D * visibility) * NdotL;
}

// Irradiance maps in this renderer store the diffuse convolution (E / pi).
float3 pbrIndirectDiffuse(float3 baseColor, float metallic, float3 F, float3 diffuseEnvironment, float ao)
{
    return (float3(1.0) - F) * (1.0 - saturate(metallic)) * baseColor * diffuseEnvironment * saturate(ao);
}

// Split-sum GGX IBL: the LUT integrates the BRDF, so do not multiply by Fresnel or NdotL again.
// brdf.xy = scale/bias sampled at (NdotV, perceptual roughness).
float3 pbrIndirectSpecular(float3 f0, float2 brdf, float3 prefilteredEnvironment)
{
    return prefilteredEnvironment * (f0 * brdf.x + float3(brdf.y));
}

float shadowVisibility(float3 worldPosition, LightingFrameData light, LightingBindings bindings)
{
    if (light.shadowEnabled == 0 || bindings.shadowMap == 0xffffffffu || bindings.shadowSampler == 0xffffffffu)
    {
        return 1.0;
    }
    const float4 clip = mul(light.shadowViewProjection, float4(worldPosition, 1));
    if (clip.w <= 0.0)
    {
        return 1.0;
    }
    const float3 projected = clip.xyz / clip.w;
    const float2 uv = projected.xy * float2(0.5, -0.5) + 0.5;
    if (any(uv < 0.0) || any(uv > 1.0) || projected.z < 0.0 || projected.z > 1.0)
    {
        return 1.0;
    }
    Texture2D<float>       map = ResourceDescriptorHeap[bindings.shadowMap];
    SamplerComparisonState comparison = SamplerDescriptorHeap[bindings.shadowSampler];
    uint                   width;
    uint                   height;
    map.GetDimensions(width, height);
    const int radius = int(min(light.shadowFilterRadius, 1u));
    float     visibility = 0;
    for (int y = -radius; y <= radius; ++y)
    {
        for (int x = -radius; x <= radius; ++x)
        {
            visibility += map.SampleCmpLevelZero(comparison, uv + float2(x, y) / float2(width, height), projected.z - light.shadowBias);
        }
    }
    return visibility / float((2 * radius + 1) * (2 * radius + 1));
}

// N,V,L are normalized world directions; L points toward the directional source.
// The renderer converts World irradiance to perpendicular illuminance (lux); HDR assets use an explicit cd/m^2 calibration.
PBRLighting evaluatePBRLighting(float3 baseColor, float metallic, float roughness, float3 N, float3 V, float3 L, float visibility, float ao,
                                LightingFrameData light, LightingBindings bindings, bool computeSpecularIBL = true, float specular = 0.5)
{
    metallic = saturate(metallic);
    roughness = saturate(roughness);
    float       NdotV = saturate(dot(N, V));
    float3      f0 = pbrF0(baseColor, metallic, specular);
    PBRLighting result;
    result.fresnel = pbrFresnel(f0, NdotV);
    result.direct = pbrMetallicRoughness(baseColor, metallic, roughness, N, V, L, f0) * saturate(visibility) *
                    max(light.lightDirection.w, 0.0) * max(light.lightColor.rgb, 0.0);
    result.indirectDiffuse = 0;
    result.indirectSpecular = 0;
    if (bindings.environmentSampler == 0xffffffffu)
    {
        return result;
    }
    SamplerState sampler = SamplerDescriptorHeap[bindings.environmentSampler];
    if (bindings.irradiance != 0xffffffffu)
    {
        TextureCube<float4> diffuseEnvironment = ResourceDescriptorHeap[bindings.irradiance];
        float3              sky = float3(-N.x, N.y, -N.z);
        float3              irradiance = diffuseEnvironment.SampleLevel(sampler, sky, 0).rgb;
        result.indirectDiffuse =
            pbrIndirectDiffuse(baseColor, metallic, result.fresnel, irradiance, ao) * max(light.environmentIntensity, 0.0);
    }
    if (computeSpecularIBL && bindings.environment != 0xffffffffu && bindings.brdfLut != 0xffffffffu)
    {
        float3              r = reflect(-V, N);
        TextureCube<float4> environment = ResourceDescriptorHeap[bindings.environment];
        Texture2D<float4>   brdfLut = ResourceDescriptorHeap[bindings.brdfLut];
        const float         lod = roughness * float(max(light.environmentMipCount, 1u) - 1u);
        const float3        radiance = environment.SampleLevel(sampler, float3(-r.x, r.y, -r.z), lod).rgb;
        const float2        brdf = brdfLut.SampleLevel(sampler, float2(NdotV, roughness), 0).rg;
        result.indirectSpecular = pbrIndirectSpecular(f0, brdf, radiance) * max(light.environmentIntensity, 0.0);
    }
    return result;
}
