#include "RTDenoiseBindings.slang"
static const float2 kCrossOffsets[4] = { float2(1.0, 0.0), float2(-1.0, 0.0), float2(0.0, 1.0), float2(0.0, -1.0) };

static const float2 kDiagonalOffsets[4] = { float2(1.0, 1.0), float2(-1.0, 1.0), float2(1.0, -1.0), float2(-1.0, -1.0) };

float edgeWeightFast(float centerDepth, float sampleDepth) { return exp2(-abs(sampleDepth - centerDepth) / max(depthThreshold, 1e-6)); }

[shader("compute")]
[numthreads(16, 16, 1)]
void main(uint3 dispatchId : SV_DispatchThreadID)
{
    RWTexture2D<float2> output = ResourceDescriptorHeap[outputIndex];
    int2                size = textureBindlessSize2D(inputIndex);
    int2                xy = int2(dispatchId.xy);

    if (xy.x >= size.x || xy.y >= size.y)
    {
        return;
    }

    float2 uv = (float2(xy) + float2(0.5)) / float2(size);
    float2 texelSize = float2(1.0) / float2(size);

    half2 centerColor = half2(textureBindless2D(inputIndex, samplerIndex, uv).rg);
    float centerDepth = textureBindless2D(depthIndex, samplerIndex, uv).r;

    float2 stepTexel = texelSize * float(stepWidth);

    half2 sumColor = centerColor * 0.5h;
    float totalWeight = 0.5;

    for (int i = 0; i < 4; ++i)
    {
        float2 offset = (stepWidth == 1u ? kCrossOffsets[i] : kDiagonalOffsets[i]) * stepTexel;
        float2 sampleUV = uv + offset;
        half2  sampleColor = half2(textureBindless2D(inputIndex, samplerIndex, sampleUV).rg);
        float  sampleDepth = textureBindless2D(depthIndex, samplerIndex, sampleUV).r;

        float w = 0.125 * edgeWeightFast(centerDepth, sampleDepth);
        sumColor += sampleColor * half(w);
        totalWeight += w;
    }

    half2 result = totalWeight > 0.001 ? sumColor / half(totalWeight) : centerColor;
    output[xy] = (float4(result, 0.0h, 1.0h)).xy;
}
