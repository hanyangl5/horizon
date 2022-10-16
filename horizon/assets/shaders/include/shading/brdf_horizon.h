#include "../common/common_math.h"
#include "../common/fastmath.hsl"

struct BXDF
{
    float NoV;
    float NoL;
    float VoL;
    float NoM;
    float VoM;
    float XoV;
    float XoL;
    float XoM;
    float YoV;
    float YoL;
    float YoM;
};

void InitBXDF( inout(BXDF) context, float3 N, float3 V, float3 L )
{
	context.NoL = dot(N, L);
	context.NoV = dot(N, V);
	context.VoL = dot(V, L);
	float InvLenH = rsqrt( 2 + 2 * context.VoL );
	context.NoM = saturate( ( context.NoL + context.NoV ) * InvLenH );
	context.VoM = saturate( InvLenH + InvLenH * context.VoL );
	context.NoL = saturate( context.NoL );
	context.NoV = saturate( abs( context.NoV ) + 1e-5 );
	context.XoV = 0.0f;
	context.XoL = 0.0f;
	context.XoM = 0.0f;
	context.YoV = 0.0f;
	context.YoL = 0.0f;
	context.YoM = 0.0f;
}

float3 Diffuse_Lambert(float3 albedo) {
    return albedo * _1DIVPI;
}

float3 Fresnel_Schlick(float3 F0, float LoM) { return F0 + (1.0 - F0) * Pow5(1 - LoM); }

// for isotropic ndf, we provide ggx and gtr

float NDF_GGX(float roughness2, float NoM) { 
	float d = ( NoM * roughness2 - NoM ) * NoM + 1.0;
	return roughness2 * _1DIVPI/ ( d * d );
}

// float NDF_Aniso_GGX()

float Vis_SmithGGXCombined(float rougness2, float NoV, float NoL) 
{
	float Vis_SmithV = NoL * sqrt(NoV * (NoV - NoV * rougness2) + rougness2);
	float Vis_SmithL = NoV * sqrt(NoL * (NoL - NoL * rougness2) + rougness2);
	return 0.5  / (Vis_SmithV + Vis_SmithL);
}

float Vis_Aniso_SmithGGXCombined(float ax, float ay, float NoV, float NoL, float XoV, float XoL, float YoV, float YoL)
{
	float Vis_SmithV = NoL * length(float3(ax * XoV, ay * YoV, NoV));
	float Vis_SmithL = NoV * length(float3(ax * XoL, ay * YoL, NoL));
	return 0.5  / (Vis_SmithV + Vis_SmithL);
}
