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
static const cJSON* field(const cJSON* pObject, const char* Name) { return cJSON_GetObjectItemCaseSensitive(pObject, Name); }
static const char*  stringField(const cJSON* pObject, const char* Name)
{
    const cJSON* pValue = field(pObject, Name);
    return cJSON_IsString(pValue) ? pValue->valuestring : nullptr;
}
static bool readID(const cJSON* pValue, Guid& id, bool nullable = false)
{
    if (nullable && cJSON_IsNull(pValue))
    {
        id = {};
        return true;
    }
    return cJSON_IsString(pValue) && id.parse(pValue->valuestring) && id.IsValid();
}
static void addID(cJSON* pObject, const char* Name, Guid id)
{
    char text[kIDStringCapacity];
    id.toString(text);
    cJSON_AddItemToObject(pObject, Name, id.IsValid() ? cJSON_CreateString(text) : cJSON_CreateNull());
}
static bool typeID(const cJSON* pComponent, TypeID& Type)
{
    const char* pText = stringField(pComponent, "type");
    if (!pText || strlen(pText) != 16)
    {
        return false;
    }
    const std::from_chars_result result = std::from_chars(pText, pText + 16, Type, 16);
    return result.ec == std::errc{} && result.ptr == pText + 16 && Type != 0;
}
static int compareGuids(const void* pA, const void* pB)
{
    const Guid& a = *(const Guid*)pA;
    const Guid& b = *(const Guid*)pB;
    return (b < a) - (a < b);
}
static int compareTypes(const void* pA, const void* pB)
{
    const TypeID a = (*(const TypeDesc* const*)pA)->id;
    const TypeID b = (*(const TypeDesc* const*)pB)->id;
    return (a > b) - (a < b);
}

WorldArchive::~WorldArchive() { MemDelete(Data); }
Guid WorldArchive::getID() const { return Data ? Data->id : Guid{}; }
bool WorldArchive::parse(Span<char> json)
{
    UniquePtr<WorldArchiveData> candidate = MakeUnique<WorldArchiveData>();
    candidate->pRoot = parseWorldJson(json);
    const cJSON* pRoot = candidate->pRoot;
    const char*  pFormat = stringField(pRoot, "format");
    uint32_t     version = 0;
    if (!uniqueWorldFields(pRoot) || !pFormat || strcmp(pFormat, "Horizon.World") || !worldVersion(field(pRoot, "version"), version) ||
        version != 1 || !readID(field(pRoot, "id"), candidate->id) || !cJSON_IsArray(field(pRoot, "entities")))
    {
        LOGF(Error, "Invalid World archive schema");
        return false;
    }
    uint32_t Count = json.Count;
    if (Count && !json.Data[Count - 1])
    {
        --Count;
    }
    candidate->text.Resize(Count + 1);
    memcpy(candidate->text.GetData(), json.Data, Count);
    candidate->text[Count] = 0;
    MemDelete(Data);
    Data = candidate.Release();
    return true;
}
bool WorldArchive::serialize(Array<char>& json) const
{
    if (!Data)
    {
        return false;
    }
    json = Data->text;
    return true;
}
bool WorldArchive::capture(World& world, Guid id)
{
    if (world.isDeferred())
    {
        LOGF(Error, "Cannot capture a deferred World");
        return false;
    }
    if (!id.IsValid())
    {
        id = Data ? Data->id : Guid::create();
    }
    if (!id.IsValid())
    {
        return false;
    }
    Array<Guid> guids;
    for (ptrdiff_t i = 0; i < hmlen(world.pImpl->pEntityGuids); ++i)
    {
        guids.PushBack(world.pImpl->pEntityGuids[i].key);
    }
    if (!guids.Empty())
    {
        qsort(guids.GetData(), guids.Size(), sizeof(Guid), compareGuids);
    }
    Array<const TypeDesc*> types;
    for (ptrdiff_t i = 0; i < hmlen(world.pImpl->pTypes); ++i)
    {
        if (world.pImpl->pTypes[i].key != SceneTypes::entityIdentity && world.pImpl->pTypes[i].key != SceneTypes::name)
        {
            types.PushBack(world.pImpl->pTypes[i].pType);
        }
    }
    if (!types.Empty())
    {
        qsort(types.GetData(), types.Size(), sizeof(types[0]), compareTypes);
    }
    JsonValue root(cJSON_CreateObject());
    cJSON_AddStringToObject(root.pValue, "format", "Horizon.World");
    cJSON_AddNumberToObject(root.pValue, "version", 1);
    addID(root.pValue, "id", id);
    cJSON* pEntities = cJSON_AddArrayToObject(root.pValue, "entities");
    for (Guid guid : guids)
    {
        const Entity entity = world.getEntity(guid);
        const Entity parent = world.getParent(entity);
        const Guid   parentID = world.getEntityGuid(parent);
        if (parent.IsValid() && !parentID.IsValid())
        {
            LOGF(Error, "A persistent entity's parent must have a Guid");
            return false;
        }
        cJSON* pEntityJson = cJSON_CreateObject();
        cJSON_AddItemToArray(pEntities, pEntityJson);
        addID(pEntityJson, "id", guid);
        addID(pEntityJson, "parent", parentID);
        const Name* NameComponent = (const Name*)world.getComponent(entity, SceneTypes::name);
        if (NameComponent && !memchr(NameComponent->value, 0, sizeof(NameComponent->value)))
        {
            return false;
        }
        cJSON_AddItemToObject(pEntityJson, "name", NameComponent ? cJSON_CreateString(NameComponent->value) : cJSON_CreateNull());
        cJSON* pComponents = cJSON_AddArrayToObject(pEntityJson, "components");
        for (const TypeDesc* pType : types)
        {
            const void* pValue = world.getComponent(entity, pType->id);
            if (!pValue)
            {
                continue;
            }
            if (!normalizeWorldComponent(pType->id, (void*)pValue, false) || !validateWorldReferences(*pType, pValue, world))
            {
                LOGF(Error, "Cannot save invalid '%s' component", pType->Name);
                return false;
            }
            cJSON* pProperties = writeWorldProperties(*pType, pValue);
            if (!pProperties)
            {
                return false;
            }
            cJSON* pComponent = cJSON_CreateObject();
            cJSON_AddItemToArray(pComponents, pComponent);
            char Type[17];
            snprintf(Type, sizeof(Type), "%016llx", (unsigned long long)pType->id);
            cJSON_AddStringToObject(pComponent, "type", Type);
            cJSON_AddStringToObject(pComponent, "name", pType->Name);
            cJSON_AddNumberToObject(pComponent, "version", pType->version);
            cJSON_AddItemToObject(pComponent, "properties", pProperties);
        }
    }
    char* pText = cJSON_Print(root.pValue);
    if (!pText)
    {
        return false;
    }
    const bool result = parse({ pText, (uint32_t)strlen(pText) });
    cJSON_free(pText);
    return result;
}

class ComponentValue
{
public:
    explicit ComponentValue(const TypeDesc& Type): Type(Type), pValue(MemAllocateAligned(Type.alignment, Type.Size)) { Type.construct(pValue); }
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
    bool prepare(const cJSON* pEntities, World& world)
    {
        for (const cJSON* pEntityJson = pEntities->child; pEntityJson; pEntityJson = pEntityJson->next)
        {
            WorldLoadNode node = { .pEntityJson = pEntityJson };
            const cJSON*  NameField = field(pEntityJson, "name");
            if (!uniqueWorldFields(pEntityJson) || !readID(field(pEntityJson, "id"), node.id) ||
                !readID(field(pEntityJson, "parent"), node.parent, true) || hmgetp_null(pIndices, node.id) ||
                (!cJSON_IsNull(NameField) && (!cJSON_IsString(NameField) || strlen(NameField->valuestring) >= sizeof(Name::value))) ||
                !cJSON_IsArray(field(pEntityJson, "components")))
            {
                return false;
            }
            node.entity = world.createPersistentEntity(node.id);
            if (!node.entity.IsValid())
            {
                return false;
            }
            if (cJSON_IsString(NameField))
            {
                Name name;
                strcpy(name.value, NameField->valuestring);
                if (!world.setComponent(node.entity, SceneTypes::name, &name))
                {
                    return false;
                }
            }
            const WorldNodeIndex index = { .key = node.id, .value = nodes.Size() };
            hmputs(pIndices, index);
            nodes.PushBack(node);
        }
        for (WorldLoadNode& node : nodes)
        {
            if (!node.parent.IsValid())
            {
                continue;
            }
            const WorldNodeIndex* pIndex = hmgetp_null(pIndices, node.parent);
            if (!pIndex)
            {
                return false;
            }
            node.parentIndex = pIndex->value;
        }
        Array<uint32_t> path;
        for (uint32_t i = 0; i < nodes.Size(); ++i)
        {
            uint32_t current = i;
            while (current != UINT32_MAX && !nodes[current].visit)
            {
                nodes[current].visit = 1;
                path.PushBack(current);
                current = nodes[current].parentIndex;
            }
            if (current != UINT32_MAX && nodes[current].visit == 1)
            {
                return false;
            }
            while (!path.Empty())
            {
                nodes[path[path.Size() - 1]].visit = 2;
                order.PushBack(path[path.Size() - 1]);
                path.PopBack();
            }
        }
        return true;
    }
    Array<WorldLoadNode> nodes;
    Array<uint32_t>      order;
    WorldNodeIndex*      pIndices = nullptr;
};
static bool loadComponent(const cJSON* pComponent, const TypeDesc& Type, uint32_t version, Span<ComponentMigration> migrations,
                          World& world, Entity entity)
{
    const cJSON* pProperties = field(pComponent, "properties");
    if (version > Type.version)
    {
        return false;
    }
    JsonValue migrated;
    while (version < Type.version)
    {
        const ComponentMigration* pMigration = nullptr;
        for (const ComponentMigration& migration : migrations)
        {
            if (migration.Type == Type.id && migration.fromVersion == version)
            {
                pMigration = &migration;
            }
        }
        if (!pMigration || !pMigration->pMigrate)
        {
            return false;
        }
        char* pInput = cJSON_PrintUnformatted(pProperties);
        if (!pInput)
        {
            return false;
        }
        Array<char> output;
        const bool  success = pMigration->pMigrate({ pInput, (uint32_t)strlen(pInput) }, output);
        cJSON_free(pInput);
        if (!success || !validWorldJsonStrings({ output.GetData(), output.Size() }))
        {
            return false;
        }
        cJSON* pNext = parseWorldJson({ output.GetData(), output.Size() });
        if (!pNext)
        {
            return false;
        }
        cJSON_Delete(migrated.pValue);
        migrated.pValue = pNext;
        pProperties = pNext;
        if (!uniqueWorldFields(pProperties))
        {
            return false;
        }
        ++version;
    }
    ComponentValue value(Type);
    return readWorldProperties(Type, pProperties, value.pValue) && normalizeWorldComponent(Type.id, value.pValue, true) &&
           validateWorldReferences(Type, value.pValue, world) && world.setComponent(entity, Type.id, value.pValue);
}
bool WorldArchive::instantiate(const TypeRegistry& types, UniquePtr<World>& target, Span<ComponentMigration> migrations,
                               const IDGenerator& entityGuids) const
{
    if (!Data || (target && target->isDeferred()))
    {
        return false;
    }
    if (!validWorldJsonStrings({ Data->text.GetData(), Data->text.Size() }))
    {
        return false;
    }
    for (uint32_t i = 0; i < migrations.Count; ++i)
    {
        const ComponentMigration& entry = migrations.Data[i];
        if (!entry.Type || !entry.fromVersion || !entry.pMigrate)
        {
            return false;
        }
        for (uint32_t j = 0; j < i; ++j)
        {
            if (entry.Type == migrations.Data[j].Type && entry.fromVersion == migrations.Data[j].fromVersion)
            {
                return false;
            }
        }
    }
    UniquePtr<World> staging = MakeUnique<World>(types, entityGuids);
    WorldLoadPlan     plan;
    if (!plan.prepare(field(Data->pRoot, "entities"), *staging))
    {
        LOGF(Error, "Invalid World entities, parent references or hierarchy cycle");
        return false;
    }
    Array<TypeID> present;
    for (const WorldLoadNode& node : plan.nodes)
    {
        present.Clear();
        for (const cJSON* pComponent = field(node.pEntityJson, "components")->child; pComponent; pComponent = pComponent->next)
        {
            TypeID   id = 0;
            uint32_t version = 0;
            if (!uniqueWorldFields(pComponent) || !typeID(pComponent, id) || !stringField(pComponent, "name") ||
                !worldVersion(field(pComponent, "version"), version) || !uniqueWorldFields(field(pComponent, "properties")))
            {
                return false;
            }
            for (TypeID previous : present)
            {
                if (previous == id)
                {
                    return false;
                }
            }
            present.PushBack(id);
            const TypeDesc* pType = types.getType(id);
            if (!pType || id == SceneTypes::entityIdentity || id == SceneTypes::name ||
                !loadComponent(pComponent, *pType, version, migrations, *staging, node.entity))
            {
                LOGF(Error, "Cannot instantiate World component '%s' version %u", stringField(pComponent, "name"), version);
                return false;
            }
        }
    }
    // The graph is validated; restore parents first without repeated ancestor scans.
    for (uint32_t index : plan.order)
    {
        const WorldLoadNode& node = plan.nodes[index];
        if (node.parentIndex != UINT32_MAX)
        {
            staging->attachParent(node.entity, plan.nodes[node.parentIndex].entity);
        }
    }
    staging->updateTransforms();
    target.Swap(staging);
    return true;
}
bool WorldArchive::load(ResourceDirectory directory, const char* pFileName)
{
    FileStream stream = {};
    if (!OpenStreamFromPath(directory, pFileName, FileModeRead, &stream))
    {
        LOGF(Error, "Cannot open World archive '%s'", pFileName);
        return false;
    }
    const ssize_t Size = GetStreamFileSize(&stream);
    if (Size <= 0 || Size > 64 * 1024 * 1024)
    {
        CloseStream(&stream);
        LOGF(Error, "Invalid World archive size: '%s'", pFileName);
        return false;
    }
    Array<char>  bytes((uint32_t)Size);
    const size_t read = ReadFromStream(&stream, bytes.GetData(), (size_t)Size);
    CloseStream(&stream);
    if (read != (size_t)Size)
    {
        LOGF(Error, "Cannot read World archive '%s'", pFileName);
        return false;
    }
    return parse({ bytes.GetData(), bytes.Size() });
}
static bool writeWorldFile(ResourceDirectory directory, const char* pFileName, Span<char> text)
{
    char temporary[FS_MAX_PATH], backup[FS_MAX_PATH];
    if (snprintf(temporary, sizeof(temporary), "%s.tmp", pFileName) >= (int)sizeof(temporary) ||
        snprintf(backup, sizeof(backup), "%s.bak", pFileName) >= (int)sizeof(backup))
    {
        return false;
    }
    // Recover an interrupted replacement before starting another write.
    if (!FileExists(directory, pFileName) && FileExists(directory, backup) && !RenameFile(directory, backup, pFileName))
    {
        return false;
    }
    FileStream stream = {};
    if (!OpenStreamFromPath(directory, temporary, FileModeWrite, &stream))
    {
        return false;
    }
    const bool written = WriteToStream(&stream, text.Data, text.Count) == text.Count;
    const bool closed = CloseStream(&stream);
    if (!written || !closed)
    {
        return false;
    }
    const bool exists = FileExists(directory, pFileName);
    if ((FileExists(directory, backup) && !RemoveFile(directory, backup)) || (exists && !RenameFile(directory, pFileName, backup)))
    {
        return false;
    }
    if (!RenameFile(directory, temporary, pFileName))
    {
        if (exists && !RenameFile(directory, backup, pFileName))
        {
            LOGF(Error, "Restore World '%s' from '%s'", pFileName, backup);
        }
        return false;
    }
    if (exists && !RemoveFile(directory, backup))
    {
        LOGF(Warning, "Cannot remove World backup '%s'", backup);
    }
    return true;
}
bool WorldArchive::save(ResourceDirectory directory, const char* pFileName) const
{
    if (Data && writeWorldFile(directory, pFileName, { Data->text.GetData(), Data->text.Size() - 1 }))
    {
        return true;
    }
    LOGF(Error, "Cannot save World archive '%s'", pFileName);
    return false;
}
} // namespace hz
