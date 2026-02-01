#include "include/shading/material_params_defination.hlsl"
#include "include/shading/light_defination.h"
#include "include/shading/lighting_hlsl.h"
#include "include/shading/ibl_hlsl.h"
#include "include/translation/translation.h"
#include "include/common/common_math.h"

// Set 0: Per-frame resources

Texture2D<float4> gbuffer0_tex; // Normal (UNORM)
Texture2D<float4> gbuffer1_tex; // Albedo
Texture2D<float3> gbuffer2_tex; // Emissive (R11G11B10 HDR)
Texture2D<float4> gbuffer3_tex; // Metallic/Roughness/Alpha
Texture2D<float4> depth_tex;
//SamplerState default_sampler;

struct DeferredShadingConstants {
    float4x4 inverse_vp;
    float4 camera_pos_exposure;
    uint2 resolution;
    float2 ibl_intensity;
};
ConstantBuffer<DeferredShadingConstants> DeferredShadingConstants_cb;

struct LightCountUb { uint light_count; };
ConstantBuffer<LightCountUb> LightCountUb_cb;

struct LightDataUb { LightParams light_data[MAX_DYNAMIC_LIGHT_COUNT]; };
ConstantBuffer<LightDataUb> LightDataUb_cb;

[[vk::image_format("rgba16f")]] RWTexture2D<float4> out_color;
[[vk::image_format("rgba8")]] RWTexture2D<float4> ao_tex;

ConstantBuffer<DiffuseIrradianceSH3> DiffuseIrradianceSH3_cb;

TextureCube<float4> specular_map;
Texture2D<float4> specular_brdf_lut;
SamplerState ibl_sampler;

[numthreads(8, 8, 1)]
void main(uint3 threadID : SV_DispatchThreadID)
{
    uint2 _resolution = DeferredShadingConstants_cb.resolution - 1;
    if (threadID.x > _resolution.x || threadID.y > _resolution.y)
        return;

    int3 loadCoord = int3(threadID.xy, 0);
    float depth = depth_tex.Load(loadCoord).r;
    if (depth == 1.0f)
    {
        out_color[threadID.xy] = float4(0.0, 0.0, 0.0, 1.0);
        return;
    }
        
    float4 gbuffer0 = gbuffer0_tex.Load(loadCoord);
    float4 gbuffer1 = gbuffer1_tex.Load(loadCoord);
    float3 gbuffer2 = gbuffer2_tex.Load(loadCoord);
    float4 gbuffer3 = gbuffer3_tex.Load(loadCoord);

    MaterialProperties mat;
    mat.albedo = gbuffer1.xyz;
    mat.metallic = gbuffer3.x;
    float roughness = max(0.045f, gbuffer3.y);
    float alpha = gbuffer3.z;
    mat.roughness = roughness;
    mat.roughness2 = Pow2(roughness);
    mat.f0 = lerp(float3(0.04, 0.04, 0.04), mat.albedo, mat.metallic);
    // Unpack emissive from R11G11B10 (GPU auto-unpacks)
    mat.emissive = gbuffer2;
    float2 uv = float2(threadID.xy) / float2(_resolution);
    float3 world_pos = ReconstructWorldPos(DeferredShadingConstants_cb.inverse_vp, depth, uv);
    // Unpack normal from [0,1] to [-1,1]
    float3 n = normalize(gbuffer0.xyz * 2.0 - 1.0);
    float3 v = -normalize(world_pos - DeferredShadingConstants_cb.camera_pos_exposure.xyz);
    float NoV = saturate(dot(n, v));
    float4 radiance = float4(0.0, 0.0, 0.0, 0.0);
    radiance.xyz += mat.emissive;
    for (uint i = 0; i < LightCountUb_cb.light_count; i++)
    {
        radiance += Radiance(mat, LightDataUb_cb.light_data[i], n, v, world_pos);
    }

    float3 reflect_dir = normalize(2.0 * dot(n, v) * n - v);
    float3 specular = specular_map.SampleLevel(ibl_sampler, reflect_dir, roughness * 8.0).xyz;
    float2 ibl_uv = float2(roughness, NoV);
    float2 env = specular_brdf_lut.SampleLevel(ibl_sampler, ibl_uv,0).xy;
    float3 ambient = IBL(DiffuseIrradianceSH3_cb, specular, env, n, NoV, mat) *
        ao_tex.Load(threadID.xy).r * DeferredShadingConstants_cb.ibl_intensity.x;
    radiance.xyz += ambient;

    out_color[threadID.xy] = radiance;
}
