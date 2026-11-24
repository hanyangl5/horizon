#include "ClusteredLighting.h"
#include "../Shaders/ClusterGrid.h"
#include "Profiler/IProfiler.h"

#include <math.h>
#include <stdlib.h>

ClusteredLighting::ClusteredLighting(hz::RenderContext& Context): Context(Context)
{
    Pipeline = Context.CreateComputePipeline({ .shaderDesc = { .stages = { { .stage = SHADER_STAGE_COMP, .pEntryPoint = "BuildMasks" } },
                                                               .pFileName = "ClusteredLighting.slang" },
                                               .Name = "Renderer.BuildLightTileMasks" });
    ASSERT(Pipeline.IsValid());
}

int ClusteredLighting::Compare(const void* A, const void* B)
{
    const Candidate& Left = *(const Candidate*)A;
    const Candidate& Right = *(const Candidate*)B;
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
    const uint64_t Capacity = max(Bytes, Buffer.GetSize() * 2);
    hz::GPUBuffer  Replacement = Context.CreateBuffer(
        { .Size = Capacity,
          .ElementCount = uint32_t(Capacity / 4),
          .Name = Mask ? "Renderer.LightTileMasks" : "Renderer.ClusterLightData",
          .usage = Mask ? RESOURCE_MEMORY_USAGE_GPU_ONLY : RESOURCE_MEMORY_USAGE_CPU_TO_GPU,
          .StartState = Mask ? RESOURCE_STATE_UNORDERED_ACCESS : RESOURCE_STATE_SHADER_RESOURCE,
          .Descriptors = Mask ? DescriptorType(DESCRIPTOR_TYPE_BUFFER_RAW | DESCRIPTOR_TYPE_RW_BUFFER_RAW) : DESCRIPTOR_TYPE_BUFFER_RAW,
          .Flags = Mask ? BUFFER_CREATION_FLAG_NONE : BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT });
    if (!Replacement.IsValid())
    {
        LOGF(Error, "Cannot allocate %llu bytes for clustered lighting", (unsigned long long)Capacity);
        return false;
    }
    Buffer = std::move(Replacement);
    return true;
}

static uint32_t DepthBin(float Depth, const ClusterGrid& Grid)
{
    return uint32_t(fminf(fmaxf((Depth - Grid.Near) * Grid.BinScale, 0), float(Grid.BinCount - 1)));
}

bool ClusteredLighting::Prepare(uint32_t Slot, hz::Span<hz::RenderLight> Lights, hz::Span<uint32_t> Active, uint32_t MainDirectional,
                                const Matrix4& ViewProjection, float Near, float Far, uint32_t Width, uint32_t Height)
{
    PROFILER_SET_CPU_SCOPE_AUTO_NAMED("ClusteredLighting::Prepare");
    Frame&      Output = Frames[Slot];
    ClusterGrid Grid = { .ClipX = ViewProjection.getRow(0),
                         .ClipY = ViewProjection.getRow(1),
                         .ClipW = ViewProjection.getRow(3),
                         .Width = Width,
                         .Height = Height,
                         .TilesX = (Width + LIGHT_TILE_SIZE - 1) / LIGHT_TILE_SIZE,
                         .Near = Near,
                         .BinScale = TF_ARRAY_COUNT(Bins) / (Far - Near),
                         .MaskIndex = UINT32_MAX,
                         .BinCount = TF_ARRAY_COUNT(Bins) };
    Vector4     Planes[] = { Grid.ClipW + Grid.ClipX, Grid.ClipW - Grid.ClipX, Grid.ClipW + Grid.ClipY, Grid.ClipW - Grid.ClipY };
    for (Vector4& Plane : Planes)
    {
        Plane /= length(Plane.getXYZ());
    }
    const float DepthScale = length(Grid.ClipW.getXYZ());
    Candidates.Clear();
    Indices.Clear();
    Spheres.Clear();
    for (uint32_t Index : Active)
    {
        const hz::RenderLight& Light = Lights.Data[Index];
        if (Index == MainDirectional || !Light.ready || Light.Type == hz::LightType::Dome || Light.Type == hz::LightType::Mesh)
        {
            continue;
        }
        if (Light.Type == hz::LightType::Directional || !(Light.range > 0))
        {
            Indices.PushBack(Index);
            continue;
        }
        const Vector4 Center(Light.transform.getCol3().getXYZ(), 1);
        const float   Depth = dot(Grid.ClipW, Center);
        const float   Radius = Light.range;
        if (Depth + Radius * DepthScale < Near || Depth - Radius * DepthScale > Far || dot(Planes[0], Center) < -Radius ||
            dot(Planes[1], Center) < -Radius || dot(Planes[2], Center) < -Radius || dot(Planes[3], Center) < -Radius)
        {
            continue;
        }
        Candidates.PushBack({ .Depth = Depth, .Index = Index });
    }
    Grid.GlobalCount = Indices.Size();
    Grid.FiniteCount = Candidates.Size();
    Output.Words = Grid.Words = (Grid.FiniteCount + 31) / 32;
    Output.TilesX = Grid.TilesX;
    Output.TilesY = (Height + LIGHT_TILE_SIZE - 1) / LIGHT_TILE_SIZE;
    if (Grid.Words)
    {
        if (!Pipeline.IsValid() || !ReserveBuffer(Context, Output.Mask, uint64_t(Grid.TilesX) * Output.TilesY * Grid.Words * 4, true))
        {
            return false;
        }
        Grid.MaskIndex = Output.Mask.GetSrvIndex();
        qsort(Candidates.GetData(), Candidates.Size(), sizeof(Candidate), Compare);
        for (Bin& Entry : Bins)
        {
            Entry = { .First = UINT32_MAX, .Last = 0 };
        }
        for (uint32_t I = 0; I < Candidates.Size(); ++I)
        {
            const Candidate&       Sorted = Candidates[I];
            const hz::RenderLight& Light = Lights.Data[Sorted.Index];
            Indices.PushBack(Sorted.Index);
            Spheres.PushBack(Vector4(Light.transform.getCol3().getXYZ(), Light.range));
            const float    Extent = Light.range * DepthScale;
            const uint32_t Last = DepthBin(Sorted.Depth + Extent, Grid);
            for (uint32_t Z = DepthBin(Sorted.Depth - Extent, Grid); Z <= Last; ++Z)
            {
                if (Bins[Z].First == UINT32_MAX)
                {
                    Bins[Z].First = I;
                }
                Bins[Z].Last = I;
            }
        }
    }
    Grid.IndexOffset = sizeof(Grid) + (Grid.Words ? sizeof(Bins) : 0);
    Grid.SphereOffset = (Grid.IndexOffset + Indices.Size() * 4 + 15) & ~15u;
    return ReserveBuffer(Context, Output.Data, Grid.SphereOffset + Spheres.Size() * sizeof(Vector4), false) &&
           Context.UpdateBuffer(Output.Data, 0, &Grid, sizeof(Grid)) &&
           (!Grid.Words || Context.UpdateBuffer(Output.Data, sizeof(Grid), Bins, sizeof(Bins))) &&
           (Indices.Empty() || Context.UpdateBuffer(Output.Data, Grid.IndexOffset, Indices.GetData(), Indices.Size() * sizeof(uint32_t))) &&
           (Spheres.Empty() || Context.UpdateBuffer(Output.Data, Grid.SphereOffset, Spheres.GetData(), Spheres.Size() * sizeof(Vector4)));
}

void ClusteredLighting::BuildMasks(hz::CommandList& Commands, uint32_t Slot) const
{
    const Frame& Input = Frames[Slot];
    Commands.BeginGpuTimestamp("BuildLightTileMasks");
    Commands.SetPipeline(Pipeline);
    Commands.SetRootConstant({ Input.Data.GetSrvIndex(), Input.Mask.GetUavIndex() });
    Commands.Dispatch((Input.TilesX + 7) / 8, (Input.TilesY + 3) / 4, Input.Words,
                      { .buffers = { &Input.Data }, .storageBuffers = { &Input.Mask } });
    Commands.EndGpuTimestamp();
}
