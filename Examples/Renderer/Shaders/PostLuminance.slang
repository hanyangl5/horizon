cbuffer RootConstant0
{
    uint  inputIndex;
    uint  outputIndex;
    uint  previousIndex;
    float adaptationTime;
};

[shader("compute")]
[numthreads(16, 16, 1)]
void CSReduce(uint3 id : SV_DispatchThreadID)
{
    Texture2D<float>   input = ResourceDescriptorHeap[inputIndex];
    RWTexture2D<float> output = ResourceDescriptorHeap[outputIndex];
    uint               width;
    uint               height;
    output.GetDimensions(width, height);
    if (any(id.xy >= uint2(width, height)))
    {
        return;
    }
    int2 pixel = int2(id.xy) * 2;
    output[id.xy] = 0.25 * (input.Load(int3(pixel, 0)) + input.Load(int3(pixel + int2(1, 0), 0)) + input.Load(int3(pixel + int2(0, 1), 0)) +
                            input.Load(int3(pixel + int2(1, 1), 0)));
}

[shader("compute")]
[numthreads(1, 1, 1)]
void CSInitialize(uint3 id : SV_DispatchThreadID)
{
    RWTexture2D<float> previous = ResourceDescriptorHeap[inputIndex];
    RWTexture2D<float> output = ResourceDescriptorHeap[outputIndex];
    previous[uint2(0)] = 50.0;
    output[uint2(0)] = 50.0;
}

[shader("compute")]
[numthreads(1, 1, 1)]
void CSAdapt(uint3 dispatchId : SV_DispatchThreadID)
{
    Texture2D<float>   input = ResourceDescriptorHeap[inputIndex];
    Texture2D<float>   previous = ResourceDescriptorHeap[previousIndex];
    RWTexture2D<float> output = ResourceDescriptorHeap[outputIndex];

    float lumCurr = input.Load(int3(0, 0, 0));
    float lumPrev = previous.Load(int3(0, 0, 0));

    lumCurr = exp2(lumCurr);
    float factor = 1.0 - exp(-adaptationTime);
    float newAdaptation = lumPrev + (lumCurr - lumPrev) * factor;

    output[int2(0, 0)] = (float4(newAdaptation)).x;
}
