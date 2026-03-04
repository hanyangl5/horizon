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
    uint meshlet_offset;
};
#ifdef SPIRV
[[vk::push_constant]] ConstantBuffer<DrawConstants> meshlet_draw_offset;
#else
ConstantBuffer<DrawConstants> meshlet_draw_offset : register(b1);
#endif

struct InstanceParameter {
    float4x4 model_matrix;
    uint material_id;
    uint joint_offset;
    uint joint_count;
    uint skinning_enabled;
};

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

struct MeshletDesc
{
    float4 bounding_sphere;
    float4 cone_axis_cutoff;
    uint vertex_offset;
    uint vertex_count;
    uint triangle_offset;
    uint triangle_count;
    uint vertex_buffer_index;
    uint instance_index;
    uint material_index;
    uint pad0;
};

struct VertexData
{
    float3 position;
    float3 normal;
    float2 uv0;
    float2 uv1;
    float3 tangent;
    float4 joint_indices;
    float4 joint_weights;
};

#ifdef SPIRV
StructuredBuffer<InstanceParameter> instance_parameter;
StructuredBuffer<MaterialDescription> material_descriptions;
SamplerState default_sampler;
#else
StructuredBuffer<InstanceParameter> instance_parameter : register(t0);
StructuredBuffer<MaterialDescription> material_descriptions : register(t2);
SamplerState default_sampler : register(s0);
#endif

struct TAAOffsets {
    float4 taa_prev_curr_offset;
};
#ifdef SPIRV
ConstantBuffer<TAAOffsets> TAAOffsets_cb;
StructuredBuffer<float4x4> prev_instance_model_matrices;
StructuredBuffer<float4x4> skin_joint_matrices;
StructuredBuffer<float4x4> prev_skin_joint_matrices;
StructuredBuffer<MeshletDesc> meshlet_descs;
StructuredBuffer<uint> meshlet_vertex_indices;
StructuredBuffer<uint> meshlet_triangle_indices;
[[vk::binding(0, 1)]] StructuredBuffer<VertexData> vertex_buffers[];
[[vk::binding(1, 1)]] Texture2D<float4> material_textures[];
#else
ConstantBuffer<TAAOffsets> TAAOffsets_cb : register(b2);
StructuredBuffer<float4x4> prev_instance_model_matrices : register(t5);
StructuredBuffer<float4x4> skin_joint_matrices : register(t3);
StructuredBuffer<float4x4> prev_skin_joint_matrices : register(t4);
StructuredBuffer<MeshletDesc> meshlet_descs : register(t6);
StructuredBuffer<uint> meshlet_vertex_indices : register(t7);
StructuredBuffer<uint> meshlet_triangle_indices : register(t8);
StructuredBuffer<VertexData> vertex_buffers[] : register(t0, space1);
Texture2D<float4> material_textures[] : register(t0, space2);
#endif

struct GBufferVaryings {
    float4 position : SV_Position;
    float3 normal : NORMAL;
    float2 uv : TEXCOORD0;
    float3 tangent : TANGENT;
    uint instance_id : TEXCOORD1;
    uint material_id : TEXCOORD2;
    float4 curr_pos : TEXCOORD3;
    float4 prev_pos : TEXCOORD4;
};

struct TaskPayload
{
    uint meshlet_index;
};

groupshared TaskPayload g_task_payload;

[numthreads(1, 1, 1)]
void ts_main(uint3 dispatch_thread_id : SV_DispatchThreadID)
{
    g_task_payload.meshlet_index = meshlet_draw_offset.meshlet_offset + dispatch_thread_id.x;
    DispatchMesh(1, 1, 1, g_task_payload);
}

[outputtopology("triangle")]
[numthreads(128, 1, 1)]
void ms_main(in payload TaskPayload payload, uint3 thread_id : SV_GroupThreadID,
             out vertices GBufferVaryings out_vertices[64], out indices uint3 out_triangles[124])
{
    const uint lane = thread_id.x;
    MeshletDesc meshlet = meshlet_descs[payload.meshlet_index];
    SetMeshOutputCounts(meshlet.vertex_count, meshlet.triangle_count);

    if (lane < meshlet.vertex_count)
    {
        const uint global_vertex_index = meshlet_vertex_indices[meshlet.vertex_offset + lane];
        const uint vb_index = meshlet.vertex_buffer_index;
        VertexData vin = vertex_buffers[NonUniformResourceIndex(vb_index)][global_vertex_index];

        InstanceParameter instance_param = instance_parameter[meshlet.instance_index];
        float4x4 model = instance_param.model_matrix;
        float4x4 prev_model = prev_instance_model_matrices[meshlet.instance_index];

        float4x4 skin_mat = float4x4(
            1.0, 0.0, 0.0, 0.0,
            0.0, 1.0, 0.0, 0.0,
            0.0, 0.0, 1.0, 0.0,
            0.0, 0.0, 0.0, 1.0);
        float4x4 prev_skin_mat = skin_mat;

        if (instance_param.skinning_enabled != 0 && instance_param.joint_count > 0)
        {
            uint4 joint_indices = uint4(vin.joint_indices);
            uint4 joint_max = uint4(instance_param.joint_count - 1, instance_param.joint_count - 1,
                                    instance_param.joint_count - 1, instance_param.joint_count - 1);
            joint_indices = min(joint_indices, joint_max);
            float4 joint_weights = vin.joint_weights;

            skin_mat =
                joint_weights.x * skin_joint_matrices[instance_param.joint_offset + joint_indices.x] +
                joint_weights.y * skin_joint_matrices[instance_param.joint_offset + joint_indices.y] +
                joint_weights.z * skin_joint_matrices[instance_param.joint_offset + joint_indices.z] +
                joint_weights.w * skin_joint_matrices[instance_param.joint_offset + joint_indices.w];
            prev_skin_mat =
                joint_weights.x * prev_skin_joint_matrices[instance_param.joint_offset + joint_indices.x] +
                joint_weights.y * prev_skin_joint_matrices[instance_param.joint_offset + joint_indices.y] +
                joint_weights.z * prev_skin_joint_matrices[instance_param.joint_offset + joint_indices.z] +
                joint_weights.w * prev_skin_joint_matrices[instance_param.joint_offset + joint_indices.w];
        }

        float4 local_pos = mul(skin_mat, float4(vin.position, 1.0));
        float4 prev_local_pos = mul(prev_skin_mat, float4(vin.position, 1.0));
        float3 local_normal = mul((float3x3)skin_mat, vin.normal);
        float3 local_tangent = mul((float3x3)skin_mat, vin.tangent);

        GBufferVaryings v;
        v.position = mul(CameraParamsUb_cb.vp, mul(model, local_pos));
        v.normal = normalize(mul(model, float4(local_normal, 0.0)).xyz);
        v.uv = vin.uv0;
        v.tangent = normalize(mul(model, float4(local_tangent, 0.0)).xyz);
        v.instance_id = meshlet.instance_index;
        v.material_id = meshlet.material_index;
        v.prev_pos = mul(CameraParamsUb_cb.prev_vp, mul(prev_model, prev_local_pos));
        v.curr_pos = mul(CameraParamsUb_cb.vp, mul(model, local_pos));
        out_vertices[lane] = v;
    }

    if (lane < meshlet.triangle_count)
    {
        const uint packed = meshlet_triangle_indices[meshlet.triangle_offset + lane];
        out_triangles[lane] = uint3(packed & 0xFFu, (packed >> 8u) & 0xFFu, (packed >> 16u) & 0xFFu);
    }
}

struct PSOutput {
    float4 gbuffer0 : SV_Target0;
    float4 gbuffer1 : SV_Target1;
    float3 gbuffer2 : SV_Target2;
    float4 gbuffer3 : SV_Target3;
    float2 gbuffer4 : SV_Target4;
};

PSOutput ps_main(GBufferVaryings vsout)
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

    psout.gbuffer0 = float4(gbuffer_normal * 0.5 + 0.5, 0.0);
    psout.gbuffer1 = float4(albedo, 0.0);
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

