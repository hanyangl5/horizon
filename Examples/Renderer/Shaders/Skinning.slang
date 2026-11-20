#include "Common.slang"

// Matches GpuSkinningJob uploaded by GpuSkinning.
struct GpuSkinningJob
{
    uint positions;
    uint normals;
    uint texcoords;
    uint joints;
    uint weights;
    uint outputPositions;
    uint outputNormals;
    uint outputTexcoords;
    uint firstVertex;
    uint vertexCount;
    uint jointOffset;
    uint padding;
};

cbuffer RootConstant0
{
    uint jobsIndex;
    uint paletteIndex;
};

float4x4 loadJoint(ByteAddressBuffer palette, uint joint)
{
    // Runtime matrices store four columns; construct then transpose HLSL's row vectors.
    const uint offset = joint * 64;
    return transpose(float4x4(asfloat(palette.Load4(offset)), asfloat(palette.Load4(offset + 16)), asfloat(palette.Load4(offset + 32)),
                              asfloat(palette.Load4(offset + 48))));
}

// The cofactor matrix keeps normals perpendicular under non-uniform scale.
float3 transformNormal(float4x4 skin, float3 normal)
{
    const float3 x = float3(skin[0][0], skin[1][0], skin[2][0]);
    const float3 y = float3(skin[0][1], skin[1][1], skin[2][1]);
    const float3 z = float3(skin[0][2], skin[1][2], skin[2][2]);
    const float3 cofactor = cross(y, z) * normal.x + cross(z, x) * normal.y + cross(x, y) * normal.z;
    return normalize(cofactor * (dot(x, cross(y, z)) < 0.0 ? -1.0 : 1.0));
}

uint encodeVertexNormal(float3 normal)
{
    const uint2 quantized = (uint2)round(saturate(encodeOctNormal(normal)) * 65535.0);
    return quantized.x | (quantized.y << 16);
}

// One job per row of thread groups; each thread skins one vertex with up to four joints. Joints are four uint16 and
// weights four floats, as the renderer's vertex layout stores them.
[shader("compute")]
[numthreads(64, 1, 1)]
void main(uint3 group : SV_GroupID, uint3 thread : SV_GroupThreadID)
{
    ByteAddressBuffer    jobs = ResourceDescriptorHeap[jobsIndex];
    const GpuSkinningJob job = jobs.Load<GpuSkinningJob>(group.y * sizeof(GpuSkinningJob));
    const uint           vertex = group.x * 64 + thread.x;
    if (vertex >= job.vertexCount)
    {
        return;
    }
    const uint        source = job.firstVertex + vertex;
    ByteAddressBuffer palette = ResourceDescriptorHeap[paletteIndex];
    ByteAddressBuffer positions = ResourceDescriptorHeap[NonUniformResourceIndex(job.positions)];
    ByteAddressBuffer normals = ResourceDescriptorHeap[NonUniformResourceIndex(job.normals)];
    ByteAddressBuffer joints = ResourceDescriptorHeap[NonUniformResourceIndex(job.joints)];
    ByteAddressBuffer weights = ResourceDescriptorHeap[NonUniformResourceIndex(job.weights)];
    const uint2       packed = joints.Load2(source * 8);
    const uint4       index = uint4(packed.x & 0xffffu, packed.x >> 16, packed.y & 0xffffu, packed.y >> 16) + job.jointOffset;
    float4            weight = asfloat(weights.Load4(source * 16));
    weight /= max(dot(weight, 1.0), 1e-6);
    const float4x4 skin = loadJoint(palette, index.x) * weight.x + loadJoint(palette, index.y) * weight.y +
                          loadJoint(palette, index.z) * weight.z + loadJoint(palette, index.w) * weight.w;

    RWByteAddressBuffer outputPositions = ResourceDescriptorHeap[NonUniformResourceIndex(job.outputPositions)];
    RWByteAddressBuffer outputNormals = ResourceDescriptorHeap[NonUniformResourceIndex(job.outputNormals)];
    const float3        position = mul(skin, float4(asfloat(positions.Load3(source * 12)), 1.0)).xyz;
    outputPositions.Store3(source * 12, asuint(position));
    outputNormals.Store(source * 4, encodeVertexNormal(transformNormal(skin, decodeVertexNormal(normals.Load(source * 4)))));
    if (job.texcoords != INVALID_TEXTURE)
    {
        ByteAddressBuffer   texcoords = ResourceDescriptorHeap[NonUniformResourceIndex(job.texcoords)];
        RWByteAddressBuffer outputTexcoords = ResourceDescriptorHeap[NonUniformResourceIndex(job.outputTexcoords)];
        outputTexcoords.Store(source * 4, texcoords.Load(source * 4));
    }
}
