/* Copyright (c) 2026 Horizon */
#pragma once

#include "Scene/AssetManager.h"
#include "Scene/World.h"
#include "Scene/SceneComponents.h"
#include "Core/IMath.h"

namespace hz
{
struct RenderPrimitiveID
{
    uint32_t index = UINT32_MAX;
    uint32_t generation = 0;
    bool     IsValid() const { return index != UINT32_MAX && generation != 0; }
    bool     operator==(const RenderPrimitiveID&) const = default;
};
struct RenderInstanceID
{
    uint32_t index = UINT32_MAX;
    uint32_t generation = 0;
    bool     IsValid() const { return index != UINT32_MAX && generation != 0; }
    bool     operator==(const RenderInstanceID&) const = default;
};
struct RenderMeshBatchID
{
    uint32_t index = UINT32_MAX;
    uint32_t generation = 0;
    bool     IsValid() const { return index != UINT32_MAX && generation != 0; }
    bool     operator==(const RenderMeshBatchID&) const = default;
};
struct RenderLightID
{
    uint32_t index = UINT32_MAX;
    uint32_t generation = 0;
    bool     IsValid() const { return index != UINT32_MAX && generation != 0; }
    bool     operator==(const RenderLightID&) const = default;
};
struct RenderCameraID
{
    uint32_t index = UINT32_MAX;
    uint32_t generation = 0;
    bool     IsValid() const { return index != UINT32_MAX && generation != 0; }
    bool     operator==(const RenderCameraID&) const = default;
};

struct RenderBounds
{
    float minimum[3] = {};
    float maximum[3] = {};
};
struct RenderPrimitive
{
    RenderPrimitiveID id;
    GpuMeshID         mesh;
    uint32_t          instanceOffset = 0;
    // Zero while assets are unavailable; phase one has one instance when ready.
    uint32_t          instanceCount = 0;
};
struct RenderInstance
{
    RenderInstanceID  id;
    RenderPrimitiveID primitive;
    Guid              entityGuid;
    Matrix4           current = Matrix4::identity();
    Matrix4           previous = Matrix4::identity();
    RenderBounds      bounds;
};
struct RenderMeshBatch
{
    RenderMeshBatchID id;
    RenderPrimitiveID primitive;
    uint64_t          submesh = 0;
    GpuSubmeshRef     mesh;
    GpuMaterialID     material;
};
struct RenderLight
{
    RenderLightID     id;
    Guid              entityGuid;
    // Preserve scale/shear; emitting dimensions remain in local space.
    Matrix4           transform = Matrix4::identity();
    LightType         Type = LightType::Point;
    float             color[3] = { 1, 1, 1 };
    float             intensity = 1;
    float             range = 0;
    float             innerConeAngle = 0.523598776f;
    float             outerConeAngle = 0.785398163f;
    float             exposure = 0;
    float             angularDiameter = 0;
    float             Width = 1;
    float             Height = 1;
    float             radius = 0.5f;
    float             length = 1;
    float             colorTemperature = 6500;
    uint32_t          enableColorTemperature = 0;
    uint32_t          normalize = 0;
    uint32_t          twoSided = 0;
    GpuTextureID      texture;
    RenderPrimitiveID primitive; // Mesh emitter on the same entity; no second geometry lease.
    uint32_t          ready = 0;
};
struct RenderCamera
{
    RenderCameraID id;
    Guid           entityGuid;
    Matrix4        current = Matrix4::identity();
    Matrix4        previous = Matrix4::identity();
    // World-space poses and lens data; viewport/projection policy belongs to the renderer.
    Camera         lens;
    Camera         previousLens;
};

struct RenderRange
{
    uint32_t first = 0;
    uint32_t Count = 0;
};
struct RenderChanges
{
    Span<uint32_t>    created;
    Span<uint32_t>    removed;
    // Sorted, merged ranges include created, modified and cleared entries.
    Span<RenderRange> ranges;
    bool              activeList = false;
};
struct RenderSceneChanges
{
    RenderChanges     primitives;
    RenderChanges     instances;
    RenderChanges     meshBatches;
    RenderChanges     lights;
    RenderChanges     cameras;
    Span<RenderRange> instanceIndices;
};

class RenderSceneData;
class GpuScene;

// Changes accumulated across updates since the GPU scene last acknowledged an Upload.
struct RenderSceneUploads
{
    Span<RenderRange> primitives;
    Span<RenderRange> instances;
    Span<RenderRange> meshBatches;
    Span<RenderRange> instanceIndices;
    Span<RenderRange> lights;
    Span<RenderRange> cameras;
    bool              activePrimitives = false;
    bool              activeInstances = false;
    bool              activeMeshBatches = false;
    bool              activeLights = false;
    bool              activeCameras = false;
};

class RenderScene
{
public:
    // Owns World asset bindings. World and asset manager must outlive this scene.
    RenderScene(World& world, AssetManager& assetManager);
    ~RenderScene();
    RenderScene(const RenderScene&) = delete;
    RenderScene& operator=(const RenderScene&) = delete;

    void               Reserve(uint32_t primitiveCount, uint32_t meshBatchCount, uint32_t lightCount = 0, uint32_t cameraCount = 0);
    // Runs after asset manager loading/mutation. Updates World transforms and bindings without advancing history.
    void               Update();
    uint64_t           GetRevision() const;
    // Only the latest Update.
    RenderSceneChanges GetChanges() const;
    // Returned spans expire on the next call or Update.
    RenderSceneUploads GetPendingUploads();
    // The next submitted frame renders previous == current for the entity, or for every entity when invalid.
    void               markDirty(Entity entity = {});

    RenderPrimitiveID      FindPrimitive(Entity entity) const;
    const RenderPrimitive* GetPrimitive(RenderPrimitiveID id) const;
    const RenderInstance*  GetInstance(RenderInstanceID id) const;
    const RenderMeshBatch* GetMeshBatch(RenderMeshBatchID id) const;
    RenderLightID          FindLight(Entity entity) const;
    RenderCameraID         FindCamera(Entity entity) const;
    const RenderLight*     GetLight(RenderLightID id) const;
    const RenderCamera*    GetCamera(RenderCameraID id) const;

    // CPU extraction views expire at the next Update. Mesh/material handles are borrowed.
    // Iterate active lists; sparse arrays include holes. These views do not pin assets for GPU work.
    // Slot order follows sparse allocation, not insertion or Entity order: consumers that need deterministic
    // output (transparency sorting, screenshots) must Sort by their own key instead of relying on index order.
    Span<RenderPrimitive> GetPrimitives() const;
    Span<RenderInstance>  GetInstances() const;
    Span<RenderMeshBatch> GetMeshBatches() const;
    Span<RenderLight>     GetLights() const;
    Span<RenderCamera>    GetCameras() const;
    Span<uint32_t>        GetActivePrimitives() const;
    Span<uint32_t>        GetActiveInstances() const;
    Span<uint32_t>        GetActiveMeshBatches() const;
    Span<uint32_t>        GetActiveLights() const;
    Span<uint32_t>        GetActiveCameras() const;
    Span<uint32_t>        GetInstanceIndices() const;

private:
    friend class GpuScene;
    // Set while a GpuScene frame is open; Update() and markDirty() must not run then.
    void             SetFrameLocked(bool locked);
    void             AcknowledgeUploads();
    // Records the current transforms and lenses as submitted, so the next Update renders them as previous.
    void             CommitHistory();
    RenderSceneData* Data = nullptr;
};
} // namespace hz
