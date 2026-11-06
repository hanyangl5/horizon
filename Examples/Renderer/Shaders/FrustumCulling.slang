// Adapted from Cookbook Renderer/shaders/FrustumCulling.comp.slang.
// Copyright (c) 2024 Packt. MIT license: ../Cookbook-LICENSE.txt.
#include "Common.slang"
cbuffer RootConstant0
{
    uint frameIndex;
    uint drawsIndex;
    uint instancesIndex;
    uint opaqueIndex;
    uint transparentIndex;
    uint statisticsIndex;
    uint drawCount;
    uint cullingEnabled;
    uint shadowsIndex;
};

struct AABB
{
    float pt[6];
};

#define Box_min_x box.pt[0]
#define Box_min_y box.pt[1]
#define Box_min_z box.pt[2]
#define Box_max_x box.pt[3]
#define Box_max_y box.pt[4]
#define Box_max_z box.pt[5]

bool isAABBinFrustum(AABB box, float4x4 vp, float4x4 inverseVP)
{
    float4 planes[6] = { vp[3] + vp[0], vp[3] - vp[0], vp[3] + vp[1], vp[3] - vp[1], vp[2], vp[3] - vp[2] };
    float4 corners[8];
    for (uint c = 0; c < 8; ++c)
    {
        float4 clip = float4(((c & 1) != 0) ? 1.0 : -1.0, ((c & 2) != 0) ? 1.0 : -1.0, ((c & 4) != 0) ? 1.0 : 0.0, 1.0);
        float4 corner = mul(inverseVP, clip);
        corners[c] = float4(corner.xyz / corner.w, 1.0);
    }

    for (int i = 0; i < 6; i++)
    {
        int r = 0;
        r += (dot(planes[i], float4(Box_min_x, Box_min_y, Box_min_z, 1.0f)) < 0) ? 1 : 0;
        r += (dot(planes[i], float4(Box_max_x, Box_min_y, Box_min_z, 1.0f)) < 0) ? 1 : 0;
        r += (dot(planes[i], float4(Box_min_x, Box_max_y, Box_min_z, 1.0f)) < 0) ? 1 : 0;
        r += (dot(planes[i], float4(Box_max_x, Box_max_y, Box_min_z, 1.0f)) < 0) ? 1 : 0;
        r += (dot(planes[i], float4(Box_min_x, Box_min_y, Box_max_z, 1.0f)) < 0) ? 1 : 0;
        r += (dot(planes[i], float4(Box_max_x, Box_min_y, Box_max_z, 1.0f)) < 0) ? 1 : 0;
        r += (dot(planes[i], float4(Box_min_x, Box_max_y, Box_max_z, 1.0f)) < 0) ? 1 : 0;
        r += (dot(planes[i], float4(Box_max_x, Box_max_y, Box_max_z, 1.0f)) < 0) ? 1 : 0;
        if (r == 8)
        {
            return false;
        }
    }

    int r = 0;
    r = 0;
    for (int i = 0; i < 8; i++)
    {
        r += ((corners[i].x > Box_max_x) ? 1 : 0);
    }
    if (r == 8)
    {
        return false;
    }
    r = 0;
    for (int i = 0; i < 8; i++)
    {
        r += ((corners[i].x < Box_min_x) ? 1 : 0);
    }
    if (r == 8)
    {
        return false;
    }
    r = 0;
    for (int i = 0; i < 8; i++)
    {
        r += ((corners[i].y > Box_max_y) ? 1 : 0);
    }
    if (r == 8)
    {
        return false;
    }
    r = 0;
    for (int i = 0; i < 8; i++)
    {
        r += ((corners[i].y < Box_min_y) ? 1 : 0);
    }
    if (r == 8)
    {
        return false;
    }
    r = 0;
    for (int i = 0; i < 8; i++)
    {
        r += ((corners[i].z > Box_max_z) ? 1 : 0);
    }
    if (r == 8)
    {
        return false;
    }
    r = 0;
    for (int i = 0; i < 8; i++)
    {
        r += ((corners[i].z < Box_min_z) ? 1 : 0);
    }
    if (r == 8)
    {
        return false;
    }

    return true;
}

[shader("compute")]
[numthreads(1, 1, 1)]
void ClearStatistics()
{
    RWByteAddressBuffer statistics = ResourceDescriptorHeap[statisticsIndex];
    statistics.Store4(0, uint4(0, 0, 0, 0));
}

[shader("compute")]
[numthreads(64, 1, 1)]
void CSMain(uint3 id : SV_DispatchThreadID)
{
    if (id.x >= drawCount)
    {
        return;
    }
    StructuredBuffer<FrameData> frames = ResourceDescriptorHeap[frameIndex];
    ByteAddressBuffer           draws = ResourceDescriptorHeap[drawsIndex];
    ByteAddressBuffer           instances = ResourceDescriptorHeap[instancesIndex];
    RWByteAddressBuffer         opaque = ResourceDescriptorHeap[opaqueIndex];
    RWByteAddressBuffer         transparent = ResourceDescriptorHeap[transparentIndex];
    RWByteAddressBuffer         shadows = ResourceDescriptorHeap[shadowsIndex];
    RWByteAddressBuffer         statistics = ResourceDescriptorHeap[statisticsIndex];
    uint4                       object = draws.Load4(id.x * 48);
    uint                        indexCount = draws.Load(id.x * 48 + 44);
    uint                        boundsOffset = object.x * 192 + 160;
    float3                      minimum = asfloat(instances.Load3(boundsOffset));
    float3                      maximum = asfloat(instances.Load3(boundsOffset + 12));
    AABB                        box = { { minimum.x, minimum.y, minimum.z, maximum.x, maximum.y, maximum.z } };
    bool keep = cullingEnabled == 0 || isAABBinFrustum(box, frames[0].viewProjection, frames[0].inverseViewProjection);
    opaque.Store(id.x * 20, id.x);
    opaque.Store4(id.x * 20 + 4, uint4(indexCount, keep && object.w != 2 ? 1 : 0, 0, 0));
    transparent.Store(id.x * 20, id.x);
    transparent.Store4(id.x * 20 + 4, uint4(indexCount, keep && object.w == 2 ? 1 : 0, 0, 0));
    shadows.Store(id.x * 20, id.x);
    shadows.Store4(id.x * 20 + 4, uint4(indexCount, object.w != 2 ? 1 : 0, 0, 0));
    if (keep)
    {
        uint ignored;
        statistics.InterlockedAdd(0, 1, ignored);
        statistics.InterlockedAdd(4, indexCount / 3, ignored);
    }
}
