/* Copyright (c) 2026 Horizon */

#include "Scene/AssetLoaders.h"
#include "Scene/MaterialAsset.h"
#include "Scene/MeshAsset.h"
#include "Scene/SceneAssetLoader.h"
#include "Scene/TextureAsset.h"
#include "SceneResourceAccess.h"
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
    ~AssetSource() { tf_free(pPath); }
    void        release();
    static void decode(void* pUser, uint64_t);
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
        tf_free(pPath);
    }
    void            release();
    AssetLoadStatus poll();
    bool            buildBounds();
    static void     decodeBounds(void* pUser, uint64_t)
    {
        AssetGeometry& geometry = *(AssetGeometry*)pUser;
        const bool     success = geometry.buildBounds();
        if (!success)
            LOGF(eERROR, "Invalid Mesh positions or draw ranges in '%s'", geometry.pPath);
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
            pSource->release();
    }
    const AssetCatalogEntry& entry;
    AssetSource*             pSource;
};

struct SourceRecord
{
    char*        key;
    AssetSource* value;
};
struct GeometryRecord
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
        const ThreadSystemInitDesc threadDesc = { .threadCount = 1, .threadName = "AssetSource" };
        if (!threadSystemInit(&threads, &threadDesc))
            LOGF(eERROR, "Cannot start Mesh/Material loader worker");
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
        AssetSource* pSource = tf_new(AssetSource, *this, pPath);
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
        AssetGeometry* pGeometry = tf_new(AssetGeometry, *this, pPath);
        shput(pGeometries, pGeometry->pPath, pGeometry);
        return pGeometry;
    }

    static void* begin(const AssetCatalogEntry& entry, AssetHandle, void* pUser)
    {
        AssetLoadersData& loader = *(AssetLoadersData*)pUser;
        if (!loader.threads || (entry.type != AssetType::Mesh && entry.type != AssetType::Material) ||
            entry.location.root != AssetPathRoot::Cooked || entry.location.elementIndex == UINT32_MAX)
        {
            LOGF(eERROR, "Invalid Mesh/Material location '%s'", entry.location.pPath);
            return nullptr;
        }
        return tf_new(SubassetRequest, entry, loader.acquireSource(entry.location.pPath));
    }

    static int compareIDs(const void* pA, const void* pB)
    {
        const AssetID& a = *(const AssetID*)pA;
        const AssetID& b = *(const AssetID*)pB;
        return (b < a) - (a < b);
    }
    static bool matchesDependencies(const AssetCatalogEntry& entry, Array<AssetID>& ids)
    {
        if (!ids.empty())
            qsort(ids.data(), ids.size(), sizeof(AssetID), compareIDs);
        uint32_t count = 0;
        for (uint32_t i = 0; i < ids.size(); ++i)
        {
            if (!ids[i].isValid() || (i && ids[i] == ids[i - 1]))
                continue;
            if (count == entry.dependencies.count || ids[i] != entry.dependencies.pData[count++])
                return false;
        }
        return count == entry.dependencies.count;
    }
    static AssetLoadResult poll(void* pJob, void*)
    {
        SubassetRequest&      request = *(SubassetRequest*)pJob;
        const AssetLoadStatus status = (AssetLoadStatus)tfrg_atomic32_load_acquire(&request.pSource->status);
        if (status != AssetLoadStatus::Succeeded)
            return { .status = status };
        const SceneAsset& scene = request.pSource->scene;
        const uint32_t    index = request.entry.location.elementIndex;
        Array<AssetID>    dependencies;
        if (request.entry.type == AssetType::Material)
        {
            if (index >= scene.getMaterials().count || scene.getMaterials().pData[index].id != request.entry.id)
                return invalid(request);
            const SceneAssetMaterial& material = scene.getMaterials().pData[index];
            dependencies.pushBack(material.baseColorTexture);
            dependencies.pushBack(material.normalTexture);
            dependencies.pushBack(material.metallicRoughnessTexture);
            dependencies.pushBack(material.emissiveTexture);
            if (!matchesDependencies(request.entry, dependencies))
                return invalid(request);
            MaterialAsset* pAsset = tf_new(MaterialAsset);
            pAsset->pSource = request.pSource;
            pAsset->index = index;
            request.pSource = nullptr;
            return { .status = AssetLoadStatus::Succeeded, .pPayload = pAsset };
        }
        if (index >= scene.getMeshes().count || scene.getMeshes().pData[index].id != request.entry.id)
            return invalid(request);
        for (const SceneAssetSubmesh& submesh : scene.getMeshes().pData[index].submeshes)
            dependencies.pushBack(submesh.material);
        if (!matchesDependencies(request.entry, dependencies))
            return invalid(request);
        MeshAsset* pAsset = tf_new(MeshAsset);
        pAsset->pSource = request.pSource;
        pAsset->index = index;
        request.pSource = nullptr;
        return { .status = AssetLoadStatus::Succeeded, .pPayload = pAsset };
    }
    static AssetLoadResult invalid(const SubassetRequest& request)
    {
        LOGF(eERROR, "Mesh/Material identity, index or dependencies differ from Catalog: '%s'", request.entry.location.pPath);
        return { .status = AssetLoadStatus::Failed };
    }
    static void destroyRequest(void* pJob, void*) { tf_delete((SubassetRequest*)pJob); }
    static void destroyMesh(void* pPayload, void*) { tf_delete((MeshAsset*)pPayload); }
    static void destroyMaterial(void* pPayload, void*) { tf_delete((MaterialAsset*)pPayload); }

    static void uploadMesh(void* pPayload, const AssetRegistry&, Span<AssetHandle>, void* pUser)
    {
        MeshAsset& mesh = *(MeshAsset*)pPayload;
        mesh.pGeometry = ((AssetLoadersData*)pUser)->acquireGeometry(mesh.getDescription().pGeometry);
    }
    static AssetLoadStatus pollMesh(void* pPayload, void*)
    {
        MeshAsset&            mesh = *(MeshAsset*)pPayload;
        const AssetLoadStatus status = mesh.pGeometry->poll();
        if (status != AssetLoadStatus::Succeeded)
            return status;
        for (const SceneAssetSubmesh& submesh : mesh.getDescription().submeshes)
        {
            if (submesh.draw >= mesh.pGeometry->draws.size())
            {
                LOGF(eERROR, "Mesh submesh draw is outside Geometry '%s'", mesh.pGeometry->pPath);
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
    SceneAssetLoader   sceneLoader;
    TextureAssetLoader textureLoader;
    ThreadSystem       threads = nullptr;
    SourceRecord*      pSources = nullptr;
    GeometryRecord*    pGeometries = nullptr;
};

AssetSource::AssetSource(AssetLoadersData& owner, const char* pFile): owner(owner)
{
    pPath = (char*)tf_malloc(strlen(pFile) + 1);
    strcpy(pPath, pFile);
}
void AssetSource::release()
{
    ASSERT(references);
    if (--references)
        return;
    if ((AssetLoadStatus)tfrg_atomic32_load_acquire(&status) == AssetLoadStatus::Pending)
        threadSystemWaitIdle(owner.threads);
    shdel(owner.pSources, pPath);
    tf_delete(this);
}
bool AssetSource::read(ResourceDirectory directory)
{
    FileStream stream = {};
    if (!fsOpenStreamFromPath(directory, pPath, FM_READ, &stream))
        return false;
    const ssize_t size = fsGetStreamFileSize(&stream);
    if (size <= 0 || size > 64 * 1024 * 1024)
    {
        fsCloseStream(&stream);
        return false;
    }
    Array<char>  json((uint32_t)size);
    const size_t read = fsReadFromStream(&stream, json.data(), (size_t)size);
    fsCloseStream(&stream);
    return read == (size_t)size && scene.parse({ json.data(), json.size() });
}
void AssetSource::decode(void* pUser, uint64_t)
{
    AssetSource& source = *(AssetSource*)pUser;
    const bool   success = source.read(source.owner.desc.cookedDirectory);
    if (!success)
        LOGF(eERROR, "Cannot decode Mesh/Material source '%s'", source.pPath);
    tfrg_atomic32_store_release(&source.status, (uint32_t)(success ? AssetLoadStatus::Succeeded : AssetLoadStatus::Failed));
}

AssetGeometry::AssetGeometry(AssetLoadersData& owner, const char* pFile): owner(owner)
{
    pPath = (char*)tf_malloc(strlen(pFile) + 1);
    strcpy(pPath, pFile);
    GeometryLoadDesc load = { .ppGeometry = &pUpload,
                              .ppGeometryData = &pShadow,
                              .pFileName = pPath,
                              .flags = GEOMETRY_LOAD_FLAG_SHADOWED,
                              .pVertexLayout = &owner.desc.vertexLayout,
                              .resourceDirectory = owner.desc.cookedDirectory,
                              .pSuccess = &uploadSucceeded };
    addResource(&load, &token);
}
void AssetGeometry::release()
{
    ASSERT(references);
    if (--references)
        return;
    ASSERT(isTokenCompleted(&token));
    ASSERT(!boundsQueued || (AssetLoadStatus)tfrg_atomic32_load_acquire(&status) != AssetLoadStatus::Pending);
    shdel(owner.pGeometries, pPath);
    tf_delete(this);
}
bool AssetGeometry::buildBounds()
{
    if (!pShadow || !pShadow->pShadow || pShadow->pShadow->vertexStrides[SEMANTIC_POSITION] != sizeof(float) * 3 ||
        pShadow->pShadow->attributeCount[SEMANTIC_POSITION] != pUpload->vertexCount)
        return false;
    const GeometryData::ShadowData& shadow = *pShadow->pShadow;
    const uint8_t*                  pPositions = (const uint8_t*)shadow.pAttributes[SEMANTIC_POSITION];
    const bool                      index32 = pUpload->vertexCount > UINT16_MAX;
    draws.resize(pUpload->drawArgCount);
    for (uint32_t i = 0; i < draws.size(); ++i)
    {
        MeshDraw& draw = draws[i];
        draw.arguments = pUpload->pDrawArgs[i];
        if (!draw.arguments.indexCount || draw.arguments.startIndex > pUpload->indexCount ||
            draw.arguments.indexCount > pUpload->indexCount - draw.arguments.startIndex)
            return false;
        for (uint32_t axis = 0; axis < 3; ++axis)
        {
            draw.bounds.minimum[axis] = FLT_MAX;
            draw.bounds.maximum[axis] = -FLT_MAX;
        }
        for (uint32_t j = 0; j < draw.arguments.indexCount; ++j)
        {
            const uint32_t index = draw.arguments.startIndex + j;
            const int64_t  vertex =
                (int64_t)(index32 ? ((const uint32_t*)shadow.pIndices)[index] : ((const uint16_t*)shadow.pIndices)[index]) +
                (int32_t)draw.arguments.vertexOffset;
            if (vertex < 0 || (uint64_t)vertex >= pUpload->vertexCount)
                return false;
            for (uint32_t axis = 0; axis < 3; ++axis)
            {
                float value;
                memcpy(&value, pPositions + (vertex * 3 + axis) * sizeof(float), sizeof(value));
                if (!isfinite(value))
                    return false;
                if (value < draw.bounds.minimum[axis])
                    draw.bounds.minimum[axis] = value;
                if (value > draw.bounds.maximum[axis])
                    draw.bounds.maximum[axis] = value;
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
        LOGF(eERROR, "Geometry upload failed: '%s'", pPath);
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
    vertexBufferCount = pUpload->vertexBufferCount;
    indexBuffer = SceneResourceAccess::take(&owner.context, pUpload->pIndexBuffer, gIndexBufferState);
    uint32_t buffer = 0;
    for (uint32_t binding = 0; binding < MAX_VERTEX_BINDINGS; ++binding)
    {
        if (!pUpload->vertexStrides[binding])
            continue;
        vertexStrides[buffer] = pUpload->vertexStrides[binding];
        vertexBuffers[buffer] = SceneResourceAccess::take(&owner.context, pUpload->pVertexBuffers[buffer], gVertexBufferState);
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
        pSource->release();
}
const SceneAssetMaterial& MaterialAsset::getDescription() const
{
    ASSERT(pSource);
    return pSource->scene.getMaterials().pData[index];
}

MeshAsset::~MeshAsset()
{
    if (pGeometry)
        pGeometry->release();
    if (pSource)
        pSource->release();
}
const SceneAssetMesh& MeshAsset::getDescription() const
{
    ASSERT(pSource);
    return pSource->scene.getMeshes().pData[index];
}
const GPUBuffer& MeshAsset::getIndexBuffer() const { return pGeometry->indexBuffer; }
const GPUBuffer& MeshAsset::getVertexBuffer(uint32_t binding) const
{
    ASSERT(binding < pGeometry->vertexBufferCount);
    return pGeometry->vertexBuffers[binding];
}
uint32_t MeshAsset::getVertexBufferCount() const { return pGeometry->vertexBufferCount; }
uint32_t MeshAsset::getVertexStride(uint32_t binding) const
{
    ASSERT(binding < pGeometry->vertexBufferCount);
    return pGeometry->vertexStrides[binding];
}
IndexType                         MeshAsset::getIndexType() const { return pGeometry->indexType; }
const IndirectDrawIndexArguments& MeshAsset::getDraw(uint32_t submesh) const
{
    ASSERT(submesh < getDescription().submeshes.count);
    return pGeometry->draws[getDescription().submeshes.pData[submesh].draw].arguments;
}
const MeshBounds& MeshAsset::getBounds(uint32_t submesh) const
{
    ASSERT(submesh < getDescription().submeshes.count);
    return pGeometry->draws[getDescription().submeshes.pData[submesh].draw].bounds;
}

AssetLoaders::AssetLoaders(RenderContext& context, const AssetLoadersDesc& desc): pData(tf_new(AssetLoadersData, context, desc)) {}
AssetLoaders::~AssetLoaders() { tf_delete(pData); }
void AssetLoaders::registerLoaders(AssetRegistry& registry)
{
    registry.setLoader(AssetType::Scene, pData->sceneLoader.getLoader());
    registry.setLoader(AssetType::Texture, pData->textureLoader.getLoader());
    registry.setLoader(AssetType::Material, { .pUserData = pData,
                                              .pBegin = AssetLoadersData::begin,
                                              .pPoll = AssetLoadersData::poll,
                                              .pDestroyRequest = AssetLoadersData::destroyRequest,
                                              .pDestroyPayload = AssetLoadersData::destroyMaterial });
    registry.setLoader(AssetType::Mesh, { .pUserData = pData,
                                          .pBegin = AssetLoadersData::begin,
                                          .pPoll = AssetLoadersData::poll,
                                          .pDestroyRequest = AssetLoadersData::destroyRequest,
                                          .pDestroyPayload = AssetLoadersData::destroyMesh,
                                          .pUpload = AssetLoadersData::uploadMesh,
                                          .pPollUpload = AssetLoadersData::pollMesh,
                                          .pWaitUpload = AssetLoadersData::waitMesh });
}
} // namespace hz
