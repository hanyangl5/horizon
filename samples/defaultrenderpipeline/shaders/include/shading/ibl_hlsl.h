#ifndef IBL_HLSL_H
#define IBL_HLSL_H

#include "../common/common_math.h"
#include "brdf_horizon_hlsl.h"
#include "material_params_defination.hlsl"

struct DiffuseIrradianceSH3
{
    float3 sh[9];
};

float3 Irradiance_SphericalHarmonics(DiffuseIrradianceSH3 ibl, float3 normal)
{
    return max(ibl.sh[0] + ibl.sh[1] * normal.y + ibl.sh[2] * normal.z + ibl.sh[3] * normal.x +
                   ibl.sh[4] * (normal.y * normal.x) + ibl.sh[5] * (normal.y * normal.z) +
                   ibl.sh[6] * (3.0 * normal.z * normal.z - 1.0) + ibl.sh[7] * (normal.z * normal.x) +
                   ibl.sh[8] * (normal.x * normal.x - normal.y * normal.y),
               0.0);
}

float3 fresnelSchlickRoughness(float cosTheta, float3 F0, float roughness)
{
    return F0 + (max(float3(1.0 - roughness, 1.0 - roughness, 1.0 - roughness), F0) - F0) *
                    Pow5(clamp(1.0 - cosTheta, 0.0, 1.0));
}

float3 IBL(DiffuseIrradianceSH3 ibl, float3 specular, float2 env, float3 normal, float NoV, MaterialProperties mat)
{
    float3 specular_color = (mat.f0 * env.x + env.y) * specular;
    float3 diffuse_color = Irradiance_SphericalHarmonics(ibl, normal) * Diffuse_Lambert(mat.albedo);
    float3 f = fresnelSchlickRoughness(NoV, mat.f0, mat.roughness);
    float3 kd = (float3(1.0, 1.0, 1.0) - f) * (1.0 - mat.metallic);
    diffuse_color *= kd;
    return diffuse_color + specular_color;
}

#endif
