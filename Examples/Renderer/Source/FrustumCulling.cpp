#include "FrustumCulling.h"
#include "Profiler/IProfiler.h"

// Mirrors the RootConstant0 block of FrustumCulling.slang field for field.
struct CullingConstants
{
    uint32_t FrameIndex;
    uint32_t DrawsIndex;
    uint32_t InstancesIndex;
    uint32_t MaterialsIndex;
    uint32_t FiltersIndex;
    uint32_t FilterCount;
    uint32_t StatisticsIndex;
    uint32_t DrawCount;
    uint32_t CullingEnabled;
};
static_assert(sizeof(CullingConstants) == 9 * sizeof(uint32_t));
// Offset of statisticsIndex, which ClearStatistics reads on its own.
constexpr uint32_t CullingStatisticsSlot = offsetof(CullingConstants, StatisticsIndex) / sizeof(uint32_t);

FrustumCulling::FrustumCulling(hz::RenderContext& Context)
{
    Pipeline = Context.CreateComputePipeline(
        { .shaderDesc = { .stages = { { .stage = SHADER_STAGE_COMP, .pEntryPoint = "CSMain", .Name = "Renderer.FrustumCull" } },
                          .pFileName = "FrustumCulling.slang" },
          .Name = "Renderer.FrustumCull" });
    ClearPipeline =
        Context.CreateComputePipeline({ .shaderDesc = { .stages = { { .stage = SHADER_STAGE_COMP, .pEntryPoint = "ClearStatistics" } },
                                                        .pFileName = "FrustumCulling.slang" },
                                        .Name = "Renderer.ClearCullingStatistics" });
    for (hz::GPUBuffer& Buffer : Statistics)
    {
        Buffer = Context.CreateBuffer({ .Size = 16,
                                        .ElementCount = 4,
                                        .Name = "Renderer.CullingStatistics",
                                        .usage = RESOURCE_MEMORY_USAGE_GPU_ONLY,
                                        .StartState = RESOURCE_STATE_UNORDERED_ACCESS,
                                        .Descriptors = DESCRIPTOR_TYPE_RW_BUFFER_RAW });
        ASSERT(Buffer.IsValid());
    }
    ASSERT(Pipeline.IsValid() && ClearPipeline.IsValid());
}

void FrustumCulling::Execute(hz::CommandList& Commands, const hz::GPUBuffer& Frame, const hz::SceneView& View, const hz::GpuDrawList& Draws,
                             bool Culling)
{
    PROFILER_SET_CPU_SCOPE_AUTO_NAMED("FrustumCulling::Execute");
    const uint32_t       Backing = View.FrameIndex;
    const hz::GPUBuffer& Stats = Statistics[Backing];
    Commands.SetPipeline(ClearPipeline);
    Commands.SetRootConstant({ Stats.GetUavIndex() }, CullingStatisticsSlot);
    Commands.Dispatch(1, 1, 1, { .storageBuffers = { &Stats } });
    const uint32_t Count = Draws.GetCount(Backing);
    if (!Count)
    {
        return;
    }
    const hz::GpuSceneBuffers& Gpu = View.GpuScene.GetBuffers();
    const hz::GPUBuffer&       DrawData = Draws.GetDrawData(Backing);
    const hz::GPUBuffer&       Filters = Draws.GetFilters(Backing);
    const uint32_t             ListCount = Draws.GetListCount();
    Commands.BeginGpuTimestamp("GPU Frustum Culling");
    Commands.SetPipeline(Pipeline);
    const CullingConstants Binding = { .FrameIndex = Frame.GetSrvIndex(),
                                       .DrawsIndex = DrawData.GetSrvIndex(),
                                       .InstancesIndex = Gpu.instances.GetSrvIndex(),
                                       .MaterialsIndex = Gpu.materials.GetSrvIndex(),
                                       .FiltersIndex = Filters.GetSrvIndex(),
                                       .FilterCount = ListCount,
                                       .StatisticsIndex = Stats.GetUavIndex(),
                                       .DrawCount = Count,
                                       .CullingEnabled = Culling ? 1u : 0u };
    Commands.SetRootConstants(Binding);
    const hz::GPUBuffer* Outputs[hz::GpuDrawList::MaxLists + 1] = { &Stats };
    for (uint32_t List = 0; List < ListCount; ++List)
    {
        Outputs[List + 1] = &Draws.GetCommands(Backing, { List });
    }
    Commands.Dispatch(
        (Count + 63) / 64, 1, 1,
        { .buffers = { &Frame, &DrawData, &Gpu.instances, &Gpu.materials, &Filters }, .storageBuffers = { Outputs, ListCount + 1 } });
    Commands.EndGpuTimestamp();
}
