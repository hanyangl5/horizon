#ifndef BRDF_HORIZON_HLSL_H
#define BRDF_HORIZON_HLSL_H

#include "../common/common_math.h"
#include "../common/fastmath.hlsl"
#include "material_params_defination.hlsl"

struct BXDF
{
    float NoV, NoL, VoL, NoM, VoM;
    float XoV, XoL, XoM, YoV, YoL, YoM;
};

void InitBXDF(inout BXDF bxdf, float3 N, float3 V, float3 L)
{
    bxdf.NoL = saturate(dot(N, L));
    bxdf.NoV = saturate(dot(N, V));
    bxdf.VoL = saturate(dot(V, L));
    float3 M = V + L;
    float M2 = dot(M, M);
    M = M2 > 0.0 ? M * rsqrt(M2) : N;
    bxdf.NoM = saturate(dot(N, M));
    bxdf.VoM = saturate(dot(V, M));
}

float3 Diffuse_Lambert(float3 albedo)
{
    return albedo * _1DIVPI;
}

float3 Diffuse_Burley(float3 albedo, float roughness, float NoV, float NoL, float VoH)
{
    float FD90 = 0.5 + 2.0 * VoH * VoH * roughness;
    float FdV = 1.0 + (FD90 - 1.0) * Pow5(1.0 - NoV);
    float FdL = 1.0 + (FD90 - 1.0) * Pow5(1.0 - NoL);
    return albedo * (_1DIVPI * FdV * FdL);
}

float3 Fresnel_Schlick(float3 F0, float LoM)
{
    return F0 + (1.0 - F0) * Pow5(1.0 - saturate(LoM));
}

float NDF_GGX(float roughness2, float NoM)
{
    float d = (NoM * roughness2 - NoM) * NoM + 1.0;
    return roughness2 * _1DIVPI / (d * d);
}

float Vis_SmithGGXCombined(float roughness2, float NoV, float NoL)
{
    float Vis_SmithV = NoL * sqrt(NoV * (NoV - NoV * roughness2) + roughness2);
    float Vis_SmithL = NoV * sqrt(NoL * (NoL - NoL * roughness2) + roughness2);
    return 0.5 / (Vis_SmithV + Vis_SmithL);
}

float Vis_SmithJointApprox(float roughness2, float NoV, float NoL)
{
    float a = sqrt(roughness2);
    float Vis_SmithV = NoL * (NoV * (1.0 - a) + a);
    float Vis_SmithL = NoV * (NoL * (1.0 - a) + a);
    return 0.5 / (Vis_SmithV + Vis_SmithL);
}

float Vis_Aniso_SmithGGXCombined(float ax, float ay, float NoV, float NoL, float XoV, float XoL, float YoV, float YoL)
{
    float Vis_SmithV = NoL * length(float3(ax * XoV, ay * YoV, NoV));
    float Vis_SmithL = NoV * length(float3(ax * XoL, ay * YoL, NoL));
    return 0.5 / (Vis_SmithV + Vis_SmithL);
}

float3 Brdf_Opaque_Default(MaterialProperties mat, BXDF bxdf)
{
    float D = NDF_GGX(mat.roughness2, bxdf.NoM);
    float G = Vis_SmithJointApprox(mat.roughness2, bxdf.NoV, bxdf.NoL);
    float3 F = Fresnel_Schlick(mat.f0, bxdf.VoM);
    float3 kd = (1.0 - F) * (1.0 - mat.metallic);
    float3 diffuse = kd * Diffuse_Burley(mat.albedo, mat.roughness, bxdf.NoV, bxdf.NoL, bxdf.VoM);
    float3 specular = D * G * F;
    return diffuse + specular;
}

#endif
