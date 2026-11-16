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
struct CatalogIndex
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
        const size_t       dependencyBytes = entry.dependencies.Count * sizeof(AssetID);
        AssetCatalogEntry* pEntry = (AssetCatalogEntry*)MemAllocate(sizeof(AssetCatalogEntry) + dependencyBytes + sourceBytes + pathBytes);
        *pEntry = entry;
        AssetID* pDependencies = (AssetID*)(pEntry + 1);
        if (dependencyBytes)
            memcpy(pDependencies, entry.dependencies.Data, dependencyBytes);
        char* pSource = (char*)pDependencies + dependencyBytes;
        memcpy(pSource, entry.pSourcePath, sourceBytes);
        memcpy(pSource + sourceBytes, entry.location.pPath, pathBytes);
        pEntry->pSourcePath = pSource;
        pEntry->location.pPath = pSource + sourceBytes;
        pEntry->dependencies = dependencyBytes ? Span<AssetID>(pDependencies, entry.dependencies.Count) : Span<AssetID>{};
        const CatalogIndex mapping = { .key = entry.id, .value = pEntry };
        hmputs(pIndices, mapping);
    }

    ~AssetCatalogData()
    {
        for (ptrdiff_t i = 0; i < hmlen(pIndices); ++i)
        {
            MemDeallocate(pIndices[i].value);
        }
        hmfree(pIndices);
        for (AssetCatalogEntry* pEntry : retiredEntries)
            MemDeallocate(pEntry);
    }

    CatalogIndex*             pIndices = nullptr;
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
        dependencies.Reserve(entry.dependencies.Count);
        for (AssetID id : entry.dependencies)
            dependencies.PushBack(id);
        entries.PushBack(entry);
        entries[0].dependencies = finishDependencies(0);
    }

    explicit CatalogBatch(const SceneAsset& scene, const char* pPath)
    {
        uint32_t dependencyCount = scene.getNodes().Count + scene.getMaterials().Count * 4;
        for (const SceneAssetMesh& mesh : scene.getMeshes())
            dependencyCount += mesh.submeshes.Count;
        dependencies.Reserve(dependencyCount);
        entries.Reserve(1 + scene.getTextures().Count + scene.getMaterials().Count + scene.getMeshes().Count);
        for (const SceneAssetTexture& texture : scene.getTextures())
            entries.PushBack(
                { .id = texture.id,
                  .Type = AssetType::Texture,
                  .pSourcePath = texture.pSource,
                  .location = { .pPath = texture.pPath, .root = texture.cooked ? AssetPathRoot::Cooked : AssetPathRoot::Source },
                  .srgb = texture.srgb });
        uint32_t index = 0;
        for (const SceneAssetMaterial& material : scene.getMaterials())
        {
            const uint32_t start = dependencies.Size();
            addDependency(material.baseColorTexture);
            addDependency(material.normalTexture);
            addDependency(material.metallicRoughnessTexture);
            addDependency(material.emissiveTexture);
            entries.PushBack({ .id = material.id,
                               .Type = AssetType::Material,
                               .pSourcePath = scene.getSource(),
                               .location = { .pPath = pPath, .elementIndex = index++ },
                               .dependencies = finishDependencies(start) });
        }
        index = 0;
        for (const SceneAssetMesh& mesh : scene.getMeshes())
        {
            const uint32_t start = dependencies.Size();
            for (const SceneAssetSubmesh& submesh : mesh.submeshes)
                addDependency(submesh.material);
            entries.PushBack({ .id = mesh.id,
                               .Type = AssetType::Mesh,
                               .pSourcePath = scene.getSource(),
                               .location = { .pPath = pPath, .elementIndex = index++ },
                               .dependencies = finishDependencies(start) });
        }
        const uint32_t start = dependencies.Size();
        for (const SceneAssetNode& node : scene.getNodes())
            addDependency(node.mesh);
        entries.PushBack({ .id = scene.getID(),
                           .Type = AssetType::Scene,
                           .pSourcePath = scene.getSource(),
                           .location = { .pPath = pPath },
                           .dependencies = finishDependencies(start) });
    }

    Span<AssetCatalogEntry> getEntries() const { return { entries.GetData(), entries.Size() }; }

private:
    void addDependency(AssetID id)
    {
        if (id.IsValid())
            dependencies.PushBack(id);
    }

    Span<AssetID> finishDependencies(uint32_t start)
    {
        if (start == dependencies.Size())
            return {};
        qsort(dependencies.GetData() + start, dependencies.Size() - start, sizeof(AssetID), compareIDs);
        uint32_t end = start + 1;
        for (uint32_t i = end; i < dependencies.Size(); ++i)
            if (dependencies[i] != dependencies[end - 1])
                dependencies[end++] = dependencies[i];
        dependencies.Resize(end);
        return { dependencies.GetData() + start, end - start };
    }

    Array<AssetCatalogEntry> entries;
    Array<AssetID>           dependencies;
};

static bool sameEntry(const AssetCatalogEntry& a, const AssetCatalogEntry& b)
{
    if (a.Type != b.Type || a.location.root != b.location.root || a.location.elementIndex != b.location.elementIndex || a.srgb != b.srgb ||
        strcmp(a.pSourcePath, b.pSourcePath) || strcmp(a.location.pPath, b.location.pPath) || a.dependencies.Count != b.dependencies.Count)
        return false;
    for (uint32_t i = 0; i < a.dependencies.Count; ++i)
        if (a.dependencies.Data[i] != b.dependencies.Data[i])
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

AssetCatalog::~AssetCatalog() { Clear(); }
AssetCatalog::AssetCatalog(AssetCatalog&& other) noexcept: Data(other.Data) { other.Data = nullptr; }

AssetCatalog& AssetCatalog::operator=(AssetCatalog&& other) noexcept
{
    if (this != &other)
    {
        Clear();
        Data = other.Data;
        other.Data = nullptr;
    }
    return *this;
}

bool AssetCatalog::add(const AssetCatalogEntry& entry) { return set(entry, false); }
bool AssetCatalog::replace(const AssetCatalogEntry& entry) { return set(entry, true); }

bool AssetCatalog::set(const AssetCatalogEntry& entry, bool replaceExisting)
{
    if (!entry.id.IsValid() || entry.Type > AssetType::Texture || entry.location.root > AssetPathRoot::Cooked ||
        !validPath(entry.pSourcePath) || !validPath(entry.location.pPath) || (entry.dependencies.Count && !entry.dependencies.Data))
    {
        LOGF(Error, "Invalid AssetCatalog entry");
        return false;
    }
    for (AssetID id : entry.dependencies)
        if (!id.IsValid())
        {
            LOGF(Error, "Invalid AssetCatalog dependency");
            return false;
        }
    const CatalogBatch       batch(entry);
    const AssetCatalogEntry& normalized = batch.getEntries().Data[0];
    const AssetCatalogEntry* pExisting = find(entry.id);
    if (pExisting)
    {
        if (sameEntry(*pExisting, normalized))
            return true;
        if (!replaceExisting || pExisting->Type != entry.Type)
        {
            LOGF(Error, "Conflicting AssetCatalog entry from '%s'", entry.pSourcePath);
            return false;
        }
        Data->retiredEntries.PushBack((AssetCatalogEntry*)pExisting);
    }
    if (!Data)
        Data = MemNew(AssetCatalogData);
    Data->insert(normalized);
    return true;
}

bool AssetCatalog::addScene(const SceneAsset& scene, const char* pCookedScenePath)
{
    if (!scene.IsValid() || !validPath(pCookedScenePath))
    {
        LOGF(Error, "AssetCatalog requires a decoded SceneAsset and a relative cooked path");
        return false;
    }
    const CatalogBatch batch(scene, pCookedScenePath);
    for (const AssetCatalogEntry& entry : batch.getEntries())
    {
        const AssetCatalogEntry* pExisting = find(entry.id);
        if (pExisting && !sameEntry(*pExisting, entry))
        {
            char id[kIDStringCapacity];
            entry.id.ToString(id);
            LOGF(Error, "Conflicting AssetCatalog entry '%s' from '%s'", id, pCookedScenePath);
            return false;
        }
    }
    if (!Data)
        Data = MemNew(AssetCatalogData);
    for (const AssetCatalogEntry& entry : batch.getEntries())
    {
        if (find(entry.id))
            continue;
        Data->insert(entry);
    }
    return true;
}

const AssetCatalogEntry* AssetCatalog::find(AssetID id) const
{
    if (!Data || !id.IsValid())
        return nullptr;
    const CatalogIndex* pIndex = hmgetp_null(Data->pIndices, id);
    return pIndex ? pIndex->value : nullptr;
}

uint32_t                 AssetCatalog::getCount() const { return Data ? (uint32_t)hmlen(Data->pIndices) : 0; }
const AssetCatalogEntry* AssetCatalog::findScene(const char* pCookedScenePath) const
{
    if (!Data || !pCookedScenePath)
        return nullptr;
    for (ptrdiff_t i = 0; i < hmlen(Data->pIndices); ++i)
    {
        const AssetCatalogEntry* pEntry = Data->pIndices[i].value;
        if (pEntry->Type == AssetType::Scene && pEntry->location.root == AssetPathRoot::Cooked &&
            !strcmp(pEntry->location.pPath, pCookedScenePath))
            return pEntry;
    }
    return nullptr;
}

bool AssetCatalog::remove(AssetID id)
{
    if (!Data)
        return false;
    const CatalogIndex* pIndex = hmgetp_null(Data->pIndices, id);
    if (!pIndex)
    {
        return false;
    }
    Data->retiredEntries.PushBack(pIndex->value);
    hmdel(Data->pIndices, id);
    return true;
}

bool AssetCatalog::merge(const AssetCatalog& other)
{
    if (!other.Data)
        return true;
    for (ptrdiff_t i = 0; i < hmlen(other.Data->pIndices); ++i)
    {
        const AssetCatalogEntry& entry = *other.Data->pIndices[i].value;
        const AssetCatalogEntry* pExisting = find(entry.id);
        if (pExisting && !sameEntry(*pExisting, entry))
        {
            LOGF(Error, "Conflicting AssetCatalog entry from '%s'", entry.pSourcePath);
            return false;
        }
    }
    if (!Data)
        Data = MemNew(AssetCatalogData);
    for (ptrdiff_t i = 0; i < hmlen(other.Data->pIndices); ++i)
    {
        const AssetCatalogEntry& entry = *other.Data->pIndices[i].value;
        if (!find(entry.id))
            Data->insert(entry);
    }
    return true;
}

class CatalogJson
{
public:
    ~CatalogJson() { cJSON_Delete(pRoot); }
    cJSON* pRoot = nullptr;
};

static const char* catalogString(const cJSON* pObject, const char* Name)
{
    const cJSON* pValue = cJSON_GetObjectItemCaseSensitive(pObject, Name);
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
    if (json.Data && json.Count && json.Count <= 64 * 1024 * 1024)
        document.pRoot = cJSON_ParseWithLengthOpts(json.Data, json.Count, &pEnd, false);
    if (!document.pRoot)
        return false;
    while (pEnd < json.Data + json.Count && isspace((unsigned char)*pEnd))
        ++pEnd;
    if (pEnd < json.Data + json.Count && !*pEnd && pEnd + 1 == json.Data + json.Count)
        ++pEnd;
    const cJSON* pRoot = document.pRoot;
    const char*  pFormat = catalogString(pRoot, "format");
    uint32_t     version = 0;
    const cJSON* pEntries = cJSON_GetObjectItemCaseSensitive(pRoot, "entries");
    if (pEnd != json.Data + json.Count || !uniqueFields(pRoot) || !pFormat || strcmp(pFormat, "Horizon.AssetCatalog") ||
        !catalogNumber(cJSON_GetObjectItemCaseSensitive(pRoot, "version"), version) || version != 1 || !cJSON_IsArray(pEntries))
        return false;
    Array<AssetID> dependencies;
    for (const cJSON* pEntry = pEntries->child; pEntry; pEntry = pEntry->next)
    {
        AssetCatalogEntry entry;
        uint32_t          Type = 0, root = 0;
        const char*       pID = catalogString(pEntry, "id");
        const cJSON*      pIndex = cJSON_GetObjectItemCaseSensitive(pEntry, "elementIndex");
        const cJSON*      pSrgb = cJSON_GetObjectItemCaseSensitive(pEntry, "srgb");
        const cJSON*      pDependencies = cJSON_GetObjectItemCaseSensitive(pEntry, "dependencies");
        entry.pSourcePath = catalogString(pEntry, "source");
        entry.location.pPath = catalogString(pEntry, "path");
        if (!uniqueFields(pEntry) || !pID || !entry.id.Parse(pID) || catalog.find(entry.id) ||
            !catalogNumber(cJSON_GetObjectItemCaseSensitive(pEntry, "type"), Type) || Type > (uint32_t)AssetType::Texture ||
            !catalogNumber(cJSON_GetObjectItemCaseSensitive(pEntry, "root"), root) || root > (uint32_t)AssetPathRoot::Cooked ||
            (!cJSON_IsNull(pIndex) && !catalogNumber(pIndex, entry.location.elementIndex)) || !cJSON_IsBool(pSrgb) ||
            !cJSON_IsArray(pDependencies))
            return false;
        entry.Type = (AssetType)Type;
        entry.location.root = (AssetPathRoot)root;
        entry.srgb = cJSON_IsTrue(pSrgb);
        dependencies.Clear();
        for (const cJSON* pDependency = pDependencies->child; pDependency; pDependency = pDependency->next)
        {
            AssetID id;
            if (!cJSON_IsString(pDependency) || !id.Parse(pDependency->valuestring) || !id.IsValid())
                return false;
            dependencies.PushBack(id);
        }
        entry.dependencies = { dependencies.GetData(), dependencies.Size() };
        if (!catalog.add(entry))
            return false;
    }
    return true;
}

bool AssetCatalog::Parse(Span<char> json)
{
    AssetCatalog candidate;
    if (!readCatalog(json, candidate))
    {
        LOGF(Error, "Invalid AssetCatalog JSON");
        return false;
    }
    if (candidate.Data)
        for (ptrdiff_t i = 0; i < hmlen(candidate.Data->pIndices); ++i)
        {
            const AssetCatalogEntry& entry = *candidate.Data->pIndices[i].value;
            const AssetCatalogEntry* pExisting = find(entry.id);
            if (pExisting && pExisting->Type != entry.Type)
            {
                LOGF(Error, "AssetCatalog replacement cannot change an AssetID's type");
                return false;
            }
        }
    if (!Data)
        Data = MemNew(AssetCatalogData);
    for (ptrdiff_t i = 0; i < hmlen(Data->pIndices); ++i)
    {
        Data->retiredEntries.PushBack(Data->pIndices[i].value);
    }
    hmfree(Data->pIndices);
    if (candidate.Data)
    {
        Data->pIndices = candidate.Data->pIndices;
        candidate.Data->pIndices = nullptr;
    }
    return true;
}

bool AssetCatalog::load(ResourceDirectory directory, const char* pFileName)
{
    if (!validPath(pFileName))
    {
        LOGF(Error, "AssetCatalog requires a relative index path");
        return false;
    }
    FileStream stream = {};
    if (!OpenStreamFromPath(directory, pFileName, FileModeRead, &stream))
        return false;
    const ssize_t Size = GetStreamFileSize(&stream);
    if (Size <= 0 || Size > 64 * 1024 * 1024)
    {
        CloseStream(&stream);
        LOGF(Error, "Invalid AssetCatalog file size: '%s'", pFileName);
        return false;
    }
    Array<char>  json((uint32_t)Size);
    const size_t bytesRead = ReadFromStream(&stream, json.GetData(), (size_t)Size);
    CloseStream(&stream);
    if (bytesRead != (size_t)Size)
    {
        LOGF(Error, "Cannot read AssetCatalog '%s'", pFileName);
        return false;
    }
    return Parse({ json.GetData(), json.Size() });
}

static int compareEntries(const void* pA, const void* pB)
{
    return compareIDs(&(*(const AssetCatalogEntry* const*)pA)->id, &(*(const AssetCatalogEntry* const*)pB)->id);
}

bool AssetCatalog::serialize(Array<char>& json) const
{
    Array<const AssetCatalogEntry*> entries;
    entries.Reserve(getCount());
    if (Data)
        for (ptrdiff_t i = 0; i < hmlen(Data->pIndices); ++i)
        {
            entries.PushBack(Data->pIndices[i].value);
        }
    if (!entries.Empty())
        qsort(entries.GetData(), entries.Size(), sizeof(entries[0]), compareEntries);
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
        pEntry->id.ToString(id);
        cJSON_AddStringToObject(pValue, "id", id);
        cJSON_AddNumberToObject(pValue, "type", (uint32_t)pEntry->Type);
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
            dependency.ToString(id);
            cJSON_AddItemToArray(pDependencies, cJSON_CreateString(id));
        }
    }
    char* pText = cJSON_PrintUnformatted(document.pRoot);
    if (!pText)
        return false;
    json.Resize((uint32_t)strlen(pText) + 1);
    memcpy(json.GetData(), pText, json.Size());
    cJSON_free(pText);
    return true;
}

void AssetCatalog::Clear()
{
    MemDelete(Data);
    Data = nullptr;
}
} // namespace hz
