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

TEST_F(SceneReflectionTest, BuiltinsExposeStableTypedProperties)
{
    hz::TypeRegistry registry;
    ASSERT_TRUE(registry.RegisterBuiltins());
    ASSERT_EQ(registry.Size(), 7u);
    const hz::TypeDesc* mesh = registry.GetType(hz::SceneTypes::meshRenderer);
    ASSERT_NE(mesh, nullptr);
    EXPECT_EQ(mesh, registry.GetType("MeshRenderer"));
    EXPECT_EQ(mesh->version, 1u);
    EXPECT_EQ(mesh->Size, sizeof(hz::MeshRenderer));
    const hz::PropertyDesc* property = mesh->GetProperty("mesh");
    ASSERT_NE(property, nullptr);
    EXPECT_EQ(property, mesh->GetProperty(1u));
    EXPECT_EQ(property->kind, hz::PropertyKind::AssetReference);
    EXPECT_EQ(property->offset, offsetof(hz::MeshRenderer, mesh));
    EXPECT_EQ(property->Size, sizeof(hz::AssetID));
    EXPECT_EQ(mesh->GetProperty(99u), nullptr);
    EXPECT_EQ(mesh->GetProperty("missing"), nullptr);
    EXPECT_EQ(registry.GetType(UINT64_C(999)), nullptr);
    EXPECT_EQ(registry.GetType("missing"), nullptr);
    for (uint32_t i = 0; i < registry.Size(); ++i)
        EXPECT_EQ(registry.GetTypeAt(i), registry.GetType(registry.GetTypeAt(i)->id));

    const hz::PropertyDesc* lightType = registry.GetType(hz::SceneTypes::light)->GetProperty("type");
    ASSERT_NE(lightType, nullptr);
    ASSERT_EQ(lightType->enumValues.Count, 9u);
    EXPECT_STREQ(lightType->enumValues.Data[2].Name, "Spot");
    EXPECT_EQ(lightType->enumValues.Data[2].value, (int32_t)hz::LightType::Spot);
    const hz::PropertyDesc* emissionTexture = registry.GetType(hz::SceneTypes::light)->GetProperty("texture");
    ASSERT_NE(emissionTexture, nullptr);
    EXPECT_EQ(emissionTexture->kind, hz::PropertyKind::AssetReference);
    EXPECT_EQ(emissionTexture->offset, offsetof(hz::Light, texture));
    EXPECT_EQ(registry.GetType(hz::SceneTypes::name)->GetProperty("value")->Size, 128u);
    EXPECT_TRUE(registry.GetType(hz::SceneTypes::entityIdentity)->GetProperty("value")->Flags & hz::PROPERTY_READ_ONLY);
}

TEST_F(SceneReflectionTest, RegistrationRejectsDuplicatesWithoutPublishingPartialBatches)
{
    hz::TypeRegistry registry;
    ASSERT_TRUE(registry.RegisterBuiltins());
    const hz::TypeDesc types[] = {
        { .id = 100, .Name = "NewType", .Size = 4, .alignment = 4 },
        { .id = hz::SceneTypes::camera, .Name = "DuplicateID", .Size = 4, .alignment = 4 },
    };
    EXPECT_FALSE(registry.RegisterTypes(types));
    EXPECT_EQ(registry.Size(), 7u);
    EXPECT_EQ(registry.GetType(UINT64_C(100)), nullptr);
    const hz::TypeDesc duplicateName = { .id = 101, .Name = "Camera", .Size = 4 };
    EXPECT_FALSE(registry.RegisterTypes({ &duplicateName, 1 }));
    EXPECT_FALSE(registry.RegisterBuiltins());
    EXPECT_EQ(registry.Size(), 7u);

    const hz::TypeDesc repeated[] = {
        { .id = 100, .Name = "Same", .Size = 4 },
        { .id = 101, .Name = "Same", .Size = 4 },
    };
    EXPECT_FALSE(registry.RegisterTypes(repeated));
    EXPECT_EQ(registry.Size(), 7u);
    EXPECT_TRUE(registry.RegisterTypes({}));
}

TEST_F(SceneReflectionTest, RejectsInvalidPropertyLayoutsAndMetadata)
{
    hz::TypeRegistry registry;
    hz::PropertyDesc property = { .id = 1, .Name = "value", .kind = hz::PropertyKind::Float, .Size = 4 };
    hz::TypeDesc     Type = { .id = 100, .Name = "Invalid", .Size = 4, .alignment = 4, .properties = { &property, 1 } };
    property.offset = 1;
    EXPECT_FALSE(registry.RegisterTypes({ &Type, 1 }));
    property.offset = UINT32_MAX;
    EXPECT_FALSE(registry.RegisterTypes({ &Type, 1 }));
    property.offset = 0;
    property.Size = 3;
    EXPECT_FALSE(registry.RegisterTypes({ &Type, 1 }));
    property.Size = 4;
    property.kind = (hz::PropertyKind)255;
    EXPECT_FALSE(registry.RegisterTypes({ &Type, 1 }));
    property.kind = hz::PropertyKind::Float;
    property.Flags = hz::PROPERTY_SERIALIZED | hz::PROPERTY_TRANSIENT;
    EXPECT_FALSE(registry.RegisterTypes({ &Type, 1 }));
    property.Flags = hz::PROPERTY_SERIALIZED;
    Type.alignment = 3;
    EXPECT_FALSE(registry.RegisterTypes({ &Type, 1 }));
    Type.alignment = 4;
    Type.properties = { nullptr, 1 };
    EXPECT_FALSE(registry.RegisterTypes({ &Type, 1 }));
    Type.properties = { &property, 1 };
    Type.version = 0;
    EXPECT_FALSE(registry.RegisterTypes({ &Type, 1 }));
    EXPECT_EQ(registry.Size(), 0u);
}

TEST_F(SceneReflectionTest, RejectsDuplicatePropertiesAndIncompleteEnumOrArrayMetadata)
{
    hz::TypeRegistry registry;
    hz::PropertyDesc properties[] = {
        { .id = 1, .Name = "A", .kind = hz::PropertyKind::UnsignedInteger, .Size = 4 },
        { .id = 1, .Name = "B", .kind = hz::PropertyKind::UnsignedInteger, .offset = 4, .Size = 4 },
    };
    hz::TypeDesc Type = { .id = 100, .Name = "Invalid", .Size = 8, .alignment = 4, .properties = properties };
    EXPECT_FALSE(registry.RegisterTypes({ &Type, 1 }));
    properties[1].id = 2;
    properties[1].Name = "A";
    EXPECT_FALSE(registry.RegisterTypes({ &Type, 1 }));
    Type.properties = { properties, 1 };
    properties[0].kind = hz::PropertyKind::Enum;
    EXPECT_FALSE(registry.RegisterTypes({ &Type, 1 }));
    const hz::EnumValue duplicateValues[] = { { .Name = "A", .value = 1 }, { .Name = "B", .value = 1 } };
    properties[0].enumValues = duplicateValues;
    EXPECT_FALSE(registry.RegisterTypes({ &Type, 1 }));

    ASSERT_TRUE(registry.RegisterBuiltins());
    hz::TypeDesc incomplete = *registry.GetType(hz::SceneTypes::meshRenderer);
    incomplete.id = 101;
    incomplete.Name = "Incomplete";
    incomplete.lifecycle.pDestroy = nullptr;
    EXPECT_FALSE(registry.RegisterTypes({ &incomplete, 1 }));
    incomplete = *registry.GetType(hz::SceneTypes::meshRenderer);
    incomplete.id = 101;
    incomplete.Name = "Incomplete";
    hz::PropertyDesc array = *incomplete.GetProperty("overrides");
    incomplete.properties = { &array, 1 };
    array.pElementType = nullptr;
    EXPECT_FALSE(registry.RegisterTypes({ &incomplete, 1 }));
    array = *registry.GetType(hz::SceneTypes::meshRenderer)->GetProperty("overrides");
    hz::TypeDesc element = *array.pElementType;
    array.pElementType = &element;
    element.triviallyCopyable = false;
    EXPECT_FALSE(registry.RegisterTypes({ &incomplete, 1 }));
    element.triviallyCopyable = true;
    element.properties = {};
    EXPECT_FALSE(registry.RegisterTypes({ &incomplete, 1 }));
    EXPECT_EQ(registry.Size(), 7u);
}

TEST_F(SceneReflectionTest, ConstructionPreservesComponentDefaults)
{
    hz::TypeRegistry registry;
    ASSERT_TRUE(registry.RegisterBuiltins());
    alignas(hz::LocalTransform) uint8_t transformStorage[sizeof(hz::LocalTransform)];
    const hz::TypeDesc*                 transformType = registry.GetType(hz::SceneTypes::localTransform);
    transformType->Construct(transformStorage);
    const hz::LocalTransform* transform = (const hz::LocalTransform*)transformStorage;
    EXPECT_FLOAT_EQ((float)transform->translation.getX(), 0.0f);
    EXPECT_FLOAT_EQ((float)transform->scale.getY(), 1.0f);
    EXPECT_FLOAT_EQ((float)transform->rotation.getW(), 1.0f);
    transformType->Destroy(transformStorage);

    alignas(Matrix4) uint8_t         matrixStorage[sizeof(Matrix4)];
    const hz::TypeDesc*              matrixType = registry.GetType(hz::SceneTypes::localMatrix);
    matrixType->Construct(matrixStorage);
    EXPECT_FLOAT_EQ((float)((const Matrix4*)matrixStorage)->getElem(3, 3), 1.0f);
    matrixType->Destroy(matrixStorage);

    alignas(hz::Camera) uint8_t cameraStorage[sizeof(hz::Camera)];
    const hz::TypeDesc*         cameraType = registry.GetType(hz::SceneTypes::camera);
    cameraType->Construct(cameraStorage);
    EXPECT_FLOAT_EQ(((const hz::Camera*)cameraStorage)->nearPlane, 0.1f);
    EXPECT_FLOAT_EQ(((const hz::Camera*)cameraStorage)->farPlane, 1000.0f);
    cameraType->Destroy(cameraStorage);

    alignas(hz::Light) uint8_t lightStorage[sizeof(hz::Light)];
    const hz::TypeDesc*        lightType = registry.GetType(hz::SceneTypes::light);
    lightType->Construct(lightStorage);
    const hz::Light* light = (const hz::Light*)lightStorage;
    EXPECT_EQ(light->Type, hz::LightType::Point);
    EXPECT_FLOAT_EQ(light->range, 0.0f);
    EXPECT_FLOAT_EQ(light->radius, 0.5f);
    EXPECT_FALSE(light->texture.IsValid());
    EXPECT_FALSE(light->normalize);
    lightType->Destroy(lightStorage);
}

TEST_F(SceneReflectionTest, OwningComponentCopyAndMoveManageArrayLifetime)
{
    hz::TypeRegistry registry;
    ASSERT_TRUE(registry.RegisterBuiltins());
    const hz::TypeDesc*               Type = registry.GetType(hz::SceneTypes::meshRenderer);
    alignas(hz::MeshRenderer) uint8_t originalStorage[sizeof(hz::MeshRenderer)];
    alignas(hz::MeshRenderer) uint8_t copyStorage[sizeof(hz::MeshRenderer)];
    alignas(hz::MeshRenderer) uint8_t movedStorage[sizeof(hz::MeshRenderer)];
    Type->Construct(originalStorage);
    hz::MeshRenderer* original = (hz::MeshRenderer*)originalStorage;
    original->mesh = { .high = 11, .low = 22 };
    original->overrides.PushBack({ .submesh = 17, .material = { .high = 33, .low = 44 } });
    Type->CopyConstruct(copyStorage, originalStorage);
    hz::MeshRenderer* copy = (hz::MeshRenderer*)copyStorage;
    EXPECT_EQ(copy->mesh, original->mesh);
    ASSERT_EQ(copy->overrides.Size(), 1u);
    EXPECT_NE(copy->overrides.GetData(), original->overrides.GetData());
    EXPECT_EQ(copy->overrides[0].material, original->overrides[0].material);
    copy->overrides[0].submesh = 19;
    EXPECT_EQ(original->overrides[0].submesh, 17u);
    const hz::MaterialOverride* data = copy->overrides.GetData();
    Type->MoveConstruct(movedStorage, copyStorage);
    EXPECT_TRUE(copy->overrides.Empty());
    const hz::MeshRenderer* moved = (const hz::MeshRenderer*)movedStorage;
    EXPECT_EQ(moved->overrides.GetData(), data);
    EXPECT_EQ(moved->overrides[0].submesh, 19u);
    Type->Destroy(copyStorage);
    Type->Destroy(originalStorage);
    Type->Destroy(movedStorage);
}

TEST_F(SceneReflectionTest, CollectionOperationsHandleGrowthAliasesAndRemoval)
{
    hz::TypeRegistry registry;
    ASSERT_TRUE(registry.RegisterBuiltins());
    const hz::PropertyDesc*    property = registry.GetType(hz::SceneTypes::meshRenderer)->GetProperty("overrides");
    const hz::ArrayOperations* ops = property->pArrayOperations;
    hz::MeshRenderer           renderer;
    void*                      array = (uint8_t*)&renderer + property->offset;
    ops->pResize(array, 1);
    renderer.overrides[0] = { .submesh = 17, .material = { .low = 12 } };
    ops->pResize(array, renderer.overrides.Capacity());
    const uint32_t Count = ops->pCount(array);
    ASSERT_TRUE(ops->pInsert(array, 0, ops->Data(array)));
    EXPECT_EQ(ops->pCount(array), Count + 1);
    EXPECT_EQ(renderer.overrides[0].submesh, 17u);
    EXPECT_EQ(renderer.overrides[1].material.low, 12u);
    EXPECT_TRUE(ops->pRemove(array, 0));
    EXPECT_EQ(renderer.overrides[0].submesh, 17u);
    EXPECT_FALSE(ops->pRemove(array, ops->pCount(array)));
    EXPECT_FALSE(ops->pInsert(array, ops->pCount(array) + 1, ops->Data(array)));
    EXPECT_EQ(ops->pCount(array), Count);
    ops->pResize(array, 0);
    EXPECT_EQ(ops->pCount(array), 0u);
}
