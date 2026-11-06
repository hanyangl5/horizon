#include "RTDenoiseBindings.slang"
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

    half2 current = half2(textureBindless2D(inputIndex, samplerIndex, uv).rg);

    float depth = textureBindless2D(depthIndex, samplerIndex, uv).r;

    if (depth >= 0.9999)
    {
        output[xy] = (float4(1.0, 1.0, 0.0, 1.0)).xy;
        return;
    }

    if (historyValid == 0)
    {
        output[xy] = current;
        return;
    }

    StructuredBuffer<FrameData> frames = ResourceDescriptorHeap[frameIndex];
    float3                      worldPos = reconstructWorldPosition(uv, depth, frames[0].inverseViewProjection);
    float2                      prevUV = reprojectToPrevUV(worldPos, frames[0].previousViewProjection);

    const float margin = 2.0 / float(size.x);
    bool        valid = all((prevUV > float2(margin))) && all((prevUV < float2(1.0 - margin)));

    if (valid)
    {
        float  prevFrameDepth = textureBindless2D(previousDepthIndex, samplerIndex, prevUV).r;
        float4 prevClip = mul(frames[0].previousViewProjection, float4(worldPos, 1.0));
        float  expectedDepth = prevClip.z / prevClip.w;
        if (abs(expectedDepth - prevFrameDepth) > depthThreshold)
        {
            valid = false;
        }
    }

    half2  nMin = current;
    half2  nMax = current;
    float2 texelSize = float2(1.0) / float2(size);
    for (int dy = -1; dy <= 1; dy++)
    {
        for (int dx = -1; dx <= 1; dx++)
        {
            half2 s = half2(textureBindless2D(inputIndex, samplerIndex, uv + float2(dx, dy) * texelSize).rg);
            nMin = min(nMin, s);
            nMax = max(nMax, s);
        }
    }

    half2 result;
    if (valid)
    {
        half2 history = half2(textureBindless2D(historyIndex, samplerIndex, prevUV).rg);

        history = clamp(history, nMin, nMax);

        result = lerp(history, current, half(temporalAlpha));
    }
    else
    {
        result = current;
    }

    output[xy] = (float4(result, 0.0h, 1.0h)).xy;
}
