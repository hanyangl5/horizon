#include "include/common/descriptor.hlsl"
#include "include/translation/translation.hlsl"
#include "include/common/common_math.hlsl"
#include "include/common/noise.hlsl"

#define SSAO_SAMPLE_COUNT 32
#define SSAO_SAMPLE_RADIUS 1.0
#define SSAO_BIAS 0.025

RES(Texture2D<float4>, depth_tex, UPDATE_FREQ_PER_FRAME);
RES(Texture2D<float4>, normal_tex, UPDATE_FREQ_PER_FRAME);
RES(Texture2D<float2>, ssao_noise_tex, UPDATE_FREQ_PER_FRAME);
RES(SamplerState, default_sampler, UPDATE_FREQ_PER_FRAME);
RES(RWTexture2D<float4>, ao_factor_tex, UPDATE_FREQ_PER_FRAME);

CBUFFER(SceneConstants, UPDATE_FREQ_PER_FRAME)
{
    float4x4 camera_view;
    float4x4 camera_projection;
    float4x4 camera_view_projection;
    float4x4 camera_inverse_view_projection;
    uint2 resolution;
    uint2 pad_0;
    float3 camera_pos;
    uint pad_1;
    float ibl_intensity;
};

CBUFFER(SSAOConstant, UPDATE_FREQ_PER_FRAME)
{
    float4x4 camera_inv_projection; // we only care about view space
    float2 noise_scale;
    float2 pad0;
    float4 kernels[SSAO_SAMPLE_COUNT]; // tangent space kernels
};


[numthreads(8, 8, 1)]
void CS_MAIN(uint3 thread_id : SV_DispatchThreadID) 
{
    uint2 _resolution = Get(resolution.xy) - uint2(1.0, 1.0);

    if (thread_id.x>_resolution.x || thread_id.y>_resolution.y) {
        return;
    }

    float2 uv = float2(thread_id.xy) / float2(_resolution);

    float depth = depth_tex.Sample(default_sampler, uv).r;
    float3 view_pos = ReconstructWorldPos(camera_inv_projection, depth, uv);

    float3 normal = normal_tex.Sample(default_sampler, uv).xyz;
    
    float3 view_normal = (Get(camera_view) * float4(normal, 0.0)).xyz; // view space normal
    view_normal = normalize(view_normal);

    float ssao_factor = 0.0;

    float2 noise_uv = uv * noise_scale.xy;

    float3 rvec = float3(ssao_noise_tex.Sample(default_sampler, noise_uv).xy, 0.0);
    float3 tangent = normalize(rvec - view_normal * dot(rvec, view_normal));
	float3 bitangent = cross(tangent, view_normal);
	float3x3  tbn = transpose(float3x3(tangent, bitangent, view_normal));

    for (uint i = 0; i < SSAO_SAMPLE_COUNT; i++){
        float3 sample_pos = view_pos + mul(tbn, kernels[i].xyz) * SSAO_SAMPLE_RADIUS;

        float4 offset = camera_projection * float4(sample_pos, 1.0);
        offset.xyz /= offset.w;
        offset.xy = offset.xy * 0.5 + 0.5;
        offset.y = 1.0 - offset.y;
        if (any(GreaterThan(offset.xy, float2(1.0, 1.0))) || any(LessThan(offset.xy, float2(0.0, 0.0)))) { 
            continue; 
        }
        float sample_z = depth_tex.Sample(default_sampler, offset).r;
        // we need to compare depth in linear space
        float linearZ = ReconstructWorldPos(camera_inv_projection, sample_z, offset.xy).z;
        // occluded
        float range_falloff = SmoothStep(0.0, 1.0, SSAO_SAMPLE_RADIUS / abs(linearZ - view_pos.z)); // closer samples contribute more occlusion
        if (abs(linearZ - view_pos.z)> SSAO_SAMPLE_RADIUS) continue;
        if (linearZ >= sample_pos.z + SSAO_BIAS) {
            ssao_factor += 1.0 * 1.0f;
        }
    }

    ssao_factor /= SSAO_SAMPLE_COUNT;
    ssao_factor = 1.0 - ssao_factor;

    ao_factor_tex[thread_id.xy] = out_color;

}

