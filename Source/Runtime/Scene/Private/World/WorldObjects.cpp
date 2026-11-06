/* Copyright (c) 2026 Horizon */

#include "WorldInternal.h"
#include "Scene/SceneComponents.h"

namespace hz
{
Entity World::createObject()
{
    pImpl->checkThread();
    const ObjectID id = ObjectID::create(pImpl->objectIDs);
    return id.isValid() ? createObject(id) : Entity{};
}

Entity World::createObject(const ObjectID& id)
{
    pImpl->checkThread();
    if (!pImpl->objectIdentityType)
    {
        LOGF(eERROR, "Creating an authoring object requires registered ObjectIdentity support");
        return {};
    }
    if (!id.isValid())
    {
        LOGF(eERROR, "An authoring object requires a nonzero ObjectID");
        return {};
    }
    if (hmgetp_null(pImpl->pObjects, id))
    {
        char text[kIDStringCapacity];
        id.toString(text);
        LOGF(eERROR, "Duplicate ObjectID: %s", text);
        return {};
    }

    const Entity         entity = createEntity();
    const ObjectIdentity identity = { .value = id };
    ecs_set_id(pImpl->pWorld, entity.value, pImpl->objectIdentityType, sizeof(identity), &identity);
    const WorldObject object = { .key = id, .value = entity.value };
    hmputs(pImpl->pObjects, object);
    if (isDeferred())
        pImpl->pendingObjects.pushBack(id);
    return entity;
}

Entity World::getEntity(const ObjectID& id) const
{
    pImpl->checkThread();
    if (!id.isValid())
        return {};
    const WorldObject* pObject = hmgetp_null(pImpl->pObjects, id);
    if (!pObject || !ecs_is_alive(pImpl->pWorld, pObject->value) || !ecs_has_id(pImpl->pWorld, pObject->value, pImpl->objectIdentityType))
        return {};
    return Entity(pObject->value);
}

ObjectID World::getObjectID(Entity entity) const
{
    const ObjectIdentity* pIdentity = (const ObjectIdentity*)getComponent(entity, SceneTypes::objectIdentity);
    return pIdentity ? pIdentity->value : ObjectID{};
}

void WorldImpl::removeObjectIDs(ecs_iter_t* pIterator)
{
    WorldImpl&            world = *(WorldImpl*)pIterator->binding_ctx;
    const ObjectIdentity* pIdentities = (const ObjectIdentity*)ecs_field_w_size(pIterator, sizeof(ObjectIdentity), 1);
    for (int32_t i = 0; i < pIterator->count; ++i)
        hmdel(world.pObjects, pIdentities[i].value);
}

void WorldImpl::finishObjectChanges()
{
    // A deferred create followed by delete may never add ObjectIdentity, so its remove hook cannot run.
    for (const ObjectID& id : pendingObjects)
    {
        const WorldObject* pObject = hmgetp_null(pObjects, id);
        if (pObject && (!ecs_is_alive(pWorld, pObject->value) || !ecs_has_id(pWorld, pObject->value, objectIdentityType)))
            hmdel(pObjects, id);
    }
    pendingObjects.clear();
}
} // namespace hz
