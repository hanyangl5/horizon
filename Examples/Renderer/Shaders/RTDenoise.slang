#include "RTDenoiseBindings.slang"

// Stores view depth so the next frame can reject disoccluded history with a relative test; 0 marks the sky.
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
    Texture2D<float>            depthTexture = ResourceDescriptorHeap[depthIndex];
    StructuredBuffer<FrameData> frames = ResourceDescriptorHeap[frameIndex];
    const float                 depth = depthTexture.Load(int3(thread.xy, 0));
    const float2                uv = (float2(thread.xy) + 0.5) / float2(width, height);
    output[thread.xy] =
        depth >= 1.0 ? 0.0 : viewDepth(frames[0].viewProjection, reconstructWorldPosition(uv, depth, frames[0].inverseViewProjection));
}
