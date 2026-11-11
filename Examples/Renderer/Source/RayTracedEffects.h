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
    bool          shadows = true;
    bool          ao = true;
    bool          reflection = true;
    RTDenoiseMode denoiseMode = RTDenoiseMode::Atrous;
    uint32_t      aoSamples = 1;
    float         aoRadius = 2.0f;
    float         aoPower = 1.0f;
    float         shadowRadius = 0.02f;
    // 0: opaque candidates, 1: any-hit alpha test, 2: bounded closest-hit retracing.
    uint32_t      shadowAlphaTestMethod = 1;
    uint32_t      retraceMaxDepth = 3;
    float         temporalAlpha = 0.1f;
    float         temporalDepthThreshold = 0.1f;
    float         spatialDepthThreshold = 50000.0f;
    float         atrousDepthSigma = 0.0015f;
    bool          operator==(const RayTracedEffectsSettings&) const = default;
};

class RayTracingScene;

class RayTracedEffects
{
public:
    explicit RayTracedEffects(hz::RenderContext& context);
    ~RayTracedEffects();
    // Replacing targets requires completion of earlier draws, as for the other render passes.
    void resize(uint32_t width, uint32_t height);

    void markDirty();
    void execute(hz::CommandList& commands, const hz::GPUBuffer& frame, const DeferredGBuffer& gbuffer, const SceneView& view,
                 const GpuDrawList& draws, const LightingResources& lighting);
    bool isSupported() const { return supported; }
    const hz::GPUTexture&    getShadowAO() const { return result; }
    const hz::GPUTexture&    getReflection() const { return reflection; }
    RayTracedEffectsSettings settings;

private:
    hz::RenderContext&                context;
    hz::unique_ptr<RayTracingScene>   rayScene;
    hz::GPUPipeline                   shadowPipeline;
    hz::GPUPipeline                   temporalPipeline;
    hz::GPUPipeline                   spatialPipeline;
    hz::GPUPipeline                   atrousPipeline;
    hz::GPUPipeline                   depthCopyPipeline;
    hz::GPURayTracingPipeline         reflectionPipeline;
    hz::GPUSampler                    sampler;
    hz::GPUSampler                    materialSampler;
    hz::GPUTexture                    blueNoise;
    hz::GPUTexture                    result;
    hz::GPUTexture                    scratch;
    hz::FixedArray<hz::GPUTexture, 2> history;
    hz::GPUTexture                    previousDepth;
    hz::GPUTexture                    reflection;
    RayTracedEffectsSettings          previousSettings;
    uint32_t                          width = 0;
    uint32_t                          height = 0;
    uint32_t                          historyIndex = 0;
    uint32_t                          frameIndex = 0;
    uint64_t                          assetRevision = 0;
    bool                              bMarkedDirty = true;
    bool                              supported = false;
};
