/* Copyright (c) 2026 Horizon */

#include "Scene/AssetCatalog.h"
#include "Scene/SceneAsset.h"
#include "Core/IContainer.h"
#include <ThirdParty/cJSON/cJSON.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "Core/IMemory.h"

namespace hz
{
struct CatalogRecord
{
    AssetID            key;
    AssetCatalogEntry* value;
};

class AssetCatalogData
{
public:
    void insert(const AssetCatalogEntry& entry)
    {
        const size_t       sourceBytes = strlen(entry.pSourcePath) + 1;
        const size_t       pathBytes = strlen(entry.location.pPath) + 1;
        const size_t       dependencyBytes = entry.dependencies.count * sizeof(AssetID);
        AssetCatalogEntry* pEntry = (AssetCatalogEntry*)tf_malloc(sizeof(AssetCatalogEntry) + dependencyBytes + sourceBytes + pathBytes);
        *pEntry = entry;
        AssetID* pDependencies = (AssetID*)(pEntry + 1);
        if (dependencyBytes)
            memcpy(pDependencies, entry.dependencies.pData, dependencyBytes);
        char* pSource = (char*)pDependencies + dependencyBytes;
        memcpy(pSource, entry.pSourcePath, sourceBytes);
        memcpy(pSource + sourceBytes, entry.location.pPath, pathBytes);
        pEntry->pSourcePath = pSource;
        pEntry->location.pPath = pSource + sourceBytes;
        pEntry->dependencies = dependencyBytes ? Span<AssetID>(pDependencies, entry.dependencies.count) : Span<AssetID>{};
        const CatalogRecord record = { .key = entry.id, .value = pEntry };
        hmputs(pRecords, record);
    }

    ~AssetCatalogData()
    {
        for (ptrdiff_t i = 0; i < hmlen(pRecords); ++i)
            tf_free(pRecords[i].value);
        hmfree(pRecords);
        for (AssetCatalogEntry* pEntry : retiredEntries)
            tf_free(pEntry);
    }

    CatalogRecord*            pRecords = nullptr;
    Array<AssetCatalogEntry*> retiredEntries;
};

static int compareIDs(const void* pA, const void* pB)
{
    const AssetID& a = *(const AssetID*)pA;
    const AssetID& b = *(const AssetID*)pB;
    return (b < a) - (a < b);
}

class CatalogBatch
{
public:
    explicit CatalogBatch(const AssetCatalogEntry& entry)
    {
        dependencies.reserve(entry.dependencies.count);
        for (AssetID id : entry.dependencies)
            dependencies.pushBack(id);
        entries.pushBack(entry);
        entries[0].dependencies = finishDependencies(0);
    }

    explicit CatalogBatch(const SceneAsset& scene, const char* pPath)
    {
        uint32_t dependencyCount = scene.getNodes().count + scene.getMaterials().count * 4;
        for (const SceneAssetMesh& mesh : scene.getMeshes())
            dependencyCount += mesh.submeshes.count;
        dependencies.reserve(dependencyCount);
        entries.reserve(1 + scene.getTextures().count + scene.getMaterials().count + scene.getMeshes().count);
        for (const SceneAssetTexture& texture : scene.getTextures())
            entries.pushBack(
                { .id = texture.id,
                  .type = AssetType::Texture,
                  .pSourcePath = texture.pSource,
                  .location = { .pPath = texture.pPath, .root = texture.cooked ? AssetPathRoot::Cooked : AssetPathRoot::Source },
                  .srgb = texture.srgb });
        uint32_t index = 0;
        for (const SceneAssetMaterial& material : scene.getMaterials())
        {
            const uint32_t start = dependencies.size();
            addDependency(material.baseColorTexture);
            addDependency(material.normalTexture);
            addDependency(material.metallicRoughnessTexture);
            addDependency(material.emissiveTexture);
            entries.pushBack({ .id = material.id,
                               .type = AssetType::Material,
                               .pSourcePath = scene.getSource(),
                               .location = { .pPath = pPath, .elementIndex = index++ },
                               .dependencies = finishDependencies(start) });
        }
        index = 0;
        for (const SceneAssetMesh& mesh : scene.getMeshes())
        {
            const uint32_t start = dependencies.size();
            for (const SceneAssetSubmesh& submesh : mesh.submeshes)
                addDependency(submesh.material);
            entries.pushBack({ .id = mesh.id,
                               .type = AssetType::Mesh,
                               .pSourcePath = scene.getSource(),
                               .location = { .pPath = pPath, .elementIndex = index++ },
                               .dependencies = finishDependencies(start) });
        }
        const uint32_t start = dependencies.size();
        for (const SceneAssetNode& node : scene.getNodes())
            addDependency(node.mesh);
        entries.pushBack({ .id = scene.getID(),
                           .type = AssetType::Scene,
                           .pSourcePath = scene.getSource(),
                           .location = { .pPath = pPath },
                           .dependencies = finishDependencies(start) });
    }

    Span<AssetCatalogEntry> getEntries() const { return { entries.data(), entries.size() }; }

private:
    void addDependency(AssetID id)
    {
        if (id.isValid())
            dependencies.pushBack(id);
    }

    Span<AssetID> finishDependencies(uint32_t start)
    {
        if (start == dependencies.size())
            return {};
        qsort(dependencies.data() + start, dependencies.size() - start, sizeof(AssetID), compareIDs);
        uint32_t end = start + 1;
        for (uint32_t i = end; i < dependencies.size(); ++i)
            if (dependencies[i] != dependencies[end - 1])
                dependencies[end++] = dependencies[i];
        dependencies.resize(end);
        return { dependencies.data() + start, end - start };
    }

    Array<AssetCatalogEntry> entries;
    Array<AssetID>           dependencies;
};

static bool sameEntry(const AssetCatalogEntry& a, const AssetCatalogEntry& b)
{
    if (a.type != b.type || a.location.root != b.location.root || a.location.elementIndex != b.location.elementIndex || a.srgb != b.srgb ||
        strcmp(a.pSourcePath, b.pSourcePath) || strcmp(a.location.pPath, b.location.pPath) || a.dependencies.count != b.dependencies.count)
        return false;
    for (uint32_t i = 0; i < a.dependencies.count; ++i)
        if (a.dependencies.pData[i] != b.dependencies.pData[i])
            return false;
    return true;
}

static bool validPath(const char* pPath)
{
    if (!pPath || !*pPath || *pPath == '/' || strchr(pPath, '\\') || strchr(pPath, ':'))
        return false;
    const char* pSegment = pPath;
    for (const char* pCursor = pPath;; ++pCursor)
        if (*pCursor == '/' || !*pCursor)
        {
            if (pCursor == pSegment || (pCursor - pSegment == 1 && *pSegment == '.') ||
                (pCursor - pSegment == 2 && pSegment[0] == '.' && pSegment[1] == '.'))
                return false;
            if (!*pCursor)
                return true;
            pSegment = pCursor + 1;
        }
}

AssetCatalog::~AssetCatalog() { clear(); }
AssetCatalog::AssetCatalog(AssetCatalog&& other) noexcept: pData(other.pData) { other.pData = nullptr; }

AssetCatalog& AssetCatalog::operator=(AssetCatalog&& other) noexcept
{
    if (this != &other)
    {
        clear();
        pData = other.pData;
        other.pData = nullptr;
    }
    return *this;
}

bool AssetCatalog::add(const AssetCatalogEntry& entry) { return set(entry, false); }
bool AssetCatalog::replace(const AssetCatalogEntry& entry) { return set(entry, true); }

bool AssetCatalog::set(const AssetCatalogEntry& entry, bool replaceExisting)
{
    if (!entry.id.isValid() || entry.type > AssetType::Texture || entry.location.root > AssetPathRoot::Cooked ||
        !validPath(entry.pSourcePath) || !validPath(entry.location.pPath) || (entry.dependencies.count && !entry.dependencies.pData))
    {
        LOGF(eERROR, "Invalid AssetCatalog entry");
        return false;
    }
    for (AssetID id : entry.dependencies)
        if (!id.isValid())
        {
            LOGF(eERROR, "Invalid AssetCatalog dependency");
            return false;
        }
    const CatalogBatch       batch(entry);
    const AssetCatalogEntry& normalized = batch.getEntries().pData[0];
    const AssetCatalogEntry* pExisting = find(entry.id);
    if (pExisting)
    {
        if (sameEntry(*pExisting, normalized))
            return true;
        if (!replaceExisting || pExisting->type != entry.type)
        {
            LOGF(eERROR, "Conflicting AssetCatalog entry from '%s'", entry.pSourcePath);
            return false;
        }
        pData->retiredEntries.pushBack((AssetCatalogEntry*)pExisting);
    }
    if (!pData)
        pData = tf_new(AssetCatalogData);
    pData->insert(normalized);
    return true;
}

bool AssetCatalog::addScene(const SceneAsset& scene, const char* pCookedScenePath)
{
    if (!scene.isValid() || !validPath(pCookedScenePath))
    {
        LOGF(eERROR, "AssetCatalog requires a decoded SceneAsset and a relative cooked path");
        return false;
    }
    const CatalogBatch batch(scene, pCookedScenePath);
    for (const AssetCatalogEntry& entry : batch.getEntries())
    {
        const AssetCatalogEntry* pExisting = find(entry.id);
        if (pExisting && !sameEntry(*pExisting, entry))
        {
            char id[kIDStringCapacity];
            entry.id.toString(id);
            LOGF(eERROR, "Conflicting AssetCatalog entry '%s' from '%s'", id, pCookedScenePath);
            return false;
        }
    }
    if (!pData)
        pData = tf_new(AssetCatalogData);
    for (const AssetCatalogEntry& entry : batch.getEntries())
    {
        if (find(entry.id))
            continue;
        pData->insert(entry);
    }
    return true;
}

const AssetCatalogEntry* AssetCatalog::find(AssetID id) const
{
    if (!pData || !id.isValid())
        return nullptr;
    const CatalogRecord* pRecord = hmgetp_null(pData->pRecords, id);
    return pRecord ? pRecord->value : nullptr;
}

uint32_t                 AssetCatalog::getCount() const { return pData ? (uint32_t)hmlen(pData->pRecords) : 0; }
const AssetCatalogEntry* AssetCatalog::findScene(const char* pCookedScenePath) const
{
    if (!pData || !pCookedScenePath)
        return nullptr;
    for (ptrdiff_t i = 0; i < hmlen(pData->pRecords); ++i)
    {
        const AssetCatalogEntry* pEntry = pData->pRecords[i].value;
        if (pEntry->type == AssetType::Scene && pEntry->location.root == AssetPathRoot::Cooked &&
            !strcmp(pEntry->location.pPath, pCookedScenePath))
            return pEntry;
    }
    return nullptr;
}

bool AssetCatalog::remove(AssetID id)
{
    if (!pData)
        return false;
    const CatalogRecord* pRecord = hmgetp_null(pData->pRecords, id);
    if (!pRecord)
        return false;
    pData->retiredEntries.pushBack(pRecord->value);
    hmdel(pData->pRecords, id);
    return true;
}

bool AssetCatalog::merge(const AssetCatalog& other)
{
    if (!other.pData)
        return true;
    for (ptrdiff_t i = 0; i < hmlen(other.pData->pRecords); ++i)
    {
        const AssetCatalogEntry& entry = *other.pData->pRecords[i].value;
        const AssetCatalogEntry* pExisting = find(entry.id);
        if (pExisting && !sameEntry(*pExisting, entry))
        {
            LOGF(eERROR, "Conflicting AssetCatalog entry from '%s'", entry.pSourcePath);
            return false;
        }
    }
    if (!pData)
        pData = tf_new(AssetCatalogData);
    for (ptrdiff_t i = 0; i < hmlen(other.pData->pRecords); ++i)
    {
        const AssetCatalogEntry& entry = *other.pData->pRecords[i].value;
        if (!find(entry.id))
            pData->insert(entry);
    }
    return true;
}

class CatalogJson
{
public:
    ~CatalogJson() { cJSON_Delete(pRoot); }
    cJSON* pRoot = nullptr;
};

static const char* catalogString(const cJSON* pObject, const char* pName)
{
    const cJSON* pValue = cJSON_GetObjectItemCaseSensitive(pObject, pName);
    return cJSON_IsString(pValue) ? pValue->valuestring : nullptr;
}

static bool catalogNumber(const cJSON* pValue, uint32_t& value)
{
    if (!cJSON_IsNumber(pValue) || !(pValue->valuedouble >= 0 && pValue->valuedouble <= UINT32_MAX) ||
        pValue->valuedouble != (uint32_t)pValue->valuedouble)
        return false;
    value = (uint32_t)pValue->valuedouble;
    return true;
}

static bool uniqueFields(const cJSON* pObject)
{
    if (!cJSON_IsObject(pObject))
        return false;
    for (const cJSON* pA = pObject->child; pA; pA = pA->next)
        for (const cJSON* pB = pA->next; pB; pB = pB->next)
            if (!strcmp(pA->string, pB->string))
                return false;
    return true;
}

static bool readCatalog(Span<char> json, AssetCatalog& catalog)
{
    CatalogJson document;
    const char* pEnd = nullptr;
    if (json.pData && json.count && json.count <= 64 * 1024 * 1024)
        document.pRoot = cJSON_ParseWithLengthOpts(json.pData, json.count, &pEnd, false);
    if (!document.pRoot)
        return false;
    while (pEnd < json.pData + json.count && isspace((unsigned char)*pEnd))
        ++pEnd;
    if (pEnd < json.pData + json.count && !*pEnd && pEnd + 1 == json.pData + json.count)
        ++pEnd;
    const cJSON* pRoot = document.pRoot;
    const char*  pFormat = catalogString(pRoot, "format");
    uint32_t     version = 0;
    const cJSON* pEntries = cJSON_GetObjectItemCaseSensitive(pRoot, "entries");
    if (pEnd != json.pData + json.count || !uniqueFields(pRoot) || !pFormat || strcmp(pFormat, "Horizon.AssetCatalog") ||
        !catalogNumber(cJSON_GetObjectItemCaseSensitive(pRoot, "version"), version) || version != 1 || !cJSON_IsArray(pEntries))
        return false;
    Array<AssetID> dependencies;
    for (const cJSON* pEntry = pEntries->child; pEntry; pEntry = pEntry->next)
    {
        AssetCatalogEntry entry;
        uint32_t          type = 0, root = 0;
        const char*       pID = catalogString(pEntry, "id");
        const cJSON*      pIndex = cJSON_GetObjectItemCaseSensitive(pEntry, "elementIndex");
        const cJSON*      pSrgb = cJSON_GetObjectItemCaseSensitive(pEntry, "srgb");
        const cJSON*      pDependencies = cJSON_GetObjectItemCaseSensitive(pEntry, "dependencies");
        entry.pSourcePath = catalogString(pEntry, "source");
        entry.location.pPath = catalogString(pEntry, "path");
        if (!uniqueFields(pEntry) || !pID || !entry.id.parse(pID) || catalog.find(entry.id) ||
            !catalogNumber(cJSON_GetObjectItemCaseSensitive(pEntry, "type"), type) || type > (uint32_t)AssetType::Texture ||
            !catalogNumber(cJSON_GetObjectItemCaseSensitive(pEntry, "root"), root) || root > (uint32_t)AssetPathRoot::Cooked ||
            (!cJSON_IsNull(pIndex) && !catalogNumber(pIndex, entry.location.elementIndex)) || !cJSON_IsBool(pSrgb) ||
            !cJSON_IsArray(pDependencies))
            return false;
        entry.type = (AssetType)type;
        entry.location.root = (AssetPathRoot)root;
        entry.srgb = cJSON_IsTrue(pSrgb);
        dependencies.clear();
        for (const cJSON* pDependency = pDependencies->child; pDependency; pDependency = pDependency->next)
        {
            AssetID id;
            if (!cJSON_IsString(pDependency) || !id.parse(pDependency->valuestring) || !id.isValid())
                return false;
            dependencies.pushBack(id);
        }
        entry.dependencies = { dependencies.data(), dependencies.size() };
        if (!catalog.add(entry))
            return false;
    }
    return true;
}

bool AssetCatalog::parse(Span<char> json)
{
    AssetCatalog candidate;
    if (!readCatalog(json, candidate))
    {
        LOGF(eERROR, "Invalid AssetCatalog JSON");
        return false;
    }
    if (candidate.pData)
        for (ptrdiff_t i = 0; i < hmlen(candidate.pData->pRecords); ++i)
        {
            const AssetCatalogEntry& entry = *candidate.pData->pRecords[i].value;
            const AssetCatalogEntry* pExisting = find(entry.id);
            if (pExisting && pExisting->type != entry.type)
            {
                LOGF(eERROR, "AssetCatalog replacement cannot change an AssetID's type");
                return false;
            }
        }
    if (!pData)
        pData = tf_new(AssetCatalogData);
    for (ptrdiff_t i = 0; i < hmlen(pData->pRecords); ++i)
        pData->retiredEntries.pushBack(pData->pRecords[i].value);
    hmfree(pData->pRecords);
    if (candidate.pData)
    {
        pData->pRecords = candidate.pData->pRecords;
        candidate.pData->pRecords = nullptr;
    }
    return true;
}

bool AssetCatalog::load(ResourceDirectory directory, const char* pFileName)
{
    if (!validPath(pFileName))
    {
        LOGF(eERROR, "AssetCatalog requires a relative index path");
        return false;
    }
    FileStream stream = {};
    if (!fsOpenStreamFromPath(directory, pFileName, FM_READ, &stream))
        return false;
    const ssize_t size = fsGetStreamFileSize(&stream);
    if (size <= 0 || size > 64 * 1024 * 1024)
    {
        fsCloseStream(&stream);
        LOGF(eERROR, "Invalid AssetCatalog file size: '%s'", pFileName);
        return false;
    }
    Array<char>  json((uint32_t)size);
    const size_t bytesRead = fsReadFromStream(&stream, json.data(), (size_t)size);
    fsCloseStream(&stream);
    if (bytesRead != (size_t)size)
    {
        LOGF(eERROR, "Cannot read AssetCatalog '%s'", pFileName);
        return false;
    }
    return parse({ json.data(), json.size() });
}

static int compareEntries(const void* pA, const void* pB)
{
    return compareIDs(&(*(const AssetCatalogEntry* const*)pA)->id, &(*(const AssetCatalogEntry* const*)pB)->id);
}

bool AssetCatalog::serialize(Array<char>& json) const
{
    Array<const AssetCatalogEntry*> entries;
    entries.reserve(getCount());
    if (pData)
        for (ptrdiff_t i = 0; i < hmlen(pData->pRecords); ++i)
            entries.pushBack(pData->pRecords[i].value);
    if (!entries.empty())
        qsort(entries.data(), entries.size(), sizeof(entries[0]), compareEntries);
    CatalogJson document;
    document.pRoot = cJSON_CreateObject();
    cJSON_AddStringToObject(document.pRoot, "format", "Horizon.AssetCatalog");
    cJSON_AddNumberToObject(document.pRoot, "version", 1);
    cJSON* pEntries = cJSON_AddArrayToObject(document.pRoot, "entries");
    for (const AssetCatalogEntry* pEntry : entries)
    {
        cJSON* pValue = cJSON_CreateObject();
        cJSON_AddItemToArray(pEntries, pValue);
        char id[kIDStringCapacity];
        pEntry->id.toString(id);
        cJSON_AddStringToObject(pValue, "id", id);
        cJSON_AddNumberToObject(pValue, "type", (uint32_t)pEntry->type);
        cJSON_AddStringToObject(pValue, "source", pEntry->pSourcePath);
        cJSON_AddStringToObject(pValue, "path", pEntry->location.pPath);
        cJSON_AddNumberToObject(pValue, "root", (uint32_t)pEntry->location.root);
        if (pEntry->location.elementIndex == UINT32_MAX)
            cJSON_AddNullToObject(pValue, "elementIndex");
        else
            cJSON_AddNumberToObject(pValue, "elementIndex", pEntry->location.elementIndex);
        cJSON_AddBoolToObject(pValue, "srgb", pEntry->srgb);
        cJSON* pDependencies = cJSON_AddArrayToObject(pValue, "dependencies");
        for (AssetID dependency : pEntry->dependencies)
        {
            dependency.toString(id);
            cJSON_AddItemToArray(pDependencies, cJSON_CreateString(id));
        }
    }
    char* pText = cJSON_PrintUnformatted(document.pRoot);
    if (!pText)
        return false;
    json.resize((uint32_t)strlen(pText) + 1);
    memcpy(json.data(), pText, json.size());
    cJSON_free(pText);
    return true;
}

void AssetCatalog::clear()
{
    tf_delete(pData);
    pData = nullptr;
}
} // namespace hz
