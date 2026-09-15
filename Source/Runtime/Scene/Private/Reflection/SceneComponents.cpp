/* Copyright (c) 2026 Horizon */

#include "Scene/SceneComponents.h"
#include "Scene/SceneReflection.h"

#include <stddef.h>

#include "Core/IMemory.h"

namespace hz
{
template<typename T>
struct ComponentLifecycle
{
    static void                    construct(void* pDestination) { ::new (pDestination) T{}; }
    static void                    destroy(void* pValue) { ((T*)pValue)->~T(); }
    static void                    copyConstruct(void* pDestination, const void* pSource) { ::new (pDestination) T(*(const T*)pSource); }
    static void                    moveConstruct(void* pDestination, void* pSource) { ::new (pDestination) T(std::move(*(T*)pSource)); }
    static constexpr TypeLifecycle get()
    {
        return { .pConstruct = construct, .pDestroy = destroy, .pCopyConstruct = copyConstruct, .pMoveConstruct = moveConstruct };
    }
};

static uint32_t    overrideCount(const void* pArray) { return ((const Array<MaterialOverride>*)pArray)->size(); }
static const void* overrideData(const void* pArray) { return ((const Array<MaterialOverride>*)pArray)->data(); }
static void        resizeOverrides(void* pArray, uint32_t count) { ((Array<MaterialOverride>*)pArray)->resize(count); }

static bool insertOverride(void* pArray, uint32_t index, const void* pValue)
{
    Array<MaterialOverride>& values = *(Array<MaterialOverride>*)pArray;
    ASSERT(pValue);
    if (index > values.size())
        return false;
    const MaterialOverride value = *(const MaterialOverride*)pValue;
    values.resize(values.size() + 1);
    for (uint32_t i = values.size() - 1; i > index; --i)
        values[i] = values[i - 1];
    values[index] = value;
    return true;
}

static bool removeOverride(void* pArray, uint32_t index)
{
    Array<MaterialOverride>& values = *(Array<MaterialOverride>*)pArray;
    if (index >= values.size())
        return false;
    for (uint32_t i = index; i + 1 < values.size(); ++i)
        values[i] = values[i + 1];
    values.popBack();
    return true;
}

static const ArrayOperations overrideOperations = {
    .size = sizeof(Array<MaterialOverride>),
    .alignment = alignof(Array<MaterialOverride>),
    .pCount = overrideCount,
    .pData = overrideData,
    .pResize = resizeOverrides,
    .pInsert = insertOverride,
    .pRemove = removeOverride,
};

static const PropertyDesc overrideProperties[] = {
    { .id = 1,
      .pName = "submesh",
      .kind = PropertyKind::UnsignedInteger,
      .offset = offsetof(MaterialOverride, submesh),
      .size = sizeof(SubmeshID) },
    { .id = 2,
      .pName = "material",
      .kind = PropertyKind::AssetReference,
      .offset = offsetof(MaterialOverride, material),
      .size = sizeof(AssetID) },
};

static const TypeDesc overrideType = {
    .id = 8,
    .pName = "MaterialOverride",
    .size = sizeof(MaterialOverride),
    .alignment = alignof(MaterialOverride),
    .properties = overrideProperties,
};

static const PropertyDesc identityProperties[] = {
    { .id = 1,
      .pName = "value",
      .kind = PropertyKind::ObjectReference,
      .offset = offsetof(ObjectIdentity, value),
      .size = sizeof(ObjectID),
      .flags = PROPERTY_SERIALIZED | PROPERTY_READ_ONLY },
};
static const PropertyDesc nameProperties[] = {
    { .id = 1, .pName = "value", .kind = PropertyKind::String, .offset = offsetof(Name, value), .size = sizeof(Name::value) },
};
static const PropertyDesc transformProperties[] = {
    { .id = 1,
      .pName = "translation",
      .kind = PropertyKind::Vector3,
      .offset = offsetof(LocalTransform, translation),
      .size = sizeof(Vector3) },
    { .id = 2, .pName = "rotation", .kind = PropertyKind::Quaternion, .offset = offsetof(LocalTransform, rotation), .size = sizeof(Quat) },
    { .id = 3, .pName = "scale", .kind = PropertyKind::Vector3, .offset = offsetof(LocalTransform, scale), .size = sizeof(Vector3) },
};
static const PropertyDesc matrixProperties[] = {
    { .id = 1, .pName = "value", .kind = PropertyKind::Matrix4, .offset = offsetof(LocalMatrix, value), .size = sizeof(Matrix4) },
};
static const PropertyDesc meshProperties[] = {
    { .id = 1, .pName = "mesh", .kind = PropertyKind::AssetReference, .offset = offsetof(MeshRenderer, mesh), .size = sizeof(AssetID) },
    { .id = 2,
      .pName = "overrides",
      .kind = PropertyKind::Array,
      .offset = offsetof(MeshRenderer, overrides),
      .size = sizeof(Array<MaterialOverride>),
      .pElementType = &overrideType,
      .pArrayOperations = &overrideOperations },
};
static const EnumValue lightValues[] = {
    { .pName = "Directional", .value = (int32_t)LightType::Directional },
    { .pName = "Point", .value = (int32_t)LightType::Point },
    { .pName = "Spot", .value = (int32_t)LightType::Spot },
    { .pName = "Rect", .value = (int32_t)LightType::Rect },
    { .pName = "Disk", .value = (int32_t)LightType::Disk },
    { .pName = "Sphere", .value = (int32_t)LightType::Sphere },
    { .pName = "Cylinder", .value = (int32_t)LightType::Cylinder },
    { .pName = "Dome", .value = (int32_t)LightType::Dome },
    { .pName = "Mesh", .value = (int32_t)LightType::Mesh },
};
static const PropertyDesc lightProperties[] = {
    { .id = 1,
      .pName = "type",
      .kind = PropertyKind::Enum,
      .offset = offsetof(Light, type),
      .size = sizeof(LightType),
      .enumValues = lightValues },
    { .id = 2, .pName = "color", .kind = PropertyKind::Color, .offset = offsetof(Light, color), .size = sizeof(Vector3) },
    { .id = 3,
      .pName = "intensity",
      .kind = PropertyKind::Float,
      .offset = offsetof(Light, intensity),
      .size = sizeof(float),
      .step = 0.1 },
    { .id = 4, .pName = "range", .kind = PropertyKind::Float, .offset = offsetof(Light, range), .size = sizeof(float), .step = 0.1 },
    { .id = 5,
      .pName = "innerConeAngle",
      .kind = PropertyKind::Float,
      .offset = offsetof(Light, innerConeAngle),
      .size = sizeof(float),
      .maximum = 1.570796327,
      .step = 0.01 },
    { .id = 6,
      .pName = "outerConeAngle",
      .kind = PropertyKind::Float,
      .offset = offsetof(Light, outerConeAngle),
      .size = sizeof(float),
      .maximum = 1.570796327,
      .step = 0.01 },
    { .id = 7, .pName = "exposure", .kind = PropertyKind::Float, .offset = offsetof(Light, exposure), .size = sizeof(float), .step = 0.1 },
    { .id = 8,
      .pName = "angularDiameter",
      .kind = PropertyKind::Float,
      .offset = offsetof(Light, angularDiameter),
      .size = sizeof(float),
      .maximum = 3.141592654,
      .step = 0.001 },
    { .id = 9, .pName = "width", .kind = PropertyKind::Float, .offset = offsetof(Light, width), .size = sizeof(float), .step = 0.1 },
    { .id = 10, .pName = "height", .kind = PropertyKind::Float, .offset = offsetof(Light, height), .size = sizeof(float), .step = 0.1 },
    { .id = 11, .pName = "radius", .kind = PropertyKind::Float, .offset = offsetof(Light, radius), .size = sizeof(float), .step = 0.1 },
    { .id = 12, .pName = "length", .kind = PropertyKind::Float, .offset = offsetof(Light, length), .size = sizeof(float), .step = 0.1 },
    { .id = 13, .pName = "texture", .kind = PropertyKind::AssetReference, .offset = offsetof(Light, texture), .size = sizeof(AssetID) },
    { .id = 14,
      .pName = "colorTemperature",
      .kind = PropertyKind::Float,
      .offset = offsetof(Light, colorTemperature),
      .size = sizeof(float),
      .minimum = 1000,
      .maximum = 10000,
      .step = 100 },
    { .id = 15,
      .pName = "enableColorTemperature",
      .kind = PropertyKind::Boolean,
      .offset = offsetof(Light, enableColorTemperature),
      .size = sizeof(bool) },
    { .id = 16, .pName = "normalize", .kind = PropertyKind::Boolean, .offset = offsetof(Light, normalize), .size = sizeof(bool) },
    { .id = 17, .pName = "twoSided", .kind = PropertyKind::Boolean, .offset = offsetof(Light, twoSided), .size = sizeof(bool) },
};
static const PropertyDesc cameraProperties[] = {
    { .id = 1,
      .pName = "verticalFov",
      .kind = PropertyKind::Float,
      .offset = offsetof(Camera, verticalFov),
      .size = sizeof(float),
      .minimum = 0.01,
      .maximum = 3.13,
      .step = 0.01 },
    { .id = 2,
      .pName = "nearPlane",
      .kind = PropertyKind::Float,
      .offset = offsetof(Camera, nearPlane),
      .size = sizeof(float),
      .step = 0.01 },
    { .id = 3, .pName = "farPlane", .kind = PropertyKind::Float, .offset = offsetof(Camera, farPlane), .size = sizeof(float), .step = 1.0 },
};

bool TypeRegistry::registerBuiltins()
{
    static const TypeDesc types[] = {
        { .id = SceneTypes::objectIdentity,
          .pName = "ObjectIdentity",
          .size = sizeof(ObjectIdentity),
          .alignment = alignof(ObjectIdentity),
          .properties = identityProperties,
          .triviallyCopyable = __is_trivially_copyable(ObjectIdentity),
          .lifecycle = ComponentLifecycle<ObjectIdentity>::get() },
        { .id = SceneTypes::name,
          .pName = "Name",
          .size = sizeof(Name),
          .alignment = alignof(Name),
          .properties = nameProperties,
          .triviallyCopyable = __is_trivially_copyable(Name),
          .lifecycle = ComponentLifecycle<Name>::get() },
        { .id = SceneTypes::localTransform,
          .pName = "LocalTransform",
          .size = sizeof(LocalTransform),
          .alignment = alignof(LocalTransform),
          .properties = transformProperties,
          .triviallyCopyable = __is_trivially_copyable(LocalTransform),
          .lifecycle = ComponentLifecycle<LocalTransform>::get() },
        { .id = SceneTypes::localMatrix,
          .pName = "LocalMatrix",
          .size = sizeof(LocalMatrix),
          .alignment = alignof(LocalMatrix),
          .properties = matrixProperties,
          .triviallyCopyable = __is_trivially_copyable(LocalMatrix),
          .lifecycle = ComponentLifecycle<LocalMatrix>::get() },
        { .id = SceneTypes::meshRenderer,
          .pName = "MeshRenderer",
          .size = sizeof(MeshRenderer),
          .alignment = alignof(MeshRenderer),
          .properties = meshProperties,
          .triviallyCopyable = false,
          .lifecycle = ComponentLifecycle<MeshRenderer>::get() },
        { .id = SceneTypes::light,
          .pName = "Light",
          .size = sizeof(Light),
          .alignment = alignof(Light),
          .properties = lightProperties,
          .triviallyCopyable = __is_trivially_copyable(Light),
          .lifecycle = ComponentLifecycle<Light>::get() },
        { .id = SceneTypes::camera,
          .pName = "Camera",
          .size = sizeof(Camera),
          .alignment = alignof(Camera),
          .properties = cameraProperties,
          .triviallyCopyable = __is_trivially_copyable(Camera),
          .lifecycle = ComponentLifecycle<Camera>::get() },
    };
    return registerTypes(types);
}
} // namespace hz
