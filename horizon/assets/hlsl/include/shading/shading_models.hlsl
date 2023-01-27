#include "brdf_horizon.hlsl"
#include "brdfs.hlsl"
#include "material_params_defination.hlsl"

// UnLit

// opaque
float3 Brdf_Opaque_Default(MaterialProperties mat, BXDF bxdf) {
    float D = NDF_GGX(mat.roughness2, bxdf.NoM);
    float G = Vis_SmithGGXCombined(mat.roughness2, bxdf.NoV, bxdf.NoL);
    float3 F = Fresnel_Schlick(mat.f0, bxdf.NoM);

    float3 diffuse = Diffuse_Lambert(mat.albedo);

    float3 specular = D * G * F;

    return diffuse + specular;
}

// Masked()

// SubSurface

// ClearCoat

// Hair

// Skin

// Eye

// cloth

float3 Brdf_Standard_Burley12(MaterialProperties mat, BXDF bxdf) {

    // gbuffer save tangent

    // float aspect = SqrtFast2(1 - mat.anisotropic * 0.9);
    // aspect = 1.0;
    // float ax = max(0.001, Pow2(mat.roughness) / aspect);
    // float ay = max(0.001, Pow2(mat.roughness) * aspect);
    // float D = NDF_GTR2_Aniso(mat.roughness2, bxdf.XoM, bxdf.YoM, ax, ay);

    // float G = Vis_Smith_GGX_Aniso(bxdf.NoV, bxdf.XoV, bxdf.YoV, ax, ay) *
    //           Vis_Smith_GGX_Aniso(bxdf.NoL, bxdf.XoL, bxdf.YoL, ax, ay);
    float D = NDF_GGX(mat.roughness2, bxdf.NoM);
    float G = Vis_SmithGGXCombined(mat.roughness2, bxdf.NoV, bxdf.NoL);
    float3 F = Fresnel_Schlick(mat.f0, bxdf.NoM);

    float3 kd = float3(1.0, 1.0, 1.0) - F;
    kd *= 1.0 - mat.metallic;

    float3 diffuse = kd * DiffuseBurley12(mat.albedo, mat.roughness, bxdf.VoM, bxdf.NoL, bxdf.NoV);

    float3 specular = D * G * F;

    return diffuse + specular;
}

float3 Brdf_Subsurface_Burley12(MaterialProperties mat, BXDF bxdf) {

    // float aspect = SqrtFast2(1 - mat.anisotropic * 0.9);
    // float ax = max(0.001, Pow2(mat.roughness) / aspect);
    // float ay = max(0.001, Pow2(mat.roughness) * aspect);
    // float D = NDF_GTR2_Aniso(mat.roughness2, bxdf.XoM, bxdf.YoM, ax, ay);

    // float G = Vis_Smith_GGX_Aniso(bxdf.NoV, bxdf.XoV, bxdf.YoV, ax, ay) *
    //           Vis_Smith_GGX_Aniso(bxdf.NoL, bxdf.XoL, bxdf.YoL, ax, ay);

    float D = NDF_GGX(mat.roughness2, bxdf.NoM);
    float G = Vis_SmithGGXCombined(mat.roughness2, bxdf.NoV, bxdf.NoL);

    float3 F = Fresnel_Schlick(mat.f0, bxdf.NoM);

    float3 kd = float3(1.0, 1.0, 1.0) - F;
    kd *= 1.0 - mat.metallic;

    float3 diffuse =
        kd * (lerp(DiffuseBurley12(mat.albedo, mat.roughness, bxdf.VoM, bxdf.NoL, bxdf.NoV),
                   SubsurfaceBurley12(mat.albedo, mat.roughness, bxdf.NoL, bxdf.NoV, bxdf.VoM), mat.subsurface));

    float3 specular = D * G * F;

    return diffuse + specular;
}

float3 Brdf_Cloth_Burley12(MaterialProperties mat, BXDF bxdf) {

    float3 standard = Brdf_Standard_Burley12(mat, bxdf);

    float f = Pow5(1.0 - bxdf.NoM);

    float3 sheen_color = (1.0 - mat.metallic) * Sheen_Burley12(mat.albedo, mat.sheen_tint, mat.sheen, f);

    return standard + sheen_color;
}

float3 Brdf_ClearCoat_Burley12(MaterialProperties mat, BXDF bxdf) {

    float3 standard = Brdf_Standard_Burley12(mat, bxdf);

    // clearcoat (ior = 1.5 -> F0 = 0.04)
    float3 clearcoat = ClearCoat_Burley12(bxdf.NoL, bxdf.NoV, bxdf.NoM, mat.clearcoat, mat.clearcoat_gloss);

    return standard + clearcoat;
}
