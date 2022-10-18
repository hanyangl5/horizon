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
