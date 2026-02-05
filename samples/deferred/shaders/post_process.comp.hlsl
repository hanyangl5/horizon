#include "include/postprocess/postprocess.h"
#include "include/common/luminance.h"

// Set 0: Per-frame resources
[[vk::image_format("r11g11b10f")]] RWTexture2D<float4> color_image;
[[vk::image_format("rgba8")]] RWTexture2D<float4> out_color_image;
RWStructuredBuffer<float> adaptedLuminance;

[numthreads(8, 8, 1)]
void main(uint3 threadID : SV_DispatchThreadID)
{
    float4 color = color_image[threadID.xy];
    float exposure_scale = 1.0 / (9.6 * adaptedLuminance[0] + 0.0001);
    color.xyz *= exposure_scale;
    color.xyz = TonemapACES(color.xyz);
    color.xyz = GammaCorrection(color.xyz);
    out_color_image[threadID.xy] = color;
}
