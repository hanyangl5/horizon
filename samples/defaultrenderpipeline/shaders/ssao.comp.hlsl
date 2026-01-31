#include "include/translation/translation.h"
#include "include/common/common_math.h"
#include "include/common/hlsl_common.h"
#define USE_SSAO
#define SSAO_SAMPLE_COUNT 32
#define SSAO_SAMPLE_RADIUS 1.0
#define SSAO_BIAS 0.025
#include "include/common/hlsl_common.h"

// Set 0: Per-frame resources
[[vk::binding(0, 0)]] Texture2D<float4> depth_tex;
[[vk::binding(1, 0)]] Texture2D<float4> normal_tex;
[[vk::binding(2, 0)]] Texture2D<float2> ssao_noise_tex;
[[vk::binding(3, 0)]] SamplerState default_sampler;
[[vk::image_format("rgba8"),vk::binding(4, 0)]] RWTexture2D<float4> ao_factor_tex;

struct SSAOConstant {
    float4x4 camera_projection;
    float4x4 camera_inv_projection;
    float4x4 camera_view;
    uint2 resolution;
    float2 noise_scale;
    float4 kernels[SSAO_SAMPLE_COUNT];
};
[[vk::binding(5, 0)]] ConstantBuffer<SSAOConstant> SSAOConstant_cb;

[numthreads(8, 8, 1)]
void main(uint3 threadID : SV_DispatchThreadID)
{
    uint2 _resolution = SSAOConstant_cb.resolution - uint2(1, 1);
    if (threadID.x > _resolution.x || threadID.y > _resolution.y)
        return;

    float2 uv = float2(threadID.xy) / float2(_resolution);
    float depth = depth_tex.SampleLevel(default_sampler, uv, 00.0f).r;
    float3 view_pos = ReconstructWorldPos(SSAOConstant_cb.camera_inv_projection, depth, uv);

    float3 normal = normal_tex.SampleLevel(default_sampler, uv, 0.0f).xyz;
    float3 view_normal = mul(SSAOConstant_cb.camera_view, float4(normal, 0.0)).xyz;
    view_normal = normalize(view_normal);

    float ssao_factor = 0.0;
    float2 noise_uv = uv * SSAOConstant_cb.noise_scale.xy;
    float3 rvec = float3(ssao_noise_tex.SampleLevel(default_sampler, noise_uv, 0.0f).xy, 0.0f);
    float3 tangent = normalize(rvec - view_normal * dot(rvec, view_normal));
    float3 bitangent = cross(tangent, view_normal);
    float3x3 tbn = make_f3x3_cols(tangent, bitangent, view_normal);

    for (uint i = 0; i < SSAO_SAMPLE_COUNT; i++)
    {
        float3 sample_pos = view_pos + mul(tbn, SSAOConstant_cb.kernels[i].xyz) * SSAO_SAMPLE_RADIUS;
        float4 offset = mul(SSAOConstant_cb.camera_projection, float4(sample_pos, 1.0));
        offset.xyz /= offset.w;
        offset.xy = offset.xy * 0.5 + 0.5;
        offset.y = 1.0 - offset.y;
        if (AnyGreaterThan(offset.xy, float2(1.0, 1.0)) || AnyLessThan(offset.xy, float2(0.0, 0.0)))
            continue;
        float sample_z = depth_tex.SampleLevel(default_sampler, offset.xy, 0.0f).r;
        float linearZ = ReconstructWorldPos(SSAOConstant_cb.camera_inv_projection, sample_z, offset.xy).z;
        float range_falloff = SmoothStep(0.0, 1.0, SSAO_SAMPLE_RADIUS / abs(linearZ - view_pos.z));
        if (abs(linearZ - view_pos.z) > SSAO_SAMPLE_RADIUS)
            continue;
        if (linearZ >= sample_pos.z + SSAO_BIAS)
            ssao_factor += 1.0;
    }

    ssao_factor /= SSAO_SAMPLE_COUNT;
    ssao_factor = 1.0 - ssao_factor;
    ao_factor_tex[threadID.xy] = float4(ssao_factor, 0.0, 0.0, 0.0);
}
