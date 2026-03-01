#include "include/common/hlsl_common.h"

struct CameraParamsUb {
    float4x4 vp;
    float4x4 prev_vp;
    float4 camera_position;
};
#ifdef SPIRV
[[vk::binding(0, 0)]] ConstantBuffer<CameraParamsUb> CameraParamsUb_cb;
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

struct InstanceParameter {
    float4x4 model_matrix;
    uint material_id;
    uint joint_offset;
    uint joint_count;
    uint skinning_enabled;
};
#ifdef SPIRV
[[vk::binding(1, 0)]] StructuredBuffer<InstanceParameter> instance_parameter;
[[vk::binding(6, 0)]] StructuredBuffer<float4x4> skin_joint_matrices;
[[vk::binding(7, 0)]] StructuredBuffer<float4x4> prev_skin_joint_matrices;
[[vk::binding(5, 0)]] StructuredBuffer<float4x4> prev_instance_model_matrices;
#else
StructuredBuffer<InstanceParameter> instance_parameter : register(t0);
StructuredBuffer<float4x4> skin_joint_matrices : register(t3);
StructuredBuffer<float4x4> prev_skin_joint_matrices : register(t4);
StructuredBuffer<float4x4> prev_instance_model_matrices : register(t5);
#endif

struct VSInputSkinned {
    float3 position : POSITION;
    float3 normal : NORMAL;
    float2 uv0 : TEXCOORD0;
    float2 uv1 : TEXCOORD1;
    float3 tangent : TANGENT;
    float4 joint_indices : BLENDINDICES0;
    float4 joint_weights : BLENDWEIGHT0;
};

struct VSOutput {
    float4 position : SV_Position;
    float3 normal : NORMAL;
    float2 uv : TEXCOORD0;
    float3 tangent : TANGENT;
    uint instance_id : TEXCOORD1;
    uint material_id : TEXCOORD2;
    float4 curr_pos : TEXCOORD3;
    float4 prev_pos : TEXCOORD4;
};

VSOutput vs_main_skinned(VSInputSkinned vsin, uint InstanceID : SV_InstanceID, uint vertex_id : SV_VertexID
#ifdef SPIRV
    ,[[vk::builtin("DrawIndex")]] uint drawIndex : A
#else
#endif
)
{
    (void)vertex_id;
    VSOutput vsout;

#ifdef SPIRV
    uint mesh_id = mesh_draw_offset.mesh_id_offset + drawIndex;
#else
    uint mesh_id = mesh_draw_offset.mesh_id_offset;
#endif

    InstanceParameter instance_param = instance_parameter[mesh_id];
    float4x4 model = instance_param.model_matrix;
    float4x4 prev_model = prev_instance_model_matrices[mesh_id];

    float4x4 skin_mat = float4x4(
        1.0, 0.0, 0.0, 0.0,
        0.0, 1.0, 0.0, 0.0,
        0.0, 0.0, 1.0, 0.0,
        0.0, 0.0, 0.0, 1.0);
    float4x4 prev_skin_mat = float4x4(
        1.0, 0.0, 0.0, 0.0,
        0.0, 1.0, 0.0, 0.0,
        0.0, 0.0, 1.0, 0.0,
        0.0, 0.0, 0.0, 1.0);
    if (instance_param.skinning_enabled != 0 && instance_param.joint_count > 0)
    {
        uint4 joint_indices = uint4(vsin.joint_indices);
        uint4 joint_max = uint4(instance_param.joint_count - 1, instance_param.joint_count - 1,
                                instance_param.joint_count - 1, instance_param.joint_count - 1);
        joint_indices = min(joint_indices, joint_max);
        float4 joint_weights = vsin.joint_weights;
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

    float4 local_pos = mul(skin_mat, float4(vsin.position, 1.0));
    float4 prev_local_pos = mul(prev_skin_mat, float4(vsin.position, 1.0));
    float3 local_normal = mul((float3x3)skin_mat, vsin.normal);
    float3 local_tangent = mul((float3x3)skin_mat, vsin.tangent);

    vsout.position = mul(CameraParamsUb_cb.vp, mul(model, local_pos));
    vsout.normal = normalize(mul(model, float4(local_normal, 0.0)).xyz);
    vsout.uv = vsin.uv0;
    vsout.tangent = normalize(mul(model, float4(local_tangent, 0.0)).xyz);
    vsout.instance_id = InstanceID;
    vsout.material_id = instance_param.material_id;
    vsout.prev_pos = mul(CameraParamsUb_cb.prev_vp, mul(prev_model, prev_local_pos));
    vsout.curr_pos = mul(CameraParamsUb_cb.vp, mul(model, local_pos));
    return vsout;
}
