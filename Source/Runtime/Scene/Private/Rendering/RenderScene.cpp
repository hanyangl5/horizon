/* Copyright (c) 2026 Horizon */

#include "RenderSlots.h"
#include "Scene/WorldAssetBindings.h"
#include "Scene/MeshAsset.h"
#include <math.h>

#include "Core/IMemory.h"

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
    Entity              entity;
    RenderPrimitiveID   primitive;
    RenderInstanceID    instance;
    Array<RenderDrawID> draws;
    MeshBounds          localBounds;
    Matrix4             submitted = Matrix4::identity();
    uint64_t            historyEpoch = 0;
    bool                historyValid = false;
    bool                seen = false;
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
    uint64_t       historyEpoch = 0;
    bool           historyValid = false;
    bool           seen = false;
};

enum class RenderTable : uint32_t
{
    Primitives,
    Instances,
    Draws,
    InstanceIndices,
    Lights,
    Cameras,
    ActivePrimitives,
    ActiveInstances,
    ActiveDraws,
    ActiveLights,
    ActiveCameras
};
struct RenderChange
{
    uint64_t    revision = 0;
    RenderTable table = RenderTable::Primitives;
    RenderRange range;
};
static int compareRenderRanges(const void* pA, const void* pB)
{
    const uint32_t a = ((const RenderRange*)pA)->first, b = ((const RenderRange*)pB)->first;
    return (a > b) - (a < b);
}
static void mergeRanges(Array<RenderRange>& ranges)
{
    if (ranges.empty())
        return;
    qsort(ranges.data(), ranges.size(), sizeof(RenderRange), compareRenderRanges);
    uint32_t count = 1;
    for (uint32_t i = 1; i < ranges.size(); ++i)
    {
        RenderRange&      previous = ranges[count - 1];
        const RenderRange range = ranges[i];
        if (range.first <= previous.first + previous.count)
        {
            const uint32_t end = range.first + range.count;
            if (end > previous.first + previous.count)
                previous.count = end - previous.first;
        }
        else
            ranges[count++] = range;
    }
    ranges.resize(count);
}

class RenderSceneData
{
public:
    RenderSceneData(World& world, AssetManager& assetManager, const RenderSceneDesc& desc):
        world(world), assetManager(assetManager), bindings(world, assetManager), query(world, { SceneTypes::meshRenderer }),
        lightQuery(world, { SceneTypes::light }), cameraQuery(world, { SceneTypes::camera })
    {
        ASSERT(query.isValid() && lightQuery.isValid() && cameraQuery.isValid());
        ASSERT(desc.journalCapacity);
        journal.resize(desc.journalCapacity);
        for (Array<RenderRange>& ranges : syncRanges)
            ranges.reserve(desc.journalCapacity);
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
            RenderLightEntity& record = data.lightEntities.emplaceBack();
            record.entity = entity;
            record.id = data.lights.add();
            data.textureReleases.reserve(data.lightEntities.size());
            const RenderEntityIndex index = { .key = entity, .value = data.lightEntities.size() - 1 };
            hmputs(data.pLightIndices, index);
            pIndex = hmgetp_null(data.pLightIndices, entity);
        }
        RenderLightEntity& record = data.lightEntities[pIndex->value];
        record.seen = true;
        const Light&  light = *(const Light*)fields.pData[0];
        const AssetID textureID = light.type >= LightType::Rect ? light.texture : AssetID{};
        if (record.textureID != textureID)
        {
            if (record.texture.isValid())
                data.textureReleases.pushBack(record.texture);
            record.textureID = textureID;
            record.texture = {};
            record.error = AssetError::None;
        }
        if (textureID.isValid() && !record.texture.isValid() && (record.error == AssetError::None || record.error == AssetError::Retiring))
        {
            record.texture = data.assetManager.request(textureID, &record.error);
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
            RenderCameraEntity& record = data.cameraEntities.emplaceBack();
            record.entity = entity;
            record.id = data.cameras.add();
            const RenderEntityIndex index = { .key = entity, .value = data.cameraEntities.size() - 1 };
            hmputs(data.pCameraIndices, index);
            pIndex = hmgetp_null(data.pCameraIndices, entity);
        }
        RenderCameraEntity& record = data.cameraEntities[pIndex->value];
        record.seen = true;
        const Matrix4& current = data.world.getWorldTransform(entity)->current;
        const Camera&  lens = *(const Camera*)fields.pData[0];
        const Matrix4& previous = record.historyValid ? record.submitted : current;
        const Camera&  previousLens = record.historyValid ? record.submittedLens : lens;
        const Guid     entityGuid = data.world.getEntityGuid(entity);
        RenderCamera&  camera = data.cameras.values[record.id.index];
        if (camera.entityGuid != entityGuid || !equalMatrix(camera.current, current) || !equalMatrix(camera.previous, previous) ||
            !equalLens(camera.lens, lens) || !equalLens(camera.previousLens, previousLens))
        {
            camera.entityGuid = entityGuid;
            camera.current = current;
            camera.previous = previous;
            camera.lens = lens;
            camera.previousLens = previousLens;
            data.cameras.markDirty(record.id.index);
        }
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

    void removeDraws(RenderEntity& entity)
    {
        for (RenderDrawID draw : entity.draws)
            draws.remove(draw);
        entity.draws.clear();
    }
    void setInstanceIndex(uint32_t index, uint32_t value)
    {
        if (instanceIndices.size() <= index)
        {
            indicesDirty.reserve(index + 1);
            indicesRanges.reserve(index + 1);
        }
        while (instanceIndices.size() <= index)
        {
            indicesDirty.pushBack(instanceIndices.size());
            instanceIndices.pushBack(UINT32_MAX);
        }
        if (instanceIndices[index] != value)
        {
            instanceIndices[index] = value;
            indicesDirty.pushBack(index);
        }
    }
    RenderEntity& add(Entity entity)
    {
        RenderEntity& record = entities.emplaceBack();
        record.entity = entity;
        record.primitive = primitives.add();
        record.instance = instances.add();
        primitives.values[record.primitive.index].instanceOffset = record.primitive.index;
        instances.values[record.instance.index].primitive = record.primitive;
        setInstanceIndex(record.primitive.index, UINT32_MAX);
        const RenderEntityIndex index = { .key = entity, .value = entities.size() - 1 };
        hmputs(pIndices, index);
        return record;
    }
    void buildDraws(RenderEntity& entity, const MeshAssetBinding& binding, const MeshAsset& mesh)
    {
        removeDraws(entity);
        entity.draws.reserve(binding.materials.count);
        entity.localBounds = binding.materials.count ? mesh.getBounds(0) : MeshBounds{};
        for (uint32_t i = 0; i < binding.materials.count; ++i)
        {
            const MaterialAssetBinding& material = binding.materials.pData[i];
            const RenderDrawID          id = draws.add();
            RenderDraw&                 draw = draws.values[id.index];
            draw.primitive = entity.primitive;
            draw.submesh = material.submesh;
            draw.mesh = { .mesh = primitives.values[entity.primitive.index].mesh, .submeshIndex = i };
            draw.material = { .index = material.handle.index, .generation = material.handle.generation };
            entity.draws.pushBack(id);
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
            removeDraws(entity);
            entity.localBounds = {};
        }
        if (primitive.mesh != mesh || primitive.instanceCount != count)
        {
            if (ready && (primitive.mesh != mesh || !primitive.instanceCount))
            {
                entity.historyValid = false;
                entity.historyEpoch = ++historyEpoch;
            }
            primitive.mesh = mesh;
            primitive.instanceCount = count;
            primitives.markDirty(entity.primitive.index);
            setInstanceIndex(entity.primitive.index, ready ? entity.instance.index : UINT32_MAX);
        }
        if (ready && entity.draws.empty())
            buildDraws(entity, *pBinding, *pMesh);
        if (ready)
        {
            ASSERT(entity.draws.size() == pBinding->materials.count);
            for (uint32_t i = 0; i < entity.draws.size(); ++i)
            {
                RenderDraw&         draw = draws.values[entity.draws[i].index];
                const AssetHandle   handle = pBinding->materials.pData[i].handle;
                const GpuMaterialID material = { .index = handle.index, .generation = handle.generation };
                if (draw.material != material)
                {
                    draw.material = material;
                    draws.markDirty(draw.id.index);
                }
            }
        }
        else
            for (RenderDrawID id : entity.draws)
            {
                RenderDraw& draw = draws.values[id.index];
                if (draw.material.isValid())
                {
                    draw.material = {};
                    draws.markDirty(id.index);
                }
            }
        const WorldTransform& transform = *world.getWorldTransform(entity.entity);
        RenderInstance&       instance = instances.values[entity.instance.index];
        const Guid            entityGuid = world.getEntityGuid(entity.entity);
        const RenderBounds    bounds = ready ? transformBounds(entity.localBounds, transform.current) : RenderBounds{};
        const Matrix4&        previous = entity.historyValid ? entity.submitted : transform.current;
        if (instance.entityGuid != entityGuid || !equalMatrix(instance.current, transform.current) ||
            !equalMatrix(instance.previous, previous) || !equalBounds(instance.bounds, bounds))
        {
            instance.entityGuid = entityGuid;
            instance.current = transform.current;
            instance.previous = previous;
            instance.bounds = bounds;
            instances.markDirty(instance.id.index);
        }
    }
    static bool extractEntity(Entity entity, Span<const void*>, void* pUser)
    {
        RenderSceneData&         data = *(RenderSceneData*)pUser;
        const RenderEntityIndex* pIndex = hmgetp_null(data.pIndices, entity);
        RenderEntity&            record = pIndex ? data.entities[pIndex->value] : data.add(entity);
        data.extract(record);
        return true;
    }
    void update()
    {
        ASSERT(!world.isDeferred());
        world.updateTransforms();
        primitives.beginUpdate();
        instances.beginUpdate();
        draws.beginUpdate();
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
        bindings.update();
        query.each(extractEntity, this);
        for (uint32_t i = 0; i < entities.size();)
        {
            RenderEntity& entity = entities[i];
            if (entity.seen)
            {
                ++i;
                continue;
            }
            removeDraws(entity);
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
        draws.finishUpdate();
        lights.finishUpdate();
        cameras.finishUpdate();
        mergeRenderRanges(indicesDirty, indicesRanges);
        if (!primitives.getChanges().ranges.count && !instances.getChanges().ranges.count && !draws.getChanges().ranges.count &&
            !lights.getChanges().ranges.count && !cameras.getChanges().ranges.count && indicesRanges.empty())
            return;
        ++revision;
        record(RenderTable::Primitives, primitives.getChanges().ranges);
        record(RenderTable::Instances, instances.getChanges().ranges);
        record(RenderTable::Draws, draws.getChanges().ranges);
        record(RenderTable::InstanceIndices, { indicesRanges.data(), indicesRanges.size() });
        record(RenderTable::Lights, lights.getChanges().ranges);
        record(RenderTable::Cameras, cameras.getChanges().ranges);
        if (primitives.getChanges().activeList)
            record(RenderTable::ActivePrimitives, {});
        if (instances.getChanges().activeList)
            record(RenderTable::ActiveInstances, {});
        if (draws.getChanges().activeList)
            record(RenderTable::ActiveDraws, {});
        if (lights.getChanges().activeList)
            record(RenderTable::ActiveLights, {});
        if (cameras.getChanges().activeList)
            record(RenderTable::ActiveCameras, {});
    }

    void record(RenderTable table, Span<RenderRange> ranges)
    {
        if (!ranges.count && table <= RenderTable::Cameras)
            return;
        // Active-list changes have no sparse range, but must still occupy a journal entry.
        const uint32_t count = ranges.count ? ranges.count : 1;
        for (uint32_t i = 0; i < count; ++i)
        {
            if (journalCount == journal.size())
            {
                journalFloor = journal[journalFirst].revision;
                journalFirst = (journalFirst + 1) % journal.size();
                --journalCount;
            }
            journal[(journalFirst + journalCount++) % journal.size()] = { .revision = revision,
                                                                          .table = table,
                                                                          .range = ranges.count ? ranges.pData[i] : RenderRange{} };
        }
    }
    RenderSceneSync getChangesSince(uint64_t appliedRevision)
    {
        ASSERT(appliedRevision <= revision);
        for (Array<RenderRange>& ranges : syncRanges)
            ranges.clear();
        if (!appliedRevision || appliedRevision < journalFloor)
            return { .full = true };
        RenderSceneSync result;
        for (uint32_t i = 0; i < journalCount; ++i)
        {
            const RenderChange& change = journal[(journalFirst + i) % journal.size()];
            if (change.revision <= appliedRevision)
                continue;
            if (change.table <= RenderTable::Cameras)
                syncRanges[(uint32_t)change.table].pushBack(change.range);
            else if (change.table == RenderTable::ActivePrimitives)
                result.activePrimitives = true;
            else if (change.table == RenderTable::ActiveInstances)
                result.activeInstances = true;
            else if (change.table == RenderTable::ActiveDraws)
                result.activeDraws = true;
            else if (change.table == RenderTable::ActiveLights)
                result.activeLights = true;
            else if (change.table == RenderTable::ActiveCameras)
                result.activeCameras = true;
        }
        for (Array<RenderRange>& ranges : syncRanges)
            mergeRanges(ranges);
        result.primitives = { syncRanges[0].data(), syncRanges[0].size() };
        result.instances = { syncRanges[1].data(), syncRanges[1].size() };
        result.draws = { syncRanges[2].data(), syncRanges[2].size() };
        result.instanceIndices = { syncRanges[3].data(), syncRanges[3].size() };
        result.lights = { syncRanges[4].data(), syncRanges[4].size() };
        result.cameras = { syncRanges[5].data(), syncRanges[5].size() };
        return result;
    }
    void commitSnapshot(Span<RenderPrimitive> framePrimitives, Span<RenderInstance> frameInstances, Span<RenderCamera> frameCameras,
                        uint64_t epoch)
    {
        for (RenderEntity& entity : entities)
        {
            if (entity.historyEpoch > epoch || entity.primitive.index >= framePrimitives.count ||
                entity.instance.index >= frameInstances.count)
                continue;
            const RenderPrimitive& primitive = framePrimitives.pData[entity.primitive.index];
            const RenderInstance&  instance = frameInstances.pData[entity.instance.index];
            if (primitive.id != entity.primitive || !primitive.instanceCount || instance.id != entity.instance)
                continue;
            entity.submitted = instance.current;
            entity.historyValid = true;
        }
        for (RenderCameraEntity& entity : cameraEntities)
        {
            if (entity.historyEpoch > epoch || entity.id.index >= frameCameras.count)
                continue;
            const RenderCamera& camera = frameCameras.pData[entity.id.index];
            if (camera.id != entity.id)
                continue;
            entity.submitted = camera.current;
            entity.submittedLens = camera.lens;
            entity.historyValid = true;
        }
    }
    void resetHistory(Entity target)
    {
        ++historyEpoch;
        for (RenderEntity& entity : entities)
            if (!target.isValid() || entity.entity == target)
            {
                entity.historyValid = false;
                entity.historyEpoch = historyEpoch;
            }
        for (RenderCameraEntity& entity : cameraEntities)
            if (!target.isValid() || entity.entity == target)
            {
                entity.historyValid = false;
                entity.historyEpoch = historyEpoch;
            }
    }

    World&                                          world;
    AssetManager&                                   assetManager;
    WorldAssetBindings                              bindings;
    WorldQuery                                      query;
    WorldQuery                                      lightQuery;
    WorldQuery                                      cameraQuery;
    RenderSlots<RenderPrimitive, RenderPrimitiveID> primitives;
    RenderSlots<RenderInstance, RenderInstanceID>   instances;
    RenderSlots<RenderDraw, RenderDrawID>           draws;
    RenderSlots<RenderLight, RenderLightID>         lights;
    RenderSlots<RenderCamera, RenderCameraID>       cameras;
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
    uint64_t                                        revision = 0;
    uint64_t                                        historyEpoch = 0;
    Array<RenderChange>                             journal;
    Array<RenderRange>                              syncRanges[6];
    uint32_t                                        journalFirst = 0;
    uint32_t                                        journalCount = 0;
    uint64_t                                        journalFloor = 0;
};

RenderScene::RenderScene(World& world, AssetManager& assetManager, const RenderSceneDesc& desc):
    pData(tf_new(RenderSceneData, world, assetManager, desc))
{
}
RenderScene::~RenderScene() { tf_delete(pData); }
void RenderScene::reserve(uint32_t primitiveCount, uint32_t drawCount, uint32_t lightCount, uint32_t cameraCount)
{
    pData->primitives.reserve(primitiveCount);
    pData->instances.reserve(primitiveCount);
    pData->draws.reserve(drawCount);
    pData->entities.reserve(primitiveCount);
    pData->instanceIndices.reserve(primitiveCount);
    pData->indicesDirty.reserve(primitiveCount);
    pData->indicesRanges.reserve(primitiveCount);
    pData->lights.reserve(lightCount);
    pData->cameras.reserve(cameraCount);
    pData->lightEntities.reserve(lightCount);
    pData->cameraEntities.reserve(cameraCount);
    pData->textureReleases.reserve(lightCount);
}
void            RenderScene::update() { pData->update(); }
uint64_t        RenderScene::getRevision() const { return pData->revision; }
RenderSceneSync RenderScene::getChangesSince(uint64_t revision) { return pData->getChangesSince(revision); }
void            RenderScene::resetHistory(Entity entity) { pData->resetHistory(entity); }
uint64_t        RenderScene::getHistoryEpoch() const { return pData->historyEpoch; }
AssetSnapshot   RenderScene::captureAssets() { return pData->assetManager.captureRenderSnapshot(); }
void            RenderScene::commitSnapshot(Span<RenderPrimitive> primitives, Span<RenderInstance> instances, Span<RenderCamera> cameras,
                                            uint64_t epoch)
{
    pData->commitSnapshot(primitives, instances, cameras, epoch);
}
RenderSceneChanges RenderScene::getChanges() const
{
    return { .primitives = pData->primitives.getChanges(),
             .instances = pData->instances.getChanges(),
             .draws = pData->draws.getChanges(),
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
const RenderDraw*      RenderScene::getDraw(RenderDrawID id) const { return pData->draws.get(id); }
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
Span<RenderDraw>      RenderScene::getDraws() const { return { pData->draws.values.data(), pData->draws.values.size() }; }
Span<RenderLight>     RenderScene::getLights() const { return { pData->lights.values.data(), pData->lights.values.size() }; }
Span<RenderCamera>    RenderScene::getCameras() const { return { pData->cameras.values.data(), pData->cameras.values.size() }; }
Span<uint32_t> RenderScene::getActivePrimitives() const { return { pData->primitives.active.data(), pData->primitives.active.size() }; }
Span<uint32_t> RenderScene::getActiveInstances() const { return { pData->instances.active.data(), pData->instances.active.size() }; }
Span<uint32_t> RenderScene::getActiveDraws() const { return { pData->draws.active.data(), pData->draws.active.size() }; }
Span<uint32_t> RenderScene::getActiveLights() const { return { pData->lights.active.data(), pData->lights.active.size() }; }
Span<uint32_t> RenderScene::getActiveCameras() const { return { pData->cameras.active.data(), pData->cameras.active.size() }; }
Span<uint32_t> RenderScene::getInstanceIndices() const { return { pData->instanceIndices.data(), pData->instanceIndices.size() }; }
} // namespace hz
