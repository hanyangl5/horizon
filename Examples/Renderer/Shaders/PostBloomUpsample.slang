cbuffer RootConstant0
{
    uint downIndex;
    uint inputIndex;
    uint outputIndex;
    uint samplerIndex;
};

[shader("compute")]
[numthreads(16, 16, 1)]
void CSMain(uint3 dispatchId : SV_DispatchThreadID)
{
    Texture2D<float4>   down = ResourceDescriptorHeap[downIndex];
    Texture2D<float4>   input = ResourceDescriptorHeap[inputIndex];
    RWTexture2D<float4> output = ResourceDescriptorHeap[outputIndex];
    SamplerState        clampSampler = SamplerDescriptorHeap[samplerIndex];
    uint                width;
    uint                height;
    uint                inputWidth;
    uint                inputHeight;
    output.GetDimensions(width, height);
    input.GetDimensions(inputWidth, inputHeight);

    if (dispatchId.x >= width || dispatchId.y >= height)
    {
        return;
    }
    float2 uv = (dispatchId.xy + float2(0.5)) / float2(width, height);
    float2 texel = 1.0 / float2(inputWidth, inputHeight);
    // 3x3 tent; weights sum to 1 so the accumulation keeps the bright-pass energy.
    float3 blur = input.SampleLevel(clampSampler, uv + texel * float2(-1.0, -1.0), 0).rgb * 0.0625;
    blur += input.SampleLevel(clampSampler, uv + texel * float2(0.0, -1.0), 0).rgb * 0.125;
    blur += input.SampleLevel(clampSampler, uv + texel * float2(1.0, -1.0), 0).rgb * 0.0625;
    blur += input.SampleLevel(clampSampler, uv + texel * float2(-1.0, 0.0), 0).rgb * 0.125;
    blur += input.SampleLevel(clampSampler, uv, 0).rgb * 0.25;
    blur += input.SampleLevel(clampSampler, uv + texel * float2(1.0, 0.0), 0).rgb * 0.125;
    blur += input.SampleLevel(clampSampler, uv + texel * float2(-1.0, 1.0), 0).rgb * 0.0625;
    blur += input.SampleLevel(clampSampler, uv + texel * float2(0.0, 1.0), 0).rgb * 0.125;
    blur += input.SampleLevel(clampSampler, uv + texel * float2(1.0, 1.0), 0).rgb * 0.0625;
    // This level keeps its own downsample so wide and tight glare both reach the composite.
    output[dispatchId.xy] = float4(down.Load(int3(dispatchId.xy, 0)).rgb + blur, 1.0);
}
