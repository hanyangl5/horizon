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
    static void                    Construct(void* Destination) { ::new (Destination) T{}; }
    static void                    Destroy(void* ValueData) { ((T*)ValueData)->~T(); }
    static void                    CopyConstruct(void* Destination, const void* Source) { ::new (Destination) T(*(const T*)Source); }
    static void                    MoveConstruct(void* Destination, void* Source) { ::new (Destination) T(std::move(*(T*)Source)); }
    static constexpr TypeLifecycle Get()
    {
        return { .pConstruct = Construct, .pDestroy = Destroy, .pCopyConstruct = CopyConstruct, .pMoveConstruct = MoveConstruct };
    }
};

template<>
void ComponentLifecycle<Matrix4>::Construct(void* Destination)
{
    ::new (Destination) Matrix4(Matrix4::identity());
}

static uint32_t    OverrideCount(const void* ArrayPointer) { return ((const Array<MaterialOverride>*)ArrayPointer)->Size(); }
static const void* OverrideData(const void* ArrayPointer) { return ((const Array<MaterialOverride>*)ArrayPointer)->GetData(); }
static void        ResizeOverrides(void* ArrayPointer, uint32_t Count) { ((Array<MaterialOverride>*)ArrayPointer)->Resize(Count); }

static bool InsertOverride(void* ArrayPointer, uint32_t IndexValue, const void* ValueData)
{
    Array<MaterialOverride>& Values = *(Array<MaterialOverride>*)ArrayPointer;
    ASSERT(ValueData);
    if (IndexValue > Values.Size())
    {
        return false;
    }
    const MaterialOverride ValueValue = *(const MaterialOverride*)ValueData;
    Values.Resize(Values.Size() + 1);
    for (uint32_t I = Values.Size() - 1; I > IndexValue; --I)
    {
        Values[I] = Values[I - 1];
    }
    Values[IndexValue] = ValueValue;
    return true;
}

static bool RemoveOverride(void* ArrayPointer, uint32_t IndexValue)
{
    Array<MaterialOverride>& Values = *(Array<MaterialOverride>*)ArrayPointer;
    if (IndexValue >= Values.Size())
    {
        return false;
    }
    for (uint32_t I = IndexValue; I + 1 < Values.Size(); ++I)
    {
        Values[I] = Values[I + 1];
    }
    Values.PopBack();
    return true;
}

static const ArrayOperations overrideOperations = {
    .Size = sizeof(Array<MaterialOverride>),
    .alignment = alignof(Array<MaterialOverride>),
    .pCount = OverrideCount,
    .Data = OverrideData,
    .pResize = ResizeOverrides,
    .pInsert = InsertOverride,
    .pRemove = RemoveOverride,
};

static const PropertyDesc overrideProperties[] = {
    { .id = 1,
      .Name = "submesh",
      .kind = PropertyKind::UnsignedInteger,
      .offset = offsetof(MaterialOverride, submesh),
      .Size = sizeof(SubmeshID) },
    { .id = 2,
      .Name = "material",
      .kind = PropertyKind::AssetReference,
      .offset = offsetof(MaterialOverride, material),
      .Size = sizeof(AssetID) },
};

static const TypeDesc overrideType = {
    .id = 8,
    .Name = "MaterialOverride",
    .Size = sizeof(MaterialOverride),
    .alignment = alignof(MaterialOverride),
    .properties = overrideProperties,
};

static const PropertyDesc identityProperties[] = {
    { .id = 1,
      .Name = "value",
      .kind = PropertyKind::EntityReference,
      .offset = offsetof(EntityIdentity, value),
      .Size = sizeof(Guid),
      .Flags = PROPERTY_SERIALIZED | PROPERTY_READ_ONLY },
};
static const PropertyDesc nameProperties[] = {
    { .id = 1, .Name = "value", .kind = PropertyKind::String, .offset = offsetof(Name, value), .Size = sizeof(Name::value) },
};
static const PropertyDesc transformProperties[] = {
    { .id = 1,
      .Name = "translation",
      .kind = PropertyKind::Vector3,
      .offset = offsetof(LocalTransform, translation),
      .Size = sizeof(Vector3) },
    { .id = 2, .Name = "rotation", .kind = PropertyKind::Quaternion, .offset = offsetof(LocalTransform, rotation), .Size = sizeof(Quat) },
    { .id = 3, .Name = "scale", .kind = PropertyKind::Vector3, .offset = offsetof(LocalTransform, scale), .Size = sizeof(Vector3) },
};
static const PropertyDesc matrixProperties[] = {
    { .id = 1, .Name = "value", .kind = PropertyKind::Matrix4, .offset = 0, .Size = sizeof(Matrix4) },
};
static const PropertyDesc meshProperties[] = {
    { .id = 1, .Name = "mesh", .kind = PropertyKind::AssetReference, .offset = offsetof(MeshRenderer, mesh), .Size = sizeof(AssetID) },
    { .id = 2,
      .Name = "overrides",
      .kind = PropertyKind::Array,
      .offset = offsetof(MeshRenderer, overrides),
      .Size = sizeof(Array<MaterialOverride>),
      .pElementType = &overrideType,
      .pArrayOperations = &overrideOperations },
};
static const EnumValue LightUnitsValues[] = {
    { .Name = "Unitless", .value = (int32_t)LightUnits::Unitless }, { .Name = "Candelas", .value = (int32_t)LightUnits::Candelas },
    { .Name = "Lumens", .value = (int32_t)LightUnits::Lumens },     { .Name = "EV", .value = (int32_t)LightUnits::EV },
    { .Name = "Nits", .value = (int32_t)LightUnits::Nits },
};
static const EnumValue RayTracedShadowValues[] = {
    { .Name = "Disabled", .value = (int32_t)RayTracedShadow::Disabled },
    { .Name = "UseProjectSetting", .value = (int32_t)RayTracedShadow::UseProjectSetting },
    { .Name = "Enabled", .value = (int32_t)RayTracedShadow::Enabled },
};
static const EnumValue MegaLightsMethodValues[] = {
    { .Name = "Default", .value = (int32_t)MegaLightsMethod::Default },
    { .Name = "RayTracing", .value = (int32_t)MegaLightsMethod::RayTracing },
    { .Name = "VirtualShadowMap", .value = (int32_t)MegaLightsMethod::VirtualShadowMap },
};
static const EnumValue SkyLightSourceTypeValues[] = {
    { .Name = "CapturedScene", .value = (int32_t)SkyLightSourceType::CapturedScene },
    { .Name = "SpecifiedCubemap", .value = (int32_t)SkyLightSourceType::SpecifiedCubemap },
};
static const EnumValue OcclusionCombineValues[] = {
    { .Name = "Minimum", .value = (int32_t)OcclusionCombine::Minimum },
    { .Name = "Multiply", .value = (int32_t)OcclusionCombine::Multiply },
};
template<size_t N>
struct LightPropertyList
{
    PropertyDesc Fields[N];
};

template<size_t N, size_t M>
static constexpr LightPropertyList<N + M> CombineLightProperties(const PropertyDesc (&Parent)[N], uint32_t ParentOffset,
                                                                 const PropertyDesc (&Own)[M])
{
    LightPropertyList<N + M> Result = {};
    for (size_t I = 0; I < N; ++I)
    {
        Result.Fields[I] = Parent[I];
        Result.Fields[I].offset += ParentOffset;
    }
    for (size_t I = 0; I < M; ++I)
    {
        Result.Fields[N + I] = Own[I];
    }
    return Result;
}

#define LIGHT_PROPERTY(Id, Component, Member, Kind) \
    .id = Id, .Name = #Member, .kind = PropertyKind::Kind, .offset = offsetof(Component, Member), .Size = sizeof(Component::Member)

static constexpr PropertyDesc BaseFields[] = {
    { LIGHT_PROPERTY(3, LightComponentBase, Intensity, Float) },
    { LIGHT_PROPERTY(2, LightComponentBase, LightColor, Color) },
    { LIGHT_PROPERTY(20, LightComponentBase, bAffectsWorld, Boolean) },
    { LIGHT_PROPERTY(21, LightComponentBase, CastShadows, Boolean) },
    { LIGHT_PROPERTY(22, LightComponentBase, CastStaticShadows, Boolean) },
    { LIGHT_PROPERTY(23, LightComponentBase, CastDynamicShadows, Boolean) },
    { LIGHT_PROPERTY(24, LightComponentBase, bAffectTranslucentLighting, Boolean) },
    { LIGHT_PROPERTY(25, LightComponentBase, bTransmission, Boolean) },
    { LIGHT_PROPERTY(26, LightComponentBase, bCastVolumetricShadow, Boolean) },
    { LIGHT_PROPERTY(27, LightComponentBase, bCastDeepShadow, Boolean) },
    { LIGHT_PROPERTY(28, LightComponentBase, CastRaytracedShadow, Enum), .enumValues = RayTracedShadowValues },
    { LIGHT_PROPERTY(29, LightComponentBase, bAffectReflection, Boolean) },
    { LIGHT_PROPERTY(30, LightComponentBase, bAffectGlobalIllumination, Boolean) },
    { LIGHT_PROPERTY(31, LightComponentBase, DeepShadowLayerDistribution, Float), .minimum = 0.0, .maximum = 1.0 },
    { LIGHT_PROPERTY(32, LightComponentBase, IndirectLightingIntensity, Float), .minimum = 0.0, .maximum = 6.0 },
    { LIGHT_PROPERTY(33, LightComponentBase, VolumetricScatteringIntensity, Float), .minimum = 0.25, .maximum = 4.0 },
    { LIGHT_PROPERTY(34, LightComponentBase, SamplesPerPixel, SignedInteger) },
};
static constexpr PropertyDesc CommonFields[] = {
    { LIGHT_PROPERTY(14, LightComponent, Temperature, Float), .minimum = 1700.0, .maximum = 12000.0 },
    { LIGHT_PROPERTY(35, LightComponent, MaxDrawDistance, Float) },
    { LIGHT_PROPERTY(36, LightComponent, MaxDistanceFadeRange, Float) },
    { LIGHT_PROPERTY(15, LightComponent, bUseTemperature, Boolean) },
    { LIGHT_PROPERTY(37, LightComponent, SpecularScale, Float), .minimum = 0, .maximum = 1 },
    { LIGHT_PROPERTY(38, LightComponent, DiffuseScale, Float), .minimum = 0, .maximum = 1 },
    { LIGHT_PROPERTY(39, LightComponent, ShadowResolutionScale, Float), .minimum = .125, .maximum = 8 },
    { LIGHT_PROPERTY(40, LightComponent, ShadowBias, Float), .minimum = 0, .maximum = 1 },
    { LIGHT_PROPERTY(41, LightComponent, ShadowSlopeBias, Float), .minimum = 0, .maximum = 1 },
    { LIGHT_PROPERTY(42, LightComponent, ShadowSharpen, Float), .minimum = 0.0, .maximum = 1.0 },
    { LIGHT_PROPERTY(43, LightComponent, ContactShadowLength, Float), .minimum = 0.0, .maximum = 0.1 },
    { LIGHT_PROPERTY(44, LightComponent, ContactShadowLengthInWS, Boolean) },
    { LIGHT_PROPERTY(45, LightComponent, ContactShadowCastingIntensity, Float), .minimum = 0.0, .maximum = 1.0 },
    { LIGHT_PROPERTY(46, LightComponent, ContactShadowNonCastingIntensity, Float) },
    { LIGHT_PROPERTY(47, LightComponent, CastTranslucentShadows, Boolean) },
    { LIGHT_PROPERTY(48, LightComponent, bCastShadowsFromCinematicObjectsOnly, Boolean) },
    { LIGHT_PROPERTY(49, LightComponent, bForceCachedShadowsForMovablePrimitives, Boolean) },
    { LIGHT_PROPERTY(50, LightComponent, bAllowMegaLights, Boolean) },
    { LIGHT_PROPERTY(51, LightComponent, MegaLightsShadowMethod, Enum), .enumValues = MegaLightsMethodValues },
    { LIGHT_PROPERTY(52, LightComponent, bOverrideRayEndBias, Boolean) },
    { LIGHT_PROPERTY(53, LightComponent, RayEndBias, Float), .minimum = 0, .maximum = 100 },
    { LIGHT_PROPERTY(54, LightComponent, LightingChannels.bChannel0, Boolean) },
    { LIGHT_PROPERTY(55, LightComponent, LightingChannels.bChannel1, Boolean) },
    { LIGHT_PROPERTY(56, LightComponent, LightingChannels.bChannel2, Boolean) },
    { LIGHT_PROPERTY(57, LightComponent, ViewLightingChannels.bViewChannel0, Boolean) },
    { LIGHT_PROPERTY(58, LightComponent, ViewLightingChannels.bViewChannel1, Boolean) },
    { LIGHT_PROPERTY(59, LightComponent, ViewLightingChannels.bViewChannel2, Boolean) },
    { LIGHT_PROPERTY(60, LightComponent, ViewLightingChannels.bViewChannel3, Boolean) },
    { LIGHT_PROPERTY(61, LightComponent, ViewLightingChannels.bViewChannel4, Boolean) },
    { LIGHT_PROPERTY(62, LightComponent, LightFunctionMaterial, AssetReference) },
    { LIGHT_PROPERTY(63, LightComponent, LightFunctionScale, Vector3) },
    { LIGHT_PROPERTY(64, LightComponent, IESTexture, AssetReference) },
    { LIGHT_PROPERTY(65, LightComponent, bUseIESBrightness, Boolean) },
    { LIGHT_PROPERTY(66, LightComponent, IESBrightnessScale, Float), .minimum = 0.0, .maximum = 10.0 },
    { LIGHT_PROPERTY(67, LightComponent, LightFunctionFadeDistance, Float) },
    { LIGHT_PROPERTY(68, LightComponent, DisabledBrightness, Float), .minimum = 0.0, .maximum = 1.0 },
    { LIGHT_PROPERTY(69, LightComponent, bEnableLightShaftBloom, Boolean) },
    { LIGHT_PROPERTY(70, LightComponent, BloomScale, Float), .minimum = 0, .maximum = 10 },
    { LIGHT_PROPERTY(71, LightComponent, BloomThreshold, Float), .minimum = 0, .maximum = 4 },
    { LIGHT_PROPERTY(72, LightComponent, BloomMaxBrightness, Float), .minimum = 0, .maximum = 100 },
    { LIGHT_PROPERTY(73, LightComponent, BloomTint, Color) },
    { LIGHT_PROPERTY(74, LightComponent, bUseRayTracedDistanceFieldShadows, Boolean) },
    { LIGHT_PROPERTY(75, LightComponent, RayStartOffsetDepthScale, Float), .minimum = 0, .maximum = .1 },
};
static constexpr LightPropertyList CommonProperties = CombineLightProperties(BaseFields, offsetof(LightComponent, Base), CommonFields);
static constexpr PropertyDesc      LocalFields[] = {
    { LIGHT_PROPERTY(76, LocalLightComponent, IntensityUnits, Enum), .enumValues = LightUnitsValues },
    { LIGHT_PROPERTY(77, LocalLightComponent, InverseExposureBlend, Float), .minimum = 0.0, .maximum = 1.0 },
    { LIGHT_PROPERTY(4, LocalLightComponent, AttenuationRadius, Float), .minimum = 0.0, .maximum = 16384.0 },
    { LIGHT_PROPERTY(78, LocalLightComponent, LightmassSettings.IndirectLightingSaturation, Float) },
    { LIGHT_PROPERTY(79, LocalLightComponent, LightmassSettings.ShadowExponent, Float) },
    { LIGHT_PROPERTY(80, LocalLightComponent, LightmassSettings.bUseAreaShadowsForStationaryLight, Boolean) },
};
static constexpr LightPropertyList LocalProperties =
    CombineLightProperties(CommonProperties.Fields, offsetof(LocalLightComponent, Common), LocalFields);
static constexpr PropertyDesc PointFields[] = {
    { LIGHT_PROPERTY(81, PointLightComponent, bUseInverseSquaredFalloff, Boolean) },
    { LIGHT_PROPERTY(82, PointLightComponent, LightFalloffExponent, Float), .minimum = 2.0, .maximum = 16.0 },
    { LIGHT_PROPERTY(11, PointLightComponent, SourceRadius, Float), .minimum = 0.0, .maximum = 1000.0 },
    { LIGHT_PROPERTY(83, PointLightComponent, SoftSourceRadius, Float), .minimum = 0.0, .maximum = 1000.0 },
    { LIGHT_PROPERTY(12, PointLightComponent, SourceLength, Float), .minimum = 0.0, .maximum = 1000.0 },
};
static constexpr LightPropertyList PointProperties =
    CombineLightProperties(LocalProperties.Fields, offsetof(PointLightComponent, Local), PointFields);
static constexpr PropertyDesc SpotFields[] = {
    { LIGHT_PROPERTY(5, SpotLightComponent, InnerConeAngle, Float), .minimum = 1.0, .maximum = 80.0 },
    { LIGHT_PROPERTY(6, SpotLightComponent, OuterConeAngle, Float), .minimum = 1.0, .maximum = 80.0 },
};
static constexpr LightPropertyList SpotProperties =
    CombineLightProperties(PointProperties.Fields, offsetof(SpotLightComponent, Point), SpotFields);
static constexpr PropertyDesc RectFields[] = {
    { LIGHT_PROPERTY(9, RectLightComponent, SourceWidth, Float), .minimum = 0.0, .maximum = 1000.0 },
    { LIGHT_PROPERTY(10, RectLightComponent, SourceHeight, Float), .minimum = 0.0, .maximum = 1000.0 },
    { LIGHT_PROPERTY(84, RectLightComponent, BarnDoorAngle, Float), .minimum = 0.0, .maximum = 90.0 },
    { LIGHT_PROPERTY(85, RectLightComponent, BarnDoorLength, Float) },
    { LIGHT_PROPERTY(86, RectLightComponent, LightFunctionConeAngle, Float), .minimum = 0.0, .maximum = 89.0 },
    { LIGHT_PROPERTY(13, RectLightComponent, SourceTexture, AssetReference) },
    { LIGHT_PROPERTY(87, RectLightComponent, SourceTextureScale, Vector2) },
    { LIGHT_PROPERTY(88, RectLightComponent, SourceTextureOffset, Vector2) },
};
static constexpr LightPropertyList RectProperties =
    CombineLightProperties(LocalProperties.Fields, offsetof(RectLightComponent, Local), RectFields);
static constexpr PropertyDesc DirectionalFields[] = {
    { LIGHT_PROPERTY(89, DirectionalLightComponent, ShadowCascadeBiasDistribution, Float), .minimum = 0, .maximum = 1 },
    { LIGHT_PROPERTY(90, DirectionalLightComponent, bEnableLightShaftOcclusion, Boolean) },
    { LIGHT_PROPERTY(91, DirectionalLightComponent, OcclusionMaskDarkness, Float), .minimum = 0, .maximum = 1 },
    { LIGHT_PROPERTY(92, DirectionalLightComponent, OcclusionDepthRange, Float), .minimum = 0, .maximum = 500000 },
    { LIGHT_PROPERTY(93, DirectionalLightComponent, LightShaftOverrideDirection, Vector3) },
    { LIGHT_PROPERTY(94, DirectionalLightComponent, DynamicShadowDistanceMovableLight, Float), .minimum = 0, .maximum = 20000 },
    { LIGHT_PROPERTY(95, DirectionalLightComponent, DynamicShadowDistanceStationaryLight, Float), .minimum = 0, .maximum = 20000 },
    { LIGHT_PROPERTY(96, DirectionalLightComponent, DynamicShadowCascades, SignedInteger) },
    { LIGHT_PROPERTY(97, DirectionalLightComponent, CascadeDistributionExponent, Float), .minimum = 1, .maximum = 4 },
    { LIGHT_PROPERTY(98, DirectionalLightComponent, CascadeTransitionFraction, Float), .minimum = 0, .maximum = 0.3 },
    { LIGHT_PROPERTY(99, DirectionalLightComponent, ShadowDistanceFadeoutFraction, Float), .minimum = 0, .maximum = 1.0 },
    { LIGHT_PROPERTY(100, DirectionalLightComponent, bUseInsetShadowsForMovableObjects, Boolean) },
    { LIGHT_PROPERTY(101, DirectionalLightComponent, FarShadowCascadeCount, SignedInteger) },
    { LIGHT_PROPERTY(102, DirectionalLightComponent, FarShadowDistance, Float), .minimum = 0, .maximum = 800000 },
    { LIGHT_PROPERTY(103, DirectionalLightComponent, DistanceFieldShadowDistance, Float), .minimum = 0, .maximum = 100000 },
    { LIGHT_PROPERTY(104, DirectionalLightComponent, ForwardShadingPriority, SignedInteger) },
    { LIGHT_PROPERTY(8, DirectionalLightComponent, LightSourceAngle, Float), .minimum = 0, .maximum = 5 },
    { LIGHT_PROPERTY(105, DirectionalLightComponent, LightSourceSoftAngle, Float), .minimum = 0, .maximum = 5 },
    { LIGHT_PROPERTY(106, DirectionalLightComponent, ShadowSourceAngleFactor, Float), .minimum = 0, .maximum = 5 },
    { LIGHT_PROPERTY(107, DirectionalLightComponent, TraceDistance, Float), .minimum = 1000, .maximum = 100000 },
    { LIGHT_PROPERTY(108, DirectionalLightComponent, bAtmosphereSunLight, Boolean) },
    { LIGHT_PROPERTY(109, DirectionalLightComponent, AtmosphereSunLightIndex, SignedInteger) },
    { LIGHT_PROPERTY(110, DirectionalLightComponent, AtmosphereSunDiskColorScale, Vector4) },
    { LIGHT_PROPERTY(111, DirectionalLightComponent, bPerPixelAtmosphereTransmittance, Boolean) },
    { LIGHT_PROPERTY(112, DirectionalLightComponent, bCastShadowsOnClouds, Boolean) },
    { LIGHT_PROPERTY(113, DirectionalLightComponent, bCastShadowsOnAtmosphere, Boolean) },
    { LIGHT_PROPERTY(114, DirectionalLightComponent, bCastCloudShadows, Boolean) },
    { LIGHT_PROPERTY(115, DirectionalLightComponent, CloudShadowStrength, Float), .minimum = 0, .maximum = 1 },
    { LIGHT_PROPERTY(116, DirectionalLightComponent, CloudShadowOnAtmosphereStrength, Float), .minimum = 0, .maximum = 1 },
    { LIGHT_PROPERTY(117, DirectionalLightComponent, CloudShadowOnSurfaceStrength, Float), .minimum = 0, .maximum = 1 },
    { LIGHT_PROPERTY(118, DirectionalLightComponent, CloudShadowDepthBias, Float), .minimum = -1, .maximum = 1 },
    { LIGHT_PROPERTY(119, DirectionalLightComponent, CloudShadowExtent, Float) },
    { LIGHT_PROPERTY(120, DirectionalLightComponent, CloudShadowMapResolutionScale, Float), .minimum = 0.25, .maximum = 8 },
    { LIGHT_PROPERTY(121, DirectionalLightComponent, CloudShadowRaySampleCountScale, Float), .minimum = 0.25, .maximum = 8 },
    { LIGHT_PROPERTY(122, DirectionalLightComponent, CloudScatteredLuminanceScale, Vector4) },
    { LIGHT_PROPERTY(123, DirectionalLightComponent, LightmassSettings.IndirectLightingSaturation, Float) },
    { LIGHT_PROPERTY(124, DirectionalLightComponent, LightmassSettings.ShadowExponent, Float) },
    { LIGHT_PROPERTY(125, DirectionalLightComponent, LightmassSettings.bUseAreaShadowsForStationaryLight, Boolean) },
    { LIGHT_PROPERTY(126, DirectionalLightComponent, LightmassSettings.LightSourceAngle, Float) },
    { LIGHT_PROPERTY(127, DirectionalLightComponent, bCastModulatedShadows, Boolean) },
    { LIGHT_PROPERTY(128, DirectionalLightComponent, ModulatedShadowColor, Color) },
    { LIGHT_PROPERTY(129, DirectionalLightComponent, ShadowAmount, Float), .minimum = 0, .maximum = 1 },
};
static constexpr LightPropertyList DirectionalProperties =
    CombineLightProperties(CommonProperties.Fields, offsetof(DirectionalLightComponent, Common), DirectionalFields);
static constexpr PropertyDesc SkyFields[] = {
    { LIGHT_PROPERTY(130, SkyLightComponent, bRealTimeCapture, Boolean) },
    { LIGHT_PROPERTY(131, SkyLightComponent, SourceType, Enum), .enumValues = SkyLightSourceTypeValues },
    { LIGHT_PROPERTY(132, SkyLightComponent, Cubemap, AssetReference) },
    { LIGHT_PROPERTY(133, SkyLightComponent, SourceCubemapAngle, Float), .minimum = 0, .maximum = 360 },
    { LIGHT_PROPERTY(134, SkyLightComponent, CubemapResolution, SignedInteger) },
    { LIGHT_PROPERTY(135, SkyLightComponent, SkyDistanceThreshold, Float) },
    { LIGHT_PROPERTY(136, SkyLightComponent, bCaptureEmissiveOnly, Boolean) },
    { LIGHT_PROPERTY(137, SkyLightComponent, bLowerHemisphereIsBlack, Boolean) },
    { LIGHT_PROPERTY(138, SkyLightComponent, LowerHemisphereColor, Vector4) },
    { LIGHT_PROPERTY(139, SkyLightComponent, OcclusionMaxDistance, Float), .minimum = 200, .maximum = 1500 },
    { LIGHT_PROPERTY(140, SkyLightComponent, Contrast, Float), .minimum = 0, .maximum = 1 },
    { LIGHT_PROPERTY(141, SkyLightComponent, OcclusionExponent, Float), .minimum = .6, .maximum = 1.6 },
    { LIGHT_PROPERTY(142, SkyLightComponent, MinOcclusion, Float), .minimum = 0, .maximum = 1 },
    { LIGHT_PROPERTY(143, SkyLightComponent, OcclusionTint, Color) },
    { LIGHT_PROPERTY(144, SkyLightComponent, bCloudAmbientOcclusion, Boolean) },
    { LIGHT_PROPERTY(145, SkyLightComponent, CloudAmbientOcclusionStrength, Float), .minimum = 0, .maximum = 1 },
    { LIGHT_PROPERTY(146, SkyLightComponent, CloudAmbientOcclusionExtent, Float) },
    { LIGHT_PROPERTY(147, SkyLightComponent, CloudAmbientOcclusionMapResolutionScale, Float), .minimum = 0.25, .maximum = 8 },
    { LIGHT_PROPERTY(148, SkyLightComponent, CloudAmbientOcclusionApertureScale, Float), .minimum = 0.0, .maximum = 0.1 },
    { LIGHT_PROPERTY(149, SkyLightComponent, OcclusionCombineMode, Enum), .enumValues = OcclusionCombineValues },
};
static constexpr LightPropertyList SkyProperties = CombineLightProperties(BaseFields, offsetof(SkyLightComponent, Base), SkyFields);
#undef LIGHT_PROPERTY

static const PropertyDesc cameraProperties[] = {
    { .id = 1,
      .Name = "verticalFov",
      .kind = PropertyKind::Float,
      .offset = offsetof(Camera, verticalFov),
      .Size = sizeof(float),
      .minimum = 0.01,
      .maximum = 3.13,
      .step = 0.01 },
    { .id = 2,
      .Name = "nearPlane",
      .kind = PropertyKind::Float,
      .offset = offsetof(Camera, nearPlane),
      .Size = sizeof(float),
      .step = 0.01 },
    { .id = 3, .Name = "farPlane", .kind = PropertyKind::Float, .offset = offsetof(Camera, farPlane), .Size = sizeof(float), .step = 1.0 },
};

static const PropertyDesc skeletonProperties[] = {
    { .id = 1, .Name = "asset", .kind = PropertyKind::AssetReference, .offset = offsetof(Skeleton, asset), .Size = sizeof(AssetID) },
};
static const PropertyDesc animationPlayerProperties[] = {
    { .id = 1, .Name = "clip", .kind = PropertyKind::AssetReference, .offset = offsetof(AnimationPlayer, clip), .Size = sizeof(AssetID) },
    { .id = 2,
      .Name = "time",
      .kind = PropertyKind::Float,
      .offset = offsetof(AnimationPlayer, time),
      .Size = sizeof(float),
      .minimum = 0,
      .step = 0.01 },
    { .id = 3,
      .Name = "speed",
      .kind = PropertyKind::Float,
      .offset = offsetof(AnimationPlayer, speed),
      .Size = sizeof(float),
      .step = 0.1 },
    { .id = 4, .Name = "loop", .kind = PropertyKind::Boolean, .offset = offsetof(AnimationPlayer, loop), .Size = sizeof(bool) },
    { .id = 5, .Name = "playing", .kind = PropertyKind::Boolean, .offset = offsetof(AnimationPlayer, playing), .Size = sizeof(bool) },
};
static const PropertyDesc skinnedMeshProperties[] = {
    { .id = 1, .Name = "skin", .kind = PropertyKind::UnsignedInteger, .offset = offsetof(SkinnedMesh, skin), .Size = sizeof(uint32_t) },
};

bool TypeRegistry::RegisterBuiltins()
{
    static const TypeDesc Types[] = {
        { .id = SceneTypes::entityIdentity,
          .Name = "EntityIdentity",
          .Size = sizeof(EntityIdentity),
          .alignment = alignof(EntityIdentity),
          .properties = identityProperties,
          .triviallyCopyable = __is_trivially_copyable(EntityIdentity),
          .lifecycle = ComponentLifecycle<EntityIdentity>::Get() },
        { .id = SceneTypes::name,
          .Name = "Name",
          .Size = sizeof(Name),
          .alignment = alignof(Name),
          .properties = nameProperties,
          .triviallyCopyable = __is_trivially_copyable(Name),
          .lifecycle = ComponentLifecycle<Name>::Get() },
        { .id = SceneTypes::localTransform,
          .Name = "LocalTransform",
          .Size = sizeof(LocalTransform),
          .alignment = alignof(LocalTransform),
          .properties = transformProperties,
          .triviallyCopyable = __is_trivially_copyable(LocalTransform),
          .lifecycle = ComponentLifecycle<LocalTransform>::Get() },
        { .id = SceneTypes::localMatrix,
          .Name = "Matrix4",
          .Size = sizeof(Matrix4),
          .alignment = alignof(Matrix4),
          .properties = matrixProperties,
          .triviallyCopyable = __is_trivially_copyable(Matrix4),
          .lifecycle = ComponentLifecycle<Matrix4>::Get() },
        { .id = SceneTypes::meshRenderer,
          .Name = "MeshRenderer",
          .Size = sizeof(MeshRenderer),
          .alignment = alignof(MeshRenderer),
          .properties = meshProperties,
          .triviallyCopyable = false,
          .lifecycle = ComponentLifecycle<MeshRenderer>::Get() },
        { .id = SceneTypes::directionalLight,
          .Name = "DirectionalLight",
          .Size = sizeof(DirectionalLightComponent),
          .alignment = alignof(DirectionalLightComponent),
          .properties = DirectionalProperties.Fields,
          .triviallyCopyable = __is_trivially_copyable(DirectionalLightComponent),
          .lifecycle = ComponentLifecycle<DirectionalLightComponent>::Get() },
        { .id = SceneTypes::pointLight,
          .Name = "PointLight",
          .Size = sizeof(PointLightComponent),
          .alignment = alignof(PointLightComponent),
          .properties = PointProperties.Fields,
          .triviallyCopyable = __is_trivially_copyable(PointLightComponent),
          .lifecycle = ComponentLifecycle<PointLightComponent>::Get() },
        { .id = SceneTypes::spotLight,
          .Name = "SpotLight",
          .Size = sizeof(SpotLightComponent),
          .alignment = alignof(SpotLightComponent),
          .properties = SpotProperties.Fields,
          .triviallyCopyable = __is_trivially_copyable(SpotLightComponent),
          .lifecycle = ComponentLifecycle<SpotLightComponent>::Get() },
        { .id = SceneTypes::rectLight,
          .Name = "RectLight",
          .Size = sizeof(RectLightComponent),
          .alignment = alignof(RectLightComponent),
          .properties = RectProperties.Fields,
          .triviallyCopyable = __is_trivially_copyable(RectLightComponent),
          .lifecycle = ComponentLifecycle<RectLightComponent>::Get() },
        { .id = SceneTypes::skyLight,
          .Name = "SkyLight",
          .Size = sizeof(SkyLightComponent),
          .alignment = alignof(SkyLightComponent),
          .properties = SkyProperties.Fields,
          .triviallyCopyable = __is_trivially_copyable(SkyLightComponent),
          .lifecycle = ComponentLifecycle<SkyLightComponent>::Get() },
        { .id = SceneTypes::camera,
          .Name = "Camera",
          .Size = sizeof(Camera),
          .alignment = alignof(Camera),
          .properties = cameraProperties,
          .triviallyCopyable = __is_trivially_copyable(Camera),
          .lifecycle = ComponentLifecycle<Camera>::Get() },
        { .id = SceneTypes::skeleton,
          .Name = "Skeleton",
          .Size = sizeof(Skeleton),
          .alignment = alignof(Skeleton),
          .properties = skeletonProperties,
          .triviallyCopyable = __is_trivially_copyable(Skeleton),
          .lifecycle = ComponentLifecycle<Skeleton>::Get() },
        { .id = SceneTypes::animationPlayer,
          .Name = "AnimationPlayer",
          .Size = sizeof(AnimationPlayer),
          .alignment = alignof(AnimationPlayer),
          .properties = animationPlayerProperties,
          .triviallyCopyable = __is_trivially_copyable(AnimationPlayer),
          .lifecycle = ComponentLifecycle<AnimationPlayer>::Get() },
        { .id = SceneTypes::skinnedMesh,
          .Name = "SkinnedMesh",
          .Size = sizeof(SkinnedMesh),
          .alignment = alignof(SkinnedMesh),
          .properties = skinnedMeshProperties,
          .triviallyCopyable = __is_trivially_copyable(SkinnedMesh),
          .lifecycle = ComponentLifecycle<SkinnedMesh>::Get() },
    };
    return RegisterTypes(Types);
}
} // namespace hz
