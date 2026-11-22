#pragma once

#include "Graphics/RenderContext.h"

struct DeferredGBuffer;
struct RayTracedEffectsSettings;
struct RayTracedEffectsTargets;

class ScreenSpaceDenoise
{
public:
    explicit ScreenSpaceDenoise(hz::RenderContext& Context);
    void DenoiseShadowAO(hz::CommandList& Commands, const hz::GPUBuffer& Frame, const DeferredGBuffer& GBuffer,
                         const RayTracedEffectsSettings& Settings, const RayTracedEffectsTargets& Targets, bool UseHistory);
    void DenoiseReflections(hz::CommandList& Commands, const hz::GPUBuffer& Frame, const DeferredGBuffer& GBuffer,
                            const RayTracedEffectsSettings& Settings, const RayTracedEffectsTargets& Targets, bool UseHistory);
    void StoreDepthHistory(hz::CommandList& Commands, const hz::GPUBuffer& Frame, const hz::GPUTexture& Depth,
                           const hz::GPUTexture& Output);

private:
    hz::GPUPipeline ShadowAOTemporalPipeline;
    hz::GPUPipeline ShadowAOSpatialPipeline;
    hz::GPUPipeline ReflectionTemporalPipeline;
    hz::GPUPipeline ReflectionSpatialPipeline;
    hz::GPUPipeline DepthHistoryPipeline;
};
