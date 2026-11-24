#include "ClusterGrid.h"

cbuffer RootConstant0
{
    uint DataIndex;
    uint MaskIndex;
};

groupshared float4 Spheres[32];
float4 UnitPlane(float4 Plane) { return Plane * rsqrt(dot(Plane.xyz, Plane.xyz)); }

[numthreads(8, 4, 1)]
void BuildMasks(uint3 Tile : SV_DispatchThreadID, uint Lane : SV_GroupIndex)
{
    ByteAddressBuffer Data = ResourceDescriptorHeap[DataIndex];
    const ClusterGrid Grid = Data.Load<ClusterGrid>(0);
    const uint Light = Tile.z * 32 + Lane;
    if (Light < Grid.FiniteCount)
        Spheres[Lane] = asfloat(Data.Load4(Grid.SphereOffset + Light * 16));
    GroupMemoryBarrierWithGroupSync();
    const uint2 Pixel = Tile.xy * LIGHT_TILE_SIZE;
    if (any(Pixel >= uint2(Grid.Width, Grid.Height)))
        return;
    const float2 Scale = float2(2, -2) / float2(Grid.Width, Grid.Height);
    const float2 Start = float2(-1, 1) + Pixel * Scale;
    const float2 End = Start + LIGHT_TILE_SIZE * Scale;
    const float4 Left = UnitPlane(Grid.ClipX - Start.x * Grid.ClipW);
    const float4 Right = UnitPlane(End.x * Grid.ClipW - Grid.ClipX);
    const float4 Top = UnitPlane(Start.y * Grid.ClipW - Grid.ClipY);
    const float4 Bottom = UnitPlane(Grid.ClipY - End.y * Grid.ClipW);
    uint Mask = 0;
    const uint Count = min(32u, Grid.FiniteCount - Tile.z * 32);
    for (uint I = 0; I < Count; ++I)
    {
        const float4 Center = float4(Spheres[I].xyz, 1);
        const float Distance = min(min(dot(Left, Center), dot(Right, Center)), min(dot(Top, Center), dot(Bottom, Center)));
        if (Distance >= -Spheres[I].w)
            Mask |= 1u << I;
    }
    RWByteAddressBuffer Masks = ResourceDescriptorHeap[MaskIndex];
    Masks.Store(((Tile.y * Grid.TilesX + Tile.x) * Grid.Words + Tile.z) * 4, Mask);
}
