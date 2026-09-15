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
    ecs_query_fini(pEntities);
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

void WorldHierarchy::markDirty(ecs_entity_t entity)
{
    if (structureDirty)
        return;
    const uint32_t index = find(entity);
    if (index != UINT32_MAX && !nodes[index].dirty)
    {
        nodes[index].dirty = true;
        dirtyRoots.pushBack(index);
    }
}

bool WorldHierarchy::prepareLocalChange(ecs_entity_t entity, TypeID type, bool adding)
{
    if (type != SceneTypes::localTransform && type != SceneTypes::localMatrix)
        return true;
    const uint32_t    bit = type == SceneTypes::localTransform ? 1u : 2u;
    const LocalState* pPending = hmgetp_null(pPendingLocals, entity);
    const uint32_t    mask = pPending ? pPending->value
                                      : ((transformType && ecs_has_id(world.pWorld, entity, transformType) ? 1u : 0u) |
                                      (matrixType && ecs_has_id(world.pWorld, entity, matrixType) ? 2u : 0u));
    if (adding && (mask & (bit ^ 3u)))
    {
        LOGF(eERROR, "An entity cannot have both LocalTransform and LocalMatrix");
        return false;
    }
    const uint32_t next = adding ? mask | bit : mask & ~bit;
    if (ecs_is_deferred(world.pWorld) && next != mask)
    {
        if (!pPending)
            pendingLocals.pushBack(entity);
        const LocalState state = { .key = entity, .value = next };
        hmputs(pPendingLocals, state);
    }
    markDirty(entity);
    return true;
}

void WorldHierarchy::finishDeferredChanges()
{
    for (ecs_entity_t entity : pendingLocals)
        hmdel(pPendingLocals, entity);
    pendingLocals.clear();
}

Matrix4 WorldHierarchy::localMatrix(ecs_entity_t entity) const
{
    const LocalMatrix* pMatrix = matrixType ? (const LocalMatrix*)ecs_get_id(world.pWorld, entity, matrixType) : nullptr;
    if (pMatrix)
        return pMatrix->value;
    const LocalTransform* pTransform = transformType ? (const LocalTransform*)ecs_get_id(world.pWorld, entity, transformType) : nullptr;
    return pTransform ? Matrix4(pTransform->rotation, pTransform->translation) * Matrix4::scale(pTransform->scale) : Matrix4::identity();
}

void WorldHierarchy::rebuild()
{
    buildNodes.clear();
    for (const Node& node : nodes)
        hmdel(pBuildIndices, node.entity);
    ecs_iter_t it = ecs_query_iter(world.pWorld, pEntities);
    while (ecs_query_next(&it))
    {
        for (int32_t row = 0; row < it.count; ++row)
        {
            const Index mapping = { .key = it.entities[row], .value = buildNodes.size() };
            hmputs(pBuildIndices, mapping);
            buildNodes.pushBack({ .entity = it.entities[row] });
        }
    }
    uint32_t firstRoot = UINT32_MAX;
    for (uint32_t i = 0; i < buildNodes.size(); ++i)
    {
        BuildNode&         node = buildNodes[i];
        const ecs_entity_t parent = ecs_get_target(world.pWorld, node.entity, EcsChildOf, 0);
        const Index*       pParent = hmgetp_null(pBuildIndices, parent);
        node.parent = pParent ? pParent->value : UINT32_MAX;
        uint32_t& first = pParent ? buildNodes[node.parent].firstChild : firstRoot;
        node.nextSibling = first;
        first = i;
    }

    scratch.clear();
    scratch.reserve(buildNodes.size());
    stack.clear();
    uint32_t current = firstRoot;
    while (current != UINT32_MAX)
    {
        BuildNode& entry = buildNodes[current];
        entry.output = scratch.size();
        const uint32_t old = find(entry.entity);
        Node           node = old != UINT32_MAX ? nodes[old] : Node{};
        node.entity = entry.entity;
        node.parent = entry.parent == UINT32_MAX ? UINT32_MAX : buildNodes[entry.parent].output;
        node.dirty = true;
        node.historyDirty = false;
        scratch.pushBack(node);
        stack.pushBack(current);
        if (entry.firstChild != UINT32_MAX)
        {
            current = entry.firstChild;
            continue;
        }
        current = UINT32_MAX;
        while (!stack.empty())
        {
            const BuildNode& finished = buildNodes[stack[stack.size() - 1]];
            scratch[finished.output].subtreeEnd = scratch.size();
            stack.popBack();
            if (finished.nextSibling != UINT32_MAX)
            {
                current = finished.nextSibling;
                break;
            }
        }
    }
    ASSERT(scratch.size() == buildNodes.size());
    for (const Node& node : nodes)
        hmdel(pIndices, node.entity);
    Array<Node> old = std::move(nodes);
    nodes = std::move(scratch);
    scratch = std::move(old);
    for (uint32_t i = 0; i < nodes.size(); ++i)
    {
        const Index mapping = { .key = nodes[i].entity, .value = i };
        hmputs(pIndices, mapping);
    }
    dirtyRoots.clear();
    dirtyRoots.reserve(nodes.size());
    historyDirty.clear();
    for (uint32_t i = 0; i < nodes.size(); i = nodes[i].subtreeEnd)
        dirtyRoots.pushBack(i);
    structureDirty = false;
}

void WorldHierarchy::update()
{
    world.checkThread();
    ASSERT(!ecs_is_deferred(world.pWorld));
    if (structureDirty)
        rebuild();
    if (dirtyRoots.empty())
        return;
    qsort(dirtyRoots.data(), dirtyRoots.size(), sizeof(uint32_t), compareIndices);
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
                historyDirty.pushBack(i);
            }
        }
    }
    dirtyRoots.clear();
}

const WorldTransform* WorldHierarchy::get(ecs_entity_t entity) const
{
    world.checkThread();
    ASSERT(!structureDirty && dirtyRoots.empty());
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
    historyDirty.clear();
}

void WorldHierarchy::resetHistory(ecs_entity_t root)
{
    update();
    const uint32_t first = root ? find(root) : 0;
    if (first == UINT32_MAX)
        return;
    const uint32_t end = root ? nodes[first].subtreeEnd : nodes.size();
    for (uint32_t i = first; i < end; ++i)
        nodes[i].transform.previous = nodes[i].transform.current;
}

bool WorldHierarchy::writeLocalMatrix(ecs_entity_t entity, const Matrix4& matrix)
{
    for (uint32_t column = 0; column < 4; ++column)
        for (uint32_t row = 0; row < 4; ++row)
            if (!isfinite((float)matrix.getElem(column, row)))
            {
                LOGF(eERROR, "Cannot preserve a non-finite world transform");
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
        const LocalMatrix local = { .value = matrix };
        ecs_set_id(world.pWorld, entity, matrixType, sizeof(local), &local);
    }
    else
    {
        LOGF(eERROR, "Preserving world transforms requires registered local transform components");
        return false;
    }
    markDirty(entity);
    return true;
}

bool WorldHierarchy::setParent(ecs_entity_t entity, ecs_entity_t parent, bool keepWorld)
{
    if (ecs_is_deferred(world.pWorld))
    {
        LOGF(eERROR, "Reparenting requires a committed World outside query/defer scopes");
        return false;
    }
    for (ecs_entity_t ancestor = parent; ancestor; ancestor = ecs_get_target(world.pWorld, ancestor, EcsChildOf, 0))
        if (ancestor == entity)
            return false;
    const ecs_entity_t previous = ecs_get_target(world.pWorld, entity, EcsChildOf, 0);
    if (previous == parent)
        return true;
    if (keepWorld)
    {
        update();
        Matrix4 local = get(entity)->current;
        if (parent)
        {
            const Matrix4& parentWorld = get(parent)->current;
            const float    det = (float)determinant(parentWorld);
            if (!isfinite(det) || det == 0.0f)
            {
                LOGF(eERROR, "Cannot preserve world transform under a singular parent");
                return false;
            }
            local = inverse(parentWorld) * local;
        }
        if (!writeLocalMatrix(entity, local))
            return false;
    }
    if (parent)
        ecs_add_pair(world.pWorld, entity, EcsChildOf, parent);
    else if (previous)
        ecs_remove_pair(world.pWorld, entity, EcsChildOf, previous);
    invalidateStructure();
    return true;
}

bool WorldHierarchy::destroy(ecs_entity_t entity, bool subtree)
{
    ecs_iter_t children = ecs_children(world.pWorld, entity);
    const bool hasChildren = ecs_children_next(&children);
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
        LOGF(eERROR, "Deleting a hierarchy requires a committed World outside query/defer scopes");
        return false;
    }
    if (!subtree && !matrixType)
    {
        LOGF(eERROR, "Deleting a parent requires registered LocalMatrix support");
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
            ecs_remove_pair(world.pWorld, nodes[i].entity, EcsChildOf, entity);
        }
        ecs_delete(world.pWorld, entity);
    }
    invalidateStructure();
    return true;
}

Entity World::getParent(Entity entity) const
{
    return isAlive(entity) ? Entity(ecs_get_target(pImpl->pWorld, entity.value, EcsChildOf, 0)) : Entity{};
}
bool World::setParent(Entity entity, Entity parent, bool keepWorld)
{
    if (!isAlive(entity) || (parent.isValid() && !isAlive(parent)))
        return false;
    return pImpl->pHierarchy->setParent(entity.value, parent.value, keepWorld);
}
bool                  World::destroyEntity(Entity entity) { return isAlive(entity) && pImpl->pHierarchy->destroy(entity.value, false); }
bool                  World::destroySubtree(Entity entity) { return isAlive(entity) && pImpl->pHierarchy->destroy(entity.value, true); }
void                  World::updateTransforms() { pImpl->pHierarchy->update(); }
const WorldTransform* World::getWorldTransform(Entity entity) const
{
    return isAlive(entity) ? pImpl->pHierarchy->get(entity.value) : nullptr;
}
void World::commitTransforms() { pImpl->pHierarchy->commit(); }
void World::resetTransformHistory(Entity root)
{
    if (!root.isValid() || isAlive(root))
        pImpl->pHierarchy->resetHistory(root.value);
}
} // namespace hz
