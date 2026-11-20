/* Copyright (c) 2026 Horizon */

#include "Scene/AssetCatalog.h"
#include "AssetDependencies.h"
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
    void insert(const AssetCatalogEntry& EntryValue)
    {
        const size_t       SourceBytes = strlen(EntryValue.pSourcePath) + 1;
        const size_t       PathBytes = strlen(EntryValue.location.pPath) + 1;
        const size_t       DependencyBytes = EntryValue.dependencies.Count * sizeof(AssetID);
        AssetCatalogEntry* EntryPointer = (AssetCatalogEntry*)MemAllocate(sizeof(AssetCatalogEntry) + DependencyBytes + SourceBytes + PathBytes);
        *EntryPointer = EntryValue;
        AssetID* DependenciesPointer = (AssetID*)(EntryPointer + 1);
        if (DependencyBytes)
            memcpy(DependenciesPointer, EntryValue.dependencies.Data, DependencyBytes);
        char* SourcePointer = (char*)DependenciesPointer + DependencyBytes;
        memcpy(SourcePointer, EntryValue.pSourcePath, SourceBytes);
        memcpy(SourcePointer + SourceBytes, EntryValue.location.pPath, PathBytes);
        EntryPointer->pSourcePath = SourcePointer;
        EntryPointer->location.pPath = SourcePointer + SourceBytes;
        EntryPointer->dependencies = DependencyBytes ? Span<AssetID>(DependenciesPointer, EntryValue.dependencies.Count) : Span<AssetID>{};
        const CatalogIndex Mapping = { .key = EntryValue.id, .value = EntryPointer };
        hmputs(pIndices, Mapping);
    }

    ~AssetCatalogData()
    {
        for (ptrdiff_t I = 0; I < hmlen(pIndices); ++I)
        {
            MemDeallocate(pIndices[I].value);
        }
        hmfree(pIndices);
        for (AssetCatalogEntry* EntryPointer : retiredEntries)
            MemDeallocate(EntryPointer);
    }

    CatalogIndex*             pIndices = nullptr;
    Array<AssetCatalogEntry*> retiredEntries;
};

class CatalogBatch
{
public:
    explicit CatalogBatch(const AssetCatalogEntry& EntryValue)
    {
        dependencies.Reserve(EntryValue.dependencies.Count);
        for (AssetID Id : EntryValue.dependencies)
            dependencies.PushBack(Id);
        entries.PushBack(EntryValue);
        entries[0].dependencies = finishDependencies(0);
    }

    explicit CatalogBatch(const SceneAsset& SceneValue, const char* Path)
    {
        uint32_t DependencyCount = SceneValue.GetNodes().Count * 3 + SceneValue.GetMaterials().Count * 4 + SceneValue.GetAnimations().Count;
        for (const SceneAssetMesh& MeshValue : SceneValue.GetMeshes())
        {
            DependencyCount += MeshValue.submeshes.Count;
        }
        dependencies.Reserve(DependencyCount);
        entries.Reserve(1 + SceneValue.GetTextures().Count + SceneValue.GetMaterials().Count + SceneValue.GetMeshes().Count + SceneValue.GetSkeletons().Count +
                        SceneValue.GetAnimations().Count);
        for (const SceneAssetTexture& TextureValue : SceneValue.GetTextures())
        {
            entries.PushBack(
                { .id = TextureValue.id,
                  .Type = AssetType::Texture,
                  .pSourcePath = TextureValue.pSource,
                  .location = { .pPath = TextureValue.pPath, .root = TextureValue.cooked ? AssetPathRoot::Cooked : AssetPathRoot::Source },
                  .srgb = TextureValue.srgb });
        }
        uint32_t IndexValue = 0;
        for (const SceneAssetMaterial& MaterialValue : SceneValue.GetMaterials())
        {
            const uint32_t Start = dependencies.Size();
            addDependency(MaterialValue.baseColorTexture);
            addDependency(MaterialValue.normalTexture);
            addDependency(MaterialValue.metallicRoughnessTexture);
            addDependency(MaterialValue.emissiveTexture);
            entries.PushBack({ .id = MaterialValue.id,
                               .Type = AssetType::Material,
                               .pSourcePath = SceneValue.GetSource(),
                               .location = { .pPath = Path, .elementIndex = IndexValue++ },
                               .dependencies = finishDependencies(Start) });
        }
        for (const SceneAssetSkeleton& SkeletonValue : SceneValue.GetSkeletons())
        {
            entries.PushBack({ .id = SkeletonValue.id,
                               .Type = AssetType::Skeleton,
                               .pSourcePath = SceneValue.GetSource(),
                               .location = { .pPath = SkeletonValue.pPath } });
        }
        for (const SceneAssetAnimation& Animation : SceneValue.GetAnimations())
        {
            // The clip's tracks follow the joints of its skeleton, so loading it also loads the skeleton.
            const uint32_t Start = dependencies.Size();
            addDependency(Animation.skeleton);
            entries.PushBack({ .id = Animation.id,
                               .Type = AssetType::AnimationClip,
                               .pSourcePath = SceneValue.GetSource(),
                               .location = { .pPath = Animation.pPath },
                               .dependencies = finishDependencies(Start) });
        }
        IndexValue = 0;
        for (const SceneAssetMesh& MeshValue : SceneValue.GetMeshes())
        {
            const uint32_t Start = dependencies.Size();
            for (const SceneAssetSubmesh& Submesh : MeshValue.submeshes)
                addDependency(Submesh.material);
            entries.PushBack({ .id = MeshValue.id,
                               .Type = AssetType::Mesh,
                               .pSourcePath = SceneValue.GetSource(),
                               .location = { .pPath = Path, .elementIndex = IndexValue++ },
                               .dependencies = finishDependencies(Start) });
        }
        const uint32_t Start = dependencies.Size();
        for (const SceneAssetNode& NodeValue : SceneValue.GetNodes())
        {
            addDependency(NodeValue.mesh);
            addDependency(NodeValue.skeleton);
            addDependency(NodeValue.animation);
        }
        entries.PushBack({ .id = SceneValue.GetID(),
                           .Type = AssetType::Scene,
                           .pSourcePath = SceneValue.GetSource(),
                           .location = { .pPath = Path },
                           .dependencies = finishDependencies(Start) });
    }

    Span<AssetCatalogEntry> getEntries() const { return { entries.GetData(), entries.Size() }; }

private:
    void addDependency(AssetID Id)
    {
        if (Id.IsValid())
        {
            dependencies.PushBack(Id);
        }
    }

    Span<AssetID> finishDependencies(uint32_t Start)
    {
        const Span<AssetID> Sorted = NormalizeAssetDependencies(dependencies, Start);
        return Sorted.Count ? Sorted : Span<AssetID>{};
    }

    Array<AssetCatalogEntry> entries;
    Array<AssetID>           dependencies;
};

static bool SameEntry(const AssetCatalogEntry& AValue, const AssetCatalogEntry& BValue)
{
    if (AValue.Type != BValue.Type || AValue.location.root != BValue.location.root || AValue.location.elementIndex != BValue.location.elementIndex || AValue.srgb != BValue.srgb ||
        strcmp(AValue.pSourcePath, BValue.pSourcePath) || strcmp(AValue.location.pPath, BValue.location.pPath) || AValue.dependencies.Count != BValue.dependencies.Count)
    {
        return false;
    }
    for (uint32_t I = 0; I < AValue.dependencies.Count; ++I)
    {
        if (AValue.dependencies.Data[I] != BValue.dependencies.Data[I])
        {
            return false;
        }
    }
    return true;
}

static bool ValidPath(const char* Path)
{
    if (!Path || !*Path || *Path == '/' || strchr(Path, '\\') || strchr(Path, ':'))
        return false;
    const char* Segment = Path;
    for (const char* Cursor = Path;; ++Cursor)
        if (*Cursor == '/' || !*Cursor)
        {
            if (Cursor == Segment || (Cursor - Segment == 1 && *Segment == '.') ||
                (Cursor - Segment == 2 && Segment[0] == '.' && Segment[1] == '.'))
                return false;
            if (!*Cursor)
                return true;
            Segment = Cursor + 1;
        }
}

AssetCatalog::~AssetCatalog() { Clear(); }
AssetCatalog::AssetCatalog(AssetCatalog&& Other) noexcept: Data(Other.Data) { Other.Data = nullptr; }

AssetCatalog& AssetCatalog::operator=(AssetCatalog&& Other) noexcept
{
    if (this != &Other)
    {
        Clear();
        Data = Other.Data;
        Other.Data = nullptr;
    }
    return *this;
}

bool AssetCatalog::Add(const AssetCatalogEntry& EntryValue) { return Set(EntryValue, false); }
bool AssetCatalog::Replace(const AssetCatalogEntry& EntryValue) { return Set(EntryValue, true); }

bool AssetCatalog::Set(const AssetCatalogEntry& EntryValue, bool ReplaceExisting)
{
    if (!EntryValue.id.IsValid() || EntryValue.Type > AssetType::AnimationClip || EntryValue.location.root > AssetPathRoot::Cooked ||
        !ValidPath(EntryValue.pSourcePath) || !ValidPath(EntryValue.location.pPath) || (EntryValue.dependencies.Count && !EntryValue.dependencies.Data))
    {
        LOGF(Error, "Invalid AssetCatalog entry");
        return false;
    }
    for (AssetID Id : EntryValue.dependencies)
        if (!Id.IsValid())
        {
            LOGF(Error, "Invalid AssetCatalog dependency");
            return false;
        }
    const CatalogBatch       Batch(EntryValue);
    const AssetCatalogEntry& Normalized = Batch.getEntries().Data[0];
    const AssetCatalogEntry* Existing = Find(EntryValue.id);
    if (Existing)
    {
        if (SameEntry(*Existing, Normalized))
            return true;
        if (!ReplaceExisting || Existing->Type != EntryValue.Type)
        {
            LOGF(Error, "Conflicting AssetCatalog entry from '%s'", EntryValue.pSourcePath);
            return false;
        }
        Data->retiredEntries.PushBack((AssetCatalogEntry*)Existing);
    }
    if (!Data)
    {
        Data = MemNew(AssetCatalogData);
    }
    Data->insert(Normalized);
    return true;
}

bool AssetCatalog::AddScene(const SceneAsset& SceneValue, const char* CookedScenePath)
{
    if (!SceneValue.IsValid() || !ValidPath(CookedScenePath))
    {
        LOGF(Error, "AssetCatalog requires a decoded SceneAsset and a relative cooked path");
        return false;
    }
    const CatalogBatch Batch(SceneValue, CookedScenePath);
    for (const AssetCatalogEntry& EntryValue : Batch.getEntries())
    {
        const AssetCatalogEntry* Existing = Find(EntryValue.id);
        if (Existing && !SameEntry(*Existing, EntryValue))
        {
            char Id[kIDStringCapacity];
            EntryValue.id.ToString(Id);
            LOGF(Error, "Conflicting AssetCatalog entry '%s' from '%s'", Id, CookedScenePath);
            return false;
        }
    }
    if (!Data)
    {
        Data = MemNew(AssetCatalogData);
    }
    for (const AssetCatalogEntry& EntryValue : Batch.getEntries())
    {
        if (Find(EntryValue.id))
        {
            continue;
        }
        Data->insert(EntryValue);
    }
    return true;
}

const AssetCatalogEntry* AssetCatalog::Find(AssetID Id) const
{
    if (!Data || !Id.IsValid())
    {
        return nullptr;
    }
    const CatalogIndex* IndexPointer = hmgetp_null(Data->pIndices, Id);
    return IndexPointer ? IndexPointer->value : nullptr;
}

uint32_t                 AssetCatalog::GetCount() const { return Data ? (uint32_t)hmlen(Data->pIndices) : 0; }
const AssetCatalogEntry* AssetCatalog::FindScene(const char* CookedScenePath) const
{
    if (!Data || !CookedScenePath)
    {
        return nullptr;
    }
    for (ptrdiff_t I = 0; I < hmlen(Data->pIndices); ++I)
    {
        const AssetCatalogEntry* EntryPointer = Data->pIndices[I].value;
        if (EntryPointer->Type == AssetType::Scene && EntryPointer->location.root == AssetPathRoot::Cooked &&
            !strcmp(EntryPointer->location.pPath, CookedScenePath))
        {
            return EntryPointer;
        }
    }
    return nullptr;
}

bool AssetCatalog::Remove(AssetID Id)
{
    if (!Data)
    {
        return false;
    }
    const CatalogIndex* IndexPointer = hmgetp_null(Data->pIndices, Id);
    if (!IndexPointer)
    {
        return false;
    }
    Data->retiredEntries.PushBack(IndexPointer->value);
    hmdel(Data->pIndices, Id);
    return true;
}

bool AssetCatalog::Merge(const AssetCatalog& Other)
{
    if (!Other.Data)
    {
        return true;
    }
    for (ptrdiff_t I = 0; I < hmlen(Other.Data->pIndices); ++I)
    {
        const AssetCatalogEntry& EntryValue = *Other.Data->pIndices[I].value;
        const AssetCatalogEntry* Existing = Find(EntryValue.id);
        if (Existing && !SameEntry(*Existing, EntryValue))
        {
            LOGF(Error, "Conflicting AssetCatalog entry from '%s'", EntryValue.pSourcePath);
            return false;
        }
    }
    if (!Data)
    {
        Data = MemNew(AssetCatalogData);
    }
    for (ptrdiff_t I = 0; I < hmlen(Other.Data->pIndices); ++I)
    {
        const AssetCatalogEntry& EntryValue = *Other.Data->pIndices[I].value;
        if (!Find(EntryValue.id))
        {
            Data->insert(EntryValue);
        }
    }
    return true;
}

class CatalogJson
{
public:
    ~CatalogJson() { cJSON_Delete(pRoot); }
    cJSON* pRoot = nullptr;
};

static const char* CatalogString(const cJSON* Object, const char* Name)
{
    const cJSON* ValueData = cJSON_GetObjectItemCaseSensitive(Object, Name);
    return cJSON_IsString(ValueData) ? ValueData->valuestring : nullptr;
}

static bool CatalogNumber(const cJSON* ValueData, uint32_t& ValueValue)
{
    if (!cJSON_IsNumber(ValueData) || !(ValueData->valuedouble >= 0 && ValueData->valuedouble <= UINT32_MAX) ||
        ValueData->valuedouble != (uint32_t)ValueData->valuedouble)
        return false;
    ValueValue = (uint32_t)ValueData->valuedouble;
    return true;
}

static bool UniqueFields(const cJSON* Object)
{
    if (!cJSON_IsObject(Object))
        return false;
    for (const cJSON* LeftValue = Object->child; LeftValue; LeftValue = LeftValue->next)
        for (const cJSON* RightValue = LeftValue->next; RightValue; RightValue = RightValue->next)
            if (!strcmp(LeftValue->string, RightValue->string))
                return false;
    return true;
}

static bool ReadCatalog(Span<char> Json, AssetCatalog& Catalog)
{
    CatalogJson Document;
    const char* End = nullptr;
    if (Json.Data && Json.Count && Json.Count <= 64 * 1024 * 1024)
    {
        Document.pRoot = cJSON_ParseWithLengthOpts(Json.Data, Json.Count, &End, false);
    }
    if (!Document.pRoot)
        return false;
    while (End < Json.Data + Json.Count && isspace((unsigned char)*End))
    {
        ++End;
    }
    if (End < Json.Data + Json.Count && !*End && End + 1 == Json.Data + Json.Count)
    {
        ++End;
    }
    const cJSON* RootPointer = Document.pRoot;
    const char*  FormatPointer = CatalogString(RootPointer, "format");
    uint32_t     Version = 0;
    const cJSON* EntriesPointer = cJSON_GetObjectItemCaseSensitive(RootPointer, "entries");
    if (End != Json.Data + Json.Count || !UniqueFields(RootPointer) || !FormatPointer || strcmp(FormatPointer, "Horizon.AssetCatalog") ||
        !CatalogNumber(cJSON_GetObjectItemCaseSensitive(RootPointer, "version"), Version) || Version != 1 || !cJSON_IsArray(EntriesPointer))
    {
        return false;
    }
    Array<AssetID> DependenciesValue;
    for (const cJSON* EntryPointer = EntriesPointer->child; EntryPointer; EntryPointer = EntryPointer->next)
    {
        AssetCatalogEntry EntryValue;
        uint32_t          Type = 0, RootValue = 0;
        const char*       ID = CatalogString(EntryPointer, "id");
        const cJSON*      IndexPointer = cJSON_GetObjectItemCaseSensitive(EntryPointer, "elementIndex");
        const cJSON*      Srgb = cJSON_GetObjectItemCaseSensitive(EntryPointer, "srgb");
        const cJSON*      DependenciesPointer = cJSON_GetObjectItemCaseSensitive(EntryPointer, "dependencies");
        EntryValue.pSourcePath = CatalogString(EntryPointer, "source");
        EntryValue.location.pPath = CatalogString(EntryPointer, "path");
        if (!UniqueFields(EntryPointer) || !ID || !EntryValue.id.Parse(ID) || Catalog.Find(EntryValue.id) ||
            !CatalogNumber(cJSON_GetObjectItemCaseSensitive(EntryPointer, "type"), Type) || Type > (uint32_t)AssetType::AnimationClip ||
            !CatalogNumber(cJSON_GetObjectItemCaseSensitive(EntryPointer, "root"), RootValue) || RootValue > (uint32_t)AssetPathRoot::Cooked ||
            (!cJSON_IsNull(IndexPointer) && !CatalogNumber(IndexPointer, EntryValue.location.elementIndex)) || !cJSON_IsBool(Srgb) ||
            !cJSON_IsArray(DependenciesPointer))
        {
            return false;
        }
        EntryValue.Type = (AssetType)Type;
        EntryValue.location.root = (AssetPathRoot)RootValue;
        EntryValue.srgb = cJSON_IsTrue(Srgb);
        DependenciesValue.Clear();
        for (const cJSON* DependencyPointer = DependenciesPointer->child; DependencyPointer; DependencyPointer = DependencyPointer->next)
        {
            AssetID Id;
            if (!cJSON_IsString(DependencyPointer) || !Id.Parse(DependencyPointer->valuestring) || !Id.IsValid())
            {
                return false;
            }
            DependenciesValue.PushBack(Id);
        }
        EntryValue.dependencies = { DependenciesValue.GetData(), DependenciesValue.Size() };
        if (!Catalog.Add(EntryValue))
        {
            return false;
        }
    }
    return true;
}

bool AssetCatalog::Parse(Span<char> Json)
{
    AssetCatalog Candidate;
    if (!ReadCatalog(Json, Candidate))
    {
        LOGF(Error, "Invalid AssetCatalog JSON");
        return false;
    }
    if (Candidate.Data)
    {
        for (ptrdiff_t I = 0; I < hmlen(Candidate.Data->pIndices); ++I)
        {
            const AssetCatalogEntry& EntryValue = *Candidate.Data->pIndices[I].value;
            const AssetCatalogEntry* Existing = Find(EntryValue.id);
            if (Existing && Existing->Type != EntryValue.Type)
            {
                LOGF(Error, "AssetCatalog replacement cannot change an AssetID's type");
                return false;
            }
        }
    }
    if (!Data)
    {
        Data = MemNew(AssetCatalogData);
    }
    for (ptrdiff_t I = 0; I < hmlen(Data->pIndices); ++I)
    {
        Data->retiredEntries.PushBack(Data->pIndices[I].value);
    }
    hmfree(Data->pIndices);
    if (Candidate.Data)
    {
        Data->pIndices = Candidate.Data->pIndices;
        Candidate.Data->pIndices = nullptr;
    }
    return true;
}

bool AssetCatalog::Load(ResourceDirectory Directory, const char* FileName)
{
    if (!ValidPath(FileName))
    {
        LOGF(Error, "AssetCatalog requires a relative index path");
        return false;
    }
    FileStream Stream = {};
    if (!OpenStreamFromPath(Directory, FileName, FileModeRead, &Stream))
    {
        return false;
    }
    const ssize_t Size = GetStreamFileSize(&Stream);
    if (Size <= 0 || Size > 64 * 1024 * 1024)
    {
        CloseStream(&Stream);
        LOGF(Error, "Invalid AssetCatalog file size: '%s'", FileName);
        return false;
    }
    Array<char>  Json((uint32_t)Size);
    const size_t BytesRead = ReadFromStream(&Stream, Json.GetData(), (size_t)Size);
    CloseStream(&Stream);
    if (BytesRead != (size_t)Size)
    {
        LOGF(Error, "Cannot read AssetCatalog '%s'", FileName);
        return false;
    }
    return Parse({ Json.GetData(), Json.Size() });
}

static int CompareEntries(const void* LeftValue, const void* RightValue)
{
    return CompareAssetIDs(&(*(const AssetCatalogEntry* const*)LeftValue)->id, &(*(const AssetCatalogEntry* const*)RightValue)->id);
}

bool AssetCatalog::Serialize(Array<char>& Json) const
{
    Array<const AssetCatalogEntry*> EntriesValue;
    EntriesValue.Reserve(GetCount());
    if (Data)
    {
        for (ptrdiff_t I = 0; I < hmlen(Data->pIndices); ++I)
        {
            EntriesValue.PushBack(Data->pIndices[I].value);
        }
    }
    if (!EntriesValue.Empty())
    {
        qsort(EntriesValue.GetData(), EntriesValue.Size(), sizeof(EntriesValue[0]), CompareEntries);
    }
    CatalogJson Document;
    Document.pRoot = cJSON_CreateObject();
    cJSON_AddStringToObject(Document.pRoot, "format", "Horizon.AssetCatalog");
    cJSON_AddNumberToObject(Document.pRoot, "version", 1);
    cJSON* EntriesPointer = cJSON_AddArrayToObject(Document.pRoot, "entries");
    for (const AssetCatalogEntry* EntryPointer : EntriesValue)
    {
        cJSON* ValueData = cJSON_CreateObject();
        cJSON_AddItemToArray(EntriesPointer, ValueData);
        char Id[kIDStringCapacity];
        EntryPointer->id.ToString(Id);
        cJSON_AddStringToObject(ValueData, "id", Id);
        cJSON_AddNumberToObject(ValueData, "type", (uint32_t)EntryPointer->Type);
        cJSON_AddStringToObject(ValueData, "source", EntryPointer->pSourcePath);
        cJSON_AddStringToObject(ValueData, "path", EntryPointer->location.pPath);
        cJSON_AddNumberToObject(ValueData, "root", (uint32_t)EntryPointer->location.root);
        if (EntryPointer->location.elementIndex == UINT32_MAX)
            cJSON_AddNullToObject(ValueData, "elementIndex");
        else
            cJSON_AddNumberToObject(ValueData, "elementIndex", EntryPointer->location.elementIndex);
        cJSON_AddBoolToObject(ValueData, "srgb", EntryPointer->srgb);
        cJSON* DependenciesPointer = cJSON_AddArrayToObject(ValueData, "dependencies");
        for (AssetID DependencyValue : EntryPointer->dependencies)
        {
            DependencyValue.ToString(Id);
            cJSON_AddItemToArray(DependenciesPointer, cJSON_CreateString(Id));
        }
    }
    char* Text = cJSON_PrintUnformatted(Document.pRoot);
    if (!Text)
        return false;
    Json.Resize((uint32_t)strlen(Text) + 1);
    memcpy(Json.GetData(), Text, Json.Size());
    cJSON_free(Text);
    return true;
}

void AssetCatalog::Clear()
{
    MemDelete(Data);
    Data = nullptr;
}
} // namespace hz
