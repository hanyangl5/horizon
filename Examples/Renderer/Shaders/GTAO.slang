// Horizon search and projected-normal integral adapted from Epic's PostProcessAmbientOcclusion.usf.
#include "Common.slang"
#include "Random.slang"

cbuffer RootConstant0
{
    uint FrameIndex;
    uint DepthIndex;
    uint NormalIndex;
    uint SourceIndex;
    uint OutputIndex;
    float Radius;
};

static const uint DirectionCount = 2;
static const uint StepCount = 4;

float IntegrateHorizon(float2 Horizons, float3 Axis, float3 View, float3 Normal)
{
    const float3 PlaneNormal = normalize(cross(Axis, View));
    const float3 Perpendicular = cross(View, PlaneNormal);
    const float3 Projected = Normal - PlaneNormal * dot(Normal, PlaneNormal);
    const float ProjectedLength = max(length(Projected), 1e-6);
    const float CosAngle = clamp(dot(Projected, Perpendicular) / ProjectedLength, -1.0, 1.0);
    const float CosGamma = dot(Projected, View) / ProjectedLength;
    const float Gamma = acos(CosAngle) - PI * 0.5;
    const float2 Angles = acos(clamp(Horizons, -1.0, 1.0));
    const float2 Arc = clamp(float2(-Angles.x, Angles.y) - Gamma, -PI * 0.5, PI * 0.5) + Gamma;
    const float2 Integral = Arc * (-2.0 * CosAngle) + CosGamma - cos(2.0 * Arc - Gamma);
    return 0.25 * ProjectedLength * (Integral.x + Integral.y);
}

[numthreads(16, 16, 1)]
void CSMain(uint3 DispatchId : SV_DispatchThreadID)
{
    RWTexture2D<float4> Output = ResourceDescriptorHeap[OutputIndex];
    uint2 OutputSize;
    Output.GetDimensions(OutputSize.x, OutputSize.y);
    if (any(DispatchId.xy >= OutputSize))
    {
        return;
    }

    Texture2D<float> Depth = ResourceDescriptorHeap[DepthIndex];
    Texture2D<float4> Normals = ResourceDescriptorHeap[NormalIndex];
    StructuredBuffer<FrameData> Frames = ResourceDescriptorHeap[FrameIndex];
    uint2 Size;
    Depth.GetDimensions(Size.x, Size.y);
    const int2 Pixel = int2(min(DispatchId.xy * 2 + 1, Size - 1));
    const float DeviceDepth = Depth.Load(int3(Pixel, 0));
    if (DeviceDepth >= 1.0)
    {
        Output[DispatchId.xy] = 1.0;
        return;
    }

    const FrameData Frame = Frames[0];
    const float2 UV = (float2(Pixel) + 0.5) / float2(Size);
    const float3 Center = ReconstructWorldPosition(UV, DeviceDepth, Frame.InverseViewProjection);
    const float3 View = normalize(Frame.Eye.xyz - Center);
    const float3 EncodedNormal = DecodeOctNormal(Normals.Load(int3(Pixel, 0)).xy);
    const float3 Normal = dot(EncodedNormal, View) < 0.0 ? -EncodedNormal : EncodedNormal;
    const float Rotation = InterleavedGradientNoise(float2(Pixel), 0.0);
    const float StepOffset = (float((Pixel.y - Pixel.x) & 3) + 0.5) / 4.0;

    float Occlusion = 0.0;
    for (uint Slice = 0; Slice < DirectionCount; ++Slice)
    {
        const float Angle = PI * (float(Slice) + Rotation) / float(DirectionCount);
        const float2 ScreenDirection = float2(cos(Angle), sin(Angle));
        const float3 Axis = ReconstructWorldPosition(UV + ScreenDirection / float2(Size), DeviceDepth,
                                                    Frame.InverseViewProjection) - Center;
        const float SearchRadius = min(Radius / max(length(Axis), 1e-6), 64.0);
        float2 Horizons = -1.0;
        for (uint Step = 0; Step < StepCount; ++Step)
        {
            const float2 Delta = ScreenDirection * max(1.0, (float(Step) + StepOffset) * SearchRadius / float(StepCount));
            for (uint Side = 0; Side < 2; ++Side)
            {
                const int2 Tap = Pixel + int2(round(Side == 0 ? Delta : -Delta));
                if (all(Tap >= 0) && all(Tap < int2(Size)) && any(Tap != Pixel))
                {
                    const float TapDepth = Depth.Load(int3(Tap, 0));
                    if (TapDepth < 1.0)
                    {
                        const float2 TapUV = (float2(Tap) + 0.5) / float2(Size);
                        const float3 Offset = ReconstructWorldPosition(TapUV, TapDepth, Frame.InverseViewProjection) - Center;
                        const float DistanceSquared = dot(Offset, Offset);
                        const float Falloff = saturate(DistanceSquared / (Radius * Radius));
                        const float Horizon = dot(Offset, View) * rsqrt(max(DistanceSquared, 1e-8));
                        const float Candidate = lerp(Horizon, Horizons[Side], Falloff);
                        Horizons[Side] = max(Candidate, lerp(Candidate, Horizons[Side], 0.75));
                    }
                }
            }
        }
        const float Unoccluded = IntegrateHorizon(float2(-1.0, -1.0), Axis, View, Normal);
        Occlusion += max(0.0, Unoccluded - IntegrateHorizon(Horizons, Axis, View, Normal));
    }
    Output[DispatchId.xy] = float4((1.0 - saturate(Occlusion / float(DirectionCount))).xxx, 1.0);
}

[numthreads(16, 16, 1)]
void BlurMain(uint3 DispatchId : SV_DispatchThreadID)
{
    RWTexture2D<float4> Output = ResourceDescriptorHeap[OutputIndex];
    Texture2D<float4> Source = ResourceDescriptorHeap[SourceIndex];
    Texture2D<float> Depth = ResourceDescriptorHeap[DepthIndex];
    Texture2D<float4> Normals = ResourceDescriptorHeap[NormalIndex];
    uint2 Size;
    Output.GetDimensions(Size.x, Size.y);
    if (any(DispatchId.xy >= Size))
    {
        return;
    }

    const int2 Pixel = int2(DispatchId.xy);
    const float DeviceDepth = Depth.Load(int3(Pixel, 0));
    if (DeviceDepth >= 1.0)
    {
        Output[Pixel] = 1.0;
        return;
    }

    uint2 SourceSize;
    Source.GetDimensions(SourceSize.x, SourceSize.y);
    const float3 Normal = DecodeOctNormal(Normals.Load(int3(Pixel, 0)).xy);
    const float2 Position = (float2(Pixel) - 1.0) * 0.5;
    const int2 Base = int2(floor(Position));
    const float2 Fraction = frac(Position);
    float Sum = 0.0;
    float WeightSum = 0.0;
    for (uint I = 0; I < 4; ++I)
    {
        const int2 Offset = int2(I & 1, I >> 1);
        const int2 Tap = clamp(Base + Offset, int2(0), int2(SourceSize) - 1);
        const int2 SourcePixel = min(Tap * 2 + 1, int2(Size) - 1);
        const float TapDepth = Depth.Load(int3(SourcePixel, 0));
        if (TapDepth < 1.0)
        {
            const float3 TapNormal = DecodeOctNormal(Normals.Load(int3(SourcePixel, 0)).xy);
            const float2 Bilinear = lerp(1.0 - Fraction, Fraction, float2(Offset));
            const float RelativeDepth = abs(TapDepth - DeviceDepth) / max(1.0 - DeviceDepth, 1e-5);
            const float Weight = Bilinear.x * Bilinear.y * exp(-64.0 * RelativeDepth) * Pow4(saturate(dot(Normal, TapNormal)));
            Sum += Source.Load(int3(Tap, 0)).r * Weight;
            WeightSum += Weight;
        }
    }
    const float AO = WeightSum > 1e-4 ? Sum / WeightSum : 1.0;
    Output[Pixel] = float4(AO.xxx, 1.0);
}
