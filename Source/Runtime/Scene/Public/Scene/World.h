/* Copyright (c) 2026 Horizon */
#pragma once

#include "Scene/SceneReflection.h"
#include "Scene/SceneID.h"

namespace hz
{
class World;
class WorldQuery;
class WorldImpl;
class WorldQueryImpl;

// Runtime identity, scoped to the World that created it. isValid does not test liveness.
class Entity
{
public:
    Entity() = default;
    constexpr bool isValid() const { return value != 0; }
    constexpr bool operator==(const Entity&) const = default;

private:
    friend class World;
    friend class WorldQuery;
    // Two 64-bit fields keep the type padding-free: hash maps key on the raw bytes.
    explicit Entity(uint64_t value, uint64_t world): value(value), world(world) {}
    uint64_t value = 0;
    uint64_t world = 0;
};

struct WorldTransform;

struct PropertyValue
{
    PropertyKind kind = PropertyKind::Boolean;
    const void*  Data = nullptr;
    // Strings include the terminating null; other values must match the reflected field Size.
    uint32_t     Size = 0;
};

class DeferredChanges
{
public:
    ~DeferredChanges();
    DeferredChanges(const DeferredChanges&) = delete;
    DeferredChanges& operator=(const DeferredChanges&) = delete;

private:
    friend class World;
    explicit DeferredChanges(World& world);
    World& world;
};

class World
{
public:
    // Registers the types currently in registry; descriptors must outlive the World.
    explicit World(const TypeRegistry& registry, const IDGenerator& entityGuids = {});
    ~World();
    World(const World&) = delete;
    World& operator=(const World&) = delete;

    Entity createEntity();
    // Persistent entities. Explicit IDs must be nonzero and unique within this World.
    Entity createPersistentEntity();
    Entity createPersistentEntity(const Guid& id);
    // Pending creations Reserve their IDs immediately but become visible to lookup at defer commit.
    Entity getEntity(const Guid& id) const;
    Guid   getEntityGuid(Entity entity) const;
    // Leaf deletion can be deferred; deleting a parent preserves its children and requires a committed World.
    bool   destroyEntity(Entity entity);
    bool   isAlive(Entity entity) const;
    bool   destroySubtree(Entity entity);

    Entity                getParent(Entity entity) const;
    // Hierarchy edits run outside defer/query scopes. An invalid parent means the root.
    bool                  setParent(Entity entity, Entity parent = {}, bool keepWorld = true);
    void                  updateTransforms();
    // Valid until the next structural update. Call updateTransforms before reading.
    const WorldTransform* getWorldTransform(Entity entity) const;
    // Advance history only after a rendered frame is submitted.
    void                  commitTransforms();
    void                  resetTransformHistory(Entity root = {});

    bool        addComponent(Entity entity, TypeID type);
    bool        removeComponent(Entity entity, TypeID type);
    // Copies the value. During defer, existing components update immediately; new components wait for commit.
    bool        setComponent(Entity entity, TypeID type, const void* pValue);
    // Read-only storage view. References expire on structural changes; owning values can change on setComponent.
    const void* getComponent(Entity entity, TypeID type) const;

    // Edits require an existing component and take effect immediately, including during defer.
    bool setProperty(Entity entity, TypeID type, PropertyID property, const PropertyValue& value);
    bool resizeArray(Entity entity, TypeID type, PropertyID property, uint32_t Count);
    // pValue points to an element of the reflected array type, including when inserting from this array.
    bool insertArrayElement(Entity entity, TypeID type, PropertyID property, uint32_t index, const void* pValue);
    bool removeArrayElement(Entity entity, TypeID type, PropertyID property, uint32_t index);
    bool setArrayElementProperty(Entity entity, TypeID type, PropertyID property, uint32_t index, PropertyID elementProperty,
                                 const PropertyValue& value);

    // Defers structural changes until the outermost scope ends. World must outlive its scopes and queries.
    DeferredChanges defer();
    bool            isDeferred() const;

private:
    friend class DeferredChanges;
    friend class WorldQuery;
    friend class WorldQueryImpl;
    friend class WorldArchive;
    friend class SceneAsset;
    // False for an invalid Entity; foreign entities fail the comparison and assert in debug.
    bool       owns(Entity entity) const;
    void       attachParent(Entity entity, Entity parent);
    WorldImpl* pImpl = nullptr;
};

class WorldQuery
{
public:
    // Matches entities that own all listed components. An Empty list matches all World entities.
    WorldQuery(World& world, Span<TypeID> types);
    ~WorldQuery();
    WorldQuery(const WorldQuery&) = delete;
    WorldQuery& operator=(const WorldQuery&) = delete;
    bool        isValid() const;

    // Fields follow the requested type order. Returning false stops iteration. Structural changes are deferred.
    void each(bool (*pCallback)(Entity entity, Span<const void*> fields, void* UserData), void* UserData = nullptr);

private:
    WorldQueryImpl* pImpl = nullptr;
};
} // namespace hz
