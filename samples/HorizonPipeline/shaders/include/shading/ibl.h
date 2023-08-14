#include "../common/common_math.h"
#include "brdf_horizon.h"
#include "material_params_defination.hsl"
// float3 ComputeIBL(MaterialProperties mat,
// 	float NoV, float3 N, float3 V, TexCube(float4) iemCubemap, TexCube(float4) pmremCubemap, Tex2D(float4)
// environmentBRDF, SamplerState ibl_sampler)
// {
//     float3 albedo = mat.albedo;
//     float metalness = mat.metallic;
//     float roughness = mat.roughness;

// 	float3 F0 = float3(0.04f, 0.04f, 0.04f);
//     float3 diffuse = (1.0 - metalness) * albedo;
// 	float3 specular = lerp(F0, albedo, metalness);

// 	float3 R = normalize(reflect(-V, N));

// 	float3 irradiance = SampleTexCube(iemCubemap, ibl_sampler, N).rgb;
// 	float3 radiance = SampleLvlTexCube(pmremCubemap, ibl_sampler, R, roughness * 10.0).rgb;
// 	float2 brdf = SampleTex2D(environmentBRDF, ibl_sampler, float2(NoV, roughness)).rg;

// 	return irradiance * diffuse + radiance * (specular * brdf.x + brdf.y);
// }

float3 Irradiance_SphericalHarmonics(float3 sh[9], float3 normal) {
    return max(sh[0] + sh[1] * (normal.y) + sh[2] * (normal.z) + sh[3] * (normal.x) + sh[4] * (normal.y * normal.x) +
                   sh[5] * (normal.y * normal.z) + sh[6] * (3.0 * normal.z * normal.z - 1.0) +
                   sh[7] * (normal.z * normal.x) + sh[8] * (normal.x * normal.x - normal.y * normal.y),
               0.0);
}

float3 fresnelSchlickRoughness(float cosTheta, float3 F0, float roughness) {
    return F0 + (max(float3(1.0 - roughness), F0) - F0) * Pow5(clamp(1.0 - cosTheta, 0.0, 1.0));
}

float3 IBL(float3 sh[9], float3 specular, float2 env, float3 normal, float NoV, MaterialProperties mat) {
    float3 specular_color = (mat.f0 * env.x + env.y) * specular;
    float3 diffuse_color = Irradiance_SphericalHarmonics(sh, normal) * Diffuse_Lambert(mat.albedo);

    float3 f = fresnelSchlickRoughness(NoV, mat.f0, mat.roughness);
    float3 kd = (float3(1.0) - f) * (1.0 - mat.metallic);
    diffuse_color *= kd;
    float3 ibl_color = diffuse_color + specular_color;
    return ibl_color;
}
