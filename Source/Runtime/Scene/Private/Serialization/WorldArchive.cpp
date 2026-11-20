/* Copyright (c) 2026 Horizon */
#include "WorldJson.h"
#include "../World/WorldInternal.h"
#include "Scene/SceneComponents.h"
#include "Core/IUniquePtr.h"
#include "Core/IToolFileSystem.h"
#include <charconv>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "Core/IMemory.h"

namespace hz
{
class WorldArchiveData
{
public:
    ~WorldArchiveData() { cJSON_Delete(pRoot); }
    cJSON*      pRoot = nullptr;
    Guid        id;
    Array<char> text;
};
static const cJSON* FindArchiveField(const cJSON* Object, const char* Name) { return cJSON_GetObjectItemCaseSensitive(Object, Name); }
static const char*  StringField(const cJSON* Object, const char* Name)
{
    const cJSON* ValueData = FindArchiveField(Object, Name);
    return cJSON_IsString(ValueData) ? ValueData->valuestring : nullptr;
}
static bool ReadID(const cJSON* ValueData, Guid& Id, bool Nullable = false)
{
    if (Nullable && cJSON_IsNull(ValueData))
    {
        Id = {};
        return true;
    }
    return cJSON_IsString(ValueData) && Id.Parse(ValueData->valuestring) && Id.IsValid();
}
static void AddID(cJSON* Object, const char* Name, Guid Id)
{
    char TextValue[kIDStringCapacity];
    Id.ToString(TextValue);
    cJSON_AddItemToObject(Object, Name, Id.IsValid() ? cJSON_CreateString(TextValue) : cJSON_CreateNull());
}
static bool ReadTypeId(const cJSON* Component, TypeID& Type)
{
    const char* TextPointer = StringField(Component, "type");
    if (!TextPointer || strlen(TextPointer) != 16)
    {
        return false;
    }
    const std::from_chars_result Result = std::from_chars(TextPointer, TextPointer + 16, Type, 16);
    return Result.ec == std::errc{} && Result.ptr == TextPointer + 16 && Type != 0;
}
static int CompareGuids(const void* LeftValue, const void* RightValue)
{
    const Guid& AValue = *(const Guid*)LeftValue;
    const Guid& BValue = *(const Guid*)RightValue;
    return (BValue < AValue) - (AValue < BValue);
}
static int CompareTypes(const void* LeftValue, const void* RightValue)
{
    const TypeID AValue = (*(const TypeDesc* const*)LeftValue)->id;
    const TypeID BValue = (*(const TypeDesc* const*)RightValue)->id;
    return (AValue > BValue) - (AValue < BValue);
}

WorldArchive::~WorldArchive() { MemDelete(Data); }
Guid WorldArchive::GetID() const { return Data ? Data->id : Guid{}; }
bool WorldArchive::Parse(Span<char> Json)
{
    UniquePtr<WorldArchiveData> Candidate = MakeUnique<WorldArchiveData>();
    Candidate->pRoot = ParseWorldJson(Json);
    const cJSON* RootPointer = Candidate->pRoot;
    const char*  FormatPointer = StringField(RootPointer, "format");
    uint32_t     Version = 0;
    if (!UniqueWorldFields(RootPointer) || !FormatPointer || strcmp(FormatPointer, "Horizon.World") || !WorldVersion(FindArchiveField(RootPointer, "version"), Version) ||
        Version != 1 || !ReadID(FindArchiveField(RootPointer, "id"), Candidate->id) || !cJSON_IsArray(FindArchiveField(RootPointer, "entities")))
    {
        LOGF(Error, "Invalid World archive schema");
        return false;
    }
    uint32_t Count = Json.Count;
    if (Count && !Json.Data[Count - 1])
    {
        --Count;
    }
    Candidate->text.Resize(Count + 1);
    memcpy(Candidate->text.GetData(), Json.Data, Count);
    Candidate->text[Count] = 0;
    MemDelete(Data);
    Data = Candidate.Release();
    return true;
}
bool WorldArchive::Serialize(Array<char>& Json) const
{
    if (!Data)
    {
        return false;
    }
    Json = Data->text;
    return true;
}
bool WorldArchive::Capture(World& WorldData, Guid Id)
{
    if (WorldData.IsDeferred())
    {
        LOGF(Error, "Cannot capture a deferred World");
        return false;
    }
    if (!Id.IsValid())
    {
        Id = Data ? Data->id : Guid::Create();
    }
    if (!Id.IsValid())
    {
        return false;
    }
    Array<Guid> Guids;
    for (ptrdiff_t I = 0; I < hmlen(WorldData.pImpl->pEntityGuids); ++I)
    {
        Guids.PushBack(WorldData.pImpl->pEntityGuids[I].key);
    }
    if (!Guids.Empty())
    {
        qsort(Guids.GetData(), Guids.Size(), sizeof(Guid), CompareGuids);
    }
    Array<const TypeDesc*> Types;
    for (ptrdiff_t I = 0; I < hmlen(WorldData.pImpl->pTypes); ++I)
    {
        if (WorldData.pImpl->pTypes[I].key != SceneTypes::entityIdentity && WorldData.pImpl->pTypes[I].key != SceneTypes::name)
        {
            Types.PushBack(WorldData.pImpl->pTypes[I].pType);
        }
    }
    if (!Types.Empty())
    {
        qsort(Types.GetData(), Types.Size(), sizeof(Types[0]), CompareTypes);
    }
    JsonValue RootValue(cJSON_CreateObject());
    cJSON_AddStringToObject(RootValue.pValue, "format", "Horizon.World");
    cJSON_AddNumberToObject(RootValue.pValue, "version", 1);
    AddID(RootValue.pValue, "id", Id);
    cJSON* Entities = cJSON_AddArrayToObject(RootValue.pValue, "entities");
    for (Guid GuidValue : Guids)
    {
        const Entity EntityHandle = WorldData.GetEntity(GuidValue);
        const Entity Parent = WorldData.GetParent(EntityHandle);
        const Guid   ParentId = WorldData.GetEntityGuid(Parent);
        if (Parent.IsValid() && !ParentId.IsValid())
        {
            LOGF(Error, "A persistent entity's parent must have a Guid");
            return false;
        }
        cJSON* EntityJson = cJSON_CreateObject();
        cJSON_AddItemToArray(Entities, EntityJson);
        AddID(EntityJson, "id", GuidValue);
        AddID(EntityJson, "parent", ParentId);
        const Name* NameComponent = (const Name*)WorldData.GetComponent(EntityHandle, SceneTypes::name);
        if (NameComponent && !memchr(NameComponent->value, 0, sizeof(NameComponent->value)))
        {
            return false;
        }
        cJSON_AddItemToObject(EntityJson, "name", NameComponent ? cJSON_CreateString(NameComponent->value) : cJSON_CreateNull());
        cJSON* Components = cJSON_AddArrayToObject(EntityJson, "components");
        for (const TypeDesc* TypeInfo : Types)
        {
            const void* ValueData = WorldData.GetComponent(EntityHandle, TypeInfo->id);
            if (!ValueData)
            {
                continue;
            }
            if (!NormalizeWorldComponent(TypeInfo->id, (void*)ValueData, false) || !ValidateWorldReferences(*TypeInfo, ValueData, WorldData))
            {
                LOGF(Error, "Cannot Save invalid '%s' component", TypeInfo->Name);
                return false;
            }
            cJSON* Properties = WriteWorldProperties(*TypeInfo, ValueData);
            if (!Properties)
            {
                return false;
            }
            cJSON* Component = cJSON_CreateObject();
            cJSON_AddItemToArray(Components, Component);
            char Type[17];
            snprintf(Type, sizeof(Type), "%016llx", (unsigned long long)TypeInfo->id);
            cJSON_AddStringToObject(Component, "type", Type);
            cJSON_AddStringToObject(Component, "name", TypeInfo->Name);
            cJSON_AddNumberToObject(Component, "version", TypeInfo->version);
            cJSON_AddItemToObject(Component, "properties", Properties);
        }
    }
    char* TextPointer = cJSON_Print(RootValue.pValue);
    if (!TextPointer)
    {
        return false;
    }
    const bool Result = Parse({ TextPointer, (uint32_t)strlen(TextPointer) });
    cJSON_free(TextPointer);
    return Result;
}

class ComponentValue
{
public:
    explicit ComponentValue(const TypeDesc& Type): Type(Type), pValue(MemAllocateAligned(Type.alignment, Type.Size))
    {
        Type.Construct(pValue);
    }
    ~ComponentValue()
    {
        Type.Destroy(pValue);
        MemDeallocate(pValue);
    }
    const TypeDesc& Type;
    void*           pValue;
};
struct WorldLoadNode
{
    Guid         id;
    Guid         parent;
    const cJSON* pEntityJson;
    Entity       entity;
    uint32_t     parentIndex = UINT32_MAX;
    uint8_t      visit = 0;
};
struct WorldNodeIndex
{
    Guid     key;
    uint32_t value;
};
class WorldLoadPlan
{
public:
    ~WorldLoadPlan() { hmfree(pIndices); }
    bool prepare(const cJSON* Entities, World& WorldData)
    {
        for (const cJSON* EntityJson = Entities->child; EntityJson; EntityJson = EntityJson->next)
        {
            WorldLoadNode NodeValue = { .pEntityJson = EntityJson };
            const cJSON*  NameField = FindArchiveField(EntityJson, "name");
            if (!UniqueWorldFields(EntityJson) || !ReadID(FindArchiveField(EntityJson, "id"), NodeValue.id) ||
                !ReadID(FindArchiveField(EntityJson, "parent"), NodeValue.parent, true) || hmgetp_null(pIndices, NodeValue.id) ||
                (!cJSON_IsNull(NameField) && (!cJSON_IsString(NameField) || strlen(NameField->valuestring) >= sizeof(Name::value))) ||
                !cJSON_IsArray(FindArchiveField(EntityJson, "components")))
            {
                return false;
            }
            NodeValue.entity = WorldData.CreatePersistentEntity(NodeValue.id);
            if (!NodeValue.entity.IsValid())
            {
                return false;
            }
            if (cJSON_IsString(NameField))
            {
                Name NameValue;
                strcpy(NameValue.value, NameField->valuestring);
                if (!WorldData.SetComponent(NodeValue.entity, SceneTypes::name, &NameValue))
                {
                    return false;
                }
            }
            const WorldNodeIndex IndexValue = { .key = NodeValue.id, .value = nodes.Size() };
            hmputs(pIndices, IndexValue);
            nodes.PushBack(NodeValue);
        }
        for (WorldLoadNode& NodeValue : nodes)
        {
            if (!NodeValue.parent.IsValid())
            {
                continue;
            }
            const WorldNodeIndex* IndexPointer = hmgetp_null(pIndices, NodeValue.parent);
            if (!IndexPointer)
            {
                return false;
            }
            NodeValue.parentIndex = IndexPointer->value;
        }
        Array<uint32_t> Path;
        for (uint32_t I = 0; I < nodes.Size(); ++I)
        {
            uint32_t Current = I;
            while (Current != UINT32_MAX && !nodes[Current].visit)
            {
                nodes[Current].visit = 1;
                Path.PushBack(Current);
                Current = nodes[Current].parentIndex;
            }
            if (Current != UINT32_MAX && nodes[Current].visit == 1)
            {
                return false;
            }
            while (!Path.Empty())
            {
                nodes[Path[Path.Size() - 1]].visit = 2;
                order.PushBack(Path[Path.Size() - 1]);
                Path.PopBack();
            }
        }
        return true;
    }
    Array<WorldLoadNode> nodes;
    Array<uint32_t>      order;
    WorldNodeIndex*      pIndices = nullptr;
};
static bool LoadComponent(const cJSON* Component, const TypeDesc& Type, uint32_t Version, Span<ComponentMigration> Migrations,
                          World& WorldData, Entity EntityHandle)
{
    const cJSON* Properties = FindArchiveField(Component, "properties");
    if (Version > Type.version)
    {
        return false;
    }
    JsonValue Migrated;
    while (Version < Type.version)
    {
        const ComponentMigration* MigrationPointer = nullptr;
        for (const ComponentMigration& MigrationValue : Migrations)
        {
            if (MigrationValue.Type == Type.id && MigrationValue.fromVersion == Version)
            {
                MigrationPointer = &MigrationValue;
            }
        }
        if (!MigrationPointer || !MigrationPointer->pMigrate)
        {
            return false;
        }
        char* Input = cJSON_PrintUnformatted(Properties);
        if (!Input)
        {
            return false;
        }
        Array<char> Output;
        const bool  Success = MigrationPointer->pMigrate({ Input, (uint32_t)strlen(Input) }, Output);
        cJSON_free(Input);
        if (!Success || !ValidWorldJsonStrings({ Output.GetData(), Output.Size() }))
        {
            return false;
        }
        cJSON* Next = ParseWorldJson({ Output.GetData(), Output.Size() });
        if (!Next)
        {
            return false;
        }
        cJSON_Delete(Migrated.pValue);
        Migrated.pValue = Next;
        Properties = Next;
        if (!UniqueWorldFields(Properties))
        {
            return false;
        }
        ++Version;
    }
    ComponentValue ValueValue(Type);
    return ReadWorldProperties(Type, Properties, ValueValue.pValue) && NormalizeWorldComponent(Type.id, ValueValue.pValue, true) &&
           ValidateWorldReferences(Type, ValueValue.pValue, WorldData) && WorldData.SetComponent(EntityHandle, Type.id, ValueValue.pValue);
}
bool WorldArchive::Instantiate(const TypeRegistry& Types, UniquePtr<World>& Target, Span<ComponentMigration> Migrations,
                               const IDGenerator& EntityGuids) const
{
    if (!Data || (Target && Target->IsDeferred()))
    {
        return false;
    }
    // Queries, and the render scene and asset bindings that hold them, would outlive the replaced World.
    if (Target && Target->pImpl->queryCount != 0)
    {
        LOGF(Error, "Cannot replace a World that still has %u live queries", Target->pImpl->queryCount);
        return false;
    }
    if (!ValidWorldJsonStrings({ Data->text.GetData(), Data->text.Size() }))
    {
        return false;
    }
    for (uint32_t I = 0; I < Migrations.Count; ++I)
    {
        const ComponentMigration& Entry = Migrations.Data[I];
        if (!Entry.Type || !Entry.fromVersion || !Entry.pMigrate)
        {
            return false;
        }
        for (uint32_t J = 0; J < I; ++J)
        {
            if (Entry.Type == Migrations.Data[J].Type && Entry.fromVersion == Migrations.Data[J].fromVersion)
            {
                return false;
            }
        }
    }
    UniquePtr<World>  Staging = MakeUnique<World>(Types, EntityGuids);
    WorldLoadPlan     Plan;
    if (!Plan.prepare(FindArchiveField(Data->pRoot, "entities"), *Staging))
    {
        LOGF(Error, "Invalid World entities, parent references or hierarchy cycle");
        return false;
    }
    Array<TypeID> Present;
    for (const WorldLoadNode& NodeValue : Plan.nodes)
    {
        Present.Clear();
        for (const cJSON* Component = FindArchiveField(NodeValue.pEntityJson, "components")->child; Component; Component = Component->next)
        {
            TypeID   Id = 0;
            uint32_t Version = 0;
            if (!UniqueWorldFields(Component) || !ReadTypeId(Component, Id) || !StringField(Component, "name") ||
                !WorldVersion(FindArchiveField(Component, "version"), Version) || !UniqueWorldFields(FindArchiveField(Component, "properties")))
            {
                return false;
            }
            for (TypeID Previous : Present)
            {
                if (Previous == Id)
                {
                    return false;
                }
            }
            Present.PushBack(Id);
            const TypeDesc* TypeInfo = Types.GetType(Id);
            if (!TypeInfo || Id == SceneTypes::entityIdentity || Id == SceneTypes::name ||
                !LoadComponent(Component, *TypeInfo, Version, Migrations, *Staging, NodeValue.entity))
            {
                LOGF(Error, "Cannot instantiate World component '%s' version %u", StringField(Component, "name"), Version);
                return false;
            }
        }
    }
    // The graph is validated; restore parents first without repeated ancestor scans.
    for (uint32_t IndexValue : Plan.order)
    {
        const WorldLoadNode& NodeValue = Plan.nodes[IndexValue];
        if (NodeValue.parentIndex != UINT32_MAX)
        {
            Staging->AttachParent(NodeValue.entity, Plan.nodes[NodeValue.parentIndex].entity);
        }
    }
    Staging->UpdateTransforms();
    Target.Swap(Staging);
    return true;
}
bool WorldArchive::Load(ResourceDirectory Directory, const char* FileName)
{
    FileStream Stream = {};
    if (!OpenStreamFromPath(Directory, FileName, FileModeRead, &Stream))
    {
        LOGF(Error, "Cannot open World archive '%s'", FileName);
        return false;
    }
    const ssize_t Size = GetStreamFileSize(&Stream);
    if (Size <= 0 || Size > 64 * 1024 * 1024)
    {
        CloseStream(&Stream);
        LOGF(Error, "Invalid World archive size: '%s'", FileName);
        return false;
    }
    Array<char>  Bytes((uint32_t)Size);
    const size_t Read = ReadFromStream(&Stream, Bytes.GetData(), (size_t)Size);
    CloseStream(&Stream);
    if (Read != (size_t)Size)
    {
        LOGF(Error, "Cannot read World archive '%s'", FileName);
        return false;
    }
    return Parse({ Bytes.GetData(), Bytes.Size() });
}
static bool WriteWorldFile(ResourceDirectory Directory, const char* FileName, Span<char> TextValue)
{
    char Temporary[FS_MAX_PATH], Backup[FS_MAX_PATH];
    if (snprintf(Temporary, sizeof(Temporary), "%s.tmp", FileName) >= (int)sizeof(Temporary) ||
        snprintf(Backup, sizeof(Backup), "%s.bak", FileName) >= (int)sizeof(Backup))
    {
        return false;
    }
    // Recover an interrupted replacement before starting another write.
    if (!FileExists(Directory, FileName) && FileExists(Directory, Backup) && !RenameFile(Directory, Backup, FileName))
    {
        return false;
    }
    FileStream Stream = {};
    if (!OpenStreamFromPath(Directory, Temporary, FileModeWrite, &Stream))
    {
        return false;
    }
    const bool Written = WriteToStream(&Stream, TextValue.Data, TextValue.Count) == TextValue.Count;
    const bool Closed = CloseStream(&Stream);
    if (!Written || !Closed)
    {
        return false;
    }
    const bool Exists = FileExists(Directory, FileName);
    if ((FileExists(Directory, Backup) && !RemoveFile(Directory, Backup)) || (Exists && !RenameFile(Directory, FileName, Backup)))
    {
        return false;
    }
    if (!RenameFile(Directory, Temporary, FileName))
    {
        if (Exists && !RenameFile(Directory, Backup, FileName))
        {
            LOGF(Error, "Restore World '%s' from '%s'", FileName, Backup);
        }
        return false;
    }
    if (Exists && !RemoveFile(Directory, Backup))
    {
        LOGF(Warning, "Cannot Remove World backup '%s'", Backup);
    }
    return true;
}
bool WorldArchive::Save(ResourceDirectory Directory, const char* FileName) const
{
    if (Data && WriteWorldFile(Directory, FileName, { Data->text.GetData(), Data->text.Size() - 1 }))
    {
        return true;
    }
    LOGF(Error, "Cannot Save World archive '%s'", FileName);
    return false;
}
} // namespace hz
