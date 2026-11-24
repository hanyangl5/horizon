// Adapted from Cookbook skybox.vert.slang / skybox.frag.slang; see ../Cookbook-LICENSE.txt.
#include "Common.slang"

cbuffer RootConstant0
{
    uint  FrameIndex;
    uint  EnvironmentIndex;
    uint  SamplerIndex;
    float Intensity;
    float4 Tint;
};

static const float3 Positions[8] = { float3(-1, -1, 1),  float3(1, -1, 1),  float3(1, 1, 1),  float3(-1, 1, 1),
                               float3(-1, -1, -1), float3(1, -1, -1), float3(1, 1, -1), float3(-1, 1, -1) };
static const uint   Indices[36] = {
    0, 1, 2, 2, 3, 0, 1, 5, 6, 6, 2, 1, 7, 6, 5, 5, 4, 7, 4, 0, 3, 3, 7, 4, 4, 5, 1, 1, 0, 4, 3, 2, 6, 6, 7, 3
};

struct PSInput
{
    float4 Position: SV_Position;
    float3 Dir: TEXCOORD0;
};

PSInput VSMain(uint VertexId : SV_VertexID)
{
    StructuredBuffer<FrameData> Frame = ResourceDescriptorHeap[FrameIndex];
    int                         Idx = Indices[VertexId];
    PSInput                     Output;
    // Center the cube on the eye to discard view translation while preserving camera rotation.
    Output.Position = mul(Frame[0].ViewProjection, float4(Positions[Idx] + Frame[0].Eye.xyz, 1.0));
    Output.Position.z = Output.Position.w;
    Output.Dir = Positions[Idx].xyz;
    return Output;
}

float4 PSMain(PSInput Input): SV_Target0
{
    TextureCube<float4> Environment = ResourceDescriptorHeap[EnvironmentIndex];
    SamplerState        SurfaceSampler = SamplerDescriptorHeap[SamplerIndex];
    float3              Sky = float3(-Input.Dir.x, Input.Dir.y, -Input.Dir.z);
    return float4(Environment.Sample(SurfaceSampler, Sky).rgb * max(Intensity, 0.0) * Tint.rgb, 1);
}
