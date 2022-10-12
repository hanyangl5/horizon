#include "brdf.h"
#include "material_params_defination.hsl"

// UnLit

float3 OpaqueBrdf(MaterialProperties mat, BXDF bxdf) {
    float D = D_GGX(mat.roughness2, bxdf.NoH);
    float G = Vis_SmithJoint(mat.roughness2, bxdf.NoV, bxdf.NoL);
    float f90 = saturate(dot(mat.f0, float3(50.0 * 0.33, 50.0 * 0.33, 50.0 * 0.33)));
    float3 F = F_Schlick(mat.f0, float3(f90, f90, f90), bxdf.NoH);

    float3 FDG = D * G * F;

    float3 kd = (float3(1.0, 1.0, 1.0) - F) * (1.0 - mat.metallic);
    float3 ks = float3(1.0, 1.0, 1.0) - kd;

    float3 diffuse = kd * Diffuse_Lambert(mat.albedo);

    float3 specular = ks * FDG;

    return diffuse + specular;
}

//Masked()

// SubSurface

// ClearCoat

// Hair

// Skin
// Eye
