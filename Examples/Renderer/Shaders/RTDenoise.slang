#include "RTDenoiseBindings.slang"

// Stores view depth so the next frame can reject disoccluded history with a relative test; 0 marks the sky.
[shader("compute")]
[numthreads(16, 16, 1)]
void CSCopyDepth(uint3 Thread : SV_DispatchThreadID)
{
    RWTexture2D<float> Output = ResourceDescriptorHeap[OutputIndex];
    uint               Width;
    uint               Height;
    Output.GetDimensions(Width, Height);
    if (Thread.x >= Width || Thread.y >= Height)
    {
        return;
    }
    Texture2D<float>            DepthTexture = ResourceDescriptorHeap[DepthIndex];
    StructuredBuffer<FrameData> Frames = ResourceDescriptorHeap[FrameIndex];
    uint2                       SourceSize;
    DepthTexture.GetDimensions(SourceSize.x, SourceSize.y);
    const int2   Source = RTSourcePixel(int2(Thread.xy), int2(Width, Height), int2(SourceSize));
    const float  Depth = DepthTexture.Load(int3(Source, 0));
    const float2 UV = (float2(Source) + 0.5) / float2(SourceSize);
    Output[Thread.xy] =
        Depth >= 1.0 ? 0.0 : ViewDepth(Frames[0].ViewProjection, ReconstructWorldPosition(UV, Depth, Frames[0].InverseViewProjection));
}
