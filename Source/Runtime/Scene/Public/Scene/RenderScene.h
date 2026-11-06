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
    bool     isValid() const { return index != UINT32_MAX && generation != 0; }
    bool     operator==(const RenderPrimitiveID&) const = default;
};
struct RenderInstanceID
{
    uint32_t index = UINT32_MAX;
    uint32_t generation = 0;
    bool     isValid() const { return index != UINT32_MAX && generation != 0; }
    bool     operator==(const RenderInstanceID&) const = default;
};
struct RenderDrawID
{
    uint32_t index = UINT32_MAX;
    uint32_t generation = 0;
    bool     isValid() const { return index != UINT32_MAX && generation != 0; }
    bool     operator==(const RenderDrawID&) const = default;
};
struct RenderLightID
{
    uint32_t index = UINT32_MAX;
    uint32_t generation = 0;
    bool     isValid() const { return index != UINT32_MAX && generation != 0; }
    bool     operator==(const RenderLightID&) const = default;
};
struct RenderCameraID
{
    uint32_t index = UINT32_MAX;
    uint32_t generation = 0;
    bool     isValid() const { return index != UINT32_MAX && generation != 0; }
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
struct RenderDraw
{
    RenderDrawID      id;
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
    LightType         type = LightType::Point;
    float             color[3] = { 1, 1, 1 };
    float             intensity = 1;
    float             range = 0;
    float             innerConeAngle = 0.523598776f;
    float             outerConeAngle = 0.785398163f;
    float             exposure = 0;
    float             angularDiameter = 0;
    float             width = 1;
    float             height = 1;
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
    uint32_t count = 0;
};
struct RenderSlotChanges
{
    Span<uint32_t>    created;
    Span<uint32_t>    removed;
    // Sorted, merged ranges include created, modified and cleared slots.
    Span<RenderRange> ranges;
    bool              activeList = false;
};
struct RenderSceneChanges
{
    RenderSlotChanges primitives;
    RenderSlotChanges instances;
    RenderSlotChanges draws;
    RenderSlotChanges lights;
    RenderSlotChanges cameras;
    Span<RenderRange> instanceIndices;
};

class RenderSceneData;
class RenderFramePool;
class RenderFramePoolData;

struct RenderSceneDesc
{
    uint32_t journalCapacity = 1024;
};
struct RenderSceneSync
{
    Span<RenderRange> primitives;
    Span<RenderRange> instances;
    Span<RenderRange> draws;
    Span<RenderRange> instanceIndices;
    Span<RenderRange> lights;
    Span<RenderRange> cameras;
    bool              activePrimitives = false;
    bool              activeInstances = false;
    bool              activeDraws = false;
    bool              activeLights = false;
    bool              activeCameras = false;
    bool              full = false;
};

class RenderScene
{
public:
    // Owns World asset bindings. World and asset manager must outlive this scene.
    RenderScene(World& world, AssetManager& assetManager, const RenderSceneDesc& desc = {});
    ~RenderScene();
    RenderScene(const RenderScene&) = delete;
    RenderScene& operator=(const RenderScene&) = delete;

    void               reserve(uint32_t primitiveCount, uint32_t drawCount, uint32_t lightCount = 0, uint32_t cameraCount = 0);
    // Runs after asset manager loading/mutation. Updates World transforms and bindings without advancing history.
    void               update();
    uint64_t           getRevision() const;
    // Only the latest update; not a journal for rotating GPU backing storage.
    RenderSceneChanges getChanges() const;
    // Accumulated ranges since a backing's revision, or full=true after journal overflow.
    // Returned spans expire on the next call or update.
    RenderSceneSync    getChangesSince(uint64_t revision);
    void               resetHistory(Entity entity = {});

    RenderPrimitiveID      findPrimitive(Entity entity) const;
    const RenderPrimitive* getPrimitive(RenderPrimitiveID id) const;
    const RenderInstance*  getInstance(RenderInstanceID id) const;
    const RenderDraw*      getDraw(RenderDrawID id) const;
    RenderLightID          findLight(Entity entity) const;
    RenderCameraID         findCamera(Entity entity) const;
    const RenderLight*     getLight(RenderLightID id) const;
    const RenderCamera*    getCamera(RenderCameraID id) const;

    // CPU extraction views expire at the next update. Mesh/material handles are borrowed.
    // Iterate active lists; sparse arrays include holes. These views do not pin assets for GPU work.
    Span<RenderPrimitive> getPrimitives() const;
    Span<RenderInstance>  getInstances() const;
    Span<RenderDraw>      getDraws() const;
    Span<RenderLight>     getLights() const;
    Span<RenderCamera>    getCameras() const;
    Span<uint32_t>        getActivePrimitives() const;
    Span<uint32_t>        getActiveInstances() const;
    Span<uint32_t>        getActiveDraws() const;
    Span<uint32_t>        getActiveLights() const;
    Span<uint32_t>        getActiveCameras() const;
    Span<uint32_t>        getInstanceIndices() const;

private:
    friend class RenderFramePool;
    friend class RenderFramePoolData;
    AssetSnapshot    captureAssets();
    uint64_t         getHistoryEpoch() const;
    void             commitSnapshot(Span<RenderPrimitive> primitives, Span<RenderInstance> instances, Span<RenderCamera> cameras,
                                    uint64_t historyEpoch);
    RenderSceneData* pData = nullptr;
};
} // namespace hz
