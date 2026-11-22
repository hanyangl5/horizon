#include "SSDSpatialKernel.slang"

[shader("compute")]
[numthreads(16, 16, 1)]
void main(uint3 Thread : SV_DispatchThreadID)
{
    RWTexture2D<float4> Output = ResourceDescriptorHeap[OutputIndex];
    uint2 Extent;
    Output.GetDimensions(Extent.x, Extent.y);
    if (any(Thread.xy >= Extent))
    {
        return;
    }

    Texture2D<float4> Signal = ResourceDescriptorHeap[InputIndex];
    Texture2D<float> Depth = ResourceDescriptorHeap[DepthIndex];
    Texture2D<float4> Normals = ResourceDescriptorHeap[NormalIndex];
    StructuredBuffer<FrameData> Frames = ResourceDescriptorHeap[FrameIndex];
    const int2 Size = int2(Extent);
    const int2 Pixel = int2(Thread.xy);
    uint2 SourceSize;
    Depth.GetDimensions(SourceSize.x, SourceSize.y);
    const int2 Source = RTSourcePixel(Pixel, Size, int2(SourceSize));
    const float4 Center = Signal.Load(int3(Pixel, 0));
    const float CenterDepth = Depth.Load(int3(Source, 0));
    const float4 Material = Normals.Load(int3(Source, 0));
    const float Roughness = UnpackMaterialProperties(Material.ba).x;
    if (Center.a <= 0.0 || CenterDepth >= 1.0 || Roughness <= 1.0 / 255.0)
    {
        Output[Pixel] = Center;
        return;
    }

    const FrameData Frame = Frames[0];
    const float3 Position = ReconstructWorldPosition((float2(Source) + 0.5) / float2(SourceSize), CenterDepth, Frame.InverseViewProjection);
    const float3 Normal = DecodeOctNormal(Material.xy);
    const float MaxWorldBlur = PlaneSigma * length(Frame.Eye.xyz - Position);
    const int Radius = Roughness > 0.25 && Center.a < 8.0 ? 2 : 1;
    float3 Sum = Center.rgb;
    float WeightSum = 1.0;
    for (int Y = -1; Y <= 1; ++Y)
    {
        for (int X = -1; X <= 1; ++X)
        {
            const int2 Tap = Pixel + int2(X, Y) * Radius;
            if ((X == 0 && Y == 0) || any(Tap < 0) || any(Tap >= Size))
            {
                continue;
            }
            const float4 Value = Signal.Load(int3(Tap, 0));
            const int2 TapSource = RTSourcePixel(Tap, Size, int2(SourceSize));
            const float TapDepth = Depth.Load(int3(TapSource, 0));
            if (Value.a <= 0.0 || TapDepth >= 1.0)
            {
                continue;
            }
            const float4 TapMaterial = Normals.Load(int3(TapSource, 0));
            const float TapRoughness = UnpackMaterialProperties(TapMaterial.ba).x;
            const float3 TapPosition =
                ReconstructWorldPosition((float2(TapSource) + 0.5) / float2(SourceSize), TapDepth, Frame.InverseViewProjection);
            const float Weight = ComputeBilateralWeight(Normal, DecodeOctNormal(TapMaterial.xy), TapPosition - Position, MaxWorldBlur) *
                                 saturate(1.0 - abs(TapRoughness - Roughness) * 20.0) * (X == 0 || Y == 0 ? 0.5 : 0.25);
            Sum += Value.rgb * Weight;
            WeightSum += Weight;
        }
    }
    Output[Pixel] = float4(Sum / WeightSum, Center.a);
}
