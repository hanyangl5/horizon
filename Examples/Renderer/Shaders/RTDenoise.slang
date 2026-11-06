#include "RTDenoiseBindings.slang"

[shader("compute")]
[numthreads(16, 16, 1)]
void CSCopyDepth(uint3 thread : SV_DispatchThreadID)
{
    RWTexture2D<float> output = ResourceDescriptorHeap[outputIndex];
    uint               width;
    uint               height;
    output.GetDimensions(width, height);
    if (thread.x >= width || thread.y >= height)
    {
        return;
    }
    Texture2D<float> depth = ResourceDescriptorHeap[depthIndex];
    output[thread.xy] = depth.Load(int3(thread.xy, 0));
}
