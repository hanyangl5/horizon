

#include "include/common/bindless.h"
#include "include/shading/material_params_defination.hlsl"
#include "include/common/hlsl_common.h"

struct CameraParamsUb {
    float4x4 vp;
    float4x4 prev_vp;
    float4 camera_position;
};
#ifdef SPIRV
ConstantBuffer<CameraParamsUb> CameraParamsUb_cb;
#else
ConstantBuffer<CameraParamsUb> CameraParamsUb_cb : register(b0);
#endif

struct DrawConstants
{
    uint mesh_id_offset;
};

#ifdef SPIRV
[[vk::push_constant]] ConstantBuffer<DrawConstants> mesh_draw_offset;
#else
ConstantBuffer<DrawConstants> mesh_draw_offset : register(b1);
#endif
// App sets mesh_id = DrawIndex + mesh_id_offset when using multi-draw; otherwise mesh_id_offset alone.

struct InstanceParameter {
    float4x4 model_matrix;
    uint material_id;
};
#ifdef SPIRV
StructuredBuffer<InstanceParameter> instance_parameter;
#else
StructuredBuffer<InstanceParameter> instance_parameter : register(t0);
#endif

struct VSInput {
    float3 position : POSITION;
    float3 normal : NORMAL;
    float2 uv0 : TEXCOORD0;
    float2 uv1 : TEXCOORD1;
    float3 tangent : TANGENT;
};

struct VSOutput {
    float4 position : SV_Position;
    float3 world_pos : POSITION;
    float3 normal : NORMAL;
    float2 uv : TEXCOORD0;
    float3 tangent : TANGENT;
    uint instance_id : TEXCOORD1;
    uint material_id : TEXCOORD2;
    float4 curr_pos : TEXCOORD3;
    float4 prev_pos : TEXCOORD4;
};


VSOutput vs_main(VSInput vsin, uint InstanceID : SV_InstanceID, uint vertex_id : SV_VertexID
#ifdef SPIRV
    ,[[vk::builtin("DrawIndex")]] uint drawIndex : A
#else
#endif
)
{
    VSOutput vsout;
    //[[vk::builtin("DrawIndex")]] uint draw_index;
    #ifdef SPIRV
    uint mesh_id = mesh_draw_offset.mesh_id_offset + drawIndex;
    #else
    uint mesh_id = mesh_draw_offset.mesh_id_offset;
    #endif
    float4x4 model = instance_parameter[mesh_id].model_matrix;

    vsout.position = mul(CameraParamsUb_cb.vp, mul(model, float4(vsin.position, 1.0)));
    vsout.world_pos = mul(model, float4(vsin.position, 1.0)).xyz;
    vsout.normal = normalize(mul(model, float4(vsin.normal, 0.0)).xyz);
    vsout.uv = vsin.uv0;
    vsout.tangent = normalize(mul(model, float4(vsin.tangent, 0.0)).xyz);
    vsout.instance_id = InstanceID;
    vsout.material_id = instance_parameter[mesh_id].material_id;
    vsout.prev_pos = mul(CameraParamsUb_cb.prev_vp, mul(model, float4(vsin.position, 1.0)));
    vsout.curr_pos = mul(CameraParamsUb_cb.vp, mul(model, float4(vsin.position, 1.0)));
    return vsout;
}

struct MaterialDescription {
    uint base_color_texture_index;
    uint normal_texture_index;
    uint metallic_roughness_texture_index;
    uint emissive_textue_index;
    uint alpha_mask_texture_index;
    uint subsurface_scattering_texture_index;
    uint param_bitmask;
    uint blend_state;
    float3 base_color;
    float pad1;
    float3 emissive;
    float pad2;
    float2 metallic_roughness;
    float2 pad3;
};

// Bindless resources in set 1
#ifdef SPIRV
[[vk::binding(0, 1)]] Texture2D<float4> material_textures[];
#else
Texture2D<float4> material_textures[] : register(t1, space1);
#endif

#ifdef SPIRV
// Per-frame resources in set 0
StructuredBuffer<MaterialDescription> material_descriptions;
#else
StructuredBuffer<MaterialDescription> material_descriptions : register(t2);
#endif

#ifdef SPIRV
SamplerState default_sampler;
#else
SamplerState default_sampler : register(s0);
#endif

struct TAAOffsets { float4 taa_prev_curr_offset; };
#ifdef SPIRV
ConstantBuffer<TAAOffsets> TAAOffsets_cb;
#else
ConstantBuffer<TAAOffsets> TAAOffsets_cb : register(b2);
#endif

struct PSOutput {
    float4 gbuffer0 : SV_Target0;  // Normal (UNORM [0,1])
    float4 gbuffer1 : SV_Target1; // Albedo
    float3 gbuffer2 : SV_Target2; // Emissive (R11G11B10 HDR)
    float4 gbuffer3 : SV_Target3; // Metallic/Roughness/Alpha
    float2 gbuffer4 : SV_Target4; // Motion vector
};

PSOutput ps_main(VSOutput vsout, uint tri_id : SV_PrimitiveID)
{
    PSOutput psout;
    uint material_id = vsout.material_id;
    MaterialDescription material = material_descriptions[material_id];
    uint param_bitmask = material.param_bitmask;

    uint has_metallic_roughness = param_bitmask & HAS_METALLIC_ROUGHNESS;
    uint has_normal = param_bitmask & HAS_NORMAL;
    uint has_base_color = param_bitmask & HAS_BASE_COLOR;
    uint has_emissive = param_bitmask & HAS_EMISSIVE;

    float3 albedo = has_base_color != 0
        ? pow(material_textures[material.base_color_texture_index].Sample(default_sampler, vsout.uv).xyz, float3(2.2, 2.2, 2.2))
        : material.base_color;
    float alpha = 1.0;
    if (material.blend_state == BLEND_STATE_MASKED) {
        alpha = material_textures[material.base_color_texture_index].Sample(default_sampler, vsout.uv).w;
        if (alpha < 0.5) discard;
    }

    float3 normal_map = has_normal != 0
        ? material_textures[material.normal_texture_index].Sample(default_sampler, vsout.uv).xyz
        : float3(0, 0, 0);
    float2 mr = has_metallic_roughness != 0
        ? material_textures[material.metallic_roughness_texture_index].Sample(default_sampler, vsout.uv).yz
        : material.metallic_roughness;
    float3 emissive = has_emissive != 0
        ? pow(material_textures[material.emissive_textue_index].Sample(default_sampler, vsout.uv).xyz, float3(2.2, 2.2, 2.2))
        : material.emissive;

    normal_map = normalize(2.0 * normal_map - 1.0);

    float3 gbuffer_normal;
    if (has_normal != 0) {
        float3 normal = normalize(vsout.normal);
        float3 tangent = normalize(vsout.tangent);
        float3 bitangent = normalize(cross(tangent, normal));
        float3x3 tbn = float3x3(tangent, bitangent, normal);
        gbuffer_normal = normalize(mul(normal_map, tbn));
    } else {
        gbuffer_normal = normalize(vsout.normal);
    }

    // Pack normal from [-1,1] to [0,1] for UNORM format
    psout.gbuffer0 = float4(gbuffer_normal * 0.5 + 0.5, 0.0);
    psout.gbuffer1 = float4(albedo, 0.0);
    // Emissive in R11G11B10 format (HDR) - GPU will auto-pack float3 to R11G11B10
    psout.gbuffer2 = emissive;
    psout.gbuffer3 = float4(mr.y, mr.x, alpha, 0.0);

    float4 prev_pos = vsout.prev_pos;
    prev_pos.xyz /= prev_pos.w;
    prev_pos.xy -= TAAOffsets_cb.taa_prev_curr_offset.xy;
    float4 curr_pos = vsout.curr_pos;
    curr_pos.xyz /= curr_pos.w;
    curr_pos.xy -= TAAOffsets_cb.taa_prev_curr_offset.zw;
    psout.gbuffer4 = (curr_pos.xy - prev_pos.xy) * 0.5;

    return psout;
}
