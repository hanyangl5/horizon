/* Copyright (c) 2026 Horizon */
#pragma once

#include "Scene/RenderScene.h"
#include "Graphics/RenderContext.h"

namespace hz
{
struct GpuSceneBuffers
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
struct GpuSceneDesc
{
    uint32_t frameCount = 3;
};

class GpuSceneData;

// One persistent GPU copy of RenderScene and the asset tables, updated through per-frame staging on the graphics queue.
class GpuScene
{
public:
    // Scene, asset manager and context must outlive this object.
    GpuScene(RenderScene& scene, AssetManager& assetManager, RenderContext& context, const GpuSceneDesc& desc = {});
    ~GpuScene();
    GpuScene(const GpuScene&) = delete;
    GpuScene& operator=(const GpuScene&) = delete;

    void                   reserve(uint32_t primitiveCount, uint32_t meshBatchCount, uint32_t lightCount = 0, uint32_t cameraCount = 0);
    // Updates the scene and stages its changes. Returns false without waiting when every frame is still in GPU use.
    // Do not update the scene or reset history until endFrame or cancelFrame.
    bool                   beginFrame();
    // Record before any pass that reads the buffers.
    void                   recordUpload(CommandList& commands);
    // Marks the staged changes as uploaded and advances render history.
    void                   endFrame(SubmitHandle submission);
    // Keeps the changes pending for the next frame.
    void                   cancelFrame();
    // Buffers may be replaced by beginFrame when tables grow.
    const GpuSceneBuffers& getBuffers() const;
    // The asset tables uploaded by the current frame.
    const GpuAssetTables&  getAssetTables() const;

private:
    GpuSceneData* pData = nullptr;
};
} // namespace hz
