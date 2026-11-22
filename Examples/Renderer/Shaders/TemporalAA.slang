// Neighborhood clipping adapted from Epic's TemporalAA.usf.
#include "Common.slang"

cbuffer RootConstant0
{
    uint CurrentIndex;
    uint OpaqueIndex;
    uint DepthIndex;
    uint MotionIndex;
    uint FrameIndex;
    uint HistoryIndex;
    uint OutputIndex;
    uint SamplerIndex;
    float2 Jitter;
    float2 PreviousJitter;
};

float3 ToYCoCg(float3 Color)
{
    return float3(dot(Color, float3(0.25, 0.5, 0.25)), 0.5 * (Color.r - Color.b),
                  dot(Color, float3(-0.25, 0.5, -0.25)));
}

float3 FromYCoCg(float3 Color)
{
    return float3(Color.x + Color.y - Color.z, Color.x + Color.z, Color.x - Color.y - Color.z);
}

float3 ClampHistory(float3 History, float3 Filtered, float3 NeighborMin, float3 NeighborMax)
{
    const float3 Delta = Filtered - History;
    const float3 Direction = select(abs(Delta) < (1.0 / 65536.0), 1.0 / 65536.0, Delta);
    const float3 Enter = min((NeighborMin - History) / Direction, (NeighborMax - History) / Direction);
    return lerp(History, Filtered, saturate(max(Enter.x, max(Enter.y, Enter.z))));
}

float4 ResolveTemporalAA(uint2 Thread, uint2 Size)
{
    Texture2D<float4> Current = ResourceDescriptorHeap[CurrentIndex];
    Texture2D<float> Depth = ResourceDescriptorHeap[DepthIndex];
    SamplerState Sampler = SamplerDescriptorHeap[SamplerIndex];
    const float2 UV = (float2(Thread) + 0.5) / float2(Size);
    const float2 CurrentUV = UV + Jitter;
    const int2 Pixel = clamp(int2(floor(CurrentUV * float2(Size))), int2(0), int2(Size) - 1);
    const float DeviceDepth = Depth.Load(int3(Pixel, 0));
    const float3 Filtered = max(Current.SampleLevel(Sampler, CurrentUV, 0).rgb, 0.0);

    // Sky and OIT pixels don't have reliable motion vectors. Keep them out of the opaque history.
    bool Reactive = DeviceDepth >= 1.0;
    if (OpaqueIndex != INVALID_TEXTURE)
    {
        Texture2D<float4> Opaque = ResourceDescriptorHeap[OpaqueIndex];
        const float3 OpaqueColor = Opaque.SampleLevel(Sampler, CurrentUV, 0).rgb;
        const float3 Difference = abs(Filtered - OpaqueColor);
        Reactive = Reactive || max(Difference.r, max(Difference.g, Difference.b)) > 0.01 * max(1.0, ToYCoCg(Filtered).x);
    }
    if (Reactive)
    {
        return float4(Filtered, 0.0);
    }

    StructuredBuffer<FrameData> Frames = ResourceDescriptorHeap[FrameIndex];
    const FrameData Frame = Frames[0];
    const float2 DepthUV = (float2(Pixel) + 0.5) / float2(Size);
    const float3 Position = ReconstructWorldPosition(DepthUV, DeviceDepth, Frame.InverseViewProjection);
    const float ViewDepth = mul(Frame.ViewProjection, float4(Position, 1.0)).w;
    const float4 CurrentResult = float4(Filtered, ViewDepth);
    if (HistoryIndex == INVALID_TEXTURE)
    {
        return CurrentResult;
    }

    const float4 PreviousClip = mul(Frame.PreviousViewProjection, float4(Position, 1.0));
    if (PreviousClip.w <= 0.0)
    {
        return CurrentResult;
    }
    Texture2D<float4> Motion = ResourceDescriptorHeap[MotionIndex];
    const float2 PackedMotion = DecodeMotionVector(Motion.Load(int3(Pixel, 0)).rgb);
    const float2 CameraMotion = DepthUV - (PreviousClip.xy / PreviousClip.w * float2(0.5, -0.5) + 0.5);
    // Use the exact matrix velocity where it agrees with the 12-bit motion within quantization error.
    const float2 Velocity = select(abs(CameraMotion - PackedMotion) <= (1.0 / 4095.0 + 1e-6), CameraMotion, PackedMotion);
    const float2 PreviousUV = CurrentUV - Velocity - PreviousJitter;
    if (any(PreviousUV < 0.0) || any(PreviousUV > 1.0))
    {
        return CurrentResult;
    }

    Texture2D<float4> HistoryTexture = ResourceDescriptorHeap[HistoryIndex];
    const float2 HistoryPixel = PreviousUV * float2(Size) - 0.5;
    const int2 Base = int2(floor(HistoryPixel));
    const float2 Fraction = frac(HistoryPixel);
    float3 HistorySum = 0.0;
    float HistoryWeight = 0.0;
    for (uint I = 0; I < 4; ++I)
    {
        const int2 Offset = int2(I & 1, I >> 1);
        const int2 Tap = clamp(Base + Offset, int2(0), int2(Size) - 1);
        const float4 History = HistoryTexture.Load(int3(Tap, 0));
        if (History.a > 0.0 && abs(History.a - PreviousClip.w) <= max(0.01, PreviousClip.w * 0.02))
        {
            const float2 Bilinear = lerp(1.0 - Fraction, Fraction, float2(Offset));
            const float Weight = Bilinear.x * Bilinear.y;
            HistorySum += History.rgb * Weight;
            HistoryWeight += Weight;
        }
    }
    if (HistoryWeight <= 1e-4)
    {
        return CurrentResult;
    }

    const float3 FilteredYCoCg = ToYCoCg(Filtered);
    float3 Minimum = FilteredYCoCg;
    float3 Maximum = FilteredYCoCg;
    for (int Y = -1; Y <= 1; ++Y)
    {
        for (int X = -1; X <= 1; ++X)
        {
            const int2 Tap = clamp(Pixel + int2(X, Y), int2(0), int2(Size) - 1);
            const float3 Color = ToYCoCg(Current.Load(int3(Tap, 0)).rgb);
            Minimum = min(Minimum, Color);
            Maximum = max(Maximum, Color);
        }
    }
    const float3 History = ClampHistory(ToYCoCg(HistorySum / HistoryWeight), FilteredYCoCg, Minimum, Maximum);
    return float4(max(FromYCoCg(lerp(History, FilteredYCoCg, 0.1)), 0.0), ViewDepth);
}


[numthreads(8, 8, 1)]
void CSMain(uint3 Thread : SV_DispatchThreadID)
{
    RWTexture2D<float4> Output = ResourceDescriptorHeap[OutputIndex];
    uint2 Size;
    Output.GetDimensions(Size.x, Size.y);
    if (all(Thread.xy < Size))
    {
        Output[Thread.xy] = ResolveTemporalAA(Thread.xy, Size);
    }
}
