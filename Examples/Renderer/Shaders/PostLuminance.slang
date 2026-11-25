// Histogram trimming and eye adaptation adapted from Epic Games' PostProcessHistogramCommon.ush.
#include "PostExposure.slang"

cbuffer RootConstant0
{
    uint HistogramIndex;
    uint OutputIndex;
    uint PreviousIndex;
    float DeltaSeconds;
};

groupshared float Buckets[HistogramSize];

float ComputeAverageLuminanceWithoutOutlier()
{
    float Total = 0.0;
    for (uint I = 0; I < HistogramSize; ++I)
    {
        Total += Buckets[I];
    }

    float MinFractionSum = Total * 0.1;
    float MaxFractionSum = Total * 0.9;
    float2 SumWithoutOutliers = 0.0;
    for (uint I = 0; I < HistogramSize; ++I)
    {
        float Weight = Buckets[I];
        float Discard = min(Weight, MinFractionSum);
        MinFractionSum -= Discard;
        MaxFractionSum -= Discard;
        Weight = min(Weight - Discard, MaxFractionSum);
        MaxFractionSum -= Weight;
        float LogLuminance = lerp(HistogramMinEV, HistogramMaxEV, float(I) / float(HistogramSize - 1));
        SumWithoutOutliers += float2(LogLuminance, 1.0) * Weight;
    }
    if (SumWithoutOutliers.y == 0.0)
    {
        return exp2(HistogramMinEV);
    }
    return exp2(SumWithoutOutliers.x / SumWithoutOutliers.y);
}

float ComputeEyeAdaptation(float Previous, float Target)
{
    float CurrentEV = log2(Previous);
    float Distance = log2(Target) - CurrentEV;
    float Speed = Distance > 0.0 ? 3.0 : 1.0;
    const float TransitionDistance = 1.5;
    if (abs(Distance) > TransitionDistance)
    {
        return exp2(CurrentEV + sign(Distance) * min(abs(Distance), DeltaSeconds * Speed));
    }

    // Match the linear slope at the transition, calibrated at 60 Hz.
    const float ReferenceTime = 1.0 / 60.0;
    float M = (ReferenceTime * Speed) / (TransitionDistance * max(1.0 - exp2(-ReferenceTime * Speed), 1e-8));
    float Factor = saturate((1.0 - exp2(-DeltaSeconds * Speed)) * M);
    return exp2(CurrentEV + Distance * Factor);
}

[shader("compute")]
[numthreads(64, 1, 1)]
void CSAdapt(uint Lane : SV_GroupIndex)
{
    Texture2D<uint> Histogram = ResourceDescriptorHeap[HistogramIndex];
    uint Width, Rows;
    Histogram.GetDimensions(Width, Rows);
    float Sum = 0.0;
    for (uint Row = 0; Row < Rows; ++Row)
    {
        Sum += float(Histogram.Load(int3(Lane, Row, 0)));
    }
    Buckets[Lane] = Sum;
    GroupMemoryBarrierWithGroupSync();
    if (Lane == 0)
    {
        float Target = clamp(ComputeAverageLuminanceWithoutOutlier(), 0.18 * exp2(HistogramMinEV), 0.18 * exp2(HistogramMaxEV));
        if (PreviousIndex != 0xffffffffu)
        {
            Texture2D<float> Previous = ResourceDescriptorHeap[PreviousIndex];
            Target = ComputeEyeAdaptation(Previous.Load(int3(0, 0, 0)), Target);
        }
        RWTexture2D<float> Output = ResourceDescriptorHeap[OutputIndex];
        Output[uint2(0, 0)] = Target;
    }
}
