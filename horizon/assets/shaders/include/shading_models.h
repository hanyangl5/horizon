#include "brdf.h"
#include "material_params_defination.hsl"

// UnLit

float3 OpaqueBrdf(MaterialProperties mat, BXDF bxdf) {
    float lighting;    

    float D = D_GGX(mat.roughness2, bxdf.NoH);
    float G = Vis_SmithJoint(mat.roughness2, bxdf.NoV, bxdf.NoL);
    float3 F = F_Schlick(mat.f0, float3(1.0, 1.0, 1.0), bxdf.NoH);

    float3 FDG = D * G * F;

    float3 kd = (float3(1.0, 1.0, 1.0) - F) * (1.0 - mat.metallic);
    float3 ks = float3(1.0, 1.0, 1.0) - kd;

    float3 diffuse = kd * Diffuse_Lambert(mat.albedo);

    float3 specular = ks * FDG / (4.0 * bxdf.NoL * bxdf.NoV + 0.0001);

    return diffuse;
    //   +specular;
}

//Masked()

// SubSurface

// ClearCoat

// Hair

// Skin
// Eye
