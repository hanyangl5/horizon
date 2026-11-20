/* Copyright (c) 2026 Horizon */

#include "RenderStorage.h"
#include "Scene/WorldAssetBindings.h"
#include "Scene/MeshAsset.h"
#include "Scene/AnimationAssets.h"
#include "Scene/AnimationSystem.h"
#include "Scene/SkinningCore.h"
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
    return a.entityGuid == b.entityGuid && equalMatrix(a.transform, b.transform) && a.Type == b.Type && a.color[0] == b.color[0] &&
           a.color[1] == b.color[1] && a.color[2] == b.color[2] && a.intensity == b.intensity && a.range == b.range &&
           a.innerConeAngle == b.innerConeAngle && a.outerConeAngle == b.outerConeAngle && a.exposure == b.exposure &&
           a.angularDiameter == b.angularDiameter && a.Width == b.Width && a.Height == b.Height && a.radius == b.radius &&
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
    // SkinnedMesh skin index, or UINT32_MAX for a rigid mesh.
    uint32_t                 skin = UINT32_MAX;
    bool                     seen = false;
};
// Indexed by instance index. Pending marks entries whose submitted transform may need recording.
struct RenderHistory
{
    Matrix4 submitted = Matrix4::identity();
    bool    valid = false;
    bool    Reset = false;
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
    RenderSceneData(World& world, AssetManager& assetManager, const AnimationSystem* pAnimation):
        world(world), assetManager(assetManager), pAnimation(pAnimation), bindings(world, assetManager), changes(world), query(world, { SceneTypes::meshRenderer }),
        lightQuery(world, { SceneTypes::light }), cameraQuery(world, { SceneTypes::camera })
    {
        ASSERT(query.IsValid() && lightQuery.IsValid() && cameraQuery.IsValid());
    }
    ~RenderSceneData()
    {
        for (const RenderLightEntity& light : lightEntities)
            if (light.texture.IsValid())
            {
                assetManager.Release(light.texture);
            }
        hmfree(pIndices);
        hmfree(pLightIndices);
        hmfree(pCameraIndices);
    }

    static bool syncLight(Entity entity, Span<const void*> fields, void* User)
    {
        RenderSceneData&         data = *(RenderSceneData*)User;
        const RenderEntityIndex* pIndex = hmgetp_null(data.pLightIndices, entity);
        if (!pIndex)
        {
            RenderLightEntity& lightEntity = data.lightEntities.EmplaceBack();
            lightEntity.entity = entity;
            lightEntity.id = data.lights.Add();
            data.textureReleases.Reserve(data.lightEntities.Size());
            const RenderEntityIndex index = { .key = entity, .value = data.lightEntities.Size() - 1 };
            hmputs(data.pLightIndices, index);
            pIndex = hmgetp_null(data.pLightIndices, entity);
        }
        RenderLightEntity& lightEntity = data.lightEntities[pIndex->value];
        lightEntity.seen = true;
        const Light&  light = *(const Light*)fields.Data[0];
        const AssetID textureID = light.Type >= LightType::Rect ? light.texture : AssetID{};
        if (lightEntity.textureID != textureID)
        {
            if (lightEntity.texture.IsValid())
            {
                data.textureReleases.PushBack(lightEntity.texture);
            }
            lightEntity.textureID = textureID;
            lightEntity.texture = {};
            lightEntity.error = AssetError::None;
        }
        if (textureID.IsValid() && !lightEntity.texture.IsValid() &&
            (lightEntity.error == AssetError::None || lightEntity.error == AssetError::Retiring))
        {
            lightEntity.texture = data.assetManager.Request(textureID, &lightEntity.error);
        }
        return true;
    }
    void extractLights()
    {
        for (uint32_t i = 0; i < lightEntities.Size();)
        {
            RenderLightEntity& entity = lightEntities[i];
            if (!entity.seen)
            {
                if (entity.texture.IsValid())
                {
                    textureReleases.PushBack(entity.texture);
                }
                lights.Remove(entity.id);
                hmdel(pLightIndices, entity.entity);
                lightEntities[i] = lightEntities[lightEntities.Size() - 1];
                lightEntities.PopBack();
                if (i < lightEntities.Size())
                {
                    hmgetp_null(pLightIndices, lightEntities[i].entity)->value = i;
                }
                continue;
            }
            const Light& source = *(const Light*)world.GetComponent(entity.entity, SceneTypes::light);
            if (assetManager.GetState(entity.texture) == AssetState::Ready && !assetManager.GetTexture(entity.texture) &&
                entity.error != AssetError::LoadFailed)
            {
                entity.error = AssetError::LoadFailed;
                LOGF(Error, "Light emission map has the wrong asset type");
            }
            RenderLight next = {
                .id = entity.id,
                .entityGuid = world.GetEntityGuid(entity.entity),
                .transform = world.GetWorldTransform(entity.entity)->current,
                .Type = source.Type,
                .color = { (float)source.color.getX(), (float)source.color.getY(), (float)source.color.getZ() },
                .intensity = source.intensity,
                .range = source.range,
                .innerConeAngle = source.innerConeAngle,
                .outerConeAngle = source.outerConeAngle,
                .exposure = source.exposure,
                .angularDiameter = source.angularDiameter,
                .Width = source.Width,
                .Height = source.Height,
                .radius = source.radius,
                .length = source.length,
                .colorTemperature = source.colorTemperature,
                .enableColorTemperature = source.enableColorTemperature ? 1u : 0u,
                .normalize = source.normalize ? 1u : 0u,
                .twoSided = source.twoSided ? 1u : 0u,
                .ready = !entity.textureID.IsValid() || assetManager.GetTexture(entity.texture) ? 1u : 0u,
            };
            if (entity.textureID.IsValid() && next.ready)
            {
                next.texture = { .index = entity.texture.index, .generation = entity.texture.generation };
            }
            if (source.Type == LightType::Mesh)
            {
                const RenderEntityIndex* pIndex = hmgetp_null(pIndices, entity.entity);
                if (pIndex)
                    next.primitive = entities[pIndex->value].primitive;
                const RenderPrimitive* pPrimitive = primitives.Get(next.primitive);
                next.ready = next.ready && pPrimitive && pPrimitive->instanceCount ? 1u : 0u;
            }
            RenderLight& light = lights.values[entity.id.index];
            if (!equalLight(light, next))
            {
                light = next;
                lights.MarkDirty(entity.id.index);
            }
            ++i;
        }
        for (AssetHandle Handle : textureReleases)
        {
            assetManager.Release(Handle);
        }
        textureReleases.Clear();
    }
    static bool extractCamera(Entity entity, Span<const void*> fields, void* User)
    {
        RenderSceneData&         data = *(RenderSceneData*)User;
        const RenderEntityIndex* pIndex = hmgetp_null(data.pCameraIndices, entity);
        if (!pIndex)
        {
            RenderCameraEntity& cameraEntity = data.cameraEntities.EmplaceBack();
            cameraEntity.entity = entity;
            cameraEntity.id = data.cameras.Add();
            const RenderEntityIndex index = { .key = entity, .value = data.cameraEntities.Size() - 1 };
            hmputs(data.pCameraIndices, index);
            pIndex = hmgetp_null(data.pCameraIndices, entity);
        }
        RenderCameraEntity& cameraEntity = data.cameraEntities[pIndex->value];
        cameraEntity.seen = true;
        const Matrix4& current = data.world.GetWorldTransform(entity)->current;
        const Camera&  lens = *(const Camera*)fields.Data[0];
        const Matrix4& previous = cameraEntity.historyValid ? cameraEntity.submitted : current;
        const Camera&  previousLens = cameraEntity.historyValid ? cameraEntity.submittedLens : lens;
        const Guid     entityGuid = data.world.GetEntityGuid(entity);
        RenderCamera&  camera = data.cameras.values[cameraEntity.id.index];
        if (camera.entityGuid != entityGuid || !equalMatrix(camera.current, current) || !equalMatrix(camera.previous, previous) ||
            !equalLens(camera.lens, lens) || !equalLens(camera.previousLens, previousLens))
        {
            camera.entityGuid = entityGuid;
            camera.current = current;
            camera.previous = previous;
            camera.lens = lens;
            camera.previousLens = previousLens;
            data.cameras.MarkDirty(cameraEntity.id.index);
        }
        cameraEntity.historyReset = false;
        return true;
    }
    void removeCameras()
    {
        for (uint32_t i = 0; i < cameraEntities.Size();)
        {
            const RenderCameraEntity& entity = cameraEntities[i];
            if (entity.seen)
            {
                ++i;
                continue;
            }
            cameras.Remove(entity.id);
            hmdel(pCameraIndices, entity.entity);
            cameraEntities[i] = cameraEntities[cameraEntities.Size() - 1];
            cameraEntities.PopBack();
            if (i < cameraEntities.Size())
            {
                hmgetp_null(pCameraIndices, cameraEntities[i].entity)->value = i;
            }
        }
    }

    void removeMeshBatches(RenderEntity& entity)
    {
        for (RenderMeshBatchID meshBatch : entity.meshBatches)
        {
            meshBatches.Remove(meshBatch);
        }
        entity.meshBatches.Clear();
    }
    void setInstanceIndex(uint32_t index, uint32_t value)
    {
        if (instanceIndices.Size() <= index)
        {
            indicesDirty.Reserve(index + 1);
            indicesRanges.Reserve(index + 1);
            indicesPending.Reserve(index + 1);
        }
        while (instanceIndices.Size() <= index)
        {
            markIndex(instanceIndices.Size());
            instanceIndices.PushBack(UINT32_MAX);
            indicesPendingFlags.PushBack(true);
        }
        if (instanceIndices[index] != value)
        {
            instanceIndices[index] = value;
            markIndex(index);
        }
    }
    void markIndex(uint32_t index)
    {
        indicesDirty.PushBack(index);
        if (index >= indicesPendingFlags.Size() || !indicesPendingFlags[index])
        {
            if (index < indicesPendingFlags.Size())
            {
                indicesPendingFlags[index] = true;
            }
            indicesPending.PushBack(index);
        }
    }
    void markHistory(uint32_t instance)
    {
        RenderHistory& history = instanceHistory[instance];
        if (!history.pending)
        {
            history.pending = true;
            historyPending.PushBack(instance);
        }
    }
    RenderEntity& Add(Entity entity)
    {
        RenderEntity& renderEntity = entities.EmplaceBack();
        renderEntity.entity = entity;
        renderEntity.primitive = primitives.Add();
        renderEntity.instance = instances.Add();
        primitives.values[renderEntity.primitive.index].instanceOffset = renderEntity.primitive.index;
        instances.values[renderEntity.instance.index].primitive = renderEntity.primitive;
        if (instanceHistory.Size() <= renderEntity.instance.index)
        {
            historyPending.Reserve(renderEntity.instance.index + 1);
            historyFollowUp.Reserve(renderEntity.instance.index + 1);
            instanceHistory.Resize(renderEntity.instance.index + 1);
            instanceEntities.Resize(renderEntity.instance.index + 1);
        }
        instanceEntities[renderEntity.instance.index] = entity;
        instanceHistory[renderEntity.instance.index].valid = false;
        instanceHistory[renderEntity.instance.index].Reset = false;
        setInstanceIndex(renderEntity.primitive.index, UINT32_MAX);
        const RenderEntityIndex index = { .key = entity, .value = entities.Size() - 1 };
        hmputs(pIndices, index);
        return renderEntity;
    }
    void buildMeshBatches(RenderEntity& entity, const MeshAssetBinding& binding, const MeshAsset& mesh)
    {
        removeMeshBatches(entity);
        entity.meshBatches.Reserve(binding.materials.Count);
        entity.localBounds = binding.materials.Count ? mesh.GetBounds(0) : MeshBounds{};
        for (uint32_t i = 0; i < binding.materials.Count; ++i)
        {
            const MaterialAssetBinding& material = binding.materials.Data[i];
            const RenderMeshBatchID     id = meshBatches.Add();
            RenderMeshBatch&            meshBatch = meshBatches.values[id.index];
            meshBatch.primitive = entity.primitive;
            meshBatch.submesh = material.submesh;
            meshBatch.mesh = { .mesh = primitives.values[entity.primitive.index].mesh, .submeshIndex = i };
            meshBatch.material = { .index = material.Handle.index, .generation = material.Handle.generation };
            entity.meshBatches.PushBack(id);
            const MeshBounds& bounds = mesh.GetBounds(i);
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
        const SkinnedMesh* skinned = (const SkinnedMesh*)world.GetComponent(entity.entity, SceneTypes::skinnedMesh);
        const uint32_t     skin = skinned ? skinned->skin : UINT32_MAX;
        if ((skin == UINT32_MAX) != (entity.skin == UINT32_MAX))
        {
            if (skin == UINT32_MAX)
                removeSkinnedEntity(entity.entity);
            else
                skinnedEntities.PushBack(entity.entity);
        }
        entity.skin = skin;
        RenderPrimitive&        primitive = primitives.values[entity.primitive.index];
        const MeshAssetBinding* pBinding = bindings.Get(entity.entity);
        ASSERT(pBinding);
        const bool       ready = pBinding->state == AssetBindingState::Ready;
        const MeshAsset* pMesh = assetManager.GetMesh(pBinding->mesh);
        const GpuMeshID  mesh = pMesh ? GpuMeshID{ .index = pBinding->mesh.index, .generation = pBinding->mesh.generation } : GpuMeshID{};
        const uint32_t   Count = ready ? 1u : 0u;
        if (primitive.mesh != mesh)
        {
            removeMeshBatches(entity);
            entity.localBounds = {};
        }
        if (primitive.mesh != mesh || primitive.instanceCount != Count)
        {
            if (ready && (primitive.mesh != mesh || !primitive.instanceCount))
            {
                instanceHistory[entity.instance.index].valid = false;
                markHistory(entity.instance.index);
            }
            primitive.mesh = mesh;
            primitive.instanceCount = Count;
            primitives.MarkDirty(entity.primitive.index);
            setInstanceIndex(entity.primitive.index, ready ? entity.instance.index : UINT32_MAX);
        }
        if (ready && entity.meshBatches.Empty())
        {
            buildMeshBatches(entity, *pBinding, *pMesh);
        }
        if (ready)
        {
            ASSERT(entity.meshBatches.Size() == pBinding->materials.Count);
            for (uint32_t i = 0; i < entity.meshBatches.Size(); ++i)
            {
                RenderMeshBatch&    meshBatch = meshBatches.values[entity.meshBatches[i].index];
                const AssetHandle   Handle = pBinding->materials.Data[i].Handle;
                const GpuMaterialID material = { .index = Handle.index, .generation = Handle.generation };
                if (meshBatch.material != material)
                {
                    meshBatch.material = material;
                    meshBatches.MarkDirty(meshBatch.id.index);
                }
            }
        }
        else
            for (RenderMeshBatchID id : entity.meshBatches)
            {
                RenderMeshBatch& meshBatch = meshBatches.values[id.index];
                if (meshBatch.material.IsValid())
                {
                    meshBatch.material = {};
                    meshBatches.MarkDirty(id.index);
                }
            }
        const WorldTransform& transform = *world.GetWorldTransform(entity.entity);
        RenderInstance&       instance = instances.values[entity.instance.index];
        const Guid            entityGuid = world.GetEntityGuid(entity.entity);
        // updateSkins bounds skinned instances by their pose.
        const RenderBounds    bounds = !ready ? RenderBounds{}
                                       : entity.skin != UINT32_MAX ? instance.bounds
                                                                   : transformBounds(entity.localBounds, transform.current);
        RenderHistory&        history = instanceHistory[entity.instance.index];
        const Matrix4&        previous = history.valid ? history.submitted : transform.current;
        if (instance.entityGuid != entityGuid || !equalMatrix(instance.current, transform.current) ||
            !equalMatrix(instance.previous, previous) || !equalBounds(instance.bounds, bounds))
        {
            instance.entityGuid = entityGuid;
            instance.current = transform.current;
            instance.previous = previous;
            instance.bounds = bounds;
            instances.MarkDirty(instance.id.index);
            markHistory(entity.instance.index);
        }
        if (history.Reset)
        {
            history.Reset = false;
            markHistory(entity.instance.index);
        }
    }
    void removeSkinnedEntity(Entity entity)
    {
        for (uint32_t i = 0; i < skinnedEntities.Size(); ++i)
            if (skinnedEntities[i] == entity)
            {
                skinnedEntities[i] = skinnedEntities[skinnedEntities.Size() - 1];
                skinnedEntities.PopBack();
                return;
            }
    }
    static bool extractEntity(Entity entity, Span<const void*>, void* User)
    {
        RenderSceneData&         data = *(RenderSceneData*)User;
        const RenderEntityIndex* pIndex = hmgetp_null(data.pIndices, entity);
        RenderEntity&            renderEntity = pIndex ? data.entities[pIndex->value] : data.Add(entity);
        data.extract(renderEntity);
        return true;
    }
    void removeEntity(uint32_t i)
    {
        RenderEntity& entity = entities[i];
        if (entity.skin != UINT32_MAX)
            removeSkinnedEntity(entity.entity);
        removeMeshBatches(entity);
        setInstanceIndex(entity.primitive.index, UINT32_MAX);
        instanceEntities[entity.instance.index] = {};
        primitives.Remove(entity.primitive);
        instances.Remove(entity.instance);
        hmdel(pIndices, entity.entity);
        if (i + 1 < entities.Size())
        {
            entities[i] = std::move(entities[entities.Size() - 1]);
        }
        entities.PopBack();
        if (i < entities.Size())
        {
            hmgetp_null(pIndices, entities[i].entity)->value = i;
        }
    }
    // Extraction reads the MeshRenderer binding, the world transform, the Guid and asset states. The World change log names
    // entities whose components or transforms changed, the asset state revision covers asset loads, and history follow-ups
    // cover previous transforms that advanced on submission. Anything else would compare equal, so it is skipped.
    void extractMeshEntities()
    {
        bool               rescan = false;
        const Span<Entity> changed = changes.Drain(rescan);
        const uint64_t     assetRevision = assetManager.GetStateRevision();
        rescan |= assetRevision != extractedAssetRevision;
        extractedAssetRevision = assetRevision;
        if (rescan)
        {
            historyFollowUp.Clear();
            for (RenderEntity& entity : entities)
            {
                entity.seen = false;
            }
            query.Each(extractEntity, this);
            for (uint32_t i = 0; i < entities.Size();)
            {
                if (entities[i].seen)
                {
                    ++i;
                }
                else
                {
                    removeEntity(i);
                }
            }
            return;
        }
        for (uint32_t pass = 0; pass < 2; ++pass)
        {
            const Span<Entity> list = pass ? Span<Entity>{ historyFollowUp.GetData(), historyFollowUp.Size() } : changed;
            for (Entity entity : list)
            {
                if (!entity.IsValid())
                {
                    continue;
                }
                const RenderEntityIndex* pIndex = hmgetp_null(pIndices, entity);
                if (!world.GetComponent(entity, SceneTypes::meshRenderer))
                {
                    if (pIndex)
                    {
                        removeEntity(pIndex->value);
                    }
                    continue;
                }
                extract(pIndex ? entities[pIndex->value] : Add(entity));
            }
        }
        historyFollowUp.Clear();
    }
    // Skinned instances change every frame with their pose, so they are visited each Update instead of through the World
    // change log. A skin without a pose renders its bind pose and stays out of the skin list.
    void updateSkins()
    {
        skins.Clear();
        skinMatrices.Clear();
        for (Entity skinnedEntity : skinnedEntities)
        {
            const RenderEntity& entity = entities[hmgetp_null(pIndices, skinnedEntity)->value];
            if (entity.skin == UINT32_MAX || !primitives.values[entity.primitive.index].instanceCount)
            {
                continue;
            }
            const SkinningCore* pPose = pAnimation ? pAnimation->GetPose(world.GetParent(entity.entity)) : nullptr;
            MeshBounds          local = entity.localBounds;
            if (pPose && entity.skin < pPose->GetSkeleton().GetSkinCount())
            {
                const uint32_t jointCount = pPose->GetSkeleton().GetSkinJointCount(entity.skin);
                const uint32_t jointOffset = skinMatrices.Size();
                skinMatrices.Resize(jointOffset + jointCount);
                pPose->WriteSkinMatrices(entity.skin, skinMatrices.GetData() + jointOffset);
                skins.PushBack({ .instance = entity.instance, .jointOffset = jointOffset, .jointCount = jointCount });
                SkinBounds skinBounds;
                if (pPose->GetSkinBounds(entity.skin, skinBounds))
                {
                    memcpy(local.minimum, skinBounds.minimum, sizeof(local.minimum));
                    memcpy(local.maximum, skinBounds.maximum, sizeof(local.maximum));
                }
            }
            RenderInstance&    instance = instances.values[entity.instance.index];
            const RenderBounds bounds = transformBounds(local, instance.current);
            if (!equalBounds(instance.bounds, bounds))
            {
                instance.bounds = bounds;
                instances.MarkDirty(instance.id.index);
            }
        }
    }
    void Update()
    {
        PROFILER_SET_CPU_SCOPE_AUTO_NAMED("RenderScene::Update");
        ASSERT(!world.IsDeferred() && !frameLocked);
        {
            PROFILER_SET_CPU_SCOPE_AUTO_NAMED("RenderScene::UpdateTransforms");
            world.UpdateTransforms();
        }
        primitives.beginUpdate();
        instances.beginUpdate();
        meshBatches.beginUpdate();
        lights.beginUpdate();
        cameras.beginUpdate();
        indicesDirty.Clear();
        for (RenderLightEntity& entity : lightEntities)
            entity.seen = false;
        for (RenderCameraEntity& entity : cameraEntities)
            entity.seen = false;
        // Retain new emission maps before mesh/material bindings Release old dependencies.
        lightQuery.Each(syncLight, this);
        {
            PROFILER_SET_CPU_SCOPE_AUTO_NAMED("RenderScene::bindings");
            bindings.Update();
        }
        {
            PROFILER_SET_CPU_SCOPE_AUTO_NAMED("RenderScene::extract");
            extractMeshEntities();
            updateSkins();
        }
        extractLights();
        cameraQuery.Each(extractCamera, this);
        removeCameras();
        primitives.finishUpdate();
        instances.finishUpdate();
        meshBatches.finishUpdate();
        lights.finishUpdate();
        cameras.finishUpdate();
        mergeRenderRanges(indicesDirty, indicesRanges);
        if (primitives.GetChanges().ranges.Count || instances.GetChanges().ranges.Count || meshBatches.GetChanges().ranges.Count ||
            lights.GetChanges().ranges.Count || cameras.GetChanges().ranges.Count || !indicesRanges.Empty())
        {
            ++revision;
        }
    }
    RenderSceneUploads GetPendingUploads()
    {
        primitives.getPending(uploadRanges[0]);
        instances.getPending(uploadRanges[1]);
        meshBatches.getPending(uploadRanges[2]);
        mergeRenderRanges(indicesPending, uploadRanges[3]);
        lights.getPending(uploadRanges[4]);
        cameras.getPending(uploadRanges[5]);
        return { .primitives = { uploadRanges[0].GetData(), uploadRanges[0].Size() },
                 .instances = { uploadRanges[1].GetData(), uploadRanges[1].Size() },
                 .meshBatches = { uploadRanges[2].GetData(), uploadRanges[2].Size() },
                 .instanceIndices = { uploadRanges[3].GetData(), uploadRanges[3].Size() },
                 .lights = { uploadRanges[4].GetData(), uploadRanges[4].Size() },
                 .cameras = { uploadRanges[5].GetData(), uploadRanges[5].Size() },
                 .activePrimitives = primitives.isActivePending(),
                 .activeInstances = instances.isActivePending(),
                 .activeMeshBatches = meshBatches.isActivePending(),
                 .activeLights = lights.isActivePending(),
                 .activeCameras = cameras.isActivePending() };
    }
    void AcknowledgeUploads()
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
        indicesPending.Clear();
    }
    void CommitHistory()
    {
        PROFILER_SET_CPU_SCOPE_AUTO_NAMED("RenderScene::CommitHistory");
        for (uint32_t index : historyPending)
        {
            RenderHistory& history = instanceHistory[index];
            history.pending = false;
            const RenderInstance&  instance = instances.values[index];
            const RenderPrimitive* pPrimitive = primitives.Get(instance.primitive);
            // A Reset since the last Update renders previous == current first.
            if (history.Reset || !instance.id.IsValid() || !pPrimitive || !pPrimitive->instanceCount)
            {
                historyFollowUp.PushBack(instanceEntities[index]);
                continue;
            }
            history.submitted = instance.current;
            history.valid = true;
            // The next Update must see the new previous transform even if the entity does not change again.
            historyFollowUp.PushBack(instanceEntities[index]);
        }
        historyPending.Clear();
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
    void MarkDirty(Entity target)
    {
        ASSERT(!frameLocked);
        for (RenderEntity& entity : entities)
            if (!target.IsValid() || entity.entity == target)
            {
                instanceHistory[entity.instance.index].valid = false;
                instanceHistory[entity.instance.index].Reset = true;
                historyFollowUp.PushBack(entity.entity);
            }
        for (RenderCameraEntity& entity : cameraEntities)
            if (!target.IsValid() || entity.entity == target)
            {
                entity.historyValid = false;
                entity.historyReset = true;
            }
    }

    World&                                          world;
    AssetManager&                                   assetManager;
    const AnimationSystem*                          pAnimation = nullptr;
    WorldAssetBindings                              bindings;
    WorldChangeLog                                    changes;
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
    // Asset states seen by the last extraction; a change re-extracts every mesh entity.
    uint64_t                                          extractedAssetRevision = 0;
    // History submitted since the last Update changes each moved instance's previous transform once more.
    Array<Entity>                                     historyFollowUp;
    Array<Entity>                                     instanceEntities;
    Array<RenderRange>                              uploadRanges[6];
    // Mesh entities with a SkinnedMesh, visited by every Update.
    Array<Entity>                                   skinnedEntities;
    Array<RenderSkin>                               skins;
    Array<Matrix4>                                  skinMatrices;
    uint64_t                                        revision = 0;
    bool                                            frameLocked = false;
};

RenderScene::RenderScene(World& world, AssetManager& assetManager, const AnimationSystem* pAnimation):
    Data(MemNew(RenderSceneData, world, assetManager, pAnimation))
{
}
RenderScene::~RenderScene() { MemDelete(Data); }
void RenderScene::Reserve(uint32_t primitiveCount, uint32_t meshBatchCount, uint32_t lightCount, uint32_t cameraCount)
{
    Data->primitives.Reserve(primitiveCount);
    Data->instances.Reserve(primitiveCount);
    Data->meshBatches.Reserve(meshBatchCount);
    Data->entities.Reserve(primitiveCount);
    Data->instanceIndices.Reserve(primitiveCount);
    Data->indicesDirty.Reserve(primitiveCount);
    Data->indicesRanges.Reserve(primitiveCount);
    Data->indicesPending.Reserve(primitiveCount);
    Data->indicesPendingFlags.Reserve(primitiveCount);
    Data->instanceHistory.Reserve(primitiveCount);
    Data->historyPending.Reserve(primitiveCount);
    for (Array<RenderRange>& ranges : Data->uploadRanges)
    {
        ranges.Reserve(primitiveCount > meshBatchCount ? primitiveCount : meshBatchCount);
    }
    Data->lights.Reserve(lightCount);
    Data->cameras.Reserve(cameraCount);
    Data->lightEntities.Reserve(lightCount);
    Data->cameraEntities.Reserve(cameraCount);
    Data->textureReleases.Reserve(lightCount);
}
void               RenderScene::Update() { Data->Update(); }
uint64_t           RenderScene::GetRevision() const { return Data->revision; }
RenderSceneUploads RenderScene::GetPendingUploads() { return Data->GetPendingUploads(); }
void               RenderScene::AcknowledgeUploads() { Data->AcknowledgeUploads(); }
void               RenderScene::CommitHistory() { Data->CommitHistory(); }
void               RenderScene::SetFrameLocked(bool locked) { Data->frameLocked = locked; }
void               RenderScene::MarkDirty(Entity entity) { Data->MarkDirty(entity); }
RenderSceneChanges RenderScene::GetChanges() const
{
    return { .primitives = Data->primitives.GetChanges(),
             .instances = Data->instances.GetChanges(),
             .meshBatches = Data->meshBatches.GetChanges(),
             .lights = Data->lights.GetChanges(),
             .cameras = Data->cameras.GetChanges(),
             .instanceIndices = { Data->indicesRanges.GetData(), Data->indicesRanges.Size() } };
}
RenderPrimitiveID RenderScene::FindPrimitive(Entity entity) const
{
    const RenderEntityIndex* pIndex = hmgetp_null(Data->pIndices, entity);
    return pIndex ? Data->entities[pIndex->value].primitive : RenderPrimitiveID{};
}
const RenderPrimitive* RenderScene::GetPrimitive(RenderPrimitiveID id) const { return Data->primitives.Get(id); }
const RenderInstance*  RenderScene::GetInstance(RenderInstanceID id) const { return Data->instances.Get(id); }
const RenderMeshBatch* RenderScene::GetMeshBatch(RenderMeshBatchID id) const { return Data->meshBatches.Get(id); }
RenderLightID          RenderScene::FindLight(Entity entity) const
{
    const RenderEntityIndex* pIndex = hmgetp_null(Data->pLightIndices, entity);
    return pIndex ? Data->lightEntities[pIndex->value].id : RenderLightID{};
}
RenderCameraID RenderScene::FindCamera(Entity entity) const
{
    const RenderEntityIndex* pIndex = hmgetp_null(Data->pCameraIndices, entity);
    return pIndex ? Data->cameraEntities[pIndex->value].id : RenderCameraID{};
}
const RenderLight*    RenderScene::GetLight(RenderLightID id) const { return Data->lights.Get(id); }
const RenderCamera*   RenderScene::GetCamera(RenderCameraID id) const { return Data->cameras.Get(id); }
Span<RenderPrimitive> RenderScene::GetPrimitives() const { return { Data->primitives.values.GetData(), Data->primitives.values.Size() }; }
Span<RenderInstance>  RenderScene::GetInstances() const { return { Data->instances.values.GetData(), Data->instances.values.Size() }; }
Span<RenderMeshBatch> RenderScene::GetMeshBatches() const
{
    return { Data->meshBatches.values.GetData(), Data->meshBatches.values.Size() };
}
Span<RenderLight>  RenderScene::GetLights() const { return { Data->lights.values.GetData(), Data->lights.values.Size() }; }
Span<RenderCamera> RenderScene::GetCameras() const { return { Data->cameras.values.GetData(), Data->cameras.values.Size() }; }
Span<uint32_t> RenderScene::GetActivePrimitives() const { return { Data->primitives.active.GetData(), Data->primitives.active.Size() }; }
Span<uint32_t> RenderScene::GetActiveInstances() const { return { Data->instances.active.GetData(), Data->instances.active.Size() }; }
Span<uint32_t> RenderScene::GetActiveMeshBatches() const { return { Data->meshBatches.active.GetData(), Data->meshBatches.active.Size() }; }
Span<uint32_t> RenderScene::GetActiveLights() const { return { Data->lights.active.GetData(), Data->lights.active.Size() }; }
Span<uint32_t> RenderScene::GetActiveCameras() const { return { Data->cameras.active.GetData(), Data->cameras.active.Size() }; }
Span<uint32_t> RenderScene::GetInstanceIndices() const { return { Data->instanceIndices.GetData(), Data->instanceIndices.Size() }; }
Span<RenderSkin> RenderScene::GetSkins() const { return { Data->skins.GetData(), Data->skins.Size() }; }
Span<Matrix4>    RenderScene::GetSkinMatrices() const { return { Data->skinMatrices.GetData(), Data->skinMatrices.Size() }; }
} // namespace hz
