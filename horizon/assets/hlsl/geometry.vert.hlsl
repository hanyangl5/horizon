#include "include/common/bindless.hlsl"
#ifdef VULKAN
#extension GL_EXT_nonuniform_qualifier : enable
#endif

CBUFFER(CameraParamsUb, UPDATE_FREQ_PER_FRAME, b0, binding = 0)
{
    DATA(float4x4, vp, None);
    DATA(float4x4, prev_vp, None);
    DATA(float4, camera_position, None);
};

PUSH_CONSTANT(DrawRootConstant, b1){
    DATA(uint, mesh_id_offset, None);
};


STRUCT(InstanceParameter)
{
    DATA(float4x4,  model_matrix, None);
    DATA(uint, material_id, None);
};

RES(Buffer(InstanceParameter), instance_parameter, UPDATE_FREQ_PER_FRAME, t3, binding = 1);

STRUCT(VSInput)
{
	DATA(float3, position, POSITION);
	DATA(float3, normal, NORMAL);
	DATA(float2, uv0, UV0);
	DATA(float2, uv1, UV1);
	DATA(float3, tangent, TANGENT);
};

// // 52bytes vertex layout

// STRUCT(PackedVsInput)
// {
// 	DATA(float, packed[VERETX_LAYOUT_STRIDE], None);
// };

// RES(Buffer(PackedVsInput), vertex_buffers[], UPDATE_FREQ_BINDLESS, t3, binding = 2);

STRUCT(VSOutput)
{
	DATA(float4, position, SV_Position);
    DATA(float3, world_pos, POSITION);
	DATA(float3, normal, NORMAL);
	DATA(float2, uv, TEXCOORD0);
	DATA(float3, tangent, TANGENT);
    DATA(FLAT(uint), instance_id, None);
#ifdef VULKAN
    DATA(FLAT(uint), material_id, None);
#endif
    DATA(float4, curr_pos, None);
    DATA(float4, prev_pos, None);
};

VSOutput VS_MAIN( VSInput vsin, SV_InstanceID(uint) InstanceID, SV_VertexID(uint) vertex_id)
{
    INIT_MAIN;
    VSOutput vsout;
    uint mesh_id;
#ifdef VULKAN
    mesh_id = gl_DrawID;
#endif
    mesh_id += Get(mesh_id_offset);
    float4x4 model = Get(instance_parameter)[mesh_id].model_matrix;

    // float vbp[VERETX_LAYOUT_STRIDE] = Get(vertex_buffers)[0][mesh_id + vertex_id].packed;
    // vsin.position = GetVertexPositionFromPackedVertexBuffer(vbp);
    // vsin.normal = GetVertexNormalFromPackedVertexBuffer(vbp);
    // vsin.uv0 = GetVertexUv0FromPackedVertexBuffer(vbp);
    // vsin.uv1 = GetVertexUv1FromPackedVertexBuffer(vbp);
    // vsin.tangent = GetVertexTangentFromPackedVertexBuffer(vbp);
    
    vsout.position = vp * model * float4(vsin.position, 1.0);
    vsout.world_pos = (model * float4(vsin.position, 1.0)).xyz;
    //transpose(inverse(model)
    vsout.normal = normalize((model * float4(vsin.normal, 0.0)).xyz);
    vsout.uv = vsin.uv0;
    vsout.tangent = normalize((model * float4(vsin.tangent, 0.0)).xyz);
    vsout.instance_id = InstanceID;
    vsout.material_id = Get(instance_parameter)[mesh_id].material_id;
    vsout.prev_pos = prev_vp * model * float4(vsin.position, 1.0); // old
    vsout.curr_pos = vp * model * float4(vsin.position, 1.0); // new
    RETURN(vsout);
}
