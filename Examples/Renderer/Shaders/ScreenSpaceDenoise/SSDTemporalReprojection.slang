// History reweighting adapted from Epic's SSDTemporalAccumulation.usf.
#pragma once

#include "SSDCommon.slang"

float ReweightHistory(float SampleCount, float Rejection)
{
    return SampleCount * saturate(1.0 - Rejection);
}

float2 DenoisePreviousPixel(FrameData Frame, int2 Source, int2 Size, uint2 SourceSize, float Depth, out float PreviousViewDepth)
{
    const float2 SourceUV = (float2(Source) + 0.5) / float2(SourceSize);
    const float3 Position = ReconstructWorldPosition(SourceUV, Depth, Frame.InverseViewProjection);
    const float4 Clip = mul(Frame.PreviousViewProjection, float4(Position, 1.0));
    PreviousViewDepth = Clip.w;
    if (Clip.w <= 0.0)
    {
        return -2.0;
    }

    float2 PreviousUV = Clip.xy / Clip.w * float2(0.5, -0.5) + 0.5;
    if (MotionIndex != INVALID_TEXTURE)
    {
        Texture2D<float4> Motion = ResourceDescriptorHeap[MotionIndex];
        const float2 Packed = DecodeMotionVector(Motion.Load(int3(Source, 0)).rgb);
        const float2 Camera = SourceUV - PreviousUV;
        const float2 Velocity = select(abs(Packed - Camera) <= (1.0 / 4095.0 + 1e-6), Camera, Packed);
        PreviousUV = SourceUV - Velocity;
    }
    return (PreviousUV * float2(SourceSize) - 0.5) * float2(Size) / float2(SourceSize);
}

// Both temporal passes use the same depth test. Reflection also rejects history with a zero sample count.
bool SampleReprojectedHistory(FrameData Frame, int2 Source, int2 Size, uint2 SourceSize, float Depth,
                             bool RequireSampleCount, out float4 History)
{
    History = 0.0;
    float PreviousViewDepth;
    const float2 PreviousPixel = DenoisePreviousPixel(Frame, Source, Size, SourceSize, Depth, PreviousViewDepth);
    if (PreviousViewDepth <= 0.0)
    {
        return false;
    }

    Texture2D<float4> HistoryTexture = ResourceDescriptorHeap[HistoryIndex];
    Texture2D<float> PreviousDepth = ResourceDescriptorHeap[PreviousDepthIndex];
    const int2 Base = int2(floor(PreviousPixel));
    const float2 Fraction = PreviousPixel - float2(Base);
    float WeightSum = 0.0;
    for (int I = 0; I < 4; ++I)
    {
        const int2 Offset = int2(I & 1, I >> 1);
        const int2 Tap = Base + Offset;
        if (any(Tap < 0) || any(Tap >= Size) ||
            abs(PreviousDepth.Load(int3(Tap, 0)) - PreviousViewDepth) > DepthThreshold * PreviousViewDepth)
        {
            continue;
        }
        const float4 Value = HistoryTexture.Load(int3(Tap, 0));
        if (RequireSampleCount && Value.a <= 0.0)
        {
            continue;
        }
        const float2 Bilinear = lerp(1.0 - Fraction, Fraction, float2(Offset));
        const float Weight = Bilinear.x * Bilinear.y;
        History += Value * Weight;
        WeightSum += Weight;
    }

    if (WeightSum <= 1e-3)
    {
        return false;
    }
    History /= WeightSum;
    return true;
}
