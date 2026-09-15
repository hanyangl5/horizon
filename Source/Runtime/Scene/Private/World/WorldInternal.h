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

struct WorldObject
{
    ObjectID     key;
    ecs_entity_t value;
};

class WorldImpl
{
public:
    WorldImpl(const TypeRegistry& registry, const IDGenerator& objectIDs);
    ~WorldImpl();
    WorldImpl(const WorldImpl&) = delete;
    WorldImpl& operator=(const WorldImpl&) = delete;

private:
    friend class World;
    friend class WorldQuery;
    friend class WorldQueryImpl;
    friend class DeferredChanges;
    friend class WorldHierarchy;

    const WorldType* findType(TypeID type) { return hmgetp_null(pTypes, type); }
    void             checkThread() const { ASSERT(thread == getCurrentThreadID()); }
    void             finishObjectChanges();
    static void      removeObjectIDs(ecs_iter_t* pIterator);

    static void constructComponents(void* pDestination, int32_t count, const ecs_type_info_t* pInfo);
    static void destroyComponents(void* pValue, int32_t count, const ecs_type_info_t* pInfo);
    static void copyConstructComponents(void* pDestination, const void* pSource, int32_t count, const ecs_type_info_t* pInfo);
    static void moveConstructComponents(void* pDestination, void* pSource, int32_t count, const ecs_type_info_t* pInfo);
    static void copyComponents(void* pDestination, const void* pSource, int32_t count, const ecs_type_info_t* pInfo);
    static void moveComponents(void* pDestination, void* pSource, int32_t count, const ecs_type_info_t* pInfo);

    ecs_world_t*    pWorld = nullptr;
    WorldType*      pTypes = nullptr;
    ecs_entity_t    entityTag = 0;
    ecs_entity_t    objectIdentityType = 0;
    IDGenerator     objectIDs;
    WorldObject*    pObjects = nullptr;
    Array<ObjectID> pendingObjects;
    WorldHierarchy* pHierarchy = nullptr;
    ThreadID        thread = getCurrentThreadID();
};

} // namespace hz
