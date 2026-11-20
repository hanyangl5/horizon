// Adapted from Cookbook Renderer/shaders/FrustumCulling.comp.slang.
// Copyright (c) 2024 Packt. MIT license: ../Cookbook-LICENSE.txt.
#include "Common.slang"
#include "Material.slang"
cbuffer RootConstant0
{
    uint FrameIndex;
    uint DrawsIndex;
    uint InstancesIndex;
    uint MaterialsIndex;
    uint FiltersIndex;
    uint FilterCount;
    uint StatisticsIndex;
    uint DrawCount;
    uint CullingEnabled;
};

// Matches DrawListFilter in GpuDrawList.h. A draw enters a list when its alpha mode bit is set in alphaModes and its
// material carries every bit of requiredTags; frustum culling applies only to lists that ask for it.
struct DrawListFilter
{
    uint AlphaModes;
    uint RequiredTags;
    uint FrustumCull;
    uint CommandsIndex;
};

struct AABB
{
    float Points[6];
};

#define BOX_MIN_X Box.Points[0]
#define BOX_MIN_Y Box.Points[1]
#define BOX_MIN_Z Box.Points[2]
#define BOX_MAX_X Box.Points[3]
#define BOX_MAX_Y Box.Points[4]
#define BOX_MAX_Z Box.Points[5]

bool IsAABBInFrustum(AABB Box, float4x4 Vp, float4x4 InverseVP)
{
    float4 Planes[6] = { Vp[3] + Vp[0], Vp[3] - Vp[0], Vp[3] + Vp[1], Vp[3] - Vp[1], Vp[2], Vp[3] - Vp[2] };
    float4 Corners[8];
    for (uint C = 0; C < 8; ++C)
    {
        float4 Clip = float4(((C & 1) != 0) ? 1.0 : -1.0, ((C & 2) != 0) ? 1.0 : -1.0, ((C & 4) != 0) ? 1.0 : 0.0, 1.0);
        float4 Corner = mul(InverseVP, Clip);
        Corners[C] = float4(Corner.xyz / Corner.w, 1.0);
    }

    for (int I = 0; I < 6; I++)
    {
        int R = 0;
        R += (dot(Planes[I], float4(BOX_MIN_X, BOX_MIN_Y, BOX_MIN_Z, 1.0f)) < 0) ? 1 : 0;
        R += (dot(Planes[I], float4(BOX_MAX_X, BOX_MIN_Y, BOX_MIN_Z, 1.0f)) < 0) ? 1 : 0;
        R += (dot(Planes[I], float4(BOX_MIN_X, BOX_MAX_Y, BOX_MIN_Z, 1.0f)) < 0) ? 1 : 0;
        R += (dot(Planes[I], float4(BOX_MAX_X, BOX_MAX_Y, BOX_MIN_Z, 1.0f)) < 0) ? 1 : 0;
        R += (dot(Planes[I], float4(BOX_MIN_X, BOX_MIN_Y, BOX_MAX_Z, 1.0f)) < 0) ? 1 : 0;
        R += (dot(Planes[I], float4(BOX_MAX_X, BOX_MIN_Y, BOX_MAX_Z, 1.0f)) < 0) ? 1 : 0;
        R += (dot(Planes[I], float4(BOX_MIN_X, BOX_MAX_Y, BOX_MAX_Z, 1.0f)) < 0) ? 1 : 0;
        R += (dot(Planes[I], float4(BOX_MAX_X, BOX_MAX_Y, BOX_MAX_Z, 1.0f)) < 0) ? 1 : 0;
        if (R == 8)
        {
            return false;
        }
    }

    int R = 0;
    R = 0;
    for (int I = 0; I < 8; I++)
    {
        R += ((Corners[I].x > BOX_MAX_X) ? 1 : 0);
    }
    if (R == 8)
    {
        return false;
    }
    R = 0;
    for (int I = 0; I < 8; I++)
    {
        R += ((Corners[I].x < BOX_MIN_X) ? 1 : 0);
    }
    if (R == 8)
    {
        return false;
    }
    R = 0;
    for (int I = 0; I < 8; I++)
    {
        R += ((Corners[I].y > BOX_MAX_Y) ? 1 : 0);
    }
    if (R == 8)
    {
        return false;
    }
    R = 0;
    for (int I = 0; I < 8; I++)
    {
        R += ((Corners[I].y < BOX_MIN_Y) ? 1 : 0);
    }
    if (R == 8)
    {
        return false;
    }
    R = 0;
    for (int I = 0; I < 8; I++)
    {
        R += ((Corners[I].z > BOX_MAX_Z) ? 1 : 0);
    }
    if (R == 8)
    {
        return false;
    }
    R = 0;
    for (int I = 0; I < 8; I++)
    {
        R += ((Corners[I].z < BOX_MIN_Z) ? 1 : 0);
    }
    if (R == 8)
    {
        return false;
    }

    return true;
}

[shader("compute")]
[numthreads(1, 1, 1)]
void ClearStatistics()
{
    RWByteAddressBuffer Statistics = ResourceDescriptorHeap[StatisticsIndex];
    Statistics.Store4(0, uint4(0, 0, 0, 0));
}

[shader("compute")]
[numthreads(64, 1, 1)]
void CSMain(uint3 ID : SV_DispatchThreadID)
{
    if (ID.x >= DrawCount)
    {
        return;
    }
    StructuredBuffer<FrameData> Frames = ResourceDescriptorHeap[FrameIndex];
    ByteAddressBuffer           Draws = ResourceDescriptorHeap[DrawsIndex];
    ByteAddressBuffer           Instances = ResourceDescriptorHeap[InstancesIndex];
    ByteAddressBuffer           Materials = ResourceDescriptorHeap[MaterialsIndex];
    StructuredBuffer<DrawListFilter> Filters = ResourceDescriptorHeap[FiltersIndex];
    RWByteAddressBuffer         Statistics = ResourceDescriptorHeap[StatisticsIndex];
    uint4                       Object = Draws.Load4(ID.x * 48);
    uint                        IndexCount = Draws.Load(ID.x * 48 + 44);
    uint                        BoundsOffset = Object.x * 192 + 160;
    float3                      Minimum = asfloat(Instances.Load3(BoundsOffset));
    float3                      Maximum = asfloat(Instances.Load3(BoundsOffset + 12));
    AABB                        Box = { { Minimum.x, Minimum.y, Minimum.z, Maximum.x, Maximum.y, Maximum.z } };
    bool Keep = CullingEnabled == 0 || IsAABBInFrustum(Box, Frames[0].ViewProjection, Frames[0].InverseViewProjection);
    const uint Tags = Materials.Load<GpuMaterialData>(Object.y * sizeof(GpuMaterialData)).Tags;
    // Every list holds one slot per draw, so a draw id selects the same entry in all of them.
    for (uint I = 0; I < FilterCount; ++I)
    {
        const DrawListFilter Filter = Filters[I];
        RWByteAddressBuffer  Commands = ResourceDescriptorHeap[Filter.CommandsIndex];
        const bool Selected = (Filter.AlphaModes & (1u << Object.w)) != 0 && (Tags & Filter.RequiredTags) == Filter.RequiredTags &&
                              (Filter.FrustumCull == 0 || Keep);
        Commands.Store(ID.x * 20, ID.x);
        Commands.Store4(ID.x * 20 + 4, uint4(IndexCount, Selected ? 1 : 0, 0, 0));
    }
    if (Keep)
    {
        uint Ignored;
        Statistics.InterlockedAdd(0, 1, Ignored);
        Statistics.InterlockedAdd(4, IndexCount / 3, Ignored);
    }
}
