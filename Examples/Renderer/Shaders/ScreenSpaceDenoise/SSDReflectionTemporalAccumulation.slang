#include "SSDTemporalReprojection.slang"

// Accumulate half-resolution GGX reflection samples. Clip reprojected history against the local
// radiance distribution in YCoCg. Alpha stores sample count; untraced texels keep it at zero.
static const float MaxHistoryLength = 32.0;
static const float ClipGamma = 1.25;

float3 ToYCoCg(float3 Rgb)
{
    return float3(dot(Rgb, float3(0.25, 0.5, 0.25)), dot(Rgb, float3(0.5, 0.0, -0.5)), dot(Rgb, float3(-0.25, 0.5, -0.25)));
}

float3 FromYCoCg(float3 C) { return float3(C.x + C.y - C.z, C.x + C.z, C.x - C.y - C.z); }

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

    Texture2D<float4> Input = ResourceDescriptorHeap[InputIndex];
    Texture2D<float>  DepthTexture = ResourceDescriptorHeap[DepthIndex];
    const int2        Size = int2(Width, Height);
    const int2        Pixel = int2(Thread.xy);
    uint2             SourceSize;
    DepthTexture.GetDimensions(SourceSize.x, SourceSize.y);
    const int2   Source = RTSourcePixel(Pixel, Size, int2(SourceSize));
    const float4 Current = Input.Load(int3(Pixel, 0));
    const float  Depth = DepthTexture.Load(int3(Source, 0));
    // Alpha marks a traced reflection; sky and surfaces too rough for ray traced reflections have none.
    if (Depth >= 1.0 || Current.a <= 0.0)
    {
        Output[Pixel] = 0.0;
        return;
    }

    // Statistics over the traced neighbours only, so the roughness cutoff does not pull history toward black.
    float3 Mean = 0.0;
    float3 MeanSquare = 0.0;
    float  Count = 0.0;
    for (int Y = -1; Y <= 1; ++Y)
    {
        for (int X = -1; X <= 1; ++X)
        {
            const float4 Value = Input.Load(int3(clamp(Pixel + int2(X, Y), int2(0), Size - 1), 0));
            if (Value.a > 0.0)
            {
                const float3 C = ToYCoCg(Value.rgb);
                Mean += C;
                MeanSquare += C * C;
                Count += 1.0;
            }
        }
    }
    Mean /= Count;
    const float3 Extent = ClipGamma * sqrt(max(MeanSquare / Count - Mean * Mean, 0.0));

    float3 History = Current.rgb;
    float  HistoryLength = 0.0;
    if (HistoryValid != 0)
    {
        StructuredBuffer<FrameData> Frames = ResourceDescriptorHeap[FrameIndex];
        float4 Sum;
        if (SampleReprojectedHistory(Frames[0], Source, Size, SourceSize, Depth, true, Sum))
        {
            const float3 Previous = ToYCoCg(Sum.rgb);
            const float3 Clipped = clamp(Previous, Mean - Extent, Mean + Extent);
            const float3 Difference = abs(Clipped - Previous);
            const float LumaRejection = abs(log2(1.0 + max(Clipped.x, 0.0)) - log2(1.0 + max(Previous.x, 0.0)));
            History = FromYCoCg(Clipped);
            HistoryLength = ReweightHistory(Sum.a, max(LumaRejection, max(Difference.y, Difference.z)));
        }
    }
    HistoryLength = min(HistoryLength + 1.0, MaxHistoryLength);
    Output[Pixel] = float4(max(lerp(History, Current.rgb, max(1.0 / HistoryLength, TemporalAlpha)), 0.0), HistoryLength);
}
