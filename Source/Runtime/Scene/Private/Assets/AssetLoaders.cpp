/* Copyright (c) 2026 Horizon */

#include "Scene/AssetLoaders.h"
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
    AssetSource(AssetLoadersData& owner, const char* pFile);
    ~AssetSource() { MemDeallocate(pPath); }
    void        Release();
    static void decode(void* User, uint64_t);
    bool        read(ResourceDirectory directory);

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
};

class AssetGeometry
{
public:
    AssetGeometry(AssetLoadersData& owner, const char* pFile);
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
        AssetGeometry& geometry = *(AssetGeometry*)User;
        const bool     success = geometry.buildBounds();
        if (!success)
            LOGF(Error, "Invalid Mesh positions or Draw ranges in '%s'", geometry.pPath);
        tfrg_atomic32_store_release(&geometry.status, (uint32_t)(success ? AssetLoadStatus::Succeeded : AssetLoadStatus::Failed));
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

class SubassetRequest
{
public:
    SubassetRequest(const AssetCatalogEntry& entry, AssetSource* pSource): entry(entry), pSource(pSource) {}
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
    AssetLoadersData(RenderContext& context, const AssetLoadersDesc& desc):
        context(context), desc(desc), sceneLoader(desc.cookedDirectory), textureLoader(context, desc.cookedDirectory, desc.sourceDirectory)
    {
        ASSERT(desc.vertexLayout.attribCount && desc.vertexLayout.attribCount <= MAX_VERTEX_ATTRIBS);
        for (uint32_t i = 0; i < desc.vertexLayout.attribCount; ++i)
        {
            ASSERT(desc.vertexLayout.attribs[i].binding < MAX_VERTEX_BINDINGS);
            vertexBindingCount = TF_MAX(vertexBindingCount, desc.vertexLayout.attribs[i].binding + 1);
        }
        const ThreadSystemInitDesc threadDesc = { .threadCount = 1, .threadName = "AssetSource" };
        if (!threadSystemInit(&threads, &threadDesc))
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

    AssetSource* acquireSource(const char* pPath)
    {
        const ptrdiff_t found = shgeti(pSources, pPath);
        if (found >= 0)
        {
            ++pSources[found].value->references;
            return pSources[found].value;
        }
        AssetSource* pSource = MemNew(AssetSource, *this, pPath);
        shput(pSources, pSource->pPath, pSource);
        threadSystemAddTask(threads, AssetSource::decode, pSource);
        return pSource;
    }

    AssetGeometry* acquireGeometry(const char* pPath)
    {
        const ptrdiff_t found = shgeti(pGeometries, pPath);
        if (found >= 0)
        {
            ++pGeometries[found].value->references;
            return pGeometries[found].value;
        }
        AssetGeometry* pGeometry = MemNew(AssetGeometry, *this, pPath);
        shput(pGeometries, pGeometry->pPath, pGeometry);
        return pGeometry;
    }

    static void* begin(const AssetCatalogEntry& entry, AssetHandle, void* User)
    {
        AssetLoadersData& loader = *(AssetLoadersData*)User;
        if (!loader.threads || (entry.Type != AssetType::Mesh && entry.Type != AssetType::Material) ||
            entry.location.root != AssetPathRoot::Cooked || entry.location.elementIndex == UINT32_MAX)
        {
            LOGF(Error, "Invalid Mesh/Material location '%s'", entry.location.pPath);
            return nullptr;
        }
        return MemNew(SubassetRequest, entry, loader.acquireSource(entry.location.pPath));
    }

    static AssetLoadResult poll(void* pJob, void*)
    {
        SubassetRequest&      Request = *(SubassetRequest*)pJob;
        const AssetLoadStatus status = (AssetLoadStatus)tfrg_atomic32_load_acquire(&Request.pSource->status);
        if (status != AssetLoadStatus::Succeeded)
            return { .status = status };
        const SceneAsset& scene = Request.pSource->scene;
        const uint32_t    index = Request.entry.location.elementIndex;
        Array<AssetID>    dependencies;
        if (Request.entry.Type == AssetType::Material)
        {
            if (index >= scene.GetMaterials().Count || scene.GetMaterials().Data[index].id != Request.entry.id)
            {
                return invalid(Request);
            }
            const SceneAssetMaterial& material = scene.GetMaterials().Data[index];
            dependencies.PushBack(material.baseColorTexture);
            dependencies.PushBack(material.normalTexture);
            dependencies.PushBack(material.metallicRoughnessTexture);
            dependencies.PushBack(material.emissiveTexture);
            if (!matchesAssetDependencies(Request.entry, dependencies))
            {
                return invalid(Request);
            }
            MaterialAsset* pAsset = MemNew(MaterialAsset);
            pAsset->pSource = Request.pSource;
            pAsset->index = index;
            Request.pSource = nullptr;
            return { .status = AssetLoadStatus::Succeeded, .pPayload = pAsset };
        }
        if (index >= scene.GetMeshes().Count || scene.GetMeshes().Data[index].id != Request.entry.id)
        {
            return invalid(Request);
        }
        for (const SceneAssetSubmesh& submesh : scene.GetMeshes().Data[index].submeshes)
        {
            dependencies.PushBack(submesh.material);
        }
        if (!matchesAssetDependencies(Request.entry, dependencies))
        {
            return invalid(Request);
        }
        MeshAsset* pAsset = MemNew(MeshAsset);
        pAsset->pSource = Request.pSource;
        pAsset->index = index;
        Request.pSource = nullptr;
        return { .status = AssetLoadStatus::Succeeded, .pPayload = pAsset };
    }
    static AssetLoadResult invalid(const SubassetRequest& Request)
    {
        LOGF(Error, "Mesh/Material identity, index or dependencies differ from Catalog: '%s'", Request.entry.location.pPath);
        return { .status = AssetLoadStatus::Failed, .pError = "Mesh/Material identity differs from the catalog" };
    }
    static void destroyRequest(void* pJob, void*) { MemDelete((SubassetRequest*)pJob); }
    static void destroyMesh(void* pPayload, void*) { MemDelete((MeshAsset*)pPayload); }
    static void destroyMaterial(void* pPayload, void*) { MemDelete((MaterialAsset*)pPayload); }

    static void uploadMesh(void* pPayload, const AssetManager&, Span<AssetHandle>, void* User)
    {
        MeshAsset& mesh = *(MeshAsset*)pPayload;
        mesh.pGeometry = ((AssetLoadersData*)User)->acquireGeometry(mesh.GetDescription().pGeometry);
    }
    static AssetLoadStatus pollMesh(void* pPayload, void*)
    {
        MeshAsset&            mesh = *(MeshAsset*)pPayload;
        const AssetLoadStatus status = mesh.pGeometry->poll();
        if (status != AssetLoadStatus::Succeeded)
            return status;
        for (const SceneAssetSubmesh& submesh : mesh.GetDescription().submeshes)
        {
            if (submesh.Draw >= mesh.pGeometry->draws.Size())
            {
                LOGF(Error, "Mesh submesh Draw is outside Geometry '%s'", mesh.pGeometry->pPath);
                return AssetLoadStatus::Failed;
            }
        }
        return AssetLoadStatus::Succeeded;
    }
    static void waitMesh(void* pPayload, void*)
    {
        AssetGeometry& geometry = *((MeshAsset*)pPayload)->pGeometry;
        waitForToken(&geometry.token);
        if (geometry.boundsQueued && (AssetLoadStatus)tfrg_atomic32_load_acquire(&geometry.status) == AssetLoadStatus::Pending)
            threadSystemWaitIdle(geometry.owner.threads);
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

AssetSource::AssetSource(AssetLoadersData& owner, const char* pFile): owner(owner)
{
    pPath = (char*)MemAllocate(strlen(pFile) + 1);
    strcpy(pPath, pFile);
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
bool AssetSource::read(ResourceDirectory directory)
{
    FileStream stream = {};
    if (!OpenStreamFromPath(directory, pPath, FileModeRead, &stream))
    {
        return false;
    }
    const ssize_t Size = GetStreamFileSize(&stream);
    if (Size <= 0 || Size > 64 * 1024 * 1024)
    {
        CloseStream(&stream);
        return false;
    }
    Array<char>  json((uint32_t)Size);
    const size_t read = ReadFromStream(&stream, json.GetData(), (size_t)Size);
    CloseStream(&stream);
    return read == (size_t)Size && scene.Parse({ json.GetData(), json.Size() });
}
void AssetSource::decode(void* User, uint64_t)
{
    AssetSource& source = *(AssetSource*)User;
    const bool   success = source.read(source.owner.desc.cookedDirectory);
    if (!success)
        LOGF(Error, "Cannot decode Mesh/Material source '%s'", source.pPath);
    tfrg_atomic32_store_release(&source.status, (uint32_t)(success ? AssetLoadStatus::Succeeded : AssetLoadStatus::Failed));
}

AssetGeometry::AssetGeometry(AssetLoadersData& owner, const char* pFile): owner(owner)
{
    pPath = (char*)MemAllocate(strlen(pFile) + 1);
    strcpy(pPath, pFile);
    GeometryLoadDesc Load = { .ppGeometry = &pUpload,
                              .ppGeometryData = &pShadow,
                              .pFileName = pPath,
                              .Flags = GEOMETRY_LOAD_FLAG_SHADOWED,
                              .pVertexLayout = &owner.desc.vertexLayout,
                              .resourceDirectory = owner.desc.cookedDirectory,
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
    const GeometryData::ShadowData& shadow = *pShadow->pShadow;
    const uint8_t*                  pPositions = (const uint8_t*)shadow.pAttributes[SEMANTIC_POSITION];
    const bool                      index32 = pUpload->indexType == INDEX_TYPE_UINT32;
    draws.Resize(pUpload->drawArgCount);
    for (uint32_t i = 0; i < draws.Size(); ++i)
    {
        MeshDraw& Draw = draws[i];
        Draw.arguments = pUpload->pDrawArgs[i];
        if (!Draw.arguments.indexCount || Draw.arguments.startIndex > pUpload->indexCount ||
            Draw.arguments.indexCount > pUpload->indexCount - Draw.arguments.startIndex)
        {
            return false;
        }
        for (uint32_t axis = 0; axis < 3; ++axis)
        {
            Draw.bounds.minimum[axis] = FLT_MAX;
            Draw.bounds.maximum[axis] = -FLT_MAX;
        }
        for (uint32_t j = 0; j < Draw.arguments.indexCount; ++j)
        {
            const uint32_t index = Draw.arguments.startIndex + j;
            const int64_t  vertex =
                (int64_t)(index32 ? ((const uint32_t*)shadow.pIndices)[index] : ((const uint16_t*)shadow.pIndices)[index]) +
                (int32_t)Draw.arguments.vertexOffset;
            if (vertex < 0 || (uint64_t)vertex >= pUpload->vertexCount)
                return false;
            for (uint32_t axis = 0; axis < 3; ++axis)
            {
                float value;
                memcpy(&value, pPositions + (vertex * 3 + axis) * sizeof(float), sizeof(value));
                if (!isfinite(value))
                    return false;
                if (value < Draw.bounds.minimum[axis])
                {
                    Draw.bounds.minimum[axis] = value;
                }
                if (value > Draw.bounds.maximum[axis])
                {
                    Draw.bounds.maximum[axis] = value;
                }
            }
        }
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
    const AssetLoadStatus result = (AssetLoadStatus)tfrg_atomic32_load_acquire(&status);
    if (result != AssetLoadStatus::Succeeded)
        return result;
    indexType = (IndexType)pUpload->indexType;
    vertexBufferCount = owner.vertexBindingCount;
    indexBuffer = SceneResourceAccess::take(&owner.context, pUpload->pIndexBuffer, gIndexBufferState);
    // The loader creates buffers only for bindings with data, in binding order.
    uint32_t buffer = 0;
    for (uint32_t binding = 0; binding < vertexBufferCount; ++binding)
    {
        if (!pUpload->vertexStrides[binding])
            continue;
        vertexStrides[binding] = pUpload->vertexStrides[binding];
        vertexBuffers[binding] = SceneResourceAccess::take(&owner.context, pUpload->pVertexBuffers[buffer], gVertexBufferState);
        ++buffer;
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
const GPUBuffer& MeshAsset::GetVertexBuffer(uint32_t binding) const
{
    ASSERT(binding < pGeometry->vertexBufferCount);
    return pGeometry->vertexBuffers[binding];
}
uint32_t MeshAsset::GetVertexBufferCount() const { return pGeometry->vertexBufferCount; }
uint32_t MeshAsset::GetVertexStride(uint32_t binding) const
{
    ASSERT(binding < pGeometry->vertexBufferCount);
    return pGeometry->vertexStrides[binding];
}
IndexType                         MeshAsset::GetIndexType() const { return pGeometry->indexType; }
const IndirectDrawIndexArguments& MeshAsset::GetDraw(uint32_t submesh) const
{
    ASSERT(submesh < GetDescription().submeshes.Count);
    return pGeometry->draws[GetDescription().submeshes.Data[submesh].Draw].arguments;
}
const MeshBounds& MeshAsset::GetBounds(uint32_t submesh) const
{
    ASSERT(submesh < GetDescription().submeshes.Count);
    return pGeometry->draws[GetDescription().submeshes.Data[submesh].Draw].bounds;
}

AssetLoaders::AssetLoaders(RenderContext& context, const AssetLoadersDesc& desc): Data(MemNew(AssetLoadersData, context, desc)) {}
AssetLoaders::~AssetLoaders() { MemDelete(Data); }
void AssetLoaders::RegisterLoaders(AssetManager& assetManager)
{
    assetManager.SetLoader(AssetType::Scene, Data->sceneLoader.GetLoader());
    assetManager.SetLoader(AssetType::Texture, Data->textureLoader.GetLoader());
    assetManager.SetLoader(AssetType::Material, { .UserData = Data,
                                                  .pBegin = AssetLoadersData::begin,
                                                  .pPoll = AssetLoadersData::poll,
                                                  .pDestroyRequest = AssetLoadersData::destroyRequest,
                                                  .pDestroyPayload = AssetLoadersData::destroyMaterial });
    assetManager.SetLoader(AssetType::Mesh, { .UserData = Data,
                                              .pBegin = AssetLoadersData::begin,
                                              .pPoll = AssetLoadersData::poll,
                                              .pDestroyRequest = AssetLoadersData::destroyRequest,
                                              .pDestroyPayload = AssetLoadersData::destroyMesh,
                                              .pUpload = AssetLoadersData::uploadMesh,
                                              .pPollUpload = AssetLoadersData::pollMesh,
                                              .pWaitUpload = AssetLoadersData::waitMesh });
}
} // namespace hz
