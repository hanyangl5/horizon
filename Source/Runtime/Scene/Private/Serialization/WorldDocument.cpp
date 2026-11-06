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
class WorldDocumentData
{
public:
    ~WorldDocumentData() { cJSON_Delete(pRoot); }
    cJSON*      pRoot = nullptr;
    Guid        id;
    Array<char> text;
};
static const cJSON* field(const cJSON* pObject, const char* pName) { return cJSON_GetObjectItemCaseSensitive(pObject, pName); }
static const char*  stringField(const cJSON* pObject, const char* pName)
{
    const cJSON* pValue = field(pObject, pName);
    return cJSON_IsString(pValue) ? pValue->valuestring : nullptr;
}
static bool readID(const cJSON* pValue, Guid& id, bool nullable = false)
{
    if (nullable && cJSON_IsNull(pValue))
    {
        id = {};
        return true;
    }
    return cJSON_IsString(pValue) && id.parse(pValue->valuestring) && id.isValid();
}
static void addID(cJSON* pObject, const char* pName, Guid id)
{
    char text[kIDStringCapacity];
    id.toString(text);
    cJSON_AddItemToObject(pObject, pName, id.isValid() ? cJSON_CreateString(text) : cJSON_CreateNull());
}
static bool typeID(const cJSON* pComponent, TypeID& type)
{
    const char* pText = stringField(pComponent, "type");
    if (!pText || strlen(pText) != 16)
        return false;
    const std::from_chars_result result = std::from_chars(pText, pText + 16, type, 16);
    return result.ec == std::errc{} && result.ptr == pText + 16 && type != 0;
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

WorldDocument::~WorldDocument() { tf_delete(pData); }
Guid WorldDocument::getID() const { return pData ? pData->id : Guid{}; }
bool WorldDocument::parse(Span<char> json)
{
    unique_ptr<WorldDocumentData> candidate = make_unique<WorldDocumentData>();
    candidate->pRoot = parseWorldJson(json);
    const cJSON* pRoot = candidate->pRoot;
    const char*  pFormat = stringField(pRoot, "format");
    uint32_t     version = 0;
    if (!uniqueWorldFields(pRoot) || !pFormat || strcmp(pFormat, "Horizon.World") || !worldVersion(field(pRoot, "version"), version) ||
        version != 1 || !readID(field(pRoot, "id"), candidate->id) || !cJSON_IsArray(field(pRoot, "entities")))
    {
        LOGF(eERROR, "Invalid World document schema");
        return false;
    }
    uint32_t count = json.count;
    if (count && !json.pData[count - 1])
        --count;
    candidate->text.resize(count + 1);
    memcpy(candidate->text.data(), json.pData, count);
    candidate->text[count] = 0;
    tf_delete(pData);
    pData = candidate.release();
    return true;
}
bool WorldDocument::serialize(Array<char>& json) const
{
    if (!pData)
        return false;
    json = pData->text;
    return true;
}
bool WorldDocument::capture(World& world, Guid id)
{
    if (world.isDeferred())
    {
        LOGF(eERROR, "Cannot capture a deferred World");
        return false;
    }
    if (!id.isValid())
        id = pData ? pData->id : Guid::create();
    if (!id.isValid())
        return false;
    Array<Guid> guids;
    for (ptrdiff_t i = 0; i < hmlen(world.pImpl->pEntityGuids); ++i)
        guids.pushBack(world.pImpl->pEntityGuids[i].key);
    if (!guids.empty())
        qsort(guids.data(), guids.size(), sizeof(Guid), compareGuids);
    Array<const TypeDesc*> types;
    for (ptrdiff_t i = 0; i < hmlen(world.pImpl->pTypes); ++i)
        if (world.pImpl->pTypes[i].key != SceneTypes::entityIdentity && world.pImpl->pTypes[i].key != SceneTypes::name)
            types.pushBack(world.pImpl->pTypes[i].pType);
    if (!types.empty())
        qsort(types.data(), types.size(), sizeof(types[0]), compareTypes);
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
        if (parent.isValid() && !parentID.isValid())
        {
            LOGF(eERROR, "A persistent entity's parent must have a Guid");
            return false;
        }
        cJSON* pRecord = cJSON_CreateObject();
        cJSON_AddItemToArray(pEntities, pRecord);
        addID(pRecord, "id", guid);
        addID(pRecord, "parent", parentID);
        const Name* pName = (const Name*)world.getComponent(entity, SceneTypes::name);
        if (pName && !memchr(pName->value, 0, sizeof(pName->value)))
            return false;
        cJSON_AddItemToObject(pRecord, "name", pName ? cJSON_CreateString(pName->value) : cJSON_CreateNull());
        cJSON* pComponents = cJSON_AddArrayToObject(pRecord, "components");
        for (const TypeDesc* pType : types)
        {
            const void* pValue = world.getComponent(entity, pType->id);
            if (!pValue)
                continue;
            if (!normalizeWorldComponent(pType->id, (void*)pValue, false) || !validateWorldReferences(*pType, pValue, world))
            {
                LOGF(eERROR, "Cannot save invalid '%s' component", pType->pName);
                return false;
            }
            cJSON* pProperties = writeWorldProperties(*pType, pValue);
            if (!pProperties)
                return false;
            cJSON* pComponent = cJSON_CreateObject();
            cJSON_AddItemToArray(pComponents, pComponent);
            char type[17];
            snprintf(type, sizeof(type), "%016llx", (unsigned long long)pType->id);
            cJSON_AddStringToObject(pComponent, "type", type);
            cJSON_AddStringToObject(pComponent, "name", pType->pName);
            cJSON_AddNumberToObject(pComponent, "version", pType->version);
            cJSON_AddItemToObject(pComponent, "properties", pProperties);
        }
    }
    char* pText = cJSON_Print(root.pValue);
    if (!pText)
        return false;
    const bool result = parse({ pText, (uint32_t)strlen(pText) });
    cJSON_free(pText);
    return result;
}

class ComponentValue
{
public:
    explicit ComponentValue(const TypeDesc& type): type(type), pValue(tf_memalign(type.alignment, type.size)) { type.construct(pValue); }
    ~ComponentValue()
    {
        type.destroy(pValue);
        tf_free(pValue);
    }
    const TypeDesc& type;
    void*           pValue;
};
struct WorldLoadNode
{
    Guid         id;
    Guid         parent;
    const cJSON* pRecord;
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
        for (const cJSON* pRecord = pEntities->child; pRecord; pRecord = pRecord->next)
        {
            WorldLoadNode node = { .pRecord = pRecord };
            const cJSON*  pName = field(pRecord, "name");
            if (!uniqueWorldFields(pRecord) || !readID(field(pRecord, "id"), node.id) ||
                !readID(field(pRecord, "parent"), node.parent, true) || hmgetp_null(pIndices, node.id) ||
                (!cJSON_IsNull(pName) && (!cJSON_IsString(pName) || strlen(pName->valuestring) >= sizeof(Name::value))) ||
                !cJSON_IsArray(field(pRecord, "components")))
                return false;
            node.entity = world.createPersistentEntity(node.id);
            if (!node.entity.isValid())
                return false;
            if (cJSON_IsString(pName))
            {
                Name name;
                strcpy(name.value, pName->valuestring);
                if (!world.setComponent(node.entity, SceneTypes::name, &name))
                    return false;
            }
            const WorldNodeIndex index = { .key = node.id, .value = nodes.size() };
            hmputs(pIndices, index);
            nodes.pushBack(node);
        }
        for (WorldLoadNode& node : nodes)
        {
            if (!node.parent.isValid())
                continue;
            const WorldNodeIndex* pIndex = hmgetp_null(pIndices, node.parent);
            if (!pIndex)
                return false;
            node.parentIndex = pIndex->value;
        }
        Array<uint32_t> path;
        for (uint32_t i = 0; i < nodes.size(); ++i)
        {
            uint32_t current = i;
            while (current != UINT32_MAX && !nodes[current].visit)
            {
                nodes[current].visit = 1;
                path.pushBack(current);
                current = nodes[current].parentIndex;
            }
            if (current != UINT32_MAX && nodes[current].visit == 1)
                return false;
            while (!path.empty())
            {
                nodes[path[path.size() - 1]].visit = 2;
                order.pushBack(path[path.size() - 1]);
                path.popBack();
            }
        }
        return true;
    }
    Array<WorldLoadNode> nodes;
    Array<uint32_t>      order;
    WorldNodeIndex*      pIndices = nullptr;
};
static bool loadComponent(const cJSON* pComponent, const TypeDesc& type, uint32_t version, Span<ComponentMigration> migrations,
                          World& world, Entity entity)
{
    const cJSON* pProperties = field(pComponent, "properties");
    if (version > type.version)
        return false;
    JsonValue migrated;
    while (version < type.version)
    {
        const ComponentMigration* pMigration = nullptr;
        for (const ComponentMigration& migration : migrations)
            if (migration.type == type.id && migration.fromVersion == version)
                pMigration = &migration;
        if (!pMigration || !pMigration->pMigrate)
            return false;
        char* pInput = cJSON_PrintUnformatted(pProperties);
        if (!pInput)
            return false;
        Array<char> output;
        const bool  success = pMigration->pMigrate({ pInput, (uint32_t)strlen(pInput) }, output);
        cJSON_free(pInput);
        if (!success || !validWorldJsonStrings({ output.data(), output.size() }))
            return false;
        cJSON* pNext = parseWorldJson({ output.data(), output.size() });
        if (!pNext)
            return false;
        cJSON_Delete(migrated.pValue);
        migrated.pValue = pNext;
        pProperties = pNext;
        if (!uniqueWorldFields(pProperties))
            return false;
        ++version;
    }
    ComponentValue value(type);
    return readWorldProperties(type, pProperties, value.pValue) && normalizeWorldComponent(type.id, value.pValue, true) &&
           validateWorldReferences(type, value.pValue, world) && world.setComponent(entity, type.id, value.pValue);
}
bool WorldDocument::instantiate(const TypeRegistry& types, unique_ptr<World>& target, Span<ComponentMigration> migrations,
                                const IDGenerator& entityGuids) const
{
    if (!pData || (target && target->isDeferred()))
        return false;
    if (!validWorldJsonStrings({ pData->text.data(), pData->text.size() }))
        return false;
    for (uint32_t i = 0; i < migrations.count; ++i)
    {
        const ComponentMigration& entry = migrations.pData[i];
        if (!entry.type || !entry.fromVersion || !entry.pMigrate)
            return false;
        for (uint32_t j = 0; j < i; ++j)
            if (entry.type == migrations.pData[j].type && entry.fromVersion == migrations.pData[j].fromVersion)
                return false;
    }
    unique_ptr<World> staging = make_unique<World>(types, entityGuids);
    WorldLoadPlan     plan;
    if (!plan.prepare(field(pData->pRoot, "entities"), *staging))
    {
        LOGF(eERROR, "Invalid World entities, parent references or hierarchy cycle");
        return false;
    }
    Array<TypeID> present;
    for (const WorldLoadNode& node : plan.nodes)
    {
        present.clear();
        for (const cJSON* pComponent = field(node.pRecord, "components")->child; pComponent; pComponent = pComponent->next)
        {
            TypeID   id = 0;
            uint32_t version = 0;
            if (!uniqueWorldFields(pComponent) || !typeID(pComponent, id) || !stringField(pComponent, "name") ||
                !worldVersion(field(pComponent, "version"), version) || !uniqueWorldFields(field(pComponent, "properties")))
                return false;
            for (TypeID previous : present)
                if (previous == id)
                    return false;
            present.pushBack(id);
            const TypeDesc* pType = types.getType(id);
            if (!pType || id == SceneTypes::entityIdentity || id == SceneTypes::name ||
                !loadComponent(pComponent, *pType, version, migrations, *staging, node.entity))
            {
                LOGF(eERROR, "Cannot instantiate World component '%s' version %u", stringField(pComponent, "name"), version);
                return false;
            }
        }
    }
    // The graph is validated; restore parents first without repeated ancestor scans.
    for (uint32_t index : plan.order)
    {
        const WorldLoadNode& node = plan.nodes[index];
        if (node.parentIndex != UINT32_MAX)
            staging->attachParent(node.entity, plan.nodes[node.parentIndex].entity);
    }
    staging->updateTransforms();
    target.swap(staging);
    return true;
}
bool WorldDocument::load(ResourceDirectory directory, const char* pFileName)
{
    FileStream stream = {};
    if (!fsOpenStreamFromPath(directory, pFileName, FM_READ, &stream))
    {
        LOGF(eERROR, "Cannot open World document '%s'", pFileName);
        return false;
    }
    const ssize_t size = fsGetStreamFileSize(&stream);
    if (size <= 0 || size > 64 * 1024 * 1024)
    {
        fsCloseStream(&stream);
        LOGF(eERROR, "Invalid World document size: '%s'", pFileName);
        return false;
    }
    Array<char>  bytes((uint32_t)size);
    const size_t read = fsReadFromStream(&stream, bytes.data(), (size_t)size);
    fsCloseStream(&stream);
    if (read != (size_t)size)
    {
        LOGF(eERROR, "Cannot read World document '%s'", pFileName);
        return false;
    }
    return parse({ bytes.data(), bytes.size() });
}
static bool writeWorldFile(ResourceDirectory directory, const char* pFileName, Span<char> text)
{
    char temporary[FS_MAX_PATH], backup[FS_MAX_PATH];
    if (snprintf(temporary, sizeof(temporary), "%s.tmp", pFileName) >= (int)sizeof(temporary) ||
        snprintf(backup, sizeof(backup), "%s.bak", pFileName) >= (int)sizeof(backup))
        return false;
    // Recover an interrupted replacement before starting another write.
    if (!fsFileExist(directory, pFileName) && fsFileExist(directory, backup) && !fsRenameFile(directory, backup, pFileName))
        return false;
    FileStream stream = {};
    if (!fsOpenStreamFromPath(directory, temporary, FM_WRITE, &stream))
        return false;
    const bool written = fsWriteToStream(&stream, text.pData, text.count) == text.count;
    const bool closed = fsCloseStream(&stream);
    if (!written || !closed)
        return false;
    const bool exists = fsFileExist(directory, pFileName);
    if ((fsFileExist(directory, backup) && !fsRemoveFile(directory, backup)) || (exists && !fsRenameFile(directory, pFileName, backup)))
        return false;
    if (!fsRenameFile(directory, temporary, pFileName))
    {
        if (exists && !fsRenameFile(directory, backup, pFileName))
            LOGF(eERROR, "Restore World '%s' from '%s'", pFileName, backup);
        return false;
    }
    if (exists && !fsRemoveFile(directory, backup))
        LOGF(eWARNING, "Cannot remove World backup '%s'", backup);
    return true;
}
bool WorldDocument::save(ResourceDirectory directory, const char* pFileName) const
{
    if (pData && writeWorldFile(directory, pFileName, { pData->text.data(), pData->text.size() - 1 }))
        return true;
    LOGF(eERROR, "Cannot save World document '%s'", pFileName);
    return false;
}
} // namespace hz
