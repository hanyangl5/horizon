cbuffer RootConstant0
{
    uint inputIndex;
    uint outputIndex;
    uint samplerIndex;
    uint horizontal;
};

[shader("compute")]
[numthreads(16, 16, 1)]
void CSMain(uint3 dispatchId : SV_DispatchThreadID)
{
    Texture2D<float4>   input = ResourceDescriptorHeap[inputIndex];
    RWTexture2D<float4> output = ResourceDescriptorHeap[outputIndex];
    SamplerState        clampSampler = SamplerDescriptorHeap[samplerIndex];
    uint                width;
    uint                height;
    input.GetDimensions(width, height);

    const float2 size = float2(width, height);
    const float2 xy = dispatchId.xy;

    if (xy.x >= size.x || xy.y >= size.y)
    {
        return;
    }

    const float2 texCoord = (dispatchId.xy + float2(0.5)) / size;
    const float2 texel = 1.0 / size;

    const float radius = (horizontal != 0) ? 1.5 : 2.5;

    float3 c = float3(0.0);
    c += input.SampleLevel(clampSampler, texCoord + texel * float2(-radius, 0.0), 0).rgb;
    c += input.SampleLevel(clampSampler, texCoord + texel * float2(radius, 0.0), 0).rgb;
    c += input.SampleLevel(clampSampler, texCoord + texel * float2(0.0, -radius), 0).rgb;
    c += input.SampleLevel(clampSampler, texCoord + texel * float2(0.0, radius), 0).rgb;
    c += 2.0 * input.SampleLevel(clampSampler, texCoord + texel * float2(-radius, -radius), 0).rgb;
    c += 2.0 * input.SampleLevel(clampSampler, texCoord + texel * float2(radius, -radius), 0).rgb;
    c += 2.0 * input.SampleLevel(clampSampler, texCoord + texel * float2(-radius, radius), 0).rgb;
    c += 2.0 * input.SampleLevel(clampSampler, texCoord + texel * float2(radius, radius), 0).rgb;
    c *= 1.0 / 12.0;

    output[int2(xy)] = (float4(c, 1.0));
}
