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
static bool EqualMatrix(const Matrix4& A, const Matrix4& B)
{
    for (uint32_t Column = 0; Column < 4; ++Column)
    {
        for (uint32_t Row = 0; Row < 4; ++Row)
        {
            if ((float)A.getElem(Column, Row) != (float)B.getElem(Column, Row))
            {
                return false;
            }
        }
    }
    return true;
}
static bool EqualBounds(const RenderBounds& A, const RenderBounds& B)
{
    for (uint32_t I = 0; I < 3; ++I)
    {
        if (A.minimum[I] != B.minimum[I] || A.maximum[I] != B.maximum[I])
        {
            return false;
        }
    }
    return true;
}
static bool EqualLens(const Camera& A, const Camera& B)
{
    return A.verticalFov == B.verticalFov && A.nearPlane == B.nearPlane && A.farPlane == B.farPlane;
}
static bool EqualLight(const RenderLight& A, const RenderLight& B)
{
    return A.entityGuid == B.entityGuid && EqualMatrix(A.transform, B.transform) && A.Type == B.Type && A.color[0] == B.color[0] &&
           A.color[1] == B.color[1] && A.color[2] == B.color[2] && A.intensity == B.intensity && A.InfluenceRadius == B.InfluenceRadius &&
           A.innerConeAngle == B.innerConeAngle && A.outerConeAngle == B.outerConeAngle && A.Width == B.Width && A.Height == B.Height &&
           A.colorTemperature == B.colorTemperature && A.enableColorTemperature == B.enableColorTemperature &&
           A.CastShadows == B.CastShadows && A.CastRaytracedShadow == B.CastRaytracedShadow && A.ShadowConeRadius == B.ShadowConeRadius &&
           A.DiffuseScale == B.DiffuseScale && A.SourceRadius == B.SourceRadius && A.SoftSourceRadius == B.SoftSourceRadius &&
           A.SourceLength == B.SourceLength && A.SkyAngle == B.SkyAngle && A.SpecularScale == B.SpecularScale &&
           A.AffectTranslucency == B.AffectTranslucency && A.AffectReflection == B.AffectReflection && A.texture == B.texture &&
           A.ready == B.ready;
}

constexpr float CmToMeters = 0.01f;
constexpr float DegreesToRadians = PI / 180.0f;

static void ExtractLight(const LightComponentBase& Source, RenderLight& Light)
{
    Light.color[0] = (float)Source.LightColor.getX();
    Light.color[1] = (float)Source.LightColor.getY();
    Light.color[2] = (float)Source.LightColor.getZ();
    Light.intensity = Source.Intensity;
    Light.CastShadows = Source.CastShadows && Source.CastDynamicShadows;
    Light.CastRaytracedShadow = Source.CastRaytracedShadow;
    Light.AffectTranslucency = Source.bAffectTranslucentLighting;
    Light.AffectReflection = Source.bAffectReflection;
    Light.ready = Source.bAffectsWorld;
}

static void ExtractLight(const LightComponent& Source, RenderLight& Light)
{
    ExtractLight(Source.Base, Light);
    Light.colorTemperature = Source.Temperature;
    Light.enableColorTemperature = Source.bUseTemperature;
    Light.DiffuseScale = Source.DiffuseScale;
    Light.SpecularScale = Source.SpecularScale;
}

static void ExtractLight(const LocalLightComponent& Source, RenderLight& Light)
{
    ExtractLight(Source.Common, Light);
    Light.InfluenceRadius = fmaxf(Source.AttenuationRadius, 0.0f) * CmToMeters;

    switch (Source.IntensityUnits)
    {
    case LightUnits::Candelas:
        break;
    case LightUnits::EV:
        Light.intensity = exp2f(Light.intensity);
        break;
    case LightUnits::Lumens:
        if (Light.Type == LightType::Spot)
        {
            Light.intensity /= 2.0f * PI * fmaxf(1.0f - cosf(Light.outerConeAngle), 1e-7f);
        }
        else
        {
            Light.intensity /= Light.Type == LightType::Rect ? PI : 4.0f * PI;
        }
        break;
    case LightUnits::Nits:
        Light.intensity *= Light.Type == LightType::Rect
                               ? Light.Width * Light.Height
                               : 4.0f * PI * Light.SourceRadius * (Light.SourceRadius + 0.5f * Light.SourceLength);
        break;
    case LightUnits::Unitless:
        Light.intensity *= 0.0016f;
        break;
    }
}

static void ExtractLight(const PointLightComponent& Source, RenderLight& Light)
{
    Light.SourceRadius = fmaxf(Source.SourceRadius, 0.0f) * CmToMeters;
    Light.SoftSourceRadius = fmaxf(Source.SoftSourceRadius, 0.0f) * CmToMeters;
    Light.SourceLength = fmaxf(Source.SourceLength, 0.0f) * CmToMeters;
    ExtractLight(Source.Local, Light);
}

static AssetID ExtractLight(LightType Type, const void* Component, RenderLight& Light)
{
    switch (Type)
    {
    case LightType::Directional:
    {
        const DirectionalLightComponent& Sun = *(const DirectionalLightComponent*)Component;
        ExtractLight(Sun.Common, Light);
        const float HalfAngle = fminf(fmaxf(Sun.LightSourceAngle, 0.0f), 180.0f) * (0.5f * DegreesToRadians);
        Light.ShadowConeRadius = fminf(sinf(HalfAngle) * fmaxf(Sun.ShadowSourceAngleFactor, 0.0f), 1.0f);
        break;
    }
    case LightType::Point:
        ExtractLight(*(const PointLightComponent*)Component, Light);
        break;
    case LightType::Spot:
    {
        const SpotLightComponent& Spot = *(const SpotLightComponent*)Component;
        Light.innerConeAngle = fminf(fmaxf(Spot.InnerConeAngle, 0.0f), 88.9f) * DegreesToRadians;
        Light.outerConeAngle =
            fminf(fmaxf(Spot.OuterConeAngle * DegreesToRadians, Light.innerConeAngle + 0.001f), 88.9f * DegreesToRadians + 0.001f);
        ExtractLight(Spot.Point, Light);
        break;
    }
    case LightType::Rect:
    {
        const RectLightComponent& Rect = *(const RectLightComponent*)Component;
        Light.Width = fmaxf(Rect.SourceWidth, 0.0f) * CmToMeters;
        Light.Height = fmaxf(Rect.SourceHeight, 0.0f) * CmToMeters;
        ExtractLight(Rect.Local, Light);
        return Rect.SourceTexture;
    }
    case LightType::Sky:
    {
        const SkyLightComponent& Sky = *(const SkyLightComponent*)Component;
        ExtractLight(Sky.Base, Light);
        Light.SkyAngle = Sky.SourceCubemapAngle * DegreesToRadians;
        if (Sky.SourceType == SkyLightSourceType::SpecifiedCubemap)
        {
            Light.ready = Light.ready && Sky.Cubemap.IsValid();
            return Sky.Cubemap;
        }
        break;
    }
    }
    return {};
}

static RenderBounds TransformBounds(const MeshBounds& Bounds, const Matrix4& Matrix)
{
    RenderBounds Result;
    for (uint32_t Row = 0; Row < 3; ++Row)
    {
        float MinimumValue = (float)Matrix.getElem(3, Row), MaximumValue = MinimumValue;
        for (uint32_t Column = 0; Column < 3; ++Column)
        {
            const float A = (float)Matrix.getElem(Column, Row) * Bounds.minimum[Column];
            const float B = (float)Matrix.getElem(Column, Row) * Bounds.maximum[Column];
            MinimumValue += fminf(A, B);
            MaximumValue += fmaxf(A, B);
        }
        Result.minimum[Row] = MinimumValue;
        Result.maximum[Row] = MaximumValue;
    }
    return Result;
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
    LightType     Type;
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
    RenderSceneData(World& WorldData, AssetManager& AssetManagerValue, const AnimationSystem* Animation):
        world(WorldData), assetManager(AssetManagerValue), animation(Animation), bindings(WorldData, AssetManagerValue),
        changes(WorldData, SceneTypes::meshRenderer), query(WorldData, { SceneTypes::meshRenderer }),
        LightQueries{ { WorldData, { SceneTypes::directionalLight } },
                      { WorldData, { SceneTypes::pointLight } },
                      { WorldData, { SceneTypes::spotLight } },
                      { WorldData, { SceneTypes::rectLight } },
                      { WorldData, { SceneTypes::skyLight } } },
        cameraQuery(WorldData, { SceneTypes::camera })
    {
        for (const WorldQuery& Query : LightQueries)
        {
            ASSERT(Query.IsValid());
        }
        ASSERT(query.IsValid() && cameraQuery.IsValid());
    }
    ~RenderSceneData()
    {
        for (const RenderLightEntity& LightValue : lightEntities)
        {
            if (LightValue.texture.IsValid())
            {
                assetManager.Release(LightValue.texture);
            }
        }
        hmfree(pIndices);
        for (RenderEntityIndex*& Indices : LightIndices)
        {
            hmfree(Indices);
        }
        hmfree(pCameraIndices);
    }

    struct LightQueryContext
    {
        RenderSceneData& Scene;
        LightType        Type;
    };

    static bool syncLight(Entity EntityHandle, Span<const void*> Fields, void* User)
    {
        RenderSceneData&         DataValue = static_cast<LightQueryContext*>(User)->Scene;
        const LightType          Type = static_cast<LightQueryContext*>(User)->Type;
        RenderEntityIndex*&      Indices = DataValue.LightIndices[(uint32_t)Type];
        const RenderEntityIndex* IndexPointer = hmgetp_null(Indices, EntityHandle);
        if (!IndexPointer)
        {
            RenderLightEntity& LightEntity = DataValue.lightEntities.EmplaceBack();
            LightEntity.entity = EntityHandle;
            LightEntity.Type = Type;
            LightEntity.id = DataValue.lights.Add();
            DataValue.textureReleases.Reserve(DataValue.lightEntities.Size());
            const RenderEntityIndex IndexValue = { .key = EntityHandle, .value = DataValue.lightEntities.Size() - 1 };
            hmputs(Indices, IndexValue);
            IndexPointer = hmgetp_null(Indices, EntityHandle);
        }
        RenderLightEntity& LightEntity = DataValue.lightEntities[IndexPointer->value];
        LightEntity.seen = true;
        RenderLight   Next = { .id = LightEntity.id,
                               .entityGuid = DataValue.world.GetEntityGuid(EntityHandle),
                               .transform = DataValue.world.GetWorldTransform(EntityHandle)->current,
                               .Type = Type };
        const AssetID TextureId = ExtractLight(Type, Fields.Data[0], Next);
        Next.intensity = fmaxf(Next.intensity, 0.0f);
        if (LightEntity.textureID != TextureId)
        {
            if (LightEntity.texture.IsValid())
            {
                DataValue.textureReleases.PushBack(LightEntity.texture);
            }
            LightEntity.textureID = TextureId;
            LightEntity.texture = {};
            LightEntity.error = AssetError::None;
        }
        if (TextureId.IsValid() && !LightEntity.texture.IsValid() &&
            (LightEntity.error == AssetError::None || LightEntity.error == AssetError::Retiring))
        {
            LightEntity.texture = DataValue.assetManager.Request(TextureId, &LightEntity.error);
        }
        if (DataValue.assetManager.GetState(LightEntity.texture) == AssetState::Ready &&
            !DataValue.assetManager.GetTexture(LightEntity.texture) && LightEntity.error != AssetError::LoadFailed)
        {
            LightEntity.error = AssetError::LoadFailed;
            LOGF(Error, "Light emission map has the wrong asset type");
        }
        Next.ready = Next.ready && (!TextureId.IsValid() || DataValue.assetManager.GetTexture(LightEntity.texture));
        if (LightEntity.textureID.IsValid() && Next.ready)
        {
            Next.texture = { .index = LightEntity.texture.index, .generation = LightEntity.texture.generation };
        }
        RenderLight& LightValue = DataValue.lights.Values[LightEntity.id.index];
        if (!EqualLight(LightValue, Next))
        {
            DataValue.LocalLightMembershipPending |= LightValue.IsActiveLocalLight() != Next.IsActiveLocalLight();
            LightValue = Next;
            DataValue.lights.MarkDirty(LightEntity.id.index);
        }
        return true;
    }
    void removeMissingLights()
    {
        for (uint32_t I = 0; I < lightEntities.Size();)
        {
            RenderLightEntity& EntityHandle = lightEntities[I];
            if (!EntityHandle.seen)
            {
                if (EntityHandle.texture.IsValid())
                {
                    textureReleases.PushBack(EntityHandle.texture);
                }
                lights.Remove(EntityHandle.id);
                hmdel(LightIndices[(uint32_t)EntityHandle.Type], EntityHandle.entity);
                lightEntities[I] = lightEntities[lightEntities.Size() - 1];
                lightEntities.PopBack();
                if (I < lightEntities.Size())
                {
                    hmgetp_null(LightIndices[(uint32_t)lightEntities[I].Type], lightEntities[I].entity)->value = I;
                }
                continue;
            }
            ++I;
        }
        for (AssetHandle Handle : textureReleases)
        {
            assetManager.Release(Handle);
        }
        textureReleases.Clear();
    }
    static bool extractCamera(Entity EntityHandle, Span<const void*> Fields, void* User)
    {
        RenderSceneData&         DataValue = *(RenderSceneData*)User;
        const RenderEntityIndex* IndexPointer = hmgetp_null(DataValue.pCameraIndices, EntityHandle);
        if (!IndexPointer)
        {
            RenderCameraEntity& CameraEntity = DataValue.cameraEntities.EmplaceBack();
            CameraEntity.entity = EntityHandle;
            CameraEntity.id = DataValue.cameras.Add();
            const RenderEntityIndex IndexValue = { .key = EntityHandle, .value = DataValue.cameraEntities.Size() - 1 };
            hmputs(DataValue.pCameraIndices, IndexValue);
            IndexPointer = hmgetp_null(DataValue.pCameraIndices, EntityHandle);
        }
        RenderCameraEntity& CameraEntity = DataValue.cameraEntities[IndexPointer->value];
        CameraEntity.seen = true;
        const Matrix4& Current = DataValue.world.GetWorldTransform(EntityHandle)->current;
        const Camera&  Lens = *(const Camera*)Fields.Data[0];
        const Matrix4& Previous = CameraEntity.historyValid ? CameraEntity.submitted : Current;
        const Camera&  PreviousLens = CameraEntity.historyValid ? CameraEntity.submittedLens : Lens;
        const Guid     EntityGuid = DataValue.world.GetEntityGuid(EntityHandle);
        RenderCamera&  CameraData = DataValue.cameras.Values[CameraEntity.id.index];
        if (CameraData.entityGuid != EntityGuid || !EqualMatrix(CameraData.current, Current) ||
            !EqualMatrix(CameraData.previous, Previous) || !EqualLens(CameraData.lens, Lens) ||
            !EqualLens(CameraData.previousLens, PreviousLens))
        {
            CameraData.entityGuid = EntityGuid;
            CameraData.current = Current;
            CameraData.previous = Previous;
            CameraData.lens = Lens;
            CameraData.previousLens = PreviousLens;
            DataValue.cameras.MarkDirty(CameraEntity.id.index);
        }
        CameraEntity.historyReset = false;
        return true;
    }
    void removeCameras()
    {
        for (uint32_t I = 0; I < cameraEntities.Size();)
        {
            const RenderCameraEntity& EntityHandle = cameraEntities[I];
            if (EntityHandle.seen)
            {
                ++I;
                continue;
            }
            cameras.Remove(EntityHandle.id);
            hmdel(pCameraIndices, EntityHandle.entity);
            cameraEntities[I] = cameraEntities[cameraEntities.Size() - 1];
            cameraEntities.PopBack();
            if (I < cameraEntities.Size())
            {
                hmgetp_null(pCameraIndices, cameraEntities[I].entity)->value = I;
            }
        }
    }

    void removeMeshBatches(RenderEntity& EntityHandle)
    {
        for (RenderMeshBatchID MeshBatch : EntityHandle.meshBatches)
        {
            meshBatches.Remove(MeshBatch);
        }
        EntityHandle.meshBatches.Clear();
    }
    void setInstanceIndex(uint32_t IndexValue, uint32_t Value)
    {
        if (instanceIndices.Size() <= IndexValue)
        {
            indicesDirty.Reserve(IndexValue + 1);
            indicesRanges.Reserve(IndexValue + 1);
            indicesPending.Reserve(IndexValue + 1);
        }
        while (instanceIndices.Size() <= IndexValue)
        {
            markIndex(instanceIndices.Size());
            instanceIndices.PushBack(UINT32_MAX);
            indicesPendingFlags.PushBack(true);
        }
        if (instanceIndices[IndexValue] != Value)
        {
            instanceIndices[IndexValue] = Value;
            markIndex(IndexValue);
        }
    }
    void markIndex(uint32_t IndexValue)
    {
        indicesDirty.PushBack(IndexValue);
        if (IndexValue >= indicesPendingFlags.Size() || !indicesPendingFlags[IndexValue])
        {
            if (IndexValue < indicesPendingFlags.Size())
            {
                indicesPendingFlags[IndexValue] = true;
            }
            indicesPending.PushBack(IndexValue);
        }
    }
    void markHistory(uint32_t Instance)
    {
        RenderHistory& History = instanceHistory[Instance];
        if (!History.pending)
        {
            History.pending = true;
            historyPending.PushBack(Instance);
        }
    }
    RenderEntity& Add(Entity EntityHandle)
    {
        RenderEntity& RenderEntityValue = entities.EmplaceBack();
        RenderEntityValue.entity = EntityHandle;
        RenderEntityValue.primitive = primitives.Add();
        RenderEntityValue.instance = instances.Add();
        primitives.Values[RenderEntityValue.primitive.index].instanceOffset = RenderEntityValue.primitive.index;
        instances.Values[RenderEntityValue.instance.index].primitive = RenderEntityValue.primitive;
        if (instanceHistory.Size() <= RenderEntityValue.instance.index)
        {
            historyPending.Reserve(RenderEntityValue.instance.index + 1);
            historyFollowUp.Reserve(RenderEntityValue.instance.index + 1);
            instanceHistory.Resize(RenderEntityValue.instance.index + 1);
            instanceEntities.Resize(RenderEntityValue.instance.index + 1);
        }
        instanceEntities[RenderEntityValue.instance.index] = EntityHandle;
        instanceHistory[RenderEntityValue.instance.index].valid = false;
        instanceHistory[RenderEntityValue.instance.index].Reset = false;
        setInstanceIndex(RenderEntityValue.primitive.index, UINT32_MAX);
        const RenderEntityIndex IndexValue = { .key = EntityHandle, .value = entities.Size() - 1 };
        hmputs(pIndices, IndexValue);
        return RenderEntityValue;
    }
    void buildMeshBatches(RenderEntity& EntityHandle, const MeshAssetBinding& BindingValue, const MeshAsset& MeshValue)
    {
        removeMeshBatches(EntityHandle);
        EntityHandle.meshBatches.Reserve(BindingValue.materials.Count);
        EntityHandle.localBounds = BindingValue.materials.Count ? MeshValue.GetBounds(0) : MeshBounds{};
        for (uint32_t I = 0; I < BindingValue.materials.Count; ++I)
        {
            const MaterialAssetBinding& MaterialValue = BindingValue.materials.Data[I];
            const RenderMeshBatchID     Id = meshBatches.Add();
            RenderMeshBatch&            MeshBatch = meshBatches.Values[Id.index];
            MeshBatch.primitive = EntityHandle.primitive;
            MeshBatch.submesh = MaterialValue.submesh;
            MeshBatch.mesh = { .mesh = primitives.Values[EntityHandle.primitive.index].mesh, .submeshIndex = I };
            MeshBatch.material = { .index = MaterialValue.Handle.index, .generation = MaterialValue.Handle.generation };
            EntityHandle.meshBatches.PushBack(Id);
            const MeshBounds& Bounds = MeshValue.GetBounds(I);
            for (uint32_t Axis = 0; Axis < 3; ++Axis)
            {
                EntityHandle.localBounds.minimum[Axis] = fminf(EntityHandle.localBounds.minimum[Axis], Bounds.minimum[Axis]);
                EntityHandle.localBounds.maximum[Axis] = fmaxf(EntityHandle.localBounds.maximum[Axis], Bounds.maximum[Axis]);
            }
        }
    }
    void extract(RenderEntity& EntityHandle)
    {
        EntityHandle.seen = true;
        const SkinnedMesh* Skinned = (const SkinnedMesh*)world.GetComponent(EntityHandle.entity, SceneTypes::skinnedMesh);
        const uint32_t     Skin = Skinned ? Skinned->skin : UINT32_MAX;
        if ((Skin == UINT32_MAX) != (EntityHandle.skin == UINT32_MAX))
        {
            if (Skin == UINT32_MAX)
            {
                removeSkinnedEntity(EntityHandle.entity);
            }
            else
            {
                skinnedEntities.PushBack(EntityHandle.entity);
            }
        }
        EntityHandle.skin = Skin;
        RenderPrimitive&        PrimitiveValue = primitives.Values[EntityHandle.primitive.index];
        const MeshAssetBinding* BindingPointer = bindings.Get(EntityHandle.entity);
        ASSERT(BindingPointer);
        const bool       ReadyValue = BindingPointer->state == AssetBindingState::Ready;
        const MeshAsset* MeshPointer = assetManager.GetMesh(BindingPointer->mesh);
        const GpuMeshID  MeshValue =
            MeshPointer ? GpuMeshID{ .index = BindingPointer->mesh.index, .generation = BindingPointer->mesh.generation } : GpuMeshID{};
        const uint32_t Count = ReadyValue ? 1u : 0u;
        if (PrimitiveValue.mesh != MeshValue)
        {
            removeMeshBatches(EntityHandle);
            EntityHandle.localBounds = {};
        }
        if (PrimitiveValue.mesh != MeshValue || PrimitiveValue.instanceCount != Count)
        {
            if (ReadyValue && (PrimitiveValue.mesh != MeshValue || !PrimitiveValue.instanceCount))
            {
                instanceHistory[EntityHandle.instance.index].valid = false;
                markHistory(EntityHandle.instance.index);
            }
            PrimitiveValue.mesh = MeshValue;
            PrimitiveValue.instanceCount = Count;
            primitives.MarkDirty(EntityHandle.primitive.index);
            setInstanceIndex(EntityHandle.primitive.index, ReadyValue ? EntityHandle.instance.index : UINT32_MAX);
        }
        if (ReadyValue && EntityHandle.meshBatches.Empty())
        {
            buildMeshBatches(EntityHandle, *BindingPointer, *MeshPointer);
        }
        if (ReadyValue)
        {
            ASSERT(EntityHandle.meshBatches.Size() == BindingPointer->materials.Count);
            for (uint32_t I = 0; I < EntityHandle.meshBatches.Size(); ++I)
            {
                RenderMeshBatch&    MeshBatch = meshBatches.Values[EntityHandle.meshBatches[I].index];
                const AssetHandle   Handle = BindingPointer->materials.Data[I].Handle;
                const GpuMaterialID MaterialValue = { .index = Handle.index, .generation = Handle.generation };
                if (MeshBatch.material != MaterialValue)
                {
                    MeshBatch.material = MaterialValue;
                    meshBatches.MarkDirty(MeshBatch.id.index);
                }
            }
        }
        else
        {
            for (RenderMeshBatchID Id : EntityHandle.meshBatches)
            {
                RenderMeshBatch& MeshBatch = meshBatches.Values[Id.index];
                if (MeshBatch.material.IsValid())
                {
                    MeshBatch.material = {};
                    meshBatches.MarkDirty(Id.index);
                }
            }
        }
        const WorldTransform& Transform = *world.GetWorldTransform(EntityHandle.entity);
        RenderInstance&       Instance = instances.Values[EntityHandle.instance.index];
        const Guid            EntityGuid = world.GetEntityGuid(EntityHandle.entity);
        // updateSkins bounds skinned instances by their pose.
        const RenderBounds    Bounds = !ReadyValue                       ? RenderBounds{}
                                       : EntityHandle.skin != UINT32_MAX ? Instance.bounds
                                                                         : TransformBounds(EntityHandle.localBounds, Transform.current);
        RenderHistory&        History = instanceHistory[EntityHandle.instance.index];
        const Matrix4&        Previous = History.valid ? History.submitted : Transform.current;
        if (Instance.entityGuid != EntityGuid || !EqualMatrix(Instance.current, Transform.current) ||
            !EqualMatrix(Instance.previous, Previous) || !EqualBounds(Instance.bounds, Bounds))
        {
            Instance.entityGuid = EntityGuid;
            Instance.current = Transform.current;
            Instance.previous = Previous;
            Instance.bounds = Bounds;
            instances.MarkDirty(Instance.id.index);
            markHistory(EntityHandle.instance.index);
        }
        if (History.Reset)
        {
            History.Reset = false;
            markHistory(EntityHandle.instance.index);
        }
    }
    void removeSkinnedEntity(Entity EntityHandle)
    {
        for (uint32_t I = 0; I < skinnedEntities.Size(); ++I)
        {
            if (skinnedEntities[I] == EntityHandle)
            {
                skinnedEntities[I] = skinnedEntities[skinnedEntities.Size() - 1];
                skinnedEntities.PopBack();
                return;
            }
        }
    }
    static bool extractEntity(Entity EntityHandle, Span<const void*>, void* User)
    {
        RenderSceneData&         DataValue = *(RenderSceneData*)User;
        const RenderEntityIndex* IndexPointer = hmgetp_null(DataValue.pIndices, EntityHandle);
        RenderEntity&            RenderEntityValue = IndexPointer ? DataValue.entities[IndexPointer->value] : DataValue.Add(EntityHandle);
        DataValue.extract(RenderEntityValue);
        return true;
    }
    void removeEntity(uint32_t I)
    {
        RenderEntity& EntityHandle = entities[I];
        if (EntityHandle.skin != UINT32_MAX)
        {
            removeSkinnedEntity(EntityHandle.entity);
        }
        removeMeshBatches(EntityHandle);
        setInstanceIndex(EntityHandle.primitive.index, UINT32_MAX);
        instanceEntities[EntityHandle.instance.index] = {};
        primitives.Remove(EntityHandle.primitive);
        instances.Remove(EntityHandle.instance);
        hmdel(pIndices, EntityHandle.entity);
        if (I + 1 < entities.Size())
        {
            entities[I] = std::move(entities[entities.Size() - 1]);
        }
        entities.PopBack();
        if (I < entities.Size())
        {
            hmgetp_null(pIndices, entities[I].entity)->value = I;
        }
    }
    void ExtractChangedMeshes(Span<Entity> List)
    {
        for (Entity EntityHandle : List)
        {
            if (!EntityHandle.IsValid())
            {
                continue;
            }
            const RenderEntityIndex* IndexPointer = hmgetp_null(pIndices, EntityHandle);
            if (!world.GetComponent(EntityHandle, SceneTypes::meshRenderer))
            {
                if (IndexPointer)
                {
                    removeEntity(IndexPointer->value);
                }
                continue;
            }
            extract(IndexPointer ? entities[IndexPointer->value] : Add(EntityHandle));
        }
    }
    // Extraction reads the MeshRenderer binding, the world transform, the Guid and asset states. The World change log names
    // entities whose components or transforms changed, the asset state revision covers asset loads, and history follow-ups
    // cover previous transforms that advanced on submission. Anything else would compare equal, so it is skipped.
    void extractMeshEntities()
    {
        bool               Rescan = false;
        const Span<Entity> Changed = changes.Drain(Rescan);
        const uint64_t     AssetRevision = assetManager.GetStateRevision();
        Rescan |= AssetRevision != extractedAssetRevision;
        extractedAssetRevision = AssetRevision;
        if (Rescan)
        {
            PROFILER_SET_CPU_SCOPE_AUTO_NAMED("RenderScene::RescanMeshes");
            historyFollowUp.Clear();
            for (RenderEntity& EntityHandle : entities)
            {
                EntityHandle.seen = false;
            }
            query.Each(extractEntity, this);
            for (uint32_t I = 0; I < entities.Size();)
            {
                if (entities[I].seen)
                {
                    ++I;
                }
                else
                {
                    removeEntity(I);
                }
            }
            return;
        }
        {
            PROFILER_SET_CPU_SCOPE_AUTO_NAMED("RenderScene::ExtractChangedMeshes");
            ExtractChangedMeshes(Changed);
        }
        {
            PROFILER_SET_CPU_SCOPE_AUTO_NAMED("RenderScene::ExtractMeshHistory");
            ExtractChangedMeshes({ historyFollowUp.GetData(), historyFollowUp.Size() });
        }
        historyFollowUp.Clear();
    }
    // Skinned instances change every frame with their pose, so they are visited each Update instead of through the World
    // change log. A skin without a pose renders its bind pose and stays out of the skin list.
    void updateSkins()
    {
        PROFILER_SET_CPU_SCOPE_AUTO_NAMED("RenderScene::UpdateSkins");
        skins.Clear();
        skinMatrices.Clear();
        for (Entity SkinnedEntity : skinnedEntities)
        {
            const RenderEntity& EntityHandle = entities[hmgetp_null(pIndices, SkinnedEntity)->value];
            if (EntityHandle.skin == UINT32_MAX || !primitives.Values[EntityHandle.primitive.index].instanceCount)
            {
                continue;
            }
            const SkinningCore* Pose = animation ? animation->GetPose(world.GetParent(EntityHandle.entity)) : nullptr;
            MeshBounds          Local = EntityHandle.localBounds;
            if (Pose && EntityHandle.skin < Pose->GetSkeleton().GetSkinCount())
            {
                const uint32_t JointCount = Pose->GetSkeleton().GetSkinJointCount(EntityHandle.skin);
                const uint32_t JointOffset = skinMatrices.Size();
                skinMatrices.Resize(JointOffset + JointCount);
                Pose->WriteSkinMatrices(EntityHandle.skin, skinMatrices.GetData() + JointOffset);
                skins.PushBack({ .instance = EntityHandle.instance, .jointOffset = JointOffset, .jointCount = JointCount });
                SkinBounds SkinBoundsValue;
                if (Pose->GetSkinBounds(EntityHandle.skin, SkinBoundsValue))
                {
                    memcpy(Local.minimum, SkinBoundsValue.Minimum, sizeof(Local.minimum));
                    memcpy(Local.maximum, SkinBoundsValue.Maximum, sizeof(Local.maximum));
                }
            }
            RenderInstance&    Instance = instances.Values[EntityHandle.instance.index];
            const RenderBounds Bounds = TransformBounds(Local, Instance.current);
            if (!EqualBounds(Instance.bounds, Bounds))
            {
                Instance.bounds = Bounds;
                instances.MarkDirty(Instance.id.index);
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
        primitives.BeginUpdate();
        instances.BeginUpdate();
        meshBatches.BeginUpdate();
        lights.BeginUpdate();
        cameras.BeginUpdate();
        indicesDirty.Clear();
        for (RenderLightEntity& EntityHandle : lightEntities)
        {
            EntityHandle.seen = false;
        }
        for (RenderCameraEntity& EntityHandle : cameraEntities)
        {
            EntityHandle.seen = false;
        }
        {
            PROFILER_SET_CPU_SCOPE_AUTO_NAMED("RenderScene::SyncLights");
            // Retain new emission maps before mesh/material bindings Release old dependencies.
            for (uint32_t I = 0; I < TF_ARRAY_COUNT(LightQueries); ++I)
            {
                LightQueryContext Context = { .Scene = *this, .Type = (LightType)I };
                LightQueries[I].Each(syncLight, &Context);
            }
        }
        {
            PROFILER_SET_CPU_SCOPE_AUTO_NAMED("RenderScene::bindings");
            bindings.Update();
        }
        {
            PROFILER_SET_CPU_SCOPE_AUTO_NAMED("RenderScene::extract");
            extractMeshEntities();
            updateSkins();
        }
        {
            PROFILER_SET_CPU_SCOPE_AUTO_NAMED("RenderScene::RemoveMissingLights");
            removeMissingLights();
        }
        {
            PROFILER_SET_CPU_SCOPE_AUTO_NAMED("RenderScene::UpdateCameras");
            cameraQuery.Each(extractCamera, this);
            removeCameras();
        }
        {
            PROFILER_SET_CPU_SCOPE_AUTO_NAMED("RenderScene::FinishDirtyRanges");
            primitives.FinishUpdate();
            instances.FinishUpdate();
            meshBatches.FinishUpdate();
            lights.FinishUpdate();
            cameras.FinishUpdate();
            MergeRenderRanges(indicesDirty, indicesRanges);
        }
        if (primitives.GetChanges().ranges.Count || instances.GetChanges().ranges.Count || meshBatches.GetChanges().ranges.Count ||
            lights.GetChanges().ranges.Count || cameras.GetChanges().ranges.Count || !indicesRanges.Empty())
        {
            ++revision;
        }
    }
    RenderSceneUploads GetPendingUploads()
    {
        primitives.GetPending(uploadRanges[0]);
        instances.GetPending(uploadRanges[1]);
        meshBatches.GetPending(uploadRanges[2]);
        MergeRenderRanges(indicesPending, uploadRanges[3]);
        lights.GetPending(uploadRanges[4]);
        cameras.GetPending(uploadRanges[5]);
        return { .primitives = { uploadRanges[0].GetData(), uploadRanges[0].Size() },
                 .instances = { uploadRanges[1].GetData(), uploadRanges[1].Size() },
                 .meshBatches = { uploadRanges[2].GetData(), uploadRanges[2].Size() },
                 .instanceIndices = { uploadRanges[3].GetData(), uploadRanges[3].Size() },
                 .lights = { uploadRanges[4].GetData(), uploadRanges[4].Size() },
                 .cameras = { uploadRanges[5].GetData(), uploadRanges[5].Size() },
                 .activePrimitives = primitives.IsActivePending(),
                 .activeInstances = instances.IsActivePending(),
                 .activeMeshBatches = meshBatches.IsActivePending(),
                 .activeLights = lights.IsActivePending(),
                 .ActiveLocalLightIndices = lights.IsActivePending() || LocalLightMembershipPending,
                 .activeCameras = cameras.IsActivePending() };
    }
    void AcknowledgeUploads()
    {
        primitives.ClearPending();
        instances.ClearPending();
        meshBatches.ClearPending();
        lights.ClearPending();
        LocalLightMembershipPending = false;
        cameras.ClearPending();
        for (uint32_t IndexValue : indicesPending)
        {
            indicesPendingFlags[IndexValue] = false;
        }
        indicesPending.Clear();
    }
    void CommitHistory()
    {
        PROFILER_SET_CPU_SCOPE_AUTO_NAMED("RenderScene::CommitHistory");
        for (uint32_t IndexValue : historyPending)
        {
            RenderHistory& History = instanceHistory[IndexValue];
            History.pending = false;
            const RenderInstance&  Instance = instances.Values[IndexValue];
            const RenderPrimitive* PrimitivePointer = primitives.Get(Instance.primitive);
            // A Reset since the last Update renders previous == current first.
            if (History.Reset || !Instance.id.IsValid() || !PrimitivePointer || !PrimitivePointer->instanceCount)
            {
                historyFollowUp.PushBack(instanceEntities[IndexValue]);
                continue;
            }
            History.submitted = Instance.current;
            History.valid = true;
            // The next Update must see the new previous transform even if the entity does not change again.
            historyFollowUp.PushBack(instanceEntities[IndexValue]);
        }
        historyPending.Clear();
        for (RenderCameraEntity& EntityHandle : cameraEntities)
        {
            if (EntityHandle.historyReset)
            {
                continue;
            }
            const RenderCamera& CameraData = cameras.Values[EntityHandle.id.index];
            EntityHandle.submitted = CameraData.current;
            EntityHandle.submittedLens = CameraData.lens;
            EntityHandle.historyValid = true;
        }
    }
    void MarkDirty(Entity Target)
    {
        ASSERT(!frameLocked);
        for (RenderEntity& EntityHandle : entities)
        {
            if (!Target.IsValid() || EntityHandle.entity == Target)
            {
                instanceHistory[EntityHandle.instance.index].valid = false;
                instanceHistory[EntityHandle.instance.index].Reset = true;
                historyFollowUp.PushBack(EntityHandle.entity);
            }
        }
        for (RenderCameraEntity& EntityHandle : cameraEntities)
        {
            if (!Target.IsValid() || EntityHandle.entity == Target)
            {
                EntityHandle.historyValid = false;
                EntityHandle.historyReset = true;
            }
        }
    }

    World&                                            world;
    AssetManager&                                     assetManager;
    const AnimationSystem*                            animation = nullptr;
    WorldAssetBindings                                bindings;
    WorldChangeLog                                    changes;
    WorldQuery                                        query;
    WorldQuery                                        LightQueries[TF_ARRAY_COUNT(LightComponentTypes)];
    WorldQuery                                        cameraQuery;
    RenderStorage<RenderPrimitive, RenderPrimitiveID> primitives;
    RenderStorage<RenderInstance, RenderInstanceID>   instances;
    RenderStorage<RenderMeshBatch, RenderMeshBatchID> meshBatches;
    RenderStorage<RenderLight, RenderLightID>         lights;
    bool                                              LocalLightMembershipPending = false;
    RenderStorage<RenderCamera, RenderCameraID>       cameras;
    Array<RenderEntity>                               entities;
    RenderEntityIndex*                                pIndices = nullptr;
    Array<RenderLightEntity>                          lightEntities;
    Array<RenderCameraEntity>                         cameraEntities;
    RenderEntityIndex*                                LightIndices[TF_ARRAY_COUNT(LightComponentTypes)] = {};
    RenderEntityIndex*                                pCameraIndices = nullptr;
    Array<AssetHandle>                                textureReleases;
    Array<uint32_t>                                   instanceIndices;
    Array<uint32_t>                                   indicesDirty;
    Array<RenderRange>                                indicesRanges;
    Array<uint32_t>                                   indicesPending;
    Array<bool>                                       indicesPendingFlags;
    Array<RenderHistory>                              instanceHistory;
    Array<uint32_t>                                   historyPending;
    // Asset states seen by the last extraction; a change re-extracts every mesh entity.
    uint64_t                                          extractedAssetRevision = 0;
    // History submitted since the last Update changes each moved instance's previous transform once more.
    Array<Entity>                                     historyFollowUp;
    Array<Entity>                                     instanceEntities;
    Array<RenderRange>                                uploadRanges[6];
    // Mesh entities with a SkinnedMesh, visited by every Update.
    Array<Entity>                                     skinnedEntities;
    Array<RenderSkin>                                 skins;
    Array<Matrix4>                                    skinMatrices;
    uint64_t                                          revision = 0;
    bool                                              frameLocked = false;
};

RenderScene::RenderScene(World& WorldData, AssetManager& AssetManagerValue, const AnimationSystem* Animation):
    Data(MemNew(RenderSceneData, WorldData, AssetManagerValue, Animation))
{
}
RenderScene::~RenderScene() { MemDelete(Data); }
void RenderScene::Reserve(uint32_t PrimitiveCount, uint32_t MeshBatchCount, uint32_t LightCount, uint32_t CameraCount)
{
    Data->primitives.Reserve(PrimitiveCount);
    Data->instances.Reserve(PrimitiveCount);
    Data->meshBatches.Reserve(MeshBatchCount);
    Data->entities.Reserve(PrimitiveCount);
    Data->instanceIndices.Reserve(PrimitiveCount);
    Data->indicesDirty.Reserve(PrimitiveCount);
    Data->indicesRanges.Reserve(PrimitiveCount);
    Data->indicesPending.Reserve(PrimitiveCount);
    Data->indicesPendingFlags.Reserve(PrimitiveCount);
    Data->instanceHistory.Reserve(PrimitiveCount);
    Data->historyPending.Reserve(PrimitiveCount);
    for (Array<RenderRange>& Ranges : Data->uploadRanges)
    {
        Ranges.Reserve(PrimitiveCount > MeshBatchCount ? PrimitiveCount : MeshBatchCount);
    }
    Data->lights.Reserve(LightCount);
    Data->cameras.Reserve(CameraCount);
    Data->lightEntities.Reserve(LightCount);
    Data->cameraEntities.Reserve(CameraCount);
    Data->textureReleases.Reserve(LightCount);
}
void               RenderScene::Update() { Data->Update(); }
uint64_t           RenderScene::GetRevision() const { return Data->revision; }
RenderSceneUploads RenderScene::GetPendingUploads() { return Data->GetPendingUploads(); }
void               RenderScene::AcknowledgeUploads() { Data->AcknowledgeUploads(); }
void               RenderScene::CommitHistory() { Data->CommitHistory(); }
void               RenderScene::SetFrameLocked(bool Locked) { Data->frameLocked = Locked; }
void               RenderScene::MarkDirty(Entity EntityHandle) { Data->MarkDirty(EntityHandle); }
RenderSceneChanges RenderScene::GetChanges() const
{
    return { .primitives = Data->primitives.GetChanges(),
             .instances = Data->instances.GetChanges(),
             .meshBatches = Data->meshBatches.GetChanges(),
             .lights = Data->lights.GetChanges(),
             .cameras = Data->cameras.GetChanges(),
             .instanceIndices = { Data->indicesRanges.GetData(), Data->indicesRanges.Size() } };
}
RenderPrimitiveID RenderScene::FindPrimitive(Entity EntityHandle) const
{
    const RenderEntityIndex* IndexPointer = hmgetp_null(Data->pIndices, EntityHandle);
    return IndexPointer ? Data->entities[IndexPointer->value].primitive : RenderPrimitiveID{};
}
const RenderPrimitive* RenderScene::GetPrimitive(RenderPrimitiveID Id) const { return Data->primitives.Get(Id); }
const RenderInstance*  RenderScene::GetInstance(RenderInstanceID Id) const { return Data->instances.Get(Id); }
const RenderMeshBatch* RenderScene::GetMeshBatch(RenderMeshBatchID Id) const { return Data->meshBatches.Get(Id); }
RenderLightID          RenderScene::FindLight(Entity EntityHandle) const
{
    for (uint32_t I = 0; I < TF_ARRAY_COUNT(LightComponentTypes); ++I)
    {
        const RenderLightID Id = FindLight(EntityHandle, (LightType)I);
        if (Id.IsValid())
        {
            return Id;
        }
    }
    return {};
}
RenderLightID RenderScene::FindLight(Entity EntityHandle, LightType Type) const
{
    ASSERT((uint32_t)Type < TF_ARRAY_COUNT(LightComponentTypes));
    const RenderEntityIndex* IndexPointer = hmgetp_null(Data->LightIndices[(uint32_t)Type], EntityHandle);
    return IndexPointer ? Data->lightEntities[IndexPointer->value].id : RenderLightID{};
}
RenderCameraID RenderScene::FindCamera(Entity EntityHandle) const
{
    const RenderEntityIndex* IndexPointer = hmgetp_null(Data->pCameraIndices, EntityHandle);
    return IndexPointer ? Data->cameraEntities[IndexPointer->value].id : RenderCameraID{};
}
const RenderLight*    RenderScene::GetLight(RenderLightID Id) const { return Data->lights.Get(Id); }
const RenderCamera*   RenderScene::GetCamera(RenderCameraID Id) const { return Data->cameras.Get(Id); }
Span<RenderPrimitive> RenderScene::GetPrimitives() const { return { Data->primitives.Values.GetData(), Data->primitives.Values.Size() }; }
Span<RenderInstance>  RenderScene::GetInstances() const { return { Data->instances.Values.GetData(), Data->instances.Values.Size() }; }
Span<RenderMeshBatch> RenderScene::GetMeshBatches() const
{
    return { Data->meshBatches.Values.GetData(), Data->meshBatches.Values.Size() };
}
Span<RenderLight>  RenderScene::GetLights() const { return { Data->lights.Values.GetData(), Data->lights.Values.Size() }; }
Span<RenderCamera> RenderScene::GetCameras() const { return { Data->cameras.Values.GetData(), Data->cameras.Values.Size() }; }
Span<uint32_t> RenderScene::GetActivePrimitives() const { return { Data->primitives.Active.GetData(), Data->primitives.Active.Size() }; }
Span<uint32_t> RenderScene::GetActiveInstances() const { return { Data->instances.Active.GetData(), Data->instances.Active.Size() }; }
Span<uint32_t> RenderScene::GetActiveMeshBatches() const { return { Data->meshBatches.Active.GetData(), Data->meshBatches.Active.Size() }; }
Span<uint32_t> RenderScene::GetActiveLights() const { return { Data->lights.Active.GetData(), Data->lights.Active.Size() }; }
Span<uint32_t> RenderScene::GetActiveCameras() const { return { Data->cameras.Active.GetData(), Data->cameras.Active.Size() }; }
Span<uint32_t> RenderScene::GetInstanceIndices() const { return { Data->instanceIndices.GetData(), Data->instanceIndices.Size() }; }
Span<RenderSkin> RenderScene::GetSkins() const { return { Data->skins.GetData(), Data->skins.Size() }; }
Span<Matrix4>    RenderScene::GetSkinMatrices() const { return { Data->skinMatrices.GetData(), Data->skinMatrices.Size() }; }
} // namespace hz
