#include "RTDenoiseBindings.slang"

// Temporal accumulation of the mirror reflections. The input traces one deterministic ray per texel, so it does not
// need a spatial filter; what flickers is undersampled detail at half resolution, which history averages out. History
// follows the surface motion and is clipped to the current neighbourhood in YCoCg, so parallax in the reflection
// becomes a short lag instead of a ghost. Writes (radiance, history length); texels without a reflection write zero.
static const float kMaxHistoryLength = 32.0;
static const float kClipGamma = 1.25;

float3 toYCoCg(float3 rgb)
{
    return float3(dot(rgb, float3(0.25, 0.5, 0.25)), dot(rgb, float3(0.5, 0.0, -0.5)), dot(rgb, float3(-0.25, 0.5, -0.25)));
}

float3 fromYCoCg(float3 c) { return float3(c.x + c.y - c.z, c.x + c.z, c.x - c.y - c.z); }

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

    Texture2D<float4> input = ResourceDescriptorHeap[inputIndex];
    Texture2D<float>  depthTexture = ResourceDescriptorHeap[depthIndex];
    const int2        size = int2(width, height);
    const int2        pixel = int2(thread.xy);
    uint2             sourceSize;
    depthTexture.GetDimensions(sourceSize.x, sourceSize.y);
    const int2   source = rtSourcePixel(pixel, size, int2(sourceSize));
    const float4 current = input.Load(int3(pixel, 0));
    const float  depth = depthTexture.Load(int3(source, 0));
    // Alpha marks a traced reflection; sky and surfaces too rough for ray traced reflections have none.
    if (depth >= 1.0 || current.a <= 0.0)
    {
        output[pixel] = 0.0;
        return;
    }

    // Statistics over the traced neighbours only, so the roughness cutoff does not pull history toward black.
    float3 mean = 0.0;
    float3 meanSquare = 0.0;
    float  count = 0.0;
    for (int y = -1; y <= 1; ++y)
    {
        for (int x = -1; x <= 1; ++x)
        {
            const float4 value = input.Load(int3(clamp(pixel + int2(x, y), int2(0), size - 1), 0));
            if (value.a > 0.0)
            {
                const float3 c = toYCoCg(value.rgb);
                mean += c;
                meanSquare += c * c;
                count += 1.0;
            }
        }
    }
    mean /= count;
    const float3 extent = kClipGamma * sqrt(max(meanSquare / count - mean * mean, 0.0));

    float3 history = current.rgb;
    float  historyLength = 0.0;
    if (historyValid != 0)
    {
        StructuredBuffer<FrameData> frames = ResourceDescriptorHeap[frameIndex];
        Texture2D<float4>           historyTexture = ResourceDescriptorHeap[historyIndex];
        Texture2D<float>            previousDepth = ResourceDescriptorHeap[previousDepthIndex];
        const float3                worldPosition =
            reconstructWorldPosition((float2(source) + 0.5) / float2(sourceSize), depth, frames[0].inverseViewProjection);
        const float4 previousClip = mul(frames[0].previousViewProjection, float4(worldPosition, 1.0));
        const float2 previousSource = (previousClip.xy / previousClip.w * float2(0.5, -0.5) + 0.5) * float2(sourceSize) - 0.5;
        const float2 previousPixel = previousSource * float2(size) / float2(sourceSize);
        const int2   base = int2(floor(previousPixel));
        const float2 fraction = previousPixel - float2(base);
        float4       sum = 0.0;
        float        weightSum = 0.0;
        for (int i = 0; i < 4; ++i)
        {
            const int2   offset = int2(i & 1, i >> 1);
            const int2   tap = base + offset;
            const float4 value = previousClip.w > 0.0 && all(tap >= 0) && all(tap < size) ? historyTexture.Load(int3(tap, 0)) : 0.0;
            // Taps of another surface or without accumulated reflection are skipped.
            if (value.a <= 0.0 || abs(previousDepth.Load(int3(tap, 0)) - previousClip.w) > depthThreshold * previousClip.w)
            {
                continue;
            }
            const float2 bilinear = lerp(1.0 - fraction, fraction, float2(offset));
            sum += value * (bilinear.x * bilinear.y);
            weightSum += bilinear.x * bilinear.y;
        }
        if (weightSum > 1e-3)
        {
            sum /= weightSum;
            history = fromYCoCg(clamp(toYCoCg(sum.rgb), mean - extent, mean + extent));
            historyLength = sum.a;
        }
    }
    historyLength = min(historyLength + 1.0, kMaxHistoryLength);
    output[pixel] = float4(max(lerp(history, current.rgb, max(1.0 / historyLength, temporalAlpha)), 0.0), historyLength);
}
