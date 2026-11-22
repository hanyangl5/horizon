#include "SSDTemporalReprojection.slang"

// Writes (shadow, AO, history length). The blend weight follows the per-pixel sample count down to temporalAlpha, so
// disoccluded pixels converge quickly and settled pixels average many frames.
static const float MaxHistoryLength = 64.0;
// The 1 spp input is binary, so the clip box needs a floor: a converged penumbra must survive a neighborhood whose
// samples all happened to agree this frame.
static const float ClipGamma = 1.5;
static const float ClipFloor = 0.1;

[shader("compute")]
[numthreads(16, 16, 1)]
void main(uint3 Thread : SV_DispatchThreadID)
{
    RWTexture2D<float4> Output = ResourceDescriptorHeap[OutputIndex];
    uint                Width;
    uint                Height;
    Output.GetDimensions(Width, Height);
    if (Thread.x >= Width || Thread.y >= Height)
    {
        return;
    }

    Texture2D<float2> Input = ResourceDescriptorHeap[InputIndex];
    Texture2D<float>  DepthTexture = ResourceDescriptorHeap[DepthIndex];
    const int2        Size = int2(Width, Height);
    const int2        Pixel = int2(Thread.xy);
    uint2             SourceSize;
    DepthTexture.GetDimensions(SourceSize.x, SourceSize.y);
    const int2   Source = RTSourcePixel(Pixel, Size, int2(SourceSize));
    const float2 Current = Input.Load(int3(Pixel, 0));
    const float  Depth = DepthTexture.Load(int3(Source, 0));
    if (Depth >= 1.0)
    {
        Output[Pixel] = float4(1.0, 1.0, 0.0, 0.0);
        return;
    }

    float2 Mean = 0.0;
    float2 MeanSquare = 0.0;
    for (int Y = -1; Y <= 1; ++Y)
    {
        for (int X = -1; X <= 1; ++X)
        {
            const float2 Value = Input.Load(int3(clamp(Pixel + int2(X, Y), int2(0), Size - 1), 0));
            Mean += Value;
            MeanSquare += Value * Value;
        }
    }
    Mean /= 9.0;
    const float2 Extent = ClipGamma * sqrt(max(MeanSquare / 9.0 - Mean * Mean, 0.0)) + ClipFloor;

    float2 History = Current;
    float  HistoryLength = 0.0;
    if (HistoryValid != 0)
    {
        StructuredBuffer<FrameData> Frames = ResourceDescriptorHeap[FrameIndex];
        float4 Sum;
        if (SampleReprojectedHistory(Frames[0], Source, Size, SourceSize, Depth, false, Sum))
        {
            History = clamp(Sum.xy, Mean - Extent, Mean + Extent);
            const float2 Difference = abs(History - Sum.xy);
            HistoryLength = ReweightHistory(Sum.z, max(Difference.x, Difference.y));
        }
    }
    HistoryLength = min(HistoryLength + 1.0, MaxHistoryLength);
    Output[Pixel] = float4(lerp(History, Current, max(1.0 / HistoryLength, TemporalAlpha)), HistoryLength, 0.0);
}
