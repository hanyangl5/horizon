/* Copyright (c) 2026 Horizon */
#pragma once

#include "Scene/World.h"
#include "Core/IContainer.h"
#include "Core/IThread.h"
#include <flecs.h>

namespace hz
{
class WorldHierarchy;
struct WorldType
{
    TypeID          key;
    ecs_entity_t    component;
    const TypeDesc* pType;
};

struct EntityGuidEntry
{
    Guid         key;
    ecs_entity_t value;
};

class WorldChangeLogImpl
{
public:
    explicit WorldChangeLogImpl(World& world): world(world) {}

    // Past this many entries a full rescan is cheaper than replaying the log.
    static constexpr uint32_t kCapacity = 65536;

    void record(ecs_entity_t entity, uint32_t world)
    {
        if (overflowed)
        {
            return;
        }
        if (entries.Size() == kCapacity)
        {
            overflowed = true;
            entries.Clear();
            return;
        }
        entries.PushBack(Entity(entity, world));
    }

    World&        world;
    Array<Entity> entries;
    Array<Entity> drained;
    bool          overflowed = true;
};

class WorldImpl
{
public:
    WorldImpl(const TypeRegistry& registry, const IDGenerator& entityGuids);
    ~WorldImpl();
    WorldImpl(const WorldImpl&) = delete;
    WorldImpl& operator=(const WorldImpl&) = delete;

private:
    friend class World;
    friend class WorldQuery;
    friend class WorldQueryImpl;
    friend class WorldChangeLog;
    friend class DeferredChanges;
    friend class WorldHierarchy;
    friend class WorldArchive;

    const WorldType* findType(TypeID Type) { return hmgetp_null(pTypes, Type); }
    void             checkThread() const { ASSERT(thread == GetCurrentThreadID()); }
    void             finishGuidChanges();
    void*            findEditableProperty(ecs_entity_t entity, TypeID Type, PropertyID property, const PropertyDesc*& pProperty);
    void             notifyPropertyChange(ecs_entity_t entity, TypeID Type);
    // Feeds every live WorldChangeLog. Structural changes made inside a Defer scope are recorded when requested.
    void             recordChange(ecs_entity_t entity)
    {
        for (WorldChangeLogImpl* pLog : changeLogs)
        {
            pLog->record(entity, id);
        }
    }
    static void      removeEntityGuids(ecs_iter_t* pIterator);

    static void ConstructComponents(void* pDestination, int32_t Count, const ecs_type_info_t* pInfo);
    static void DestroyComponents(void* pValue, int32_t Count, const ecs_type_info_t* pInfo);
    static void CopyConstructComponents(void* pDestination, const void* pSource, int32_t Count, const ecs_type_info_t* pInfo);
    static void MoveConstructComponents(void* pDestination, void* pSource, int32_t Count, const ecs_type_info_t* pInfo);
    static void CopyComponents(void* pDestination, const void* pSource, int32_t Count, const ecs_type_info_t* pInfo);
    static void MoveComponents(void* pDestination, void* pSource, int32_t Count, const ecs_type_info_t* pInfo);

    ecs_world_t*     pWorld = nullptr;
    WorldType*       pTypes = nullptr;
    // Unique per process; stamped into every Entity this World hands out.
    uint32_t                   id = 0;
    ecs_entity_t     entityTag = 0;
    ecs_entity_t     parentRelation = 0;
    ecs_entity_t     entityIdentityType = 0;
    IDGenerator      entityGuids;
    EntityGuidEntry* pEntityGuids = nullptr;
    Array<Guid>      pendingGuids;
    WorldHierarchy*  pHierarchy = nullptr;
    // Queries own flecs state in this World, so it must not be destroyed or replaced while any are live.
    uint32_t                   queryCount = 0;
    Array<WorldChangeLogImpl*> changeLogs;
    ThreadID                   thread = GetCurrentThreadID();
};

} // namespace hz
