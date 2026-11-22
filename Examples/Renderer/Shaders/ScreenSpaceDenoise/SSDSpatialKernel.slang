// Bilateral position and normal weights adapted from Epic's SSDSignalFramework.ush.
#pragma once

#include "SSDCommon.slang"

float ComputeBilateralWeight(float3 Normal, float3 SampleNormal, float3 Offset, float MaxWorldBlur)
{
    const float PositionWeight = saturate(1.0 - abs(dot(Normal, Offset)) / max(MaxWorldBlur, 1e-6));
    return PositionWeight * Pow4(saturate(dot(Normal, SampleNormal)));
}
