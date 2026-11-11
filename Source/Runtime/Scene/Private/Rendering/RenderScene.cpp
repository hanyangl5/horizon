/* Copyright (c) 2026 Horizon */

#include "RenderStorage.h"
#include "Scene/WorldAssetBindings.h"
#include "Scene/MeshAsset.h"
#include <math.h>

#include "Core/IMemory.h"
#include "Profiler/IProfiler.h"

namespace hz
{
static bool equalMatrix(const Matrix4& a, const Matrix4& b)
{
    for (uint32_t column = 0; column < 4; ++column)
        for (uint32_t row = 0; row < 4; ++row)
            if ((float)a.getElem(column, row) != (float)b.getElem(column, row))
                return false;
    return true;
}
static bool equalBounds(const RenderBounds& a, const RenderBounds& b)
{
    for (uint32_t i = 0; i < 3; ++i)
        if (a.minimum[i] != b.minimum[i] || a.maximum[i] != b.maximum[i])
            return false;
    return true;
}
static bool equalLens(const Camera& a, const Camera& b)
{
    return a.verticalFov == b.verticalFov && a.nearPlane == b.nearPlane && a.farPlane == b.farPlane;
}
static bool equalLight(const RenderLight& a, const RenderLight& b)
{
    return a.entityGuid == b.entityGuid && equalMatrix(a.transform, b.transform) && a.type == b.type && a.color[0] == b.color[0] &&
           a.color[1] == b.color[1] && a.color[2] == b.color[2] && a.intensity == b.intensity && a.range == b.range &&
           a.innerConeAngle == b.innerConeAngle && a.outerConeAngle == b.outerConeAngle && a.exposure == b.exposure &&
           a.angularDiameter == b.angularDiameter && a.width == b.width && a.height == b.height && a.radius == b.radius &&
           a.length == b.length && a.colorTemperature == b.colorTemperature && a.enableColorTemperature == b.enableColorTemperature &&
           a.normalize == b.normalize && a.twoSided == b.twoSided && a.texture == b.texture && a.primitive == b.primitive &&
           a.ready == b.ready;
}
static RenderBounds transformBounds(const MeshBounds& bounds, const Matrix4& matrix)
{
    RenderBounds result;
    for (uint32_t row = 0; row < 3; ++row)
    {
        float minimum = (float)matrix.getElem(3, row), maximum = minimum;
        for (uint32_t column = 0; column < 3; ++column)
        {
            const float a = (float)matrix.getElem(column, row) * bounds.minimum[column];
            const float b = (float)matrix.getElem(column, row) * bounds.maximum[column];
            minimum += fminf(a, b);
            maximum += fmaxf(a, b);
        }
        result.minimum[row] = minimum;
        result.maximum[row] = maximum;
    }
    return result;
}

struct RenderEntity
{
    Entity                   entity;
    RenderPrimitiveID        primitive;
    RenderInstanceID         instance;
    Array<RenderMeshBatchID> meshBatches;
    MeshBounds               localBounds;
    bool                     seen = false;
};
// Indexed by instance index. Pending marks entries whose submitted transform may need recording.
struct RenderHistory
{
    Matrix4 submitted = Matrix4::identity();
    bool    valid = false;
    bool    reset = false;
    bool    pending = false;
};
struct RenderEntityIndex
{
    Entity   key;
    uint32_t value;
};
struct RenderLightEntity
{
    Entity        entity;
    RenderLightID id;
    AssetID       textureID;
    AssetHandle   texture;
    AssetError    error = AssetError::None;
    bool          seen = false;
};
struct RenderCameraEntity
{
    Entity         entity;
    RenderCameraID id;
    Matrix4        submitted = Matrix4::identity();
    Camera         submittedLens;
    bool           historyValid = false;
    bool           historyReset = false;
    bool           seen = false;
};

class RenderSceneData
{
public:
    RenderSceneData(World& world, AssetManager& assetManager):
        world(world), assetManager(assetManager), bindings(world, assetManager), query(world, { SceneTypes::meshRenderer }),
        lightQuery(world, { SceneTypes::light }), cameraQuery(world, { SceneTypes::camera })
    {
        ASSERT(query.isValid() && lightQuery.isValid() && cameraQuery.isValid());
    }
    ~RenderSceneData()
    {
        for (const RenderLightEntity& light : lightEntities)
            if (light.texture.isValid())
                assetManager.release(light.texture);
        hmfree(pIndices);
        hmfree(pLightIndices);
        hmfree(pCameraIndices);
    }

    static bool syncLight(Entity entity, Span<const void*> fields, void* pUser)
    {
        RenderSceneData&         data = *(RenderSceneData*)pUser;
        const RenderEntityIndex* pIndex = hmgetp_null(data.pLightIndices, entity);
        if (!pIndex)
        {
            RenderLightEntity& lightEntity = data.lightEntities.emplaceBack();
            lightEntity.entity = entity;
            lightEntity.id = data.lights.add();
            data.textureReleases.reserve(data.lightEntities.size());
            const RenderEntityIndex index = { .key = entity, .value = data.lightEntities.size() - 1 };
            hmputs(data.pLightIndices, index);
            pIndex = hmgetp_null(data.pLightIndices, entity);
        }
        RenderLightEntity& lightEntity = data.lightEntities[pIndex->value];
        lightEntity.seen = true;
        const Light&  light = *(const Light*)fields.pData[0];
        const AssetID textureID = light.type >= LightType::Rect ? light.texture : AssetID{};
        if (lightEntity.textureID != textureID)
        {
            if (lightEntity.texture.isValid())
            {
                data.textureReleases.pushBack(lightEntity.texture);
            }
            lightEntity.textureID = textureID;
            lightEntity.texture = {};
            lightEntity.error = AssetError::None;
        }
        if (textureID.isValid() && !lightEntity.texture.isValid() &&
            (lightEntity.error == AssetError::None || lightEntity.error == AssetError::Retiring))
        {
            lightEntity.texture = data.assetManager.request(textureID, &lightEntity.error);
        }
        return true;
    }
    void extractLights()
    {
        for (uint32_t i = 0; i < lightEntities.size();)
        {
            RenderLightEntity& entity = lightEntities[i];
            if (!entity.seen)
            {
                if (entity.texture.isValid())
                    textureReleases.pushBack(entity.texture);
                lights.remove(entity.id);
                hmdel(pLightIndices, entity.entity);
                lightEntities[i] = lightEntities[lightEntities.size() - 1];
                lightEntities.popBack();
                if (i < lightEntities.size())
                    hmgetp_null(pLightIndices, lightEntities[i].entity)->value = i;
                continue;
            }
            const Light& source = *(const Light*)world.getComponent(entity.entity, SceneTypes::light);
            if (assetManager.getState(entity.texture) == AssetState::Ready && !assetManager.getTexture(entity.texture) &&
                entity.error != AssetError::LoadFailed)
            {
                entity.error = AssetError::LoadFailed;
                LOGF(eERROR, "Light emission map has the wrong asset type");
            }
            RenderLight next = {
                .id = entity.id,
                .entityGuid = world.getEntityGuid(entity.entity),
                .transform = world.getWorldTransform(entity.entity)->current,
                .type = source.type,
                .color = { (float)source.color.getX(), (float)source.color.getY(), (float)source.color.getZ() },
                .intensity = source.intensity,
                .range = source.range,
                .innerConeAngle = source.innerConeAngle,
                .outerConeAngle = source.outerConeAngle,
                .exposure = source.exposure,
                .angularDiameter = source.angularDiameter,
                .width = source.width,
                .height = source.height,
                .radius = source.radius,
                .length = source.length,
                .colorTemperature = source.colorTemperature,
                .enableColorTemperature = source.enableColorTemperature ? 1u : 0u,
                .normalize = source.normalize ? 1u : 0u,
                .twoSided = source.twoSided ? 1u : 0u,
                .ready = !entity.textureID.isValid() || assetManager.getTexture(entity.texture) ? 1u : 0u,
            };
            if (entity.textureID.isValid() && next.ready)
                next.texture = { .index = entity.texture.index, .generation = entity.texture.generation };
            if (source.type == LightType::Mesh)
            {
                const RenderEntityIndex* pIndex = hmgetp_null(pIndices, entity.entity);
                if (pIndex)
                    next.primitive = entities[pIndex->value].primitive;
                const RenderPrimitive* pPrimitive = primitives.get(next.primitive);
                next.ready = next.ready && pPrimitive && pPrimitive->instanceCount ? 1u : 0u;
            }
            RenderLight& light = lights.values[entity.id.index];
            if (!equalLight(light, next))
            {
                light = next;
                lights.markDirty(entity.id.index);
            }
            ++i;
        }
        for (AssetHandle handle : textureReleases)
            assetManager.release(handle);
        textureReleases.clear();
    }
    static bool extractCamera(Entity entity, Span<const void*> fields, void* pUser)
    {
        RenderSceneData&         data = *(RenderSceneData*)pUser;
        const RenderEntityIndex* pIndex = hmgetp_null(data.pCameraIndices, entity);
        if (!pIndex)
        {
            RenderCameraEntity& cameraEntity = data.cameraEntities.emplaceBack();
            cameraEntity.entity = entity;
            cameraEntity.id = data.cameras.add();
            const RenderEntityIndex index = { .key = entity, .value = data.cameraEntities.size() - 1 };
            hmputs(data.pCameraIndices, index);
            pIndex = hmgetp_null(data.pCameraIndices, entity);
        }
        RenderCameraEntity& cameraEntity = data.cameraEntities[pIndex->value];
        cameraEntity.seen = true;
        const Matrix4& current = data.world.getWorldTransform(entity)->current;
        const Camera&  lens = *(const Camera*)fields.pData[0];
        const Matrix4& previous = cameraEntity.historyValid ? cameraEntity.submitted : current;
        const Camera&  previousLens = cameraEntity.historyValid ? cameraEntity.submittedLens : lens;
        const Guid     entityGuid = data.world.getEntityGuid(entity);
        RenderCamera&  camera = data.cameras.values[cameraEntity.id.index];
        if (camera.entityGuid != entityGuid || !equalMatrix(camera.current, current) || !equalMatrix(camera.previous, previous) ||
            !equalLens(camera.lens, lens) || !equalLens(camera.previousLens, previousLens))
        {
            camera.entityGuid = entityGuid;
            camera.current = current;
            camera.previous = previous;
            camera.lens = lens;
            camera.previousLens = previousLens;
            data.cameras.markDirty(cameraEntity.id.index);
        }
        cameraEntity.historyReset = false;
        return true;
    }
    void removeCameras()
    {
        for (uint32_t i = 0; i < cameraEntities.size();)
        {
            const RenderCameraEntity& entity = cameraEntities[i];
            if (entity.seen)
            {
                ++i;
                continue;
            }
            cameras.remove(entity.id);
            hmdel(pCameraIndices, entity.entity);
            cameraEntities[i] = cameraEntities[cameraEntities.size() - 1];
            cameraEntities.popBack();
            if (i < cameraEntities.size())
                hmgetp_null(pCameraIndices, cameraEntities[i].entity)->value = i;
        }
    }

    void removeMeshBatches(RenderEntity& entity)
    {
        for (RenderMeshBatchID meshBatch : entity.meshBatches)
        {
            meshBatches.remove(meshBatch);
        }
        entity.meshBatches.clear();
    }
    void setInstanceIndex(uint32_t index, uint32_t value)
    {
        if (instanceIndices.size() <= index)
        {
            indicesDirty.reserve(index + 1);
            indicesRanges.reserve(index + 1);
            indicesPending.reserve(index + 1);
        }
        while (instanceIndices.size() <= index)
        {
            markIndex(instanceIndices.size());
            instanceIndices.pushBack(UINT32_MAX);
            indicesPendingFlags.pushBack(true);
        }
        if (instanceIndices[index] != value)
        {
            instanceIndices[index] = value;
            markIndex(index);
        }
    }
    void markIndex(uint32_t index)
    {
        indicesDirty.pushBack(index);
        if (index >= indicesPendingFlags.size() || !indicesPendingFlags[index])
        {
            if (index < indicesPendingFlags.size())
            {
                indicesPendingFlags[index] = true;
            }
            indicesPending.pushBack(index);
        }
    }
    void markHistory(uint32_t instance)
    {
        RenderHistory& history = instanceHistory[instance];
        if (!history.pending)
        {
            history.pending = true;
            historyPending.pushBack(instance);
        }
    }
    RenderEntity& add(Entity entity)
    {
        RenderEntity& renderEntity = entities.emplaceBack();
        renderEntity.entity = entity;
        renderEntity.primitive = primitives.add();
        renderEntity.instance = instances.add();
        primitives.values[renderEntity.primitive.index].instanceOffset = renderEntity.primitive.index;
        instances.values[renderEntity.instance.index].primitive = renderEntity.primitive;
        if (instanceHistory.size() <= renderEntity.instance.index)
        {
            historyPending.reserve(renderEntity.instance.index + 1);
            instanceHistory.resize(renderEntity.instance.index + 1);
        }
        instanceHistory[renderEntity.instance.index].valid = false;
        instanceHistory[renderEntity.instance.index].reset = false;
        setInstanceIndex(renderEntity.primitive.index, UINT32_MAX);
        const RenderEntityIndex index = { .key = entity, .value = entities.size() - 1 };
        hmputs(pIndices, index);
        return renderEntity;
    }
    void buildMeshBatches(RenderEntity& entity, const MeshAssetBinding& binding, const MeshAsset& mesh)
    {
        removeMeshBatches(entity);
        entity.meshBatches.reserve(binding.materials.count);
        entity.localBounds = binding.materials.count ? mesh.getBounds(0) : MeshBounds{};
        for (uint32_t i = 0; i < binding.materials.count; ++i)
        {
            const MaterialAssetBinding& material = binding.materials.pData[i];
            const RenderMeshBatchID     id = meshBatches.add();
            RenderMeshBatch&            meshBatch = meshBatches.values[id.index];
            meshBatch.primitive = entity.primitive;
            meshBatch.submesh = material.submesh;
            meshBatch.mesh = { .mesh = primitives.values[entity.primitive.index].mesh, .submeshIndex = i };
            meshBatch.material = { .index = material.handle.index, .generation = material.handle.generation };
            entity.meshBatches.pushBack(id);
            const MeshBounds& bounds = mesh.getBounds(i);
            for (uint32_t axis = 0; axis < 3; ++axis)
            {
                entity.localBounds.minimum[axis] = fminf(entity.localBounds.minimum[axis], bounds.minimum[axis]);
                entity.localBounds.maximum[axis] = fmaxf(entity.localBounds.maximum[axis], bounds.maximum[axis]);
            }
        }
    }
    void extract(RenderEntity& entity)
    {
        entity.seen = true;
        RenderPrimitive&        primitive = primitives.values[entity.primitive.index];
        const MeshAssetBinding* pBinding = bindings.get(entity.entity);
        ASSERT(pBinding);
        const bool       ready = pBinding->state == AssetBindingState::Ready;
        const MeshAsset* pMesh = assetManager.getMesh(pBinding->mesh);
        const GpuMeshID  mesh = pMesh ? GpuMeshID{ .index = pBinding->mesh.index, .generation = pBinding->mesh.generation } : GpuMeshID{};
        const uint32_t   count = ready ? 1u : 0u;
        if (primitive.mesh != mesh)
        {
            removeMeshBatches(entity);
            entity.localBounds = {};
        }
        if (primitive.mesh != mesh || primitive.instanceCount != count)
        {
            if (ready && (primitive.mesh != mesh || !primitive.instanceCount))
            {
                instanceHistory[entity.instance.index].valid = false;
                markHistory(entity.instance.index);
            }
            primitive.mesh = mesh;
            primitive.instanceCount = count;
            primitives.markDirty(entity.primitive.index);
            setInstanceIndex(entity.primitive.index, ready ? entity.instance.index : UINT32_MAX);
        }
        if (ready && entity.meshBatches.empty())
        {
            buildMeshBatches(entity, *pBinding, *pMesh);
        }
        if (ready)
        {
            ASSERT(entity.meshBatches.size() == pBinding->materials.count);
            for (uint32_t i = 0; i < entity.meshBatches.size(); ++i)
            {
                RenderMeshBatch&    meshBatch = meshBatches.values[entity.meshBatches[i].index];
                const AssetHandle   handle = pBinding->materials.pData[i].handle;
                const GpuMaterialID material = { .index = handle.index, .generation = handle.generation };
                if (meshBatch.material != material)
                {
                    meshBatch.material = material;
                    meshBatches.markDirty(meshBatch.id.index);
                }
            }
        }
        else
            for (RenderMeshBatchID id : entity.meshBatches)
            {
                RenderMeshBatch& meshBatch = meshBatches.values[id.index];
                if (meshBatch.material.isValid())
                {
                    meshBatch.material = {};
                    meshBatches.markDirty(id.index);
                }
            }
        const WorldTransform& transform = *world.getWorldTransform(entity.entity);
        RenderInstance&       instance = instances.values[entity.instance.index];
        const Guid            entityGuid = world.getEntityGuid(entity.entity);
        const RenderBounds    bounds = ready ? transformBounds(entity.localBounds, transform.current) : RenderBounds{};
        RenderHistory&        history = instanceHistory[entity.instance.index];
        const Matrix4&        previous = history.valid ? history.submitted : transform.current;
        if (instance.entityGuid != entityGuid || !equalMatrix(instance.current, transform.current) ||
            !equalMatrix(instance.previous, previous) || !equalBounds(instance.bounds, bounds))
        {
            instance.entityGuid = entityGuid;
            instance.current = transform.current;
            instance.previous = previous;
            instance.bounds = bounds;
            instances.markDirty(instance.id.index);
            markHistory(entity.instance.index);
        }
        if (history.reset)
        {
            history.reset = false;
            markHistory(entity.instance.index);
        }
    }
    static bool extractEntity(Entity entity, Span<const void*>, void* pUser)
    {
        RenderSceneData&         data = *(RenderSceneData*)pUser;
        const RenderEntityIndex* pIndex = hmgetp_null(data.pIndices, entity);
        RenderEntity&            renderEntity = pIndex ? data.entities[pIndex->value] : data.add(entity);
        data.extract(renderEntity);
        return true;
    }
    void update()
    {
        PROFILER_SET_CPU_SCOPE_AUTO_NAMED("RenderScene::update");
        ASSERT(!world.isDeferred() && !frameLocked);
        {
            PROFILER_SET_CPU_SCOPE_AUTO_NAMED("RenderScene::updateTransforms");
            world.updateTransforms();
        }
        primitives.beginUpdate();
        instances.beginUpdate();
        meshBatches.beginUpdate();
        lights.beginUpdate();
        cameras.beginUpdate();
        indicesDirty.clear();
        for (RenderEntity& entity : entities)
            entity.seen = false;
        for (RenderLightEntity& entity : lightEntities)
            entity.seen = false;
        for (RenderCameraEntity& entity : cameraEntities)
            entity.seen = false;
        // Retain new emission maps before mesh/material bindings release old dependencies.
        lightQuery.each(syncLight, this);
        {
            PROFILER_SET_CPU_SCOPE_AUTO_NAMED("RenderScene::bindings");
            bindings.update();
        }
        {
            PROFILER_SET_CPU_SCOPE_AUTO_NAMED("RenderScene::extract");
            query.each(extractEntity, this);
        }
        for (uint32_t i = 0; i < entities.size();)
        {
            RenderEntity& entity = entities[i];
            if (entity.seen)
            {
                ++i;
                continue;
            }
            removeMeshBatches(entity);
            setInstanceIndex(entity.primitive.index, UINT32_MAX);
            primitives.remove(entity.primitive);
            instances.remove(entity.instance);
            hmdel(pIndices, entity.entity);
            if (i + 1 < entities.size())
                entities[i] = std::move(entities[entities.size() - 1]);
            entities.popBack();
            if (i < entities.size())
                hmgetp_null(pIndices, entities[i].entity)->value = i;
        }
        extractLights();
        cameraQuery.each(extractCamera, this);
        removeCameras();
        primitives.finishUpdate();
        instances.finishUpdate();
        meshBatches.finishUpdate();
        lights.finishUpdate();
        cameras.finishUpdate();
        mergeRenderRanges(indicesDirty, indicesRanges);
        if (primitives.getChanges().ranges.count || instances.getChanges().ranges.count || meshBatches.getChanges().ranges.count ||
            lights.getChanges().ranges.count || cameras.getChanges().ranges.count || !indicesRanges.empty())
        {
            ++revision;
        }
    }
    RenderSceneUploads getPendingUploads()
    {
        primitives.getPending(uploadRanges[0]);
        instances.getPending(uploadRanges[1]);
        meshBatches.getPending(uploadRanges[2]);
        mergeRenderRanges(indicesPending, uploadRanges[3]);
        lights.getPending(uploadRanges[4]);
        cameras.getPending(uploadRanges[5]);
        return { .primitives = { uploadRanges[0].data(), uploadRanges[0].size() },
                 .instances = { uploadRanges[1].data(), uploadRanges[1].size() },
                 .meshBatches = { uploadRanges[2].data(), uploadRanges[2].size() },
                 .instanceIndices = { uploadRanges[3].data(), uploadRanges[3].size() },
                 .lights = { uploadRanges[4].data(), uploadRanges[4].size() },
                 .cameras = { uploadRanges[5].data(), uploadRanges[5].size() },
                 .activePrimitives = primitives.isActivePending(),
                 .activeInstances = instances.isActivePending(),
                 .activeMeshBatches = meshBatches.isActivePending(),
                 .activeLights = lights.isActivePending(),
                 .activeCameras = cameras.isActivePending() };
    }
    void acknowledgeUploads()
    {
        primitives.clearPending();
        instances.clearPending();
        meshBatches.clearPending();
        lights.clearPending();
        cameras.clearPending();
        for (uint32_t index : indicesPending)
        {
            indicesPendingFlags[index] = false;
        }
        indicesPending.clear();
    }
    void commitHistory()
    {
        PROFILER_SET_CPU_SCOPE_AUTO_NAMED("RenderScene::commitHistory");
        for (uint32_t index : historyPending)
        {
            RenderHistory& history = instanceHistory[index];
            history.pending = false;
            const RenderInstance&  instance = instances.values[index];
            const RenderPrimitive* pPrimitive = primitives.get(instance.primitive);
            // A reset since the last update renders previous == current first.
            if (history.reset || !instance.id.isValid() || !pPrimitive || !pPrimitive->instanceCount)
            {
                continue;
            }
            history.submitted = instance.current;
            history.valid = true;
        }
        historyPending.clear();
        for (RenderCameraEntity& entity : cameraEntities)
        {
            if (entity.historyReset)
            {
                continue;
            }
            const RenderCamera& camera = cameras.values[entity.id.index];
            entity.submitted = camera.current;
            entity.submittedLens = camera.lens;
            entity.historyValid = true;
        }
    }
    void markDirty(Entity target)
    {
        ASSERT(!frameLocked);
        for (RenderEntity& entity : entities)
            if (!target.isValid() || entity.entity == target)
            {
                instanceHistory[entity.instance.index].valid = false;
                instanceHistory[entity.instance.index].reset = true;
            }
        for (RenderCameraEntity& entity : cameraEntities)
            if (!target.isValid() || entity.entity == target)
            {
                entity.historyValid = false;
                entity.historyReset = true;
            }
    }

    World&                                          world;
    AssetManager&                                   assetManager;
    WorldAssetBindings                              bindings;
    WorldQuery                                      query;
    WorldQuery                                      lightQuery;
    WorldQuery                                      cameraQuery;
    RenderStorage<RenderPrimitive, RenderPrimitiveID> primitives;
    RenderStorage<RenderInstance, RenderInstanceID>   instances;
    RenderStorage<RenderMeshBatch, RenderMeshBatchID> meshBatches;
    RenderStorage<RenderLight, RenderLightID>         lights;
    RenderStorage<RenderCamera, RenderCameraID>       cameras;
    Array<RenderEntity>                             entities;
    RenderEntityIndex*                              pIndices = nullptr;
    Array<RenderLightEntity>                        lightEntities;
    Array<RenderCameraEntity>                       cameraEntities;
    RenderEntityIndex*                              pLightIndices = nullptr;
    RenderEntityIndex*                              pCameraIndices = nullptr;
    Array<AssetHandle>                              textureReleases;
    Array<uint32_t>                                 instanceIndices;
    Array<uint32_t>                                 indicesDirty;
    Array<RenderRange>                              indicesRanges;
    Array<uint32_t>                                 indicesPending;
    Array<bool>                                     indicesPendingFlags;
    Array<RenderHistory>                            instanceHistory;
    Array<uint32_t>                                 historyPending;
    Array<RenderRange>                              uploadRanges[6];
    uint64_t                                        revision = 0;
    bool                                            frameLocked = false;
};

RenderScene::RenderScene(World& world, AssetManager& assetManager): pData(tf_new(RenderSceneData, world, assetManager)) {}
RenderScene::~RenderScene() { tf_delete(pData); }
void RenderScene::reserve(uint32_t primitiveCount, uint32_t meshBatchCount, uint32_t lightCount, uint32_t cameraCount)
{
    pData->primitives.reserve(primitiveCount);
    pData->instances.reserve(primitiveCount);
    pData->meshBatches.reserve(meshBatchCount);
    pData->entities.reserve(primitiveCount);
    pData->instanceIndices.reserve(primitiveCount);
    pData->indicesDirty.reserve(primitiveCount);
    pData->indicesRanges.reserve(primitiveCount);
    pData->indicesPending.reserve(primitiveCount);
    pData->indicesPendingFlags.reserve(primitiveCount);
    pData->instanceHistory.reserve(primitiveCount);
    pData->historyPending.reserve(primitiveCount);
    for (Array<RenderRange>& ranges : pData->uploadRanges)
    {
        ranges.reserve(primitiveCount > meshBatchCount ? primitiveCount : meshBatchCount);
    }
    pData->lights.reserve(lightCount);
    pData->cameras.reserve(cameraCount);
    pData->lightEntities.reserve(lightCount);
    pData->cameraEntities.reserve(cameraCount);
    pData->textureReleases.reserve(lightCount);
}
void               RenderScene::update() { pData->update(); }
uint64_t           RenderScene::getRevision() const { return pData->revision; }
RenderSceneUploads RenderScene::getPendingUploads() { return pData->getPendingUploads(); }
void               RenderScene::acknowledgeUploads() { pData->acknowledgeUploads(); }
void               RenderScene::commitHistory() { pData->commitHistory(); }
void               RenderScene::setFrameLocked(bool locked) { pData->frameLocked = locked; }
void               RenderScene::markDirty(Entity entity) { pData->markDirty(entity); }
RenderSceneChanges RenderScene::getChanges() const
{
    return { .primitives = pData->primitives.getChanges(),
             .instances = pData->instances.getChanges(),
             .meshBatches = pData->meshBatches.getChanges(),
             .lights = pData->lights.getChanges(),
             .cameras = pData->cameras.getChanges(),
             .instanceIndices = { pData->indicesRanges.data(), pData->indicesRanges.size() } };
}
RenderPrimitiveID RenderScene::findPrimitive(Entity entity) const
{
    const RenderEntityIndex* pIndex = hmgetp_null(pData->pIndices, entity);
    return pIndex ? pData->entities[pIndex->value].primitive : RenderPrimitiveID{};
}
const RenderPrimitive* RenderScene::getPrimitive(RenderPrimitiveID id) const { return pData->primitives.get(id); }
const RenderInstance*  RenderScene::getInstance(RenderInstanceID id) const { return pData->instances.get(id); }
const RenderMeshBatch* RenderScene::getMeshBatch(RenderMeshBatchID id) const { return pData->meshBatches.get(id); }
RenderLightID          RenderScene::findLight(Entity entity) const
{
    const RenderEntityIndex* pIndex = hmgetp_null(pData->pLightIndices, entity);
    return pIndex ? pData->lightEntities[pIndex->value].id : RenderLightID{};
}
RenderCameraID RenderScene::findCamera(Entity entity) const
{
    const RenderEntityIndex* pIndex = hmgetp_null(pData->pCameraIndices, entity);
    return pIndex ? pData->cameraEntities[pIndex->value].id : RenderCameraID{};
}
const RenderLight*    RenderScene::getLight(RenderLightID id) const { return pData->lights.get(id); }
const RenderCamera*   RenderScene::getCamera(RenderCameraID id) const { return pData->cameras.get(id); }
Span<RenderPrimitive> RenderScene::getPrimitives() const { return { pData->primitives.values.data(), pData->primitives.values.size() }; }
Span<RenderInstance>  RenderScene::getInstances() const { return { pData->instances.values.data(), pData->instances.values.size() }; }
Span<RenderMeshBatch> RenderScene::getMeshBatches() const { return { pData->meshBatches.values.data(), pData->meshBatches.values.size() }; }
Span<RenderLight>     RenderScene::getLights() const { return { pData->lights.values.data(), pData->lights.values.size() }; }
Span<RenderCamera>    RenderScene::getCameras() const { return { pData->cameras.values.data(), pData->cameras.values.size() }; }
Span<uint32_t> RenderScene::getActivePrimitives() const { return { pData->primitives.active.data(), pData->primitives.active.size() }; }
Span<uint32_t> RenderScene::getActiveInstances() const { return { pData->instances.active.data(), pData->instances.active.size() }; }
Span<uint32_t> RenderScene::getActiveMeshBatches() const { return { pData->meshBatches.active.data(), pData->meshBatches.active.size() }; }
Span<uint32_t> RenderScene::getActiveLights() const { return { pData->lights.active.data(), pData->lights.active.size() }; }
Span<uint32_t> RenderScene::getActiveCameras() const { return { pData->cameras.active.data(), pData->cameras.active.size() }; }
Span<uint32_t> RenderScene::getInstanceIndices() const { return { pData->instanceIndices.data(), pData->instanceIndices.size() }; }
} // namespace hz
