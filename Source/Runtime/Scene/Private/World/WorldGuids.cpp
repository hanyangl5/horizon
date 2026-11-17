/* Copyright (c) 2026 Horizon */

#include "WorldInternal.h"
#include "Scene/SceneComponents.h"

namespace hz
{
Entity World::CreatePersistentEntity()
{
    pImpl->checkThread();
    const Guid id = Guid::Create(pImpl->entityGuids);
    return id.IsValid() ? CreatePersistentEntity(id) : Entity{};
}

Entity World::CreatePersistentEntity(const Guid& id)
{
    pImpl->checkThread();
    if (!pImpl->entityIdentityType)
    {
        LOGF(Error, "Creating a persistent entity requires registered EntityIdentity support");
        return {};
    }
    if (!id.IsValid())
    {
        LOGF(Error, "A persistent entity requires a nonzero Guid");
        return {};
    }
    if (hmgetp_null(pImpl->pEntityGuids, id))
    {
        char text[kIDStringCapacity];
        id.ToString(text);
        LOGF(Error, "Duplicate Guid: %s", text);
        return {};
    }

    const Entity         entity = CreateEntity();
    const EntityIdentity identity = { .value = id };
    ecs_set_id(pImpl->pWorld, entity.value, pImpl->entityIdentityType, sizeof(identity), &identity);
    const EntityGuidEntry entry = { .key = id, .value = entity.value };
    hmputs(pImpl->pEntityGuids, entry);
    if (IsDeferred())
    {
        pImpl->pendingGuids.PushBack(id);
    }
    return entity;
}

Entity World::GetEntity(const Guid& id) const
{
    pImpl->checkThread();
    if (!id.IsValid())
    {
        return {};
    }
    const EntityGuidEntry* pEntry = hmgetp_null(pImpl->pEntityGuids, id);
    if (!pEntry || !ecs_is_alive(pImpl->pWorld, pEntry->value) || !ecs_has_id(pImpl->pWorld, pEntry->value, pImpl->entityIdentityType))
    {
        return {};
    }
    return Entity(pEntry->value, pImpl->id);
}

Guid World::GetEntityGuid(Entity entity) const
{
    const EntityIdentity* pIdentity = (const EntityIdentity*)GetComponent(entity, SceneTypes::entityIdentity);
    return pIdentity ? pIdentity->value : Guid{};
}

void WorldImpl::removeEntityGuids(ecs_iter_t* pIterator)
{
    WorldImpl&            world = *(WorldImpl*)pIterator->binding_ctx;
    const EntityIdentity* pIdentities = (const EntityIdentity*)ecs_field_w_size(pIterator, sizeof(EntityIdentity), 1);
    for (int32_t i = 0; i < pIterator->count; ++i)
    {
        hmdel(world.pEntityGuids, pIdentities[i].value);
    }
}

void WorldImpl::finishGuidChanges()
{
    // A deferred create followed by delete may never Add EntityIdentity, so its Remove hook cannot run.
    for (const Guid& id : pendingGuids)
    {
        const EntityGuidEntry* pEntry = hmgetp_null(pEntityGuids, id);
        if (pEntry && (!ecs_is_alive(pWorld, pEntry->value) || !ecs_has_id(pWorld, pEntry->value, entityIdentityType)))
        {
            hmdel(pEntityGuids, id);
        }
    }
    pendingGuids.Clear();
}
} // namespace hz
