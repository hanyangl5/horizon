/* Copyright (c) 2026 Horizon */

#include "WorldHierarchy.h"
#include "Scene/SceneComponents.h"

#include <math.h>
#include <stdlib.h>

#include "Core/IMemory.h"

namespace hz
{
static int CompareIndices(const void* LeftPointer, const void* RightPointer)
{
    const uint32_t LeftValue = *(const uint32_t*)LeftPointer;
    const uint32_t RightValue = *(const uint32_t*)RightPointer;
    return (LeftValue > RightValue) - (LeftValue < RightValue);
}

WorldHierarchy::WorldHierarchy(WorldImpl& WorldData): world(WorldData)
{
    const WorldType* TransformPointer = WorldData.findType(SceneTypes::localTransform);
    const WorldType* MatrixPointer = WorldData.findType(SceneTypes::localMatrix);
    transformType = TransformPointer ? TransformPointer->component : 0;
    matrixType = MatrixPointer ? MatrixPointer->component : 0;
    const ecs_query_desc_t Desc = { .filter = { .terms = { { .id = WorldData.entityTag, .src = { .flags = EcsSelf } } } } };
    pEntities = ecs_query_init(WorldData.pWorld, &Desc);
    ASSERT(pEntities);
}

WorldHierarchy::~WorldHierarchy()
{
    if (structureDirty)
        rebuild();
    ecs_query_fini(pEntities);
    // Avoid Flecs recursively cascading deletion through a deep hierarchy at ecs_fini.
    for (uint32_t I = nodes.Size(); I > 0; --I)
    {
        if (nodes[I - 1].entity)
        {
            ecs_delete(world.pWorld, nodes[I - 1].entity);
        }
    }
    hmfree(pIndices);
    hmfree(pBuildIndices);
    hmfree(pPendingLocals);
}

uint32_t WorldHierarchy::Find(ecs_entity_t EntityHandle) const
{
    ptrdiff_t    IndexValue;
    Index*       Lookup = pIndices;
    const Index* EntryPointer = Lookup ? hmgetp_ts(Lookup, EntityHandle, IndexValue) : nullptr;
    return EntryPointer && IndexValue >= 0 ? EntryPointer->value : UINT32_MAX;
}

void WorldHierarchy::addRoot(ecs_entity_t EntityHandle)
{
    world.recordChange(EntityHandle);
    if (structureDirty || ecs_is_deferred(world.pWorld))
    {
        // A deferred entity is not visible to the rebuild query until the scope commits.
        structureDirty = true;
        return;
    }
    const uint32_t IndexValue = nodes.Size();
    nodes.PushBack({ .entity = EntityHandle, .subtreeEnd = IndexValue + 1, .dirty = true });
    const Index Mapping = { .key = EntityHandle, .value = IndexValue };
    hmputs(pIndices, Mapping);
    dirtyRoots.PushBack(IndexValue);
}

void WorldHierarchy::removeLeaf(uint32_t IndexValue)
{
    // The slot stays as a tombstone so no other index moves; the next rebuild drops it.
    hmdel(pIndices, nodes[IndexValue].entity);
    nodes[IndexValue].entity = 0;
    if (++tombstones > nodes.Size() / 4)
    {
        structureDirty = true;
    }
}

void WorldHierarchy::markTransformDirty(ecs_entity_t EntityHandle)
{
    if (structureDirty)
        return;
    const uint32_t IndexValue = Find(EntityHandle);
    if (IndexValue != UINT32_MAX && !nodes[IndexValue].dirty)
    {
        nodes[IndexValue].dirty = true;
        dirtyRoots.PushBack(IndexValue);
    }
}

bool WorldHierarchy::prepareLocalChange(ecs_entity_t EntityHandle, TypeID Type, bool Adding)
{
    // Every component edit passes through here, including ones that turn out to be rejected; a spurious entry only
    // costs the consumer one comparison.
    world.recordChange(EntityHandle);
    if (Type != SceneTypes::localTransform && Type != SceneTypes::localMatrix)
    {
        return true;
    }
    const uint32_t    Bit = Type == SceneTypes::localTransform ? 1u : 2u;
    const LocalState* Pending = hmgetp_null(pPendingLocals, EntityHandle);
    const uint32_t    Mask = Pending ? Pending->value
                                     : ((transformType && ecs_has_id(world.pWorld, EntityHandle, transformType) ? 1u : 0u) |
                                     (matrixType && ecs_has_id(world.pWorld, EntityHandle, matrixType) ? 2u : 0u));
    if (Adding && (Mask & (Bit ^ 3u)))
    {
        LOGF(Error, "An entity cannot have both LocalTransform and Matrix4");
        return false;
    }
    const uint32_t Next = Adding ? Mask | Bit : Mask & ~Bit;
    if (ecs_is_deferred(world.pWorld) && Next != Mask)
    {
        if (!Pending)
        {
            pendingLocals.PushBack(EntityHandle);
        }
        const LocalState StateValue = { .key = EntityHandle, .value = Next };
        hmputs(pPendingLocals, StateValue);
    }
    markTransformDirty(EntityHandle);
    return true;
}

void WorldHierarchy::finishDeferredChanges()
{
    for (ecs_entity_t EntityHandle : pendingLocals)
    {
        hmdel(pPendingLocals, EntityHandle);
    }
    pendingLocals.Clear();
}

Matrix4 WorldHierarchy::localMatrix(ecs_entity_t EntityHandle) const
{
    const Matrix4* MatrixPointer = matrixType ? (const Matrix4*)ecs_get_id(world.pWorld, EntityHandle, matrixType) : nullptr;
    if (MatrixPointer)
    {
        return *MatrixPointer;
    }
    const LocalTransform* TransformPointer =
        transformType ? (const LocalTransform*)ecs_get_id(world.pWorld, EntityHandle, transformType) : nullptr;
    return TransformPointer ? Matrix4(TransformPointer->rotation, TransformPointer->translation) * Matrix4::scale(TransformPointer->scale)
                            : Matrix4::identity();
}

void WorldHierarchy::rebuild()
{
    buildNodes.Clear();
    for (const Node& NodeValue : nodes)
    {
        hmdel(pBuildIndices, NodeValue.entity);
    }
    ecs_iter_t It = ecs_query_iter(world.pWorld, pEntities);
    while (ecs_query_next(&It))
    {
        for (int32_t Row = 0; Row < It.count; ++Row)
        {
            const Index Mapping = { .key = It.entities[Row], .value = buildNodes.Size() };
            hmputs(pBuildIndices, Mapping);
            buildNodes.PushBack({ .entity = It.entities[Row] });
        }
    }
    uint32_t FirstRoot = UINT32_MAX;
    for (uint32_t I = 0; I < buildNodes.Size(); ++I)
    {
        BuildNode&         NodeValue = buildNodes[I];
        const ecs_entity_t ParentValue = ecs_get_target(world.pWorld, NodeValue.entity, world.parentRelation, 0);
        const Index*       ParentPointer = hmgetp_null(pBuildIndices, ParentValue);
        NodeValue.parent = ParentPointer ? ParentPointer->value : UINT32_MAX;
        uint32_t& First = ParentPointer ? buildNodes[NodeValue.parent].firstChild : FirstRoot;
        NodeValue.nextSibling = First;
        First = I;
    }

    scratch.Clear();
    scratch.Reserve(buildNodes.Size());
    stack.Clear();
    uint32_t Current = FirstRoot;
    while (Current != UINT32_MAX)
    {
        BuildNode& EntryValue = buildNodes[Current];
        EntryValue.output = scratch.Size();
        const uint32_t Old = Find(EntryValue.entity);
        Node           NodeValue = Old != UINT32_MAX ? nodes[Old] : Node{};
        NodeValue.entity = EntryValue.entity;
        NodeValue.parent = EntryValue.parent == UINT32_MAX ? UINT32_MAX : buildNodes[EntryValue.parent].output;
        NodeValue.dirty = true;
        scratch.PushBack(NodeValue);
        stack.PushBack(Current);
        if (EntryValue.firstChild != UINT32_MAX)
        {
            Current = EntryValue.firstChild;
            continue;
        }
        Current = UINT32_MAX;
        while (!stack.Empty())
        {
            const BuildNode& Finished = buildNodes[stack[stack.Size() - 1]];
            scratch[Finished.output].subtreeEnd = scratch.Size();
            stack.PopBack();
            if (Finished.nextSibling != UINT32_MAX)
            {
                Current = Finished.nextSibling;
                break;
            }
        }
    }
    ASSERT(scratch.Size() == buildNodes.Size());
    for (const Node& NodeValue : nodes)
    {
        hmdel(pIndices, NodeValue.entity);
    }
    Array<Node> Old = std::move(nodes);
    nodes = std::move(scratch);
    scratch = std::move(Old);
    for (uint32_t I = 0; I < nodes.Size(); ++I)
    {
        const Index Mapping = { .key = nodes[I].entity, .value = I };
        hmputs(pIndices, Mapping);
    }
    // A rebuild touches every node, so consumers rescan instead of replaying one entry per entity.
    for (WorldChangeLogImpl* Log : world.changeLogs)
    {
        Log->overflowed = true;
        Log->entries.Clear();
    }
    dirtyRoots.Clear();
    dirtyRoots.Reserve(nodes.Size());
    for (uint32_t I = 0; I < nodes.Size(); I = nodes[I].subtreeEnd)
    {
        dirtyRoots.PushBack(I);
    }
    tombstones = 0;
    structureDirty = false;
}

void WorldHierarchy::Update()
{
    world.checkThread();
    ASSERT(!ecs_is_deferred(world.pWorld));
    if (structureDirty)
        rebuild();
    if (dirtyRoots.Empty())
    {
        return;
    }
    qsort(dirtyRoots.GetData(), dirtyRoots.Size(), sizeof(uint32_t), CompareIndices);
    uint32_t ProcessedEnd = 0;
    for (uint32_t Root : dirtyRoots)
    {
        if (Root < ProcessedEnd)
        {
            continue;
        }
        ProcessedEnd = nodes[Root].subtreeEnd;
        for (uint32_t I = Root; I < ProcessedEnd; ++I)
        {
            Node& NodeValue = nodes[I];
            if (!NodeValue.entity)
            {
                continue;
            }
            NodeValue.transform.current = NodeValue.parent == UINT32_MAX
                                              ? localMatrix(NodeValue.entity)
                                              : nodes[NodeValue.parent].transform.current * localMatrix(NodeValue.entity);
            NodeValue.dirty = false;
            world.recordChange(NodeValue.entity);
        }
    }
    dirtyRoots.Clear();
}

const WorldTransform* WorldHierarchy::Get(ecs_entity_t EntityHandle) const
{
    world.checkThread();
    ASSERT(!structureDirty && dirtyRoots.Empty());
    const uint32_t IndexValue = Find(EntityHandle);
    return IndexValue != UINT32_MAX ? &nodes[IndexValue].transform : nullptr;
}

bool WorldHierarchy::writeLocalMatrix(ecs_entity_t EntityHandle, const Matrix4& MatrixValue)
{
    for (uint32_t Column = 0; Column < 4; ++Column)
    {
        for (uint32_t Row = 0; Row < 4; ++Row)
        {
            if (!isfinite((float)MatrixValue.getElem(Column, Row)))
            {
                LOGF(Error, "Cannot preserve a non-finite world transform");
                return false;
            }
        }
    }
    LocalTransform TransformValue;
    decompose(MatrixValue, &TransformValue.translation, &TransformValue.rotation, &TransformValue.scale);
    const Matrix4 Reconstructed = Matrix4(TransformValue.rotation, TransformValue.translation) * Matrix4::scale(TransformValue.scale);
    bool          Representable = true;
    for (uint32_t Column = 0; Column < 4; ++Column)
    {
        for (uint32_t Row = 0; Row < 4; ++Row)
        {
            const float Expected = (float)MatrixValue.getElem(Column, Row);
            const float Actual = (float)Reconstructed.getElem(Column, Row);
            if (!isfinite(Actual) || fabsf(Actual - Expected) > 1e-5f * fmaxf(1.0f, fabsf(Expected)))
            {
                Representable = false;
            }
        }
    }
    if (Representable && transformType)
    {
        if (matrixType)
            ecs_remove_id(world.pWorld, EntityHandle, matrixType);
        ecs_set_id(world.pWorld, EntityHandle, transformType, sizeof(TransformValue), &TransformValue);
    }
    else if (matrixType)
    {
        if (transformType)
            ecs_remove_id(world.pWorld, EntityHandle, transformType);
        ecs_set_id(world.pWorld, EntityHandle, matrixType, sizeof(MatrixValue), &MatrixValue);
    }
    else
    {
        LOGF(Error, "Preserving world transforms requires registered local transform components");
        return false;
    }
    markTransformDirty(EntityHandle);
    return true;
}

bool WorldHierarchy::SetParent(ecs_entity_t EntityHandle, ecs_entity_t ParentValue, bool KeepWorld)
{
    if (ecs_is_deferred(world.pWorld))
    {
        LOGF(Error, "Reparenting requires a committed World outside query/Defer scopes");
        return false;
    }
    for (ecs_entity_t Ancestor = ParentValue; Ancestor; Ancestor = ecs_get_target(world.pWorld, Ancestor, world.parentRelation, 0))
    {
        if (Ancestor == EntityHandle)
        {
            return false;
        }
    }
    const ecs_entity_t Previous = ecs_get_target(world.pWorld, EntityHandle, world.parentRelation, 0);
    if (Previous == ParentValue)
    {
        return true;
    }
    if (KeepWorld)
    {
        Update();
        Matrix4 Local = Get(EntityHandle)->current;
        if (ParentValue)
        {
            const Matrix4& ParentWorld = Get(ParentValue)->current;
            const float    Det = (float)determinant(ParentWorld);
            if (!isfinite(Det) || Det == 0.0f)
            {
                LOGF(Error, "Cannot preserve world transform under a singular parent");
                return false;
            }
            Local = inverse(ParentWorld) * Local;
            // Preserve the exact affine row after floating-point inversion.
            Local.setRow(3, Vector4(0, 0, 0, 1));
        }
        if (!writeLocalMatrix(EntityHandle, Local))
        {
            return false;
        }
    }
    if (ParentValue)
    {
        ecs_add_pair(world.pWorld, EntityHandle, world.parentRelation, ParentValue);
    }
    else if (Previous)
    {
        ecs_remove_pair(world.pWorld, EntityHandle, world.parentRelation, Previous);
    }
    invalidateStructure();
    return true;
}

bool WorldHierarchy::Destroy(ecs_entity_t EntityHandle, bool Subtree)
{
    ecs_term_t Term = { .id = ecs_pair(world.parentRelation, EntityHandle), .src = { .flags = EcsSelf } };
    ecs_iter_t Children = ecs_term_iter(world.pWorld, &Term);
    const bool HasChildren = ecs_term_next(&Children);
    if (HasChildren)
    {
        ecs_iter_fini(&Children);
    }
    if (!HasChildren)
    {
        world.recordChange(EntityHandle);
        ecs_delete(world.pWorld, EntityHandle);
        // A deferred delete still shows its entity to the rebuild query until the scope commits.
        const uint32_t IndexValue = structureDirty || ecs_is_deferred(world.pWorld) ? UINT32_MAX : Find(EntityHandle);
        if (IndexValue != UINT32_MAX)
        {
            removeLeaf(IndexValue);
        }
        else
        {
            invalidateStructure();
        }
        return true;
    }
    if (ecs_is_deferred(world.pWorld))
    {
        LOGF(Error, "Deleting a hierarchy requires a committed World outside query/Defer scopes");
        return false;
    }
    if (!Subtree && !matrixType)
    {
        LOGF(Error, "Deleting a parent requires registered Matrix4 support");
        return false;
    }
    Update();
    const uint32_t Root = Find(EntityHandle);
    const uint32_t End = nodes[Root].subtreeEnd;
    if (Subtree)
    {
        for (uint32_t I = End; I > Root; --I)
        {
            if (nodes[I - 1].entity)
            {
                world.recordChange(nodes[I - 1].entity);
                ecs_delete(world.pWorld, nodes[I - 1].entity);
            }
        }
    }
    else
    {
        // Each direct child's cached subtree range remains valid until the next Update.
        for (uint32_t I = Root + 1; I < End; I = nodes[I].subtreeEnd)
        {
            if (!nodes[I].entity)
            {
                continue;
            }
            if (!writeLocalMatrix(nodes[I].entity, nodes[I].transform.current))
            {
                return false;
            }
            ecs_remove_pair(world.pWorld, nodes[I].entity, world.parentRelation, EntityHandle);
        }
        ecs_delete(world.pWorld, EntityHandle);
    }
    invalidateStructure();
    return true;
}

Entity World::GetParent(Entity EntityHandle) const
{
    return IsAlive(EntityHandle) ? Entity(ecs_get_target(pImpl->pWorld, EntityHandle.value, pImpl->parentRelation, 0), pImpl->id)
                                 : Entity{};
}
void World::AttachParent(Entity EntityHandle, Entity ParentValue)
{
    ASSERT(IsAlive(EntityHandle) && IsAlive(ParentValue) && !IsDeferred());
    ecs_add_pair(pImpl->pWorld, EntityHandle.value, pImpl->parentRelation, ParentValue.value);
    pImpl->pHierarchy->invalidateStructure();
}
bool World::SetParent(Entity EntityHandle, Entity ParentValue, bool KeepWorld)
{
    if (!IsAlive(EntityHandle) || (ParentValue.IsValid() && !IsAlive(ParentValue)))
    {
        return false;
    }
    return pImpl->pHierarchy->SetParent(EntityHandle.value, ParentValue.value, KeepWorld);
}
bool World::DestroyEntity(Entity EntityHandle) { return IsAlive(EntityHandle) && pImpl->pHierarchy->Destroy(EntityHandle.value, false); }
bool World::DestroySubtree(Entity EntityHandle) { return IsAlive(EntityHandle) && pImpl->pHierarchy->Destroy(EntityHandle.value, true); }
void                  World::UpdateTransforms() { pImpl->pHierarchy->Update(); }
const WorldTransform* World::GetWorldTransform(Entity EntityHandle) const
{
    return IsAlive(EntityHandle) ? pImpl->pHierarchy->Get(EntityHandle.value) : nullptr;
}
} // namespace hz
