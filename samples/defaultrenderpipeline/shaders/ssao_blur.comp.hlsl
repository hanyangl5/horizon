// Set 0: Per-frame resources
[[vk::image_format("rgba8"),vk::binding(0, 0)]] RWTexture2D<float4> ssao_blur_in;
[[vk::image_format("rgba8"),vk::binding(1, 0)]] RWTexture2D<float4> ssao_blur_out;

[numthreads(8, 8, 1)]
void main(uint3 threadID : SV_DispatchThreadID)
{
    float4 result = float4(0.0, 0.0, 0.0, 0.0);
    for (int x = -2; x < 2; ++x)
    {
        for (int y = -2; y < 2; ++y)
        {
            uint2 coord = threadID.xy + uint2(x, y);
            result += ssao_blur_in[coord].r;
        }
    }
    result = result / (4.0 * 4.0);
    ssao_blur_out[threadID.xy] = result;
}
