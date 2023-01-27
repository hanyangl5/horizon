#include "../common/common_math.hlsl"
#include "../common/fastmath.hlsl"

struct BXDF {
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

void InitBXDF(inout(BXDF) bxdf, float3 N, float3 V, float3 L) {
    bxdf.NoL = saturate(dot(N, L));
    bxdf.NoV = saturate(dot(N, V));
    bxdf.VoL = saturate(dot(V, L));
    float3 M = (V + L) / 2.0;
    bxdf.NoM = saturate(dot(N, M));
    bxdf.VoM = saturate(dot(V, M));
    // bxdf.XoV = 0.0f;
    // bxdf.XoL = 0.0f;
    // bxdf.XoM = 0.0f;
    // bxdf.YoV = 0.0f;
    // bxdf.YoL = 0.0f;
    // bxdf.YoM = 0.0f;
}

float3 Diffuse_Lambert(float3 albedo) { return albedo * _1DIVPI; }

float3 Fresnel_Schlick(float3 F0, float LoM) { return F0 + (1.0 - F0) * Pow5(1 - LoM); }

// for isotropic ndf, we provide ggx and gtr

float NDF_GGX(float roughness2, float NoM) {
    float d = (NoM * roughness2 - NoM) * NoM + 1.0;
    return roughness2 * _1DIVPI / (d * d);
}

// float NDF_Aniso_GGX()

float Vis_SmithGGXCombined(float roughness2, float NoV, float NoL) {
    float Vis_SmithV = NoL * sqrt(NoV * (NoV - NoV * roughness2) + roughness2);
    float Vis_SmithL = NoV * sqrt(NoL * (NoL - NoL * roughness2) + roughness2);
    return 0.5 / (Vis_SmithV + Vis_SmithL);
}

float Vis_Aniso_SmithGGXCombined(float ax, float ay, float NoV, float NoL, float XoV, float XoL, float YoV, float YoL) {
    float Vis_SmithV = NoL * length(float3(ax * XoV, ay * YoV, NoV));
    float Vis_SmithL = NoV * length(float3(ax * XoL, ay * YoL, NoL));
    return 0.5 / (Vis_SmithV + Vis_SmithL);
}
