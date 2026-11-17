#pragma once

#include "LightingTypes.h"
#include "Scene/GpuDrawList.h"
#include "Core/IUniquePtr.h"

struct RayTracedEffectsSettings
{
    bool     Shadows = true;
    bool     AO = true;
    bool     Reflection = true;
    uint32_t AOSamples = 1;
    float    AORadius = 2.0f;
    float    AOPower = 1.0f;
    float    ShadowRadius = 0.02f;
    // 0: opaque candidates, 1: any-hit alpha test, 2: bounded closest-hit retracing.
    uint32_t ShadowAlphaTestMethod = 1;
    uint32_t RetraceMaxDepth = 3;
    // Minimum blend weight of the current frame once the per-pixel history is long.
    float    TemporalAlpha = 0.05f;
    // History is rejected when its view depth differs by more than this fraction.
    float    TemporalDepthThreshold = 0.05f;
    // A-trous plane distance falloff as a fraction of the view distance.
    float    AtrousPlaneSigma = 0.01f;
    bool     operator==(const RayTracedEffectsSettings&) const = default;
};

class RayTracingScene;

// Viewport-sized resources of the ray traced effects. History and PreviousDepth are written this frame; the Previous*
// copies hold last frame's, valid only when HistoryValid is set.
struct RayTracedEffectsTargets
{
    const hz::GPUTexture* Result = nullptr;
    const hz::GPUTexture* Scratch = nullptr;
    const hz::GPUTexture* Reflection = nullptr;
    const hz::GPUTexture* History = nullptr;
    const hz::GPUTexture* PreviousHistory = nullptr;
    const hz::GPUTexture* PreviousDepth = nullptr;
    const hz::GPUTexture* PreviousFrameDepth = nullptr;
    bool                  HistoryValid = false;
};

class RayTracedEffects
{
public:
    static constexpr hz::Format ResultFormat = hz::Format::R8G8_UNORM;
    static constexpr hz::Format ScratchFormat = hz::Format::R16G16_SFLOAT;
    // Shadow, AO and the per-pixel history length.
    static constexpr hz::Format HistoryFormat = hz::Format::R16G16B16A16_SFLOAT;
    static constexpr hz::Format DepthFormat = hz::Format::R32_SFLOAT;
    static constexpr hz::Format ReflectionFormat = hz::Format::R32G32B32A32_SFLOAT;
    // The effects trace one ray per texel at half the viewport resolution; lighting upsamples them by depth.
    static constexpr uint32_t   ResolutionShift = 1;

    explicit RayTracedEffects(hz::RenderContext& Context);
    ~RayTracedEffects();
    bool Load();

    void MarkDirty();
    void Execute(hz::CommandList& Commands, const hz::GPUBuffer& Frame, const DeferredGBuffer& GBuffer, const hz::SceneView& View,
                 const hz::GpuDrawList& Draws, const LightingResources& Lighting, const RayTracedEffectsTargets& Targets);
    bool IsSupported() const { return Supported; }
    RayTracedEffectsSettings Settings;

private:
    hz::RenderContext&             Context;
    hz::UniquePtr<RayTracingScene> RayScene;
    hz::GPUPipeline                ShadowPipeline;
    hz::GPUPipeline                TemporalPipeline;
    hz::GPUPipeline                AtrousPipeline;
    hz::GPUPipeline                DepthCopyPipeline;
    hz::GPURayTracingPipeline      ReflectionPipeline;
    hz::GPUSampler                 MaterialSampler;
    hz::GPUTexture                 BlueNoise;
    RayTracedEffectsSettings       PreviousSettings;
    uint32_t                       FrameIndex = 0;
    uint64_t                       AssetRevision = 0;
    bool                           MarkedDirty = true;
    bool                           Supported = false;
};
