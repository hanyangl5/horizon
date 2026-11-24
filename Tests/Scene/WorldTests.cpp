#include <gtest/gtest.h>

#include "Scene/SceneComponents.h"
#include "Scene/World.h"

#include <string.h>

static_assert(sizeof(hz::Entity) == 2 * sizeof(uint64_t));

class SceneWorldTest: public ::testing::Test
{
protected:
    void SetUp() override
    {
        ASSERT_TRUE(InitMemoryAlloc(nullptr));
        initialStats = GetMemoryTrackingStats();
    }
    void TearDown() override
    {
        const MemoryTrackingStats stats = GetMemoryTrackingStats();
        if (stats.TrackingEnabled)
        {
            EXPECT_EQ(stats.LiveAllocationCount, initialStats.LiveAllocationCount);
            EXPECT_EQ(stats.LiveRequestedBytes, initialStats.LiveRequestedBytes);
        }
        ExitMemoryAlloc();
    }
    MemoryTrackingStats initialStats = {};
};

TEST_F(SceneWorldTest, EntityGenerationRejectsDeletedHandles)
{
    hz::TypeRegistry types;
    ASSERT_TRUE(types.RegisterBuiltins());
    hz::World world(types);
    EXPECT_FALSE(world.IsAlive({}));
    const hz::Entity first = world.CreateEntity();
    ASSERT_TRUE(first.IsValid());
    EXPECT_TRUE(world.IsAlive(first));
    EXPECT_TRUE(world.DestroyEntity(first));
    EXPECT_FALSE(world.IsAlive(first));
    const hz::Entity second = world.CreateEntity();
    ASSERT_TRUE(world.IsAlive(second));
    EXPECT_NE(first, second);
    EXPECT_FALSE(world.DestroyEntity(first));
    EXPECT_FALSE(world.AddComponent(first, hz::SceneTypes::camera));
    EXPECT_FALSE(world.RemoveComponent(first, hz::SceneTypes::camera));
    EXPECT_EQ(world.GetComponent(first, hz::SceneTypes::camera), nullptr);
    EXPECT_TRUE(world.IsAlive(second));
}

TEST_F(SceneWorldTest, ForeignEntitiesAreRejectedByOtherWorlds)
{
    hz::TypeRegistry types;
    ASSERT_TRUE(types.RegisterBuiltins());
    hz::World first(types);
    hz::World second(types);
    const hz::Entity entity = first.CreateEntity();
    ASSERT_TRUE(first.IsAlive(entity));
    // Both worlds allocate the same Flecs index, so only the recorded owner can tell them apart.
    EXPECT_FALSE(second.IsAlive(entity));
    EXPECT_FALSE(second.DestroyEntity(entity));
    EXPECT_FALSE(second.DestroySubtree(entity));
    EXPECT_EQ(second.GetComponent(entity, hz::SceneTypes::camera), nullptr);
    EXPECT_EQ(second.GetEntityGuid(entity), hz::Guid{});
    EXPECT_FALSE(second.AddComponent(entity, hz::SceneTypes::camera));
    EXPECT_FALSE(second.SetProperty(entity, hz::SceneTypes::camera, 0, {}));
    EXPECT_FALSE(second.SetParent(entity, entity));
    EXPECT_TRUE(first.IsAlive(entity));
}

TEST_F(SceneWorldTest, ComponentsConstructDefaultsAndCopyValues)
{
    hz::TypeRegistry types;
    ASSERT_TRUE(types.RegisterBuiltins());
    hz::World        world(types);
    const hz::Entity entity = world.CreateEntity();
    ASSERT_TRUE(world.AddComponent(entity, hz::SceneTypes::camera));
    const hz::Camera* camera = (const hz::Camera*)world.GetComponent(entity, hz::SceneTypes::camera);
    ASSERT_NE(camera, nullptr);
    EXPECT_FLOAT_EQ(camera->nearPlane, 0.1f);
    EXPECT_FLOAT_EQ(camera->farPlane, 1000.0f);
    const hz::Camera replacement = { .verticalFov = 0.9f, .nearPlane = 0.2f, .farPlane = 500.0f };
    ASSERT_TRUE(world.SetComponent(entity, hz::SceneTypes::camera, &replacement));
    camera = (const hz::Camera*)world.GetComponent(entity, hz::SceneTypes::camera);
    EXPECT_NE(camera, &replacement);
    EXPECT_FLOAT_EQ(camera->farPlane, 500.0f);
    ASSERT_TRUE(world.AddComponent(entity, hz::SceneTypes::camera));
    EXPECT_FLOAT_EQ(((const hz::Camera*)world.GetComponent(entity, hz::SceneTypes::camera))->farPlane, 500.0f);
    EXPECT_FALSE(world.AddComponent(entity, 999));
    EXPECT_FALSE(world.SetComponent(entity, 999, &replacement));
    EXPECT_EQ(world.GetComponent(entity, 999), nullptr);
    EXPECT_TRUE(world.RemoveComponent(entity, hz::SceneTypes::camera));
    EXPECT_EQ(world.GetComponent(entity, hz::SceneTypes::camera), nullptr);
}

TEST_F(SceneWorldTest, OwningComponentsSurviveTableMovesAndRowRemoval)
{
    hz::TypeRegistry types;
    ASSERT_TRUE(types.RegisterBuiltins());
    hz::World  world(types);
    hz::Entity entities[64];
    for (uint32_t i = 0; i < 64; ++i)
    {
        entities[i] = world.CreateEntity();
        hz::MeshRenderer mesh;
        mesh.mesh.low = i + 1;
        mesh.overrides.PushBack({ .submesh = i + 1, .material = { .low = i + 100 } });
        ASSERT_TRUE(world.SetComponent(entities[i], hz::SceneTypes::meshRenderer, &mesh));
        ASSERT_TRUE(world.AddComponent(entities[i], hz::SceneTypes::name));
        ASSERT_TRUE(world.AddComponent(entities[i], hz::SceneTypes::localTransform));
        ASSERT_TRUE(world.RemoveComponent(entities[i], hz::SceneTypes::name));
    }
    for (uint32_t i = 0; i < 64; i += 2)
        ASSERT_TRUE(world.DestroyEntity(entities[i]));
    for (uint32_t i = 1; i < 64; i += 2)
    {
        const hz::MeshRenderer* mesh = (const hz::MeshRenderer*)world.GetComponent(entities[i], hz::SceneTypes::meshRenderer);
        ASSERT_NE(mesh, nullptr);
        ASSERT_EQ(mesh->overrides.Size(), 1u);
        EXPECT_EQ(mesh->mesh.low, i + 1);
        EXPECT_EQ(mesh->overrides[0].material.low, i + 100);
        ASSERT_TRUE(world.SetComponent(entities[i], hz::SceneTypes::meshRenderer, mesh));
        EXPECT_EQ(((const hz::MeshRenderer*)world.GetComponent(entities[i], hz::SceneTypes::meshRenderer))->overrides[0].submesh, i + 1);
    }
}

TEST_F(SceneWorldTest, CopiesOwningComponentFromStorageWhileGrowingItsTable)
{
    hz::TypeRegistry types;
    ASSERT_TRUE(types.RegisterBuiltins());
    hz::World        world(types);
    hz::Entity       source = world.CreateEntity();
    hz::MeshRenderer mesh = { .mesh = { .low = 17 } };
    mesh.overrides.PushBack({ .submesh = 7, .material = { .low = 42 } });
    ASSERT_TRUE(world.SetComponent(source, hz::SceneTypes::meshRenderer, &mesh));
    for (uint32_t i = 0; i < 1000; ++i)
    {
        SCOPED_TRACE(i);
        const hz::Entity destination = world.CreateEntity();
        const void*      pSource = world.GetComponent(source, hz::SceneTypes::meshRenderer);
        ASSERT_TRUE(world.SetComponent(destination, hz::SceneTypes::meshRenderer, pSource));
        const hz::MeshRenderer* pCopy = (const hz::MeshRenderer*)world.GetComponent(destination, hz::SceneTypes::meshRenderer);
        ASSERT_NE(pCopy, nullptr);
        EXPECT_EQ(pCopy->mesh, mesh.mesh);
        ASSERT_EQ(pCopy->overrides.Size(), 1u);
        EXPECT_EQ(pCopy->overrides[0].submesh, 7u);
        EXPECT_EQ(pCopy->overrides[0].material.low, 42u);
        const hz::MeshRenderer* pOriginal = (const hz::MeshRenderer*)world.GetComponent(source, hz::SceneTypes::meshRenderer);
        ASSERT_EQ(pOriginal->overrides.Size(), 1u);
        EXPECT_NE(pCopy->overrides.GetData(), pOriginal->overrides.GetData());
        source = destination;
    }
}

TEST_F(SceneWorldTest, NestedDeferredScopesDelayStructureAndOwnQueuedValues)
{
    hz::TypeRegistry types;
    ASSERT_TRUE(types.RegisterBuiltins());
    hz::World        world(types);
    const hz::Entity entity = world.CreateEntity();
    ASSERT_TRUE(world.AddComponent(entity, hz::SceneTypes::camera));
    hz::Entity created;
    {
        hz::DeferredChanges changes = world.Defer();
        EXPECT_TRUE(world.IsDeferred());
        created = world.CreateEntity();
        ASSERT_TRUE(world.IsAlive(created));
        {
            hz::DeferredChanges nested = world.Defer();
            hz::MeshRenderer    mesh;
            mesh.overrides.PushBack({ .submesh = 7, .material = { .low = 29 } });
            ASSERT_TRUE(world.SetComponent(created, hz::SceneTypes::meshRenderer, &mesh));
            const hz::Camera camera = { .farPlane = 250.0f };
            ASSERT_TRUE(world.SetComponent(entity, hz::SceneTypes::camera, &camera));
        }
        EXPECT_EQ(world.GetComponent(created, hz::SceneTypes::meshRenderer), nullptr);
        EXPECT_FLOAT_EQ(((const hz::Camera*)world.GetComponent(entity, hz::SceneTypes::camera))->farPlane, 250.0f);
        EXPECT_TRUE(world.IsDeferred());
    }
    EXPECT_FALSE(world.IsDeferred());
    const hz::MeshRenderer* mesh = (const hz::MeshRenderer*)world.GetComponent(created, hz::SceneTypes::meshRenderer);
    ASSERT_NE(mesh, nullptr);
    ASSERT_EQ(mesh->overrides.Size(), 1u);
    EXPECT_EQ(mesh->overrides[0].material.low, 29u);
    EXPECT_FLOAT_EQ(((const hz::Camera*)world.GetComponent(entity, hz::SceneTypes::camera))->farPlane, 250.0f);
}

struct QueryState
{
    hz::World* world;
    uint32_t   Count = 0;
};

static bool countEntities(hz::Entity, hz::Span<const void*> fields, void* context)
{
    EXPECT_EQ(fields.Count, 0u);
    ++((QueryState*)context)->Count;
    return true;
}

static bool replaceQueriedEntity(hz::Entity entity, hz::Span<const void*> fields, void* context)
{
    QueryState& state = *(QueryState*)context;
    EXPECT_EQ(fields.Count, 2u);
    EXPECT_NE(fields.Data[0], nullptr);
    EXPECT_NE(fields.Data[1], nullptr);
    EXPECT_FLOAT_EQ(((const hz::Camera*)fields.Data[1])->farPlane, 1000.0f);
    EXPECT_TRUE(state.world->IsDeferred());
    EXPECT_TRUE(state.world->DestroyEntity(entity));
    const hz::Entity replacement = state.world->CreateEntity();
    EXPECT_TRUE(state.world->AddComponent(replacement, hz::SceneTypes::meshRenderer));
    EXPECT_TRUE(state.world->AddComponent(replacement, hz::SceneTypes::camera));
    ++state.Count;
    return true;
}

TEST_F(SceneWorldTest, QueriesFilterEntitiesAndDeferStructuralChanges)
{
    hz::TypeRegistry types;
    ASSERT_TRUE(types.RegisterBuiltins());
    hz::World      world(types);
    hz::WorldQuery all(world, {});
    hz::WorldQuery renderables(world, { hz::SceneTypes::meshRenderer, hz::SceneTypes::camera });
    ASSERT_TRUE(all.IsValid());
    ASSERT_TRUE(renderables.IsValid());
    for (uint32_t i = 0; i < 12; ++i)
    {
        const hz::Entity entity = world.CreateEntity();
        ASSERT_TRUE(world.AddComponent(entity, hz::SceneTypes::camera));
        if (i < 8)
            ASSERT_TRUE(world.AddComponent(entity, hz::SceneTypes::meshRenderer));
    }
    QueryState state = { .world = &world };
    all.Each(countEntities, &state);
    EXPECT_EQ(state.Count, 12u);
    state.Count = 0;
    renderables.Each(replaceQueriedEntity, &state);
    EXPECT_EQ(state.Count, 8u);
    EXPECT_FALSE(world.IsDeferred());
    state.Count = 0;
    all.Each(countEntities, &state);
    EXPECT_EQ(state.Count, 12u);
}

static bool stopAfterDelete(hz::Entity entity, hz::Span<const void*>, void* context)
{
    QueryState& state = *(QueryState*)context;
    ++state.Count;
    EXPECT_TRUE(state.world->DestroyEntity(entity));
    return false;
}

TEST_F(SceneWorldTest, EarlyQueryExitCommitsAndReleasesIterator)
{
    hz::TypeRegistry types;
    ASSERT_TRUE(types.RegisterBuiltins());
    hz::World      world(types);
    hz::WorldQuery query(world, {});
    world.CreateEntity();
    world.CreateEntity();
    QueryState state = { .world = &world };
    query.Each(stopAfterDelete, &state);
    EXPECT_EQ(state.Count, 1u);
    EXPECT_FALSE(world.IsDeferred());
    state.Count = 0;
    query.Each(countEntities, &state);
    EXPECT_EQ(state.Count, 1u);
}

TEST_F(SceneWorldTest, DeferredDeleteDiscardsOwningValuesAndWorldsStayIndependent)
{
    hz::TypeRegistry types;
    ASSERT_TRUE(types.RegisterBuiltins());
    hz::World        first(types);
    hz::World        second(types);
    const hz::Entity original = first.CreateEntity();
    const hz::Entity other = second.CreateEntity();
    ASSERT_TRUE(first.AddComponent(original, hz::SceneTypes::camera));
    EXPECT_EQ(second.GetComponent(other, hz::SceneTypes::camera), nullptr);
    {
        hz::DeferredChanges changes = first.Defer();
        hz::MeshRenderer    mesh;
        mesh.overrides.PushBack({ .submesh = 1 });
        const hz::Entity created = first.CreateEntity();
        ASSERT_TRUE(first.SetComponent(created, hz::SceneTypes::meshRenderer, &mesh));
        ASSERT_TRUE(first.DestroyEntity(created));
        ASSERT_TRUE(first.DestroyEntity(original));
        ASSERT_TRUE(first.SetComponent(original, hz::SceneTypes::meshRenderer, &mesh));
    }
    EXPECT_FALSE(first.IsAlive(original));
    EXPECT_TRUE(second.IsAlive(other));
}

TEST_F(SceneWorldTest, PersistentEntitiesKeepIdentityAcrossComponentMovesAndRejectDuplicates)
{
    hz::TypeRegistry types;
    ASSERT_TRUE(types.RegisterBuiltins());
    hz::World        world(types);
    const hz::Guid   id = { .high = 3, .low = 41 };
    const hz::Entity object = world.CreatePersistentEntity(id);
    ASSERT_TRUE(world.IsAlive(object));
    EXPECT_EQ(world.GetEntityGuid(object), id);
    EXPECT_EQ(world.GetEntity(id), object);
    EXPECT_FALSE(world.CreatePersistentEntity(id).IsValid());
    EXPECT_FALSE(world.CreatePersistentEntity({}).IsValid());
    EXPECT_FALSE(world.GetEntity({}).IsValid());
    EXPECT_FALSE(world.GetEntity({ .low = 42 }).IsValid());
    for (hz::TypeID component : { hz::SceneTypes::camera, hz::SceneTypes::name, hz::SceneTypes::meshRenderer })
    {
        ASSERT_TRUE(world.AddComponent(object, component));
        EXPECT_EQ(world.GetEntityGuid(object), id);
        EXPECT_EQ(world.GetEntity(id), object);
        ASSERT_TRUE(world.RemoveComponent(object, component));
    }
    const hz::EntityIdentity replacement = { .value = { .low = 99 } };
    EXPECT_FALSE(world.SetComponent(object, hz::SceneTypes::entityIdentity, &replacement));
    EXPECT_FALSE(world.RemoveComponent(object, hz::SceneTypes::entityIdentity));
    const hz::Entity runtimeOnly = world.CreateEntity();
    EXPECT_FALSE(world.AddComponent(runtimeOnly, hz::SceneTypes::entityIdentity));
    EXPECT_FALSE(world.GetEntityGuid(runtimeOnly).IsValid());
    EXPECT_EQ(world.GetEntity(id), object);
    EXPECT_FALSE(world.GetEntity(replacement.value).IsValid());
}

struct GuidSource
{
    uint8_t next = 0;
    bool    fail = false;
};

static bool generateGuid(void* pContext, uint8_t (&bytes)[16])
{
    GuidSource& source = *(GuidSource*)pContext;
    if (source.fail)
        return false;
    for (uint8_t& value : bytes)
        value = 0;
    bytes[15] = ++source.next;
    return true;
}

TEST_F(SceneWorldTest, GuidGenerationIsInjectableAndIdentitiesAreScopedToEachWorld)
{
    hz::TypeRegistry types;
    ASSERT_TRUE(types.RegisterBuiltins());
    GuidSource       firstSource;
    GuidSource       secondSource;
    hz::World        first(types, { .pGenerate = generateGuid, .UserData = &firstSource });
    hz::World        second(types, { .pGenerate = generateGuid, .UserData = &secondSource });
    const hz::Entity a = first.CreatePersistentEntity();
    const hz::Entity b = second.CreatePersistentEntity();
    ASSERT_TRUE(first.IsAlive(a));
    ASSERT_TRUE(second.IsAlive(b));
    const hz::Guid id = first.GetEntityGuid(a);
    ASSERT_TRUE(id.IsValid());
    EXPECT_EQ(second.GetEntityGuid(b), id);
    const hz::Entity other = first.CreatePersistentEntity();
    EXPECT_NE(first.GetEntityGuid(other), id);
    firstSource.next = 0;
    EXPECT_FALSE(first.CreatePersistentEntity().IsValid());
    firstSource.fail = true;
    EXPECT_FALSE(first.CreatePersistentEntity().IsValid());
    ASSERT_TRUE(first.DestroyEntity(a));
    EXPECT_FALSE(first.GetEntity(id).IsValid());
    EXPECT_EQ(second.GetEntity(id), b);

    hz::TypeRegistry emptyTypes;
    hz::World        Empty(emptyTypes);
    EXPECT_FALSE(Empty.CreatePersistentEntity(id).IsValid());
}

TEST_F(SceneWorldTest, DeferredPersistentEntitiesReserveGuidsAndReleaseCancelledCreations)
{
    hz::TypeRegistry types;
    ASSERT_TRUE(types.RegisterBuiltins());
    hz::World        world(types);
    const hz::Guid   existingID = { .low = 1 };
    const hz::Guid   cancelledID = { .low = 2 };
    const hz::Guid   newID = { .low = 3 };
    const hz::Entity existing = world.CreatePersistentEntity(existingID);
    hz::Entity       created;
    {
        hz::DeferredChanges changes = world.Defer();
        const hz::Entity    cancelled = world.CreatePersistentEntity(cancelledID);
        ASSERT_TRUE(world.IsAlive(cancelled));
        EXPECT_FALSE(world.GetEntity(cancelledID).IsValid());
        EXPECT_FALSE(world.GetEntityGuid(cancelled).IsValid());
        {
            hz::DeferredChanges nested = world.Defer();
            EXPECT_FALSE(world.CreatePersistentEntity(cancelledID).IsValid());
            ASSERT_TRUE(world.DestroyEntity(cancelled));
            created = world.CreatePersistentEntity(newID);
            ASSERT_TRUE(world.IsAlive(created));
        }
        EXPECT_FALSE(world.GetEntity(newID).IsValid());
        ASSERT_TRUE(world.DestroyEntity(existing));
        EXPECT_EQ(world.GetEntity(existingID), existing);
        EXPECT_FALSE(world.CreatePersistentEntity(existingID).IsValid());
    }
    EXPECT_FALSE(world.GetEntity(cancelledID).IsValid());
    EXPECT_FALSE(world.GetEntity(existingID).IsValid());
    EXPECT_EQ(world.GetEntity(newID), created);
    EXPECT_EQ(world.GetEntityGuid(created), newID);
    ASSERT_TRUE(world.CreatePersistentEntity(cancelledID).IsValid());
    const hz::Entity restored = world.CreatePersistentEntity(existingID);
    ASSERT_TRUE(world.IsAlive(restored));
    EXPECT_NE(restored, existing);
    EXPECT_FALSE(world.IsAlive(existing));
    EXPECT_EQ(world.GetEntity(existingID), restored);
}

TEST_F(SceneWorldTest, HierarchyDeletionRemovesOnlyDestroyedEntityIdentities)
{
    hz::TypeRegistry types;
    ASSERT_TRUE(types.RegisterBuiltins());
    hz::World        world(types);
    const hz::Guid   parentID = { .low = 1 };
    const hz::Guid   childID = { .low = 2 };
    const hz::Guid   leafID = { .low = 3 };
    const hz::Entity parent = world.CreatePersistentEntity(parentID);
    const hz::Entity child = world.CreatePersistentEntity(childID);
    const hz::Entity leaf = world.CreatePersistentEntity(leafID);
    ASSERT_TRUE(world.SetParent(child, parent, false));
    ASSERT_TRUE(world.SetParent(leaf, child, false));
    ASSERT_TRUE(world.DestroyEntity(parent));
    EXPECT_FALSE(world.GetEntity(parentID).IsValid());
    EXPECT_EQ(world.GetEntity(childID), child);
    EXPECT_EQ(world.GetEntity(leafID), leaf);
    ASSERT_TRUE(world.DestroySubtree(child));
    EXPECT_FALSE(world.GetEntity(childID).IsValid());
    EXPECT_FALSE(world.GetEntity(leafID).IsValid());
}

TEST_F(SceneWorldTest, PropertyEditsValidateBeforeWriting)
{
    hz::TypeRegistry types;
    ASSERT_TRUE(types.RegisterBuiltins());
    hz::World               world(types);
    const hz::Guid          id = { .low = 1 };
    const hz::Entity        entity = world.CreatePersistentEntity(id);
    const float             intensity = 5.0f;
    const hz::PropertyValue value = { .kind = hz::PropertyKind::Float, .Data = &intensity, .Size = sizeof(intensity) };
    EXPECT_FALSE(world.SetProperty(entity, hz::SceneTypes::light, 3, value));
    ASSERT_TRUE(world.AddComponent(entity, hz::SceneTypes::light));
    EXPECT_FALSE(world.SetProperty({}, hz::SceneTypes::light, 3, value));
    EXPECT_FALSE(world.SetProperty(entity, 999, 3, value));
    EXPECT_FALSE(world.SetProperty(entity, hz::SceneTypes::light, 999, value));
    EXPECT_FALSE(world.SetProperty(entity, hz::SceneTypes::light, 3,
                                   { .kind = hz::PropertyKind::UnsignedInteger, .Data = &intensity, .Size = sizeof(intensity) }));
    EXPECT_FALSE(world.SetProperty(entity, hz::SceneTypes::light, 3, { .kind = hz::PropertyKind::Float, .Data = &intensity, .Size = 1 }));
    EXPECT_FALSE(world.SetProperty(entity, hz::SceneTypes::light, 3, {}));
    const int32_t invalidEnum = 99;
    EXPECT_FALSE(world.SetProperty(entity, hz::SceneTypes::light, 1,
                                   { .kind = hz::PropertyKind::Enum, .Data = &invalidEnum, .Size = sizeof(invalidEnum) }));
    const uint8_t invalidBool = 2;
    EXPECT_FALSE(world.SetProperty(entity, hz::SceneTypes::light, 15,
                                   { .kind = hz::PropertyKind::Boolean, .Data = &invalidBool, .Size = sizeof(invalidBool) }));
    EXPECT_FALSE(world.SetProperty(entity, hz::SceneTypes::entityIdentity, 1,
                                   { .kind = hz::PropertyKind::EntityReference, .Data = &id, .Size = sizeof(id) }));
    EXPECT_EQ(world.GetEntityGuid(entity), id);
    const hz::Light* pLight = (const hz::Light*)world.GetComponent(entity, hz::SceneTypes::light);
    EXPECT_FLOAT_EQ(pLight->intensity, 1.0f);
    EXPECT_EQ(pLight->Type, hz::LightType::Point);
    EXPECT_FALSE(pLight->enableColorTemperature);
    ASSERT_TRUE(world.SetProperty(entity, hz::SceneTypes::light, 3, value));
    const hz::LightType lightType = hz::LightType::Rect;
    ASSERT_TRUE(world.SetProperty(entity, hz::SceneTypes::light, 1,
                                  { .kind = hz::PropertyKind::Enum, .Data = &lightType, .Size = sizeof(lightType) }));
    EXPECT_EQ(pLight->Type, lightType);
    EXPECT_FLOAT_EQ(pLight->intensity, intensity);
    EXPECT_FLOAT_EQ(pLight->Width, 1.0f);
    ASSERT_TRUE(world.DestroyEntity(entity));
    EXPECT_FALSE(world.SetProperty(entity, hz::SceneTypes::light, 3, value));
}

TEST_F(SceneWorldTest, StringEditsHandleBoundsAndAliasing)
{
    hz::TypeRegistry types;
    ASSERT_TRUE(types.RegisterBuiltins());
    hz::World        world(types);
    const hz::Entity entity = world.CreateEntity();
    ASSERT_TRUE(world.AddComponent(entity, hz::SceneTypes::name));
    char text[sizeof(hz::Name::value)];
    memset(text, 'x', sizeof(text));
    EXPECT_FALSE(
        world.SetProperty(entity, hz::SceneTypes::name, 1, { .kind = hz::PropertyKind::String, .Data = text, .Size = sizeof(text) }));
    text[sizeof(text) - 1] = 0;
    ASSERT_TRUE(
        world.SetProperty(entity, hz::SceneTypes::name, 1, { .kind = hz::PropertyKind::String, .Data = text, .Size = sizeof(text) }));
    const hz::Name* Name = (const hz::Name*)world.GetComponent(entity, hz::SceneTypes::name);
    EXPECT_STREQ(Name->value, text);
    EXPECT_FALSE(
        world.SetProperty(entity, hz::SceneTypes::name, 1, { .kind = hz::PropertyKind::String, .Data = text, .Size = sizeof(text) + 1 }));
    ASSERT_TRUE(world.SetProperty(entity, hz::SceneTypes::name, 1,
                                  { .kind = hz::PropertyKind::String, .Data = Name->value + sizeof(text) - 3, .Size = 3 }));
    EXPECT_STREQ(Name->value, "xx");
    for (uint32_t i = 2; i < sizeof(text); ++i)
        EXPECT_EQ(Name->value[i], 0);
    ASSERT_TRUE(world.SetProperty(entity, hz::SceneTypes::name, 1, { .kind = hz::PropertyKind::String, .Data = "", .Size = 1 }));
    EXPECT_STREQ(Name->value, "");
}

TEST_F(SceneWorldTest, ArrayEditsPreserveOwnershipAndValidateIndices)
{
    hz::TypeRegistry types;
    ASSERT_TRUE(types.RegisterBuiltins());
    hz::World        world(types);
    const hz::Entity entity = world.CreateEntity();
    ASSERT_TRUE(world.AddComponent(entity, hz::SceneTypes::meshRenderer));
    ASSERT_TRUE(world.ResizeArray(entity, hz::SceneTypes::meshRenderer, 2, 1));
    const hz::MeshRenderer* pMesh = (const hz::MeshRenderer*)world.GetComponent(entity, hz::SceneTypes::meshRenderer);
    EXPECT_EQ(pMesh->overrides[0].submesh, 0u);
    EXPECT_FALSE(pMesh->overrides[0].material.IsValid());
    const hz::AssetID       material = { .low = 7 };
    const hz::PropertyValue value = { .kind = hz::PropertyKind::AssetReference, .Data = &material, .Size = sizeof(material) };
    EXPECT_FALSE(world.SetArrayElementProperty(entity, hz::SceneTypes::meshRenderer, 2, 1, 2, value));
    EXPECT_FALSE(world.SetArrayElementProperty(entity, hz::SceneTypes::meshRenderer, 2, 0, 999, value));
    EXPECT_FALSE(
        world.SetArrayElementProperty(entity, hz::SceneTypes::meshRenderer, 2, 0, 2,
                                      { .kind = hz::PropertyKind::EntityReference, .Data = &material, .Size = sizeof(material) }));
    ASSERT_TRUE(world.SetArrayElementProperty(entity, hz::SceneTypes::meshRenderer, 2, 0, 2, value));
    const hz::MaterialOverride* pStorage = pMesh->overrides.GetData();
    const MemoryTrackingStats   before = GetMemoryTrackingStats();
    ASSERT_TRUE(world.SetProperty(entity, hz::SceneTypes::meshRenderer, 1, value));
    ASSERT_TRUE(world.SetArrayElementProperty(entity, hz::SceneTypes::meshRenderer, 2, 0, 2, value));
    const MemoryTrackingStats after = GetMemoryTrackingStats();
    EXPECT_EQ(pMesh->overrides.GetData(), pStorage);
    if (before.TrackingEnabled)
    {
        EXPECT_EQ(after.TotalAllocationCount, before.TotalAllocationCount);
        EXPECT_EQ(after.ReallocationCount, before.ReallocationCount);
    }
    const hz::MaterialOverride other = { .submesh = 8, .material = { .low = 9 } };
    EXPECT_FALSE(world.InsertArrayElement(entity, hz::SceneTypes::meshRenderer, 2, 2, &other));
    EXPECT_FALSE(world.RemoveArrayElement(entity, hz::SceneTypes::meshRenderer, 2, 1));
    EXPECT_FALSE(world.ResizeArray(entity, hz::SceneTypes::meshRenderer, 1, 2));
    EXPECT_FALSE(world.SetProperty(entity, hz::SceneTypes::meshRenderer, 2,
                                   { .kind = hz::PropertyKind::Array, .Data = &pMesh->overrides, .Size = sizeof(pMesh->overrides) }));
    EXPECT_EQ(pMesh->overrides.Size(), 1u);
    ASSERT_TRUE(world.InsertArrayElement(entity, hz::SceneTypes::meshRenderer, 2, 0, &other));
    // Insert from the array itself across a Capacity increase.
    const uint32_t Capacity = pMesh->overrides.Capacity();
    ASSERT_TRUE(world.ResizeArray(entity, hz::SceneTypes::meshRenderer, 2, Capacity));
    ASSERT_TRUE(world.InsertArrayElement(entity, hz::SceneTypes::meshRenderer, 2, 0, &pMesh->overrides[1]));
    EXPECT_EQ(pMesh->overrides[0].material, material);
    EXPECT_EQ(pMesh->overrides[1].submesh, other.submesh);
    ASSERT_TRUE(world.RemoveArrayElement(entity, hz::SceneTypes::meshRenderer, 2, 1));
    EXPECT_EQ(pMesh->overrides[1].material, material);
    ASSERT_TRUE(world.ResizeArray(entity, hz::SceneTypes::meshRenderer, 2, 0));
    EXPECT_EQ(pMesh->overrides.Size(), 0u);
}

TEST_F(SceneWorldTest, ArrayEditsRespectContainerAndElementReadOnlyFlags)
{
    hz::TypeRegistry types;
    ASSERT_TRUE(types.RegisterBuiltins());
    const hz::TypeDesc& meshType = *types.GetType(hz::SceneTypes::meshRenderer);
    hz::PropertyDesc    properties[] = { meshType.properties.Data[0], meshType.properties.Data[1] };
    hz::TypeDesc        elementType = *properties[1].pElementType;
    hz::PropertyDesc    elementProperties[] = { elementType.properties.Data[0], elementType.properties.Data[1] };
    elementProperties[1].Flags |= hz::PROPERTY_READ_ONLY;
    elementType.properties = elementProperties;
    properties[1].pElementType = &elementType;
    properties[0].Flags |= hz::PROPERTY_READ_ONLY;
    hz::TypeDesc editable = meshType;
    editable.id = 100;
    editable.Name = "TestEditableArray";
    editable.properties = properties;
    hz::PropertyDesc lockedProperties[] = { properties[0], properties[1] };
    lockedProperties[1].Flags |= hz::PROPERTY_READ_ONLY;
    hz::TypeDesc locked = editable;
    locked.id = 101;
    locked.Name = "TestLockedArray";
    locked.properties = lockedProperties;
    const hz::TypeDesc customTypes[] = { editable, locked };
    ASSERT_TRUE(types.RegisterTypes(customTypes));
    hz::World        world(types);
    const hz::Entity entity = world.CreateEntity();
    ASSERT_TRUE(world.AddComponent(entity, editable.id));
    ASSERT_TRUE(world.AddComponent(entity, locked.id));
    const hz::MaterialOverride value = { .submesh = 1, .material = { .low = 2 } };
    const hz::PropertyValue    material = { .kind = hz::PropertyKind::AssetReference,
                                            .Data = &value.material,
                                            .Size = sizeof(value.material) };
    EXPECT_FALSE(world.SetProperty(entity, editable.id, 1, material));
    ASSERT_TRUE(world.InsertArrayElement(entity, editable.id, 2, 0, &value));
    EXPECT_FALSE(world.SetArrayElementProperty(entity, editable.id, 2, 0, 2, material));
    EXPECT_FALSE(world.ResizeArray(entity, locked.id, 2, 1));
    EXPECT_FALSE(world.InsertArrayElement(entity, locked.id, 2, 0, &value));
    EXPECT_FALSE(world.RemoveArrayElement(entity, locked.id, 2, 0));
    EXPECT_FALSE(world.SetArrayElementProperty(entity, locked.id, 2, 0, 2, material));
    const hz::MeshRenderer* pMesh = (const hz::MeshRenderer*)world.GetComponent(entity, editable.id);
    EXPECT_FALSE(pMesh->mesh.IsValid());
    EXPECT_EQ(pMesh->overrides[0].material, value.material);
    EXPECT_EQ(((const hz::MeshRenderer*)world.GetComponent(entity, locked.id))->overrides.Size(), 0u);
}

TEST_F(SceneWorldTest, PropertyEditsPropagateTransformsAndRespectDeferredVisibility)
{
    hz::TypeRegistry types;
    ASSERT_TRUE(types.RegisterBuiltins());
    hz::World        world(types);
    const hz::Entity parent = world.CreateEntity();
    const hz::Entity child = world.CreateEntity();
    ASSERT_TRUE(world.AddComponent(parent, hz::SceneTypes::localTransform));
    ASSERT_TRUE(world.SetParent(child, parent, false));
    world.UpdateTransforms();
    const Vector3           translation(5.0f, 0.0f, 0.0f);
    const hz::PropertyValue value = { .kind = hz::PropertyKind::Vector3, .Data = &translation, .Size = sizeof(translation) };
    {
        hz::DeferredChanges changes = world.Defer();
        ASSERT_TRUE(world.SetProperty(parent, hz::SceneTypes::localTransform, 1, value));
        EXPECT_FLOAT_EQ((float)((const hz::LocalTransform*)world.GetComponent(parent, hz::SceneTypes::localTransform))->translation.getX(),
                        5.0f);
        ASSERT_TRUE(world.AddComponent(child, hz::SceneTypes::localTransform));
        EXPECT_FALSE(world.SetProperty(child, hz::SceneTypes::localTransform, 1, value));
    }
    world.UpdateTransforms();
    EXPECT_FLOAT_EQ((float)world.GetWorldTransform(child)->current.getTranslation().getX(), 5.0f);
    ASSERT_TRUE(world.SetProperty(child, hz::SceneTypes::localTransform, 1, value));
    world.UpdateTransforms();
    EXPECT_FLOAT_EQ((float)world.GetWorldTransform(child)->current.getTranslation().getX(), 10.0f);
    ASSERT_TRUE(world.RemoveComponent(parent, hz::SceneTypes::localTransform));
    ASSERT_TRUE(world.AddComponent(parent, hz::SceneTypes::localMatrix));
    const Matrix4 matrix = Matrix4::translation(Vector3(2.0f, 0.0f, 0.0f));
    ASSERT_TRUE(world.SetProperty(parent, hz::SceneTypes::localMatrix, 1,
                                  { .kind = hz::PropertyKind::Matrix4, .Data = &matrix, .Size = sizeof(matrix) }));
    world.UpdateTransforms();
    EXPECT_FLOAT_EQ((float)world.GetWorldTransform(child)->current.getTranslation().getX(), 7.0f);
}
