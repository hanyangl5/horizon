/* Copyright (c) 2026 Horizon */

#include "Scene/GpuScene.h"
#include "Core/IContainer.h"
#include "Core/IMemory.h"
#include "Profiler/IProfiler.h"

namespace hz
{
struct GpuTable
{
    GPUBuffer*              pBuffer = nullptr;
    const char*             Name = "";
    uint32_t                stride = 0;
    uint64_t                Capacity = 0;
    // Set when the buffer is new, so the next submitted frame must write the whole table.
    bool                    full = true;
    Array<BufferCopyRegion> regions;
};
struct GpuTableSource
{
    const void*       Data = nullptr;
    uint32_t          Count = 0;
    Span<RenderRange> ranges;
    bool              whole = false;
};
struct GpuSceneFrame
{
    GPUBuffer    staging;
    uint64_t     Capacity = 0;
    SubmitHandle submission;
};

template<typename T>
static GpuTableSource source(Span<T> values, Span<RenderRange> ranges, bool whole = false)
{
    return { .Data = values.Data, .Count = values.Count, .ranges = ranges, .whole = whole };
}

static uint64_t growCapacity(uint64_t Capacity, uint64_t Size)
{
    Capacity = Size > Capacity * 2 ? Size : Capacity * 2;
    return (Capacity + 255) & ~UINT64_C(255);
}

constexpr uint32_t kGpuTableCount = 16;

class GpuSceneData
{
public:
    GpuSceneData(RenderScene& scene, AssetManager& assetManager, RenderContext& context, const GpuSceneDesc& desc):
        scene(scene), assetManager(assetManager), context(context)
    {
        ASSERT(desc.frameCount);
        frames.Resize(desc.frameCount);
        GPUBuffer* const  pBuffers[] = { &buffers.primitives,        &buffers.instances,        &buffers.meshBatches,
                                         &buffers.instanceIndices,   &buffers.activePrimitives, &buffers.activeInstances,
                                         &buffers.activeMeshBatches, &buffers.lights,           &buffers.cameras,
                                         &buffers.activeLights,      &buffers.activeCameras,    &buffers.meshes,
                                         &buffers.vertexStreams,     &buffers.submeshes,        &buffers.materials,
                                         &buffers.textures };
        const uint32_t    strides[] = { sizeof(RenderPrimitive),
                                        sizeof(RenderInstance),
                                        sizeof(RenderMeshBatch),
                                        4,
                                        4,
                                        4,
                                        4,
                                        sizeof(RenderLight),
                                        sizeof(RenderCamera),
                                        4,
                                        4,
                                        sizeof(GpuMeshData),
                                        sizeof(GpuVertexStream),
                                        sizeof(GpuSubmeshData),
                                        sizeof(GpuMaterialData),
                                        sizeof(GpuTextureData) };
        const char* const names[] = { "GpuScene.Primitives",        "GpuScene.Instances",        "GpuScene.MeshBatches",
                                      "GpuScene.InstanceIndices",   "GpuScene.ActivePrimitives", "GpuScene.ActiveInstances",
                                      "GpuScene.ActiveMeshBatches", "GpuScene.Lights",           "GpuScene.Cameras",
                                      "GpuScene.ActiveLights",      "GpuScene.ActiveCameras",    "GpuScene.Meshes",
                                      "GpuScene.VertexStreams",     "GpuScene.Submeshes",        "GpuScene.Materials",
                                      "GpuScene.Textures" };
        static_assert(TF_ARRAY_COUNT(pBuffers) == kGpuTableCount && TF_ARRAY_COUNT(strides) == kGpuTableCount &&
                      TF_ARRAY_COUNT(names) == kGpuTableCount);
        for (uint32_t i = 0; i < kGpuTableCount; ++i)
        {
            tables[i] = { .pBuffer = pBuffers[i], .Name = names[i], .stride = strides[i] };
        }
    }
    ~GpuSceneData()
    {
        ASSERT(!open);
        for (GpuTable& table : tables)
        {
            context.DeferRelease(std::move(*table.pBuffer));
        }
        for (GpuSceneFrame& frame : frames)
        {
            context.DeferRelease(std::move(frame.staging));
        }
    }

    bool BeginFrame()
    {
        PROFILER_SET_CPU_SCOPE_AUTO_NAMED("GpuScene::BeginFrame");
        ASSERT(!open);
        current = UINT32_MAX;
        for (uint32_t i = 0; i < frames.Size() && current == UINT32_MAX; ++i)
        {
            const uint32_t candidate = (next + i) % frames.Size();
            if (!frames[candidate].submission.IsValid() || context.IsComplete(frames[candidate].submission))
            {
                current = candidate;
            }
        }
        if (current == UINT32_MAX)
        {
            return false;
        }
        scene.Update();
        assets = assetManager.GetGpuTables();
        const RenderSceneUploads uploads = scene.GetPendingUploads();
        const bool               assetsChanged = assets.revision != uploadedRevision;
        const GpuTableSource     sources[] = {
            source(scene.GetPrimitives(), uploads.primitives),
            source(scene.GetInstances(), uploads.instances),
            source(scene.GetMeshBatches(), uploads.meshBatches),
            source(scene.GetInstanceIndices(), uploads.instanceIndices),
            source(scene.GetActivePrimitives(), {}, uploads.activePrimitives),
            source(scene.GetActiveInstances(), {}, uploads.activeInstances),
            source(scene.GetActiveMeshBatches(), {}, uploads.activeMeshBatches),
            source(scene.GetLights(), uploads.lights),
            source(scene.GetCameras(), uploads.cameras),
            source(scene.GetActiveLights(), {}, uploads.activeLights),
            source(scene.GetActiveCameras(), {}, uploads.activeCameras),
            source(assets.meshes, {}, assetsChanged),
            source(assets.vertexStreams, {}, assetsChanged),
            source(assets.submeshes, {}, assetsChanged),
            source(assets.materials, {}, assetsChanged),
            source(assets.textures, {}, assetsChanged),
        };
        uint64_t staged = 0;
        for (uint32_t i = 0; i < kGpuTableCount; ++i)
        {
            GpuTable&             table = tables[i];
            const GpuTableSource& input = sources[i];
            const uint64_t        Size = (uint64_t)input.Count * table.stride;
            table.regions.Clear();
            if (Size > table.Capacity)
            {
                table.Capacity = growCapacity(table.Capacity, Size);
                context.DeferRelease(std::move(*table.pBuffer));
                *table.pBuffer = context.CreateBuffer({ .Size = table.Capacity,
                                                        .ElementCount = (uint32_t)(table.Capacity / 4),
                                                        .Name = table.Name,
                                                        .usage = MemoryUsageGpuOnly,
                                                        .StartState = ResourceStateShaderResource,
                                                        .Descriptors = DescriptorTypeBufferRaw });
                ASSERT(table.pBuffer->IsValid());
                table.full = true;
            }
            if (!Size)
            {
                continue;
            }
            if (table.full || input.whole)
            {
                table.regions.PushBack({ .srcOffset = staged, .dstOffset = 0, .Size = Size });
                staged += Size;
                continue;
            }
            for (RenderRange range : input.ranges)
            {
                ASSERT(range.first <= input.Count && range.Count <= input.Count - range.first);
                const uint64_t bytes = (uint64_t)range.Count * table.stride;
                table.regions.PushBack({ .srcOffset = staged, .dstOffset = (uint64_t)range.first * table.stride, .Size = bytes });
                staged += bytes;
            }
        }
        GpuSceneFrame& frame = frames[current];
        {
            PROFILER_SET_CPU_SCOPE_AUTO_NAMED("GpuScene::stage");
            if (staged > frame.Capacity)
            {
                frame.Capacity = growCapacity(frame.Capacity, staged);
                context.DeferRelease(std::move(frame.staging));
                frame.staging = context.CreateBuffer({ .Size = frame.Capacity,
                                                       .Name = "GpuScene.Staging",
                                                       .usage = MemoryUsageCpuToGpu,
                                                       .StartState = ResourceStateGenericRead,
                                                       .Flags = BufferCreationFlagPersistentMapBit });
                ASSERT(frame.staging.IsValid());
            }
            for (uint32_t i = 0; i < kGpuTableCount; ++i)
            {
                for (const BufferCopyRegion& region : tables[i].regions)
                {
                    const bool written = context.UpdateBuffer(frame.staging, region.srcOffset,
                                                              (const uint8_t*)sources[i].Data + region.dstOffset, region.Size);
                    ASSERT(written);
                }
            }
        }
        open = true;
        recorded = false;
        return true;
    }

    RenderScene&         scene;
    AssetManager&        assetManager;
    RenderContext&       context;
    GpuSceneBuffers      buffers;
    GpuTable             tables[kGpuTableCount];
    Array<GpuSceneFrame> frames;
    GpuAssetTables       assets;
    uint64_t             uploadedRevision = 0;
    uint32_t             current = UINT32_MAX;
    uint32_t             next = 0;
    bool                 open = false;
    bool                 recorded = false;
};

GpuScene::GpuScene(RenderScene& scene, AssetManager& assetManager, RenderContext& context, const GpuSceneDesc& desc):
    Data(MemNew(GpuSceneData, scene, assetManager, context, desc))
{
}
GpuScene::~GpuScene() { MemDelete(Data); }

void GpuScene::Reserve(uint32_t primitiveCount, uint32_t meshBatchCount, uint32_t lightCount, uint32_t cameraCount)
{
    ASSERT(!Data->open);
    Data->scene.Reserve(primitiveCount, meshBatchCount, lightCount, cameraCount);
    for (GpuTable& table : Data->tables)
    {
        table.regions.Reserve(primitiveCount > meshBatchCount ? primitiveCount : meshBatchCount);
    }
}

bool GpuScene::BeginFrame()
{
    if (!Data->BeginFrame())
    {
        return false;
    }
    Data->scene.SetFrameLocked(true);
    return true;
}

void GpuScene::Upload(CommandList& commands)
{
    PROFILER_SET_CPU_SCOPE_AUTO_NAMED("GpuScene::Upload");
    ASSERT(Data->open && !Data->recorded);
    const GpuSceneFrame& frame = Data->frames[Data->current];
    for (const GpuTable& table : Data->tables)
    {
        if (!table.regions.Empty())
        {
            commands.CopyBufferRegions(*table.pBuffer, frame.staging, { table.regions.GetData(), table.regions.Size() });
        }
    }
    Data->recorded = true;
}

void GpuScene::EndFrame(SubmitHandle submission)
{
    PROFILER_SET_CPU_SCOPE_AUTO_NAMED("GpuScene::EndFrame");
    ASSERT(Data->open && Data->recorded && submission.IsValid());
    Data->frames[Data->current].submission = submission;
    Data->scene.SetFrameLocked(false);
    Data->scene.AcknowledgeUploads();
    Data->scene.CommitHistory();
    Data->uploadedRevision = Data->assets.revision;
    for (GpuTable& table : Data->tables)
    {
        table.full = false;
    }
    Data->next = (Data->current + 1) % Data->frames.Size();
    Data->open = false;
}

void GpuScene::CancelFrame()
{
    ASSERT(Data->open);
    Data->scene.SetFrameLocked(false);
    Data->open = false;
}

const GpuSceneBuffers& GpuScene::GetBuffers() const { return Data->buffers; }
const GpuAssetTables&  GpuScene::GetAssetTables() const { return Data->assets; }
} // namespace hz
