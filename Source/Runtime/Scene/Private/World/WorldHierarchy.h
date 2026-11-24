/* Copyright (c) 2026 Horizon */
#pragma once

#include "WorldInternal.h"
#include "Scene/SceneComponents.h"

namespace hz
{
class WorldHierarchy
{
public:
    explicit WorldHierarchy(WorldImpl& world);
    ~WorldHierarchy();
    WorldHierarchy(const WorldHierarchy&) = delete;
    WorldHierarchy& operator=(const WorldHierarchy&) = delete;

    void                  invalidateStructure() { structureDirty = true; }
    // A new entity has no parent or children, so it can be appended as a root instead of rebuilding the order.
    void                  addRoot(ecs_entity_t entity);
    bool                  prepareLocalChange(ecs_entity_t entity, TypeID Type, bool adding);
    void                  finishDeferredChanges();
    bool                  SetParent(ecs_entity_t entity, ecs_entity_t parent, bool keepWorld);
    bool                  Destroy(ecs_entity_t entity, bool subtree);
    void                  Update();
    const WorldTransform* Get(ecs_entity_t entity) const;

private:
    struct Node
    {
        ecs_entity_t   entity = 0;
        uint32_t       parent = UINT32_MAX;
        uint32_t       subtreeEnd = 0;
        WorldTransform transform;
        bool           dirty = false;
    };
    struct BuildNode
    {
        ecs_entity_t entity;
        uint32_t     parent = UINT32_MAX;
        uint32_t     firstChild = UINT32_MAX;
        uint32_t     nextSibling = UINT32_MAX;
        uint32_t     output = 0;
    };
    struct Index
    {
        ecs_entity_t key;
        uint32_t     value;
    };
    struct LocalState
    {
        ecs_entity_t key;
        uint32_t     value;
    };

    uint32_t Find(ecs_entity_t entity) const;
    void     rebuild();
    Matrix4  localMatrix(ecs_entity_t entity) const;
    bool     writeLocalMatrix(ecs_entity_t entity, const Matrix4& matrix);
    void     markTransformDirty(ecs_entity_t entity);
    void     removeLeaf(uint32_t index);

    WorldImpl&          world;
    ecs_query_t*        pEntities = nullptr;
    ecs_entity_t        transformType = 0;
    ecs_entity_t        matrixType = 0;
    Index*              pIndices = nullptr;
    Index*              pBuildIndices = nullptr;
    LocalState*         pPendingLocals = nullptr;
    Array<ecs_entity_t> pendingLocals;
    Array<Node>         nodes;
    Array<Node>         scratch;
    Array<BuildNode>    buildNodes;
    Array<uint32_t>     stack;
    Array<uint32_t>     dirtyRoots;
    // Destroyed leaves left in place by removeLeaf; their entity is 0.
    uint32_t            tombstones = 0;
    bool                structureDirty = true;
};
} // namespace hz
