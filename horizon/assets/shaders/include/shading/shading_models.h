#include "brdf_horizon.h"
#include "brdf_disney.h"
#include "material_params_defination.hsl"

// UnLit

// opaque
float3 Brdf_Opaque_Default(MaterialProperties mat, BXDF bxdf) {
    float D = NDF_GGX(mat.roughness2, bxdf.NoM);
    float G = Vis_SmithGGXCombined(mat.roughness2, bxdf.NoV, bxdf.NoL);
    float3 F = Fresnel_Schlick(mat.f0, bxdf.NoM);

    float3 kd = float3(1.0, 1.0, 1.0) - F;
    kd *= 1.0 - mat.metallic;

    float3 diffuse = kd * Diffuse_Lambert(mat.albedo);

    float3 specular = D * G * F;

    return diffuse + specular;
}

//Masked()

// SubSurface

// ClearCoat

// Hair

// Skin

// Eye

// cloth

float3 Brdf_Sheen_Burley12(MaterialProperties mat, BXDF bxdf) {
    float sheen_tint = 0.5, sheen = 0.5;
    float D = NDF_GGX(mat.roughness2, bxdf.NoM);
    float G = Vis_SmithGGXCombined(mat.roughness2, bxdf.NoV, bxdf.NoL);
    float3 F = Fresnel_Schlick(mat.f0, bxdf.NoM);

    float3 kd = float3(1.0, 1.0, 1.0) - F;
    kd *= 1.0 - mat.metallic;

    float3 diffuse = kd * (DiffuseBurley12(mat.albedo, mat.roughness, bxdf.VoM, bxdf.NoL, bxdf.NoV) + Sheen_Burley12(mat.albedo, sheen_tint, sheen, F));

    float3 specular = D * G * F;

    return diffuse + specular;
}

float3 Brdf_Subsurface_Burley12(MaterialProperties mat, BXDF bxdf) {
    float subsurface = 0.5;
    float D = NDF_GGX(mat.roughness2, bxdf.NoM);
    float G = Vis_SmithGGXCombined(mat.roughness2, bxdf.NoV, bxdf.NoL);
    float3 F = Fresnel_Schlick(mat.f0, bxdf.NoM);

    float3 kd = float3(1.0, 1.0, 1.0) - F;
    kd *= 1.0 - mat.metallic;

    float3 diffuse = kd * (lerp(DiffuseBurley12(mat.albedo, mat.roughness, bxdf.VoM, bxdf.NoL, bxdf.NoV), SubsurfaceBurley12(mat.albedo, mat.roughness, bxdf.NoL, bxdf.NoV, bxdf.VoM), subsurface));

    float3 specular = D * G * F;

    return diffuse + specular;
}

float3 Brdf_ClearCoat_Burley12(MaterialProperties mat, BXDF bxdf) {
    float D = NDF_GGX(mat.roughness2, bxdf.NoM);
    float G = Vis_SmithGGXCombined(mat.roughness2, bxdf.NoV, bxdf.NoL);
    float3 F = Fresnel_Schlick(mat.f0, bxdf.NoM);

    float3 kd = float3(1.0, 1.0, 1.0) - F;
    kd *= 1.0 - mat.metallic;

    float3 diffuse = kd * DiffuseBurley12(mat.albedo, mat.roughness, bxdf.VoM, bxdf.NoL, bxdf.NoV);

    float3 specular = D * G * F;

    return diffuse + specular;
}
