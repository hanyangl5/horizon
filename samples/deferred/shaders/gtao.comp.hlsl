#include "include/translation/translation.h"

// Set 0: Per-frame resources
Texture2D<float4> depth_tex;
Texture2D<float4> normal_tex;
SamplerState default_sampler;
[[vk::image_format("r8")]] RWTexture2D<float4> ao_factor_tex;

struct GTAOConstant
{
    float4x4 camera_projection;
    float4x4 camera_inv_projection;
    float4x4 camera_view;
    uint2 resolution;
    float radius;
    float falloff;
    float thickness;
    float bias;
    uint direction_count;
    uint step_count;
    float max_pixel_radius;
    float intensity;
};
ConstantBuffer<GTAOConstant> GTAOConstant_cb;

static const float PI_F = 3.14159265f;
static const float TWO_PI_F = 6.28318530f;

// Texture-free interleaved gradient noise.
float InterleavedGradientNoise(float2 pixel)
{
    return frac(52.9829189f * frac(dot(pixel, float2(0.06711056f, 0.00583715f))));
}

float3 GetViewPos(float2 uv, float depth)
{
    // ReconstructWorldPos also works for view space when camera_inv_projection is provided.
    return ReconstructWorldPos(GTAOConstant_cb.camera_inv_projection, depth, uv);
}

float3 GetViewNormal(float2 uv)
{
    float3 normal_ws = normal_tex.SampleLevel(default_sampler, uv, 0.0f).xyz * 2.0f - 1.0f;
    float3 normal_vs = mul(GTAOConstant_cb.camera_view, float4(normal_ws, 0.0f)).xyz;
    return normalize(normal_vs);
}

float ComputeDirectionalHorizonAO(float2 uv, float3 view_pos, float3 view_normal, float2 dir_uv, float jitter)
{
    uint step_count = max(GTAOConstant_cb.step_count, 1u);
    float side_occlusion_sum = 0.0f;

    [unroll]
    for (uint side = 0; side < 2; ++side)
    {
        float sign = side == 0 ? -1.0f : 1.0f;
        float horizon = 0.0f;

        [loop]
        for (uint step = 1; step <= step_count; ++step)
        {
            float t = (float(step) + jitter) / float(step_count);
            float2 sample_uv = uv + dir_uv * sign * t;
            if (sample_uv.x <= 0.0f || sample_uv.x >= 1.0f || sample_uv.y <= 0.0f || sample_uv.y >= 1.0f)
            {
                break;
            }

            float sample_depth = depth_tex.SampleLevel(default_sampler, sample_uv, 0.0f).r;
            if (sample_depth >= 1.0f)
            {
                continue;
            }

            float3 sample_view_pos = GetViewPos(sample_uv, sample_depth);
            float3 to_sample = sample_view_pos - view_pos;
            float dist2 = dot(to_sample, to_sample);
            if (dist2 <= 1.0e-8f)
            {
                continue;
            }

            float dist = sqrt(dist2);
            if (dist > GTAOConstant_cb.radius + GTAOConstant_cb.falloff)
            {
                continue;
            }

            float3 to_sample_dir = to_sample / dist;
            float nds = dot(view_normal, to_sample_dir);
            float angular = saturate((nds - GTAOConstant_cb.bias) / max(1.0f - GTAOConstant_cb.bias, 1.0e-4f));

            float edge0 = GTAOConstant_cb.radius;
            float edge1 = GTAOConstant_cb.radius + GTAOConstant_cb.falloff + GTAOConstant_cb.thickness;
            float range = 1.0f - smoothstep(edge0, edge1, dist);

            horizon = max(horizon, angular * range);
        }

        side_occlusion_sum += horizon;
    }

    return side_occlusion_sum * 0.5f;
}

[numthreads(8, 8, 1)]
void main(uint3 threadID : SV_DispatchThreadID)
{
    if (threadID.x >= GTAOConstant_cb.resolution.x || threadID.y >= GTAOConstant_cb.resolution.y)
    {
        return;
    }

    uint2 pixel = threadID.xy;
    float2 resolution = float2(GTAOConstant_cb.resolution);
    float2 uv = (float2(pixel) + 0.5f) / resolution;

    float depth = depth_tex.Load(int3(pixel, 0)).r;
    if (depth >= 1.0f)
    {
        ao_factor_tex[pixel] = float4(1.0f, 0.0f, 0.0f, 0.0f);
        return;
    }

    float3 view_pos = GetViewPos(uv, depth);
    float3 view_normal = GetViewNormal(uv);

    float proj_scale = 0.5f * GTAOConstant_cb.camera_projection._22 * resolution.y;
    float view_depth = max(-view_pos.z, 1.0e-3f);
    float pixel_radius = clamp((GTAOConstant_cb.radius * proj_scale) / view_depth, 2.0f, GTAOConstant_cb.max_pixel_radius);
    float2 dir_step_uv = pixel_radius / resolution;

    float noise0 = InterleavedGradientNoise(float2(pixel));
    float noise1 = InterleavedGradientNoise(float2(pixel.yx) + 19.19f);
    float rotation = noise0 * TWO_PI_F;
    float jitter = noise1;

    uint direction_count = max(GTAOConstant_cb.direction_count, 1u);
    float occlusion = 0.0f;
    [loop]
    for (uint i = 0; i < direction_count; ++i)
    {
        float angle = (float(i) + 0.5f) * (PI_F / float(direction_count)) + rotation;
        float2 dir = float2(cos(angle), sin(angle));
        occlusion += ComputeDirectionalHorizonAO(uv, view_pos, view_normal, dir * dir_step_uv, jitter);
    }

    occlusion /= float(direction_count);
    float ao = saturate(1.0f - occlusion * GTAOConstant_cb.intensity);
    ao_factor_tex[pixel] = float4(ao, 0.0f, 0.0f, 0.0f);
}
