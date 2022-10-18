/*
# Copyright Disney Enterprises, Inc.  All rights reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License
# and the following modification to it: Section 6 Trademarks.
# deleted and replaced with:
#
# 6. Trademarks. This License does not grant permission to use the
# trade names, trademarks, service marks, or product names of the
# Licensor and its affiliates, except as required for reproducing
# the content of the NOTICE file.
#
# You may obtain a copy of the License at
# http://www.apache.org/licenses/LICENSE-2.0

https://github.com/wdas/brdf/blob/425f6ef183f3e57c2a351622790ca8ea472ddfe1/src/brdfs/disney.brdf

*/

#include "../common/common_math.h"
#include "../common/fastmath.hsl"
#include "../common/luminance.h"

// struct MaterialProperties_Disney12{
//     float3 baseColor;
//     float metallic;
//     float subsurface;
//     float specular;
//     float roughness;
//     float specularTint;
//     float anisotropic;
//     float sheen;
//     float sheenTint;
//     float clearcoat;
//     float clearcoatGloss;
// };

/*
explaination: 
this brdf coupled smooth and rough mat.subsurface scattering model
consider the refaction incident and outgoing the surface (smooth)
consider the retroreflection increase by mat.roughness at grazing angle(rough)

if fd90 > 1.0, diffuse increase at grazing angle. 
if fd90 > 1.0, diffuse increase at grazing angle.
*/
float3 DiffuseBurley12(float3 albedo, float roughness, float LoH, float NoL, float NoV) {
    float fd90 = 0.5f + 2.0 * roughness * LoH * LoH;
    float f_theta_l = 1.0 + (fd90 - 1.0) * Pow5(1.0 - NoL); // incident light refractiion
    float f_theta_v =  1.0 + (fd90 - 1.0) * Pow5(1.0 - NoV); // outgoing light refraction
    return albedo * _1DIVPI * f_theta_v * f_theta_l;
}

// Based on Hanrahan-Krueger brdf approximation of isotropic bssrdf
// 1.25 scale is used to (roughly) preserve albedo
// Fss90 used to "flatten" retroreflection based on roughness
float3 SubsurfaceBurley12(float3 albedo, float roughness, float NoL, float NoV, float LoH) {
    float f_ss_90 = LoH * LoH * roughness;
    float f_ss = (1.0 + (f_ss_90 - 1.0) * Pow5(1.0 - NoL)) * (1.0 + (f_ss_90 - 1.0) * Pow5(1.0 - NoV));
    float3 ss = albedo * _1DIVPI * 1.25 * (f_ss * (1 / (NoL + NoV) - 0.5) + 0.5);
    return ss;
}

float NDF_GTR_GAMMA(float NoH, float a, float gamma) {
    float k;
    float a2 = a * a;
    if(a == 1.0) {
        k = 1.0;
    } else if(gamma == 1.0 && a != 1.0) {
        k = (a2 - 1.0) / log(a2);
    } else if(gamma != 1.0 && a != 1.0) {
        k = (gamma - 1.0) * (a2 - 1.0) / (1.0 - pow(a2, 1.0 - gamma));
    }
    float denom = pow(1.0 + Pow2(NoH) * (a2 - 1.0), gamma);
    return k / denom;
}

float NDF_GTR1(float NdotH, float a)
{
    if (a >= 1) return 1/_PI;
    float a2 = a*a;
    float t = 1 + (a2-1)*NdotH*NdotH;
    return (a2-1) / (_PI*log(a2)*t);
}

float NDF_GTR2(float NdotH, float a)
{
    float a2 = a*a;
    float t = 1 + (a2-1)*NdotH*NdotH;
    return a2 / (_PI * t*t);
}

float NDF_GTR2_Aniso(float NoH, float XoH, float YoH, float ax, float ay)
{
    return 1 / (_PI * ax*ay * Pow2( Pow2(XoH/ax) + Pow2(YoH/ay) + NoH*NoH ));
}

float Vis_smithG_GGX_Aniso(float NdotV, float VdotX, float VdotY, float ax, float ay)
{
    return 1 / (NdotV + Pow2t( Pow2(VdotX*ax) + Pow2(VdotY*ay) + Pow2(NdotV) ));
}


float3 Sheen_Burley12(float3 albedo, float sheen_tint, float sheen, float3 fresnel) {
    float l = Luminance(albedo); // luminance approx.
    float3 c_tint = l > 0.0 ? albedo / l: float3(1.0, 1.0, 1.0); // normalize lum. to isolate hue+sat
    float3 c_sheen = mix(float3(1), c_tint, sheen_tint);
    return fresnel * sheen * c_sheen * albedo * _1DIVPI;
}

// color baseColor .82 .67 .16
// float metallic 0 1 0
// float subsurface 0 1 0
// float specular 0 1 .5
// float roughness 0 1 .5
// float specularTint 0 1 0
// float anisotropic 0 1 0
// float sheen 0 1 0
// float sheenTint 0 1 .5
// float clearcoat 0 1 0
// float clearcoatGloss 0 1 1


// float smithG_GGX(float NdotV, float alphaG)
// {
//     float a = alphaG*alphaG;
//     float b = NdotV*NdotV;
//     return 1 / (NdotV + Pow2t(a + b - a*b));
// }


// float3 BRDF(MaterialProperties_Disney12 mat, float3 L, float3 V, float3 N, float3 X, float3 Y )
// {
//     float NdotL = dot(N,L);
//     float NdotV = dot(N,V);
//     if (NdotL < 0 || NdotV < 0) return float3(0);

//     float3 H = normalize(L+V);
//     float NdotH = dot(N,H);
//     float LdotH = dot(L,H);

//     float3 Cdlin = mon2lin(mat.baseColor);
//     float Cdlum = .3*Cdlin[0] + .6*Cdlin[1]  + .1*Cdlin[2]; // luminance approx.

//     float3 Ctint = Cdlum > 0 ? Cdlin/Cdlum : float3(1); // normalize lum. to isolate hue+sat
//     float3 Cspec0 = mix(mat.specular*.08*mix(float3(1), Ctint, mat.specularTint), Cdlin, mat.metallic);
//     float3 Csheen = mix(float3(1), Ctint, mat.sheenTint);

//     // Diffuse fresnel - go from 1 at normal incidence to .5 at grazing
//     // and mix in diffuse retro-reflection based on mat.roughness
//     float FL = SchlickFresnel(NdotL), FV = SchlickFresnel(NdotV);
//     float Fd90 = 0.5 + 2 * LdotH*LdotH * mat.roughness;
//     float Fd = mix(1.0, Fd90, FL) * mix(1.0, Fd90, FV);

//     // Based on Hanrahan-Krueger brdf approximation of isotropic bssrdf
//     // 1.25 scale is used to (roughly) preserve albedo
//     // Fss90 used to "flatten" retroreflection based on mat.roughness
//     float Fss90 = LdotH*LdotH*mat.roughness;
//     float Fss = mix(1.0, Fss90, FL) * mix(1.0, Fss90, FV);
//     float ss = 1.25 * (Fss * (1 / (NdotL + NdotV) - .5) + .5);

//     // mat.specular
//     float aspect = Pow2t(1-mat.anisotropic*.9);
//     float ax = max(.001, Pow2(mat.roughness)/aspect);
//     float ay = max(.001, Pow2(mat.roughness)*aspect);
//     float Ds = GTR2_aniso(NdotH, dot(H, X), dot(H, Y), ax, ay);
//     float FH = SchlickFresnel(LdotH);
//     float3 Fs = mix(Cspec0, float3(1), FH);
//     float Gs;
//     Gs  = smithG_GGX_aniso(NdotL, dot(L, X), dot(L, Y), ax, ay);
//     Gs *= smithG_GGX_aniso(NdotV, dot(V, X), dot(V, Y), ax, ay);

//     // mat.sheen
//     float3 Fsheen = FH * mat.sheen * Csheen;

//     // mat.clearcoat (ior = 1.5 -> F0 = 0.04)
//     float Dr = GTR1(NdotH, mix(.1,.001,mat.clearcoatGloss));
//     float Fr = mix(.04, 1.0, FH);
//     float Gr = smithG_GGX(NdotL, .25) * smithG_GGX(NdotV, .25);

//     return ((1/_PI) * mix(Fd, ss, mat.subsurface)*Cdlin + Fsheen)
//         * (1-mat.metallic)
//         + Gs*Fs*Ds + .25*mat.clearcoat*Gr*Fr*Dr;
// }


// // 15 https://zhuanlan.zhihu.com/p/345518461

// // decoupled smooth and rough mat.subsurface scattering
// float3 SubsurfaceBurley15(float3 albedo, float roughness, float NoL, float NoV, float LoM) {
//     // 
//     float f_l = Pow5(1.0 - NoL); // incident light refractiion
//     float f_v = Pow5(1.0 - NoV); // outgoing light refraction
//     float r_r =  2.0 * roughness * LoM * LoM;
//     // lambert
//     float3 f_lambert = albedo * _1DIVPI;

//     // retro-reflection
//     float3 f_retroreflection = f_lambert * r_r * (f_l + f_v + f_l * f_v * (r_r - 1.0));

//     float3 f_d = f_lambert * (1.0 - 0.5 * f_l) * (1.0 - 0.5 * f_v) + f_retroreflection;

//     return f_d;
// }