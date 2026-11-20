#include "Common.slang"

// Matches GpuSkinningJob uploaded by GpuSkinning.
struct GpuSkinningJob
{
    uint Positions;
    uint Normals;
    uint Texcoords;
    uint Joints;
    uint Weights;
    uint OutputPositions;
    uint OutputNormals;
    uint OutputTexcoords;
    uint FirstVertex;
    uint VertexCount;
    uint JointOffset;
    uint Padding;
};

cbuffer RootConstant0
{
    uint JobsIndex;
    uint PaletteIndex;
};

float4x4 LoadJoint(ByteAddressBuffer Palette, uint Joint)
{
    // Runtime matrices store four columns; construct then transpose HLSL's row vectors.
    const uint Offset = Joint * 64;
    return transpose(float4x4(asfloat(Palette.Load4(Offset)), asfloat(Palette.Load4(Offset + 16)), asfloat(Palette.Load4(Offset + 32)),
                              asfloat(Palette.Load4(Offset + 48))));
}

// The cofactor matrix keeps normals perpendicular under non-uniform scale.
float3 TransformNormal(float4x4 Skin, float3 Normal)
{
    const float3 X = float3(Skin[0][0], Skin[1][0], Skin[2][0]);
    const float3 Y = float3(Skin[0][1], Skin[1][1], Skin[2][1]);
    const float3 Z = float3(Skin[0][2], Skin[1][2], Skin[2][2]);
    const float3 Cofactor = cross(Y, Z) * Normal.x + cross(Z, X) * Normal.y + cross(X, Y) * Normal.z;
    return normalize(Cofactor * (dot(X, cross(Y, Z)) < 0.0 ? -1.0 : 1.0));
}

uint EncodeVertexNormal(float3 Normal)
{
    const uint2 Quantized = (uint2)round(saturate(EncodeOctNormal(Normal)) * 65535.0);
    return Quantized.x | (Quantized.y << 16);
}

// One job per row of thread groups; each thread skins one vertex with up to four joints. Joints are four uint16 and
// weights four floats, as the renderer's vertex layout stores them.
[shader("compute")]
[numthreads(64, 1, 1)]
void main(uint3 Group : SV_GroupID, uint3 Thread : SV_GroupThreadID)
{
    ByteAddressBuffer    Jobs = ResourceDescriptorHeap[JobsIndex];
    const GpuSkinningJob Job = Jobs.Load<GpuSkinningJob>(Group.y * sizeof(GpuSkinningJob));
    const uint           Vertex = Group.x * 64 + Thread.x;
    if (Vertex >= Job.VertexCount)
    {
        return;
    }
    const uint        Source = Job.FirstVertex + Vertex;
    ByteAddressBuffer Palette = ResourceDescriptorHeap[PaletteIndex];
    ByteAddressBuffer Positions = ResourceDescriptorHeap[NonUniformResourceIndex(Job.Positions)];
    ByteAddressBuffer Normals = ResourceDescriptorHeap[NonUniformResourceIndex(Job.Normals)];
    ByteAddressBuffer Joints = ResourceDescriptorHeap[NonUniformResourceIndex(Job.Joints)];
    ByteAddressBuffer Weights = ResourceDescriptorHeap[NonUniformResourceIndex(Job.Weights)];
    const uint2       Packed = Joints.Load2(Source * 8);
    const uint4       Index = uint4(Packed.x & 0xffffu, Packed.x >> 16, Packed.y & 0xffffu, Packed.y >> 16) + Job.JointOffset;
    float4            Weight = asfloat(Weights.Load4(Source * 16));
    Weight /= max(dot(Weight, 1.0), 1e-6);
    const float4x4 Skin = LoadJoint(Palette, Index.x) * Weight.x + LoadJoint(Palette, Index.y) * Weight.y +
                          LoadJoint(Palette, Index.z) * Weight.z + LoadJoint(Palette, Index.w) * Weight.w;

    RWByteAddressBuffer OutputPositions = ResourceDescriptorHeap[NonUniformResourceIndex(Job.OutputPositions)];
    RWByteAddressBuffer OutputNormals = ResourceDescriptorHeap[NonUniformResourceIndex(Job.OutputNormals)];
    const float3        Position = mul(Skin, float4(asfloat(Positions.Load3(Source * 12)), 1.0)).xyz;
    OutputPositions.Store3(Source * 12, asuint(Position));
    OutputNormals.Store(Source * 4, EncodeVertexNormal(TransformNormal(Skin, DecodeVertexNormal(Normals.Load(Source * 4)))));
    if (Job.Texcoords != INVALID_TEXTURE)
    {
        ByteAddressBuffer   Texcoords = ResourceDescriptorHeap[NonUniformResourceIndex(Job.Texcoords)];
        RWByteAddressBuffer OutputTexcoords = ResourceDescriptorHeap[NonUniformResourceIndex(Job.OutputTexcoords)];
        OutputTexcoords.Store(Source * 4, Texcoords.Load(Source * 4));
    }
}
