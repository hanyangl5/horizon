#include <gtest/gtest.h>

#include "Scene/SceneComponents.h"
#include "Scene/World.h"

class SceneTransformTest: public ::testing::Test
{
protected:
    void SetUp() override { ASSERT_TRUE(initMemAlloc(nullptr)); }
    void TearDown() override { exitMemAlloc(); }
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
    ASSERT_TRUE(world.setComponent(entity, hz::SceneTypes::localTransform, &transform));
}

TEST_F(SceneTransformTest, PropagatesLocalChangesAndRejectsCycles)
{
    hz::TypeRegistry types;
    ASSERT_TRUE(types.registerBuiltins());
    hz::World        world(types);
    const hz::Entity root = world.createEntity();
    const hz::Entity child = world.createEntity();
    const hz::Entity leaf = world.createEntity();
    setTranslation(world, root, 10.0f);
    setTranslation(world, child, 2.0f);
    setTranslation(world, leaf, 3.0f);
    ASSERT_TRUE(world.setParent(child, root, false));
    ASSERT_TRUE(world.setParent(leaf, child, false));
    {
        hz::DeferredChanges changes = world.defer();
        EXPECT_FALSE(world.setParent(child, {}));
        EXPECT_FALSE(world.destroyEntity(root));
        EXPECT_FALSE(world.destroySubtree(root));
    }
    EXPECT_FALSE(world.setParent(root, leaf));
    EXPECT_FALSE(world.setParent(root, root));
    world.updateTransforms();
    expectMatrix(world.getWorldTransform(leaf)->current, Matrix4::translation(Vector3(15.0f, 0.0f, 0.0f)));
    setTranslation(world, child, 5.0f);
    world.updateTransforms();
    expectMatrix(world.getWorldTransform(leaf)->current, Matrix4::translation(Vector3(18.0f, 0.0f, 0.0f)));
    EXPECT_EQ(world.getParent(leaf), child);
    EXPECT_FALSE(world.getParent(root).isValid());
}

TEST_F(SceneTransformTest, PreservesShearAndRejectsSingularParentsAtomically)
{
    hz::TypeRegistry types;
    ASSERT_TRUE(types.registerBuiltins());
    hz::World                world(types);
    const hz::Entity         parent = world.createEntity();
    const hz::Entity         child = world.createEntity();
    const hz::LocalTransform scaled = { .scale = Vector3(2.0f, 1.0f, 1.0f) };
    const hz::LocalTransform rotated = { .rotation = Quat::rotationZ(0.6f) };
    ASSERT_TRUE(world.setComponent(parent, hz::SceneTypes::localTransform, &scaled));
    ASSERT_TRUE(world.setComponent(child, hz::SceneTypes::localTransform, &rotated));
    world.updateTransforms();
    const Matrix4 before = world.getWorldTransform(child)->current;
    ASSERT_TRUE(world.setParent(child, parent));
    EXPECT_NE(world.getComponent(child, hz::SceneTypes::localMatrix), nullptr);
    EXPECT_EQ(world.getComponent(child, hz::SceneTypes::localTransform), nullptr);
    world.updateTransforms();
    expectMatrix(world.getWorldTransform(child)->current, before);

    const hz::Entity         singular = world.createEntity();
    const hz::LocalTransform zero = { .scale = Vector3(0.0f, 1.0f, 1.0f) };
    ASSERT_TRUE(world.setComponent(singular, hz::SceneTypes::localTransform, &zero));
    EXPECT_FALSE(world.setParent(child, singular));
    EXPECT_EQ(world.getParent(child), parent);
    world.updateTransforms();
    expectMatrix(world.getWorldTransform(child)->current, before);
    ASSERT_TRUE(world.setParent(child, singular, false));
    EXPECT_EQ(world.getParent(child), singular);
}

TEST_F(SceneTransformTest, ParentDeletionPreservesChildrenButSubtreeDeletionRemovesThem)
{
    hz::TypeRegistry types;
    ASSERT_TRUE(types.registerBuiltins());
    hz::World        world(types);
    const hz::Entity root = world.createEntity();
    const hz::Entity child = world.createEntity();
    const hz::Entity sibling = world.createEntity();
    const hz::Entity leaf = world.createEntity();
    setTranslation(world, root, 10.0f);
    setTranslation(world, child, 2.0f);
    setTranslation(world, leaf, 3.0f);
    ASSERT_TRUE(world.setParent(child, root, false));
    ASSERT_TRUE(world.setParent(sibling, root, false));
    ASSERT_TRUE(world.setParent(leaf, child, false));
    ASSERT_TRUE(world.destroyEntity(root));
    EXPECT_FALSE(world.isAlive(root));
    EXPECT_TRUE(world.isAlive(child));
    EXPECT_TRUE(world.isAlive(sibling));
    EXPECT_FALSE(world.getParent(child).isValid());
    EXPECT_EQ(world.getParent(leaf), child);
    world.updateTransforms();
    expectMatrix(world.getWorldTransform(leaf)->current, Matrix4::translation(Vector3(15.0f, 0.0f, 0.0f)));
    ASSERT_TRUE(world.destroySubtree(child));
    EXPECT_FALSE(world.isAlive(child));
    EXPECT_FALSE(world.isAlive(leaf));
    EXPECT_TRUE(world.isAlive(sibling));
    world.updateTransforms();
    EXPECT_EQ(world.getWorldTransform(leaf), nullptr);
}

TEST_F(SceneTransformTest, HistoryAdvancesOnlyOnCommitAndCanResetASubtree)
{
    hz::TypeRegistry types;
    ASSERT_TRUE(types.registerBuiltins());
    hz::World        world(types);
    const hz::Entity entity = world.createEntity();
    const hz::Entity child = world.createEntity();
    const hz::Entity other = world.createEntity();
    ASSERT_TRUE(world.setParent(child, entity, false));
    setTranslation(world, entity, 1.0f);
    world.updateTransforms();
    expectMatrix(world.getWorldTransform(entity)->previous, world.getWorldTransform(entity)->current);
    setTranslation(world, entity, 2.0f);
    world.updateTransforms();
    setTranslation(world, entity, 3.0f);
    world.updateTransforms();
    expectMatrix(world.getWorldTransform(entity)->previous, Matrix4::translation(Vector3(1.0f, 0.0f, 0.0f)));
    // Rebuilding the hierarchy must preserve uncommitted history on existing entities.
    world.createEntity();
    world.updateTransforms();
    expectMatrix(world.getWorldTransform(entity)->previous, Matrix4::translation(Vector3(1.0f, 0.0f, 0.0f)));
    world.commitTransforms();
    expectMatrix(world.getWorldTransform(entity)->previous, world.getWorldTransform(entity)->current);
    setTranslation(world, entity, 4.0f);
    setTranslation(world, other, 7.0f);
    world.updateTransforms();
    world.resetTransformHistory(entity);
    expectMatrix(world.getWorldTransform(entity)->previous, world.getWorldTransform(entity)->current);
    expectMatrix(world.getWorldTransform(child)->previous, world.getWorldTransform(child)->current);
    expectMatrix(world.getWorldTransform(other)->previous, Matrix4::identity());
}

TEST_F(SceneTransformTest, DeferredLocalChangesRemainMutuallyExclusive)
{
    hz::TypeRegistry types;
    ASSERT_TRUE(types.registerBuiltins());
    hz::World             world(types);
    const hz::Entity      entity = world.createEntity();
    const hz::LocalMatrix matrix = { .value = Matrix4::translation(Vector3(4.0f, 0.0f, 0.0f)) };
    {
        hz::DeferredChanges changes = world.defer();
        setTranslation(world, entity, 1.0f);
        EXPECT_FALSE(world.setComponent(entity, hz::SceneTypes::localMatrix, &matrix));
        ASSERT_TRUE(world.removeComponent(entity, hz::SceneTypes::localTransform));
        ASSERT_TRUE(world.setComponent(entity, hz::SceneTypes::localMatrix, &matrix));
        EXPECT_FALSE(world.addComponent(entity, hz::SceneTypes::localTransform));
    }
    EXPECT_EQ(world.getComponent(entity, hz::SceneTypes::localTransform), nullptr);
    world.updateTransforms();
    expectMatrix(world.getWorldTransform(entity)->current, matrix.value);
    EXPECT_FALSE(world.addComponent(entity, hz::SceneTypes::localTransform));
    ASSERT_TRUE(world.removeComponent(entity, hz::SceneTypes::localMatrix));
    world.updateTransforms();
    expectMatrix(world.getWorldTransform(entity)->current, Matrix4::identity());
}

TEST_F(SceneTransformTest, DeepHierarchyUsesIterativeTraversalAndCleanUpdatesDoNotAllocate)
{
    hz::TypeRegistry types;
    ASSERT_TRUE(types.registerBuiltins());
    hz::World             world(types);
    hz::Entity            parent;
    hz::Entity            root;
    hz::Array<hz::Entity> entities;
    for (uint32_t i = 0; i < 2048; ++i)
    {
        const hz::Entity entity = world.createEntity();
        entities.pushBack(entity);
        setTranslation(world, entity, 1.0f);
        ASSERT_TRUE(world.setParent(entity, parent, false));
        if (!root.isValid())
            root = entity;
        parent = entity;
    }
    world.updateTransforms();
    expectMatrix(world.getWorldTransform(parent)->current, Matrix4::translation(Vector3(2048.0f, 0.0f, 0.0f)));
    world.commitTransforms();
    const MemoryTrackingStats before = memGetTrackingStats();
    for (hz::Entity entity : entities)
        setTranslation(world, entity, 1.0f);
    for (uint32_t i = 0; i < 16; ++i)
    {
        setTranslation(world, root, 2.0f);
        world.updateTransforms();
        world.commitTransforms();
        world.updateTransforms();
    }
    const MemoryTrackingStats after = memGetTrackingStats();
    if (before.trackingEnabled)
    {
        EXPECT_EQ(before.totalAllocationCount, after.totalAllocationCount);
        EXPECT_EQ(before.reallocationCount, after.reallocationCount);
    }
    expectMatrix(world.getWorldTransform(parent)->current, Matrix4::translation(Vector3(2049.0f, 0.0f, 0.0f)));
    ASSERT_TRUE(world.destroySubtree(root));
    EXPECT_FALSE(world.isAlive(parent));
}

TEST_F(SceneTransformTest, BuildsOneHundredThousandEntitiesAndUpdatesASingleDirtyRoot)
{
    hz::TypeRegistry types;
    ASSERT_TRUE(types.registerBuiltins());
    hz::World  world(types);
    hz::Entity first;
    hz::Entity last;
    for (uint32_t i = 0; i < 100000; ++i)
    {
        last = world.createEntity();
        setTranslation(world, last, (float)i);
        if (!first.isValid())
            first = last;
    }
    world.updateTransforms();
    world.commitTransforms();
    setTranslation(world, first, 12.0f);
    world.updateTransforms();
    expectMatrix(world.getWorldTransform(first)->current, Matrix4::translation(Vector3(12.0f, 0.0f, 0.0f)));
    expectMatrix(world.getWorldTransform(first)->previous, Matrix4::identity());
    expectMatrix(world.getWorldTransform(last)->current, Matrix4::translation(Vector3(99999.0f, 0.0f, 0.0f)));
}
