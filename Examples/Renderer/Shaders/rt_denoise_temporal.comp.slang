#include "RTDenoiseBindings.slang"

// Writes (shadow, AO, history length). The blend weight follows the per-pixel sample count down to temporalAlpha, so
// disoccluded pixels converge quickly and settled pixels average many frames.
static const float kMaxHistoryLength = 64.0;
// The 1 spp input is binary, so the clip box needs a floor: a converged penumbra must survive a neighborhood whose
// samples all happened to agree this frame.
static const float kClipGamma = 1.5;
static const float kClipFloor = 0.1;

[shader("compute")]
[numthreads(16, 16, 1)]
void main(uint3 thread : SV_DispatchThreadID)
{
    RWTexture2D<float4> output = ResourceDescriptorHeap[outputIndex];
    uint                width;
    uint                height;
    output.GetDimensions(width, height);
    if (thread.x >= width || thread.y >= height)
    {
        return;
    }

    Texture2D<float2> input = ResourceDescriptorHeap[inputIndex];
    Texture2D<float>  depthTexture = ResourceDescriptorHeap[depthIndex];
    const int2        size = int2(width, height);
    const int2        pixel = int2(thread.xy);
    uint2             sourceSize;
    depthTexture.GetDimensions(sourceSize.x, sourceSize.y);
    const int2   source = rtSourcePixel(pixel, size, int2(sourceSize));
    const float2 current = input.Load(int3(pixel, 0));
    const float  depth = depthTexture.Load(int3(source, 0));
    if (depth >= 1.0)
    {
        output[pixel] = float4(1.0, 1.0, 0.0, 0.0);
        return;
    }

    float2 mean = 0.0;
    float2 meanSquare = 0.0;
    for (int y = -1; y <= 1; ++y)
    {
        for (int x = -1; x <= 1; ++x)
        {
            const float2 value = input.Load(int3(clamp(pixel + int2(x, y), int2(0), size - 1), 0));
            mean += value;
            meanSquare += value * value;
        }
    }
    mean /= 9.0;
    const float2 extent = kClipGamma * sqrt(max(meanSquare / 9.0 - mean * mean, 0.0)) + kClipFloor;

    float2 history = current;
    float  historyLength = 0.0;
    if (historyValid != 0)
    {
        StructuredBuffer<FrameData> frames = ResourceDescriptorHeap[frameIndex];
        Texture2D<float4>           historyTexture = ResourceDescriptorHeap[historyIndex];
        Texture2D<float>            previousDepth = ResourceDescriptorHeap[previousDepthIndex];
        const float3 worldPosition =
            reconstructWorldPosition((float2(source) + 0.5) / float2(sourceSize), depth, frames[0].inverseViewProjection);
        const float4 previousClip = mul(frames[0].previousViewProjection, float4(worldPosition, 1.0));
        // Texels sit at their source pixels, so a static view maps each texel back onto itself.
        const float2 previousSource = (previousClip.xy / previousClip.w * float2(0.5, -0.5) + 0.5) * float2(sourceSize) - 0.5;
        const float2 previousPixel = previousSource * float2(size) / float2(sourceSize);
        const int2   base = int2(floor(previousPixel));
        const float2 fraction = previousPixel - float2(base);
        float4       sum = 0.0;
        float        weightSum = 0.0;
        // Bilinear history with each tap validated separately, so a silhouette never blends foreground into background.
        for (int i = 0; i < 4; ++i)
        {
            const int2 offset = int2(i & 1, i >> 1);
            const int2 tap = base + offset;
            if (previousClip.w <= 0.0 || any(tap < 0) || any(tap >= size) ||
                abs(previousDepth.Load(int3(tap, 0)) - previousClip.w) > depthThreshold * previousClip.w)
            {
                continue;
            }
            const float2 bilinear = lerp(1.0 - fraction, fraction, float2(offset));
            sum += historyTexture.Load(int3(tap, 0)) * (bilinear.x * bilinear.y);
            weightSum += bilinear.x * bilinear.y;
        }
        if (weightSum > 1e-3)
        {
            sum /= weightSum;
            history = clamp(sum.xy, mean - extent, mean + extent);
            historyLength = sum.z;
        }
    }
    historyLength = min(historyLength + 1.0, kMaxHistoryLength);
    output[pixel] = float4(lerp(history, current, max(1.0 / historyLength, temporalAlpha)), historyLength, 0.0);
}
