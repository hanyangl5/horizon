/* Copyright (c) 2026 Horizon */

#include "WorldHierarchy.h"
#include "Scene/SceneComponents.h"

#include <math.h>
#include <stdlib.h>

#include "Core/IMemory.h"

namespace hz
{
static int compareIndices(const void* pLeft, const void* pRight)
{
    const uint32_t left = *(const uint32_t*)pLeft;
    const uint32_t right = *(const uint32_t*)pRight;
    return (left > right) - (left < right);
}

WorldHierarchy::WorldHierarchy(WorldImpl& world): world(world)
{
    const WorldType* pTransform = world.findType(SceneTypes::localTransform);
    const WorldType* pMatrix = world.findType(SceneTypes::localMatrix);
    transformType = pTransform ? pTransform->component : 0;
    matrixType = pMatrix ? pMatrix->component : 0;
    const ecs_query_desc_t desc = { .filter = { .terms = { { .id = world.entityTag, .src = { .flags = EcsSelf } } } } };
    pEntities = ecs_query_init(world.pWorld, &desc);
    ASSERT(pEntities);
}

WorldHierarchy::~WorldHierarchy()
{
    if (structureDirty)
        rebuild();
    ecs_query_fini(pEntities);
    // Avoid Flecs recursively cascading deletion through a deep hierarchy at ecs_fini.
    for (uint32_t i = nodes.Size(); i > 0; --i)
        ecs_delete(world.pWorld, nodes[i - 1].entity);
    hmfree(pIndices);
    hmfree(pBuildIndices);
    hmfree(pPendingLocals);
}

uint32_t WorldHierarchy::find(ecs_entity_t entity) const
{
    ptrdiff_t    index;
    Index*       pLookup = pIndices;
    const Index* pEntry = pLookup ? hmgetp_ts(pLookup, entity, index) : nullptr;
    return pEntry && index >= 0 ? pEntry->value : UINT32_MAX;
}

void WorldHierarchy::markTransformDirty(ecs_entity_t entity)
{
    if (structureDirty)
        return;
    const uint32_t index = find(entity);
    if (index != UINT32_MAX && !nodes[index].dirty)
    {
        nodes[index].dirty = true;
        dirtyRoots.PushBack(index);
    }
}

bool WorldHierarchy::prepareLocalChange(ecs_entity_t entity, TypeID Type, bool adding)
{
    if (Type != SceneTypes::localTransform && Type != SceneTypes::localMatrix)
        return true;
    const uint32_t    bit = Type == SceneTypes::localTransform ? 1u : 2u;
    const LocalState* pPending = hmgetp_null(pPendingLocals, entity);
    const uint32_t    mask = pPending ? pPending->value
                                      : ((transformType && ecs_has_id(world.pWorld, entity, transformType) ? 1u : 0u) |
                                      (matrixType && ecs_has_id(world.pWorld, entity, matrixType) ? 2u : 0u));
    if (adding && (mask & (bit ^ 3u)))
    {
        LOGF(Error, "An entity cannot have both LocalTransform and Matrix4");
        return false;
    }
    const uint32_t next = adding ? mask | bit : mask & ~bit;
    if (ecs_is_deferred(world.pWorld) && next != mask)
    {
        if (!pPending)
            pendingLocals.PushBack(entity);
        const LocalState state = { .key = entity, .value = next };
        hmputs(pPendingLocals, state);
    }
    markTransformDirty(entity);
    return true;
}

void WorldHierarchy::finishDeferredChanges()
{
    for (ecs_entity_t entity : pendingLocals)
        hmdel(pPendingLocals, entity);
    pendingLocals.Clear();
}

Matrix4 WorldHierarchy::localMatrix(ecs_entity_t entity) const
{
    const Matrix4* pMatrix = matrixType ? (const Matrix4*)ecs_get_id(world.pWorld, entity, matrixType) : nullptr;
    if (pMatrix)
        return *pMatrix;
    const LocalTransform* pTransform = transformType ? (const LocalTransform*)ecs_get_id(world.pWorld, entity, transformType) : nullptr;
    return pTransform ? Matrix4(pTransform->rotation, pTransform->translation) * Matrix4::scale(pTransform->scale) : Matrix4::identity();
}

void WorldHierarchy::rebuild()
{
    buildNodes.Clear();
    for (const Node& node : nodes)
        hmdel(pBuildIndices, node.entity);
    ecs_iter_t it = ecs_query_iter(world.pWorld, pEntities);
    while (ecs_query_next(&it))
    {
        for (int32_t row = 0; row < it.count; ++row)
        {
            const Index mapping = { .key = it.entities[row], .value = buildNodes.Size() };
            hmputs(pBuildIndices, mapping);
            buildNodes.PushBack({ .entity = it.entities[row] });
        }
    }
    uint32_t firstRoot = UINT32_MAX;
    for (uint32_t i = 0; i < buildNodes.Size(); ++i)
    {
        BuildNode&         node = buildNodes[i];
        const ecs_entity_t parent = ecs_get_target(world.pWorld, node.entity, world.parentRelation, 0);
        const Index*       pParent = hmgetp_null(pBuildIndices, parent);
        node.parent = pParent ? pParent->value : UINT32_MAX;
        uint32_t& first = pParent ? buildNodes[node.parent].firstChild : firstRoot;
        node.nextSibling = first;
        first = i;
    }

    scratch.Clear();
    scratch.Reserve(buildNodes.Size());
    stack.Clear();
    uint32_t current = firstRoot;
    while (current != UINT32_MAX)
    {
        BuildNode& entry = buildNodes[current];
        entry.output = scratch.Size();
        const uint32_t old = find(entry.entity);
        Node           node = old != UINT32_MAX ? nodes[old] : Node{};
        node.entity = entry.entity;
        node.parent = entry.parent == UINT32_MAX ? UINT32_MAX : buildNodes[entry.parent].output;
        node.dirty = true;
        node.historyDirty = false;
        scratch.PushBack(node);
        stack.PushBack(current);
        if (entry.firstChild != UINT32_MAX)
        {
            current = entry.firstChild;
            continue;
        }
        current = UINT32_MAX;
        while (!stack.Empty())
        {
            const BuildNode& finished = buildNodes[stack[stack.Size() - 1]];
            scratch[finished.output].subtreeEnd = scratch.Size();
            stack.PopBack();
            if (finished.nextSibling != UINT32_MAX)
            {
                current = finished.nextSibling;
                break;
            }
        }
    }
    ASSERT(scratch.Size() == buildNodes.Size());
    for (const Node& node : nodes)
        hmdel(pIndices, node.entity);
    Array<Node> old = std::move(nodes);
    nodes = std::move(scratch);
    scratch = std::move(old);
    for (uint32_t i = 0; i < nodes.Size(); ++i)
    {
        const Index mapping = { .key = nodes[i].entity, .value = i };
        hmputs(pIndices, mapping);
    }
    dirtyRoots.Clear();
    dirtyRoots.Reserve(nodes.Size());
    historyDirty.Clear();
    for (uint32_t i = 0; i < nodes.Size(); i = nodes[i].subtreeEnd)
        dirtyRoots.PushBack(i);
    structureDirty = false;
}

void WorldHierarchy::update()
{
    world.checkThread();
    ASSERT(!ecs_is_deferred(world.pWorld));
    if (structureDirty)
        rebuild();
    if (dirtyRoots.Empty())
        return;
    qsort(dirtyRoots.GetData(), dirtyRoots.Size(), sizeof(uint32_t), compareIndices);
    uint32_t processedEnd = 0;
    for (uint32_t root : dirtyRoots)
    {
        if (root < processedEnd)
            continue;
        processedEnd = nodes[root].subtreeEnd;
        for (uint32_t i = root; i < processedEnd; ++i)
        {
            Node& node = nodes[i];
            node.transform.current =
                node.parent == UINT32_MAX ? localMatrix(node.entity) : nodes[node.parent].transform.current * localMatrix(node.entity);
            if (!node.initialized)
            {
                node.transform.previous = node.transform.current;
                node.initialized = true;
            }
            node.dirty = false;
            if (!node.historyDirty)
            {
                node.historyDirty = true;
                historyDirty.PushBack(i);
            }
        }
    }
    dirtyRoots.Clear();
}

const WorldTransform* WorldHierarchy::Get(ecs_entity_t entity) const
{
    world.checkThread();
    ASSERT(!structureDirty && dirtyRoots.Empty());
    const uint32_t index = find(entity);
    return index != UINT32_MAX ? &nodes[index].transform : nullptr;
}

void WorldHierarchy::commit()
{
    update();
    for (uint32_t index : historyDirty)
    {
        nodes[index].transform.previous = nodes[index].transform.current;
        nodes[index].historyDirty = false;
    }
    historyDirty.Clear();
}

void WorldHierarchy::markDirty(ecs_entity_t root)
{
    update();
    const uint32_t first = root ? find(root) : 0;
    if (first == UINT32_MAX)
        return;
    const uint32_t end = root ? nodes[first].subtreeEnd : nodes.Size();
    for (uint32_t i = first; i < end; ++i)
        nodes[i].transform.previous = nodes[i].transform.current;
}

bool WorldHierarchy::writeLocalMatrix(ecs_entity_t entity, const Matrix4& matrix)
{
    for (uint32_t column = 0; column < 4; ++column)
        for (uint32_t row = 0; row < 4; ++row)
            if (!isfinite((float)matrix.getElem(column, row)))
            {
                LOGF(Error, "Cannot preserve a non-finite world transform");
                return false;
            }
    LocalTransform transform;
    decompose(matrix, &transform.translation, &transform.rotation, &transform.scale);
    const Matrix4 reconstructed = Matrix4(transform.rotation, transform.translation) * Matrix4::scale(transform.scale);
    bool          representable = true;
    for (uint32_t column = 0; column < 4; ++column)
        for (uint32_t row = 0; row < 4; ++row)
        {
            const float expected = (float)matrix.getElem(column, row);
            const float actual = (float)reconstructed.getElem(column, row);
            if (!isfinite(actual) || fabsf(actual - expected) > 1e-5f * fmaxf(1.0f, fabsf(expected)))
                representable = false;
        }
    if (representable && transformType)
    {
        if (matrixType)
            ecs_remove_id(world.pWorld, entity, matrixType);
        ecs_set_id(world.pWorld, entity, transformType, sizeof(transform), &transform);
    }
    else if (matrixType)
    {
        if (transformType)
            ecs_remove_id(world.pWorld, entity, transformType);
        ecs_set_id(world.pWorld, entity, matrixType, sizeof(matrix), &matrix);
    }
    else
    {
        LOGF(Error, "Preserving world transforms requires registered local transform components");
        return false;
    }
    markTransformDirty(entity);
    return true;
}

bool WorldHierarchy::setParent(ecs_entity_t entity, ecs_entity_t parent, bool keepWorld)
{
    if (ecs_is_deferred(world.pWorld))
    {
        LOGF(Error, "Reparenting requires a committed World outside query/Defer scopes");
        return false;
    }
    for (ecs_entity_t ancestor = parent; ancestor; ancestor = ecs_get_target(world.pWorld, ancestor, world.parentRelation, 0))
        if (ancestor == entity)
            return false;
    const ecs_entity_t previous = ecs_get_target(world.pWorld, entity, world.parentRelation, 0);
    if (previous == parent)
        return true;
    if (keepWorld)
    {
        update();
        Matrix4 local = Get(entity)->current;
        if (parent)
        {
            const Matrix4& parentWorld = Get(parent)->current;
            const float    det = (float)determinant(parentWorld);
            if (!isfinite(det) || det == 0.0f)
            {
                LOGF(Error, "Cannot preserve world transform under a singular parent");
                return false;
            }
            local = inverse(parentWorld) * local;
            // Preserve the exact affine row after floating-point inversion.
            local.setRow(3, Vector4(0, 0, 0, 1));
        }
        if (!writeLocalMatrix(entity, local))
            return false;
    }
    if (parent)
        ecs_add_pair(world.pWorld, entity, world.parentRelation, parent);
    else if (previous)
        ecs_remove_pair(world.pWorld, entity, world.parentRelation, previous);
    invalidateStructure();
    return true;
}

bool WorldHierarchy::Destroy(ecs_entity_t entity, bool subtree)
{
    ecs_term_t term = { .id = ecs_pair(world.parentRelation, entity), .src = { .flags = EcsSelf } };
    ecs_iter_t children = ecs_term_iter(world.pWorld, &term);
    const bool hasChildren = ecs_term_next(&children);
    if (hasChildren)
        ecs_iter_fini(&children);
    if (!hasChildren)
    {
        ecs_delete(world.pWorld, entity);
        invalidateStructure();
        return true;
    }
    if (ecs_is_deferred(world.pWorld))
    {
        LOGF(Error, "Deleting a hierarchy requires a committed World outside query/Defer scopes");
        return false;
    }
    if (!subtree && !matrixType)
    {
        LOGF(Error, "Deleting a parent requires registered Matrix4 support");
        return false;
    }
    update();
    const uint32_t root = find(entity);
    const uint32_t end = nodes[root].subtreeEnd;
    if (subtree)
    {
        for (uint32_t i = end; i > root; --i)
            ecs_delete(world.pWorld, nodes[i - 1].entity);
    }
    else
    {
        // Each direct child's cached subtree range remains valid until the next update.
        for (uint32_t i = root + 1; i < end; i = nodes[i].subtreeEnd)
        {
            if (!writeLocalMatrix(nodes[i].entity, nodes[i].transform.current))
                return false;
            ecs_remove_pair(world.pWorld, nodes[i].entity, world.parentRelation, entity);
        }
        ecs_delete(world.pWorld, entity);
    }
    invalidateStructure();
    return true;
}

Entity World::GetParent(Entity entity) const
{
    return IsAlive(entity) ? Entity(ecs_get_target(pImpl->pWorld, entity.value, pImpl->parentRelation, 0), pImpl->id) : Entity{};
}
void World::AttachParent(Entity entity, Entity parent)
{
    ASSERT(IsAlive(entity) && IsAlive(parent) && !IsDeferred());
    ecs_add_pair(pImpl->pWorld, entity.value, pImpl->parentRelation, parent.value);
    pImpl->pHierarchy->invalidateStructure();
}
bool World::setParent(Entity entity, Entity parent, bool keepWorld)
{
    if (!IsAlive(entity) || (parent.IsValid() && !IsAlive(parent)))
        return false;
    return pImpl->pHierarchy->setParent(entity.value, parent.value, keepWorld);
}
bool                  World::DestroyEntity(Entity entity) { return IsAlive(entity) && pImpl->pHierarchy->Destroy(entity.value, false); }
bool                  World::DestroySubtree(Entity entity) { return IsAlive(entity) && pImpl->pHierarchy->Destroy(entity.value, true); }
void                  World::UpdateTransforms() { pImpl->pHierarchy->update(); }
const WorldTransform* World::GetWorldTransform(Entity entity) const
{
    return IsAlive(entity) ? pImpl->pHierarchy->Get(entity.value) : nullptr;
}
void World::CommitTransforms() { pImpl->pHierarchy->commit(); }
void World::resetTransformHistory(Entity root)
{
    if (!root.IsValid() || IsAlive(root))
        pImpl->pHierarchy->markDirty(root.value);
}
} // namespace hz
