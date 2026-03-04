#include "include/postprocess/postprocess.h"

// Set 0: Per-frame resources
Texture2D<float4> color_image;
[[vk::image_format("rgba8")]] RWTexture2D<float4> out_color_image;

struct ExposureConstant
{
    float4 exposure_ev100__;
};

ConstantBuffer<ExposureConstant> ExposureConstants_cb;

[numthreads(8, 8, 1)]
void main(uint3 threadID : SV_DispatchThreadID)
{
    uint width, height;
    out_color_image.GetDimensions(width, height);
    if (threadID.x >= width || threadID.y >= height)
        return;

    float4 color = color_image.Load(int3(threadID.xy, 0));
    color.xyz *= ExposureConstants_cb.exposure_ev100__.x;
    color.xyz = TonemapACES(color.xyz);
    color.xyz = GammaCorrection(color.xyz);
    out_color_image[threadID.xy] = color;
}
