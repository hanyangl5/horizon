// Histogram bin weighting adapted from Epic Games' PostProcessHistogram.usf.
#include "PostExposure.slang"

cbuffer RootConstant0
{
    uint SceneColorIndex;
    uint HistogramIndex;
};

groupshared uint SharedHistogram[HistogramSize];

[shader("compute")]
[numthreads(64, 1, 1)]
void CSMain(uint3 Group : SV_GroupID, uint Lane : SV_GroupIndex)
{
    Texture2D<float4> SceneColor = ResourceDescriptorHeap[SceneColorIndex];
    RWTexture2D<uint> Histogram = ResourceDescriptorHeap[HistogramIndex];
    uint Width, Height, Bins, Rows;
    SceneColor.GetDimensions(Width, Height);
    Histogram.GetDimensions(Bins, Rows);
    SharedHistogram[Lane] = 0;
    GroupMemoryBarrierWithGroupSync();

    // Each group owns a histogram row. Adjacent lanes read adjacent pixels.
    for (uint Y = Group.x; Y < Height; Y += Rows)
    {
        for (uint X = Lane; X < Width; X += HistogramSize)
        {
            float3 Color = SceneColor.Load(int3(X, Y, 0)).rgb;
            float LuminanceValue = dot(Color, float3(1.0 / 3.0));
            if (isfinite(LuminanceValue) && LuminanceValue > exp2(HistogramMinEV))
            {
                float Position = saturate((log2(LuminanceValue) - HistogramMinEV) / (HistogramMaxEV - HistogramMinEV));
                float Bucket = Position * (HistogramSize - 1);
                uint Lower = uint(Bucket);
                uint UpperWeight = uint(round(frac(Bucket) * 256.0));
                if (Lower != 0)
                {
                    InterlockedAdd(SharedHistogram[Lower], 256u - UpperWeight);
                }
                if (UpperWeight != 0)
                {
                    InterlockedAdd(SharedHistogram[Lower + 1], UpperWeight);
                }
            }
        }
    }

    GroupMemoryBarrierWithGroupSync();
    Histogram[uint2(Lane, Group.x)] = SharedHistogram[Lane];
}
