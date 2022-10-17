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

    float3 diffuse = kd * Diffuse_Lambert(mat.albedo);

    float3 specular = D * G * F;

    return diffuse + specular;
}

float3 Brdf_Disney12(MaterialProperties_Disney12 mat, BXDF bxdf) {
    float NdotL = bxdf.NoL;
    float NdotV = bxdf.NoV;
    if (NdotL < 0 || NdotV < 0) return float3(0);

    float NdotH = bxdf.NoM;
    float LdotH = bxdf.VoM;

    float3 Cdlin = mon2lin(mat.baseColor);
    float Cdlum = .3*Cdlin[0] + .6*Cdlin[1]  + .1*Cdlin[2]; // luminance approx.

    float3 Ctint = Cdlum > 0 ? Cdlin/Cdlum : float3(1); // normalize lum. to isolate hue+sat
    float3 Cspec0 = mix(mat.specular*.08*mix(float3(1), Ctint, mat.specularTint), Cdlin, mat.metallic);
    float3 Csheen = mix(float3(1), Ctint, mat.sheenTint);

    // Diffuse fresnel - go from 1 at normal incidence to .5 at grazing
    // and mix in diffuse retro-reflection based on mat.roughness
    float FL = SchlickFresnel(NdotL), FV = SchlickFresnel(NdotV);
    float Fd90 = 0.5 + 2 * LdotH*LdotH * mat.roughness;
    float Fd = mix(1.0, Fd90, FL) * mix(1.0, Fd90, FV);

    // Based on Hanrahan-Krueger brdf approximation of isotropic bssrdf
    // 1.25 scale is used to (roughly) preserve albedo
    // Fss90 used to "flatten" retroreflection based on mat.roughness
    float Fss90 = LdotH*LdotH*mat.roughness;
    float Fss = mix(1.0, Fss90, FL) * mix(1.0, Fss90, FV);
    float ss = 1.25 * (Fss * (1 / (NdotL + NdotV) - .5) + .5);

    // mat.specular
    float aspect = sqrt(1-mat.anisotropic*.9);
    float ax = max(.001, sqr(mat.roughness)/aspect);
    float ay = max(.001, sqr(mat.roughness)*aspect);
    float Ds = GTR2_aniso(NdotH, bxdf.XoM, bxdf.YoM, ax, ay);
    float FH = SchlickFresnel(LdotH);
    float3 Fs = mix(Cspec0, float3(1), FH);
    float Gs;
    Gs  = smithG_GGX_aniso(NdotL, bxdf.XoL, bxdf.YoL, ax, ay);
    Gs *= smithG_GGX_aniso(NdotV, bxdf.XoV, bxdf.YoV, ax, ay);

    // mat.sheen
    float3 Fsheen = FH * mat.sheen * Csheen;

    // mat.clearcoat (ior = 1.5 -> F0 = 0.04)
    float Dr = GTR1(NdotH, mix(.1,.001,mat.clearcoatGloss));
    float Fr = mix(.04, 1.0, FH);
    float Gr = smithG_GGX(NdotL, .25) * smithG_GGX(NdotV, .25);

    return ((1/_PI) * mix(Fd, ss, mat.subsurface)*Cdlin + Fsheen)
        * (1-mat.metallic)
        + Gs*Fs*Ds + .25*mat.clearcoat*Gr*Fr*Dr;
}

//Masked()

// SubSurface

// ClearCoat

// Hair

// Skin
// Eye
