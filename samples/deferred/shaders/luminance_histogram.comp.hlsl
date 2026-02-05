#include "include/common/luminance.h"
#include "include/postprocess/histogram.h"

// Set 0: Per-frame resources
[[vk::image_format("r11g11b10f")]] RWTexture2D<float4> color_image;
RWStructuredBuffer<uint> histogram;
RWStructuredBuffer<float> adaptedLuminance;

struct LuminanceHistogramConstants {
    uint2 resolution;
    uint pixelCount;
    float maxLuminance;
    float timeCoeff;
};
ConstantBuffer<LuminanceHistogramConstants> LuminanceHistogramConstants_cb;

groupshared uint histogramShared[GROUP_SIZE];

[numthreads(16, 16, 1)]
void main(uint3 threadID : SV_DispatchThreadID, uint localIndex : SV_GroupIndex)
{
    histogramShared[localIndex] = 0;
    GroupMemoryBarrierWithGroupSync();

    if (threadID.x < LuminanceHistogramConstants_cb.resolution.x && threadID.y < LuminanceHistogramConstants_cb.resolution.y) {
        float3 color = color_image[threadID.xy].xyz;
        uint binIndex = HDRToHistogramBin(color, LuminanceHistogramConstants_cb.maxLuminance);
        InterlockedAdd(histogramShared[binIndex], 1);
    }

    GroupMemoryBarrierWithGroupSync();
    InterlockedAdd(histogram[localIndex], histogramShared[localIndex]);
    if (localIndex == 0)
        adaptedLuminance[0] = 0.0;
}
