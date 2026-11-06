/* Copyright (c) 2026 Horizon */
#pragma once

#include "Scene/RenderScene.h"
#include "Graphics/RenderContext.h"

namespace hz
{
struct RenderFrameBuffers
{
    GPUBuffer primitives;
    GPUBuffer instances;
    GPUBuffer meshBatches;
    GPUBuffer instanceIndices;
    GPUBuffer activePrimitives;
    GPUBuffer activeInstances;
    GPUBuffer activeMeshBatches;
    GPUBuffer lights;
    GPUBuffer cameras;
    GPUBuffer activeLights;
    GPUBuffer activeCameras;
    GPUBuffer meshes;
    GPUBuffer vertexStreams;
    GPUBuffer submeshes;
    GPUBuffer materials;
    GPUBuffer textures;
};
struct RenderFramePoolDesc
{
    uint32_t frameCount = 3;
};

class RenderFramePoolData;
class RenderFramePool;

// One move-only frame lease. All views remain immutable until discard or retirement.
class RenderFrame
{
public:
    RenderFrame() = default;
    ~RenderFrame();
    RenderFrame(RenderFrame&&) noexcept;
    RenderFrame& operator=(RenderFrame&&) noexcept;
    RenderFrame(const RenderFrame&) = delete;
    RenderFrame&              operator=(const RenderFrame&) = delete;
    bool                      isValid() const { return pData != nullptr; }
    uint64_t                  getRevision() const;
    uint32_t                  getBackingIndex() const { return index; }
    const AssetSnapshot&      getAssets() const;
    const RenderFrameBuffers& getBuffers() const;
    Span<RenderPrimitive>     getPrimitives() const;
    Span<RenderInstance>      getInstances() const;
    Span<RenderMeshBatch>     getMeshBatches() const;
    Span<RenderLight>         getLights() const;
    Span<RenderCamera>        getCameras() const;
    Span<uint32_t>            getInstanceIndices() const;
    Span<uint32_t>            getActivePrimitives() const;
    Span<uint32_t>            getActiveInstances() const;
    Span<uint32_t>            getActiveMeshBatches() const;
    Span<uint32_t>            getActiveLights() const;
    Span<uint32_t>            getActiveCameras() const;

private:
    friend class RenderFramePool;
    RenderFramePoolData* pData = nullptr;
    uint32_t             index = UINT32_MAX;
};

class RenderFramePool
{
public:
    // Scene, asset manager and context outlive the pool; destroy unsubmitted leases before the pool.
    RenderFramePool(RenderScene& scene, RenderContext& context, const RenderFramePoolDesc& desc = {});
    ~RenderFramePool();
    RenderFramePool(const RenderFramePool&) = delete;
    RenderFramePool& operator=(const RenderFramePool&) = delete;
    void             reserve(uint32_t primitiveCount, uint32_t meshBatchCount, uint32_t lightCount = 0, uint32_t cameraCount = 0);
    // Updates extraction and pins the matching asset table. One CPU lease at a time.
    // Returns invalid if every backing is still in GPU use; never waits for a fence.
    RenderFrame      capture();
    // Commit history from this exact frame; keep buffers and payloads until its fence completes.
    void             retire(RenderFrame&& frame, SubmitHandle submission);
    void             collect();

private:
    RenderFramePoolData* pData = nullptr;
};
} // namespace hz
