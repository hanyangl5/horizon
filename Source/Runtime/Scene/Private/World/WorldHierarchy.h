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
    bool                  prepareLocalChange(ecs_entity_t entity, TypeID type, bool adding);
    void                  finishDeferredChanges();
    bool                  setParent(ecs_entity_t entity, ecs_entity_t parent, bool keepWorld);
    bool                  destroy(ecs_entity_t entity, bool subtree);
    void                  update();
    const WorldTransform* get(ecs_entity_t entity) const;
    void                  commit();
    void                  resetHistory(ecs_entity_t root);

private:
    struct Node
    {
        ecs_entity_t   entity = 0;
        uint32_t       parent = UINT32_MAX;
        uint32_t       subtreeEnd = 0;
        WorldTransform transform;
        bool           dirty = false;
        bool           historyDirty = false;
        bool           initialized = false;
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

    uint32_t find(ecs_entity_t entity) const;
    void     rebuild();
    Matrix4  localMatrix(ecs_entity_t entity) const;
    bool     writeLocalMatrix(ecs_entity_t entity, const Matrix4& matrix);
    void     markDirty(ecs_entity_t entity);

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
    Array<uint32_t>     historyDirty;
    bool                structureDirty = true;
};
} // namespace hz
