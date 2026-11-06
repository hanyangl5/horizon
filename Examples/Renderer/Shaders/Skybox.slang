// Adapted from Cookbook skybox.vert.slang / skybox.frag.slang; see ../Cookbook-LICENSE.txt.
#include "Common.slang"

cbuffer RootConstant0
{
    uint  frameIndex;
    uint  environmentIndex;
    uint  samplerIndex;
    float intensity;
};

static const float3 pos[8] = { float3(-1, -1, 1),  float3(1, -1, 1),  float3(1, 1, 1),  float3(-1, 1, 1),
                               float3(-1, -1, -1), float3(1, -1, -1), float3(1, 1, -1), float3(-1, 1, -1) };
static const uint   indices[36] = {
    0, 1, 2, 2, 3, 0, 1, 5, 6, 6, 2, 1, 7, 6, 5, 5, 4, 7, 4, 0, 3, 3, 7, 4, 4, 5, 1, 1, 0, 4, 3, 2, 6, 6, 7, 3
};

struct PSInput
{
    float4 position: SV_Position;
    float3 dir: TEXCOORD0;
};

PSInput VSMain(uint vertexId : SV_VertexID)
{
    StructuredBuffer<FrameData> frame = ResourceDescriptorHeap[frameIndex];
    int                         idx = indices[vertexId];
    PSInput                     output;
    // Center the cube on the eye to discard view translation while preserving camera rotation.
    output.position = mul(frame[0].viewProjection, float4(pos[idx] + frame[0].eye.xyz, 1.0));
    output.position.z = output.position.w;
    output.dir = pos[idx].xyz;
    return output;
}

float4 PSMain(PSInput input): SV_Target0
{
    TextureCube<float4> environment = ResourceDescriptorHeap[environmentIndex];
    SamplerState        surfaceSampler = SamplerDescriptorHeap[samplerIndex];
    float3              sky = float3(-input.dir.x, input.dir.y, -input.dir.z);
    return float4(environment.Sample(surfaceSampler, sky).rgb * max(intensity, 0.0), 1);
}
