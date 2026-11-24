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
    WorldQueryImpl(World& WorldData, Span<TypeID> Types);
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
static void* WorldMalloc(ecs_size_t Size) { return MemAllocate(Size); }
static void* WorldCalloc(ecs_size_t Size) { return MemCallocate(1, Size); }
static void* WorldRealloc(void* Data, ecs_size_t Size) { return MemReallocate(Data, Size); }
static void  WorldFree(void* Data) { MemDeallocate(Data); }
static char* WorldStrdup(const char* Text)
{
    if (!Text)
    {
        return nullptr;
    }
    const size_t Size = strlen(Text) + 1;
    char*        Copy = (char*)MemAllocate(Size);
    memcpy(Copy, Text, Size);
    return Copy;
}

static void InitWorldOS()
{
    static bool Initialized = false;
    if (Initialized)
    {
        return;
    }
    ecs_os_set_api_defaults();
    ecs_os_api_t Api = ecs_os_get_api();
    Api.malloc_ = WorldMalloc;
    Api.calloc_ = WorldCalloc;
    Api.realloc_ = WorldRealloc;
    Api.free_ = WorldFree;
    Api.strdup_ = WorldStrdup;
    ecs_os_set_api(&Api);
    Initialized = true;
}

void WorldImpl::ConstructComponents(void* Destination, int32_t Count, const ecs_type_info_t* Info)
{
    const TypeDesc& Type = *(const TypeDesc*)Info->hooks.ctx;
    for (int32_t I = 0; I < Count; ++I)
    {
        Type.Construct((uint8_t*)Destination + (size_t)I * Type.Size);
    }
}

void WorldImpl::DestroyComponents(void* ValueData, int32_t Count, const ecs_type_info_t* Info)
{
    const TypeDesc& Type = *(const TypeDesc*)Info->hooks.ctx;
    for (int32_t I = 0; I < Count; ++I)
    {
        Type.Destroy((uint8_t*)ValueData + (size_t)I * Type.Size);
    }
}

void WorldImpl::CopyConstructComponents(void* Destination, const void* Source, int32_t Count, const ecs_type_info_t* Info)
{
    const TypeDesc& Type = *(const TypeDesc*)Info->hooks.ctx;
    for (int32_t I = 0; I < Count; ++I)
    {
        Type.CopyConstruct((uint8_t*)Destination + (size_t)I * Type.Size, (const uint8_t*)Source + (size_t)I * Type.Size);
    }
}

void WorldImpl::MoveConstructComponents(void* Destination, void* Source, int32_t Count, const ecs_type_info_t* Info)
{
    const TypeDesc& Type = *(const TypeDesc*)Info->hooks.ctx;
    for (int32_t I = 0; I < Count; ++I)
    {
        Type.MoveConstruct((uint8_t*)Destination + (size_t)I * Type.Size, (uint8_t*)Source + (size_t)I * Type.Size);
    }
}

void WorldImpl::CopyComponents(void* Destination, const void* Source, int32_t Count, const ecs_type_info_t* Info)
{
    if (Destination == Source)
    {
        return;
    }
    DestroyComponents(Destination, Count, Info);
    CopyConstructComponents(Destination, Source, Count, Info);
}

void WorldImpl::MoveComponents(void* Destination, void* Source, int32_t Count, const ecs_type_info_t* Info)
{
    if (Destination == Source)
    {
        return;
    }
    DestroyComponents(Destination, Count, Info);
    MoveConstructComponents(Destination, Source, Count, Info);
}

WorldImpl::WorldImpl(const TypeRegistry& Registry, const IDGenerator& EntityGuids): entityGuids(EntityGuids)
{
    static tfrg_atomic32_t NextWorldId = 0;
    id = tfrg_atomic32_add_relaxed(&NextWorldId, 1) + 1;
    InitWorldOS();
    pWorld = ecs_mini();
    ASSERT(pWorld);
    entityTag = ecs_new_id(pWorld);
    // World Owns transform propagation; its parent relation must not trigger recursive Flecs traversal.
    parentRelation = ecs_new_w_id(pWorld, EcsExclusive);
    ecs_add_id(pWorld, parentRelation, EcsTag);
    ecs_add_pair(pWorld, parentRelation, EcsOnDeleteTarget, EcsDelete);
    for (uint32_t I = 0; I < Registry.Size(); ++I)
    {
        const TypeDesc*         TypeInfo = Registry.GetTypeAt(I);
        const ecs_entity_desc_t EntityDesc = { .name = TypeInfo->Name, .sep = "", .use_low_id = true };
        const ecs_component_desc_t Desc = {
            .entity = ecs_entity_init(pWorld, &EntityDesc),
            .type = {
                .size = (ecs_size_t)TypeInfo->Size,
                .alignment = (ecs_size_t)TypeInfo->alignment,
                .hooks = {
                    .ctor = ConstructComponents,
                    .dtor = DestroyComponents,
                    .copy = CopyComponents,
                    .move = MoveComponents,
                    .copy_ctor = CopyConstructComponents,
                    .move_ctor = MoveConstructComponents,
                    .on_remove = TypeInfo->id == SceneTypes::entityIdentity ? removeEntityGuids : nullptr,
                    .ctx = (void*)TypeInfo,
                    .binding_ctx = this,
                },
            },
        };
        const ecs_entity_t Component = ecs_component_init(pWorld, &Desc);
        ASSERT(Component);
        if (TypeInfo->id == SceneTypes::entityIdentity)
        {
            entityIdentityType = Component;
        }
        const WorldType Mapping = { .key = TypeInfo->id, .component = Component, .pType = TypeInfo };
        hmputs(pTypes, Mapping);
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

World::World(const TypeRegistry& Registry, const IDGenerator& EntityGuids): pImpl(MemNew(WorldImpl, Registry, EntityGuids)) {}
World::~World() { MemDelete(pImpl); }

Entity World::CreateEntity()
{
    pImpl->checkThread();
    const ecs_entity_t EntityHandle = ecs_new_w_id(pImpl->pWorld, pImpl->entityTag);
    pImpl->pHierarchy->addRoot(EntityHandle);
    return Entity(EntityHandle, pImpl->id);
}

bool World::Owns(Entity EntityHandle) const
{
    // Invalid entities keep their "none" meaning; a Handle from another World is simply not part of this one.
    return !EntityHandle.IsValid() || EntityHandle.world == pImpl->id;
}

bool World::IsAlive(Entity EntityHandle) const
{
    pImpl->checkThread();
    return Owns(EntityHandle) && EntityHandle.IsValid() && ecs_is_alive(pImpl->pWorld, EntityHandle.value);
}

bool World::AddComponent(Entity EntityHandle, TypeID Type)
{
    if (Type == SceneTypes::entityIdentity)
    {
        LOGF(Error, "EntityIdentity is managed by World::CreatePersistentEntity");
        return false;
    }
    if (!IsAlive(EntityHandle))
    {
        return false;
    }
    const WorldType* TypeInfo = pImpl->findType(Type);
    if (!TypeInfo)
    {
        return false;
    }
    if (!pImpl->pHierarchy->prepareLocalChange(EntityHandle.value, Type, true))
    {
        return false;
    }
    ecs_add_id(pImpl->pWorld, EntityHandle.value, TypeInfo->component);
    return true;
}

bool World::RemoveComponent(Entity EntityHandle, TypeID Type)
{
    if (Type == SceneTypes::entityIdentity)
    {
        LOGF(Error, "EntityIdentity cannot be removed from a live entity");
        return false;
    }
    if (!IsAlive(EntityHandle))
    {
        return false;
    }
    const WorldType* TypeInfo = pImpl->findType(Type);
    if (!TypeInfo)
    {
        return false;
    }
    pImpl->pHierarchy->prepareLocalChange(EntityHandle.value, Type, false);
    ecs_remove_id(pImpl->pWorld, EntityHandle.value, TypeInfo->component);
    return true;
}

bool World::SetComponent(Entity EntityHandle, TypeID Type, const void* ValueData)
{
    ASSERT(ValueData);
    if (Type == SceneTypes::entityIdentity)
    {
        LOGF(Error, "EntityIdentity cannot be changed after entity creation");
        return false;
    }
    if (!IsAlive(EntityHandle))
    {
        return false;
    }
    const WorldType* TypeInfo = pImpl->findType(Type);
    if (!TypeInfo)
    {
        return false;
    }
    if (!pImpl->pHierarchy->prepareLocalChange(EntityHandle.value, Type, true))
    {
        return false;
    }
    if (!ecs_has_id(pImpl->pWorld, EntityHandle.value, TypeInfo->component) && !ecs_is_deferred(pImpl->pWorld))
    {
        // Adding a component can move the table containing pValue before Flecs copies it.
        const TypeDesc& Description = *TypeInfo->pType;
        void*           Copy = MemAllocateAligned(Description.alignment, Description.Size);
        Description.CopyConstruct(Copy, ValueData);
        ecs_set_id(pImpl->pWorld, EntityHandle.value, TypeInfo->component, Description.Size, Copy);
        Description.Destroy(Copy);
        MemDeallocate(Copy);
        return true;
    }
    ecs_set_id(pImpl->pWorld, EntityHandle.value, TypeInfo->component, TypeInfo->pType->Size, ValueData);
    return true;
}

const void* World::GetComponent(Entity EntityHandle, TypeID Type) const
{
    if (!IsAlive(EntityHandle))
    {
        return nullptr;
    }
    const WorldType* TypeInfo = pImpl->findType(Type);
    return TypeInfo ? ecs_get_id(pImpl->pWorld, EntityHandle.value, TypeInfo->component) : nullptr;
}

DeferredChanges World::Defer() { return DeferredChanges(*this); }
bool            World::IsDeferred() const
{
    pImpl->checkThread();
    return ecs_is_deferred(pImpl->pWorld);
}

DeferredChanges::DeferredChanges(World& WorldData): world(WorldData)
{
    WorldData.pImpl->checkThread();
    ecs_defer_begin(WorldData.pImpl->pWorld);
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

WorldQueryImpl::WorldQueryImpl(World& WorldData, Span<TypeID> Types): pOwner(&WorldData)
{
    WorldData.pImpl->checkThread();
    ASSERT(!WorldData.IsDeferred());
    ++WorldData.pImpl->queryCount;
    fields.Resize(Types.Count);
    Array<ecs_term_t> Terms(Types.Count + 1);
    Terms[0].id = WorldData.pImpl->entityTag;
    Terms[0].src.flags = EcsSelf;
    for (uint32_t I = 0; I < Types.Count; ++I)
    {
        const WorldType* TypeInfo = WorldData.pImpl->findType(Types.Data[I]);
        if (!TypeInfo)
        {
            LOGF(Error, "World query references an unregistered component type");
            return;
        }
        Terms[I + 1].id = TypeInfo->component;
        Terms[I + 1].src.flags = EcsSelf;
        Terms[I + 1].inout = EcsIn;
    }
    const ecs_query_desc_t Desc = { .filter = { .terms_buffer = Terms.GetData(), .terms_buffer_count = (int32_t)Terms.Size() } };
    pQuery = ecs_query_init(WorldData.pImpl->pWorld, &Desc);
    ASSERT(pQuery);
}

WorldQueryImpl::~WorldQueryImpl()
{
    pOwner->pImpl->checkThread();
    ASSERT(!iterating);
    if (pQuery)
    {
        ecs_query_fini(pQuery);
    }
    --pOwner->pImpl->queryCount;
}

WorldChangeLog::WorldChangeLog(World& WorldData): pImpl(MemNew(WorldChangeLogImpl, WorldData))
{
    WorldData.pImpl->checkThread();
    // Counted with the queries: a log names entities of this World, so it must not outlive a replacement.
    ++WorldData.pImpl->queryCount;
    WorldData.pImpl->changeLogs.PushBack(pImpl);
}

WorldChangeLog::~WorldChangeLog()
{
    WorldImpl&                  WorldData = *pImpl->world.pImpl;
    Array<WorldChangeLogImpl*>& Logs = WorldData.changeLogs;
    WorldData.checkThread();
    for (uint32_t I = 0; I < Logs.Size(); ++I)
    {
        if (Logs[I] == pImpl)
        {
            Logs[I] = Logs[Logs.Size() - 1];
            Logs.PopBack();
            break;
        }
    }
    --WorldData.queryCount;
    MemDelete(pImpl);
}

Span<Entity> WorldChangeLog::Drain(bool& Overflowed)
{
    pImpl->world.pImpl->checkThread();
    Overflowed = pImpl->overflowed;
    pImpl->overflowed = false;
    // Swap so the returned entries stay valid while new changes are recorded.
    Array<Entity> Previous = std::move(pImpl->drained);
    pImpl->drained = std::move(pImpl->entries);
    pImpl->entries = std::move(Previous);
    pImpl->entries.Clear();
    return { pImpl->drained.GetData(), pImpl->drained.Size() };
}

WorldQuery::WorldQuery(World& WorldData, Span<TypeID> Types): pImpl(MemNew(WorldQueryImpl, WorldData, Types)) {}
WorldQuery::~WorldQuery() { MemDelete(pImpl); }

bool WorldQuery::IsValid() const { return pImpl->pQuery != nullptr; }

void WorldQuery::Each(bool (*Callback)(Entity, Span<const void*>, void*), void* UserData)
{
    ASSERT(IsValid() && Callback && !pImpl->iterating);
    DeferredChanges Changes = pImpl->pOwner->Defer();
    pImpl->iterating = true;
    ecs_iter_t It = ecs_query_iter(pImpl->pOwner->pImpl->pWorld, pImpl->pQuery);
    while (ecs_query_next(&It))
    {
        for (int32_t Row = 0; Row < It.count; ++Row)
        {
            for (uint32_t Field = 0; Field < pImpl->fields.Size(); ++Field)
            {
                pImpl->fields[Field] = (const uint8_t*)It.ptrs[Field + 1] + (size_t)Row * It.sizes[Field + 1];
            }
            if (!Callback(Entity(It.entities[Row], pImpl->pOwner->pImpl->id), { pImpl->fields.GetData(), pImpl->fields.Size() }, UserData))
            {
                ecs_iter_fini(&It);
                pImpl->iterating = false;
                return;
            }
        }
    }
    pImpl->iterating = false;
}
} // namespace hz
