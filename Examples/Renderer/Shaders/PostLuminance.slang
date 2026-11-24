cbuffer RootConstant0
{
    uint  InputIndex;
    uint  OutputIndex;
    uint  PreviousIndex;
    float AdaptationTime;
};

[shader("compute")]
[numthreads(16, 16, 1)]
void CSReduce(uint3 ID : SV_DispatchThreadID)
{
    Texture2D<float>   Input = ResourceDescriptorHeap[InputIndex];
    RWTexture2D<float> Output = ResourceDescriptorHeap[OutputIndex];
    uint               Width;
    uint               Height;
    Output.GetDimensions(Width, Height);
    if (any(ID.xy >= uint2(Width, Height)))
    {
        return;
    }
    int2 Pixel = int2(ID.xy) * 2;
    Output[ID.xy] = 0.25 * (Input.Load(int3(Pixel, 0)) + Input.Load(int3(Pixel + int2(1, 0), 0)) + Input.Load(int3(Pixel + int2(0, 1), 0)) +
                            Input.Load(int3(Pixel + int2(1, 1), 0)));
}

[shader("compute")]
[numthreads(1, 1, 1)]
void CSInitialize(uint3 ID : SV_DispatchThreadID)
{
    RWTexture2D<float> Previous = ResourceDescriptorHeap[InputIndex];
    RWTexture2D<float> Output = ResourceDescriptorHeap[OutputIndex];
    Previous[uint2(0)] = 50.0;
    Output[uint2(0)] = 50.0;
}

[shader("compute")]
[numthreads(1, 1, 1)]
void CSAdapt(uint3 DispatchId : SV_DispatchThreadID)
{
    Texture2D<float>   Input = ResourceDescriptorHeap[InputIndex];
    Texture2D<float>   Previous = ResourceDescriptorHeap[PreviousIndex];
    RWTexture2D<float> Output = ResourceDescriptorHeap[OutputIndex];

    float LumCurr = Input.Load(int3(0, 0, 0));
    float LumPrev = Previous.Load(int3(0, 0, 0));

    LumCurr = exp2(LumCurr);
    float Factor = 1.0 - exp(-AdaptationTime);
    float NewAdaptation = LumPrev + (LumCurr - LumPrev) * Factor;

    Output[int2(0, 0)] = (float4(NewAdaptation)).x;
}
