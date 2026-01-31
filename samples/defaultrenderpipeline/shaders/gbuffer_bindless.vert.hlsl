
struct CameraParamsUb {
    float4x4 vp;
    float4x4 prev_vp;
    float4 camera_position;
};
[[vk::binding(0, 0)]] ConstantBuffer<CameraParamsUb> CameraParamsUb_cb;

struct DrawRootConstant { uint mesh_id_offset; };
[[vk::push_constant]] DrawRootConstant mesh_draw_offset;
// App sets mesh_id = DrawIndex + mesh_id_offset when using multi-draw; otherwise mesh_id_offset alone.

struct InstanceParameter {
    float4x4 model_matrix;
    uint material_id;
};
[[vk::binding(1, 0)]] StructuredBuffer<InstanceParameter> instance_parameter;

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

VSOutput main(VSInput vsin, uint InstanceID : SV_InstanceID, uint vertex_id : SV_VertexID)
{
    VSOutput vsout;
    uint mesh_id = mesh_draw_offset.mesh_id_offset;
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
