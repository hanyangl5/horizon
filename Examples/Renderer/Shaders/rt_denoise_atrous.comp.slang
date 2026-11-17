#include "RTDenoiseBindings.slang"

// One 5x5 B3-spline a-trous iteration. Taps stop at depth discontinuities by their distance to the center pixel's
// tangent plane, relative to view distance, and at creases by the normal difference.
static const float kKernel[3] = { 3.0 / 8.0, 1.0 / 4.0, 1.0 / 16.0 };
static const float kNormalPower = 8.0;

[shader("compute")]
[numthreads(16, 16, 1)]
void main(uint3 thread : SV_DispatchThreadID)
{
    RWTexture2D<float2> output = ResourceDescriptorHeap[outputIndex];
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
    const float2      center = input.Load(int3(pixel, 0));
    const float       depth = depthTexture.Load(int3(pixel, 0));
    if (depth >= 1.0)
    {
        output[pixel] = center;
        return;
    }

    StructuredBuffer<FrameData> frames = ResourceDescriptorHeap[frameIndex];
    Texture2D<float4>           normals = ResourceDescriptorHeap[normalIndex];
    const float4x4              inverseViewProjection = frames[0].inverseViewProjection;
    const float3 position = reconstructWorldPosition((float2(pixel) + 0.5) / float2(size), depth, inverseViewProjection);
    const float3 normal = decodeOctNormal(normals.Load(int3(pixel, 0)).xy);
    const float  planeScale = 1.0 / (planeSigma * length(frames[0].eye.xyz - position));
    float2       sum = center * (kKernel[0] * kKernel[0]);
    float        weightSum = kKernel[0] * kKernel[0];
    for (int y = -2; y <= 2; ++y)
    {
        for (int x = -2; x <= 2; ++x)
        {
            const int2 tap = pixel + int2(x, y) * int(stepWidth);
            if ((x == 0 && y == 0) || any(tap < 0) || any(tap >= size))
            {
                continue;
            }
            const float tapDepth = depthTexture.Load(int3(tap, 0));
            if (tapDepth >= 1.0)
            {
                continue;
            }
            const float3 tapPosition = reconstructWorldPosition((float2(tap) + 0.5) / float2(size), tapDepth, inverseViewProjection);
            const float3 tapNormal = decodeOctNormal(normals.Load(int3(tap, 0)).xy);
            const float  weight = kKernel[abs(x)] * kKernel[abs(y)] * pow(saturate(dot(normal, tapNormal)), kNormalPower) *
                                 exp(-abs(dot(normal, tapPosition - position)) * planeScale);
            sum += input.Load(int3(tap, 0)) * weight;
            weightSum += weight;
        }
    }
    output[pixel] = sum / weightSum;
}
