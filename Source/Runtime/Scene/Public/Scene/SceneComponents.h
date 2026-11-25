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
// 6 was the retired generic light component.
constexpr uint64_t camera = 7;
// 8 is the MaterialOverride element type.
constexpr uint64_t skeleton = 9;
constexpr uint64_t animationPlayer = 10;
constexpr uint64_t skinnedMesh = 11;
constexpr uint64_t directionalLight = 12;
constexpr uint64_t pointLight = 13;
constexpr uint64_t spotLight = 14;
constexpr uint64_t rectLight = 15;
constexpr uint64_t skyLight = 16;
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
    Directional,
    Point,
    Spot,
    Rect,
    Sky,
};

constexpr uint64_t LightComponentTypes[] = { SceneTypes::directionalLight, SceneTypes::pointLight, SceneTypes::spotLight,
                                             SceneTypes::rectLight, SceneTypes::skyLight };

enum class LightUnits : int32_t
{
    Unitless,
    Candelas,
    Lumens,
    EV,
    Nits,
};

enum class RayTracedShadow : int32_t
{
    Disabled,
    UseProjectSetting,
    Enabled,
};

enum class MegaLightsMethod : int32_t
{
    Default,
    RayTracing,
    VirtualShadowMap,
};

enum class SkyLightSourceType : int32_t
{
    CapturedScene,
    SpecifiedCubemap,
};

enum class OcclusionCombine : int32_t
{
    Minimum,
    Multiply,
};

constexpr bool IsLocalLight(LightType Type) { return Type == LightType::Point || Type == LightType::Spot || Type == LightType::Rect; }

struct LightingChannelsData
{
    bool bChannel0 = true;
    bool bChannel1 = false;
    bool bChannel2 = false;
};

struct ViewLightingChannelsData
{
    bool bViewChannel0 = true;
    bool bViewChannel1 = false;
    bool bViewChannel2 = false;
    bool bViewChannel3 = false;
    bool bViewChannel4 = false;
};

struct LightmassSettingsData
{
    float IndirectLightingSaturation = 1;
    float ShadowExponent = 2;
    bool  bUseAreaShadowsForStationaryLight = false;
};

struct DirectionalLightmassSettingsData
{
    float IndirectLightingSaturation = 1;
    float ShadowExponent = 2;
    bool  bUseAreaShadowsForStationaryLight = false;
    float LightSourceAngle = 1;
};

// TODO: inherit is better
// UE authoring fields use centimeters and degrees unless the field specifies another unit.
// Colors are scene-linear. Entity transforms and renderer distances remain in meters.
struct LightComponentBase
{
    float           Intensity = PI;
    Vector3         LightColor = Vector3(1);
    bool            bAffectsWorld = true;
    bool            CastShadows = true;
    bool            CastStaticShadows = true;
    bool            CastDynamicShadows = true;
    bool            bAffectTranslucentLighting = true;
    bool            bTransmission = false;
    bool            bCastVolumetricShadow = false;
    bool            bCastDeepShadow = false;
    RayTracedShadow CastRaytracedShadow = RayTracedShadow::UseProjectSetting;
    bool            bAffectReflection = true;
    bool            bAffectGlobalIllumination = true;
    float           DeepShadowLayerDistribution = 0.5f;
    float           IndirectLightingIntensity = 1.0f;
    float           VolumetricScatteringIntensity = 1.0f;
    int32_t         SamplesPerPixel = 1;
};

struct LightComponent
{
    LightComponentBase       Base;
    float                    Temperature = 6500.0f;
    float                    MaxDrawDistance = 0.0f;
    float                    MaxDistanceFadeRange = 0.0f;
    bool                     bUseTemperature = false;
    float                    SpecularScale = 1.0f;
    float                    DiffuseScale = 1.0f;
    float                    ShadowResolutionScale = 1.0f;
    float                    ShadowBias = 0.5f;
    float                    ShadowSlopeBias = 0.5f;
    float                    ShadowSharpen = 0.0f;
    float                    ContactShadowLength = 0.0f;
    bool                     ContactShadowLengthInWS = false;
    float                    ContactShadowCastingIntensity = 1.0f;
    float                    ContactShadowNonCastingIntensity = 0.0f;
    bool                     CastTranslucentShadows = false;
    bool                     bCastShadowsFromCinematicObjectsOnly = false;
    bool                     bForceCachedShadowsForMovablePrimitives = false;
    bool                     bAllowMegaLights = true;
    MegaLightsMethod         MegaLightsShadowMethod = MegaLightsMethod::Default;
    bool                     bOverrideRayEndBias = false;
    float                    RayEndBias = 0.0f;
    LightingChannelsData     LightingChannels;
    ViewLightingChannelsData ViewLightingChannels;
    AssetID                  LightFunctionMaterial = {};
    Vector3                  LightFunctionScale = Vector3(1024);
    AssetID                  IESTexture = {};
    bool                     bUseIESBrightness = false;
    float                    IESBrightnessScale = 1.0f;
    float                    LightFunctionFadeDistance = 100000.0f;
    float                    DisabledBrightness = 0.5f;
    bool                     bEnableLightShaftBloom = false;
    float                    BloomScale = .2f;
    float                    BloomThreshold = 0;
    float                    BloomMaxBrightness = 100.0f;
    Vector3                  BloomTint = Vector3(1);
    bool                     bUseRayTracedDistanceFieldShadows = false;
    float                    RayStartOffsetDepthScale = .003f;
};

struct LocalLightComponent
{
    LightComponent        Common = { .Base = { .Intensity = 5000.0f } };
    LightUnits            IntensityUnits = LightUnits::Unitless;
    float                 InverseExposureBlend = 0;
    float                 AttenuationRadius = 1000;
    LightmassSettingsData LightmassSettings;
};

struct PointLightComponent
{
    LocalLightComponent Local;
    bool                bUseInverseSquaredFalloff = true;
    float               LightFalloffExponent = 8.0f;
    float               SourceRadius = 0.0f;
    float               SoftSourceRadius = 0.0f;
    float               SourceLength = 0.0f;
};

struct SpotLightComponent
{
    PointLightComponent Point;
    float               InnerConeAngle = 0.0f;
    float               OuterConeAngle = 44.0f;
};

struct RectLightComponent
{
    LocalLightComponent Local;
    float               SourceWidth = 64.0f;
    float               SourceHeight = 64.0f;
    float               BarnDoorAngle = 88.0f;
    float               BarnDoorLength = 20.0f;
    float               LightFunctionConeAngle = 0.0f;
    AssetID             SourceTexture = {};
    float2              SourceTextureScale = float2(1.0f);
    float2              SourceTextureOffset = float2(0.0f);
};

struct DirectionalLightComponent
{
    LightComponent                   Common = { .Base = { .Intensity = 10.0f, .bCastVolumetricShadow = true },
                                                .CastTranslucentShadows = true,
                                                .bUseRayTracedDistanceFieldShadows = true };
    float                            ShadowCascadeBiasDistribution = 1;
    bool                             bEnableLightShaftOcclusion = false;
    float                            OcclusionMaskDarkness = 0.05f;
    float                            OcclusionDepthRange = 100000.0f;
    Vector3                          LightShaftOverrideDirection = Vector3(0.0f);
    float                            DynamicShadowDistanceMovableLight = 40000.0f;
    float                            DynamicShadowDistanceStationaryLight = 0.0f;
    int32_t                          DynamicShadowCascades = 4;
    float                            CascadeDistributionExponent = 3.0f;
    float                            CascadeTransitionFraction = 0.1f;
    float                            ShadowDistanceFadeoutFraction = 0.1f;
    bool                             bUseInsetShadowsForMovableObjects = true;
    int32_t                          FarShadowCascadeCount = 0;
    float                            FarShadowDistance = 300000.0f;
    float                            DistanceFieldShadowDistance = 51200.0f;
    int32_t                          ForwardShadingPriority = 0;
    float                            LightSourceAngle = 0.5357f;
    float                            LightSourceSoftAngle = 0.0f;
    float                            ShadowSourceAngleFactor = 1.0f;
    float                            TraceDistance = 10000.0f;
    bool                             bAtmosphereSunLight = true;
    int32_t                          AtmosphereSunLightIndex = 0;
    Vector4                          AtmosphereSunDiskColorScale = Vector4(1.0f);
    bool                             bPerPixelAtmosphereTransmittance = false;
    bool                             bCastShadowsOnClouds = false;
    bool                             bCastShadowsOnAtmosphere = false;
    bool                             bCastCloudShadows = false;
    float                            CloudShadowStrength = 1.0f;
    float                            CloudShadowOnAtmosphereStrength = 1.0f;
    float                            CloudShadowOnSurfaceStrength = 1.0f;
    float                            CloudShadowDepthBias = 0.0f;
    float                            CloudShadowExtent = 150.0f;
    float                            CloudShadowMapResolutionScale = 1.0f;
    float                            CloudShadowRaySampleCountScale = 1.0f;
    Vector4                          CloudScatteredLuminanceScale = Vector4(1.0f);
    DirectionalLightmassSettingsData LightmassSettings;
    bool                             bCastModulatedShadows = false;
    Vector3                          ModulatedShadowColor = Vector3(0.21586f);
    float                            ShadowAmount = 1.0f;
};

struct SkyLightComponent
{
    LightComponentBase Base = { .Intensity = 1.0f,
                                .bAffectTranslucentLighting = false,
                                .bCastVolumetricShadow = true,
                                .SamplesPerPixel = 4 };
    bool               bRealTimeCapture = false;
    SkyLightSourceType SourceType = SkyLightSourceType::CapturedScene;
    AssetID            Cubemap = {};
    float              SourceCubemapAngle = 0;
    int32_t            CubemapResolution = 128;
    float              SkyDistanceThreshold = 150000;
    bool               bCaptureEmissiveOnly = false;
    bool               bLowerHemisphereIsBlack = true;
    Vector4            LowerHemisphereColor = Vector4(0, 0, 0, 1);
    float              OcclusionMaxDistance = 1000;
    float              Contrast = 0;
    float              OcclusionExponent = 1;
    float              MinOcclusion = 0;
    Vector3            OcclusionTint = Vector3(0.0f);
    bool               bCloudAmbientOcclusion = false;
    float              CloudAmbientOcclusionStrength = 1.0f;
    float              CloudAmbientOcclusionExtent = 150.0f;
    float              CloudAmbientOcclusionMapResolutionScale = 1.0f;
    float              CloudAmbientOcclusionApertureScale = 0.05f;
    OcclusionCombine   OcclusionCombineMode = OcclusionCombine::Minimum;
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
