// Radial/spot masks and polygon irradiance adapted from Epic's DeferredLightingCommon.ush and RectLight.ush.
#pragma once

#include "LightingBRDF.slang"
#include "ShadingModels.slang"
#include "ClusterGrid.h"

static const uint LightPoint = 1;
static const uint LightSpot = 2;
static const uint LightRect = 3;
#include "GpuLightData.h"

float SpotAttenuation(float3 L, float3 Direction, float2 SpotAngles)
{
    return Pow2(saturate((dot(L, -Direction) - SpotAngles.x) * SpotAngles.y));
}

float RadialAttenuation(float DistanceSquared, float InvRadius)
{
    return Pow2(saturate(1.0 - Pow2(DistanceSquared * Pow2(InvRadius))));
}

float3 IntegrateEdge(float3 A, float3 B)
{
    const float3 Edge = cross(A, B);
    const float CosAngle = clamp(dot(A, B), -1.0, 1.0);
    return Edge * (acos(CosAngle) * rsqrt(max(1.0 - CosAngle * CosAngle, 1e-8)));
}

float3 PolygonIrradiance(float3 A, float3 B, float3 C, float3 D)
{
    A = normalize(A);
    B = normalize(B);
    C = normalize(C);
    D = normalize(D);
    return 0.5 * (IntegrateEdge(A, B) + IntegrateEdge(B, C) + IntegrateEdge(C, D) + IntegrateEdge(D, A));
}

float GetLocalLightAttenuation(GpuLightData Light, uint Type, float3 Position, out float3 ToLight, out float3 L,
                               out float DistanceSquared)
{
    ToLight = Light.PositionAndInvRadius.xyz - Position;
    DistanceSquared = max(dot(ToLight, ToLight), 1e-4);
    L = normalize(ToLight + float3(0.0, 0.0, 1e-8));

    if (DistanceSquared * Pow2(Light.PositionAndInvRadius.w) >= 1.0)
        return 0.0;
    float LightMask = RadialAttenuation(DistanceSquared, Light.PositionAndInvRadius.w);
    if (Type == LightSpot)
    {
        LightMask *= SpotAttenuation(L, Light.DirectionAndType.xyz, Light.SpotAngles);
    }
    return LightMask;
}

FDirectLighting IntegrateBxDF(GpuLightData Light, uint Type, FGBufferData GBuffer, float3 V, float3 ToLight, float3 L,
                              float DistanceSquared)
{
    const float3 Direction = Light.DirectionAndType.xyz;
    const float3 X = Light.HalfAxisX.xyz;
    const float3 Y = Light.HalfAxisY.xyz;
    FDirectLighting Lighting = {};
    if (Light.Color.a <= 0.0)
    {
        return Lighting;
    }

    if (dot(Direction, -L) <= 0.0)
        return Lighting;
    const float Node = 0.577350269;
    for (uint I = 0; I < 4; ++I)
    {
        const float2 Sign = float2(I & 1, I >> 1) * 2.0 - 1.0;
        const float3 ToSample = ToLight + Node * (Sign.x * X + Sign.y * Y);
        const float SampleDist2 = max(dot(ToSample, ToSample), 1e-4);
        const float3 SampleDirection = ToSample * rsqrt(SampleDist2);
        const float EmitterCosine = dot(Direction, -SampleDirection);
        const float Weight = saturate(EmitterCosine) * (0.25 * Light.Color.a / SampleDist2);
        const FDirectLighting Sample = IntegrateBxDF(GBuffer, V, SampleDirection, Weight);
        Lighting.Diffuse += Sample.Diffuse;
        Lighting.Specular += Sample.Specular;
    }

    // Use the Lambert polygon integral when the whole rectangle is above the surface horizon.
    if (Type == LightRect && dot(GBuffer.WorldNormal, ToLight) > abs(dot(GBuffer.WorldNormal, X)) + abs(dot(GBuffer.WorldNormal, Y)))
    {
        float3 Irradiance = PolygonIrradiance(ToLight - X - Y, ToLight + X - Y, ToLight + X + Y, ToLight - X + Y);
        Irradiance *= dot(Irradiance, ToLight) < 0.0 ? -1.0 : 1.0;
        const FBxDFEnergyTerms Energy = ComputeGGXSpecEnergyTerms(GBuffer.Roughness,
                                                                 saturate(abs(dot(GBuffer.WorldNormal, V)) + 1e-5),
                                                                 GBuffer.SpecularColor);
        Lighting.Diffuse = Diffuse_Lambert(GBuffer.DiffuseColor) * ComputeEnergyPreservation(Energy) *
                           max(dot(GBuffer.WorldNormal, Irradiance), 0.0);
    }
    return Lighting;
}

float3 EvaluateLocalLight(GpuLightData Light, FGBufferData GBuffer, float3 Position, float3 V)
{
    const uint Type = uint(Light.DirectionAndType.w);
    if (Light.PositionAndInvRadius.w <= 0.0)
        return 0.0;
    float3 ToLight, L;
    float DistanceSquared;
    const float LightMask = GetLocalLightAttenuation(Light, Type, Position, ToLight, L, DistanceSquared);
    if (LightMask <= 0.0)
    {
        return 0.0;
    }

    FDirectLighting Lighting;
    if (Type == LightPoint || Type == LightSpot)
    {
        float Falloff = rcp(DistanceSquared);
        if (Light.SourceLength > 0.0)
        {
            const float3 HalfLine = 0.5 * Light.SourceLength * Light.DirectionAndType.xyz;
            const float3 A = ToLight - HalfLine, B = ToLight + HalfLine;
            const float DA = max(length(A), 1e-4), DB = max(length(B), 1e-4);
            Falloff = 2.0 / max(DA * DB + dot(A, B), 1e-4);
            const float3 Sum = A / DA + B / DB;
            L = Sum * rsqrt(max(dot(Sum, Sum), 1e-8));
        }
        const float InvDistance = rsqrt(DistanceSquared);
        GBuffer.Roughness = saturate(GBuffer.Roughness + Light.SoftSourceRadius * InvDistance);
        Lighting = DefaultLitBxDF(GBuffer, V, L, min(Light.SourceRadius * InvDistance, 0.9999));
        Lighting.Diffuse *= Falloff;
        Lighting.Specular *= Falloff;
    }
    else
    {
        Lighting = IntegrateBxDF(Light, Type, GBuffer, V, ToLight, L, DistanceSquared);
    }
    return (Lighting.Diffuse + Lighting.Specular) * (Light.Color.rgb * LightMask);
}

GpuLightData LoadLocalLight(ByteAddressBuffer Lights, ByteAddressBuffer Indices, uint Entry)
{
    const uint LightIndex = Indices.Load(Entry * sizeof(uint));
    return Lights.Load<GpuLightData>(LightIndex * GpuLightStride);
}

float3 AccumulateLocalLights(LightingFrameData Frame, FGBufferData GBuffer, float3 Position, float3 V)
{
    float3 Result = 0.0;
    if (Frame.LocalLightCount == 0)
    {
        return Result;
    }
    ByteAddressBuffer Lights = ResourceDescriptorHeap[Frame.LocalLightsIndex];
    ByteAddressBuffer ActiveLights = ResourceDescriptorHeap[Frame.ActiveLocalLightsIndex];
    for (uint Index = 0; Index < Frame.LocalLightCount; ++Index)
    {
        GpuLightData Light = LoadLocalLight(Lights, ActiveLights, Index);
        Result += EvaluateLocalLight(Light, GBuffer, Position, V);
    }
    return Result;
}

uint2 GetZBinLightRange(ByteAddressBuffer ZBins, LightingFrameData Frame, float Near, float3 Position)
{
    const float Depth = dot(Frame.ClusterLightingClipW, float4(Position, 1));
    const uint Z = uint(clamp((Depth - Near) * Frame.ClusterLightingBinScale, 0.0, float(CLUSTER_DEPTH_BIN_COUNT - 1)));
    return ZBins.Load2(Frame.ClusterLightingZBinsOffset + Z * sizeof(uint2));
}

// The batch overlaps the inclusive Z-bin range. Both bit indices are in [0, 31].
uint GetLightRangeMask(uint2 Range, uint FirstLight)
{
    const uint LastBit = LIGHTS_PER_BITMASK - 1u;
    const uint First = max(Range.x, FirstLight) - FirstLight;
    const uint Last = min(Range.y - FirstLight, LastBit);
    return (~0u << First) & (~0u >> (LastBit - Last));
}

float3 AccumulateScreenLights(uint2 Pixel, float Near, LightingFrameData Frame, FGBufferData GBuffer, float3 Position, float3 V)
{
    if (Frame.ClusterLightingZBinsIndex == INVALID_TEXTURE)
    {
        return AccumulateLocalLights(Frame, GBuffer, Position, V);
    }
    float3 Result = 0.0;
    if (Frame.ClusterLightingTileLightBitMasksIndex == INVALID_TEXTURE)
        return Result;
    ByteAddressBuffer Lights = ResourceDescriptorHeap[Frame.LocalLightsIndex];
    ByteAddressBuffer Indices = ResourceDescriptorHeap[Frame.ActiveLocalLightsIndex];
    ByteAddressBuffer ZBins = ResourceDescriptorHeap[Frame.ClusterLightingZBinsIndex];
    ByteAddressBuffer Masks = ResourceDescriptorHeap[Frame.ClusterLightingTileLightBitMasksIndex];
    const uint2 LightRange = GetZBinLightRange(ZBins, Frame, Near, Position);
    const uint2 Range = LightRange / LIGHTS_PER_BITMASK;
    const uint TileBase = GetTileLightMaskBase(Pixel / LIGHT_TILE_SIZE, Frame.ClusterLightingTilesX);
    for (uint i = Range.x; i <= Range.y; ++i)
    {
        const uint FirstLight = i * LIGHTS_PER_BITMASK;
        const uint TileMask = Masks.Load((TileBase + i) * sizeof(uint));
        uint Bits = TileMask & GetLightRangeMask(LightRange, FirstLight);
        while (Bits != 0)
        {
            const uint SortedIndex = FirstLight + firstbitlow(Bits);
            GpuLightData Light = LoadLocalLight(Lights, Indices, Frame.LocalLightCount + SortedIndex);
            Result += EvaluateLocalLight(Light, GBuffer, Position, V);
            Bits &= Bits - 1;
        }
    }
    return Result;
}

float3 AccumulateMainLight(LightingFrameData Frame, FGBufferData GBuffer, float3 V, float SurfaceShadow, uint PassMask = 0)
{
    if (Frame.LightDirection.w <= 0.0 || SurfaceShadow <= 0.0 || (Frame.MainExcludedPasses & PassMask) != 0)
        return 0.0;
    const FDirectLighting Lighting = DefaultLitBxDF(GBuffer, V, -normalize(Frame.LightDirection.xyz), 0.0);
    return (Lighting.Diffuse * Frame.MainDiffuseScale + Lighting.Specular * Frame.MainSpecularScale) *
           (max(Frame.LightColor.rgb, 0.0) * (Frame.LightDirection.w * saturate(SurfaceShadow)));
}
