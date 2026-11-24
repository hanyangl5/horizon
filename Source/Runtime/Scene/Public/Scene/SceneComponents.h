/* Copyright (c) 2026 Horizon */
#pragma once

#include "Core/IContainer.h"
#include "Core/IMath.h"
#include "Scene/SceneID.h"

namespace hz
{
namespace SceneTypes
{
// Persistent schema IDs; renaming a Type must not change its ID.
constexpr uint64_t entityIdentity = 1;
constexpr uint64_t name = 2;
constexpr uint64_t localTransform = 3;
constexpr uint64_t localMatrix = 4;
constexpr uint64_t meshRenderer = 5;
constexpr uint64_t light = 6;
constexpr uint64_t camera = 7;
// 8 is the MaterialOverride element type.
constexpr uint64_t skeleton = 9;
constexpr uint64_t animationPlayer = 10;
constexpr uint64_t skinnedMesh = 11;
} // namespace SceneTypes

using SubmeshID = uint64_t;

struct EntityIdentity
{
    Guid value;
};

struct Name
{
    char value[128] = {};
};

struct LocalTransform
{
    Vector3 translation = Vector3(0.0f);
    Quat    rotation = Quat::identity();
    Vector3 scale = Vector3(1.0f);
};

// Derived hierarchy data, not a reflected authoring component.
struct WorldTransform
{
    Matrix4 current = Matrix4::identity();
};

struct MaterialOverride
{
    SubmeshID submesh = 0;
    AssetID   material;
};

struct MeshRenderer
{
    AssetID                 mesh;
    Array<MaterialOverride> overrides;
};

enum class LightType : int32_t
{
    Directional = 0,
    Point = 1,
    Spot = 2,
    Rect = 3,
    Disk = 4,
    Sphere = 5,
    Cylinder = 6,
    Dome = 7,
    Mesh = 8,
};

// Authoring data; position and orientation come from the entity transform.
// Lengths are local-space meters, angles are radians, color is scene-linear RGB.
struct Light
{
    LightType Type = LightType::Point;
    Vector3   color = Vector3(1.0f);
    // Directional: perpendicular illuminance in lux. Point/Spot: W/sr. Dome/area: W/(m^2 sr).
    // Normalized area lights instead specify total power in W, before texture modulation.
    float     intensity = 1.0f;
    float     range = 0.0f;                  // Optional finite-light cutoff in world meters; 0 means no cutoff.
    float     innerConeAngle = 0.523598776f; // Spot cone half-angles around local -Z.
    float     outerConeAngle = 0.785398163f;
    float     exposure = 0.0f;        // Emission multiplier: 2^exposure.
    float     angularDiameter = 0.0f; // Directional: full cone angle; 0 is a delta light.
    float     Width = 1.0f;           // Rect spans local X/Y and emits along -Z.
    float     Height = 1.0f;
    float     radius = 0.5f;              // Disk (XY, -Z), Sphere, or Cylinder.
    float     length = 1.0f;              // Cylinder along local Y, emitting from its side without end caps.
    AssetID   texture;                    // Linear emission map; Dome uses latitude-longitude, Mesh uses mesh UVs.
    float     colorTemperature = 6500.0f; // Kelvin; used only when enabled.
    bool      enableColorTemperature = false;
    bool      normalize = false; // Area lights: preserve untextured power as emitting area changes.
    bool      twoSided = false;  // Area lights: emit on both sides, sharing power when normalized.
    // Mesh uses the MeshRenderer on the same entity as its emitting surface.
};

// The entity is the root of a skeleton: its world transform places the skeleton's model space, and skinned meshes
// attach as its direct children.
struct Skeleton
{
    AssetID asset;
};

// Plays a clip of the Skeleton on the same entity.
struct AnimationPlayer
{
    AssetID clip;
    float   time = 0.0f; // Seconds into the clip.
    float   speed = 1.0f;
    bool    loop = true;
    bool    playing = true;
};

// Deforms the MeshRenderer on the same entity with a skin of the Skeleton on the parent entity. The mesh must carry
// joint and weight streams.
struct SkinnedMesh
{
    uint32_t skin = 0;
};

struct Camera
{
    float verticalFov = 1.047197551f;
    float nearPlane = 0.1f;
    float farPlane = 1000.0f;
};
} // namespace hz
