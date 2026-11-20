/* Copyright (c) 2026 Horizon */

#include "Scene/AssetLoaders.h"
#include "Scene/AnimationAssets.h"
#include "Scene/MaterialAsset.h"
#include "Scene/MeshAsset.h"
#include "Scene/SceneAssetLoader.h"
#include "Scene/TextureAsset.h"
#include "SceneResourceAccess.h"
#include "AssetDependencies.h"
#include "Resources/IResourceLoader.h"
#include "Core/IThread.h"
#include <float.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "Core/IMemory.h"

namespace hz
{
class AssetLoadersData;

class AssetSource
{
public:
    AssetSource(AssetLoadersData& Owner, const char* File);
    ~AssetSource() { MemDeallocate(pPath); }
    void        Release();
    static void decode(void* User, uint64_t);
    bool        read(ResourceDirectory Directory);

    AssetLoadersData& owner;
    char*             pPath;
    SceneAsset        scene;
    uint32_t          references = 1;
    tfrg_atomic32_t   status = (uint32_t)AssetLoadStatus::Pending;
};

struct MeshDraw
{
    IndirectDrawIndexArguments arguments;
    MeshBounds                 bounds;
    MeshVertexRange            vertices;
};

class AssetGeometry
{
public:
    AssetGeometry(AssetLoadersData& Owner, const char* File);
    ~AssetGeometry()
    {
        removeResource(pUpload);
        if (pShadow)
            removeResource(pShadow);
        MemDeallocate(pPath);
    }
    void            Release();
    AssetLoadStatus poll();
    bool            buildBounds();
    static void     decodeBounds(void* User, uint64_t)
    {
        AssetGeometry& GeometryValue = *(AssetGeometry*)User;
        const bool     Success = GeometryValue.buildBounds();
        if (!Success)
            LOGF(Error, "Invalid Mesh positions or Draw ranges in '%s'", GeometryValue.pPath);
        tfrg_atomic32_store_release(&GeometryValue.status, (uint32_t)(Success ? AssetLoadStatus::Succeeded : AssetLoadStatus::Failed));
    }

    AssetLoadersData& owner;
    char*             pPath;
    uint32_t          references = 1;
    Geometry*         pUpload = nullptr;
    GeometryData*     pShadow = nullptr;
    SyncToken         token = 0;
    bool              uploadSucceeded = false;
    bool              boundsQueued = false;
    bool              adopted = false;
    tfrg_atomic32_t   status = (uint32_t)AssetLoadStatus::Pending;
    GPUBuffer         indexBuffer;
    // Indexed by layout binding; a binding the file lacks keeps an invalid buffer and a zero stride.
    GPUBuffer         vertexBuffers[MAX_VERTEX_BINDINGS];
    uint32_t          vertexStrides[MAX_VERTEX_BINDINGS] = {};
    uint32_t          vertexBufferCount = 0;
    IndexType         indexType = INDEX_TYPE_UINT16;
    Array<MeshDraw>   draws;
};

// Reads a standalone skeleton or animation file and decodes it on the worker thread.
class AnimationFileRequest
{
public:
    AnimationFileRequest(AssetLoadersData& Owner, const AssetCatalogEntry& Entry): owner(Owner), entry(Entry) {}
    ~AnimationFileRequest();
    static void decode(void* User, uint64_t);

    AssetLoadersData&        owner;
    const AssetCatalogEntry& entry;
    SkeletonAsset*           skeleton = nullptr;
    AnimationClipAsset*      clip = nullptr;
    tfrg_atomic32_t          status = (uint32_t)AssetLoadStatus::Pending;
};

class SubassetRequest
{
public:
    SubassetRequest(const AssetCatalogEntry& Entry, AssetSource* SourcePointer): entry(Entry), pSource(SourcePointer) {}
    ~SubassetRequest()
    {
        if (pSource)
            pSource->Release();
    }
    const AssetCatalogEntry& entry;
    AssetSource*             pSource;
};

struct AssetSourceIndex
{
    char*        key;
    AssetSource* value;
};
struct AssetGeometryIndex
{
    char*          key;
    AssetGeometry* value;
};

class AssetLoadersData
{
public:
    AssetLoadersData(RenderContext& Context, const AssetLoadersDesc& Desc):
        context(Context), desc(Desc), sceneLoader(Desc.cookedDirectory), textureLoader(Context, Desc.cookedDirectory, Desc.sourceDirectory)
    {
        ASSERT(Desc.vertexLayout.attribCount && Desc.vertexLayout.attribCount <= MAX_VERTEX_ATTRIBS);
        for (uint32_t I = 0; I < Desc.vertexLayout.attribCount; ++I)
        {
            ASSERT(Desc.vertexLayout.attribs[I].binding < MAX_VERTEX_BINDINGS);
            vertexBindingCount = TF_MAX(vertexBindingCount, Desc.vertexLayout.attribs[I].binding + 1);
        }
        const ThreadSystemInitDesc ThreadDescValue = { .threadCount = 1, .threadName = "AssetSource" };
        if (!threadSystemInit(&threads, &ThreadDescValue))
            LOGF(Error, "Cannot start Mesh/Material loader worker");
    }
    ~AssetLoadersData()
    {
        ASSERT(shlen(pSources) == 0 && shlen(pGeometries) == 0);
        if (threads)
            threadSystemExit(&threads, &gThreadSystemExitDescDefault);
        shfree(pSources);
        shfree(pGeometries);
    }

    AssetSource* acquireSource(const char* Path)
    {
        const ptrdiff_t Found = shgeti(pSources, Path);
        if (Found >= 0)
        {
            ++pSources[Found].value->references;
            return pSources[Found].value;
        }
        AssetSource* SourcePointer = MemNew(AssetSource, *this, Path);
        shput(pSources, SourcePointer->pPath, SourcePointer);
        threadSystemAddTask(threads, AssetSource::decode, SourcePointer);
        return SourcePointer;
    }

    AssetGeometry* acquireGeometry(const char* Path)
    {
        const ptrdiff_t Found = shgeti(pGeometries, Path);
        if (Found >= 0)
        {
            ++pGeometries[Found].value->references;
            return pGeometries[Found].value;
        }
        AssetGeometry* GeometryPointer = MemNew(AssetGeometry, *this, Path);
        shput(pGeometries, GeometryPointer->pPath, GeometryPointer);
        return GeometryPointer;
    }

    static void* begin(const AssetCatalogEntry& Entry, AssetHandle, void* User)
    {
        AssetLoadersData& Loader = *(AssetLoadersData*)User;
        if (!Loader.threads || (Entry.Type != AssetType::Mesh && Entry.Type != AssetType::Material) ||
            Entry.location.root != AssetPathRoot::Cooked || Entry.location.elementIndex == UINT32_MAX)
        {
            LOGF(Error, "Invalid Mesh/Material location '%s'", Entry.location.pPath);
            return nullptr;
        }
        return MemNew(SubassetRequest, Entry, Loader.acquireSource(Entry.location.pPath));
    }

    static AssetLoadResult poll(void* Job, void*)
    {
        SubassetRequest&      Request = *(SubassetRequest*)Job;
        const AssetLoadStatus Status = (AssetLoadStatus)tfrg_atomic32_load_acquire(&Request.pSource->status);
        if (Status != AssetLoadStatus::Succeeded)
            return { .status = Status };
        const SceneAsset& SceneValue = Request.pSource->scene;
        const uint32_t    IndexValue = Request.entry.location.elementIndex;
        Array<AssetID>    DependenciesValue;
        if (Request.entry.Type == AssetType::Material)
        {
            if (IndexValue >= SceneValue.GetMaterials().Count || SceneValue.GetMaterials().Data[IndexValue].id != Request.entry.id)
            {
                return invalid(Request);
            }
            const SceneAssetMaterial& MaterialValue = SceneValue.GetMaterials().Data[IndexValue];
            DependenciesValue.PushBack(MaterialValue.baseColorTexture);
            DependenciesValue.PushBack(MaterialValue.normalTexture);
            DependenciesValue.PushBack(MaterialValue.metallicRoughnessTexture);
            DependenciesValue.PushBack(MaterialValue.emissiveTexture);
            if (!MatchesAssetDependencies(Request.entry, DependenciesValue))
            {
                return invalid(Request);
            }
            MaterialAsset* Asset = MemNew(MaterialAsset);
            Asset->pSource = Request.pSource;
            Asset->index = IndexValue;
            Request.pSource = nullptr;
            return { .status = AssetLoadStatus::Succeeded, .pPayload = Asset };
        }
        if (IndexValue >= SceneValue.GetMeshes().Count || SceneValue.GetMeshes().Data[IndexValue].id != Request.entry.id)
        {
            return invalid(Request);
        }
        for (const SceneAssetSubmesh& Submesh : SceneValue.GetMeshes().Data[IndexValue].submeshes)
        {
            DependenciesValue.PushBack(Submesh.material);
        }
        if (!MatchesAssetDependencies(Request.entry, DependenciesValue))
        {
            return invalid(Request);
        }
        MeshAsset* Asset = MemNew(MeshAsset);
        Asset->pSource = Request.pSource;
        Asset->index = IndexValue;
        Request.pSource = nullptr;
        return { .status = AssetLoadStatus::Succeeded, .pPayload = Asset };
    }
    static AssetLoadResult invalid(const SubassetRequest& Request)
    {
        LOGF(Error, "Mesh/Material identity, index or dependencies differ from Catalog: '%s'", Request.entry.location.pPath);
        return { .status = AssetLoadStatus::Failed, .pError = "Mesh/Material identity differs from the catalog" };
    }
    static void destroyRequest(void* Job, void*) { MemDelete((SubassetRequest*)Job); }
    static void destroyMesh(void* Payload, void*) { MemDelete((MeshAsset*)Payload); }
    static void destroyMaterial(void* Payload, void*) { MemDelete((MaterialAsset*)Payload); }

    static void uploadMesh(void* Payload, const AssetManager&, Span<AssetHandle>, void* User)
    {
        MeshAsset& MeshValue = *(MeshAsset*)Payload;
        MeshValue.pGeometry = ((AssetLoadersData*)User)->acquireGeometry(MeshValue.GetDescription().pGeometry);
    }
    static AssetLoadStatus pollMesh(void* Payload, void*)
    {
        MeshAsset&            MeshValue = *(MeshAsset*)Payload;
        const AssetLoadStatus Status = MeshValue.pGeometry->poll();
        if (Status != AssetLoadStatus::Succeeded)
            return Status;
        for (const SceneAssetSubmesh& Submesh : MeshValue.GetDescription().submeshes)
        {
            if (Submesh.Draw >= MeshValue.pGeometry->draws.Size())
            {
                LOGF(Error, "Mesh submesh Draw is outside Geometry '%s'", MeshValue.pGeometry->pPath);
                return AssetLoadStatus::Failed;
            }
        }
        return AssetLoadStatus::Succeeded;
    }
    static void* beginAnimation(const AssetCatalogEntry& Entry, AssetHandle, void* User)
    {
        AssetLoadersData& Loader = *(AssetLoadersData*)User;
        if (!Loader.threads || (Entry.Type != AssetType::Skeleton && Entry.Type != AssetType::AnimationClip) ||
            Entry.location.root != AssetPathRoot::Cooked || Entry.location.elementIndex != UINT32_MAX)
        {
            LOGF(Error, "Invalid skeleton/animation location '%s'", Entry.location.pPath);
            return nullptr;
        }
        AnimationFileRequest* RequestPointer = MemNew(AnimationFileRequest, Loader, Entry);
        threadSystemAddTask(Loader.threads, AnimationFileRequest::decode, RequestPointer);
        return RequestPointer;
    }
    static AssetLoadResult pollAnimation(void* Job, void*)
    {
        AnimationFileRequest& Request = *(AnimationFileRequest*)Job;
        const AssetLoadStatus Status = (AssetLoadStatus)tfrg_atomic32_load_acquire(&Request.status);
        if (Status != AssetLoadStatus::Succeeded)
            return { .status = Status };
        void* Payload = Request.entry.Type == AssetType::Skeleton ? (void*)Request.skeleton : (void*)Request.clip;
        Request.skeleton = nullptr;
        Request.clip = nullptr;
        return { .status = AssetLoadStatus::Succeeded, .pPayload = Payload };
    }
    static void destroyAnimationRequest(void* Job, void*) { MemDelete((AnimationFileRequest*)Job); }
    static void destroySkeleton(void* Payload, void*) { MemDelete((SkeletonAsset*)Payload); }
    static void destroyAnimationClip(void* Payload, void*) { MemDelete((AnimationClipAsset*)Payload); }

    static void waitMesh(void* Payload, void*)
    {
        AssetGeometry& GeometryValue = *((MeshAsset*)Payload)->pGeometry;
        waitForToken(&GeometryValue.token);
        if (GeometryValue.boundsQueued && (AssetLoadStatus)tfrg_atomic32_load_acquire(&GeometryValue.status) == AssetLoadStatus::Pending)
            threadSystemWaitIdle(GeometryValue.owner.threads);
    }

    RenderContext&     context;
    AssetLoadersDesc   desc;
    uint32_t           vertexBindingCount = 0;
    SceneAssetLoader   sceneLoader;
    TextureAssetLoader textureLoader;
    ThreadSystem       threads = nullptr;
    AssetSourceIndex*   pSources = nullptr;
    AssetGeometryIndex* pGeometries = nullptr;
};

AnimationFileRequest::~AnimationFileRequest()
{
    if ((AssetLoadStatus)tfrg_atomic32_load_acquire(&status) == AssetLoadStatus::Pending)
        threadSystemWaitIdle(owner.threads);
    MemDelete(skeleton);
    MemDelete(clip);
}
void AnimationFileRequest::decode(void* User, uint64_t)
{
    AnimationFileRequest& Request = *(AnimationFileRequest*)User;
    FileStream            Stream = {};
    bool                  Success = false;
    if (OpenStreamFromPath(Request.owner.desc.cookedDirectory, Request.entry.location.pPath, FileModeRead, &Stream))
    {
        const ssize_t Size = GetStreamFileSize(&Stream);
        Array<char>   Bytes(Size > 0 && Size <= 256 * 1024 * 1024 ? (uint32_t)Size : 0u);
        Success = !Bytes.Empty() && ReadFromStream(&Stream, Bytes.GetData(), Bytes.Size()) == Bytes.Size();
        CloseStream(&Stream);
        if (Success && Request.entry.Type == AssetType::Skeleton)
        {
            Request.skeleton = MemNew(SkeletonAsset);
            Success = Request.skeleton->Parse({ Bytes.GetData(), Bytes.Size() });
        }
        else if (Success)
        {
            Request.clip = MemNew(AnimationClipAsset);
            Success = Request.clip->Parse({ Bytes.GetData(), Bytes.Size() });
        }
    }
    if (!Success)
        LOGF(Error, "Cannot decode skeleton/animation '%s'", Request.entry.location.pPath);
    tfrg_atomic32_store_release(&Request.status, (uint32_t)(Success ? AssetLoadStatus::Succeeded : AssetLoadStatus::Failed));
}

AssetSource::AssetSource(AssetLoadersData& Owner, const char* File): owner(Owner)
{
    pPath = (char*)MemAllocate(strlen(File) + 1);
    strcpy(pPath, File);
}
void AssetSource::Release()
{
    ASSERT(references);
    if (--references)
        return;
    if ((AssetLoadStatus)tfrg_atomic32_load_acquire(&status) == AssetLoadStatus::Pending)
        threadSystemWaitIdle(owner.threads);
    shdel(owner.pSources, pPath);
    MemDelete(this);
}
bool AssetSource::read(ResourceDirectory Directory)
{
    FileStream Stream = {};
    if (!OpenStreamFromPath(Directory, pPath, FileModeRead, &Stream))
    {
        return false;
    }
    const ssize_t Size = GetStreamFileSize(&Stream);
    if (Size <= 0 || Size > 64 * 1024 * 1024)
    {
        CloseStream(&Stream);
        return false;
    }
    Array<char>  Json((uint32_t)Size);
    const size_t Read = ReadFromStream(&Stream, Json.GetData(), (size_t)Size);
    CloseStream(&Stream);
    return Read == (size_t)Size && scene.Parse({ Json.GetData(), Json.Size() });
}
void AssetSource::decode(void* User, uint64_t)
{
    AssetSource& SourceValue = *(AssetSource*)User;
    const bool   Success = SourceValue.read(SourceValue.owner.desc.cookedDirectory);
    if (!Success)
        LOGF(Error, "Cannot decode Mesh/Material source '%s'", SourceValue.pPath);
    tfrg_atomic32_store_release(&SourceValue.status, (uint32_t)(Success ? AssetLoadStatus::Succeeded : AssetLoadStatus::Failed));
}

AssetGeometry::AssetGeometry(AssetLoadersData& Owner, const char* File): owner(Owner)
{
    pPath = (char*)MemAllocate(strlen(File) + 1);
    strcpy(pPath, File);
    GeometryLoadDesc Load = { .ppGeometry = &pUpload,
                              .ppGeometryData = &pShadow,
                              .pFileName = pPath,
                              .Flags = GEOMETRY_LOAD_FLAG_SHADOWED,
                              .pVertexLayout = &Owner.desc.vertexLayout,
                              .resourceDirectory = Owner.desc.cookedDirectory,
                              .pSuccess = &uploadSucceeded };
    addResource(&Load, &token);
}
void AssetGeometry::Release()
{
    ASSERT(references);
    if (--references)
        return;
    ASSERT(isTokenCompleted(&token));
    ASSERT(!boundsQueued || (AssetLoadStatus)tfrg_atomic32_load_acquire(&status) != AssetLoadStatus::Pending);
    shdel(owner.pGeometries, pPath);
    MemDelete(this);
}
bool AssetGeometry::buildBounds()
{
    if (!pShadow || !pShadow->pShadow || pShadow->pShadow->vertexStrides[SEMANTIC_POSITION] != sizeof(float) * 3 ||
        pShadow->pShadow->attributeCount[SEMANTIC_POSITION] != pUpload->vertexCount)
        return false;
    const GeometryData::ShadowData& Shadow = *pShadow->pShadow;
    const uint8_t*                  Positions = (const uint8_t*)Shadow.pAttributes[SEMANTIC_POSITION];
    const bool                      Index32 = pUpload->indexType == INDEX_TYPE_UINT32;
    draws.Resize(pUpload->drawArgCount);
    for (uint32_t I = 0; I < draws.Size(); ++I)
    {
        MeshDraw& Draw = draws[I];
        Draw.arguments = pUpload->pDrawArgs[I];
        if (!Draw.arguments.indexCount || Draw.arguments.startIndex > pUpload->indexCount ||
            Draw.arguments.indexCount > pUpload->indexCount - Draw.arguments.startIndex)
        {
            return false;
        }
        for (uint32_t Axis = 0; Axis < 3; ++Axis)
        {
            Draw.bounds.minimum[Axis] = FLT_MAX;
            Draw.bounds.maximum[Axis] = -FLT_MAX;
        }
        uint32_t LastVertex = 0;
        Draw.vertices.first = UINT32_MAX;
        for (uint32_t J = 0; J < Draw.arguments.indexCount; ++J)
        {
            const uint32_t IndexValue = Draw.arguments.startIndex + J;
            const int64_t  Vertex =
                (int64_t)(Index32 ? ((const uint32_t*)Shadow.pIndices)[IndexValue] : ((const uint16_t*)Shadow.pIndices)[IndexValue]) +
                (int32_t)Draw.arguments.vertexOffset;
            if (Vertex < 0 || (uint64_t)Vertex >= pUpload->vertexCount)
                return false;
            Draw.vertices.first = TF_MIN(Draw.vertices.first, (uint32_t)Vertex);
            LastVertex = TF_MAX(LastVertex, (uint32_t)Vertex);
            for (uint32_t Axis = 0; Axis < 3; ++Axis)
            {
                float Value;
                memcpy(&Value, Positions + (Vertex * 3 + Axis) * sizeof(float), sizeof(Value));
                if (!isfinite(Value))
                    return false;
                if (Value < Draw.bounds.minimum[Axis])
                {
                    Draw.bounds.minimum[Axis] = Value;
                }
                if (Value > Draw.bounds.maximum[Axis])
                {
                    Draw.bounds.maximum[Axis] = Value;
                }
            }
        }
        Draw.vertices.Count = LastVertex - Draw.vertices.first + 1;
    }
    return true;
}
AssetLoadStatus AssetGeometry::poll()
{
    if (adopted)
        return AssetLoadStatus::Succeeded;
    if (!isTokenCompleted(&token))
        return AssetLoadStatus::Pending;
    if (!uploadSucceeded || !pUpload)
    {
        LOGF(Error, "Geometry Upload failed: '%s'", pPath);
        return AssetLoadStatus::Failed;
    }
    if (!boundsQueued)
    {
        boundsQueued = true;
        threadSystemAddTask(owner.threads, decodeBounds, this);
    }
    const AssetLoadStatus Result = (AssetLoadStatus)tfrg_atomic32_load_acquire(&status);
    if (Result != AssetLoadStatus::Succeeded)
        return Result;
    indexType = (IndexType)pUpload->indexType;
    vertexBufferCount = owner.vertexBindingCount;
    indexBuffer = SceneResourceAccess::take(&owner.context, pUpload->pIndexBuffer, gIndexBufferState);
    // The loader creates buffers only for bindings with data, in binding order.
    uint32_t BufferValue = 0;
    for (uint32_t Binding = 0; Binding < vertexBufferCount; ++Binding)
    {
        if (!pUpload->vertexStrides[Binding])
            continue;
        vertexStrides[Binding] = pUpload->vertexStrides[Binding];
        vertexBuffers[Binding] = SceneResourceAccess::take(&owner.context, pUpload->pVertexBuffers[BufferValue], gVertexBufferState);
        ++BufferValue;
    }
    removeResource(pShadow);
    pShadow = nullptr;
    removeResource(pUpload);
    pUpload = nullptr;
    adopted = true;
    return AssetLoadStatus::Succeeded;
}

MaterialAsset::~MaterialAsset()
{
    if (pSource)
        pSource->Release();
}
const SceneAssetMaterial& MaterialAsset::GetDescription() const
{
    ASSERT(pSource);
    return pSource->scene.GetMaterials().Data[index];
}

MeshAsset::~MeshAsset()
{
    if (pGeometry)
        pGeometry->Release();
    if (pSource)
        pSource->Release();
}
const SceneAssetMesh& MeshAsset::GetDescription() const
{
    ASSERT(pSource);
    return pSource->scene.GetMeshes().Data[index];
}
const GPUBuffer& MeshAsset::GetIndexBuffer() const { return pGeometry->indexBuffer; }
const GPUBuffer& MeshAsset::GetVertexBuffer(uint32_t Binding) const
{
    ASSERT(Binding < pGeometry->vertexBufferCount);
    return pGeometry->vertexBuffers[Binding];
}
uint32_t MeshAsset::GetVertexBufferCount() const { return pGeometry->vertexBufferCount; }
uint32_t MeshAsset::GetVertexStride(uint32_t Binding) const
{
    ASSERT(Binding < pGeometry->vertexBufferCount);
    return pGeometry->vertexStrides[Binding];
}
IndexType                         MeshAsset::GetIndexType() const { return pGeometry->indexType; }
const IndirectDrawIndexArguments& MeshAsset::GetDraw(uint32_t Submesh) const
{
    ASSERT(Submesh < GetDescription().submeshes.Count);
    return pGeometry->draws[GetDescription().submeshes.Data[Submesh].Draw].arguments;
}
const MeshBounds& MeshAsset::GetBounds(uint32_t Submesh) const
{
    ASSERT(Submesh < GetDescription().submeshes.Count);
    return pGeometry->draws[GetDescription().submeshes.Data[Submesh].Draw].bounds;
}
const MeshVertexRange& MeshAsset::GetVertexRange(uint32_t Submesh) const
{
    ASSERT(Submesh < GetDescription().submeshes.Count);
    return pGeometry->draws[GetDescription().submeshes.Data[Submesh].Draw].vertices;
}

AssetLoaders::AssetLoaders(RenderContext& Context, const AssetLoadersDesc& Desc): Data(MemNew(AssetLoadersData, Context, Desc)) {}
AssetLoaders::~AssetLoaders() { MemDelete(Data); }
void AssetLoaders::RegisterLoaders(AssetManager& AssetManagerValue)
{
    AssetManagerValue.SetLoader(AssetType::Scene, Data->sceneLoader.GetLoader());
    AssetManagerValue.SetLoader(AssetType::Texture, Data->textureLoader.GetLoader());
    AssetManagerValue.SetLoader(AssetType::Material, { .UserData = Data,
                                                  .pBegin = AssetLoadersData::begin,
                                                  .pPoll = AssetLoadersData::poll,
                                                  .pDestroyRequest = AssetLoadersData::destroyRequest,
                                                  .pDestroyPayload = AssetLoadersData::destroyMaterial });
    AssetManagerValue.SetLoader(AssetType::Skeleton, { .UserData = Data,
                                                  .pBegin = AssetLoadersData::beginAnimation,
                                                  .pPoll = AssetLoadersData::pollAnimation,
                                                  .pDestroyRequest = AssetLoadersData::destroyAnimationRequest,
                                                  .pDestroyPayload = AssetLoadersData::destroySkeleton });
    AssetManagerValue.SetLoader(AssetType::AnimationClip, { .UserData = Data,
                                                       .pBegin = AssetLoadersData::beginAnimation,
                                                       .pPoll = AssetLoadersData::pollAnimation,
                                                       .pDestroyRequest = AssetLoadersData::destroyAnimationRequest,
                                                       .pDestroyPayload = AssetLoadersData::destroyAnimationClip });
    AssetManagerValue.SetLoader(AssetType::Mesh, { .UserData = Data,
                                              .pBegin = AssetLoadersData::begin,
                                              .pPoll = AssetLoadersData::poll,
                                              .pDestroyRequest = AssetLoadersData::destroyRequest,
                                              .pDestroyPayload = AssetLoadersData::destroyMesh,
                                              .pUpload = AssetLoadersData::uploadMesh,
                                              .pPollUpload = AssetLoadersData::pollMesh,
                                              .pWaitUpload = AssetLoadersData::waitMesh });
}
} // namespace hz
