/* Copyright (c) 2026 Horizon */

#include "Scene/GpuScene.h"
#include "Core/IContainer.h"
#include "Core/IMemory.h"

namespace hz
{
struct GpuTable
{
    GPUBuffer*              pBuffer = nullptr;
    const char*             pName = "";
    uint32_t                stride = 0;
    uint64_t                capacity = 0;
    // Set when the buffer is new, so the next submitted frame must write the whole table.
    bool                    full = true;
    Array<BufferCopyRegion> regions;
};
struct GpuTableSource
{
    const void*       pData = nullptr;
    uint32_t          count = 0;
    Span<RenderRange> ranges;
    bool              whole = false;
};
struct GpuSceneFrame
{
    GPUBuffer    staging;
    uint64_t     capacity = 0;
    SubmitHandle submission;
};

template<typename T>
static GpuTableSource source(Span<T> values, Span<RenderRange> ranges, bool whole = false)
{
    return { .pData = values.pData, .count = values.count, .ranges = ranges, .whole = whole };
}

static uint64_t growCapacity(uint64_t capacity, uint64_t size)
{
    capacity = size > capacity * 2 ? size : capacity * 2;
    return (capacity + 255) & ~UINT64_C(255);
}

constexpr uint32_t kGpuTableCount = 16;

class GpuSceneData
{
public:
    GpuSceneData(RenderScene& scene, AssetManager& assetManager, RenderContext& context, const GpuSceneDesc& desc):
        scene(scene), assetManager(assetManager), context(context)
    {
        ASSERT(desc.frameCount);
        frames.resize(desc.frameCount);
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
                                        sizeof(GpuMeshRecord),
                                        sizeof(GpuVertexStream),
                                        sizeof(GpuSubmeshRecord),
                                        sizeof(GpuMaterialRecord),
                                        sizeof(GpuTextureRecord) };
        const char* const names[] = { "GpuScene.Primitives",        "GpuScene.Instances",        "GpuScene.MeshBatches",
                                      "GpuScene.InstanceIndices",   "GpuScene.ActivePrimitives", "GpuScene.ActiveInstances",
                                      "GpuScene.ActiveMeshBatches", "GpuScene.Lights",           "GpuScene.Cameras",
                                      "GpuScene.ActiveLights",      "GpuScene.ActiveCameras",    "GpuScene.Meshes",
                                      "GpuScene.VertexStreams",     "GpuScene.Submeshes",        "GpuScene.Materials",
                                      "GpuScene.Textures" };
        static_assert(TF_ARRAY_COUNT(pBuffers) == kGpuTableCount && TF_ARRAY_COUNT(strides) == kGpuTableCount &&
                      TF_ARRAY_COUNT(names) == kGpuTableCount);
        for (uint32_t i = 0; i < kGpuTableCount; ++i)
            tables[i] = { .pBuffer = pBuffers[i], .pName = names[i], .stride = strides[i] };
    }
    ~GpuSceneData()
    {
        ASSERT(!open);
        for (GpuTable& table : tables)
            context.deferRelease(std::move(*table.pBuffer));
        for (GpuSceneFrame& frame : frames)
            context.deferRelease(std::move(frame.staging));
    }

    bool beginFrame()
    {
        ASSERT(!open);
        current = UINT32_MAX;
        for (uint32_t i = 0; i < frames.size() && current == UINT32_MAX; ++i)
        {
            const uint32_t candidate = (next + i) % frames.size();
            if (!frames[candidate].submission.isValid() || context.isComplete(frames[candidate].submission))
                current = candidate;
        }
        if (current == UINT32_MAX)
            return false;
        scene.update();
        assets = assetManager.getGpuTables();
        const RenderSceneUploads uploads = scene.getPendingUploads();
        const bool               assetsChanged = assets.revision != uploadedRevision;
        const GpuTableSource     sources[] = {
            source(scene.getPrimitives(), uploads.primitives),
            source(scene.getInstances(), uploads.instances),
            source(scene.getMeshBatches(), uploads.meshBatches),
            source(scene.getInstanceIndices(), uploads.instanceIndices),
            source(scene.getActivePrimitives(), {}, uploads.activePrimitives),
            source(scene.getActiveInstances(), {}, uploads.activeInstances),
            source(scene.getActiveMeshBatches(), {}, uploads.activeMeshBatches),
            source(scene.getLights(), uploads.lights),
            source(scene.getCameras(), uploads.cameras),
            source(scene.getActiveLights(), {}, uploads.activeLights),
            source(scene.getActiveCameras(), {}, uploads.activeCameras),
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
            const uint64_t        size = (uint64_t)input.count * table.stride;
            table.regions.clear();
            if (size > table.capacity)
            {
                table.capacity = growCapacity(table.capacity, size);
                context.deferRelease(std::move(*table.pBuffer));
                *table.pBuffer = context.createBuffer({ .size = table.capacity,
                                                        .elementCount = (uint32_t)(table.capacity / 4),
                                                        .pName = table.pName,
                                                        .usage = RESOURCE_MEMORY_USAGE_GPU_ONLY,
                                                        .startState = RESOURCE_STATE_SHADER_RESOURCE,
                                                        .descriptors = DESCRIPTOR_TYPE_BUFFER_RAW });
                ASSERT(table.pBuffer->isValid());
                table.full = true;
            }
            if (!size)
                continue;
            if (table.full || input.whole)
            {
                table.regions.pushBack({ .srcOffset = staged, .dstOffset = 0, .size = size });
                staged += size;
                continue;
            }
            for (RenderRange range : input.ranges)
            {
                ASSERT(range.first <= input.count && range.count <= input.count - range.first);
                const uint64_t bytes = (uint64_t)range.count * table.stride;
                table.regions.pushBack({ .srcOffset = staged, .dstOffset = (uint64_t)range.first * table.stride, .size = bytes });
                staged += bytes;
            }
        }
        GpuSceneFrame& frame = frames[current];
        if (staged > frame.capacity)
        {
            frame.capacity = growCapacity(frame.capacity, staged);
            context.deferRelease(std::move(frame.staging));
            frame.staging = context.createBuffer({ .size = frame.capacity,
                                                   .pName = "GpuScene.Staging",
                                                   .usage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU,
                                                   .startState = RESOURCE_STATE_GENERIC_READ,
                                                   .flags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT });
            ASSERT(frame.staging.isValid());
        }
        for (uint32_t i = 0; i < kGpuTableCount; ++i)
            for (const BufferCopyRegion& region : tables[i].regions)
            {
                const bool written =
                    context.updateBuffer(frame.staging, region.srcOffset, (const uint8_t*)sources[i].pData + region.dstOffset, region.size);
                ASSERT(written);
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
    pData(tf_new(GpuSceneData, scene, assetManager, context, desc))
{
}
GpuScene::~GpuScene() { tf_delete(pData); }

void GpuScene::reserve(uint32_t primitiveCount, uint32_t meshBatchCount, uint32_t lightCount, uint32_t cameraCount)
{
    ASSERT(!pData->open);
    pData->scene.reserve(primitiveCount, meshBatchCount, lightCount, cameraCount);
    for (GpuTable& table : pData->tables)
        table.regions.reserve(primitiveCount > meshBatchCount ? primitiveCount : meshBatchCount);
}

bool GpuScene::beginFrame()
{
    if (!pData->beginFrame())
        return false;
    pData->scene.setFrameLocked(true);
    return true;
}

void GpuScene::recordUpload(CommandList& commands)
{
    ASSERT(pData->open && !pData->recorded);
    const GpuSceneFrame& frame = pData->frames[pData->current];
    for (const GpuTable& table : pData->tables)
        if (!table.regions.empty())
            commands.copyBufferRegions(*table.pBuffer, frame.staging, { table.regions.data(), table.regions.size() });
    pData->recorded = true;
}

void GpuScene::endFrame(SubmitHandle submission)
{
    ASSERT(pData->open && pData->recorded && submission.isValid());
    pData->frames[pData->current].submission = submission;
    pData->scene.setFrameLocked(false);
    pData->scene.acknowledgeUploads();
    pData->scene.commitHistory();
    pData->uploadedRevision = pData->assets.revision;
    for (GpuTable& table : pData->tables)
        table.full = false;
    pData->next = (pData->current + 1) % pData->frames.size();
    pData->open = false;
}

void GpuScene::cancelFrame()
{
    ASSERT(pData->open);
    pData->scene.setFrameLocked(false);
    pData->open = false;
}

const GpuSceneBuffers& GpuScene::getBuffers() const { return pData->buffers; }
const GpuAssetTables&  GpuScene::getAssetTables() const { return pData->assets; }
} // namespace hz
