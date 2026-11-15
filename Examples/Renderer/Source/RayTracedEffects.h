#pragma once

#include "DeferredLighting.h"
#include "GpuDrawList.h"
#include "Core/IUniquePtr.h"

enum class RTDenoiseMode : uint32_t
{
    Gaussian,
    Atrous
};

struct RayTracedEffectsSettings
{
    bool          Shadows = true;
    bool          AO = true;
    bool          Reflection = true;
    RTDenoiseMode DenoiseMode = RTDenoiseMode::Atrous;
    uint32_t      AOSamples = 1;
    float         AORadius = 2.0f;
    float         AOPower = 1.0f;
    float         ShadowRadius = 0.02f;
    // 0: opaque candidates, 1: any-hit alpha test, 2: bounded closest-hit retracing.
    uint32_t      ShadowAlphaTestMethod = 1;
    uint32_t      RetraceMaxDepth = 3;
    float         TemporalAlpha = 0.1f;
    float         TemporalDepthThreshold = 0.1f;
    float         SpatialDepthThreshold = 50000.0f;
    float         AtrousDepthSigma = 0.0015f;
    bool          operator==(const RayTracedEffectsSettings&) const = default;
};

class RayTracingScene;

class RayTracedEffects
{
public:
    explicit RayTracedEffects(hz::RenderContext& Context);
    ~RayTracedEffects();
    // Replacing targets requires completion of earlier draws, as for the other render passes.
    void Resize(uint32_t Width, uint32_t Height);

    void MarkDirty();
    void Execute(hz::CommandList& Commands, const hz::GPUBuffer& Frame, const DeferredGBuffer& GBuffer, const SceneView& View,
                 const GpuDrawList& Draws, const LightingResources& Lighting);
    bool IsSupported() const { return Supported; }
    const hz::GPUTexture& GetShadowAO() const { return Result; }
    const hz::GPUTexture& GetReflection() const { return Reflection; }
    RayTracedEffectsSettings Settings;

private:
    hz::RenderContext&                Context;
    hz::UniquePtr<RayTracingScene>   RayScene;
    hz::GPUPipeline                   ShadowPipeline;
    hz::GPUPipeline                   TemporalPipeline;
    hz::GPUPipeline                   SpatialPipeline;
    hz::GPUPipeline                   AtrousPipeline;
    hz::GPUPipeline                   DepthCopyPipeline;
    hz::GPURayTracingPipeline         ReflectionPipeline;
    hz::GPUSampler                    Sampler;
    hz::GPUSampler                    MaterialSampler;
    hz::GPUTexture                    BlueNoise;
    hz::GPUTexture                    Result;
    hz::GPUTexture                    Scratch;
    hz::FixedArray<hz::GPUTexture, 2> History;
    hz::GPUTexture                    PreviousDepth;
    hz::GPUTexture                    Reflection;
    RayTracedEffectsSettings          PreviousSettings;
    uint32_t                          Width = 0;
    uint32_t                          Height = 0;
    uint32_t                          HistoryIndex = 0;
    uint32_t                          FrameIndex = 0;
    uint64_t                          AssetRevision = 0;
    bool                              MarkedDirty = true;
    bool                              Supported = false;
};
