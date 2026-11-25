#include "ClusteredLighting.h"
#include "DeferredLighting.h"
#include "RendererConstants.h"
#include "Profiler/IProfiler.h"

#include <math.h>
#include <stdlib.h>

ClusteredLighting::ClusteredLighting(hz::RenderContext& Context): Context(Context)
{
    Pipeline =
        Context.CreateComputePipeline({ .shaderDesc = { .stages = { { .stage = SHADER_STAGE_COMP, .pEntryPoint = "BuildLightTileMasks" } },
                                                        .pFileName = "ClusteredLighting.slang" },
                                        .Name = "Renderer.BuildLightTileMasks" });
    ASSERT(Pipeline.IsValid());
}

int ClusteredLighting::Compare(const void* A, const void* B)
{
    const LocalLightData& Left = *(const LocalLightData*)A;
    const LocalLightData& Right = *(const LocalLightData*)B;
    if (Left.Depth != Right.Depth)
    {
        return Left.Depth < Right.Depth ? -1 : 1;
    }
    return (Left.Index > Right.Index) - (Left.Index < Right.Index);
}

static bool ReserveBuffer(hz::RenderContext& Context, hz::GPUBuffer& Buffer, uint64_t Bytes, bool Mask)
{
    if (Bytes <= Buffer.GetSize())
    {
        return true;
    }
    hz::GPUBuffer Replacement = Context.CreateBuffer(
        { .Size = Bytes,
          .ElementCount = uint32_t(Bytes / sizeof(uint32_t)),
          .Name = Mask ? "Renderer.TileLightBitMasks" : "Renderer.ZBins",
          .usage = Mask ? RESOURCE_MEMORY_USAGE_GPU_ONLY : RESOURCE_MEMORY_USAGE_CPU_TO_GPU,
          .StartState = Mask ? RESOURCE_STATE_UNORDERED_ACCESS : RESOURCE_STATE_SHADER_RESOURCE,
          .Descriptors = Mask ? DescriptorType(DESCRIPTOR_TYPE_BUFFER_RAW | DESCRIPTOR_TYPE_RW_BUFFER_RAW) : DESCRIPTOR_TYPE_BUFFER_RAW,
          .Flags = Mask ? BUFFER_CREATION_FLAG_NONE : BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT });
    if (!Replacement.IsValid())
    {
        LOGF(Error, "Cannot allocate %llu bytes for clustered lighting", (unsigned long long)Bytes);
        return false;
    }
    Buffer = std::move(Replacement);
    return true;
}

static constexpr uint32_t LightBatchCount(uint32_t LightCount) { return (LightCount + LIGHTS_PER_BITMASK - 1) / LIGHTS_PER_BITMASK; }

static uint32_t DepthBin(float Depth, float Near, float BinScale)
{
    return uint32_t(fminf(fmaxf((Depth - Near) * BinScale, 0), float(CLUSTER_DEPTH_BIN_COUNT - 1)));
}

bool ClusteredLighting::Prepare(LightingFrameData& Frame, hz::GPUBuffer& ZBins, hz::GPUBuffer& TileLightBitMasks,
                                hz::Span<hz::RenderLight> Lights, hz::Span<uint32_t> Active, const FrameData& Camera)
{
    PROFILER_SET_CPU_SCOPE_AUTO_NAMED("ClusteredLighting::Prepare");
    const Vector4 ClipX = Camera.ViewProjection.getRow(0);
    const Vector4 ClipY = Camera.ViewProjection.getRow(1);
    const Vector4 ClipW = Camera.ViewProjection.getRow(3);
    Frame.ClusterLightingClipW = ClipW;
    Frame.ClusterLightingBinScale = CLUSTER_DEPTH_BIN_COUNT / (Camera.Far - Camera.Near);
    Frame.ClusterLightingTilesX = (Camera.Width + LIGHT_TILE_SIZE - 1) / LIGHT_TILE_SIZE;
    Frame.ClusterLightingTileLightBitMasksIndex = UINT32_MAX;
    Frame.ClusterLightingZBinsIndex = UINT32_MAX;
    Vector4 Planes[] = { ClipW + ClipX, ClipW - ClipX, ClipW + ClipY, ClipW - ClipY };
    for (Vector4& Plane : Planes)
    {
        Plane /= length(Plane.getXYZ());
    }
    const float DepthScale = length(ClipW.getXYZ());
    SortedLightList.Clear();
    Indices.Clear();
    for (uint32_t Index : Active)
    {
        const hz::RenderLight& Light = Lights.Data[Index];
        if (!Light.IsActiveLocalLight())
        {
            continue;
        }
        const float   Radius = Light.InfluenceRadius;
        const Vector4 Center(Light.transform.getCol3().getXYZ(), 1);
        const float   Depth = dot(ClipW, Center);
        if (Depth + Radius * DepthScale < Camera.Near || Depth - Radius * DepthScale > Camera.Far || dot(Planes[0], Center) < -Radius ||
            dot(Planes[1], Center) < -Radius || dot(Planes[2], Center) < -Radius || dot(Planes[3], Center) < -Radius)
        {
            continue;
        }
        if (SortedLightList.Size() == MaxClusteredLights)
        {
            return false;
        }
        SortedLightList.PushBack({ .Depth = Depth, .Index = Index });
    }
    const uint32_t TilesY = (Camera.Height + LIGHT_TILE_SIZE - 1) / LIGHT_TILE_SIZE;
    if (!SortedLightList.Empty())
    {
        const uint64_t MaskBytes = uint64_t(Frame.ClusterLightingTilesX) * TilesY * TILE_LIGHT_BITMASK_COUNT * sizeof(uint32_t);
        if (!Pipeline.IsValid() || !ReserveBuffer(Context, TileLightBitMasks, MaskBytes, true))
        {
            return false;
        }
        qsort(SortedLightList.GetData(), SortedLightList.Size(), sizeof(LocalLightData), Compare);
        for (Bin& Entry : Bins)
        {
            Entry = { .First = UINT32_MAX, .Last = 0 };
        }
        for (uint32_t I = 0; I < SortedLightList.Size(); ++I)
        {
            const LocalLightData&  Sorted = SortedLightList[I];
            const hz::RenderLight& Light = Lights.Data[Sorted.Index];
            Indices.PushBack(Sorted.Index);
            const float    Extent = Light.InfluenceRadius * DepthScale;
            const uint32_t First = DepthBin(Sorted.Depth - Extent, Camera.Near, Frame.ClusterLightingBinScale);
            const uint32_t Last = DepthBin(Sorted.Depth + Extent, Camera.Near, Frame.ClusterLightingBinScale);
            for (uint32_t Z = First; Z <= Last; ++Z)
            {
                if (Bins[Z].First == UINT32_MAX)
                {
                    Bins[Z].First = I;
                }
                Bins[Z].Last = I;
            }
        }
    }
    if (!ReserveBuffer(Context, ZBins, sizeof(Bins), false) || (!Indices.Empty() && !Context.UpdateBuffer(ZBins, 0, Bins, sizeof(Bins))))
    {
        Indices.Clear();
        return false;
    }
    Frame.ClusterLightingZBinsIndex = ZBins.GetSrvIndex();
    Frame.ClusterLightingTileLightBitMasksIndex = Indices.Empty() ? UINT32_MAX : TileLightBitMasks.GetSrvIndex();
    return true;
}

void ClusteredLighting::BuildLightTileMasks(hz::CommandList& Commands, const FrameData& Camera, const hz::GPUBuffer& TileLightBitMasks,
                                            const hz::GPUBuffer& Lights, const hz::GPUBuffer& ActiveLights,
                                            uint32_t ActiveLocalLightCount) const
{
    struct Constants
    {
        Vector4  ClipX, ClipY, ClipW;
        uint32_t Width, Height, LightCount, LightIndexOffset;
        uint32_t LightsIndex, ActiveLightsIndex, MasksIndex, Padding;
    };
    const Constants Bindings = { .ClipX = Camera.ViewProjection.getRow(0),
                                 .ClipY = Camera.ViewProjection.getRow(1),
                                 .ClipW = Camera.ViewProjection.getRow(3),
                                 .Width = Camera.Width,
                                 .Height = Camera.Height,
                                 .LightCount = Indices.Size(),
                                 .LightIndexOffset = ActiveLocalLightCount,
                                 .LightsIndex = Lights.GetSrvIndex(),
                                 .ActiveLightsIndex = ActiveLights.GetSrvIndex(),
                                 .MasksIndex = TileLightBitMasks.GetUavIndex() };
    const uint32_t  TilesX = (Camera.Width + LIGHT_TILE_SIZE - 1) / LIGHT_TILE_SIZE;
    const uint32_t  TilesY = (Camera.Height + LIGHT_TILE_SIZE - 1) / LIGHT_TILE_SIZE;
    Commands.BeginGpuTimestamp("BuildLightTileMasks");
    Commands.SetPipeline(Pipeline);
    Commands.SetRootConstants(Bindings);
    Commands.Dispatch((TilesX + LIGHT_MASK_GROUP_SIZE_X - 1) / LIGHT_MASK_GROUP_SIZE_X,
                      (TilesY + LIGHT_MASK_GROUP_SIZE_Y - 1) / LIGHT_MASK_GROUP_SIZE_Y, LightBatchCount(Indices.Size()),
                      { .buffers = { &Lights, &ActiveLights }, .storageBuffers = { &TileLightBitMasks } });
    Commands.EndGpuTimestamp();
}
