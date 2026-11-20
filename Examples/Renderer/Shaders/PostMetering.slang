#include "PostExposure.slang"

cbuffer RootConstant0
{
    uint SceneColorIndex;
    uint LuminanceOutputIndex;
};

// Exposure metering only: bloom thresholds and blurs its own half-resolution chain.
[shader("compute")]
[numthreads(16, 16, 1)]
void CSMain(uint3 DispatchId : SV_DispatchThreadID)
{
    Texture2D<float4>  SceneColor = ResourceDescriptorHeap[SceneColorIndex];
    RWTexture2D<float> LuminanceOutput = ResourceDescriptorHeap[LuminanceOutputIndex];
    uint               Width;
    uint               Height;
    LuminanceOutput.GetDimensions(Width, Height);

    if (DispatchId.x >= Width || DispatchId.y >= Height)
    {
        return;
    }
    uint SourceWidth;
    uint SourceHeight;
    SceneColor.GetDimensions(SourceWidth, SourceHeight);
    float2 Footprint = float2(SourceWidth, SourceHeight) / float2(Width, Height);
    float2 Lower = float2(DispatchId.xy) * Footprint;
    float2 Upper = float2(DispatchId.xy + 1) * Footprint;
    int2   First = int2(floor(Lower));
    int2   Last = min(int2(ceil(Upper)), int2(SourceWidth, SourceHeight));
    float3 Color = float3(0.0);
    // Integrate the entire source footprint so subpixel highlights cannot fall between sparse samples.
    for (int Y = First.y; Y < Last.y; ++Y)
    {
        float WeightY = max(min(Upper.y, float(Y + 1)) - max(Lower.y, float(Y)), 0.0);
        for (int X = First.x; X < Last.x; ++X)
        {
            float WeightX = max(min(Upper.x, float(X + 1)) - max(Lower.x, float(X)), 0.0);
            Color += SceneColor.Load(int3(X, Y, 0)).rgb * (WeightX * WeightY);
        }
    }
    Color /= Footprint.x * Footprint.y;
    // Mip averaging in log space yields a geometric mean for exposure metering.
    LuminanceOutput[DispatchId.xy] = log2(max(Luminance(Color), 0.0001));
}
