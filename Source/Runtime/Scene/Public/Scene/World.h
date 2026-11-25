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
class WorldChangeLogImpl;

// Runtime identity, scoped to the World that created it. IsValid does not test liveness.
class Entity
{
public:
    Entity() = default;
    constexpr bool IsValid() const { return value != 0; }
    constexpr bool operator==(const Entity&) const = default;

private:
    friend class World;
    friend class WorldQuery;
    friend class WorldChangeLogImpl;
    // Two 64-bit fields keep the Type padding-free: Hash maps key on the raw bytes.
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
    // Registers the types currently in registry; Descriptors must outlive the World.
    explicit World(const TypeRegistry& registry, const IDGenerator& entityGuids = {});
    ~World();
    World(const World&) = delete;
    World& operator=(const World&) = delete;

    Entity CreateEntity();
    // Persistent entities. Explicit IDs must be nonzero and unique within this World.
    Entity CreatePersistentEntity();
    Entity CreatePersistentEntity(const Guid& id);
    // Pending creations Reserve their IDs immediately but become visible to lookup at Defer commit.
    Entity GetEntity(const Guid& id) const;
    Guid   GetEntityGuid(Entity entity) const;
    // Leaf deletion can be deferred; deleting a parent preserves its children and requires a committed World.
    bool   DestroyEntity(Entity entity);
    bool   IsAlive(Entity entity) const;
    bool   DestroySubtree(Entity entity);

    Entity                GetParent(Entity entity) const;
    // Hierarchy edits run outside Defer/query scopes. An invalid parent means the root.
    bool                  SetParent(Entity entity, Entity parent = {}, bool keepWorld = true);
    void                  UpdateTransforms();
    // Valid until the next structural Update. Call UpdateTransforms before reading.
    const WorldTransform* GetWorldTransform(Entity entity) const;

    bool        AddComponent(Entity entity, TypeID Type);
    bool        RemoveComponent(Entity entity, TypeID Type);
    // Copies the value. During Defer, existing components Update immediately; new components Wait for commit.
    bool        SetComponent(Entity entity, TypeID Type, const void* pValue);
    // Read-only storage view. References expire on structural changes; owning values can change on SetComponent.
    const void* GetComponent(Entity entity, TypeID Type) const;

    // Edits require an existing component and take effect immediately, including during Defer.
    bool SetProperty(Entity entity, TypeID Type, PropertyID property, const PropertyValue& value);
    bool ResizeArray(Entity entity, TypeID Type, PropertyID property, uint32_t Count);
    // pValue points to an element of the reflected array Type, including when inserting from this array.
    bool InsertArrayElement(Entity entity, TypeID Type, PropertyID property, uint32_t index, const void* pValue);
    bool RemoveArrayElement(Entity entity, TypeID Type, PropertyID property, uint32_t index);
    bool SetArrayElementProperty(Entity entity, TypeID Type, PropertyID property, uint32_t index, PropertyID elementProperty,
                                 const PropertyValue& value);

    // Defers structural changes until the outermost scope ends. World must outlive its scopes and queries.
    DeferredChanges Defer();
    bool            IsDeferred() const;

private:
    friend class DeferredChanges;
    friend class WorldQuery;
    friend class WorldQueryImpl;
    friend class WorldChangeLog;
    friend class WorldChangeLogImpl;
    friend class WorldArchive;
    friend class SceneAsset;
    // False for an invalid Entity; foreign entities fail the comparison and assert in debug.
    bool       Owns(Entity entity) const;
    void       AttachParent(Entity entity, Entity parent);
    WorldImpl* pImpl = nullptr;
};

// Collects entities whose components, hierarchy or world transform changed, so consumers can update incrementally.
// Entries may repeat and may name entities destroyed since. When the log overflows it drops its entries and reports
// overflow instead, and the consumer rescans the World. The World must not be destroyed or replaced while a log is live.
class WorldChangeLog
{
public:
    // A required component limits the log to its owners, including addition and removal of the component itself.
    explicit WorldChangeLog(World& world, TypeID RequiredComponent = 0);
    ~WorldChangeLog();
    WorldChangeLog(const WorldChangeLog&) = delete;
    WorldChangeLog& operator=(const WorldChangeLog&) = delete;

    // Changes recorded since the previous call, which are then cleared. Call after UpdateTransforms so transform
    // propagation is included. The span expires at the next call.
    Span<Entity> Drain(bool& overflowed);

private:
    WorldChangeLogImpl* pImpl = nullptr;
};

class WorldQuery
{
public:
    // Matches entities that own all listed components. An Empty list matches all World entities.
    WorldQuery(World& world, Span<TypeID> types);
    ~WorldQuery();
    WorldQuery(const WorldQuery&) = delete;
    WorldQuery& operator=(const WorldQuery&) = delete;
    bool        IsValid() const;

    // Fields follow the requested Type order. Returning false stops iteration. Structural changes are deferred.
    void Each(bool (*pCallback)(Entity entity, Span<const void*> fields, void* UserData), void* UserData = nullptr);

private:
    WorldQueryImpl* pImpl = nullptr;
};
} // namespace hz
