// Set 0: Per-frame resources
Texture2D<float4> gtao_blur_in;
Texture2D<float4> depth_tex;
Texture2D<float4> normal_tex;
[[vk::image_format("r8")]] RWTexture2D<float4> gtao_blur_out;

[numthreads(8, 8, 1)]
void main(uint3 threadID : SV_DispatchThreadID)
{
    uint width, height;
    gtao_blur_out.GetDimensions(width, height);
    if (threadID.x >= width || threadID.y >= height)
    {
        return;
    }

    int2 center = int2(threadID.xy);
    int2 max_coord = int2(width, height) - int2(1, 1);

    float center_depth = depth_tex.Load(int3(center, 0)).r;
    float3 center_normal = normalize(normal_tex.Load(int3(center, 0)).xyz * 2.0f - 1.0f);

    float weighted_sum = 0.0f;
    float weight_sum = 0.0f;

    [unroll]
    for (int y = -2; y <= 2; ++y)
    {
        [unroll]
        for (int x = -2; x <= 2; ++x)
        {
            int2 sample_coord = clamp(center + int2(x, y), int2(0, 0), max_coord);
            float sample_ao = gtao_blur_in.Load(int3(sample_coord, 0)).r;

            float sample_depth = depth_tex.Load(int3(sample_coord, 0)).r;
            float3 sample_normal = normalize(normal_tex.Load(int3(sample_coord, 0)).xyz * 2.0f - 1.0f);

            float spatial = exp(-0.35f * float(x * x + y * y));
            float depth_term = abs(sample_depth - center_depth);
            float depth_weight = exp(-depth_term * 250.0f);
            float normal_weight = pow(saturate(dot(center_normal, sample_normal)), 16.0f);

            float weight = spatial * depth_weight * normal_weight;
            weighted_sum += sample_ao * weight;
            weight_sum += weight;
        }
    }

    float ao = weight_sum > 1.0e-5f ? (weighted_sum / weight_sum) : gtao_blur_in.Load(int3(center, 0)).r;
    gtao_blur_out[threadID.xy] = float4(ao, 0.0f, 0.0f, 0.0f);
}
