#include "RTDenoiseBindings.slang"

// One 5x5 B3-spline a-trous iteration. Taps stop at depth discontinuities by their distance to the center pixel's
// tangent plane, relative to view distance, and at creases by the normal difference.
static const float Kernel[3] = { 3.0 / 8.0, 1.0 / 4.0, 1.0 / 16.0 };
static const float NormalPower = 8.0;

[shader("compute")]
[numthreads(16, 16, 1)]
void main(uint3 Thread : SV_DispatchThreadID)
{
    RWTexture2D<float2> Output = ResourceDescriptorHeap[OutputIndex];
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
    const float2 Center = Input.Load(int3(Pixel, 0));
    const float  Depth = DepthTexture.Load(int3(Source, 0));
    if (Depth >= 1.0)
    {
        Output[Pixel] = Center;
        return;
    }

    StructuredBuffer<FrameData> Frames = ResourceDescriptorHeap[FrameIndex];
    Texture2D<float4>           Normals = ResourceDescriptorHeap[NormalIndex];
    const float4x4              InverseViewProjection = Frames[0].InverseViewProjection;
    const float3 Position =
        ReconstructWorldPosition((float2(Source) + 0.5) / float2(SourceSize), Depth, InverseViewProjection);
    const float3 Normal = DecodeOctNormal(Normals.Load(int3(Source, 0)).xy);
    const float  PlaneScale = 1.0 / (PlaneSigma * length(Frames[0].Eye.xyz - Position));
    float2       Sum = Center * (Kernel[0] * Kernel[0]);
    float        WeightSum = Kernel[0] * Kernel[0];
    for (int Y = -2; Y <= 2; ++Y)
    {
        for (int X = -2; X <= 2; ++X)
        {
            const int2 Tap = Pixel + int2(X, Y) * int(StepWidth);
            if ((X == 0 && Y == 0) || any(Tap < 0) || any(Tap >= Size))
            {
                continue;
            }
            const int2  TapSource = RTSourcePixel(Tap, Size, int2(SourceSize));
            const float TapDepth = DepthTexture.Load(int3(TapSource, 0));
            if (TapDepth >= 1.0)
            {
                continue;
            }
            const float3 TapPosition =
                ReconstructWorldPosition((float2(TapSource) + 0.5) / float2(SourceSize), TapDepth, InverseViewProjection);
            const float3 TapNormal = DecodeOctNormal(Normals.Load(int3(TapSource, 0)).xy);
            const float  Weight = Kernel[abs(X)] * Kernel[abs(Y)] * pow(saturate(dot(Normal, TapNormal)), NormalPower) *
                                 exp(-abs(dot(Normal, TapPosition - Position)) * PlaneScale);
            Sum += Input.Load(int3(Tap, 0)) * Weight;
            WeightSum += Weight;
        }
    }
    Output[Pixel] = Sum / WeightSum;
}
