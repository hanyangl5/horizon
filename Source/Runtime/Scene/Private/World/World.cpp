/* Copyright (c) 2026 Horizon */

#include "WorldInternal.h"
#include "WorldHierarchy.h"

#include "Core/IContainer.h"
#include "Core/IThread.h"

#include <flecs.h>
#include <string.h>

namespace hz
{
class WorldQueryImpl
{
public:
    WorldQueryImpl(World& world, Span<TypeID> types);
    ~WorldQueryImpl();
    WorldQueryImpl(const WorldQueryImpl&) = delete;
    WorldQueryImpl& operator=(const WorldQueryImpl&) = delete;

private:
    friend class WorldQuery;
    World*             pOwner;
    ecs_query_t*       pQuery = nullptr;
    Array<const void*> fields;
    bool               iterating = false;
};

} // namespace hz

#include "Core/IMemory.h"

namespace hz
{
static void* worldMalloc(ecs_size_t Size) { return MemAllocate(Size); }
static void* worldCalloc(ecs_size_t Size) { return MemCallocate(1, Size); }
static void* worldRealloc(void* Data, ecs_size_t Size) { return MemReallocate(Data, Size); }
static void  worldFree(void* Data) { MemDeallocate(Data); }
static char* worldStrdup(const char* pText)
{
    if (!pText)
        return nullptr;
    const size_t Size = strlen(pText) + 1;
    char*        pCopy = (char*)MemAllocate(Size);
    memcpy(pCopy, pText, Size);
    return pCopy;
}

static void initWorldOS()
{
    static bool initialized = false;
    if (initialized)
        return;
    ecs_os_set_api_defaults();
    ecs_os_api_t api = ecs_os_get_api();
    api.malloc_ = worldMalloc;
    api.calloc_ = worldCalloc;
    api.realloc_ = worldRealloc;
    api.free_ = worldFree;
    api.strdup_ = worldStrdup;
    ecs_os_set_api(&api);
    initialized = true;
}

void WorldImpl::constructComponents(void* pDestination, int32_t Count, const ecs_type_info_t* pInfo)
{
    const TypeDesc& Type = *(const TypeDesc*)pInfo->hooks.ctx;
    for (int32_t i = 0; i < Count; ++i)
    {
        Type.Construct((uint8_t*)pDestination + (size_t)i * Type.Size);
    }
}

void WorldImpl::destroyComponents(void* pValue, int32_t Count, const ecs_type_info_t* pInfo)
{
    const TypeDesc& Type = *(const TypeDesc*)pInfo->hooks.ctx;
    for (int32_t i = 0; i < Count; ++i)
    {
        Type.Destroy((uint8_t*)pValue + (size_t)i * Type.Size);
    }
}

void WorldImpl::copyConstructComponents(void* pDestination, const void* pSource, int32_t Count, const ecs_type_info_t* pInfo)
{
    const TypeDesc& Type = *(const TypeDesc*)pInfo->hooks.ctx;
    for (int32_t i = 0; i < Count; ++i)
    {
        Type.CopyConstruct((uint8_t*)pDestination + (size_t)i * Type.Size, (const uint8_t*)pSource + (size_t)i * Type.Size);
    }
}

void WorldImpl::moveConstructComponents(void* pDestination, void* pSource, int32_t Count, const ecs_type_info_t* pInfo)
{
    const TypeDesc& Type = *(const TypeDesc*)pInfo->hooks.ctx;
    for (int32_t i = 0; i < Count; ++i)
    {
        Type.MoveConstruct((uint8_t*)pDestination + (size_t)i * Type.Size, (uint8_t*)pSource + (size_t)i * Type.Size);
    }
}

void WorldImpl::copyComponents(void* pDestination, const void* pSource, int32_t Count, const ecs_type_info_t* pInfo)
{
    if (pDestination == pSource)
        return;
    destroyComponents(pDestination, Count, pInfo);
    copyConstructComponents(pDestination, pSource, Count, pInfo);
}

void WorldImpl::moveComponents(void* pDestination, void* pSource, int32_t Count, const ecs_type_info_t* pInfo)
{
    if (pDestination == pSource)
        return;
    destroyComponents(pDestination, Count, pInfo);
    moveConstructComponents(pDestination, pSource, Count, pInfo);
}

WorldImpl::WorldImpl(const TypeRegistry& registry, const IDGenerator& entityGuids): entityGuids(entityGuids)
{
    static tfrg_atomic32_t nextWorldId = 0;
    id = tfrg_atomic32_add_relaxed(&nextWorldId, 1) + 1;
    initWorldOS();
    pWorld = ecs_mini();
    ASSERT(pWorld);
    entityTag = ecs_new_id(pWorld);
    // World Owns transform propagation; its parent relation must not trigger recursive Flecs traversal.
    parentRelation = ecs_new_w_id(pWorld, EcsExclusive);
    ecs_add_id(pWorld, parentRelation, EcsTag);
    ecs_add_pair(pWorld, parentRelation, EcsOnDeleteTarget, EcsDelete);
    for (uint32_t i = 0; i < registry.Size(); ++i)
    {
        const TypeDesc*         pType = registry.GetTypeAt(i);
        const ecs_entity_desc_t entityDesc = { .name = pType->Name, .sep = "", .use_low_id = true };
        const ecs_component_desc_t desc = {
            .entity = ecs_entity_init(pWorld, &entityDesc),
            .type = {
                .size = (ecs_size_t)pType->Size,
                .alignment = (ecs_size_t)pType->alignment,
                .hooks = {
                    .ctor = constructComponents,
                    .dtor = destroyComponents,
                    .copy = copyComponents,
                    .move = moveComponents,
                    .copy_ctor = copyConstructComponents,
                    .move_ctor = moveConstructComponents,
                    .on_remove = pType->id == SceneTypes::entityIdentity ? removeEntityGuids : nullptr,
                    .ctx = (void*)pType,
                    .binding_ctx = this,
                },
            },
        };
        const ecs_entity_t component = ecs_component_init(pWorld, &desc);
        ASSERT(component);
        if (pType->id == SceneTypes::entityIdentity)
        {
            entityIdentityType = component;
        }
        const WorldType mapping = { .key = pType->id, .component = component, .pType = pType };
        hmputs(pTypes, mapping);
    }
    pHierarchy = MemNew(WorldHierarchy, *this);
}

WorldImpl::~WorldImpl()
{
    checkThread();
    ASSERT(!ecs_is_deferred(pWorld));
    ASSERTMSG(queryCount == 0, "World destroyed with %u live queries", queryCount);
    MemDelete(pHierarchy);
    ecs_fini(pWorld);
    hmfree(pEntityGuids);
    hmfree(pTypes);
}

World::World(const TypeRegistry& registry, const IDGenerator& entityGuids): pImpl(MemNew(WorldImpl, registry, entityGuids)) {}
World::~World() { MemDelete(pImpl); }

Entity World::CreateEntity()
{
    pImpl->checkThread();
    const ecs_entity_t entity = ecs_new_w_id(pImpl->pWorld, pImpl->entityTag);
    pImpl->pHierarchy->addRoot(entity);
    return Entity(entity, pImpl->id);
}

bool World::Owns(Entity entity) const
{
    // Invalid entities keep their "none" meaning; a Handle from another World is simply not part of this one.
    return !entity.IsValid() || entity.world == pImpl->id;
}

bool World::IsAlive(Entity entity) const
{
    pImpl->checkThread();
    return Owns(entity) && entity.IsValid() && ecs_is_alive(pImpl->pWorld, entity.value);
}

bool World::AddComponent(Entity entity, TypeID Type)
{
    if (Type == SceneTypes::entityIdentity)
    {
        LOGF(Error, "EntityIdentity is managed by World::CreatePersistentEntity");
        return false;
    }
    if (!IsAlive(entity))
    {
        return false;
    }
    const WorldType* pType = pImpl->findType(Type);
    if (!pType)
        return false;
    if (!pImpl->pHierarchy->prepareLocalChange(entity.value, Type, true))
    {
        return false;
    }
    ecs_add_id(pImpl->pWorld, entity.value, pType->component);
    return true;
}

bool World::RemoveComponent(Entity entity, TypeID Type)
{
    if (Type == SceneTypes::entityIdentity)
    {
        LOGF(Error, "EntityIdentity cannot be removed from a live entity");
        return false;
    }
    if (!IsAlive(entity))
    {
        return false;
    }
    const WorldType* pType = pImpl->findType(Type);
    if (!pType)
        return false;
    pImpl->pHierarchy->prepareLocalChange(entity.value, Type, false);
    ecs_remove_id(pImpl->pWorld, entity.value, pType->component);
    return true;
}

bool World::SetComponent(Entity entity, TypeID Type, const void* pValue)
{
    ASSERT(pValue);
    if (Type == SceneTypes::entityIdentity)
    {
        LOGF(Error, "EntityIdentity cannot be changed after entity creation");
        return false;
    }
    if (!IsAlive(entity))
    {
        return false;
    }
    const WorldType* pType = pImpl->findType(Type);
    if (!pType)
        return false;
    if (!pImpl->pHierarchy->prepareLocalChange(entity.value, Type, true))
    {
        return false;
    }
    if (!ecs_has_id(pImpl->pWorld, entity.value, pType->component) && !ecs_is_deferred(pImpl->pWorld))
    {
        // Adding a component can move the table containing pValue before Flecs copies it.
        const TypeDesc& description = *pType->pType;
        void*           pCopy = MemAllocateAligned(description.alignment, description.Size);
        description.CopyConstruct(pCopy, pValue);
        ecs_set_id(pImpl->pWorld, entity.value, pType->component, description.Size, pCopy);
        description.Destroy(pCopy);
        MemDeallocate(pCopy);
        return true;
    }
    ecs_set_id(pImpl->pWorld, entity.value, pType->component, pType->pType->Size, pValue);
    return true;
}

const void* World::GetComponent(Entity entity, TypeID Type) const
{
    if (!IsAlive(entity))
    {
        return nullptr;
    }
    const WorldType* pType = pImpl->findType(Type);
    return pType ? ecs_get_id(pImpl->pWorld, entity.value, pType->component) : nullptr;
}

DeferredChanges World::Defer() { return DeferredChanges(*this); }
bool            World::IsDeferred() const
{
    pImpl->checkThread();
    return ecs_is_deferred(pImpl->pWorld);
}

DeferredChanges::DeferredChanges(World& world): world(world)
{
    world.pImpl->checkThread();
    ecs_defer_begin(world.pImpl->pWorld);
}

DeferredChanges::~DeferredChanges()
{
    world.pImpl->checkThread();
    if (ecs_defer_end(world.pImpl->pWorld))
    {
        world.pImpl->pHierarchy->finishDeferredChanges();
        world.pImpl->finishGuidChanges();
    }
}

WorldQueryImpl::WorldQueryImpl(World& world, Span<TypeID> types): pOwner(&world)
{
    world.pImpl->checkThread();
    ASSERT(!world.IsDeferred());
    ++world.pImpl->queryCount;
    fields.Resize(types.Count);
    Array<ecs_term_t> terms(types.Count + 1);
    terms[0].id = world.pImpl->entityTag;
    terms[0].src.flags = EcsSelf;
    for (uint32_t i = 0; i < types.Count; ++i)
    {
        const WorldType* pType = world.pImpl->findType(types.Data[i]);
        if (!pType)
        {
            LOGF(Error, "World query references an unregistered component type");
            return;
        }
        terms[i + 1].id = pType->component;
        terms[i + 1].src.flags = EcsSelf;
        terms[i + 1].inout = EcsIn;
    }
    const ecs_query_desc_t desc = { .filter = { .terms_buffer = terms.GetData(), .terms_buffer_count = (int32_t)terms.Size() } };
    pQuery = ecs_query_init(world.pImpl->pWorld, &desc);
    ASSERT(pQuery);
}

WorldQueryImpl::~WorldQueryImpl()
{
    pOwner->pImpl->checkThread();
    ASSERT(!iterating);
    if (pQuery)
        ecs_query_fini(pQuery);
    --pOwner->pImpl->queryCount;
}

WorldChangeLog::WorldChangeLog(World& world): pImpl(MemNew(WorldChangeLogImpl, world))
{
    world.pImpl->checkThread();
    // Counted with the queries: a log names entities of this World, so it must not outlive a replacement.
    ++world.pImpl->queryCount;
    world.pImpl->changeLogs.PushBack(pImpl);
}

WorldChangeLog::~WorldChangeLog()
{
    WorldImpl&                  world = *pImpl->world.pImpl;
    Array<WorldChangeLogImpl*>& logs = world.changeLogs;
    world.checkThread();
    for (uint32_t i = 0; i < logs.Size(); ++i)
    {
        if (logs[i] == pImpl)
        {
            logs[i] = logs[logs.Size() - 1];
            logs.PopBack();
            break;
        }
    }
    --world.queryCount;
    MemDelete(pImpl);
}

Span<Entity> WorldChangeLog::Drain(bool& overflowed)
{
    pImpl->world.pImpl->checkThread();
    overflowed = pImpl->overflowed;
    pImpl->overflowed = false;
    // Swap so the returned entries stay valid while new changes are recorded.
    Array<Entity> previous = std::move(pImpl->drained);
    pImpl->drained = std::move(pImpl->entries);
    pImpl->entries = std::move(previous);
    pImpl->entries.Clear();
    return { pImpl->drained.GetData(), pImpl->drained.Size() };
}

WorldQuery::WorldQuery(World& world, Span<TypeID> types): pImpl(MemNew(WorldQueryImpl, world, types)) {}
WorldQuery::~WorldQuery() { MemDelete(pImpl); }

bool WorldQuery::IsValid() const { return pImpl->pQuery != nullptr; }

void WorldQuery::Each(bool (*pCallback)(Entity, Span<const void*>, void*), void* UserData)
{
    ASSERT(IsValid() && pCallback && !pImpl->iterating);
    DeferredChanges changes = pImpl->pOwner->Defer();
    pImpl->iterating = true;
    ecs_iter_t it = ecs_query_iter(pImpl->pOwner->pImpl->pWorld, pImpl->pQuery);
    while (ecs_query_next(&it))
    {
        for (int32_t row = 0; row < it.count; ++row)
        {
            for (uint32_t field = 0; field < pImpl->fields.Size(); ++field)
            {
                pImpl->fields[field] = (const uint8_t*)it.ptrs[field + 1] + (size_t)row * it.sizes[field + 1];
            }
            if (!pCallback(Entity(it.entities[row], pImpl->pOwner->pImpl->id), { pImpl->fields.GetData(), pImpl->fields.Size() }, UserData))
            {
                ecs_iter_fini(&it);
                pImpl->iterating = false;
                return;
            }
        }
    }
    pImpl->iterating = false;
}
} // namespace hz
