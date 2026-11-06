/* Copyright (c) 2026 Horizon */

#include "Scene/RenderFrame.h"
#include "Core/IContainer.h"
#include "Core/IMemory.h"

namespace hz
{
template<typename T>
static void copyValues(T* pTarget, const T* pSource, uint32_t count)
{
    if constexpr (__is_trivially_copyable(T))
        memcpy(pTarget, pSource, sizeof(T) * count);
    else
        for (uint32_t i = 0; i < count; ++i)
            pTarget[i] = pSource[i];
}

template<typename T>
static void syncArray(Array<T>& target, Span<T> source, Span<RenderRange> ranges, bool full)
{
    target.resize(source.count);
    if (full && source.count)
        copyValues(target.data(), source.pData, source.count);
    else
        for (RenderRange range : ranges)
        {
            ASSERT(range.first <= source.count && range.count <= source.count - range.first);
            if (range.count)
                copyValues(target.data() + range.first, source.pData + range.first, range.count);
        }
}

template<typename T>
static void upload(RenderContext& context, GPUBuffer& buffer, uint64_t& capacity, Span<T> source, Span<RenderRange> ranges, bool full)
{
    const uint64_t size = sizeof(T) * (uint64_t)source.count;
    if (!size)
        return;
    if (size > capacity)
    {
        capacity = size > capacity * 2 ? size : capacity * 2;
        capacity = (capacity + 255) & ~UINT64_C(255);
        buffer = context.createBuffer({ .size = capacity,
                                        .elementCount = (uint32_t)(capacity / 4),
                                        .usage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU,
                                        .startState = RESOURCE_STATE_SHADER_RESOURCE,
                                        .descriptors = DESCRIPTOR_TYPE_BUFFER_RAW,
                                        .flags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT });
        ASSERT(buffer.isValid());
        full = true;
    }
    if (full)
    {
        const bool updated = context.updateBuffer(buffer, 0, source.pData, size);
        ASSERT(updated);
    }
    else
        for (RenderRange range : ranges)
            if (range.count)
            {
                const bool updated = context.updateBuffer(buffer, sizeof(T) * (uint64_t)range.first, source.pData + range.first,
                                                          sizeof(T) * (uint64_t)range.count);
                ASSERT(updated);
            }
}

struct RenderFrameData
{
    Array<RenderObject>   objects;
    Array<RenderInstance> instances;
    Array<RenderDraw>     draws;
    Array<uint32_t>       instanceIndices;
    Array<uint32_t>       activeObjects;
    Array<uint32_t>       activeInstances;
    Array<uint32_t>       activeDraws;
    Array<RenderLight>    lights;
    Array<RenderCamera>   cameras;
    Array<uint32_t>       activeLights;
    Array<uint32_t>       activeCameras;
    RenderFrameBuffers    buffers;
    uint64_t              capacities[16] = {};
    AssetSnapshot         assets;
    SubmitHandle          submission;
    uint64_t              sceneRevision = 0;
    uint64_t              assetRevision = 0;
    uint64_t              revision = 0;
    uint64_t              historyEpoch = 0;
    bool                  leased = false;
};

class RenderFramePoolData
{
public:
    RenderFramePoolData(RenderScene& scene, RenderContext& context, const RenderFramePoolDesc& desc): scene(scene), context(context)
    {
        ASSERT(desc.frameCount);
        frames.resize(desc.frameCount);
    }
    ~RenderFramePoolData()
    {
        ASSERT(!cpuLease);
        for (RenderFrameData& frame : frames)
        {
            if (frame.submission.isValid())
                context.wait(frame.submission);
            frame.assets = {};
        }
    }
    void collect()
    {
        for (RenderFrameData& frame : frames)
            if (frame.submission.isValid() && context.isComplete(frame.submission))
            {
                frame.submission = {};
                frame.assets = {};
            }
    }
    void discard(uint32_t index)
    {
        ASSERT(cpuLease && frames[index].leased);
        frames[index].assets = {};
        frames[index].leased = false;
        cpuLease = false;
    }
    uint32_t capture()
    {
        ASSERT(!cpuLease);
        collect();
        uint32_t index = UINT32_MAX;
        for (uint32_t i = 0; i < frames.size(); ++i)
        {
            const uint32_t candidate = (next + i) % frames.size();
            if (!frames[candidate].submission.isValid())
            {
                index = candidate;
                break;
            }
        }
        if (index == UINT32_MAX)
            return index;
        scene.update();
        RenderFrameData& frame = frames[index];
        frame.assets = scene.captureAssets();
        ASSERT(frame.assets.isValid());
        const RenderSceneSync changes = scene.getChangesSince(frame.sceneRevision);
        syncArray(frame.objects, scene.getObjects(), changes.objects, changes.full);
        syncArray(frame.instances, scene.getInstances(), changes.instances, changes.full);
        syncArray(frame.draws, scene.getDraws(), changes.draws, changes.full);
        syncArray(frame.instanceIndices, scene.getInstanceIndices(), changes.instanceIndices, changes.full);
        syncArray(frame.activeObjects, scene.getActiveObjects(), {}, changes.full || changes.activeObjects);
        syncArray(frame.activeInstances, scene.getActiveInstances(), {}, changes.full || changes.activeInstances);
        syncArray(frame.activeDraws, scene.getActiveDraws(), {}, changes.full || changes.activeDraws);
        syncArray(frame.lights, scene.getLights(), changes.lights, changes.full);
        syncArray(frame.cameras, scene.getCameras(), changes.cameras, changes.full);
        syncArray(frame.activeLights, scene.getActiveLights(), {}, changes.full || changes.activeLights);
        syncArray(frame.activeCameras, scene.getActiveCameras(), {}, changes.full || changes.activeCameras);
        upload<RenderObject>(context, frame.buffers.objects, frame.capacities[0], { frame.objects.data(), frame.objects.size() },
                             changes.objects, changes.full);
        upload<RenderInstance>(context, frame.buffers.instances, frame.capacities[1], { frame.instances.data(), frame.instances.size() },
                               changes.instances, changes.full);
        upload<RenderDraw>(context, frame.buffers.draws, frame.capacities[2], { frame.draws.data(), frame.draws.size() }, changes.draws,
                           changes.full);
        upload<uint32_t>(context, frame.buffers.instanceIndices, frame.capacities[3],
                         { frame.instanceIndices.data(), frame.instanceIndices.size() }, changes.instanceIndices, changes.full);
        upload<uint32_t>(context, frame.buffers.activeObjects, frame.capacities[4],
                         { frame.activeObjects.data(), frame.activeObjects.size() }, {}, changes.full || changes.activeObjects);
        upload<uint32_t>(context, frame.buffers.activeInstances, frame.capacities[5],
                         { frame.activeInstances.data(), frame.activeInstances.size() }, {}, changes.full || changes.activeInstances);
        upload<uint32_t>(context, frame.buffers.activeDraws, frame.capacities[6], { frame.activeDraws.data(), frame.activeDraws.size() },
                         {}, changes.full || changes.activeDraws);
        const GpuAssetTableSnapshot tables = frame.assets.getGpuTables();
        const bool                  assetChanged = frame.assetRevision != frame.assets.getRevision();
        upload(context, frame.buffers.meshes, frame.capacities[7], tables.meshes, {}, assetChanged);
        upload(context, frame.buffers.vertexStreams, frame.capacities[8], tables.vertexStreams, {}, assetChanged);
        upload(context, frame.buffers.submeshes, frame.capacities[9], tables.submeshes, {}, assetChanged);
        upload(context, frame.buffers.materials, frame.capacities[10], tables.materials, {}, assetChanged);
        upload(context, frame.buffers.textures, frame.capacities[11], tables.textures, {}, assetChanged);
        upload<RenderLight>(context, frame.buffers.lights, frame.capacities[12], { frame.lights.data(), frame.lights.size() },
                            changes.lights, changes.full);
        upload<RenderCamera>(context, frame.buffers.cameras, frame.capacities[13], { frame.cameras.data(), frame.cameras.size() },
                             changes.cameras, changes.full);
        upload<uint32_t>(context, frame.buffers.activeLights, frame.capacities[14],
                         { frame.activeLights.data(), frame.activeLights.size() }, {}, changes.full || changes.activeLights);
        upload<uint32_t>(context, frame.buffers.activeCameras, frame.capacities[15],
                         { frame.activeCameras.data(), frame.activeCameras.size() }, {}, changes.full || changes.activeCameras);
        if (lastSceneRevision != scene.getRevision() || lastAssetRevision != frame.assets.getRevision())
        {
            ++revision;
            lastSceneRevision = scene.getRevision();
            lastAssetRevision = frame.assets.getRevision();
        }
        frame.sceneRevision = scene.getRevision();
        frame.assetRevision = frame.assets.getRevision();
        frame.revision = revision;
        frame.leased = true;
        cpuLease = true;
        next = (index + 1) % frames.size();
        return index;
    }

    RenderScene&           scene;
    RenderContext&         context;
    Array<RenderFrameData> frames;
    uint32_t               next = 0;
    uint64_t               revision = 0;
    uint64_t               lastSceneRevision = 0;
    uint64_t               lastAssetRevision = 0;
    bool                   cpuLease = false;
};

RenderFramePool::RenderFramePool(RenderScene& scene, RenderContext& context, const RenderFramePoolDesc& desc):
    pData(tf_new(RenderFramePoolData, scene, context, desc))
{
}
RenderFramePool::~RenderFramePool() { tf_delete(pData); }
void RenderFramePool::reserve(uint32_t objectCount, uint32_t drawCount, uint32_t lightCount, uint32_t cameraCount)
{
    ASSERT(!pData->cpuLease);
    pData->scene.reserve(objectCount, drawCount, lightCount, cameraCount);
    for (RenderFrameData& frame : pData->frames)
    {
        frame.objects.reserve(objectCount);
        frame.instances.reserve(objectCount);
        frame.draws.reserve(drawCount);
        frame.instanceIndices.reserve(objectCount);
        frame.activeObjects.reserve(objectCount);
        frame.activeInstances.reserve(objectCount);
        frame.activeDraws.reserve(drawCount);
        frame.lights.reserve(lightCount);
        frame.cameras.reserve(cameraCount);
        frame.activeLights.reserve(lightCount);
        frame.activeCameras.reserve(cameraCount);
    }
}
RenderFrame RenderFramePool::capture()
{
    RenderFrame result;
    result.index = pData->capture();
    if (result.index != UINT32_MAX)
    {
        result.pData = pData;
        pData->frames[result.index].historyEpoch = pData->scene.getHistoryEpoch();
    }
    return result;
}
void RenderFramePool::retire(RenderFrame&& snapshot, SubmitHandle submission)
{
    ASSERT(snapshot.pData == pData && snapshot.isValid() && submission.isValid());
    RenderFrameData& frame = pData->frames[snapshot.index];
    pData->scene.commitSnapshot({ frame.objects.data(), frame.objects.size() }, { frame.instances.data(), frame.instances.size() },
                                { frame.cameras.data(), frame.cameras.size() }, frame.historyEpoch);
    frame.submission = submission;
    frame.leased = false;
    pData->cpuLease = false;
    snapshot.pData = nullptr;
    snapshot.index = UINT32_MAX;
}
void RenderFramePool::collect() { pData->collect(); }

RenderFrame::~RenderFrame()
{
    if (pData)
        pData->discard(index);
}
RenderFrame::RenderFrame(RenderFrame&& other) noexcept: pData(other.pData), index(other.index)
{
    other.pData = nullptr;
    other.index = UINT32_MAX;
}
RenderFrame& RenderFrame::operator=(RenderFrame&& other) noexcept
{
    if (this != &other)
    {
        if (pData)
            pData->discard(index);
        pData = other.pData;
        index = other.index;
        other.pData = nullptr;
        other.index = UINT32_MAX;
    }
    return *this;
}
uint64_t RenderFrame::getRevision() const
{
    ASSERT(pData);
    return pData->frames[index].revision;
}
const AssetSnapshot& RenderFrame::getAssets() const
{
    ASSERT(pData);
    return pData->frames[index].assets;
}
const RenderFrameBuffers& RenderFrame::getBuffers() const
{
    ASSERT(pData);
    return pData->frames[index].buffers;
}
Span<RenderObject> RenderFrame::getObjects() const
{
    ASSERT(pData);
    const RenderFrameData& frame = pData->frames[index];
    return { frame.objects.data(), frame.objects.size() };
}
Span<RenderInstance> RenderFrame::getInstances() const
{
    ASSERT(pData);
    const RenderFrameData& frame = pData->frames[index];
    return { frame.instances.data(), frame.instances.size() };
}
Span<RenderDraw> RenderFrame::getDraws() const
{
    ASSERT(pData);
    const RenderFrameData& frame = pData->frames[index];
    return { frame.draws.data(), frame.draws.size() };
}
Span<RenderLight> RenderFrame::getLights() const
{
    ASSERT(pData);
    const RenderFrameData& frame = pData->frames[index];
    return { frame.lights.data(), frame.lights.size() };
}
Span<RenderCamera> RenderFrame::getCameras() const
{
    ASSERT(pData);
    const RenderFrameData& frame = pData->frames[index];
    return { frame.cameras.data(), frame.cameras.size() };
}
Span<uint32_t> RenderFrame::getInstanceIndices() const
{
    ASSERT(pData);
    const RenderFrameData& frame = pData->frames[index];
    return { frame.instanceIndices.data(), frame.instanceIndices.size() };
}
Span<uint32_t> RenderFrame::getActiveObjects() const
{
    ASSERT(pData);
    const RenderFrameData& frame = pData->frames[index];
    return { frame.activeObjects.data(), frame.activeObjects.size() };
}
Span<uint32_t> RenderFrame::getActiveInstances() const
{
    ASSERT(pData);
    const RenderFrameData& frame = pData->frames[index];
    return { frame.activeInstances.data(), frame.activeInstances.size() };
}
Span<uint32_t> RenderFrame::getActiveDraws() const
{
    ASSERT(pData);
    const RenderFrameData& frame = pData->frames[index];
    return { frame.activeDraws.data(), frame.activeDraws.size() };
}
Span<uint32_t> RenderFrame::getActiveLights() const
{
    ASSERT(pData);
    const RenderFrameData& frame = pData->frames[index];
    return { frame.activeLights.data(), frame.activeLights.size() };
}
Span<uint32_t> RenderFrame::getActiveCameras() const
{
    ASSERT(pData);
    const RenderFrameData& frame = pData->frames[index];
    return { frame.activeCameras.data(), frame.activeCameras.size() };
}
} // namespace hz
