#include <gtest/gtest.h>

#include "Scene/SceneComponents.h"
#include "Scene/World.h"

static_assert(sizeof(hz::Entity) == sizeof(uint64_t));

class SceneWorldTest: public ::testing::Test
{
protected:
    void SetUp() override
    {
        ASSERT_TRUE(initMemAlloc(nullptr));
        initialStats = memGetTrackingStats();
    }
    void TearDown() override
    {
        const MemoryTrackingStats stats = memGetTrackingStats();
        if (stats.trackingEnabled)
        {
            EXPECT_EQ(stats.liveAllocationCount, initialStats.liveAllocationCount);
            EXPECT_EQ(stats.liveRequestedBytes, initialStats.liveRequestedBytes);
        }
        exitMemAlloc();
    }
    MemoryTrackingStats initialStats = {};
};

TEST_F(SceneWorldTest, EntityGenerationRejectsDeletedHandles)
{
    hz::TypeRegistry types;
    ASSERT_TRUE(types.registerBuiltins());
    hz::World world(types);
    EXPECT_FALSE(world.isAlive({}));
    const hz::Entity first = world.createEntity();
    ASSERT_TRUE(first.isValid());
    EXPECT_TRUE(world.isAlive(first));
    EXPECT_TRUE(world.destroyEntity(first));
    EXPECT_FALSE(world.isAlive(first));
    const hz::Entity second = world.createEntity();
    ASSERT_TRUE(world.isAlive(second));
    EXPECT_NE(first, second);
    EXPECT_FALSE(world.destroyEntity(first));
    EXPECT_FALSE(world.addComponent(first, hz::SceneTypes::camera));
    EXPECT_FALSE(world.removeComponent(first, hz::SceneTypes::camera));
    EXPECT_EQ(world.getComponent(first, hz::SceneTypes::camera), nullptr);
    EXPECT_TRUE(world.isAlive(second));
}

TEST_F(SceneWorldTest, ComponentsConstructDefaultsAndCopyValues)
{
    hz::TypeRegistry types;
    ASSERT_TRUE(types.registerBuiltins());
    hz::World        world(types);
    const hz::Entity entity = world.createEntity();
    ASSERT_TRUE(world.addComponent(entity, hz::SceneTypes::camera));
    const hz::Camera* camera = (const hz::Camera*)world.getComponent(entity, hz::SceneTypes::camera);
    ASSERT_NE(camera, nullptr);
    EXPECT_FLOAT_EQ(camera->nearPlane, 0.1f);
    EXPECT_FLOAT_EQ(camera->farPlane, 1000.0f);
    const hz::Camera replacement = { .verticalFov = 0.9f, .nearPlane = 0.2f, .farPlane = 500.0f };
    ASSERT_TRUE(world.setComponent(entity, hz::SceneTypes::camera, &replacement));
    camera = (const hz::Camera*)world.getComponent(entity, hz::SceneTypes::camera);
    EXPECT_NE(camera, &replacement);
    EXPECT_FLOAT_EQ(camera->farPlane, 500.0f);
    ASSERT_TRUE(world.addComponent(entity, hz::SceneTypes::camera));
    EXPECT_FLOAT_EQ(((const hz::Camera*)world.getComponent(entity, hz::SceneTypes::camera))->farPlane, 500.0f);
    EXPECT_FALSE(world.addComponent(entity, 999));
    EXPECT_FALSE(world.setComponent(entity, 999, &replacement));
    EXPECT_EQ(world.getComponent(entity, 999), nullptr);
    EXPECT_TRUE(world.removeComponent(entity, hz::SceneTypes::camera));
    EXPECT_EQ(world.getComponent(entity, hz::SceneTypes::camera), nullptr);
}

TEST_F(SceneWorldTest, OwningComponentsSurviveTableMovesAndRowRemoval)
{
    hz::TypeRegistry types;
    ASSERT_TRUE(types.registerBuiltins());
    hz::World  world(types);
    hz::Entity entities[64];
    for (uint32_t i = 0; i < 64; ++i)
    {
        entities[i] = world.createEntity();
        hz::MeshRenderer mesh;
        mesh.mesh.low = i + 1;
        mesh.overrides.pushBack({ .submesh = i + 1, .material = { .low = i + 100 } });
        ASSERT_TRUE(world.setComponent(entities[i], hz::SceneTypes::meshRenderer, &mesh));
        ASSERT_TRUE(world.addComponent(entities[i], hz::SceneTypes::name));
        ASSERT_TRUE(world.addComponent(entities[i], hz::SceneTypes::localTransform));
        ASSERT_TRUE(world.removeComponent(entities[i], hz::SceneTypes::name));
    }
    for (uint32_t i = 0; i < 64; i += 2)
        ASSERT_TRUE(world.destroyEntity(entities[i]));
    for (uint32_t i = 1; i < 64; i += 2)
    {
        const hz::MeshRenderer* mesh = (const hz::MeshRenderer*)world.getComponent(entities[i], hz::SceneTypes::meshRenderer);
        ASSERT_NE(mesh, nullptr);
        ASSERT_EQ(mesh->overrides.size(), 1u);
        EXPECT_EQ(mesh->mesh.low, i + 1);
        EXPECT_EQ(mesh->overrides[0].material.low, i + 100);
        ASSERT_TRUE(world.setComponent(entities[i], hz::SceneTypes::meshRenderer, mesh));
        EXPECT_EQ(((const hz::MeshRenderer*)world.getComponent(entities[i], hz::SceneTypes::meshRenderer))->overrides[0].submesh, i + 1);
    }
}

TEST_F(SceneWorldTest, NestedDeferredScopesDelayStructureAndOwnQueuedValues)
{
    hz::TypeRegistry types;
    ASSERT_TRUE(types.registerBuiltins());
    hz::World        world(types);
    const hz::Entity entity = world.createEntity();
    ASSERT_TRUE(world.addComponent(entity, hz::SceneTypes::camera));
    hz::Entity created;
    {
        hz::DeferredChanges changes = world.defer();
        EXPECT_TRUE(world.isDeferred());
        created = world.createEntity();
        ASSERT_TRUE(world.isAlive(created));
        {
            hz::DeferredChanges nested = world.defer();
            hz::MeshRenderer    mesh;
            mesh.overrides.pushBack({ .submesh = 7, .material = { .low = 29 } });
            ASSERT_TRUE(world.setComponent(created, hz::SceneTypes::meshRenderer, &mesh));
            const hz::Camera camera = { .farPlane = 250.0f };
            ASSERT_TRUE(world.setComponent(entity, hz::SceneTypes::camera, &camera));
        }
        EXPECT_EQ(world.getComponent(created, hz::SceneTypes::meshRenderer), nullptr);
        EXPECT_FLOAT_EQ(((const hz::Camera*)world.getComponent(entity, hz::SceneTypes::camera))->farPlane, 250.0f);
        EXPECT_TRUE(world.isDeferred());
    }
    EXPECT_FALSE(world.isDeferred());
    const hz::MeshRenderer* mesh = (const hz::MeshRenderer*)world.getComponent(created, hz::SceneTypes::meshRenderer);
    ASSERT_NE(mesh, nullptr);
    ASSERT_EQ(mesh->overrides.size(), 1u);
    EXPECT_EQ(mesh->overrides[0].material.low, 29u);
    EXPECT_FLOAT_EQ(((const hz::Camera*)world.getComponent(entity, hz::SceneTypes::camera))->farPlane, 250.0f);
}

struct QueryState
{
    hz::World* world;
    uint32_t   count = 0;
};

static bool countEntities(hz::Entity, hz::Span<const void*> fields, void* context)
{
    EXPECT_EQ(fields.count, 0u);
    ++((QueryState*)context)->count;
    return true;
}

static bool replaceQueriedEntity(hz::Entity entity, hz::Span<const void*> fields, void* context)
{
    QueryState& state = *(QueryState*)context;
    EXPECT_EQ(fields.count, 2u);
    EXPECT_NE(fields.pData[0], nullptr);
    EXPECT_NE(fields.pData[1], nullptr);
    EXPECT_FLOAT_EQ(((const hz::Camera*)fields.pData[1])->farPlane, 1000.0f);
    EXPECT_TRUE(state.world->isDeferred());
    EXPECT_TRUE(state.world->destroyEntity(entity));
    const hz::Entity replacement = state.world->createEntity();
    EXPECT_TRUE(state.world->addComponent(replacement, hz::SceneTypes::meshRenderer));
    EXPECT_TRUE(state.world->addComponent(replacement, hz::SceneTypes::camera));
    ++state.count;
    return true;
}

TEST_F(SceneWorldTest, QueriesFilterEntitiesAndDeferStructuralChanges)
{
    hz::TypeRegistry types;
    ASSERT_TRUE(types.registerBuiltins());
    hz::World      world(types);
    hz::WorldQuery all(world, {});
    hz::WorldQuery renderables(world, { hz::SceneTypes::meshRenderer, hz::SceneTypes::camera });
    ASSERT_TRUE(all.isValid());
    ASSERT_TRUE(renderables.isValid());
    for (uint32_t i = 0; i < 12; ++i)
    {
        const hz::Entity entity = world.createEntity();
        ASSERT_TRUE(world.addComponent(entity, hz::SceneTypes::camera));
        if (i < 8)
            ASSERT_TRUE(world.addComponent(entity, hz::SceneTypes::meshRenderer));
    }
    QueryState state = { .world = &world };
    all.each(countEntities, &state);
    EXPECT_EQ(state.count, 12u);
    state.count = 0;
    renderables.each(replaceQueriedEntity, &state);
    EXPECT_EQ(state.count, 8u);
    EXPECT_FALSE(world.isDeferred());
    state.count = 0;
    all.each(countEntities, &state);
    EXPECT_EQ(state.count, 12u);
}

static bool stopAfterDelete(hz::Entity entity, hz::Span<const void*>, void* context)
{
    QueryState& state = *(QueryState*)context;
    ++state.count;
    EXPECT_TRUE(state.world->destroyEntity(entity));
    return false;
}

TEST_F(SceneWorldTest, EarlyQueryExitCommitsAndReleasesIterator)
{
    hz::TypeRegistry types;
    ASSERT_TRUE(types.registerBuiltins());
    hz::World      world(types);
    hz::WorldQuery query(world, {});
    world.createEntity();
    world.createEntity();
    QueryState state = { .world = &world };
    query.each(stopAfterDelete, &state);
    EXPECT_EQ(state.count, 1u);
    EXPECT_FALSE(world.isDeferred());
    state.count = 0;
    query.each(countEntities, &state);
    EXPECT_EQ(state.count, 1u);
}

TEST_F(SceneWorldTest, DeferredDeleteDiscardsOwningValuesAndWorldsStayIndependent)
{
    hz::TypeRegistry types;
    ASSERT_TRUE(types.registerBuiltins());
    hz::World        first(types);
    hz::World        second(types);
    const hz::Entity original = first.createEntity();
    const hz::Entity other = second.createEntity();
    ASSERT_TRUE(first.addComponent(original, hz::SceneTypes::camera));
    EXPECT_EQ(second.getComponent(other, hz::SceneTypes::camera), nullptr);
    {
        hz::DeferredChanges changes = first.defer();
        hz::MeshRenderer    mesh;
        mesh.overrides.pushBack({ .submesh = 1 });
        const hz::Entity created = first.createEntity();
        ASSERT_TRUE(first.setComponent(created, hz::SceneTypes::meshRenderer, &mesh));
        ASSERT_TRUE(first.destroyEntity(created));
        ASSERT_TRUE(first.destroyEntity(original));
        ASSERT_TRUE(first.setComponent(original, hz::SceneTypes::meshRenderer, &mesh));
    }
    EXPECT_FALSE(first.isAlive(original));
    EXPECT_TRUE(second.isAlive(other));
}
