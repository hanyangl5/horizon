#include "include/common/luminance.h"
#include "include/common/hlsl_common.h"

// Set 0: Per-frame resources
Texture2D<float4> prev_color_tex;
Texture2D<float4> curr_color_tex;
Texture2D<float2> mv_tex;
[[vk::image_format("rgba8")]] RWTexture2D<float4> out_color_tex;
SamplerState history_sampler;

struct TAAConstants
{
    float history_valid;
    float static_curr_weight;
    float velocity_scale;
    float velocity_disocclusion_threshold;
};
ConstantBuffer<TAAConstants> TAAConstants_cb;

// RGB <-> YCoCg conversion
// Reference: "High Quality Temporal Supersampling" (Karis, SIGGRAPH 2014)
float3 RGBToYCoCg(float3 rgb)
{
    return float3(
         0.25 * rgb.r + 0.5 * rgb.g + 0.25 * rgb.b,
         0.5  * rgb.r                - 0.5  * rgb.b,
        -0.25 * rgb.r + 0.5 * rgb.g - 0.25 * rgb.b
    );
}

float3 YCoCgToRGB(float3 ycocg)
{
    float y  = ycocg.x;
    float co = ycocg.y;
    float cg = ycocg.z;
    return float3(
        y + co - cg,
        y      + cg,
        y - co - cg
    );
}

// Clip history color towards the current neighborhood AABB center.
// Returns the clipped point on the AABB surface closest to the history sample
// along the line from aabb_center to history.
// Reference: "Temporal Reprojection Anti-Aliasing" (Karis, SIGGRAPH 2014)
float3 ClipAABB(float3 aabb_min, float3 aabb_max, float3 history, float3 aabb_center)
{
    float3 dir = history - aabb_center;
    float3 inv_dir = 1.0 / max(abs(dir), float3(1e-6, 1e-6, 1e-6));

    float3 half_extent = (aabb_max - aabb_min) * 0.5;
    float3 t_max = half_extent * inv_dir;

    float t = min(t_max.x, min(t_max.y, t_max.z));
    t = saturate(t);

    return aabb_center + dir * t;
}

[numthreads(8, 8, 1)]
void main(uint3 threadID : SV_DispatchThreadID)
{
    uint width, height;
    curr_color_tex.GetDimensions(width, height);
    if (threadID.x >= width || threadID.y >= height)
        return;

    int2 max_coord = int2(width, height) - int2(1, 1);
    int2 curr_xy = int2(threadID.xy);
    int3 currCoord = int3(curr_xy, 0);
    float3 curr_color = curr_color_tex.Load(currCoord).xyz;
    float2 motion_vector = mv_tex.Load(currCoord).xy;

    float2 resolution = float2(width, height);
    float2 curr_uv = (float2(curr_xy) + 0.5) / resolution;
    float2 prev_uv = clamp(curr_uv - motion_vector, float2(0.0, 0.0), float2(1.0, 1.0));
    float3 prev_color = prev_color_tex.SampleLevel(history_sampler, prev_uv, 0.0).xyz;

    // Sample 3x3 neighborhood for variance clipping
    float3 samples[9];
    int sample_idx = 0;
    [unroll]
    for (int dy = -1; dy <= 1; dy++)
    {
        [unroll]
        for (int dx = -1; dx <= 1; dx++)
        {
            int2 p = clamp(curr_xy + int2(dx, dy), int2(0, 0), max_coord);
            samples[sample_idx] = RGBToYCoCg(curr_color_tex.Load(int3(p, 0)).xyz);
            sample_idx++;
        }
    }

    // Compute mean and variance in YCoCg space
    float3 moment1 = float3(0.0, 0.0, 0.0);
    float3 moment2 = float3(0.0, 0.0, 0.0);
    [unroll]
    for (int i = 0; i < 9; i++)
    {
        moment1 += samples[i];
        moment2 += samples[i] * samples[i];
    }
    moment1 /= 9.0;
    moment2 /= 9.0;

    float3 stddev = sqrt(max(moment2 - moment1 * moment1, float3(0.0, 0.0, 0.0)));

    // Variance clip: construct AABB from mean +/- gamma * stddev
    static const float VARIANCE_CLIP_GAMMA = 1.0;
    float3 aabb_min = moment1 - VARIANCE_CLIP_GAMMA * stddev;
    float3 aabb_max = moment1 + VARIANCE_CLIP_GAMMA * stddev;

    // Clip history in YCoCg space
    float3 prev_ycocg = RGBToYCoCg(prev_color);
    prev_ycocg = ClipAABB(aabb_min, aabb_max, prev_ycocg, moment1);
    prev_color = YCoCgToRGB(prev_ycocg);

    if (TAAConstants_cb.history_valid < 0.5)
    {
        out_color_tex[threadID.xy] = float4(curr_color, 1.0);
        return;
    }

    const float motion_len = length(motion_vector);
    const float motion_factor = saturate(motion_len * TAAConstants_cb.velocity_scale);

    float curr_weight = lerp(saturate(TAAConstants_cb.static_curr_weight), 1.0, motion_factor);
    if (motion_len >= TAAConstants_cb.velocity_disocclusion_threshold)
    {
        curr_weight = 1.0;
    }

    const float prev_luma = Luminance(prev_color);
    const float curr_luma = Luminance(curr_color);
    const float luma_delta = abs(curr_luma - prev_luma) / max(max(curr_luma, prev_luma), 1e-4);
    curr_weight = max(curr_weight, saturate(luma_delta));

    out_color_tex[threadID.xy] = float4(lerp(prev_color, curr_color, curr_weight), 1.0);
}
