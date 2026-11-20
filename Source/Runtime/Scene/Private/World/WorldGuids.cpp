/* Copyright (c) 2026 Horizon */

#include "WorldInternal.h"
#include "Scene/SceneComponents.h"

namespace hz
{
Entity World::CreatePersistentEntity()
{
    pImpl->checkThread();
    const Guid Id = Guid::Create(pImpl->entityGuids);
    return Id.IsValid() ? CreatePersistentEntity(Id) : Entity{};
}

Entity World::CreatePersistentEntity(const Guid& Id)
{
    pImpl->checkThread();
    if (!pImpl->entityIdentityType)
    {
        LOGF(Error, "Creating a persistent entity requires registered EntityIdentity support");
        return {};
    }
    if (!Id.IsValid())
    {
        LOGF(Error, "A persistent entity requires a nonzero Guid");
        return {};
    }
    if (hmgetp_null(pImpl->pEntityGuids, Id))
    {
        char Text[kIDStringCapacity];
        Id.ToString(Text);
        LOGF(Error, "Duplicate Guid: %s", Text);
        return {};
    }

    const Entity         EntityHandle = CreateEntity();
    const EntityIdentity IdentityValue = { .value = Id };
    ecs_set_id(pImpl->pWorld, EntityHandle.value, pImpl->entityIdentityType, sizeof(IdentityValue), &IdentityValue);
    const EntityGuidEntry EntryValue = { .key = Id, .value = EntityHandle.value };
    hmputs(pImpl->pEntityGuids, EntryValue);
    if (IsDeferred())
    {
        pImpl->pendingGuids.PushBack(Id);
    }
    return EntityHandle;
}

Entity World::GetEntity(const Guid& Id) const
{
    pImpl->checkThread();
    if (!Id.IsValid())
    {
        return {};
    }
    const EntityGuidEntry* EntryPointer = hmgetp_null(pImpl->pEntityGuids, Id);
    if (!EntryPointer || !ecs_is_alive(pImpl->pWorld, EntryPointer->value) || !ecs_has_id(pImpl->pWorld, EntryPointer->value, pImpl->entityIdentityType))
    {
        return {};
    }
    return Entity(EntryPointer->value, pImpl->id);
}

Guid World::GetEntityGuid(Entity EntityHandle) const
{
    const EntityIdentity* IdentityPointer = (const EntityIdentity*)GetComponent(EntityHandle, SceneTypes::entityIdentity);
    return IdentityPointer ? IdentityPointer->value : Guid{};
}

void WorldImpl::removeEntityGuids(ecs_iter_t* Iterator)
{
    WorldImpl&            WorldData = *(WorldImpl*)Iterator->binding_ctx;
    const EntityIdentity* Identities = (const EntityIdentity*)ecs_field_w_size(Iterator, sizeof(EntityIdentity), 1);
    for (int32_t I = 0; I < Iterator->count; ++I)
    {
        hmdel(WorldData.pEntityGuids, Identities[I].value);
    }
}

void WorldImpl::finishGuidChanges()
{
    // A deferred create followed by delete may never Add EntityIdentity, so its Remove hook cannot run.
    for (const Guid& Id : pendingGuids)
    {
        const EntityGuidEntry* EntryPointer = hmgetp_null(pEntityGuids, Id);
        if (EntryPointer && (!ecs_is_alive(pWorld, EntryPointer->value) || !ecs_has_id(pWorld, EntryPointer->value, entityIdentityType)))
        {
            hmdel(pEntityGuids, Id);
        }
    }
    pendingGuids.Clear();
}
} // namespace hz
