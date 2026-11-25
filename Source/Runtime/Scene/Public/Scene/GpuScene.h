/* Copyright (c) 2026 Horizon */
#pragma once

#include "Scene/RenderScene.h"
#include "Graphics/RenderContext.h"

namespace hz
{
// Color.rgb is illuminance for directional lights, peak intensity for punctual lights and luminance for rectangles.
// Color.a stores the rectangle area in square meters.
struct GpuLightData
{
    Vector4           PositionAndInvRadius = Vector4(0.0f);
    Vector4           DirectionAndType = Vector4(0.0f);
    Vector4           Color = Vector4(0.0f);
    Vector4           HalfAxisX = Vector4(0.0f);
    Vector4           HalfAxisY = Vector4(0.0f);
    float2            SpotAngles = float2(0.0f);
    float             SourceRadius = 0;
    float             SoftSourceRadius = 0;
    float             SourceLength = 0;
    RenderLightID     Id;
    GpuTextureID      Texture;
};

static_assert(sizeof(GpuLightData) == 128);

Vector3 ResolveLightColor(const RenderLight& Light);

struct GpuSceneBuffers
{
    GPUBuffer primitives;
    GPUBuffer instances;
    GPUBuffer meshBatches;
    GPUBuffer instanceIndices;
    GPUBuffer activePrimitives;
    GPUBuffer activeInstances;
    GPUBuffer activeMeshBatches;
    // Local lights use scene slot indices; directional lights are packed contiguously.
    GPUBuffer localLights;
    GPUBuffer cameras;
    GPUBuffer activeLocalLights;
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

    void                   Reserve(uint32_t primitiveCount, uint32_t meshBatchCount, uint32_t lightCount = 0, uint32_t cameraCount = 0);
    // Updates the scene and stages its changes. Returns false without waiting when every frame is still in GPU use.
    // Do not Update the scene or Reset history until EndFrame or CancelFrame.
    bool                   BeginFrame(bool ReserveViewLightIndices = false);
    // Optional view indices follow the scene-order indices in activeLocalLights. Reserve them in BeginFrame.
    void                   Upload(CommandList& commands, Span<uint32_t> ViewLightIndices = {});
    // Marks the staged changes as uploaded and advances render history.
    void                   EndFrame(SubmitHandle submission);
    // Keeps the changes pending for the next frame.
    void                   CancelFrame();
    // Buffers may be replaced by BeginFrame when tables grow.
    const GpuSceneBuffers& GetBuffers() const;
    uint32_t               GetActiveLocalLightCount() const;
    // The asset tables uploaded by the current frame.
    const GpuAssetTables&  GetAssetTables() const;

private:
    GpuSceneData* Data = nullptr;
};
} // namespace hz
