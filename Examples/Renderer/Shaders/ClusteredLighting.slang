#include "ClusterGrid.h"
#include "GpuLightData.h"

cbuffer RootConstant0
{
    float4 ClipX, ClipY, ClipW;
    uint Width, Height, LightCount, LightIndexOffset;
    uint LightsIndex, ActiveLightsIndex, MasksIndex, Padding;
};

groupshared float4 Spheres[LIGHTS_PER_BITMASK];
float4 UnitPlane(float4 Plane) { return Plane * rsqrt(dot(Plane.xyz, Plane.xyz)); }

float4x4 GetTilePlanes(uint2 Pixel)
{
    const float2 Scale = float2(2, -2) / float2(Width, Height);
    const float2 Start = float2(-1, 1) + Pixel * Scale;
    const float2 End = Start + LIGHT_TILE_SIZE * Scale;
    return float4x4(UnitPlane(ClipX - Start.x * ClipW), UnitPlane(End.x * ClipW - ClipX),
                    UnitPlane(Start.y * ClipW - ClipY), UnitPlane(ClipY - End.y * ClipW));
}

[numthreads(LIGHT_MASK_GROUP_SIZE_X, LIGHT_MASK_GROUP_SIZE_Y, 1)]
void BuildLightTileMasks(uint3 Tile : SV_DispatchThreadID, uint Lane : SV_GroupIndex)
{
    const uint FirstLight = Tile.z * LIGHTS_PER_BITMASK;
    const uint Light = FirstLight + Lane;
    if (Light < LightCount)
    {
        ByteAddressBuffer Indices = ResourceDescriptorHeap[ActiveLightsIndex];
        ByteAddressBuffer Lights = ResourceDescriptorHeap[LightsIndex];
        const uint Index = Indices.Load((LightIndexOffset + Light) * sizeof(uint));
        const float4 Bounds = Lights.Load<float4>(Index * GpuLightStride);
        Spheres[Lane] = float4(Bounds.xyz, rcp(Bounds.w));
    }
    GroupMemoryBarrierWithGroupSync();
    const uint2 Pixel = Tile.xy * LIGHT_TILE_SIZE;
    if (any(Pixel >= uint2(Width, Height)))
        return;
    const float4x4 Planes = GetTilePlanes(Pixel);
    const uint Count = min(LIGHTS_PER_BITMASK, LightCount - FirstLight);
    uint Mask = 0;
    for (uint I = 0; I < Count; ++I)
    {
        const float4 Sphere = Spheres[I];
        if (all(mul(Planes, float4(Sphere.xyz, 1)) >= -Sphere.w))
            Mask |= 1u << I;
    }
    RWByteAddressBuffer Masks = ResourceDescriptorHeap[MasksIndex];
    const uint TilesX = (Width + LIGHT_TILE_SIZE - 1) / LIGHT_TILE_SIZE;
    Masks.Store((GetTileLightMaskBase(Tile.xy, TilesX) + Tile.z) * sizeof(uint), Mask);
}
