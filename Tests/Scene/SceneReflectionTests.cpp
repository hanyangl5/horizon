#include <gtest/gtest.h>

#include "Scene/SceneComponents.h"
#include "Scene/SceneReflection.h"

#include <stddef.h>
#include <string.h>

class SceneReflectionTest: public ::testing::Test
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

TEST_F(SceneReflectionTest, BuiltinsExposeStableTypedProperties)
{
    hz::TypeRegistry registry;
    ASSERT_TRUE(registry.registerBuiltins());
    ASSERT_EQ(registry.size(), 7u);
    const hz::TypeDesc* mesh = registry.getType(hz::SceneTypes::meshRenderer);
    ASSERT_NE(mesh, nullptr);
    EXPECT_EQ(mesh, registry.getType("MeshRenderer"));
    EXPECT_EQ(mesh->version, 1u);
    EXPECT_EQ(mesh->size, sizeof(hz::MeshRenderer));
    const hz::PropertyDesc* property = mesh->getProperty("mesh");
    ASSERT_NE(property, nullptr);
    EXPECT_EQ(property, mesh->getProperty(1u));
    EXPECT_EQ(property->kind, hz::PropertyKind::AssetReference);
    EXPECT_EQ(property->offset, offsetof(hz::MeshRenderer, mesh));
    EXPECT_EQ(property->size, sizeof(hz::AssetID));
    EXPECT_EQ(mesh->getProperty(99u), nullptr);
    EXPECT_EQ(mesh->getProperty("missing"), nullptr);
    EXPECT_EQ(registry.getType(UINT64_C(999)), nullptr);
    EXPECT_EQ(registry.getType("missing"), nullptr);
    for (uint32_t i = 0; i < registry.size(); ++i)
        EXPECT_EQ(registry.getTypeAt(i), registry.getType(registry.getTypeAt(i)->id));

    const hz::PropertyDesc* lightType = registry.getType(hz::SceneTypes::light)->getProperty("type");
    ASSERT_NE(lightType, nullptr);
    ASSERT_EQ(lightType->enumValues.count, 9u);
    EXPECT_STREQ(lightType->enumValues.pData[2].pName, "Spot");
    EXPECT_EQ(lightType->enumValues.pData[2].value, (int32_t)hz::LightType::Spot);
    const hz::PropertyDesc* emissionTexture = registry.getType(hz::SceneTypes::light)->getProperty("texture");
    ASSERT_NE(emissionTexture, nullptr);
    EXPECT_EQ(emissionTexture->kind, hz::PropertyKind::AssetReference);
    EXPECT_EQ(emissionTexture->offset, offsetof(hz::Light, texture));
    EXPECT_EQ(registry.getType(hz::SceneTypes::name)->getProperty("value")->size, 128u);
    EXPECT_TRUE(registry.getType(hz::SceneTypes::objectIdentity)->getProperty("value")->flags & hz::PROPERTY_READ_ONLY);
}

TEST_F(SceneReflectionTest, RegistrationRejectsDuplicatesWithoutPublishingPartialBatches)
{
    hz::TypeRegistry registry;
    ASSERT_TRUE(registry.registerBuiltins());
    const hz::TypeDesc types[] = {
        { .id = 100, .pName = "NewType", .size = 4, .alignment = 4 },
        { .id = hz::SceneTypes::camera, .pName = "DuplicateID", .size = 4, .alignment = 4 },
    };
    EXPECT_FALSE(registry.registerTypes(types));
    EXPECT_EQ(registry.size(), 7u);
    EXPECT_EQ(registry.getType(UINT64_C(100)), nullptr);
    const hz::TypeDesc duplicateName = { .id = 101, .pName = "Camera", .size = 4 };
    EXPECT_FALSE(registry.registerTypes({ &duplicateName, 1 }));
    EXPECT_FALSE(registry.registerBuiltins());
    EXPECT_EQ(registry.size(), 7u);

    const hz::TypeDesc repeated[] = {
        { .id = 100, .pName = "Same", .size = 4 },
        { .id = 101, .pName = "Same", .size = 4 },
    };
    EXPECT_FALSE(registry.registerTypes(repeated));
    EXPECT_EQ(registry.size(), 7u);
    EXPECT_TRUE(registry.registerTypes({}));
}

TEST_F(SceneReflectionTest, RejectsInvalidPropertyLayoutsAndMetadata)
{
    hz::TypeRegistry registry;
    hz::PropertyDesc property = { .id = 1, .pName = "value", .kind = hz::PropertyKind::Float, .size = 4 };
    hz::TypeDesc     type = { .id = 100, .pName = "Invalid", .size = 4, .alignment = 4, .properties = { &property, 1 } };
    property.offset = 1;
    EXPECT_FALSE(registry.registerTypes({ &type, 1 }));
    property.offset = UINT32_MAX;
    EXPECT_FALSE(registry.registerTypes({ &type, 1 }));
    property.offset = 0;
    property.size = 3;
    EXPECT_FALSE(registry.registerTypes({ &type, 1 }));
    property.size = 4;
    property.kind = (hz::PropertyKind)255;
    EXPECT_FALSE(registry.registerTypes({ &type, 1 }));
    property.kind = hz::PropertyKind::Float;
    property.flags = hz::PROPERTY_SERIALIZED | hz::PROPERTY_TRANSIENT;
    EXPECT_FALSE(registry.registerTypes({ &type, 1 }));
    property.flags = hz::PROPERTY_SERIALIZED;
    type.alignment = 3;
    EXPECT_FALSE(registry.registerTypes({ &type, 1 }));
    type.alignment = 4;
    type.properties = { nullptr, 1 };
    EXPECT_FALSE(registry.registerTypes({ &type, 1 }));
    type.properties = { &property, 1 };
    type.version = 0;
    EXPECT_FALSE(registry.registerTypes({ &type, 1 }));
    EXPECT_EQ(registry.size(), 0u);
}

TEST_F(SceneReflectionTest, RejectsDuplicatePropertiesAndIncompleteEnumOrArrayMetadata)
{
    hz::TypeRegistry registry;
    hz::PropertyDesc properties[] = {
        { .id = 1, .pName = "a", .kind = hz::PropertyKind::UnsignedInteger, .size = 4 },
        { .id = 1, .pName = "b", .kind = hz::PropertyKind::UnsignedInteger, .offset = 4, .size = 4 },
    };
    hz::TypeDesc type = { .id = 100, .pName = "Invalid", .size = 8, .alignment = 4, .properties = properties };
    EXPECT_FALSE(registry.registerTypes({ &type, 1 }));
    properties[1].id = 2;
    properties[1].pName = "a";
    EXPECT_FALSE(registry.registerTypes({ &type, 1 }));
    type.properties = { properties, 1 };
    properties[0].kind = hz::PropertyKind::Enum;
    EXPECT_FALSE(registry.registerTypes({ &type, 1 }));
    const hz::EnumValue duplicateValues[] = { { .pName = "A", .value = 1 }, { .pName = "B", .value = 1 } };
    properties[0].enumValues = duplicateValues;
    EXPECT_FALSE(registry.registerTypes({ &type, 1 }));

    ASSERT_TRUE(registry.registerBuiltins());
    hz::TypeDesc incomplete = *registry.getType(hz::SceneTypes::meshRenderer);
    incomplete.id = 101;
    incomplete.pName = "Incomplete";
    incomplete.lifecycle.pDestroy = nullptr;
    EXPECT_FALSE(registry.registerTypes({ &incomplete, 1 }));
    incomplete = *registry.getType(hz::SceneTypes::meshRenderer);
    incomplete.id = 101;
    incomplete.pName = "Incomplete";
    hz::PropertyDesc array = *incomplete.getProperty("overrides");
    incomplete.properties = { &array, 1 };
    array.pElementType = nullptr;
    EXPECT_FALSE(registry.registerTypes({ &incomplete, 1 }));
    array = *registry.getType(hz::SceneTypes::meshRenderer)->getProperty("overrides");
    hz::TypeDesc element = *array.pElementType;
    array.pElementType = &element;
    element.triviallyCopyable = false;
    EXPECT_FALSE(registry.registerTypes({ &incomplete, 1 }));
    element.triviallyCopyable = true;
    element.properties = {};
    EXPECT_FALSE(registry.registerTypes({ &incomplete, 1 }));
    EXPECT_EQ(registry.size(), 7u);
}

TEST_F(SceneReflectionTest, ConstructionPreservesComponentDefaults)
{
    hz::TypeRegistry registry;
    ASSERT_TRUE(registry.registerBuiltins());
    alignas(hz::LocalTransform) uint8_t transformStorage[sizeof(hz::LocalTransform)];
    const hz::TypeDesc*                 transformType = registry.getType(hz::SceneTypes::localTransform);
    transformType->construct(transformStorage);
    const hz::LocalTransform* transform = (const hz::LocalTransform*)transformStorage;
    EXPECT_FLOAT_EQ((float)transform->translation.getX(), 0.0f);
    EXPECT_FLOAT_EQ((float)transform->scale.getY(), 1.0f);
    EXPECT_FLOAT_EQ((float)transform->rotation.getW(), 1.0f);
    transformType->destroy(transformStorage);

    alignas(hz::LocalMatrix) uint8_t matrixStorage[sizeof(hz::LocalMatrix)];
    const hz::TypeDesc*              matrixType = registry.getType(hz::SceneTypes::localMatrix);
    matrixType->construct(matrixStorage);
    EXPECT_FLOAT_EQ((float)((const hz::LocalMatrix*)matrixStorage)->value.getElem(3, 3), 1.0f);
    matrixType->destroy(matrixStorage);

    alignas(hz::Camera) uint8_t cameraStorage[sizeof(hz::Camera)];
    const hz::TypeDesc*         cameraType = registry.getType(hz::SceneTypes::camera);
    cameraType->construct(cameraStorage);
    EXPECT_FLOAT_EQ(((const hz::Camera*)cameraStorage)->nearPlane, 0.1f);
    EXPECT_FLOAT_EQ(((const hz::Camera*)cameraStorage)->farPlane, 1000.0f);
    cameraType->destroy(cameraStorage);

    alignas(hz::Light) uint8_t lightStorage[sizeof(hz::Light)];
    const hz::TypeDesc*        lightType = registry.getType(hz::SceneTypes::light);
    lightType->construct(lightStorage);
    const hz::Light* light = (const hz::Light*)lightStorage;
    EXPECT_EQ(light->type, hz::LightType::Point);
    EXPECT_FLOAT_EQ(light->range, 0.0f);
    EXPECT_FLOAT_EQ(light->radius, 0.5f);
    EXPECT_FALSE(light->texture.isValid());
    EXPECT_FALSE(light->normalize);
    lightType->destroy(lightStorage);
}

TEST_F(SceneReflectionTest, OwningComponentCopyAndMoveManageArrayLifetime)
{
    hz::TypeRegistry registry;
    ASSERT_TRUE(registry.registerBuiltins());
    const hz::TypeDesc*               type = registry.getType(hz::SceneTypes::meshRenderer);
    alignas(hz::MeshRenderer) uint8_t originalStorage[sizeof(hz::MeshRenderer)];
    alignas(hz::MeshRenderer) uint8_t copyStorage[sizeof(hz::MeshRenderer)];
    alignas(hz::MeshRenderer) uint8_t movedStorage[sizeof(hz::MeshRenderer)];
    type->construct(originalStorage);
    hz::MeshRenderer* original = (hz::MeshRenderer*)originalStorage;
    original->mesh = { .high = 11, .low = 22 };
    original->overrides.pushBack({ .submesh = 17, .material = { .high = 33, .low = 44 } });
    type->copyConstruct(copyStorage, originalStorage);
    hz::MeshRenderer* copy = (hz::MeshRenderer*)copyStorage;
    EXPECT_EQ(copy->mesh, original->mesh);
    ASSERT_EQ(copy->overrides.size(), 1u);
    EXPECT_NE(copy->overrides.data(), original->overrides.data());
    EXPECT_EQ(copy->overrides[0].material, original->overrides[0].material);
    copy->overrides[0].submesh = 19;
    EXPECT_EQ(original->overrides[0].submesh, 17u);
    const hz::MaterialOverride* data = copy->overrides.data();
    type->moveConstruct(movedStorage, copyStorage);
    EXPECT_TRUE(copy->overrides.empty());
    const hz::MeshRenderer* moved = (const hz::MeshRenderer*)movedStorage;
    EXPECT_EQ(moved->overrides.data(), data);
    EXPECT_EQ(moved->overrides[0].submesh, 19u);
    type->destroy(copyStorage);
    type->destroy(originalStorage);
    type->destroy(movedStorage);
}

TEST_F(SceneReflectionTest, CollectionOperationsHandleGrowthAliasesAndRemoval)
{
    hz::TypeRegistry registry;
    ASSERT_TRUE(registry.registerBuiltins());
    const hz::PropertyDesc*    property = registry.getType(hz::SceneTypes::meshRenderer)->getProperty("overrides");
    const hz::ArrayOperations* ops = property->pArrayOperations;
    hz::MeshRenderer           renderer;
    void*                      array = (uint8_t*)&renderer + property->offset;
    ops->pResize(array, 1);
    renderer.overrides[0] = { .submesh = 17, .material = { .low = 12 } };
    ops->pResize(array, renderer.overrides.capacity());
    const uint32_t count = ops->pCount(array);
    ASSERT_TRUE(ops->pInsert(array, 0, ops->pData(array)));
    EXPECT_EQ(ops->pCount(array), count + 1);
    EXPECT_EQ(renderer.overrides[0].submesh, 17u);
    EXPECT_EQ(renderer.overrides[1].material.low, 12u);
    EXPECT_TRUE(ops->pRemove(array, 0));
    EXPECT_EQ(renderer.overrides[0].submesh, 17u);
    EXPECT_FALSE(ops->pRemove(array, ops->pCount(array)));
    EXPECT_FALSE(ops->pInsert(array, ops->pCount(array) + 1, ops->pData(array)));
    EXPECT_EQ(ops->pCount(array), count);
    ops->pResize(array, 0);
    EXPECT_EQ(ops->pCount(array), 0u);
}
