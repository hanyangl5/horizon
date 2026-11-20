// Cookbook Math.slang; see ../Cookbook-LICENSE.txt.
#pragma once

static const float M_PI = 3.141592653589793;

float3 ReconstructWorldPosition(float2 UV, float Depth, float4x4 InvViewProj)
{
    float2 Ndc = UV * float2(2.0, -2.0) + float2(-1.0, 1.0);
    float4 World = mul(InvViewProj, float4(Ndc, Depth, 1.0));
    return World.xyz / World.w;
}

float Pow2(float X) { return X * X; }

float3x3 GetTangentBasis(float3 TangentZ)
{
    const float Sign_ = TangentZ.z >= 0 ? 1 : -1;
    const float A = -1.0 / (Sign_ + TangentZ.z);
    const float B = TangentZ.x * TangentZ.y * A;

    float3 TangentX = float3(1 + Sign_ * A * Pow2(TangentZ.x), Sign_ * B, -Sign_ * TangentZ.x);
    float3 TangentY = float3(B, Sign_ + A * Pow2(TangentZ.y), -TangentZ.y);

    return transpose(float3x3(TangentX, TangentY, TangentZ));
}

float4 CosineSampleHemisphere(float2 E)
{
    float Phi = 2.0 * M_PI * E.x;
    float CosTheta = sqrt(E.y);
    float SinTheta = sqrt(1.0 - CosTheta * CosTheta);

    float3 H;
    H.x = SinTheta * cos(Phi);
    H.y = SinTheta * sin(Phi);
    H.z = CosTheta;

    float Pdf = CosTheta * (1.0 / M_PI);

    return float4(H, Pdf);
}

float4 UniformSampleConeRobust(float2 E, float SinThetaMax2)
{
    float Phi = 2.0 * M_PI * E.x;
    float OneMinusCosThetaMax = SinThetaMax2 < 0.01 ? SinThetaMax2 * (0.5 + 0.125 * SinThetaMax2) : 1.0 - sqrt(1.0 - SinThetaMax2);

    float CosTheta = 1.0 - OneMinusCosThetaMax * E.y;
    float SinTheta = sqrt(1.0 - CosTheta * CosTheta);

    float3 L;
    L.x = SinTheta * cos(Phi);
    L.y = SinTheta * sin(Phi);
    L.z = CosTheta;
    float Pdf = 1.0 / (2.0 * M_PI * OneMinusCosThetaMax);

    return float4(L, Pdf);
}

float3 GenerateSoftShadowRayDir(float3 LightDir, float LightRadius, float2 RandSample)
{
    float    SinThetaMax2 = LightRadius * LightRadius;
    float4   ConeSample = UniformSampleConeRobust(RandSample, SinThetaMax2);
    float3x3 Basis = GetTangentBasis(LightDir);
    return normalize(mul(Basis, ConeSample.xyz));
}

float2 ReprojectToPrevUV(float3 WorldPos, float4x4 PrevViewProj)
{
    float4 PrevClip = mul(PrevViewProj, float4(WorldPos, 1.0));
    float2 PrevNDC = PrevClip.xy / PrevClip.w;
    return PrevNDC * float2(0.5, -0.5) + float2(0.5, 0.5);
}

float3 FresnelSchlickRoughness(float CosTheta, float3 F0, float Roughness)
{
    return F0 + (max(float3(1.0 - Roughness), F0) - F0) * pow(clamp(1.0 - CosTheta, 0.0, 1.0), 5.0);
}
