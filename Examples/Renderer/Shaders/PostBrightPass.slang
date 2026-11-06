#include "PostExposure.slang"

cbuffer RootConstant0
{
    uint  sceneColorIndex;
    uint  brightOutputIndex;
    uint  luminanceOutputIndex;
    uint  samplerIndex;
    float exposure;
    uint  enableBloom;
    uint  adaptedLuminanceIndex;
    uint  autoExposure;
};

[shader("compute")]
[numthreads(16, 16, 1)]
void CSMain(uint3 dispatchId : SV_DispatchThreadID)
{
    Texture2D<float4>   sceneColor = ResourceDescriptorHeap[sceneColorIndex];
    Texture2D<float>    adapted = ResourceDescriptorHeap[adaptedLuminanceIndex];
    RWTexture2D<float4> brightOutput = ResourceDescriptorHeap[brightOutputIndex];
    RWTexture2D<float>  luminanceOutput = ResourceDescriptorHeap[luminanceOutputIndex];
    SamplerState        clampSampler = SamplerDescriptorHeap[samplerIndex];
    uint                width;
    uint                height;
    brightOutput.GetDimensions(width, height);

    const float2 sizeOut = float2(width, height);

    const float2 xy = dispatchId.xy;
    const float2 uv0 = (dispatchId.xy + float2(0)) / sizeOut;
    const float2 uv1 = (dispatchId.xy + float2(1)) / sizeOut;

    if (xy.x >= sizeOut.x || xy.y >= sizeOut.y)
    {
        return;
    }

    float2 dxdy = (uv1 - uv0) / 3;

    float4 color = float4(0);

    for (int v = 0; v != 3; v++)
    {
        for (int u = 0; u != 3; u++)
        {
            color += sceneColor.SampleLevel(clampSampler, uv0 + (float2(u, v) + 0.5) * dxdy, 0);
        }
    }

    color /= 9.0;
    float  luminance = max(dot(color.rgb, float3(0.2126, 0.7152, 0.0722)), 0.0001);
    float  averageLuminance = adapted.Load(int3(0, 0, 0));
    float  scale = autoExposure != 0 ? exposureScale(averageLuminance, exposure) : exposure;
    float3 exposed = color.rgb * scale;
    float  exposedLuminance = luminance * scale;
    // Soft knee around an exposure-relative threshold of 1.
    float  knee = clamp(exposedLuminance - 0.5, 0.0, 1.0);
    float  contribution = max(exposedLuminance - 1.0, knee * knee * 0.5);
    float3 bright = exposed * (contribution / max(exposedLuminance, 0.0001));
    brightOutput[int2(xy)] = float4(enableBloom != 0 ? bright : float3(0.0), 1.0);
    // Mip averaging in log space yields a geometric mean for exposure metering.
    luminanceOutput[int2(xy)] = log2(luminance);
}
