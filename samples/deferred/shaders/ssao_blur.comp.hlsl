// Set 0: Per-frame resources
Texture2D<float4> ssao_blur_in;
[[vk::image_format("r8")]] RWTexture2D<float4> ssao_blur_out;

[numthreads(8, 8, 1)]
void main(uint3 threadID : SV_DispatchThreadID)
{
    uint width, height;
    ssao_blur_out.GetDimensions(width, height);
    if (threadID.x >= width || threadID.y >= height)
        return;

    int2 max_coord = int2(width, height) - int2(1, 1);
    int2 center = int2(threadID.xy);
    float4 result = float4(0.0, 0.0, 0.0, 0.0);
    for (int x = -2; x < 2; ++x)
    {
        for (int y = -2; y < 2; ++y)
        {
            int2 coord = clamp(center + int2(x, y), int2(0, 0), max_coord);
            result += ssao_blur_in.Load(int3(coord, 0)).r;
        }
    }
    result = result / (4.0 * 4.0);
    ssao_blur_out[threadID.xy] = result;
}
