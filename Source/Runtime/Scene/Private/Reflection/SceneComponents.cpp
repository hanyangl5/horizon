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

template<>
void ComponentLifecycle<Matrix4>::construct(void* pDestination)
{
    ::new (pDestination) Matrix4(Matrix4::identity());
}

static uint32_t    overrideCount(const void* pArray) { return ((const Array<MaterialOverride>*)pArray)->Size(); }
static const void* overrideData(const void* pArray) { return ((const Array<MaterialOverride>*)pArray)->GetData(); }
static void        resizeOverrides(void* pArray, uint32_t Count) { ((Array<MaterialOverride>*)pArray)->resize(Count); }

static bool insertOverride(void* pArray, uint32_t index, const void* pValue)
{
    Array<MaterialOverride>& values = *(Array<MaterialOverride>*)pArray;
    ASSERT(pValue);
    if (index > values.Size())
        return false;
    const MaterialOverride value = *(const MaterialOverride*)pValue;
    values.resize(values.Size() + 1);
    for (uint32_t i = values.Size() - 1; i > index; --i)
        values[i] = values[i - 1];
    values[index] = value;
    return true;
}

static bool removeOverride(void* pArray, uint32_t index)
{
    Array<MaterialOverride>& values = *(Array<MaterialOverride>*)pArray;
    if (index >= values.Size())
        return false;
    for (uint32_t i = index; i + 1 < values.Size(); ++i)
        values[i] = values[i + 1];
    values.PopBack();
    return true;
}

static const ArrayOperations overrideOperations = {
    .Size = sizeof(Array<MaterialOverride>),
    .alignment = alignof(Array<MaterialOverride>),
    .pCount = overrideCount,
    .Data = overrideData,
    .pResize = resizeOverrides,
    .pInsert = insertOverride,
    .pRemove = removeOverride,
};

static const PropertyDesc overrideProperties[] = {
    { .id = 1,
      .pName = "submesh",
      .kind = PropertyKind::UnsignedInteger,
      .offset = offsetof(MaterialOverride, submesh),
      .Size = sizeof(SubmeshID) },
    { .id = 2,
      .pName = "material",
      .kind = PropertyKind::AssetReference,
      .offset = offsetof(MaterialOverride, material),
      .Size = sizeof(AssetID) },
};

static const TypeDesc overrideType = {
    .id = 8,
    .pName = "MaterialOverride",
    .Size = sizeof(MaterialOverride),
    .alignment = alignof(MaterialOverride),
    .properties = overrideProperties,
};

static const PropertyDesc identityProperties[] = {
    { .id = 1,
      .pName = "value",
      .kind = PropertyKind::EntityReference,
      .offset = offsetof(EntityIdentity, value),
      .Size = sizeof(Guid),
      .flags = PROPERTY_SERIALIZED | PROPERTY_READ_ONLY },
};
static const PropertyDesc nameProperties[] = {
    { .id = 1, .pName = "value", .kind = PropertyKind::String, .offset = offsetof(Name, value), .Size = sizeof(Name::value) },
};
static const PropertyDesc transformProperties[] = {
    { .id = 1,
      .pName = "translation",
      .kind = PropertyKind::Vector3,
      .offset = offsetof(LocalTransform, translation),
      .Size = sizeof(Vector3) },
    { .id = 2, .pName = "rotation", .kind = PropertyKind::Quaternion, .offset = offsetof(LocalTransform, rotation), .Size = sizeof(Quat) },
    { .id = 3, .pName = "scale", .kind = PropertyKind::Vector3, .offset = offsetof(LocalTransform, scale), .Size = sizeof(Vector3) },
};
static const PropertyDesc matrixProperties[] = {
    { .id = 1, .pName = "value", .kind = PropertyKind::Matrix4, .offset = 0, .Size = sizeof(Matrix4) },
};
static const PropertyDesc meshProperties[] = {
    { .id = 1, .pName = "mesh", .kind = PropertyKind::AssetReference, .offset = offsetof(MeshRenderer, mesh), .Size = sizeof(AssetID) },
    { .id = 2,
      .pName = "overrides",
      .kind = PropertyKind::Array,
      .offset = offsetof(MeshRenderer, overrides),
      .Size = sizeof(Array<MaterialOverride>),
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
      .Size = sizeof(LightType),
      .enumValues = lightValues },
    { .id = 2, .pName = "color", .kind = PropertyKind::Color, .offset = offsetof(Light, color), .Size = sizeof(Vector3) },
    { .id = 3,
      .pName = "intensity",
      .kind = PropertyKind::Float,
      .offset = offsetof(Light, intensity),
      .Size = sizeof(float),
      .step = 0.1 },
    { .id = 4, .pName = "range", .kind = PropertyKind::Float, .offset = offsetof(Light, range), .Size = sizeof(float), .step = 0.1 },
    { .id = 5,
      .pName = "innerConeAngle",
      .kind = PropertyKind::Float,
      .offset = offsetof(Light, innerConeAngle),
      .Size = sizeof(float),
      .maximum = 1.570796327,
      .step = 0.01 },
    { .id = 6,
      .pName = "outerConeAngle",
      .kind = PropertyKind::Float,
      .offset = offsetof(Light, outerConeAngle),
      .Size = sizeof(float),
      .maximum = 1.570796327,
      .step = 0.01 },
    { .id = 7, .pName = "exposure", .kind = PropertyKind::Float, .offset = offsetof(Light, exposure), .Size = sizeof(float), .step = 0.1 },
    { .id = 8,
      .pName = "angularDiameter",
      .kind = PropertyKind::Float,
      .offset = offsetof(Light, angularDiameter),
      .Size = sizeof(float),
      .maximum = 3.141592654,
      .step = 0.001 },
    { .id = 9, .pName = "width", .kind = PropertyKind::Float, .offset = offsetof(Light, width), .Size = sizeof(float), .step = 0.1 },
    { .id = 10, .pName = "height", .kind = PropertyKind::Float, .offset = offsetof(Light, height), .Size = sizeof(float), .step = 0.1 },
    { .id = 11, .pName = "radius", .kind = PropertyKind::Float, .offset = offsetof(Light, radius), .Size = sizeof(float), .step = 0.1 },
    { .id = 12, .pName = "length", .kind = PropertyKind::Float, .offset = offsetof(Light, length), .Size = sizeof(float), .step = 0.1 },
    { .id = 13, .pName = "texture", .kind = PropertyKind::AssetReference, .offset = offsetof(Light, texture), .Size = sizeof(AssetID) },
    { .id = 14,
      .pName = "colorTemperature",
      .kind = PropertyKind::Float,
      .offset = offsetof(Light, colorTemperature),
      .Size = sizeof(float),
      .minimum = 1000,
      .maximum = 10000,
      .step = 100 },
    { .id = 15,
      .pName = "enableColorTemperature",
      .kind = PropertyKind::Boolean,
      .offset = offsetof(Light, enableColorTemperature),
      .Size = sizeof(bool) },
    { .id = 16, .pName = "normalize", .kind = PropertyKind::Boolean, .offset = offsetof(Light, normalize), .Size = sizeof(bool) },
    { .id = 17, .pName = "twoSided", .kind = PropertyKind::Boolean, .offset = offsetof(Light, twoSided), .Size = sizeof(bool) },
};
static const PropertyDesc cameraProperties[] = {
    { .id = 1,
      .pName = "verticalFov",
      .kind = PropertyKind::Float,
      .offset = offsetof(Camera, verticalFov),
      .Size = sizeof(float),
      .minimum = 0.01,
      .maximum = 3.13,
      .step = 0.01 },
    { .id = 2,
      .pName = "nearPlane",
      .kind = PropertyKind::Float,
      .offset = offsetof(Camera, nearPlane),
      .Size = sizeof(float),
      .step = 0.01 },
    { .id = 3, .pName = "farPlane", .kind = PropertyKind::Float, .offset = offsetof(Camera, farPlane), .Size = sizeof(float), .step = 1.0 },
};

bool TypeRegistry::registerBuiltins()
{
    static const TypeDesc types[] = {
        { .id = SceneTypes::entityIdentity,
          .pName = "EntityIdentity",
          .Size = sizeof(EntityIdentity),
          .alignment = alignof(EntityIdentity),
          .properties = identityProperties,
          .triviallyCopyable = __is_trivially_copyable(EntityIdentity),
          .lifecycle = ComponentLifecycle<EntityIdentity>::get() },
        { .id = SceneTypes::name,
          .pName = "Name",
          .Size = sizeof(Name),
          .alignment = alignof(Name),
          .properties = nameProperties,
          .triviallyCopyable = __is_trivially_copyable(Name),
          .lifecycle = ComponentLifecycle<Name>::get() },
        { .id = SceneTypes::localTransform,
          .pName = "LocalTransform",
          .Size = sizeof(LocalTransform),
          .alignment = alignof(LocalTransform),
          .properties = transformProperties,
          .triviallyCopyable = __is_trivially_copyable(LocalTransform),
          .lifecycle = ComponentLifecycle<LocalTransform>::get() },
        { .id = SceneTypes::localMatrix,
          .pName = "Matrix4",
          .Size = sizeof(Matrix4),
          .alignment = alignof(Matrix4),
          .properties = matrixProperties,
          .triviallyCopyable = __is_trivially_copyable(Matrix4),
          .lifecycle = ComponentLifecycle<Matrix4>::get() },
        { .id = SceneTypes::meshRenderer,
          .pName = "MeshRenderer",
          .Size = sizeof(MeshRenderer),
          .alignment = alignof(MeshRenderer),
          .properties = meshProperties,
          .triviallyCopyable = false,
          .lifecycle = ComponentLifecycle<MeshRenderer>::get() },
        { .id = SceneTypes::light,
          .pName = "Light",
          .Size = sizeof(Light),
          .alignment = alignof(Light),
          .properties = lightProperties,
          .triviallyCopyable = __is_trivially_copyable(Light),
          .lifecycle = ComponentLifecycle<Light>::get() },
        { .id = SceneTypes::camera,
          .pName = "Camera",
          .Size = sizeof(Camera),
          .alignment = alignof(Camera),
          .properties = cameraProperties,
          .triviallyCopyable = __is_trivially_copyable(Camera),
          .lifecycle = ComponentLifecycle<Camera>::get() },
    };
    return registerTypes(types);
}
} // namespace hz
