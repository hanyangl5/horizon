/* Copyright (c) 2026 Horizon */
#pragma once

#include "Scene/RenderScene.h"
#include "Graphics/RenderContext.h"

namespace hz
{
struct SceneFrameBuffers
{
    GPUBuffer objects;
    GPUBuffer instances;
    GPUBuffer draws;
    GPUBuffer instanceIndices;
    GPUBuffer activeObjects;
    GPUBuffer activeInstances;
    GPUBuffer activeDraws;
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
struct SceneFramePoolDesc
{
    uint32_t frameCount = 3;
};

class SceneFramePoolData;
class SceneFramePool;

// One move-only frame lease. All views remain immutable until discard or retirement.
class SceneFrameSnapshot
{
public:
    SceneFrameSnapshot() = default;
    ~SceneFrameSnapshot();
    SceneFrameSnapshot(SceneFrameSnapshot&&) noexcept;
    SceneFrameSnapshot& operator=(SceneFrameSnapshot&&) noexcept;
    SceneFrameSnapshot(const SceneFrameSnapshot&) = delete;
    SceneFrameSnapshot&      operator=(const SceneFrameSnapshot&) = delete;
    bool                     isValid() const { return pData != nullptr; }
    uint64_t                 getRevision() const;
    uint32_t                 getBackingIndex() const { return index; }
    const AssetSnapshot&     getAssets() const;
    const SceneFrameBuffers& getBuffers() const;
    Span<RenderObject>       getObjects() const;
    Span<RenderInstance>     getInstances() const;
    Span<RenderDraw>         getDraws() const;
    Span<RenderLight>        getLights() const;
    Span<RenderCamera>       getCameras() const;
    Span<uint32_t>           getInstanceIndices() const;
    Span<uint32_t>           getActiveObjects() const;
    Span<uint32_t>           getActiveInstances() const;
    Span<uint32_t>           getActiveDraws() const;
    Span<uint32_t>           getActiveLights() const;
    Span<uint32_t>           getActiveCameras() const;

private:
    friend class SceneFramePool;
    SceneFramePoolData* pData = nullptr;
    uint32_t            index = UINT32_MAX;
};

class SceneFramePool
{
public:
    // Scene, registry and context outlive the pool; destroy unsubmitted leases before the pool.
    SceneFramePool(RenderScene& scene, RenderContext& context, const SceneFramePoolDesc& desc = {});
    ~SceneFramePool();
    SceneFramePool(const SceneFramePool&) = delete;
    SceneFramePool&    operator=(const SceneFramePool&) = delete;
    void               reserve(uint32_t objectCount, uint32_t drawCount, uint32_t lightCount = 0, uint32_t cameraCount = 0);
    // Updates extraction and pins the matching asset table. One CPU lease at a time.
    // Returns invalid if every backing is still in GPU use; never waits for a fence.
    SceneFrameSnapshot capture();
    // Commit history from this exact frame; keep buffers and payloads until its fence completes.
    void               retire(SceneFrameSnapshot&& frame, SubmitHandle submission);
    void               collect();

private:
    SceneFramePoolData* pData = nullptr;
};
} // namespace hz
