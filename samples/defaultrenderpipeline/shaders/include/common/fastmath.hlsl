/*
    horizon fast math lib (HLSL)
*/
#ifndef FASTMATH_HLSL
#define FASTMATH_HLSL

#include "common_math.h"

float sqrtIEEEIntApproximation(float x, const int inSqrtConst)
{
    int _x = asint(x);
    _x = inSqrtConst + (_x >> 1);
    return asfloat(_x);
}

float rcpSqrtIEEEIntApproximation(float x, const int inRcpSqrtConst)
{
    int _x = asint(x);
    _x = inRcpSqrtConst - (_x >> 1);
    return asfloat(_x);
}

float rcpSqrtNewtonRaphson(float xHalf, float inRcpX)
{
    return inRcpX * (-xHalf * (inRcpX * inRcpX) + 1.5f);
}

float rcpIEEEIntApproximation(float x, const int inRcpConst)
{
    int _x = asint(x);
    _x = inRcpConst - _x;
    return asfloat(_x);
}

float rcpNewtonRaphson(float x, float inRcpX)
{
    return inRcpX * (-inRcpX * x + 2.0f);
}

float RSqrtFast0(float x) { return rcpSqrtIEEEIntApproximation(x, 0x5f3759df); }

float RSqrtFast1(float x)
{
    float xhalf = 0.5f * x;
    float xRcpSqrt = rcpSqrtIEEEIntApproximation(x, 0x5F375A86);
    xRcpSqrt = rcpSqrtNewtonRaphson(xhalf, xRcpSqrt);
    return xRcpSqrt;
}

float RSqrtFast2(float x)
{
    float xhalf = 0.5f * x;
    float xRcpSqrt = rcpSqrtIEEEIntApproximation(x, 0x5F375A86);
    xRcpSqrt = rcpSqrtNewtonRaphson(xhalf, xRcpSqrt);
    xRcpSqrt = rcpSqrtNewtonRaphson(xhalf, xRcpSqrt);
    return xRcpSqrt;
}

float SqrtFast0(float x) { return sqrtIEEEIntApproximation(x, 0x1FBD1DF5); }
float SqrtFast1(float x) { return x * RSqrtFast1(x); }
float SqrtFast2(float x) { return x * RSqrtFast2(x); }

float RcpFast0(float x) { return rcpIEEEIntApproximation(x, 0x7EF311C2); }
float RcpFast1(float x)
{
    float xRcp = rcpIEEEIntApproximation(x, 0x7EF311C3);
    xRcp = rcpNewtonRaphson(x, xRcp);
    return xRcp;
}
float RcpFast2(float x)
{
    float xRcp = rcpIEEEIntApproximation(x, 0x7EF312AC);
    xRcp = rcpNewtonRaphson(x, xRcp);
    xRcp = rcpNewtonRaphson(x, xRcp);
    return xRcp;
}

float LengthFast0(float3 v) { float l2 = dot(v, v); return SqrtFast0(l2); }
float3 NormalizeFast0(float3 v) { float l2 = dot(v, v); return v * RSqrtFast0(l2); }

float AcosFast(float x)
{
    float _x = abs(x);
    float res = -0.156583f * _x + (0.5 * _PI);
    res *= sqrt(1.0f - _x);
    return (x >= 0) ? res : _PI - res;
}
float2 AcosFast(float2 x) { return float2(AcosFast(x.x), AcosFast(x.y)); }
float3 AcosFast(float3 x) { return float3(AcosFast(x.x), AcosFast(x.y), AcosFast(x.z)); }
float4 AcosFast(float4 x) { return float4(AcosFast(x.x), AcosFast(x.y), AcosFast(x.z), AcosFast(x.w)); }

float AsinFast(float x) { return (0.5 * _PI) - AcosFast(x); }
float2 AsinFast(float2 x) { return float2(AsinFast(x.x), AsinFast(x.y)); }
float3 AsinFast(float3 x) { return float3(AsinFast(x.x), AsinFast(x.y), AsinFast(x.z)); }
float4 AsinFast(float4 x) { return float4(AsinFast(x.x), AsinFast(x.y), AsinFast(x.z), AsinFast(x.w)); }

float AtanFastPos(float x)
{
    float t0 = (x < 1.0f) ? x : 1.0f / x;
    float t1 = t0 * t0;
    float poly = 0.0872929f;
    poly = -0.301895f + poly * t1;
    poly = 1.0f + poly * t1;
    poly = poly * t0;
    return (x < 1.0f) ? poly : (0.5 * _PI) - poly;
}
float AtanFast(float x) { float t0 = AtanFastPos(abs(x)); return (x < 0) ? -t0 : t0; }
float2 AtanFast(float2 x) { return float2(AtanFast(x.x), AtanFast(x.y)); }
float3 AtanFast(float3 x) { return float3(AtanFast(x.x), AtanFast(x.y), AtanFast(x.z)); }
float4 AtanFast(float4 x) { return float4(AtanFast(x.x), AtanFast(x.y), AtanFast(x.z), AtanFast(x.w)); }

float Atan2Fast(float y, float x)
{
    float t0 = max(abs(x), abs(y));
    float t1 = min(abs(x), abs(y));
    float t3 = t1 / t0;
    float t4 = t3 * t3;
    t0 = 0.0872929f;
    t0 = t0 * t4 - 0.301895f;
    t0 = t0 * t4 + 1.0f;
    t3 = t0 * t3;
    t3 = abs(y) > abs(x) ? (0.5 * _PI) - t3 : t3;
    t3 = x < 0 ? _PI - t3 : t3;
    t3 = y < 0 ? -t3 : t3;
    return t3;
}
float2 Atan2Fast(float2 y, float2 x) { return float2(Atan2Fast(y.x, x.x), Atan2Fast(y.y, x.y)); }
float3 Atan2Fast(float3 y, float3 x) { return float3(Atan2Fast(y.x, x.x), Atan2Fast(y.y, x.y), Atan2Fast(y.z, x.z)); }
float4 Atan2Fast(float4 y, float4 x) { return float4(Atan2Fast(y.x, x.x), Atan2Fast(y.y, x.y), Atan2Fast(y.z, x.z), Atan2Fast(y.w, x.w)); }

float AcosFast4(float x)
{
    float x1 = abs(x);
    float x2 = x1 * x1;
    float x3 = x2 * x1;
    float s = -0.2121144f * x1 + 1.5707288f;
    s = 0.0742610f * x2 + s;
    s = -0.0187293f * x3 + s;
    s = sqrt(1.0f - x1) * s;
    return x >= 0.0f ? s : _PI - s;
}
float AsinFast4(float x) { return (0.5 * _PI) - AcosFast4(x); }

#endif
