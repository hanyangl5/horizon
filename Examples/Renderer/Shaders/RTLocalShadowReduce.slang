#include "RTLocalShadowConfig.h"

cbuffer RootConstant0 { uint InputIndex, OutputIndex, Width, Height; }
groupshared uint Sums[LOCAL_SHADOW_GROUP_THREADS];

[numthreads(LOCAL_SHADOW_GROUP_SIZE, LOCAL_SHADOW_GROUP_SIZE, 1)]
void ReduceLocalShadowCounts(uint2 Pixel : SV_DispatchThreadID, uint2 Group : SV_GroupID, uint Lane : SV_GroupIndex)
{
    Texture2D<uint>   Input = ResourceDescriptorHeap[InputIndex];
    RWTexture2D<uint> Output = ResourceDescriptorHeap[OutputIndex];
    uint              Sum = 0;
    // The final group covers all remaining blocks, including very wide render targets.
    uint2             Size;
    Output.GetDimensions(Size.x, Size.y);
    uint2 Step = all(Size == 1) ? uint2(LOCAL_SHADOW_GROUP_SIZE, LOCAL_SHADOW_GROUP_SIZE) : uint2(Width, Height);
    for (uint Y = Pixel.y; Y < Height; Y += Step.y)
    {
        for (uint X = Pixel.x; X < Width; X += Step.x)
        {
            Sum += Input.Load(int3(X, Y, 0));
        }
    }
    Sums[Lane] = Sum;
    GroupMemoryBarrierWithGroupSync();
    for (uint Offset = LOCAL_SHADOW_GROUP_THREADS / 2; Offset > 0; Offset >>= 1)
    {
        if (Lane < Offset)
        {
            Sums[Lane] += Sums[Lane + Offset];
        }
        GroupMemoryBarrierWithGroupSync();
    }
    if (Lane == 0)
    {
        Output[Group] = Sums[0];
    }
}
