/* Copyright (c) 2026 Horizon */
#pragma once

#include "Scene/SceneReflection.h"

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
    explicit Entity(uint64_t value): value(value) {}
    uint64_t value = 0;
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
    explicit World(const TypeRegistry& registry);
    ~World();
    World(const World&) = delete;
    World& operator=(const World&) = delete;

    Entity createEntity();
    bool   destroyEntity(Entity entity);
    bool   isAlive(Entity entity) const;

    bool        addComponent(Entity entity, TypeID type);
    bool        removeComponent(Entity entity, TypeID type);
    // Copies the value. During defer, existing components update immediately; new components wait for commit.
    bool        setComponent(Entity entity, TypeID type, const void* pValue);
    // Read-only storage view. References expire on structural changes; owning values can change on setComponent.
    const void* getComponent(Entity entity, TypeID type) const;

    // Defers structural changes until the outermost scope ends. World must outlive its scopes and queries.
    DeferredChanges defer();
    bool            isDeferred() const;

private:
    friend class DeferredChanges;
    friend class WorldQuery;
    friend class WorldQueryImpl;
    WorldImpl* pImpl = nullptr;
};

class WorldQuery
{
public:
    // Matches entities that own all listed components. An empty list matches all World entities.
    WorldQuery(World& world, Span<TypeID> types);
    ~WorldQuery();
    WorldQuery(const WorldQuery&) = delete;
    WorldQuery& operator=(const WorldQuery&) = delete;
    bool        isValid() const;

    // Fields follow the requested type order. Returning false stops iteration. Structural changes are deferred.
    void each(bool (*pCallback)(Entity entity, Span<const void*> fields, void* pUserData), void* pUserData = nullptr);

private:
    WorldQueryImpl* pImpl = nullptr;
};
} // namespace hz
