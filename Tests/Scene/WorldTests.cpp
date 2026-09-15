#include <gtest/gtest.h>

#include "Scene/SceneComponents.h"
#include "Scene/World.h"

#include <string.h>

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

TEST_F(SceneWorldTest, PersistentObjectsKeepIdentityAcrossComponentMovesAndRejectDuplicates)
{
    hz::TypeRegistry types;
    ASSERT_TRUE(types.registerBuiltins());
    hz::World          world(types);
    const hz::ObjectID id = { .high = 3, .low = 41 };
    const hz::Entity   object = world.createObject(id);
    ASSERT_TRUE(world.isAlive(object));
    EXPECT_EQ(world.getObjectID(object), id);
    EXPECT_EQ(world.getEntity(id), object);
    EXPECT_FALSE(world.createObject(id).isValid());
    EXPECT_FALSE(world.createObject({}).isValid());
    EXPECT_FALSE(world.getEntity({}).isValid());
    EXPECT_FALSE(world.getEntity({ .low = 42 }).isValid());
    for (hz::TypeID component : { hz::SceneTypes::camera, hz::SceneTypes::name, hz::SceneTypes::meshRenderer })
    {
        ASSERT_TRUE(world.addComponent(object, component));
        EXPECT_EQ(world.getObjectID(object), id);
        EXPECT_EQ(world.getEntity(id), object);
        ASSERT_TRUE(world.removeComponent(object, component));
    }
    const hz::ObjectIdentity replacement = { .value = { .low = 99 } };
    EXPECT_FALSE(world.setComponent(object, hz::SceneTypes::objectIdentity, &replacement));
    EXPECT_FALSE(world.removeComponent(object, hz::SceneTypes::objectIdentity));
    const hz::Entity runtimeOnly = world.createEntity();
    EXPECT_FALSE(world.addComponent(runtimeOnly, hz::SceneTypes::objectIdentity));
    EXPECT_FALSE(world.getObjectID(runtimeOnly).isValid());
    EXPECT_EQ(world.getEntity(id), object);
    EXPECT_FALSE(world.getEntity(replacement.value).isValid());
}

struct ObjectIDSource
{
    uint8_t next = 0;
    bool    fail = false;
};

static bool generateObjectID(void* pContext, uint8_t (&bytes)[16])
{
    ObjectIDSource& source = *(ObjectIDSource*)pContext;
    if (source.fail)
        return false;
    for (uint8_t& value : bytes)
        value = 0;
    bytes[15] = ++source.next;
    return true;
}

TEST_F(SceneWorldTest, ObjectGenerationIsInjectableAndIdentitiesAreScopedToEachWorld)
{
    hz::TypeRegistry types;
    ASSERT_TRUE(types.registerBuiltins());
    ObjectIDSource   firstSource;
    ObjectIDSource   secondSource;
    hz::World        first(types, { .pGenerate = generateObjectID, .pUserData = &firstSource });
    hz::World        second(types, { .pGenerate = generateObjectID, .pUserData = &secondSource });
    const hz::Entity a = first.createObject();
    const hz::Entity b = second.createObject();
    ASSERT_TRUE(first.isAlive(a));
    ASSERT_TRUE(second.isAlive(b));
    const hz::ObjectID id = first.getObjectID(a);
    ASSERT_TRUE(id.isValid());
    EXPECT_EQ(second.getObjectID(b), id);
    const hz::Entity other = first.createObject();
    EXPECT_NE(first.getObjectID(other), id);
    firstSource.next = 0;
    EXPECT_FALSE(first.createObject().isValid());
    firstSource.fail = true;
    EXPECT_FALSE(first.createObject().isValid());
    ASSERT_TRUE(first.destroyEntity(a));
    EXPECT_FALSE(first.getEntity(id).isValid());
    EXPECT_EQ(second.getEntity(id), b);

    hz::TypeRegistry emptyTypes;
    hz::World        empty(emptyTypes);
    EXPECT_FALSE(empty.createObject(id).isValid());
}

TEST_F(SceneWorldTest, DeferredObjectsReserveIDsAndReleaseCancelledCreations)
{
    hz::TypeRegistry types;
    ASSERT_TRUE(types.registerBuiltins());
    hz::World          world(types);
    const hz::ObjectID existingID = { .low = 1 };
    const hz::ObjectID cancelledID = { .low = 2 };
    const hz::ObjectID newID = { .low = 3 };
    const hz::Entity   existing = world.createObject(existingID);
    hz::Entity         created;
    {
        hz::DeferredChanges changes = world.defer();
        const hz::Entity    cancelled = world.createObject(cancelledID);
        ASSERT_TRUE(world.isAlive(cancelled));
        EXPECT_FALSE(world.getEntity(cancelledID).isValid());
        EXPECT_FALSE(world.getObjectID(cancelled).isValid());
        {
            hz::DeferredChanges nested = world.defer();
            EXPECT_FALSE(world.createObject(cancelledID).isValid());
            ASSERT_TRUE(world.destroyEntity(cancelled));
            created = world.createObject(newID);
            ASSERT_TRUE(world.isAlive(created));
        }
        EXPECT_FALSE(world.getEntity(newID).isValid());
        ASSERT_TRUE(world.destroyEntity(existing));
        EXPECT_EQ(world.getEntity(existingID), existing);
        EXPECT_FALSE(world.createObject(existingID).isValid());
    }
    EXPECT_FALSE(world.getEntity(cancelledID).isValid());
    EXPECT_FALSE(world.getEntity(existingID).isValid());
    EXPECT_EQ(world.getEntity(newID), created);
    EXPECT_EQ(world.getObjectID(created), newID);
    ASSERT_TRUE(world.createObject(cancelledID).isValid());
    const hz::Entity restored = world.createObject(existingID);
    ASSERT_TRUE(world.isAlive(restored));
    EXPECT_NE(restored, existing);
    EXPECT_FALSE(world.isAlive(existing));
    EXPECT_EQ(world.getEntity(existingID), restored);
}

TEST_F(SceneWorldTest, HierarchyDeletionRemovesOnlyDestroyedObjectIdentities)
{
    hz::TypeRegistry types;
    ASSERT_TRUE(types.registerBuiltins());
    hz::World          world(types);
    const hz::ObjectID parentID = { .low = 1 };
    const hz::ObjectID childID = { .low = 2 };
    const hz::ObjectID leafID = { .low = 3 };
    const hz::Entity   parent = world.createObject(parentID);
    const hz::Entity   child = world.createObject(childID);
    const hz::Entity   leaf = world.createObject(leafID);
    ASSERT_TRUE(world.setParent(child, parent, false));
    ASSERT_TRUE(world.setParent(leaf, child, false));
    ASSERT_TRUE(world.destroyEntity(parent));
    EXPECT_FALSE(world.getEntity(parentID).isValid());
    EXPECT_EQ(world.getEntity(childID), child);
    EXPECT_EQ(world.getEntity(leafID), leaf);
    ASSERT_TRUE(world.destroySubtree(child));
    EXPECT_FALSE(world.getEntity(childID).isValid());
    EXPECT_FALSE(world.getEntity(leafID).isValid());
}

TEST_F(SceneWorldTest, PropertyEditsValidateBeforeWriting)
{
    hz::TypeRegistry types;
    ASSERT_TRUE(types.registerBuiltins());
    hz::World               world(types);
    const hz::ObjectID      id = { .low = 1 };
    const hz::Entity        entity = world.createObject(id);
    const float             intensity = 5.0f;
    const hz::PropertyValue value = { .kind = hz::PropertyKind::Float, .pData = &intensity, .size = sizeof(intensity) };
    EXPECT_FALSE(world.setProperty(entity, hz::SceneTypes::light, 3, value));
    ASSERT_TRUE(world.addComponent(entity, hz::SceneTypes::light));
    EXPECT_FALSE(world.setProperty({}, hz::SceneTypes::light, 3, value));
    EXPECT_FALSE(world.setProperty(entity, 999, 3, value));
    EXPECT_FALSE(world.setProperty(entity, hz::SceneTypes::light, 999, value));
    EXPECT_FALSE(world.setProperty(entity, hz::SceneTypes::light, 3,
                                   { .kind = hz::PropertyKind::UnsignedInteger, .pData = &intensity, .size = sizeof(intensity) }));
    EXPECT_FALSE(world.setProperty(entity, hz::SceneTypes::light, 3, { .kind = hz::PropertyKind::Float, .pData = &intensity, .size = 1 }));
    EXPECT_FALSE(world.setProperty(entity, hz::SceneTypes::light, 3, {}));
    const int32_t invalidEnum = 99;
    EXPECT_FALSE(world.setProperty(entity, hz::SceneTypes::light, 1,
                                   { .kind = hz::PropertyKind::Enum, .pData = &invalidEnum, .size = sizeof(invalidEnum) }));
    const uint8_t invalidBool = 2;
    EXPECT_FALSE(world.setProperty(entity, hz::SceneTypes::light, 15,
                                   { .kind = hz::PropertyKind::Boolean, .pData = &invalidBool, .size = sizeof(invalidBool) }));
    EXPECT_FALSE(world.setProperty(entity, hz::SceneTypes::objectIdentity, 1,
                                   { .kind = hz::PropertyKind::ObjectReference, .pData = &id, .size = sizeof(id) }));
    EXPECT_EQ(world.getObjectID(entity), id);
    const hz::Light* pLight = (const hz::Light*)world.getComponent(entity, hz::SceneTypes::light);
    EXPECT_FLOAT_EQ(pLight->intensity, 1.0f);
    EXPECT_EQ(pLight->type, hz::LightType::Point);
    EXPECT_FALSE(pLight->enableColorTemperature);
    ASSERT_TRUE(world.setProperty(entity, hz::SceneTypes::light, 3, value));
    const hz::LightType lightType = hz::LightType::Rect;
    ASSERT_TRUE(world.setProperty(entity, hz::SceneTypes::light, 1,
                                  { .kind = hz::PropertyKind::Enum, .pData = &lightType, .size = sizeof(lightType) }));
    EXPECT_EQ(pLight->type, lightType);
    EXPECT_FLOAT_EQ(pLight->intensity, intensity);
    EXPECT_FLOAT_EQ(pLight->width, 1.0f);
    ASSERT_TRUE(world.destroyEntity(entity));
    EXPECT_FALSE(world.setProperty(entity, hz::SceneTypes::light, 3, value));
}

TEST_F(SceneWorldTest, StringEditsHandleBoundsAndAliasing)
{
    hz::TypeRegistry types;
    ASSERT_TRUE(types.registerBuiltins());
    hz::World        world(types);
    const hz::Entity entity = world.createEntity();
    ASSERT_TRUE(world.addComponent(entity, hz::SceneTypes::name));
    char text[sizeof(hz::Name::value)];
    memset(text, 'x', sizeof(text));
    EXPECT_FALSE(
        world.setProperty(entity, hz::SceneTypes::name, 1, { .kind = hz::PropertyKind::String, .pData = text, .size = sizeof(text) }));
    text[sizeof(text) - 1] = 0;
    ASSERT_TRUE(
        world.setProperty(entity, hz::SceneTypes::name, 1, { .kind = hz::PropertyKind::String, .pData = text, .size = sizeof(text) }));
    const hz::Name* pName = (const hz::Name*)world.getComponent(entity, hz::SceneTypes::name);
    EXPECT_STREQ(pName->value, text);
    EXPECT_FALSE(
        world.setProperty(entity, hz::SceneTypes::name, 1, { .kind = hz::PropertyKind::String, .pData = text, .size = sizeof(text) + 1 }));
    ASSERT_TRUE(world.setProperty(entity, hz::SceneTypes::name, 1,
                                  { .kind = hz::PropertyKind::String, .pData = pName->value + sizeof(text) - 3, .size = 3 }));
    EXPECT_STREQ(pName->value, "xx");
    for (uint32_t i = 2; i < sizeof(text); ++i)
        EXPECT_EQ(pName->value[i], 0);
    ASSERT_TRUE(world.setProperty(entity, hz::SceneTypes::name, 1, { .kind = hz::PropertyKind::String, .pData = "", .size = 1 }));
    EXPECT_STREQ(pName->value, "");
}

TEST_F(SceneWorldTest, ArrayEditsPreserveOwnershipAndValidateIndices)
{
    hz::TypeRegistry types;
    ASSERT_TRUE(types.registerBuiltins());
    hz::World        world(types);
    const hz::Entity entity = world.createEntity();
    ASSERT_TRUE(world.addComponent(entity, hz::SceneTypes::meshRenderer));
    ASSERT_TRUE(world.resizeArray(entity, hz::SceneTypes::meshRenderer, 2, 1));
    const hz::MeshRenderer* pMesh = (const hz::MeshRenderer*)world.getComponent(entity, hz::SceneTypes::meshRenderer);
    EXPECT_EQ(pMesh->overrides[0].submesh, 0u);
    EXPECT_FALSE(pMesh->overrides[0].material.isValid());
    const hz::AssetID       material = { .low = 7 };
    const hz::PropertyValue value = { .kind = hz::PropertyKind::AssetReference, .pData = &material, .size = sizeof(material) };
    EXPECT_FALSE(world.setArrayElementProperty(entity, hz::SceneTypes::meshRenderer, 2, 1, 2, value));
    EXPECT_FALSE(world.setArrayElementProperty(entity, hz::SceneTypes::meshRenderer, 2, 0, 999, value));
    EXPECT_FALSE(
        world.setArrayElementProperty(entity, hz::SceneTypes::meshRenderer, 2, 0, 2,
                                      { .kind = hz::PropertyKind::ObjectReference, .pData = &material, .size = sizeof(material) }));
    ASSERT_TRUE(world.setArrayElementProperty(entity, hz::SceneTypes::meshRenderer, 2, 0, 2, value));
    const hz::MaterialOverride* pStorage = pMesh->overrides.data();
    const MemoryTrackingStats   before = memGetTrackingStats();
    ASSERT_TRUE(world.setProperty(entity, hz::SceneTypes::meshRenderer, 1, value));
    ASSERT_TRUE(world.setArrayElementProperty(entity, hz::SceneTypes::meshRenderer, 2, 0, 2, value));
    const MemoryTrackingStats after = memGetTrackingStats();
    EXPECT_EQ(pMesh->overrides.data(), pStorage);
    if (before.trackingEnabled)
    {
        EXPECT_EQ(after.totalAllocationCount, before.totalAllocationCount);
        EXPECT_EQ(after.reallocationCount, before.reallocationCount);
    }
    const hz::MaterialOverride other = { .submesh = 8, .material = { .low = 9 } };
    EXPECT_FALSE(world.insertArrayElement(entity, hz::SceneTypes::meshRenderer, 2, 2, &other));
    EXPECT_FALSE(world.removeArrayElement(entity, hz::SceneTypes::meshRenderer, 2, 1));
    EXPECT_FALSE(world.resizeArray(entity, hz::SceneTypes::meshRenderer, 1, 2));
    EXPECT_FALSE(world.setProperty(entity, hz::SceneTypes::meshRenderer, 2,
                                   { .kind = hz::PropertyKind::Array, .pData = &pMesh->overrides, .size = sizeof(pMesh->overrides) }));
    EXPECT_EQ(pMesh->overrides.size(), 1u);
    ASSERT_TRUE(world.insertArrayElement(entity, hz::SceneTypes::meshRenderer, 2, 0, &other));
    // Insert from the array itself across a capacity increase.
    const uint32_t capacity = pMesh->overrides.capacity();
    ASSERT_TRUE(world.resizeArray(entity, hz::SceneTypes::meshRenderer, 2, capacity));
    ASSERT_TRUE(world.insertArrayElement(entity, hz::SceneTypes::meshRenderer, 2, 0, &pMesh->overrides[1]));
    EXPECT_EQ(pMesh->overrides[0].material, material);
    EXPECT_EQ(pMesh->overrides[1].submesh, other.submesh);
    ASSERT_TRUE(world.removeArrayElement(entity, hz::SceneTypes::meshRenderer, 2, 1));
    EXPECT_EQ(pMesh->overrides[1].material, material);
    ASSERT_TRUE(world.resizeArray(entity, hz::SceneTypes::meshRenderer, 2, 0));
    EXPECT_EQ(pMesh->overrides.size(), 0u);
}

TEST_F(SceneWorldTest, ArrayEditsRespectContainerAndElementReadOnlyFlags)
{
    hz::TypeRegistry types;
    ASSERT_TRUE(types.registerBuiltins());
    const hz::TypeDesc& meshType = *types.getType(hz::SceneTypes::meshRenderer);
    hz::PropertyDesc    properties[] = { meshType.properties.pData[0], meshType.properties.pData[1] };
    hz::TypeDesc        elementType = *properties[1].pElementType;
    hz::PropertyDesc    elementProperties[] = { elementType.properties.pData[0], elementType.properties.pData[1] };
    elementProperties[1].flags |= hz::PROPERTY_READ_ONLY;
    elementType.properties = elementProperties;
    properties[1].pElementType = &elementType;
    properties[0].flags |= hz::PROPERTY_READ_ONLY;
    hz::TypeDesc editable = meshType;
    editable.id = 100;
    editable.pName = "TestEditableArray";
    editable.properties = properties;
    hz::PropertyDesc lockedProperties[] = { properties[0], properties[1] };
    lockedProperties[1].flags |= hz::PROPERTY_READ_ONLY;
    hz::TypeDesc locked = editable;
    locked.id = 101;
    locked.pName = "TestLockedArray";
    locked.properties = lockedProperties;
    const hz::TypeDesc customTypes[] = { editable, locked };
    ASSERT_TRUE(types.registerTypes(customTypes));
    hz::World        world(types);
    const hz::Entity entity = world.createEntity();
    ASSERT_TRUE(world.addComponent(entity, editable.id));
    ASSERT_TRUE(world.addComponent(entity, locked.id));
    const hz::MaterialOverride value = { .submesh = 1, .material = { .low = 2 } };
    const hz::PropertyValue    material = { .kind = hz::PropertyKind::AssetReference,
                                            .pData = &value.material,
                                            .size = sizeof(value.material) };
    EXPECT_FALSE(world.setProperty(entity, editable.id, 1, material));
    ASSERT_TRUE(world.insertArrayElement(entity, editable.id, 2, 0, &value));
    EXPECT_FALSE(world.setArrayElementProperty(entity, editable.id, 2, 0, 2, material));
    EXPECT_FALSE(world.resizeArray(entity, locked.id, 2, 1));
    EXPECT_FALSE(world.insertArrayElement(entity, locked.id, 2, 0, &value));
    EXPECT_FALSE(world.removeArrayElement(entity, locked.id, 2, 0));
    EXPECT_FALSE(world.setArrayElementProperty(entity, locked.id, 2, 0, 2, material));
    const hz::MeshRenderer* pMesh = (const hz::MeshRenderer*)world.getComponent(entity, editable.id);
    EXPECT_FALSE(pMesh->mesh.isValid());
    EXPECT_EQ(pMesh->overrides[0].material, value.material);
    EXPECT_EQ(((const hz::MeshRenderer*)world.getComponent(entity, locked.id))->overrides.size(), 0u);
}

TEST_F(SceneWorldTest, PropertyEditsPropagateTransformsAndRespectDeferredVisibility)
{
    hz::TypeRegistry types;
    ASSERT_TRUE(types.registerBuiltins());
    hz::World        world(types);
    const hz::Entity parent = world.createEntity();
    const hz::Entity child = world.createEntity();
    ASSERT_TRUE(world.addComponent(parent, hz::SceneTypes::localTransform));
    ASSERT_TRUE(world.setParent(child, parent, false));
    world.updateTransforms();
    const Vector3           translation(5.0f, 0.0f, 0.0f);
    const hz::PropertyValue value = { .kind = hz::PropertyKind::Vector3, .pData = &translation, .size = sizeof(translation) };
    {
        hz::DeferredChanges changes = world.defer();
        ASSERT_TRUE(world.setProperty(parent, hz::SceneTypes::localTransform, 1, value));
        EXPECT_FLOAT_EQ((float)((const hz::LocalTransform*)world.getComponent(parent, hz::SceneTypes::localTransform))->translation.getX(),
                        5.0f);
        ASSERT_TRUE(world.addComponent(child, hz::SceneTypes::localTransform));
        EXPECT_FALSE(world.setProperty(child, hz::SceneTypes::localTransform, 1, value));
    }
    world.updateTransforms();
    EXPECT_FLOAT_EQ((float)world.getWorldTransform(child)->current.getTranslation().getX(), 5.0f);
    EXPECT_FLOAT_EQ((float)world.getWorldTransform(child)->previous.getTranslation().getX(), 0.0f);
    world.commitTransforms();
    ASSERT_TRUE(world.setProperty(child, hz::SceneTypes::localTransform, 1, value));
    world.updateTransforms();
    EXPECT_FLOAT_EQ((float)world.getWorldTransform(child)->current.getTranslation().getX(), 10.0f);
    EXPECT_FLOAT_EQ((float)world.getWorldTransform(child)->previous.getTranslation().getX(), 5.0f);
    ASSERT_TRUE(world.removeComponent(parent, hz::SceneTypes::localTransform));
    ASSERT_TRUE(world.addComponent(parent, hz::SceneTypes::localMatrix));
    const Matrix4 matrix = Matrix4::translation(Vector3(2.0f, 0.0f, 0.0f));
    ASSERT_TRUE(world.setProperty(parent, hz::SceneTypes::localMatrix, 1,
                                  { .kind = hz::PropertyKind::Matrix4, .pData = &matrix, .size = sizeof(matrix) }));
    world.updateTransforms();
    EXPECT_FLOAT_EQ((float)world.getWorldTransform(child)->current.getTranslation().getX(), 7.0f);
}
