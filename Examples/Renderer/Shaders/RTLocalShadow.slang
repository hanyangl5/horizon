#define RT_LOCAL_SHADOW
#include "RTLocalShadowConfig.h"
#include "RTShared.slang"
#include "BlueNoise.slang"
#include "MonteCarlo.slang"
#include "DeferredLightCommon.slang"

static const uint  MaxShadowLights = 8;
static const float TargetRays = 4.0;
static const float MaxRaysPerLight = 16.0;
groupshared uint   GroupCount;

bool LoadReceiver(uint2 Pixel, uint2 Size, out float3 P, out float3 N, out float3 Origin)
{
    P = N = Origin = 0;
    Texture2D<float> Depth = ResourceDescriptorHeap[DepthIndex];
    uint2            Full;
    Depth.GetDimensions(Full.x, Full.y);
    int2  Source = RTSourcePixel(Pixel, Size, Full);
    float D = Depth.Load(int3(Source, 0));
    if (D >= 1.0 || TlasIndex == INVALID_TEXTURE)
    {
        return false;
    }
    StructuredBuffer<FrameData> Frames = ResourceDescriptorHeap[FrameIndexBuffer];
    Texture2D<float4>           Normals = ResourceDescriptorHeap[NormalIndex];
    float2                      UV = (float2(Source) + 0.5) / Full;
    P = ReconstructWorldPosition(UV, D, Frames[0].InverseViewProjection);
    N = DecodeOctNormal(Normals.Load(int3(Source, 0)).xy);
    Origin = RTGBufferRayOrigin(UV, D, P, N, Frames[0].Eye.xyz, Frames[0].InverseViewProjection);
    return true;
}

[numthreads(LOCAL_SHADOW_GROUP_SIZE, LOCAL_SHADOW_GROUP_SIZE, 1)]
void ResolveLocalShadowSlots(uint2 Pixel : SV_DispatchThreadID, uint2 Group : SV_GroupID, uint Lane : SV_GroupIndex)
{
    RWTexture2D<uint4> Slots = ResourceDescriptorHeap[LocalShadowSlots];
    uint2              Size;
    Slots.GetDimensions(Size.x, Size.y);
    if (Lane == 0)
    {
        GroupCount = 0;
    }
    GroupMemoryBarrierWithGroupSync();
    uint Count = 0;
    if (all(Pixel < Size))
    {
        uint4  Packed = 0;
        float3 P, N, Origin;
        if (LoadReceiver(Pixel, Size, P, N, Origin))
        {
            StructuredBuffer<FrameData>         Frames = ResourceDescriptorHeap[FrameIndexBuffer];
            StructuredBuffer<LightingFrameData> Lighting = ResourceDescriptorHeap[LightingIndex];
            LightingFrameData                   F = Lighting[0];
            ByteAddressBuffer                   Bins = ResourceDescriptorHeap[F.ClusterLightingZBinsIndex];
            ByteAddressBuffer                   Masks = ResourceDescriptorHeap[F.ClusterLightingTileLightBitMasksIndex];
            ByteAddressBuffer                   Lights = ResourceDescriptorHeap[F.LocalLightsIndex];
            ByteAddressBuffer                   Indices = ResourceDescriptorHeap[F.ActiveLocalLightsIndex];
            uint2                               Range = GetZBinLightRange(Bins, F, Frames[0].Near, P);
            uint2                               Source = RTSourcePixel(Pixel, Size, uint2(Frames[0].Width, Frames[0].Height));
            uint                                Base = GetTileLightMaskBase(Source / LIGHT_TILE_SIZE, F.ClusterLightingTilesX);
            for (uint Batch = Range.x / LIGHTS_PER_BITMASK; Batch <= Range.y / LIGHTS_PER_BITMASK && Count < MaxShadowLights; ++Batch)
            {
                uint First = Batch * LIGHTS_PER_BITMASK;
                uint Bits = Masks.Load((Base + Batch) * 4) & GetLightRangeMask(Range, First);
                while (Bits && Count < MaxShadowLights)
                {
                    uint Sorted = First + firstbitlow(Bits);
                    Bits &= Bits - 1;
                    GpuLightData Light = LoadLocalLight(Lights, Indices, F.LocalLightCount + Sorted);
                    float3       ToLight, L;
                    float        Dist2;
                    uint         Type = uint(Light.DirectionAndType.w);
                    if (!Light.CastLocalShadow || !any(Light.Color.rgb > 0) ||
                        GetLocalLightAttenuation(Light, Type, P, ToLight, L, Dist2) <= 0 || dot(N, L) <= 0 ||
                        (Type == LightRect && dot(Light.DirectionAndType.xyz, -L) <= 0))
                    {
                        continue;
                    }
                    Packed[Count / 2] |= (Sorted + 1) << (16 * (Count & 1));
                    ++Count;
                }
            }
        }
        Slots[Pixel] = Packed;
    }
    uint WaveCount = WaveActiveSum(Count);
    if (WaveIsFirstLane())
    {
        InterlockedAdd(GroupCount, WaveCount);
    }
    GroupMemoryBarrierWithGroupSync();
    if (Lane == 0)
    {
        RWTexture2D<uint> Counts = ResourceDescriptorHeap[LocalShadowCounts];
        Counts[Group] = GroupCount;
    }
}

[numthreads(LOCAL_SHADOW_GROUP_SIZE, LOCAL_SHADOW_GROUP_SIZE, 1)]
void TraceLocalShadowMask(uint2 Pixel : SV_DispatchThreadID)
{
    RWTexture2D<uint> Mask = ResourceDescriptorHeap[OutputIndex];
    uint2             Size;
    Mask.GetDimensions(Size.x, Size.y);
    if (any(Pixel >= Size))
    {
        return;
    }
    Texture2D<uint4> Slots = ResourceDescriptorHeap[LocalShadowSlots];
    uint4            Packed = Slots.Load(int3(Pixel, 0));
    uint             Visibility = 0xff;
    float3           P, N, Origin;
    if (Packed.x != 0 && LoadReceiver(Pixel, Size, P, N, Origin))
    {
        Texture2D<uint> Total = ResourceDescriptorHeap[LocalShadowTotal];
        float           Budget = min(MaxRaysPerLight, TargetRays * float(Size.x) * Size.y / max(1u, Total.Load(int3(0, 0, 0))));
        StructuredBuffer<LightingFrameData> Lighting = ResourceDescriptorHeap[LightingIndex];
        LightingFrameData                   F = Lighting[0];
        ByteAddressBuffer                   Lights = ResourceDescriptorHeap[F.LocalLightsIndex];
        ByteAddressBuffer                   Indices = ResourceDescriptorHeap[F.ActiveLocalLightsIndex];
        RaytracingAccelerationStructure     Scene = ResourceDescriptorHeap[TlasIndex];
        Texture2D<float2>                   Noise = ResourceDescriptorHeap[BlueNoiseIndex];
        for (uint Slot = 0; Slot < MaxShadowLights; ++Slot)
        {
            uint Id = (Packed[Slot / 2] >> (16 * (Slot & 1))) & 0xffff;
            if (!Id)
            {
                break;
            }
            float2 Xi = GetBlueNoiseVec2(Noise, Pixel, FrameNumber + Slot * 17);
            uint   Samples = uint(floor(Budget)) + uint(Xi.x < frac(Budget));
            if (!Samples)
            {
                continue;
            }
            GpuLightData Light = LoadLocalLight(Lights, Indices, F.LocalLightCount + Id - 1);
            float        Visible = 0;
            for (uint I = 0; I < Samples; ++I)
            {
                float2 U = frac(GetBlueNoiseVec2(Noise, Pixel, FrameNumber + Slot * 17 + 7) + I * float2(0.7548777, 0.5698403));
                float3 To = Light.PositionAndInvRadius.xyz - Origin;
                if (uint(Light.DirectionAndType.w) == LightRect)
                {
                    To += (2 * U.x - 1) * Light.HalfAxisX.xyz + (2 * U.y - 1) * Light.HalfAxisY.xyz;
                }
                else if (Light.SourceRadius > 0)
                {
                    float2 Disk = Light.SourceRadius * sqrt(U.x) * float2(cos(2 * PI * U.y), sin(2 * PI * U.y));
                    To += mul(float3(Disk, 0), GetTangentBasis(normalize(To)));
                }
                float   Distance = length(To);
                RayDesc Ray = { Origin, 0, To / max(Distance, 1e-6), Distance * 0.999 };
                if (Distance > 0 && dot(N, Ray.Direction) > 0)
                {
                    Visible += TraceRTShadow(Scene, Ray, AlphaTestMethod != 0);
                }
            }
            if (Visible == 0 || (Visible < Samples && Visible <= Samples * Xi.y))
            {
                Visibility &= ~(1u << Slot);
            }
        }
    }
    Mask[Pixel] = Visibility;
}
