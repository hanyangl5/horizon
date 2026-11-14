#include "PostExposure.slang"

cbuffer RootConstant0
{
    uint  inputIndex;
    uint  outputIndex;
    uint  samplerIndex;
    uint  adaptedLuminanceIndex;
    uint  threshold;
    uint  autoExposure;
    float exposure;
};

[shader("compute")]
[numthreads(16, 16, 1)]
void CSMain(uint3 dispatchId : SV_DispatchThreadID)
{
    Texture2D<float4>   input = ResourceDescriptorHeap[inputIndex];
    RWTexture2D<float4> output = ResourceDescriptorHeap[outputIndex];
    Texture2D<float>    adapted = ResourceDescriptorHeap[adaptedLuminanceIndex];
    SamplerState        clampSampler = SamplerDescriptorHeap[samplerIndex];
    uint                width;
    uint                height;
    output.GetDimensions(width, height);

    if (dispatchId.x >= width || dispatchId.y >= height)
    {
        return;
    }
    // Every level is half the previous one, so one bilinear tap at the texel centre is an exact 2x2 box average.
    float2 uv = (dispatchId.xy + float2(0.5)) / float2(width, height);
    float3 color = input.SampleLevel(clampSampler, uv, 0).rgb;
    // A single non-finite texel must not turn the whole glow into NaN.
    color = all(isfinite(color)) ? color : float3(0.0);
    if (threshold != 0)
    {
        float averageLuminance = adapted.Load(int3(0, 0, 0));
        float scale = autoExposure != 0 ? exposureScale(averageLuminance, exposure) : exposure;
        color *= scale;
        // Soft knee around an exposure-relative threshold of 1; a 2x2 average still keeps subpixel highlights.
        float exposedLuminance = max(luminance(color), 0.0001);
        float knee = clamp(exposedLuminance - 0.5, 0.0, 1.0);
        color *= max(exposedLuminance - 1.0, knee * knee * 0.5) / exposedLuminance;
    }
    output[dispatchId.xy] = float4(color, 1.0);
}
