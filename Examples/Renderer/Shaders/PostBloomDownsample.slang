#include "PostExposure.slang"

cbuffer RootConstant0
{
    uint  InputIndex;
    uint  OutputIndex;
    uint  SamplerIndex;
    uint  AdaptedLuminanceIndex;
    uint  Threshold;
    uint  AutoExposure;
    float Exposure;
};

[shader("compute")]
[numthreads(16, 16, 1)]
void CSMain(uint3 DispatchId : SV_DispatchThreadID)
{
    Texture2D<float4>   Input = ResourceDescriptorHeap[InputIndex];
    RWTexture2D<float4> Output = ResourceDescriptorHeap[OutputIndex];
    Texture2D<float>    Adapted = ResourceDescriptorHeap[AdaptedLuminanceIndex];
    SamplerState        ClampSampler = SamplerDescriptorHeap[SamplerIndex];
    uint                Width;
    uint                Height;
    Output.GetDimensions(Width, Height);

    if (DispatchId.x >= Width || DispatchId.y >= Height)
    {
        return;
    }
    // Every level is half the previous one, so one bilinear tap at the texel centre is an exact 2x2 box average.
    float2 UV = (DispatchId.xy + float2(0.5)) / float2(Width, Height);
    float3 Color = Input.SampleLevel(ClampSampler, UV, 0).rgb;
    // A single non-finite texel must not turn the whole glow into NaN.
    Color = all(isfinite(Color)) ? Color : float3(0.0);
    if (Threshold != 0)
    {
        float AverageLuminance = Adapted.Load(int3(0, 0, 0));
        float Scale = AutoExposure != 0 ? ExposureScale(AverageLuminance, Exposure) : Exposure;
        Color *= Scale;
        // Soft knee around an exposure-relative threshold of 1; a 2x2 average still keeps subpixel highlights.
        float ExposedLuminance = max(Luminance(Color), 0.0001);
        float Knee = clamp(ExposedLuminance - 0.5, 0.0, 1.0);
        Color *= max(ExposedLuminance - 1.0, Knee * Knee * 0.5) / ExposedLuminance;
    }
    Output[DispatchId.xy] = float4(Color, 1.0);
}
