#include <gtest/gtest.h>

#include "Scene/SceneComponents.h"
#include "Scene/World.h"
#include "Core/IContainer.h"

class SceneTransformTest: public ::testing::Test
{
protected:
    void SetUp() override { ASSERT_TRUE(InitMemoryAlloc(nullptr)); }
    void TearDown() override { ExitMemoryAlloc(); }
};

static void expectMatrix(const Matrix4& actual, const Matrix4& expected)
{
    for (uint32_t column = 0; column < 4; ++column)
        for (uint32_t row = 0; row < 4; ++row)
            EXPECT_NEAR((float)actual.getElem(column, row), (float)expected.getElem(column, row), 0.0001f);
}

static void setTranslation(hz::World& world, hz::Entity entity, float x, float y = 0.0f)
{
    const hz::LocalTransform transform = { .translation = Vector3(x, y, 0.0f) };
    ASSERT_TRUE(world.SetComponent(entity, hz::SceneTypes::localTransform, &transform));
}

TEST_F(SceneTransformTest, PropagatesLocalChangesAndRejectsCycles)
{
    hz::TypeRegistry types;
    ASSERT_TRUE(types.RegisterBuiltins());
    hz::World        world(types);
    const hz::Entity root = world.CreateEntity();
    const hz::Entity child = world.CreateEntity();
    const hz::Entity leaf = world.CreateEntity();
    setTranslation(world, root, 10.0f);
    setTranslation(world, child, 2.0f);
    setTranslation(world, leaf, 3.0f);
    ASSERT_TRUE(world.setParent(child, root, false));
    ASSERT_TRUE(world.setParent(leaf, child, false));
    {
        hz::DeferredChanges changes = world.Defer();
        EXPECT_FALSE(world.setParent(child, {}));
        EXPECT_FALSE(world.DestroyEntity(root));
        EXPECT_FALSE(world.DestroySubtree(root));
    }
    EXPECT_FALSE(world.setParent(root, leaf));
    EXPECT_FALSE(world.setParent(root, root));
    world.UpdateTransforms();
    expectMatrix(world.GetWorldTransform(leaf)->current, Matrix4::translation(Vector3(15.0f, 0.0f, 0.0f)));
    setTranslation(world, child, 5.0f);
    world.UpdateTransforms();
    expectMatrix(world.GetWorldTransform(leaf)->current, Matrix4::translation(Vector3(18.0f, 0.0f, 0.0f)));
    EXPECT_EQ(world.GetParent(leaf), child);
    EXPECT_FALSE(world.GetParent(root).IsValid());
}

TEST_F(SceneTransformTest, PreservesShearAndRejectsSingularParentsAtomically)
{
    hz::TypeRegistry types;
    ASSERT_TRUE(types.RegisterBuiltins());
    hz::World                world(types);
    const hz::Entity         parent = world.CreateEntity();
    const hz::Entity         child = world.CreateEntity();
    const hz::LocalTransform scaled = { .scale = Vector3(2.0f, 1.0f, 1.0f) };
    const hz::LocalTransform rotated = { .rotation = Quat::rotationZ(0.6f) };
    ASSERT_TRUE(world.SetComponent(parent, hz::SceneTypes::localTransform, &scaled));
    ASSERT_TRUE(world.SetComponent(child, hz::SceneTypes::localTransform, &rotated));
    world.UpdateTransforms();
    const Matrix4 before = world.GetWorldTransform(child)->current;
    ASSERT_TRUE(world.setParent(child, parent));
    EXPECT_NE(world.GetComponent(child, hz::SceneTypes::localMatrix), nullptr);
    EXPECT_EQ(world.GetComponent(child, hz::SceneTypes::localTransform), nullptr);
    world.UpdateTransforms();
    expectMatrix(world.GetWorldTransform(child)->current, before);

    const hz::Entity         singular = world.CreateEntity();
    const hz::LocalTransform zero = { .scale = Vector3(0.0f, 1.0f, 1.0f) };
    ASSERT_TRUE(world.SetComponent(singular, hz::SceneTypes::localTransform, &zero));
    EXPECT_FALSE(world.setParent(child, singular));
    EXPECT_EQ(world.GetParent(child), parent);
    world.UpdateTransforms();
    expectMatrix(world.GetWorldTransform(child)->current, before);
    ASSERT_TRUE(world.setParent(child, singular, false));
    EXPECT_EQ(world.GetParent(child), singular);
}

TEST_F(SceneTransformTest, ParentDeletionPreservesChildrenButSubtreeDeletionRemovesThem)
{
    hz::TypeRegistry types;
    ASSERT_TRUE(types.RegisterBuiltins());
    hz::World        world(types);
    const hz::Entity root = world.CreateEntity();
    const hz::Entity child = world.CreateEntity();
    const hz::Entity sibling = world.CreateEntity();
    const hz::Entity leaf = world.CreateEntity();
    setTranslation(world, root, 10.0f);
    setTranslation(world, child, 2.0f);
    setTranslation(world, leaf, 3.0f);
    ASSERT_TRUE(world.setParent(child, root, false));
    ASSERT_TRUE(world.setParent(sibling, root, false));
    ASSERT_TRUE(world.setParent(leaf, child, false));
    ASSERT_TRUE(world.DestroyEntity(root));
    EXPECT_FALSE(world.IsAlive(root));
    EXPECT_TRUE(world.IsAlive(child));
    EXPECT_TRUE(world.IsAlive(sibling));
    EXPECT_FALSE(world.GetParent(child).IsValid());
    EXPECT_EQ(world.GetParent(leaf), child);
    world.UpdateTransforms();
    expectMatrix(world.GetWorldTransform(leaf)->current, Matrix4::translation(Vector3(15.0f, 0.0f, 0.0f)));
    ASSERT_TRUE(world.DestroySubtree(child));
    EXPECT_FALSE(world.IsAlive(child));
    EXPECT_FALSE(world.IsAlive(leaf));
    EXPECT_TRUE(world.IsAlive(sibling));
    world.UpdateTransforms();
    EXPECT_EQ(world.GetWorldTransform(leaf), nullptr);
}

TEST_F(SceneTransformTest, DeferredLocalChangesRemainMutuallyExclusive)
{
    hz::TypeRegistry types;
    ASSERT_TRUE(types.RegisterBuiltins());
    hz::World             world(types);
    const hz::Entity      entity = world.CreateEntity();
    const Matrix4         matrix = Matrix4::translation(Vector3(4.0f, 0.0f, 0.0f));
    {
        hz::DeferredChanges changes = world.Defer();
        setTranslation(world, entity, 1.0f);
        EXPECT_FALSE(world.SetComponent(entity, hz::SceneTypes::localMatrix, &matrix));
        ASSERT_TRUE(world.RemoveComponent(entity, hz::SceneTypes::localTransform));
        ASSERT_TRUE(world.SetComponent(entity, hz::SceneTypes::localMatrix, &matrix));
        EXPECT_FALSE(world.AddComponent(entity, hz::SceneTypes::localTransform));
    }
    EXPECT_EQ(world.GetComponent(entity, hz::SceneTypes::localTransform), nullptr);
    world.UpdateTransforms();
    expectMatrix(world.GetWorldTransform(entity)->current, matrix);
    EXPECT_FALSE(world.AddComponent(entity, hz::SceneTypes::localTransform));
    ASSERT_TRUE(world.RemoveComponent(entity, hz::SceneTypes::localMatrix));
    world.UpdateTransforms();
    expectMatrix(world.GetWorldTransform(entity)->current, Matrix4::identity());
}

TEST_F(SceneTransformTest, DeepHierarchyUsesIterativeTraversalAndCleanUpdatesDoNotAllocate)
{
    hz::TypeRegistry types;
    ASSERT_TRUE(types.RegisterBuiltins());
    hz::World             world(types);
    hz::Entity            parent;
    hz::Entity            root;
    hz::Array<hz::Entity> entities;
    for (uint32_t i = 0; i < 2048; ++i)
    {
        const hz::Entity entity = world.CreateEntity();
        entities.PushBack(entity);
        setTranslation(world, entity, 1.0f);
        ASSERT_TRUE(world.setParent(entity, parent, false));
        if (!root.IsValid())
            root = entity;
        parent = entity;
    }
    world.UpdateTransforms();
    expectMatrix(world.GetWorldTransform(parent)->current, Matrix4::translation(Vector3(2048.0f, 0.0f, 0.0f)));
    const MemoryTrackingStats before = GetMemoryTrackingStats();
    for (hz::Entity entity : entities)
        setTranslation(world, entity, 1.0f);
    for (uint32_t i = 0; i < 16; ++i)
    {
        setTranslation(world, root, 2.0f);
        world.UpdateTransforms();
        world.UpdateTransforms();
    }
    const MemoryTrackingStats after = GetMemoryTrackingStats();
    if (before.TrackingEnabled)
    {
        EXPECT_EQ(before.TotalAllocationCount, after.TotalAllocationCount);
        EXPECT_EQ(before.ReallocationCount, after.ReallocationCount);
    }
    expectMatrix(world.GetWorldTransform(parent)->current, Matrix4::translation(Vector3(2049.0f, 0.0f, 0.0f)));
    ASSERT_TRUE(world.DestroySubtree(root));
    EXPECT_FALSE(world.IsAlive(parent));
}

TEST_F(SceneTransformTest, DeepSubtreeSupportsReparentingComponentEditsAndDeletion)
{
    hz::TypeRegistry types;
    ASSERT_TRUE(types.RegisterBuiltins());
    hz::World             world(types);
    const uint32_t        Count = 10000;
    hz::Array<hz::Entity> entities(Count);
    for (uint32_t i = 0; i < Count; ++i)
    {
        entities[i] = world.CreateEntity();
        setTranslation(world, entities[i], 1.0f);
    }
    for (uint32_t i = 0; i + 1 < Count; ++i)
        ASSERT_TRUE(world.setParent(entities[i], entities[i + 1], false));
    const hz::Entity root = entities[Count - 1];
    const hz::Entity leaf = entities[0];
    world.UpdateTransforms();
    const hz::Entity firstParent = world.CreateEntity();
    const hz::Entity secondParent = world.CreateEntity();
    setTranslation(world, firstParent, 12.0f);
    setTranslation(world, secondParent, 5.0f);
    ASSERT_TRUE(world.setParent(root, firstParent, false));
    world.UpdateTransforms();
    expectMatrix(world.GetWorldTransform(leaf)->current, Matrix4::translation(Vector3((float)Count + 12.0f, 0, 0)));
    ASSERT_TRUE(world.setParent(root, secondParent));
    world.UpdateTransforms();
    expectMatrix(world.GetWorldTransform(leaf)->current, Matrix4::translation(Vector3((float)Count + 12.0f, 0, 0)));
    EXPECT_FALSE(world.setParent(secondParent, leaf));
    hz::MeshRenderer renderer;
    renderer.overrides.PushBack({ .submesh = 1, .material = { .low = 42 } });
    ASSERT_TRUE(world.SetComponent(root, hz::SceneTypes::meshRenderer, &renderer));
    ASSERT_TRUE(world.AddComponent(root, hz::SceneTypes::name));
    const char name[] = "DeepRoot";
    ASSERT_TRUE(
        world.SetProperty(root, hz::SceneTypes::name, 1, { .kind = hz::PropertyKind::String, .Data = name, .Size = sizeof(name) }));
    const Vector3 translation(4, 0, 0);
    ASSERT_TRUE(world.SetProperty(root, hz::SceneTypes::localTransform, 1,
                                  { .kind = hz::PropertyKind::Vector3, .Data = &translation, .Size = sizeof(translation) }));
    world.UpdateTransforms();
    expectMatrix(world.GetWorldTransform(leaf)->current, Matrix4::translation(Vector3((float)Count + 8.0f, 0, 0)));
    {
        hz::DeferredChanges changes = world.Defer();
        ASSERT_TRUE(world.RemoveComponent(root, hz::SceneTypes::name));
        ASSERT_TRUE(world.RemoveComponent(root, hz::SceneTypes::meshRenderer));
        ASSERT_TRUE(world.RemoveComponent(root, hz::SceneTypes::localTransform));
        const Matrix4 matrix = Matrix4::translation(Vector3(3, 0, 0));
        ASSERT_TRUE(world.SetComponent(root, hz::SceneTypes::localMatrix, &matrix));
        EXPECT_FALSE(world.DestroyEntity(root));
    }
    world.UpdateTransforms();
    expectMatrix(world.GetWorldTransform(leaf)->current, Matrix4::translation(Vector3((float)Count + 7.0f, 0, 0)));
    ASSERT_TRUE(world.DestroyEntity(secondParent));
    EXPECT_FALSE(world.GetParent(root).IsValid());
    world.UpdateTransforms();
    expectMatrix(world.GetWorldTransform(leaf)->current, Matrix4::translation(Vector3((float)Count + 7.0f, 0, 0)));
    ASSERT_TRUE(world.DestroySubtree(root));
    EXPECT_FALSE(world.IsAlive(root));
    EXPECT_FALSE(world.IsAlive(leaf));
    EXPECT_TRUE(world.IsAlive(firstParent));
    {
        hz::DeferredChanges changes = world.Defer();
        EXPECT_TRUE(world.DestroyEntity(firstParent));
    }
    EXPECT_FALSE(world.IsAlive(firstParent));
}

TEST_F(SceneTransformTest, SpawningAndDestroyingLeavesKeepsHierarchyIncremental)
{
    hz::TypeRegistry types;
    ASSERT_TRUE(types.RegisterBuiltins());
    hz::World        world(types);
    const hz::Entity root = world.CreateEntity();
    const hz::Entity child = world.CreateEntity();
    const hz::Entity leaf = world.CreateEntity();
    setTranslation(world, root, 10.0f);
    setTranslation(world, child, 2.0f);
    setTranslation(world, leaf, 3.0f);
    ASSERT_TRUE(world.setParent(child, root, false));
    ASSERT_TRUE(world.setParent(leaf, child, false));
    world.UpdateTransforms();

    // Destroying a leaf inside the hierarchy leaves the rest of the order intact.
    const hz::Entity sibling = world.CreateEntity();
    setTranslation(world, sibling, 1.0f);
    ASSERT_TRUE(world.setParent(sibling, child, false));
    world.UpdateTransforms();
    ASSERT_TRUE(world.DestroyEntity(leaf));
    setTranslation(world, child, 5.0f);
    world.UpdateTransforms();
    EXPECT_EQ(world.GetWorldTransform(leaf), nullptr);
    expectMatrix(world.GetWorldTransform(sibling)->current, Matrix4::translation(Vector3(16.0f, 0.0f, 0.0f)));

    // One spawn and one despawn per frame: a long run must not grow memory or lose transforms.
    hz::Array<hz::Entity> spawned;
    for (uint32_t frame = 0; frame < 64; ++frame)
    {
        const hz::Entity entity = world.CreateEntity();
        setTranslation(world, entity, (float)frame);
        spawned.PushBack(entity);
        if (spawned.Size() > 8)
        {
            ASSERT_TRUE(world.DestroyEntity(spawned[0]));
            for (uint32_t i = 1; i < spawned.Size(); ++i)
                spawned[i - 1] = spawned[i];
            spawned.PopBack();
        }
        world.UpdateTransforms();
        expectMatrix(world.GetWorldTransform(entity)->current, Matrix4::translation(Vector3((float)frame, 0.0f, 0.0f)));
        expectMatrix(world.GetWorldTransform(sibling)->current, Matrix4::translation(Vector3(16.0f, 0.0f, 0.0f)));
    }
    // A parent deleted after leaves were tombstoned still preserves its remaining children.
    ASSERT_TRUE(world.DestroyEntity(child));
    world.UpdateTransforms();
    EXPECT_FALSE(world.GetParent(sibling).IsValid());
    expectMatrix(world.GetWorldTransform(sibling)->current, Matrix4::translation(Vector3(16.0f, 0.0f, 0.0f)));
    for (hz::Entity entity : spawned)
        EXPECT_NE(world.GetWorldTransform(entity), nullptr);
}

TEST_F(SceneTransformTest, BuildsOneHundredThousandEntitiesAndUpdatesASingleDirtyRoot)
{
    hz::TypeRegistry types;
    ASSERT_TRUE(types.RegisterBuiltins());
    hz::World  world(types);
    hz::Entity first;
    hz::Entity last;
    for (uint32_t i = 0; i < 100000; ++i)
    {
        last = world.CreateEntity();
        setTranslation(world, last, (float)i);
        if (!first.IsValid())
            first = last;
    }
    world.UpdateTransforms();
    setTranslation(world, first, 12.0f);
    world.UpdateTransforms();
    expectMatrix(world.GetWorldTransform(first)->current, Matrix4::translation(Vector3(12.0f, 0.0f, 0.0f)));
    expectMatrix(world.GetWorldTransform(last)->current, Matrix4::translation(Vector3(99999.0f, 0.0f, 0.0f)));
}
