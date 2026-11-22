// Cookbook Math.slang; see ../Cookbook-LICENSE.txt.
#pragma once

#include "FastMath.slang"

static const float PI = 3.141592653589793;
static const float PI_HALF = PI * 0.5;
static const float TWO_PI = PI * 2.0;
static const float PowClamp = 0.000001f;

float3 ReconstructWorldPosition(float2 UV, float Depth, float4x4 InvViewProj)
{
    float2 Ndc = UV * float2(2.0, -2.0) + float2(-1.0, 1.0);
    float4 World = mul(InvViewProj, float4(Ndc, Depth, 1.0));
    return World.xyz / World.w;
}


float2 ReprojectToPrevUV(float3 WorldPos, float4x4 PrevViewProj)
{
    float4 PrevClip = mul(PrevViewProj, float4(WorldPos, 1.0));
    float2 PrevNDC = PrevClip.xy / PrevClip.w;
    return PrevNDC * float2(0.5, -0.5) + float2(0.5, 0.5);
}


// Standalone utility math from Epic's Common.ush.
float FmodFloor(float Lhs, float Rhs)
{
    return Lhs - floor(Lhs / Rhs) * Rhs;
}

float2 FmodFloor(float2 Lhs, float2 Rhs)
{
    return Lhs - floor(Lhs / Rhs) * Rhs;
}

float3 FmodFloor(float3 Lhs, float3 Rhs)
{
    return Lhs - floor(Lhs / Rhs) * Rhs;
}

float4 FmodFloor(float4 Lhs, float4 Rhs)
{
    return Lhs - floor(Lhs / Rhs) * Rhs;
}

float VectorSum(float V) { return V; }

float VectorSum(float2 V) { return V.x + V.y; }

float VectorSum(float3 V) { return V.x + V.y + V.z; }

float VectorSum(float4 V) { return V.x + V.y + V.z + V.w; }

float length2(float2 v)
{
    return dot(v, v);
}

float length2(float3 v)
{
    return dot(v, v);
}

float length2(float4 v)
{
    return dot(v, v);
}

uint Mod(uint a, uint b)
{
    return a % b;
}

uint2 Mod(uint2 a, uint2 b)
{
    return a % b;
}

uint3 Mod(uint3 a, uint3 b)
{
    return a % b;
}

float ClampedPow(float X,float Y)
{
    return pow(max(abs(X),PowClamp),Y);
}

float2 ClampedPow(float2 X,float2 Y)
{
    return pow(max(abs(X),float2(PowClamp,PowClamp)),Y);
}

float3 ClampedPow(float3 X,float3 Y)
{
    return pow(max(abs(X),float3(PowClamp,PowClamp,PowClamp)),Y);
}

float4 ClampedPow(float4 X,float4 Y)
{
    return pow(max(abs(X),float4(PowClamp,PowClamp,PowClamp,PowClamp)),Y);
}

float PositiveClampedPow(float Base, float Exponent)
{
    return (Base <= 2.980233e-8) ? 0.0f : pow(Base, Exponent);
}

float2 PositiveClampedPow(float2 Base, float2 Exponent)
{
    return float2(PositiveClampedPow(Base.x, Exponent.x), PositiveClampedPow(Base.y, Exponent.y));
}

float3 PositiveClampedPow(float3 Base, float3 Exponent)
{
    return float3(PositiveClampedPow(Base.xy, Exponent.xy), PositiveClampedPow(Base.z, Exponent.z));
}

float4 PositiveClampedPow(float4 Base, float4 Exponent)
{
    return float4(PositiveClampedPow(Base.xy, Exponent.xy), PositiveClampedPow(Base.zw, Exponent.zw));
}

float PhongShadingPow(float X, float Y)
{
    // The following clamping is done to prevent NaN being the result of the specular power computation.
    // Clamping has a minor performance cost.

    // In HLSL pow(a, b) is implemented as exp2(log2(a) * b).

    // For a=0 this becomes exp2(-inf * 0) = exp2(NaN) = NaN.

    // As seen in #TTP 160394 "QA Regression: PS3: Some maps have black pixelated artifacting."
    // this can cause severe image artifacts (problem was caused by specular power of 0, lightshafts propagated this to other pixels).
    // The problem appeared on PlayStation 3 but can also happen on similar PC NVidia hardware.

    // In order to avoid platform differences and rarely occuring image atrifacts we clamp the base.

    // Note: Clamping the exponent seemed to fix the issue mentioned TTP but we decided to fix the root and accept the
    // minor performance cost.

    return ClampedPow(X, Y);
}

float ConvertTangentUnormToSnorm8(float Input)
{
    int IntVal = int(round(Input * 255.0f));
    //negate
    IntVal = select(IntVal > 127, IntVal | 0xFFFFFF80, IntVal);
    return clamp(IntVal / 127.0f, -1, 1);
}

float2 ConvertTangentUnormToSnorm8(float2 Input)
{
    int2 IntVal = int2(round(Input * 255.0f));
    //negate
    IntVal = select(IntVal > 127, IntVal | 0xFFFFFF80, IntVal);
    return clamp(IntVal / 127.0f, -1, 1);
}

float3 ConvertTangentUnormToSnorm8(float3 Input)
{
    int3 IntVal = int3(round(Input * 255.0f));
    IntVal = select(IntVal > 127, IntVal | 0xFFFFFF80, IntVal);
    return clamp(IntVal / 127.0f, -1, 1);
}

float4 ConvertTangentUnormToSnorm8(float4 Input)
{
    int4 IntVal = int4(round(Input * 255.0f));
    //negate
    IntVal = select(IntVal > 127, IntVal | 0xFFFFFF80, IntVal);
    return clamp(IntVal / 127.0f, -1, 1);
}

float ConvertTangentUnormToSnorm16(float Input)
{
    int IntVal = int(round(Input * 65535.0f));
    //negate
    IntVal = select(IntVal > 32767, IntVal | 0xFFFF8000, IntVal);
    return clamp(IntVal / 32767.0f, -1, 1);
}

float2 ConvertTangentUnormToSnorm16(float2 Input)
{
    int2 IntVal = int2(round(Input * 65535.0f));
    //negate
    IntVal = select(IntVal > 32767, IntVal | 0xFFFF8000, IntVal);
    return clamp(IntVal / 32767.0f, -1, 1);
}

float3 ConvertTangentUnormToSnorm16(float3 Input)
{
    int3 IntVal = int3(round(Input * 65535.0f));
    IntVal = select(IntVal > 32767, IntVal | 0xFFFF8000, IntVal);
    return clamp(IntVal / 32767.0f, -1, 1);
}

float4 ConvertTangentUnormToSnorm16(float4 Input)
{
    int4 IntVal = int4(round(Input * 65535.0f));
    //negate
    IntVal = select(IntVal > 32767, IntVal | 0xFFFF8000, IntVal);
    return clamp(IntVal / 32767.0f, -1, 1);
}

float ConvertTangentSnormToUnorm8(float Input)
{
    float Res = select(Input >= 0.0f, Input * 127, ((Input + 1.0) * 127) + 128);
    return clamp(Res / 255, 0.0f, 0.99f);
}

float2 ConvertTangentSnormToUnorm8(float2 Input)
{
    float2 Res = select(Input >= 0.0f, Input * 127, ((Input + 1.0) * 127) + 128);
    return clamp(Res / 255, 0.0f, 0.99f);
}

float3 ConvertTangentSnormToUnorm8(float3 Input)
{
    float3 Res = select(Input >= 0.0f, Input * 127, ((Input + 1.0) * 127) + 128);
    return clamp(Res / 255, 0.0f, 0.99f);
}

float4 ConvertTangentSnormToUnorm8(float4 Input)
{
    float4 Res = select(Input >= 0.0f, Input * 127, ((Input + 1.0) * 127) + 128);
    return clamp(Res / 255, 0.0f, 0.99f);
}

float ConvertTangentSnormToUnorm16(float Input)
{
    float Res = select(Input >= 0.0f, Input * 32767, ((Input + 1.0) * 32767) + 32768);
    return clamp(Res / 65535, 0.0f, 0.99f);
}

float2 ConvertTangentSnormToUnorm16(float2 Input)
{
    float2 Res = select(Input >= 0.0f, Input * 32767, ((Input + 1.0) * 32767) + 32768);
    return clamp(Res / 65535, 0.0f, 0.99f);
}

float3 ConvertTangentSnormToUnorm16(float3 Input)
{
    float3 Res = select(Input >= 0.0f, Input * 32767, ((Input + 1.0) * 32767) + 32768);
    return clamp(Res / 65535, 0.0f, 0.99f);
}

float4 ConvertTangentSnormToUnorm16(float4 Input)
{
    float4 Res = select(Input >= 0.0f, Input * 32767, ((Input + 1.0) * 32767) + 32768);
    return clamp(Res / 65535, 0.0f, 0.99f);
}

float Square( float x )
{
    return x*x;
}

float2 Square( float2 x )
{
    return x*x;
}

float3 Square( float3 x )
{
    return x*x;
}

float4 Square( float4 x )
{
    return x*x;
}

float Pow2( float x )
{
    return x*x;
}

float2 Pow2( float2 x )
{
    return x*x;
}

float3 Pow2( float3 x )
{
    return x*x;
}

float4 Pow2( float4 x )
{
    return x*x;
}

float Pow3( float x )
{
    return x*x*x;
}

float2 Pow3( float2 x )
{
    return x*x*x;
}

float3 Pow3( float3 x )
{
    return x*x*x;
}

float4 Pow3( float4 x )
{
    return x*x*x;
}

float Pow4( float x )
{
    float xx = x*x;
    return xx * xx;
}

float2 Pow4( float2 x )
{
    float2 xx = x*x;
    return xx * xx;
}

float3 Pow4( float3 x )
{
    float3 xx = x*x;
    return xx * xx;
}

float4 Pow4( float4 x )
{
    float4 xx = x*x;
    return xx * xx;
}

float Pow5( float x )
{
    float xx = x*x;
    return xx * xx * x;
}

float2 Pow5( float2 x )
{
    float2 xx = x*x;
    return xx * xx * x;
}

float3 Pow5( float3 x )
{
    float3 xx = x*x;
    return xx * xx * x;
}

float4 Pow5( float4 x )
{
    float4 xx = x*x;
    return xx * xx * x;
}

float Pow6( float x )
{
    float xx = x*x;
    return xx * xx * xx;
}

float2 Pow6( float2 x )
{
    float2 xx = x*x;
    return xx * xx * xx;
}

float3 Pow6( float3 x )
{
    float3 xx = x*x;
    return xx * xx * xx;
}

float4 Pow6( float4 x )
{
    float4 xx = x*x;
    return xx * xx * xx;
}

float AtanFast( float x )
{
    // Minimax 3 approximation
    float3 A = x < 1 ? float3( x, 0, 1 ) : float3( 1/x, 0.5 * PI, -1 );
    return A.y + A.z * ( ( ( -0.130234 * A.x - 0.0954105 ) * A.x + 1.00712 ) * A.x - 0.00001203333 );
}

bool IsOrthoProjection(float4x4 ViewToClip)
{
    return ViewToClip._44 >= 1.0f;
}

// Screen helpers with the view parameters supplied by the caller.
float2 ViewportUVToScreenPos(float2 ViewportUV)
{
    return float2(2.0 * ViewportUV.x - 1.0, 1.0 - 2.0 * ViewportUV.y);
}

float2 ScreenPosToViewportUV(float2 ScreenPos)
{
    return float2(0.5 + 0.5 * ScreenPos.x, 0.5 - 0.5 * ScreenPos.y);
}

float2 SvPositionToViewportUV(float4 SvPosition, float2 ViewRectMin, float2 InvViewSize)
{
    return (SvPosition.xy - ViewRectMin) * InvViewSize;
}

float2 BufferUVToViewportUV(float2 BufferUV, float2 BufferSize, float2 ViewRectMin, float2 InvViewSize)
{
    return (BufferUV * BufferSize - ViewRectMin) * InvViewSize;
}

float2 ViewportUVToBufferUV(float2 ViewportUV, float2 ViewSize, float2 ViewRectMin, float2 InvBufferSize)
{
    return (ViewportUV * ViewSize + ViewRectMin) * InvBufferSize;
}

float ConvertFromDeviceZ(float DeviceZ, float4 InvDeviceZToWorldZTransform)
{
    return DeviceZ * InvDeviceZToWorldZTransform.x + InvDeviceZToWorldZTransform.y +
           rcp(DeviceZ * InvDeviceZToWorldZTransform.z - InvDeviceZToWorldZTransform.w);
}

float ConvertToDeviceZ(float SceneDepth, float4x4 ViewToClip, float4 InvDeviceZToWorldZTransform)
{
    if (IsOrthoProjection(ViewToClip))
    {
        // Horizon transforms column vectors.
        return SceneDepth * ViewToClip[2][2] + ViewToClip[2][3];
    }
    return rcp((SceneDepth + InvDeviceZToWorldZTransform.w) * InvDeviceZToWorldZTransform.z);
}

float2 GetScreenPositionForProjectionType(float2 ScreenPosition, float SceneDepth, bool Orthographic)
{
    return Orthographic ? ScreenPosition : ScreenPosition * SceneDepth;
}

float3 ScreenToViewPos(float2 ViewportUV, float SceneDepth, float4 ScreenToViewSpace, bool Orthographic)
{
    const float2 Projected = ViewportUV * ScreenToViewSpace.xy + ScreenToViewSpace.zw;
    return float3(GetScreenPositionForProjectionType(Projected, SceneDepth, Orthographic), SceneDepth);
}

float3 GetCameraVector(float3 Position, float3 CameraPosition, float3 ViewForward, bool Orthographic)
{
    return Orthographic ? ViewForward : normalize(Position - CameraPosition);
}

float2 LineBoxIntersect(float3 RayOrigin, float3 RayEnd, float3 BoxMin, float3 BoxMax)
{
    float3 InvRayDir = 1.0f / (RayEnd - RayOrigin);

    //find the ray intersection with each of the 3 planes defined by the minimum extrema.
    float3 FirstPlaneIntersections = (BoxMin - RayOrigin) * InvRayDir;
    //find the ray intersection with each of the 3 planes defined by the maximum extrema.
    float3 SecondPlaneIntersections = (BoxMax - RayOrigin) * InvRayDir;
    //get the closest of these intersections along the ray
    float3 ClosestPlaneIntersections = min(FirstPlaneIntersections, SecondPlaneIntersections);
    //get the furthest of these intersections along the ray
    float3 FurthestPlaneIntersections = max(FirstPlaneIntersections, SecondPlaneIntersections);

    float2 BoxIntersections;
    //find the furthest near intersection
    BoxIntersections.x = max(ClosestPlaneIntersections.x, max(ClosestPlaneIntersections.y, ClosestPlaneIntersections.z));
    //find the closest far intersection
    BoxIntersections.y = min(FurthestPlaneIntersections.x, min(FurthestPlaneIntersections.y, FurthestPlaneIntersections.z));
    //clamp the intersections to be between RayOrigin and RayEnd on the ray
    return saturate(BoxIntersections);
}

float ComputeDistanceFromBoxToPointInside(float3 BoxCenter, float3 BoxExtent, float3 InPoint)
{
    float3 DistancesToMin = max(InPoint - BoxCenter + BoxExtent, 0);
    float3 DistancesToMax = max(BoxCenter + BoxExtent - InPoint, 0);
    float3 ClosestDistances = min(DistancesToMin, DistancesToMax);
    return min(ClosestDistances.x, min(ClosestDistances.y, ClosestDistances.z));
}

bool RayHitSphere(float3 RayOrigin, float3 UnitRayDirection, float3 SphereCenter, float SphereRadius)
{
    float3 ClosestPointOnRay = max(0, dot(SphereCenter - RayOrigin, UnitRayDirection)) * UnitRayDirection;
    float3 CenterToRay = RayOrigin + ClosestPointOnRay - SphereCenter;
    return dot(CenterToRay, CenterToRay) <= Square(SphereRadius);
}

bool RaySegmentHitSphere(float3 RayOrigin, float3 UnitRayDirection, float RayLength, float3 SphereCenter, float SphereRadius)
{
    float DistanceAlongRay = dot(SphereCenter - RayOrigin, UnitRayDirection);
    float3 ClosestPointOnRay = DistanceAlongRay * UnitRayDirection;
    float3 CenterToRay = RayOrigin + ClosestPointOnRay - SphereCenter;
    return dot(CenterToRay, CenterToRay) <= Square(SphereRadius) && DistanceAlongRay > -SphereRadius && DistanceAlongRay - SphereRadius < RayLength;
}

float2 RayIntersectSphere(float3 RayOrigin, float3 RayDirection, float4 Sphere)
{
    float3 LocalPosition = RayOrigin - Sphere.xyz;
    float LocalPositionSqr = dot(LocalPosition, LocalPosition);

    float3 QuadraticCoef;
    QuadraticCoef.x = dot(RayDirection, RayDirection);
    QuadraticCoef.y = 2 * dot(RayDirection, LocalPosition);
    QuadraticCoef.z = LocalPositionSqr - Sphere.w * Sphere.w;

    float Discriminant = QuadraticCoef.y * QuadraticCoef.y - 4 * QuadraticCoef.x * QuadraticCoef.z;

    float2 Intersections = -1;

    // Only continue if the ray intersects the sphere
    [flatten]
    if (Discriminant >= 0)
    {
        float SqrtDiscriminant = sqrt(Discriminant);
        Intersections = (-QuadraticCoef.y + float2(-1, 1) * SqrtDiscriminant) / (2 * QuadraticCoef.x);
    }

    return Intersections;
}

void GenerateCoordinateSystem(float3 ZAxis, out float3 XAxis, out float3 YAxis)
{
    if (abs(ZAxis.x) > abs(ZAxis.y))
    {
        float InverseLength = 1.0f / sqrt(dot(ZAxis.xz, ZAxis.xz));
        XAxis = float3(-ZAxis.z * InverseLength, 0.0f, ZAxis.x * InverseLength);
    }
    else
    {
        float InverseLength = 1.0f / sqrt(dot(ZAxis.yz, ZAxis.yz));
        XAxis = float3(0.0f, ZAxis.z * InverseLength, -ZAxis.y * InverseLength);
    }

    YAxis = cross(ZAxis, XAxis);
}

float DecodePackedTwoChannelValue(float2 PackedHeight)
{
    return PackedHeight.x * 255.0 * 256.0 + PackedHeight.y * 255.0;
}

float DecodeHeightValue(float InValue)
{
    return (InValue - 32768.0) * (1.0f / 128.0f);
}

float DecodePackedHeight(float2 PackedHeight)
{
    return DecodeHeightValue(DecodePackedTwoChannelValue(PackedHeight));
}

uint ReverseBitsN(uint Bitfield, const uint BitCount)
{
    return reversebits(Bitfield) >> (32 - BitCount);
}

uint2 ZOrder2D(uint Index, const uint SizeLog2)
{
    uint2 Coord = 0;

    [unroll]
    for (uint i = 0; i < SizeLog2; i++)
    {
        Coord.x |= ((Index >> (2 * i + 0)) & 0x1) << i;
        Coord.y |= ((Index >> (2 * i + 1)) & 0x1) << i;
    }

    return Coord;
}

uint3 ZOrder3D(uint Index, const uint SizeLog2)
{
    uint3 Coord = 0;

    [unroll]
    for (uint i = 0; i < SizeLog2; i++)
    {
        Coord.x |= ((Index >> (3 * i + 0)) & 0x1) << i;
        Coord.y |= ((Index >> (3 * i + 1)) & 0x1) << i;
        Coord.z |= ((Index >> (3 * i + 2)) & 0x1) << i;
    }

    return Coord;
}

uint ZOrder3DEncode(uint3 Coord, const uint SizeLog2)
{
    uint Index = 0;

    [unroll]
    for (uint i = 0; i < SizeLog2; i++)
    {
        Index |= ((Coord.x >> i) & 0x1) << (3 * i + 0);
        Index |= ((Coord.y >> i) & 0x1) << (3 * i + 1);
        Index |= ((Coord.z >> i) & 0x1) << (3 * i + 2);
    }

    return Index;
}

uint RoundUpToPowerOfTwo(uint Val)
{
    return Val > 1 ? 1u << (firstbithigh(Val - 1) + 1) : 1;
}

uint DivideAndRoundUp(uint Dividend, uint Divisor)
{
    return (Dividend + Divisor - 1) / Divisor;
}

uint DivideAndRoundUp(uint Dividend, uint Divisor, uint DivisorAsBitShift)
{
    return (Dividend + Divisor - 1) >> DivisorAsBitShift;
}

float UNorm16ToF32(uint UNorm)
{
    return (UNorm & 0xFFFFu) / 65535.0f;
}

float SNorm16ToF32(uint SNorm)
{
    return UNorm16ToF32(SNorm) * 2.0f - 1.0f;
}

float ComputeZSliceFromDepth(float3 GridZParams, float SceneDepth)
{
    return log2(SceneDepth * GridZParams.x + GridZParams.y) * GridZParams.z;
}

float ComputeNormalizedZSliceFromDepth(float3 GridZParams, float InvGridSizeZ, float SceneDepth)
{
    return ComputeZSliceFromDepth(GridZParams, SceneDepth) * InvGridSizeZ;
}

float ComputeDepthFromZSlice(float3 GridZParams, float ZSlice)
{
    float SliceDepth = (exp2(ZSlice / GridZParams.z) - GridZParams.y) / GridZParams.x;
    return SliceDepth;
}
