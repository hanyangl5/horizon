cbuffer RootConstant0
{
    uint DownIndex;
    uint InputIndex;
    uint OutputIndex;
    uint SamplerIndex;
};

[shader("compute")]
[numthreads(16, 16, 1)]
void CSMain(uint3 DispatchId : SV_DispatchThreadID)
{
    Texture2D<float4>   Down = ResourceDescriptorHeap[DownIndex];
    Texture2D<float4>   Input = ResourceDescriptorHeap[InputIndex];
    RWTexture2D<float4> Output = ResourceDescriptorHeap[OutputIndex];
    SamplerState        ClampSampler = SamplerDescriptorHeap[SamplerIndex];
    uint                Width;
    uint                Height;
    uint                InputWidth;
    uint                InputHeight;
    Output.GetDimensions(Width, Height);
    Input.GetDimensions(InputWidth, InputHeight);

    if (DispatchId.x >= Width || DispatchId.y >= Height)
    {
        return;
    }
    float2 UV = (DispatchId.xy + float2(0.5)) / float2(Width, Height);
    float2 Texel = 1.0 / float2(InputWidth, InputHeight);
    // 3x3 tent; weights sum to 1 so the accumulation keeps the bright-pass energy.
    float3 Blur = Input.SampleLevel(ClampSampler, UV + Texel * float2(-1.0, -1.0), 0).rgb * 0.0625;
    Blur += Input.SampleLevel(ClampSampler, UV + Texel * float2(0.0, -1.0), 0).rgb * 0.125;
    Blur += Input.SampleLevel(ClampSampler, UV + Texel * float2(1.0, -1.0), 0).rgb * 0.0625;
    Blur += Input.SampleLevel(ClampSampler, UV + Texel * float2(-1.0, 0.0), 0).rgb * 0.125;
    Blur += Input.SampleLevel(ClampSampler, UV, 0).rgb * 0.25;
    Blur += Input.SampleLevel(ClampSampler, UV + Texel * float2(1.0, 0.0), 0).rgb * 0.125;
    Blur += Input.SampleLevel(ClampSampler, UV + Texel * float2(-1.0, 1.0), 0).rgb * 0.0625;
    Blur += Input.SampleLevel(ClampSampler, UV + Texel * float2(0.0, 1.0), 0).rgb * 0.125;
    Blur += Input.SampleLevel(ClampSampler, UV + Texel * float2(1.0, 1.0), 0).rgb * 0.0625;
    // This level keeps its own downsample so wide and tight glare both reach the composite.
    Output[DispatchId.xy] = float4(Down.Load(int3(DispatchId.xy, 0)).rgb + Blur, 1.0);
}
