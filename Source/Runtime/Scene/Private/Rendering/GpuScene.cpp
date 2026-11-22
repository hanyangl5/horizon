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
static GpuTableSource MakeGpuTableSource(Span<T> Values, Span<RenderRange> Ranges, bool Whole = false)
{
    return { .Data = Values.Data, .Count = Values.Count, .ranges = Ranges, .whole = Whole };
}

static uint64_t GrowCapacity(uint64_t Capacity, uint64_t Size)
{
    Capacity = Size > Capacity * 2 ? Size : Capacity * 2;
    return (Capacity + 255) & ~UINT64_C(255);
}

constexpr uint32_t kGpuTableCount = 16;

class GpuSceneData
{
public:
    GpuSceneData(RenderScene& Scene, AssetManager& AssetManagerValue, RenderContext& Context, const GpuSceneDesc& Desc):
        scene(Scene), assetManager(AssetManagerValue), context(Context)
    {
        ASSERT(Desc.frameCount);
        frames.Resize(Desc.frameCount);
        GPUBuffer* const  Buffers[] = { &buffers.primitives,        &buffers.instances,        &buffers.meshBatches,
                                        &buffers.instanceIndices,   &buffers.activePrimitives, &buffers.activeInstances,
                                        &buffers.activeMeshBatches, &buffers.lights,           &buffers.cameras,
                                        &buffers.activeLights,      &buffers.activeCameras,    &buffers.meshes,
                                        &buffers.vertexStreams,     &buffers.submeshes,        &buffers.materials,
                                        &buffers.textures };
        const uint32_t    Strides[] = { sizeof(RenderPrimitive),
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
        const char* const Names[] = { "GpuScene.Primitives",        "GpuScene.Instances",        "GpuScene.MeshBatches",
                                      "GpuScene.InstanceIndices",   "GpuScene.ActivePrimitives", "GpuScene.ActiveInstances",
                                      "GpuScene.ActiveMeshBatches", "GpuScene.Lights",           "GpuScene.Cameras",
                                      "GpuScene.ActiveLights",      "GpuScene.ActiveCameras",    "GpuScene.Meshes",
                                      "GpuScene.VertexStreams",     "GpuScene.Submeshes",        "GpuScene.Materials",
                                      "GpuScene.Textures" };
        static_assert(TF_ARRAY_COUNT(Buffers) == kGpuTableCount && TF_ARRAY_COUNT(Strides) == kGpuTableCount &&
                      TF_ARRAY_COUNT(Names) == kGpuTableCount);
        for (uint32_t I = 0; I < kGpuTableCount; ++I)
        {
            tables[I] = { .pBuffer = Buffers[I], .Name = Names[I], .stride = Strides[I] };
        }
    }
    ~GpuSceneData()
    {
        ASSERT(!open);
        for (GpuTable& Table : tables)
        {
            context.DeferRelease(std::move(*Table.pBuffer));
        }
        for (GpuSceneFrame& FrameValue : frames)
        {
            context.DeferRelease(std::move(FrameValue.staging));
        }
    }

    bool BeginFrame()
    {
        PROFILER_SET_CPU_SCOPE_AUTO_NAMED("GpuScene::BeginFrame");
        ASSERT(!open);
        current = UINT32_MAX;
        for (uint32_t I = 0; I < frames.Size() && current == UINT32_MAX; ++I)
        {
            const uint32_t Candidate = (next + I) % frames.Size();
            if (!frames[Candidate].submission.IsValid() || context.IsComplete(frames[Candidate].submission))
            {
                current = Candidate;
            }
        }
        if (current == UINT32_MAX)
        {
            return false;
        }
        scene.Update();
        assets = assetManager.GetGpuTables();
        const RenderSceneUploads Uploads = scene.GetPendingUploads();
        const bool               AssetsChanged = assets.revision != uploadedRevision;
        const GpuTableSource     Sources[] = {
            MakeGpuTableSource(scene.GetPrimitives(), Uploads.primitives),
            MakeGpuTableSource(scene.GetInstances(), Uploads.instances),
            MakeGpuTableSource(scene.GetMeshBatches(), Uploads.meshBatches),
            MakeGpuTableSource(scene.GetInstanceIndices(), Uploads.instanceIndices),
            MakeGpuTableSource(scene.GetActivePrimitives(), {}, Uploads.activePrimitives),
            MakeGpuTableSource(scene.GetActiveInstances(), {}, Uploads.activeInstances),
            MakeGpuTableSource(scene.GetActiveMeshBatches(), {}, Uploads.activeMeshBatches),
            MakeGpuTableSource(scene.GetLights(), Uploads.lights),
            MakeGpuTableSource(scene.GetCameras(), Uploads.cameras),
            MakeGpuTableSource(scene.GetActiveLights(), {}, Uploads.activeLights),
            MakeGpuTableSource(scene.GetActiveCameras(), {}, Uploads.activeCameras),
            MakeGpuTableSource(assets.meshes, {}, AssetsChanged),
            MakeGpuTableSource(assets.vertexStreams, {}, AssetsChanged),
            MakeGpuTableSource(assets.submeshes, {}, AssetsChanged),
            MakeGpuTableSource(assets.materials, {}, AssetsChanged),
            MakeGpuTableSource(assets.textures, {}, AssetsChanged),
        };
        uint64_t Staged = 0;
        for (uint32_t I = 0; I < kGpuTableCount; ++I)
        {
            GpuTable&             Table = tables[I];
            const GpuTableSource& Input = Sources[I];
            const uint64_t        Size = (uint64_t)Input.Count * Table.stride;
            Table.regions.Clear();
            if (Size > Table.Capacity)
            {
                Table.Capacity = GrowCapacity(Table.Capacity, Size);
                context.DeferRelease(std::move(*Table.pBuffer));
                *Table.pBuffer = context.CreateBuffer({ .Size = Table.Capacity,
                                                        .ElementCount = (uint32_t)(Table.Capacity / 4),
                                                        .Name = Table.Name,
                                                        .usage = RESOURCE_MEMORY_USAGE_GPU_ONLY,
                                                        .StartState = RESOURCE_STATE_SHADER_RESOURCE,
                                                        .Descriptors = DESCRIPTOR_TYPE_BUFFER_RAW });
                ASSERT(Table.pBuffer->IsValid());
                Table.full = true;
            }
            if (!Size)
            {
                continue;
            }
            if (Table.full || Input.whole)
            {
                Table.regions.PushBack({ .srcOffset = Staged, .dstOffset = 0, .Size = Size });
                Staged += Size;
                continue;
            }
            for (RenderRange Range : Input.ranges)
            {
                ASSERT(Range.first <= Input.Count && Range.Count <= Input.Count - Range.first);
                const uint64_t Bytes = (uint64_t)Range.Count * Table.stride;
                Table.regions.PushBack({ .srcOffset = Staged, .dstOffset = (uint64_t)Range.first * Table.stride, .Size = Bytes });
                Staged += Bytes;
            }
        }
        GpuSceneFrame& FrameValue = frames[current];
        {
            PROFILER_SET_CPU_SCOPE_AUTO_NAMED("GpuScene::stage");
            if (Staged > FrameValue.Capacity)
            {
                FrameValue.Capacity = GrowCapacity(FrameValue.Capacity, Staged);
                context.DeferRelease(std::move(FrameValue.staging));
                FrameValue.staging = context.CreateBuffer({ .Size = FrameValue.Capacity,
                                                            .Name = "GpuScene.Staging",
                                                            .usage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU,
                                                            .StartState = RESOURCE_STATE_GENERIC_READ,
                                                            .Flags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT });
                ASSERT(FrameValue.staging.IsValid());
            }
            for (uint32_t I = 0; I < kGpuTableCount; ++I)
            {
                for (const BufferCopyRegion& Region : tables[I].regions)
                {
                    const bool Written = context.UpdateBuffer(FrameValue.staging, Region.srcOffset,
                                                              (const uint8_t*)Sources[I].Data + Region.dstOffset, Region.Size);
                    ASSERT(Written);
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

GpuScene::GpuScene(RenderScene& Scene, AssetManager& AssetManagerValue, RenderContext& Context, const GpuSceneDesc& Desc):
    Data(MemNew(GpuSceneData, Scene, AssetManagerValue, Context, Desc))
{
}
GpuScene::~GpuScene() { MemDelete(Data); }

void GpuScene::Reserve(uint32_t PrimitiveCount, uint32_t MeshBatchCount, uint32_t LightCount, uint32_t CameraCount)
{
    ASSERT(!Data->open);
    Data->scene.Reserve(PrimitiveCount, MeshBatchCount, LightCount, CameraCount);
    for (GpuTable& Table : Data->tables)
    {
        Table.regions.Reserve(PrimitiveCount > MeshBatchCount ? PrimitiveCount : MeshBatchCount);
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

void GpuScene::Upload(CommandList& Commands)
{
    PROFILER_SET_CPU_SCOPE_AUTO_NAMED("GpuScene::Upload");
    ASSERT(Data->open && !Data->recorded);
    const GpuSceneFrame& FrameValue = Data->frames[Data->current];
    for (const GpuTable& Table : Data->tables)
    {
        if (!Table.regions.Empty())
        {
            Commands.CopyBufferRegions(*Table.pBuffer, FrameValue.staging, { Table.regions.GetData(), Table.regions.Size() });
        }
    }
    Data->recorded = true;
}

void GpuScene::EndFrame(SubmitHandle Submission)
{
    PROFILER_SET_CPU_SCOPE_AUTO_NAMED("GpuScene::EndFrame");
    ASSERT(Data->open && Data->recorded && Submission.IsValid());
    Data->frames[Data->current].submission = Submission;
    Data->scene.SetFrameLocked(false);
    Data->scene.AcknowledgeUploads();
    Data->scene.CommitHistory();
    Data->uploadedRevision = Data->assets.revision;
    for (GpuTable& Table : Data->tables)
    {
        Table.full = false;
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
