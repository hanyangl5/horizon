#pragma once

#include "BRDF.slang"
#include "ShadingEnergyConservation.slang"

struct FGBufferData
{
    float3 WorldNormal;
    float3 BaseColor;
    float  Metallic;
    float  Specular;
    float  Roughness;
    float3 DiffuseColor;
    float3 SpecularColor;
};

FGBufferData MakeDefaultLitMaterial(float3 Normal, float3 BaseColor, float Metallic, float Roughness, float Specular)
{
    FGBufferData GBuffer;
    GBuffer.WorldNormal = Normal;
    GBuffer.BaseColor = BaseColor;
    GBuffer.Metallic = saturate(Metallic);
    GBuffer.Specular = Specular;
    GBuffer.Roughness = saturate(Roughness);
    GBuffer.DiffuseColor = BaseColor * (1.0 - GBuffer.Metallic);
    GBuffer.SpecularColor = ComputeF0(Specular, BaseColor, GBuffer.Metallic);
    return GBuffer;
}

struct FDirectLighting
{
    float3 Diffuse;
    float3 Specular;
};

FDirectLighting DefaultLitBxDF(FGBufferData GBuffer, float3 V, float3 L)
{
    BxDFContext Context;
    Init(Context, GBuffer.WorldNormal, V, L);

    FDirectLighting Lighting = {};
    if (Context.NoL > 0.0)
    {
        Lighting.Diffuse = Diffuse_Lambert(GBuffer.DiffuseColor) * Context.NoL;
        Lighting.Specular = SpecularGGX(GBuffer.Roughness, GBuffer.SpecularColor, Context) * Context.NoL;

        const FBxDFEnergyTerms EnergyTerms = ComputeGGXSpecEnergyTerms(GBuffer.Roughness, Context.NoV, GBuffer.SpecularColor);
        Lighting.Diffuse *= ComputeEnergyPreservation(EnergyTerms);
        Lighting.Specular *= ComputeEnergyConservation(EnergyTerms);
    }
    return Lighting;
}


FDirectLighting IntegrateBxDF(FGBufferData GBuffer, float3 V, float3 L, float Falloff)
{
    FDirectLighting Lighting = DefaultLitBxDF(GBuffer, V, L);
    Lighting.Diffuse *= Falloff;
    Lighting.Specular *= Falloff;
    return Lighting;
}
