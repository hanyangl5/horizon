// Radial/spot masks and polygon irradiance adapted from Epic's DeferredLightingCommon.ush and RectLight.ush.
#pragma once

#include "LightingBRDF.slang"
#include "ShadingModels.slang"
#include "ClusterGrid.h"

static const uint LightDirectional = 0;
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

float3 AccumulateDynamicLighting(GpuLightData Light, FGBufferData GBuffer, float3 Position, float3 V, float SurfaceShadow)
{
    const uint Type = uint(Light.DirectionAndType.w);
    const bool RadialLight = Type != LightDirectional;
    float3 L = -Light.DirectionAndType.xyz;
    float3 ToLight = L;
    float DistanceSquared = 1.0;
    float LightMask = 1.0;
    if (RadialLight)
    {
        if (Light.PositionAndInvRadius.w <= 0.0)
            return 0.0;
        LightMask = GetLocalLightAttenuation(Light, Type, Position, ToLight, L, DistanceSquared);
    }
    if (LightMask <= 0.0 || SurfaceShadow <= 0.0)
    {
        return 0.0;
    }

    FDirectLighting Lighting;
    if (!RadialLight || Type == LightPoint || Type == LightSpot)
    {
        float Falloff = rcp(DistanceSquared);
        if (Light.SourceLength > 0.0 && RadialLight)
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
    return (Lighting.Diffuse + Lighting.Specular) * (Light.Color.rgb * (LightMask * SurfaceShadow));
}

float3 EvaluateIndexedLight(ByteAddressBuffer Lights, uint Index, FGBufferData GBuffer, float3 Position, float3 V)
{
    GpuLightData Light = Lights.Load<GpuLightData>(Index * GpuLightStride);
    if (uint(Light.DirectionAndType.w) > LightRect)
        return 0.0;
    return AccumulateDynamicLighting(Light, GBuffer, Position, V, 1.0);
}

float3 AccumulateAdditionalLights(LightingFrameData Frame, FGBufferData GBuffer, float3 Position, float3 V)
{
    float3 Result = 0.0;
    if (Frame.LightCount == 0)
    {
        return Result;
    }
    ByteAddressBuffer Lights = ResourceDescriptorHeap[Frame.LightsIndex];
    ByteAddressBuffer ActiveLights = ResourceDescriptorHeap[Frame.ActiveLightsIndex];
    for (uint Index = 0; Index < Frame.LightCount; ++Index)
    {
        const uint LightIndex = ActiveLights.Load(Index * sizeof(uint));
        if (LightIndex == Frame.MainDirectionalIndex)
        {
            continue;
        }
        Result += EvaluateIndexedLight(Lights, LightIndex, GBuffer, Position, V);
    }
    return Result;
}

float3 AccumulateScreenLights(uint DataIndex, uint2 Pixel, LightingFrameData Frame, FGBufferData GBuffer, float3 Position, float3 V)
{
    if (DataIndex == INVALID_TEXTURE)
    {
        return AccumulateAdditionalLights(Frame, GBuffer, Position, V);
    }
    ByteAddressBuffer Data = ResourceDescriptorHeap[DataIndex];
    const ClusterGrid Grid = Data.Load<ClusterGrid>(0);
    if (Grid.GlobalCount + Grid.FiniteCount == 0)
    {
        return 0.0;
    }
    ByteAddressBuffer Lights = ResourceDescriptorHeap[Frame.LightsIndex];
    float3 Result = 0.0;
    for (uint I = 0; I < Grid.GlobalCount; ++I)
    {
        Result += EvaluateIndexedLight(Lights, Data.Load(Grid.IndexOffset + I * 4), GBuffer, Position, V);
    }
    if (Grid.Words == 0)
    {
        return Result;
    }
    const float Depth = dot(Grid.ClipW, float4(Position, 1));
    const uint Z = uint(clamp((Depth - Grid.Near) * Grid.BinScale, 0.0, float(Grid.BinCount - 1)));
    const uint2 Range = Data.Load2(sizeof(ClusterGrid) + Z * 8);
    if (Range.x > Range.y)
    {
        return Result;
    }
    const uint2 Tile = Pixel / LIGHT_TILE_SIZE;
    const uint Base = (Tile.y * Grid.TilesX + Tile.x) * Grid.Words;
    ByteAddressBuffer Masks = ResourceDescriptorHeap[Grid.MaskIndex];
    for (uint Word = Range.x / 32; Word <= Range.y / 32; ++Word)
    {
        uint Bits = Masks.Load((Base + Word) * 4);
        if (Word == Range.x / 32)
        {
            Bits &= 0xffffffffu << (Range.x & 31);
        }
        if (Word == Range.y / 32)
        {
            Bits &= 0xffffffffu >> (31 - (Range.y & 31));
        }
        while (Bits != 0)
        {
            const uint I = Grid.GlobalCount + Word * 32 + firstbitlow(Bits);
            Result += EvaluateIndexedLight(Lights, Data.Load(Grid.IndexOffset + I * 4), GBuffer, Position, V);
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

float3 AccumulateSceneLights(LightingFrameData Frame, FGBufferData GBuffer, float3 Position, float3 V, float SurfaceShadow, uint PassMask = 0)
{
    return AccumulateMainLight(Frame, GBuffer, V, SurfaceShadow, PassMask) + AccumulateAdditionalLights(Frame, GBuffer, Position, V);
}
