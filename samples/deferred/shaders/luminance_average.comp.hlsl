#include "include/common/luminance.h"
#include "include/postprocess/histogram.h"

// Set 0: Per-frame resources
//[[vk::binding(0, 0)]] RWTexture2D<float4> color_image;
StructuredBuffer<uint> histogram;
RWStructuredBuffer<float> adaptedLuminance;

struct LuminanceHistogramConstants {
    uint2 resolution;
    uint pixelCount;
    float maxLuminance;
    float timeCoeff;
};
ConstantBuffer<LuminanceHistogramConstants> LuminanceHistogramConstants_cb;

groupshared float histogramShared[GROUP_SIZE];

[numthreads(16, 16, 1)]
void main(uint3 threadID : SV_DispatchThreadID, uint localIndex : SV_GroupIndex)
{
    LuminanceHistogramConstants _c = LuminanceHistogramConstants_cb;
    if (threadID.x >= _c.resolution.x || threadID.y >= _c.resolution.y)
        return;

    //float4 color = color_image[threadID.xy];
    float countForThisBin = (float)histogram[localIndex];
    histogramShared[localIndex] = countForThisBin * (float)localIndex;

    GroupMemoryBarrierWithGroupSync();

    for (uint cutoff = (GROUP_SIZE >> 1); cutoff > 0; cutoff >>= 1) {
        if (localIndex < cutoff)
            histogramShared[localIndex] += histogramShared[localIndex + cutoff];
        GroupMemoryBarrierWithGroupSync();
    }

    if (localIndex == 0) {
        float averageLuminance = (histogramShared[0] / max((float)_c.pixelCount - countForThisBin, 1.0)) - 1.0;
        averageLuminance = averageLuminance * _c.maxLuminance / 255.0;
        float adaptedLum = adaptedLuminance[0] + (averageLuminance - adaptedLuminance[0]) * _c.timeCoeff;
        adaptedLuminance[0] = adaptedLum;
    }
}
