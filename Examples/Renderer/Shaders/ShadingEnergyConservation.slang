// Analytic GGX energy path adapted from Epic's ShadingEnergyConservation.ush
// and ShadingEnergyConservationTemplate.ush.
#pragma once

#include "Math.slang"

struct FBxDFEnergyTerms
{
    float3 W;
    float3 E;
};

float2 GGXEnergyLookup(float Roughness, float NoV)
{
    const float R = Roughness;
    const float E = 1.0 - saturate(pow(R, NoV / max(R, 1e-4)) * ((R * NoV + 0.0266916) / (0.466495 + NoV)));
    const float Ef = Pow5(1.0 - NoV) * pow(2.36651 * pow(NoV, 4.7703 * R) + 0.0387332, R);
    return float2(E, Ef);
}

FBxDFEnergyTerms ComputeGGXSpecEnergyTerms(float Roughness, float NoV, float3 F0)
{
    const float2 E = GGXEnergyLookup(Roughness, NoV);
    const float F90 = saturate(50.0 * max(F0.r, max(F0.g, F0.b)));

    FBxDFEnergyTerms Result;
    Result.W = 1.0 + F0 * ((1.0 - E.x) / E.x);
    Result.E = Result.W * (E.x * F0 + E.y * (F90 - F0));
    return Result;
}

float ComputeEnergyPreservation(FBxDFEnergyTerms EnergyTerms)
{
    return 1.0 - dot(EnergyTerms.E, float3(0.2126390059, 0.7151686788, 0.0721923154));
}

float3 ComputeEnergyConservation(FBxDFEnergyTerms EnergyTerms)
{
    return EnergyTerms.W;
}
