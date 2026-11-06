// Adapted from 3D Graphics Rendering Cookbook, Second Edition (Packt, 2024).
// See ../Cookbook-LICENSE.txt. Uses the original rotation texels, eight samples and depth-aware 17-tap blur.
#include "Common.slang"

cbuffer RootConstant0
{
    uint  frameIndex;
    uint  depthIndex;
    uint  rotationsIndex;
    uint  sourceIndex;
    uint  outputIndex;
    uint  samplerIndex;
    uint  rotationSamplerIndex;
    uint  horizontal;
    uint  occlusionIndex;
    float radius;
    float attenuationScale;
    float distanceScale;
    float depthThreshold;
    float scale;
    float bias;
};

static const float3 offsets[8] = { float3(-0.5, -0.5, -0.5), float3(0.5, -0.5, -0.5), float3(-0.5, 0.5, -0.5), float3(0.5, 0.5, -0.5),
                                   float3(-0.5, -0.5, 0.5),  float3(0.5, -0.5, 0.5),  float3(-0.5, 0.5, 0.5),  float3(0.5, 0.5, 0.5) };

float scaleZ(float smpl, float2 inverseProjection)
{
    // For an affine camera pose, inverse(VP)'s last row is inverse(P)'s last row.
    // Preserve the Cookbook's negative view-space Z without hard-coding the camera's near/far planes.
    return -rcp(smpl * inverseProjection.x + inverseProjection.y);
}

[numthreads(16, 16, 1)]
void CSMain(uint3 dispatchId : SV_DispatchThreadID)
{
    Texture2D<float>            depthTexture = ResourceDescriptorHeap[depthIndex];
    Texture2D<float4>           rotations = ResourceDescriptorHeap[rotationsIndex];
    RWTexture2D<float4>         output = ResourceDescriptorHeap[outputIndex];
    SamplerState                sampler = SamplerDescriptorHeap[samplerIndex];
    SamplerState                noiseSampler = SamplerDescriptorHeap[rotationSamplerIndex];
    StructuredBuffer<FrameData> frame = ResourceDescriptorHeap[frameIndex];
    float2                      inverseProjection = float2(frame[0].inverseViewProjection[3][2], frame[0].inverseViewProjection[3][3]);
    uint                        width;
    uint                        height;
    depthTexture.GetDimensions(width, height);

    const float2 size = float2(width, height);

    const float2 xy = dispatchId.xy;
    const float2 uv = (dispatchId.xy + float2(0.5)) / size;

    if (xy.x >= size.x || xy.y >= size.y)
    {
        return;
    }

    const float  Z = scaleZ(depthTexture.SampleLevel(sampler, uv, 0), inverseProjection);
    const float3 plane = rotations.SampleLevel(noiseSampler, xy / 4.0, 0).xyz - float3(1.0);

    float att = 0.0;

    for (int i = 0; i < 8; i++)
    {
        float3 rSample = reflect(offsets[i], plane);
        float  zSample = scaleZ(depthTexture.SampleLevel(sampler, uv + radius * rSample.xy / Z, 0), inverseProjection);
        float  dist = max(zSample - Z, 0.0) / distanceScale;
        float  occl = 15.0 * max(dist * (2.0 - dist), 0.0);
        att += 1.0 / (1.0 + occl * occl);
    }

    att = clamp(att * att / 64.0 + 0.45, 0.0, 1.0) * attenuationScale;

    output[int2(xy)] = (float4(float3(att), 1.0));
}

static const float gaussWeights[17] = {
    0.00001525878906,
    0.0002441406250,
    0.001831054688,
    0.008544921875,
    0.02777099609,
    0.06665039063,
    0.1221923828,
    0.1745605469,
    0.1963806152,
    0.1745605469,
    0.1221923828,
    0.06665039063,
    0.02777099609,
    0.008544921875,
    0.001831054688,
    0.0002441406250,
    0.00001525878906
};

[numthreads(16, 16, 1)]
void BlurMain(uint3 dispatchId : SV_DispatchThreadID)
{
    Texture2D<float4>   source = ResourceDescriptorHeap[sourceIndex];
    Texture2D<float>    depthTexture = ResourceDescriptorHeap[depthIndex];
    RWTexture2D<float4> output = ResourceDescriptorHeap[outputIndex];
    SamplerState        sampler = SamplerDescriptorHeap[samplerIndex];
    uint                width;
    uint                height;
    source.GetDimensions(width, height);

    const float2 size = float2(width, height);
    const float2 xy = dispatchId.xy;

    if (xy.x >= size.x || xy.y >= size.y)
    {
        return;
    }

    const float2 texCoord = (dispatchId.xy + float2(0.5)) / size;

    const float texScaler = 1.0 / ((horizontal != 0) ? size.x : size.y);

    float3 c = float3(0.0);

    float3 fragColor = source.SampleLevel(sampler, texCoord, 0).rgb;
    float  fragDepth = depthTexture.SampleLevel(sampler, texCoord, 0);

    for (int i = 0; i != 17; i++)
    {
        float  offset = float(i - 17 / 2);
        float2 uv = texCoord + texScaler * ((horizontal != 0) ? float2(offset, 0) : float2(0, offset));
        float3 color = source.SampleLevel(sampler, uv, 0).rgb;
        float  depth = depthTexture.SampleLevel(sampler, uv, 0);

        float weight = clamp(abs(depth - fragDepth) * depthThreshold, 0.0, 1.0);
        c += lerp(color, fragColor, weight) * gaussWeights[i];
    }

    output[int2(xy)] = (float4(c, 1.0));
}

[numthreads(16, 16, 1)]
void CombineMain(uint3 dispatchId : SV_DispatchThreadID)
{
    Texture2D<float4> source = ResourceDescriptorHeap[sourceIndex];
    uint              width;
    uint              height;
    source.GetDimensions(width, height);
    if (dispatchId.x >= width || dispatchId.y >= height)
    {
        return;
    }
    const float2        uv = (float2(dispatchId.xy) + 0.5) / float2(width, height);
    Texture2D<float4>   ao = ResourceDescriptorHeap[occlusionIndex];
    SamplerState        sampler = SamplerDescriptorHeap[samplerIndex];
    const float4        color = source.SampleLevel(sampler, uv, 0);
    const float         ssao = clamp(ao.SampleLevel(sampler, uv, 0).r + bias, 0.0, 1.0);
    RWTexture2D<float4> output = ResourceDescriptorHeap[outputIndex];
    output[dispatchId.xy] = float4(lerp(color, color * ssao, scale).rgb, 1.0);
}
