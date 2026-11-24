cbuffer RootConstant0
{
    uint Source;
    uint Output;
    uint2 Pixel;
};

[numthreads(1, 1, 1)]
void CSMain()
{
    Texture2D<uint2> Ids = ResourceDescriptorHeap[Source];
    RWStructuredBuffer<uint2> Result = ResourceDescriptorHeap[Output];
    Result[0] = Ids.Load(int3(Pixel, 0));
}
