/* Copyright (c) 2026 Horizon */

#include "Scene/World.h"

#include "Core/IContainer.h"
#include "Core/IThread.h"

#include <flecs.h>
#include <string.h>

namespace hz
{
struct WorldType
{
    TypeID          key;
    ecs_entity_t    component;
    const TypeDesc* pType;
};

class WorldImpl
{
public:
    explicit WorldImpl(const TypeRegistry& registry);
    ~WorldImpl();
    WorldImpl(const WorldImpl&) = delete;
    WorldImpl& operator=(const WorldImpl&) = delete;

private:
    friend class World;
    friend class WorldQuery;
    friend class WorldQueryImpl;
    friend class DeferredChanges;

    const WorldType* findType(TypeID type) { return hmgetp_null(pTypes, type); }
    void             checkThread() const { ASSERT(thread == getCurrentThreadID()); }

    static void constructComponents(void* pDestination, int32_t count, const ecs_type_info_t* pInfo);
    static void destroyComponents(void* pValue, int32_t count, const ecs_type_info_t* pInfo);
    static void copyConstructComponents(void* pDestination, const void* pSource, int32_t count, const ecs_type_info_t* pInfo);
    static void moveConstructComponents(void* pDestination, void* pSource, int32_t count, const ecs_type_info_t* pInfo);
    static void copyComponents(void* pDestination, const void* pSource, int32_t count, const ecs_type_info_t* pInfo);
    static void moveComponents(void* pDestination, void* pSource, int32_t count, const ecs_type_info_t* pInfo);

    ecs_world_t* pWorld = nullptr;
    WorldType*   pTypes = nullptr;
    ecs_entity_t entityTag = 0;
    ThreadID     thread = getCurrentThreadID();
};

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
static void* worldMalloc(ecs_size_t size) { return tf_malloc(size); }
static void* worldCalloc(ecs_size_t size) { return tf_calloc(1, size); }
static void* worldRealloc(void* pData, ecs_size_t size) { return tf_realloc(pData, size); }
static void  worldFree(void* pData) { tf_free(pData); }
static char* worldStrdup(const char* pText)
{
    if (!pText)
        return nullptr;
    const size_t size = strlen(pText) + 1;
    char*        pCopy = (char*)tf_malloc(size);
    memcpy(pCopy, pText, size);
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

void WorldImpl::constructComponents(void* pDestination, int32_t count, const ecs_type_info_t* pInfo)
{
    const TypeDesc& type = *(const TypeDesc*)pInfo->hooks.ctx;
    for (int32_t i = 0; i < count; ++i)
        type.construct((uint8_t*)pDestination + (size_t)i * type.size);
}

void WorldImpl::destroyComponents(void* pValue, int32_t count, const ecs_type_info_t* pInfo)
{
    const TypeDesc& type = *(const TypeDesc*)pInfo->hooks.ctx;
    for (int32_t i = 0; i < count; ++i)
        type.destroy((uint8_t*)pValue + (size_t)i * type.size);
}

void WorldImpl::copyConstructComponents(void* pDestination, const void* pSource, int32_t count, const ecs_type_info_t* pInfo)
{
    const TypeDesc& type = *(const TypeDesc*)pInfo->hooks.ctx;
    for (int32_t i = 0; i < count; ++i)
        type.copyConstruct((uint8_t*)pDestination + (size_t)i * type.size, (const uint8_t*)pSource + (size_t)i * type.size);
}

void WorldImpl::moveConstructComponents(void* pDestination, void* pSource, int32_t count, const ecs_type_info_t* pInfo)
{
    const TypeDesc& type = *(const TypeDesc*)pInfo->hooks.ctx;
    for (int32_t i = 0; i < count; ++i)
        type.moveConstruct((uint8_t*)pDestination + (size_t)i * type.size, (uint8_t*)pSource + (size_t)i * type.size);
}

void WorldImpl::copyComponents(void* pDestination, const void* pSource, int32_t count, const ecs_type_info_t* pInfo)
{
    if (pDestination == pSource)
        return;
    destroyComponents(pDestination, count, pInfo);
    copyConstructComponents(pDestination, pSource, count, pInfo);
}

void WorldImpl::moveComponents(void* pDestination, void* pSource, int32_t count, const ecs_type_info_t* pInfo)
{
    if (pDestination == pSource)
        return;
    destroyComponents(pDestination, count, pInfo);
    moveConstructComponents(pDestination, pSource, count, pInfo);
}

WorldImpl::WorldImpl(const TypeRegistry& registry)
{
    initWorldOS();
    pWorld = ecs_mini();
    ASSERT(pWorld);
    entityTag = ecs_new_id(pWorld);
    for (uint32_t i = 0; i < registry.size(); ++i)
    {
        const TypeDesc*         pType = registry.getTypeAt(i);
        const ecs_entity_desc_t entityDesc = { .name = pType->pName, .sep = "", .use_low_id = true };
        const ecs_component_desc_t desc = {
            .entity = ecs_entity_init(pWorld, &entityDesc),
            .type = {
                .size = (ecs_size_t)pType->size,
                .alignment = (ecs_size_t)pType->alignment,
                .hooks = {
                    .ctor = constructComponents,
                    .dtor = destroyComponents,
                    .copy = copyComponents,
                    .move = moveComponents,
                    .copy_ctor = copyConstructComponents,
                    .move_ctor = moveConstructComponents,
                    .ctx = (void*)pType,
                },
            },
        };
        const ecs_entity_t component = ecs_component_init(pWorld, &desc);
        ASSERT(component);
        const WorldType mapping = { .key = pType->id, .component = component, .pType = pType };
        hmputs(pTypes, mapping);
    }
}

WorldImpl::~WorldImpl()
{
    checkThread();
    ASSERT(!ecs_is_deferred(pWorld));
    ecs_fini(pWorld);
    hmfree(pTypes);
}

World::World(const TypeRegistry& registry): pImpl(tf_new(WorldImpl, registry)) {}
World::~World() { tf_delete(pImpl); }

Entity World::createEntity()
{
    pImpl->checkThread();
    return Entity(ecs_new_w_id(pImpl->pWorld, pImpl->entityTag));
}

bool World::isAlive(Entity entity) const
{
    pImpl->checkThread();
    return entity.isValid() && ecs_is_alive(pImpl->pWorld, entity.value);
}

bool World::destroyEntity(Entity entity)
{
    if (!isAlive(entity))
        return false;
    ecs_delete(pImpl->pWorld, entity.value);
    return true;
}

bool World::addComponent(Entity entity, TypeID type)
{
    if (!isAlive(entity))
        return false;
    const WorldType* pType = pImpl->findType(type);
    if (!pType)
        return false;
    ecs_add_id(pImpl->pWorld, entity.value, pType->component);
    return true;
}

bool World::removeComponent(Entity entity, TypeID type)
{
    if (!isAlive(entity))
        return false;
    const WorldType* pType = pImpl->findType(type);
    if (!pType)
        return false;
    ecs_remove_id(pImpl->pWorld, entity.value, pType->component);
    return true;
}

bool World::setComponent(Entity entity, TypeID type, const void* pValue)
{
    ASSERT(pValue);
    if (!isAlive(entity))
        return false;
    const WorldType* pType = pImpl->findType(type);
    if (!pType)
        return false;
    ecs_set_id(pImpl->pWorld, entity.value, pType->component, pType->pType->size, pValue);
    return true;
}

const void* World::getComponent(Entity entity, TypeID type) const
{
    if (!isAlive(entity))
        return nullptr;
    const WorldType* pType = pImpl->findType(type);
    return pType ? ecs_get_id(pImpl->pWorld, entity.value, pType->component) : nullptr;
}

DeferredChanges World::defer() { return DeferredChanges(*this); }
bool            World::isDeferred() const
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
    ecs_defer_end(world.pImpl->pWorld);
}

WorldQueryImpl::WorldQueryImpl(World& world, Span<TypeID> types): pOwner(&world)
{
    world.pImpl->checkThread();
    ASSERT(!world.isDeferred());
    fields.resize(types.count);
    Array<ecs_term_t> terms(types.count + 1);
    terms[0].id = world.pImpl->entityTag;
    terms[0].src.flags = EcsSelf;
    for (uint32_t i = 0; i < types.count; ++i)
    {
        const WorldType* pType = world.pImpl->findType(types.pData[i]);
        if (!pType)
        {
            LOGF(eERROR, "World query references an unregistered component type");
            return;
        }
        terms[i + 1].id = pType->component;
        terms[i + 1].src.flags = EcsSelf;
        terms[i + 1].inout = EcsIn;
    }
    const ecs_query_desc_t desc = { .filter = { .terms_buffer = terms.data(), .terms_buffer_count = (int32_t)terms.size() } };
    pQuery = ecs_query_init(world.pImpl->pWorld, &desc);
    ASSERT(pQuery);
}

WorldQueryImpl::~WorldQueryImpl()
{
    pOwner->pImpl->checkThread();
    ASSERT(!iterating);
    if (pQuery)
        ecs_query_fini(pQuery);
}

WorldQuery::WorldQuery(World& world, Span<TypeID> types): pImpl(tf_new(WorldQueryImpl, world, types)) {}
WorldQuery::~WorldQuery() { tf_delete(pImpl); }

bool WorldQuery::isValid() const { return pImpl->pQuery != nullptr; }

void WorldQuery::each(bool (*pCallback)(Entity, Span<const void*>, void*), void* pUserData)
{
    ASSERT(isValid() && pCallback && !pImpl->iterating);
    DeferredChanges changes = pImpl->pOwner->defer();
    pImpl->iterating = true;
    ecs_iter_t it = ecs_query_iter(pImpl->pOwner->pImpl->pWorld, pImpl->pQuery);
    while (ecs_query_next(&it))
    {
        for (int32_t row = 0; row < it.count; ++row)
        {
            for (uint32_t field = 0; field < pImpl->fields.size(); ++field)
                pImpl->fields[field] = (const uint8_t*)it.ptrs[field + 1] + (size_t)row * it.sizes[field + 1];
            if (!pCallback(Entity(it.entities[row]), { pImpl->fields.data(), pImpl->fields.size() }, pUserData))
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
