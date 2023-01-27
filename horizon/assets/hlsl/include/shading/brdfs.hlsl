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

#include "../common/common_math.hlsl"
#include "../common/fastmath.hlsl"
#include "../common/luminance.hlsl"

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

// Physically-Based Shading at Disney

float3 DiffuseBurley12(float3 albedo, float roughness, float LoH, float NoL, float NoV) {
    float fd90 = 0.5f + 2.0 * roughness * LoH * LoH;
    float f_theta_l = 1.0 + (fd90 - 1.0) * Pow5(1.0 - NoL); // incident light refractiion
    float f_theta_v = 1.0 + (fd90 - 1.0) * Pow5(1.0 - NoV); // outgoing light refraction
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
    if (a == 1.0) {
        k = 1.0;
    } else if (gamma == 1.0 && a != 1.0) {
        k = (a2 - 1.0) / log(a2);
    } else if (gamma != 1.0 && a != 1.0) {
        k = (gamma - 1.0) * (a2 - 1.0) / (1.0 - pow(a2, 1.0 - gamma));
    }
    float denom = pow(1.0 + Pow2(NoH) * (a2 - 1.0), gamma);
    return k / denom;
}

float NDF_GTR1(float NdotH, float a) {
    if (a >= 1)
        return 1 / _PI;
    float a2 = a * a;
    float t = 1 + (a2 - 1) * NdotH * NdotH;
    return (a2 - 1) / (_PI * log(a2) * t);
}

float NDF_GTR2(float NdotH, float a2) {
    float t = 1 + (a2 - 1) * NdotH * NdotH;
    return a2 / (_PI * t * t);
}

float NDF_GTR2_Aniso(float NoH, float XoH, float YoH, float ax, float ay) {
    return 1 / (_PI * ax * ay * Pow2(Pow2(XoH / ax) + Pow2(YoH / ay) + NoH * NoH));
}

float Vis_Smith_GGX(float NdotV, float alphaG) {
    float a = alphaG * alphaG;
    float b = NdotV * NdotV;
    return 1 / (NdotV + SqrtFast2(a + b - a * b));
}

float Vis_Smith_GGX_Aniso(float NdotV, float VdotX, float VdotY, float ax, float ay) {
    return 1 / (NdotV + SqrtFast2(Pow2(VdotX * ax) + Pow2(VdotY * ay) + Pow2(NdotV)));
}

float3 Sheen_Burley12(float3 albedo, float sheen_tint, float sheen, float f) {
    float l = Luminance(albedo);                                  // luminance approx.
    float3 c_tint = l > 0.0 ? albedo / l : float3(1.0, 1.0, 1.0); // normalize lum. to isolate hue+sat
    float3 c_sheen = mix(float3(1), c_tint, sheen_tint);
    return f * sheen * c_sheen;
}

float3 ClearCoat_Burley12(float NoL, float NoV, float NoH, float clearcoat, float clearcoat_gloss) {
    float Dr = NDF_GTR1(NoH, lerp(0.1, 0.001, clearcoat_gloss));
    float Fr = lerp(0.04, 1.0, Pow5(1.0 - NoH));
    float Gr = Vis_Smith_GGX(NoL, 0.25) * Vis_Smith_GGX(NoV, 0.25);
    return float3(0.25 * clearcoat * Dr * Fr * Gr);
}

// Moving Frostbite to Physically Based Rendering

float3 DiffuseFrostbite14(float3 albedo, float roughness, float LoH, float NoL, float NoV) {
    float linear_roughness = Pow2(roughness);
    float energy_normalization = lerp(1.0, 1.0 / 1.51, linear_roughness);
    return energy_normalization * DiffuseBurley12(albedo, roughness, LoH, NoL, NoV);
}

// Extending the Disney BRDF to a BSDF with Integrated Subsurface Scattering

// decoupled smooth and rough mat.subsurface scattering
float3 SubsurfaceBurley15(float3 albedo, float roughness, float NoL, float NoV, float LoM) {
    //
    float f_l = Pow5(1.0 - NoL); // incident light refractiion
    float f_v = Pow5(1.0 - NoV); // outgoing light refraction
    float r_r = 2.0 * roughness * LoM * LoM;
    // lambert
    float3 f_lambert = albedo * _1DIVPI;

    // retro-reflection
    float3 f_retroreflection = f_lambert * r_r * (f_l + f_v + f_l * f_v * (r_r - 1.0));

    float3 f_d = f_lambert * (1.0 - 0.5 * f_l) * (1.0 - 0.5 * f_v) + f_retroreflection;

    return f_d;
}

// unreal 5.1

// #include "../common/common_math.hlsl"
// #include "../common/fastmath.hlsl"

// float3 Diffuse_Lambert( float3 DiffuseColor )
// {
// 	return DiffuseColor * (1 / PI);
// }

// // [Burley 2012, "Physically-Based Shading at Disney"]
// float3 Diffuse_Burley( float3 DiffuseColor, float Roughness, float NoV, float NoL, float VoH )
// {
// 	float FD90 = 0.5 + 2 * VoH * VoH * Roughness;
// 	float FdV = 1 + (FD90 - 1) * Pow5( 1 - NoV );
// 	float FdL = 1 + (FD90 - 1) * Pow5( 1 - NoL );
// 	return DiffuseColor * ( (1 / PI) * FdV * FdL );
// }

// // [Gotanda 2012, "Beyond a Simple Physically Based Blinn-Phong Model in Real-Time"]
// float3 Diffuse_OrenNayar( float3 DiffuseColor, float Roughness, float NoV, float NoL, float VoH )
// {
// 	float a = Roughness * Roughness;
// 	float s = a;// / ( 1.29 + 0.5 * a );
// 	float s2 = s * s;
// 	float VoL = 2 * VoH * VoH - 1;		// double angle identity
// 	float Cosri = VoL - NoV * NoL;
// 	float C1 = 1 - 0.5 * s2 / (s2 + 0.33);
// 	float C2 = 0.45 * s2 / (s2 + 0.09) * Cosri * ( Cosri >= 0 ? rcp( max( NoL, NoV ) ) : 1 );
// 	return DiffuseColor / PI * ( C1 + C2 ) * ( 1 + Roughness * 0.5 );
// }

// // [Gotanda 2014, "Designing Reflectance Models for New Consoles"]
// float3 Diffuse_Gotanda( float3 DiffuseColor, float Roughness, float NoV, float NoL, float VoH )
// {
// 	float a = Roughness * Roughness;
// 	float a2 = a * a;
// 	float F0 = 0.04;
// 	float VoL = 2 * VoH * VoH - 1;		// double angle identity
// 	float Cosri = VoL - NoV * NoL;
// #if 1
// 	float a2_13 = a2 + 1.36053;
// 	float Fr = ( 1 - ( 0.542026*a2 + 0.303573*a ) / a2_13 ) * ( 1 - pow( 1 - NoV, 5 - 4*a2 ) / a2_13 ) * ( (
// -0.733996*a2*a + 1.50912*a2 - 1.16402*a ) * pow( 1 - NoV, 1 + rcp(39*a2*a2+1) ) + 1 );
// 	//float Fr = ( 1 - 0.36 * a ) * ( 1 - pow( 1 - NoV, 5 - 4*a2 ) / a2_13 ) * ( -2.5 * Roughness * ( 1 - NoV ) + 1
// ); 	float Lm = ( max( 1 - 2*a, 0 ) * ( 1 - Pow5( 1 - NoL ) ) + min( 2*a, 1 ) ) * ( 1 - 0.5*a * (NoL - 1) ) * NoL;
// 	float Vd = ( a2 / ( (a2 + 0.09) * (1.31072 + 0.995584 * NoV) ) ) * ( 1 - pow( 1 - NoL, ( 1 - 0.3726732 * NoV *
// NoV ) / ( 0.188566 + 0.38841 * NoV ) ) ); 	float Bp = Cosri < 0 ? 1.4 * NoV * NoL * Cosri : Cosri; 	float Lr = (21.0
// / 20.0) * (1 - F0) * ( Fr * Lm + Vd + Bp ); 	return DiffuseColor / PI * Lr; #else 	float a2_13 = a2 + 1.36053; 	float Fr
// = ( 1 - ( 0.542026*a2 + 0.303573*a ) / a2_13 ) * ( 1 - pow( 1 - NoV, 5 - 4*a2 ) / a2_13 ) * ( ( -0.733996*a2*a
// + 1.50912*a2 - 1.16402*a ) * pow( 1 - NoV, 1 + rcp(39*a2*a2+1) ) + 1 ); 	float Lm = ( max( 1 - 2*a, 0 ) * ( 1 - Pow5(
// 1 - NoL ) ) + min( 2*a, 1 ) ) * ( 1 - 0.5*a + 0.5*a * NoL ); 	float Vd = ( a2 / ( (a2 + 0.09) * (1.31072 + 0.995584 *
// NoV) ) ) * ( 1 - pow( 1 - NoL, ( 1 - 0.3726732 * NoV * NoV ) / ( 0.188566 + 0.38841 * NoV ) ) ); 	float Bp = Cosri < 0
// ? 1.4 * NoV * Cosri : Cosri / max( NoL, 1e-8 ); 	float Lr = (21.0 / 20.0) * (1 - F0) * ( Fr * Lm + Vd + Bp ); 	return
// DiffuseColor / PI * Lr; #endif
// }

// // [ Chan 2018, "Material Advances in Call of Duty: WWII" ]
// // It has been extended here to fade out retro reflectivity contribution from area light in order to avoid visual
// artefacts. float3 Diffuse_Chan( float3 DiffuseColor, float a2, float NoV, float NoL, float VoH, float NoH, float
// RetroReflectivityWeight)
// {
// 	// We saturate each input to avoid out of range negative values which would result in weird darkening at the
// edge of meshes (resulting from tangent space interpolation). 	NoV = saturate(NoV); 	NoL = saturate(NoL); 	VoH =
// saturate(VoH); 	NoH = saturate(NoH);

// 	// a2 = 2 / ( 1 + exp2( 18 * g )
// 	float g = saturate( (1.0 / 18.0) * log2( 2 / a2 - 1 ) );

// 	float F0 = VoH + Pow5( 1 - VoH );
// 	float FdV = 1 - 0.75 * Pow5( 1 - NoV );
// 	float FdL = 1 - 0.75 * Pow5( 1 - NoL );

// 	// Rough (F0) to smooth (FdV * FdL) response interpolation
// 	float Fd = lerp( F0, FdV * FdL, saturate( 2.2 * g - 0.5 ) );

// 	// Retro reflectivity contribution.
// 	float Fb = ( (34.5 * g - 59 ) * g + 24.5 ) * VoH * exp2( -max( 73.2 * g - 21.2, 8.9 ) * sqrt( NoH ) );
// 	// It fades out when lights become area lights in order to avoid visual artefacts.
// 	Fb *= RetroReflectivityWeight;

// 	return DiffuseColor * ( (1 / PI) * ( Fd + Fb ) );
// }

// // [Blinn 1977, "Models of light reflection for computer synthesized pictures"]
// float D_Blinn( float a2, float NoH )
// {
// 	float n = 2 / a2 - 2;
// 	return (n+2) / (2*PI) * pow(max(abs(NoH),POW_CLAMP),n);		// 1 mad, 1 exp, 1 mul, 1 log
// }

// // [Beckmann 1963, "The scattering of electromagnetic waves from rough surfaces"]
// float D_Beckmann( float a2, float NoH )
// {
// 	float NoH2 = NoH * NoH;
// 	return exp( (NoH2 - 1) / (a2 * NoH2) ) / ( PI * a2 * NoH2 * NoH2 );
// }

// // GGX / Trowbridge-Reitz
// // [Walter et al. 2007, "Microfacet models for refraction through rough surfaces"]
// float D_GGX( float a2, float NoH )
// {
// 	float d = ( NoH * a2 - NoH ) * NoH + 1;	// 2 mad
// 	return a2 / ( PI*d*d );					// 4 mul, 1 rcp
// }

// // Anisotropic GGX
// // [Burley 2012, "Physically-Based Shading at Disney"]
// float D_GGXaniso( float ax, float ay, float NoH, float XoH, float YoH )
// {
// // The two formulations are mathematically equivalent
// #if 1
// 	float a2 = ax * ay;
// 	float3 V = float3(ay * XoH, ax * YoH, a2 * NoH);
// 	float S = dot(V, V);

// 	return (1.0f / PI) * a2 * Pow2(a2 / S);
// #else
// 	float d = XoH*XoH / (ax*ax) + YoH*YoH / (ay*ay) + NoH*NoH;
// 	return 1.0f / ( PI * ax*ay * d*d );
// #endif
// }

// float Vis_Implicit()
// {
// 	return 0.25;
// }

// // [Neumann et al. 1999, "Compact metallic reflectance models"]
// float Vis_Neumann( float NoV, float NoL )
// {
// 	return 1 / ( 4 * max( NoL, NoV ) );
// }

// // [Kelemen 2001, "A microfacet based coupled specular-matte brdf model with importance sampling"]
// float Vis_Kelemen( float VoH )
// {
// 	// constant to prevent NaN
// 	return rcp( 4 * VoH * VoH + 1e-5);
// }

// // Tuned to match behavior of Vis_Smith
// // [Schlick 1994, "An Inexpensive BRDF Model for Physically-Based Rendering"]
// float Vis_Schlick( float a2, float NoV, float NoL )
// {
// 	float k = sqrt(a2) * 0.5;
// 	float Vis_SchlickV = NoV * (1 - k) + k;
// 	float Vis_SchlickL = NoL * (1 - k) + k;
// 	return 0.25 / ( Vis_SchlickV * Vis_SchlickL );
// }

// // Smith term for GGX
// // [Smith 1967, "Geometrical shadowing of a random rough surface"]
// float Vis_Smith( float a2, float NoV, float NoL )
// {
// 	float Vis_SmithV = NoV + sqrt( NoV * (NoV - NoV * a2) + a2 );
// 	float Vis_SmithL = NoL + sqrt( NoL * (NoL - NoL * a2) + a2 );
// 	return rcp( Vis_SmithV * Vis_SmithL );
// }

// // Appoximation of joint Smith term for GGX
// // [Heitz 2014, "Understanding the Masking-Shadowing Function in Microfacet-Based BRDFs"]
// float Vis_SmithJointApprox( float a2, float NoV, float NoL )
// {
// 	float a = sqrt(a2);
// 	float Vis_SmithV = NoL * ( NoV * ( 1 - a ) + a );
// 	float Vis_SmithL = NoV * ( NoL * ( 1 - a ) + a );
// 	return 0.5 * rcp( Vis_SmithV + Vis_SmithL );
// }

// // [Heitz 2014, "Understanding the Masking-Shadowing Function in Microfacet-Based BRDFs"]
// float Vis_SmithJoint(float a2, float NoV, float NoL)
// {
// 	float Vis_SmithV = NoL * sqrt(NoV * (NoV - NoV * a2) + a2);
// 	float Vis_SmithL = NoV * sqrt(NoL * (NoL - NoL * a2) + a2);
// 	return 0.5 * rcp(Vis_SmithV + Vis_SmithL);
// }

// // [Heitz 2014, "Understanding the Masking-Shadowing Function in Microfacet-Based BRDFs"]
// float Vis_SmithJointAniso(float ax, float ay, float NoV, float NoL, float XoV, float XoL, float YoV, float YoL)
// {
// 	float Vis_SmithV = NoL * length(float3(ax * XoV, ay * YoV, NoV));
// 	float Vis_SmithL = NoV * length(float3(ax * XoL, ay * YoL, NoL));
// 	return 0.5 * rcp(Vis_SmithV + Vis_SmithL);
// }

// // [Schlick 1994, "An Inexpensive BRDF Model for Physically-Based Rendering"]
// float3 F_Schlick( float3 SpecularColor, float VoH )
// {
// 	float Fc = Pow5( 1 - VoH );					// 1 sub, 3 mul
// 	//return Fc + (1 - Fc) * SpecularColor;		// 1 add, 3 mad

// 	// Anything less than 2% is physically impossible and is instead considered to be shadowing
// 	return saturate( 50.0 * SpecularColor.g ) * Fc + (1 - Fc) * SpecularColor;
// }

// float3 F_Schlick(float3 F0, float3 F90, float VoH)
// {
// 	float Fc = Pow5(1 - VoH);
// 	return F90 * Fc + (1 - Fc) * F0;
// }

// struct BXDF
// {
//     float NoV;
//     float NoL;
//     float VoL;
//     float NoH;
//     float VoH;
//     float XoV;
//     float XoL;
//     float XoH;
//     float YoV;
//     float YoL;
//     float YoH;
// };

// void InitBXDF( inout(BXDF) context, float3 N, float3 V, float3 L )
// {
// 	context.NoL = dot(N, L);
// 	context.NoV = dot(N, V);
// 	context.VoL = dot(V, L);
// 	float InvLenH = rsqrt( 2 + 2 * context.VoL );
// 	context.NoH = saturate( ( context.NoL + context.NoV ) * InvLenH );
// 	context.VoH = saturate( InvLenH + InvLenH * context.VoL );
// 	context.NoL = saturate( context.NoL );
// 	context.NoV = saturate( abs( context.NoV ) + 1e-5 );
// 	context.XoV = 0.0f;
// 	context.XoL = 0.0f;
// 	context.XoH = 0.0f;
// 	context.YoV = 0.0f;
// 	context.YoL = 0.0f;
// 	context.YoH = 0.0f;
// }

// void InitBXDF( inout(BXDF) context, float3 N, float3 X, float3 Y, float3 V, float3 L )
// {
// 	context.NoL = dot(N, L);
// 	context.NoV = dot(N, V);
// 	context.VoL = dot(V, L);
// 	float InvLenH = rsqrt( 2 + 2 * context.VoL );
// 	context.NoH = saturate( ( context.NoL + context.NoV ) * InvLenH );
// 	context.VoH = saturate( InvLenH + InvLenH * context.VoL );
// 	//NoL = saturate( NoL );
// 	//NoV = saturate( abs( NoV ) + 1e-5 );

// 	context.XoV = dot(X, V);
// 	context.XoL = dot(X, L);
// 	context.XoH = (context.XoL + context.XoV) * InvLenH;
// 	context.YoV = dot(Y, V);
// 	context.YoL = dot(Y, L);
// 	context.YoH = (context.YoL + context.YoV) * InvLenH;
// }