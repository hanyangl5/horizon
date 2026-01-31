#ifndef HLSL_COMMON_H
#define HLSL_COMMON_H

float3x3 make_f3x3_cols(float3 c0, float3 c1, float3 c2)
{
    return float3x3(c0, c1, c2);
}

bool AnyGreaterThan(float2 a, float2 b) { return any(a > b); }
bool AnyLessThan(float2 a, float2 b) { return any(a < b); }
bool AnyGreaterThan(float2 a, float b) { return any(a > b); }
bool AnyLessThan(float2 a, float b) { return any(a < b); }

float3 fast_min(float3 a, float3 b) { return min(a, b); }
float3 fast_max(float3 a, float3 b) { return max(a, b); }

#endif
