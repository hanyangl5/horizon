#include "PostExposure.slang"

cbuffer RootConstant0
{
    uint sceneColorIndex;
    uint luminanceOutputIndex;
};

// Exposure metering only: bloom thresholds and blurs its own half-resolution chain.
[shader("compute")]
[numthreads(16, 16, 1)]
void CSMain(uint3 dispatchId : SV_DispatchThreadID)
{
    Texture2D<float4>  sceneColor = ResourceDescriptorHeap[sceneColorIndex];
    RWTexture2D<float> luminanceOutput = ResourceDescriptorHeap[luminanceOutputIndex];
    uint               width;
    uint               height;
    luminanceOutput.GetDimensions(width, height);

    if (dispatchId.x >= width || dispatchId.y >= height)
    {
        return;
    }
    uint sourceWidth;
    uint sourceHeight;
    sceneColor.GetDimensions(sourceWidth, sourceHeight);
    float2 footprint = float2(sourceWidth, sourceHeight) / float2(width, height);
    float2 lower = float2(dispatchId.xy) * footprint;
    float2 upper = float2(dispatchId.xy + 1) * footprint;
    int2   first = int2(floor(lower));
    int2   last = min(int2(ceil(upper)), int2(sourceWidth, sourceHeight));
    float3 color = float3(0.0);
    // Integrate the entire source footprint so subpixel highlights cannot fall between sparse samples.
    for (int y = first.y; y < last.y; ++y)
    {
        float weightY = max(min(upper.y, float(y + 1)) - max(lower.y, float(y)), 0.0);
        for (int x = first.x; x < last.x; ++x)
        {
            float weightX = max(min(upper.x, float(x + 1)) - max(lower.x, float(x)), 0.0);
            color += sceneColor.Load(int3(x, y, 0)).rgb * (weightX * weightY);
        }
    }
    color /= footprint.x * footprint.y;
    // Mip averaging in log space yields a geometric mean for exposure metering.
    luminanceOutput[dispatchId.xy] = log2(max(Luminance(color), 0.0001));
}
