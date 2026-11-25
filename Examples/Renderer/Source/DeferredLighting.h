#pragma once

#include "Core/IContainer.h"
#include "Core/IMath.h"
#include "Graphics/RenderContext.h"
#include "Scene/SceneView.h"

// Shared CPU/Slang layout, 176 bytes. Direction follows emitted rays; w is perpendicular illuminance in lux.
// Fields mirror the shader block, so they keep the shader's names.
struct alignas(16) LightingFrameData
{
    Matrix4                ShadowViewProjection = Matrix4::identity();
    Vector4                LightDirection = Vector4(0, -1, 0, 0);
    Vector4                LightColor = Vector4(1, 1, 1, 1);
    static constexpr float DefaultEnvironmentIntensity = 0.0f;
    float                  EnvironmentIntensity = DefaultEnvironmentIntensity; // cd/m^2 per unit HDR environment radiance.
    float                  ShadowBias = 0;
    uint32_t               ShadowEnabled = 0;
    uint32_t               RTShadowEnabled = 0;
    uint32_t               RTAOEnabled = 0;
    uint32_t               EnvironmentMipCount = 1;
    uint32_t               FrameIndex = 0;
    uint32_t               ShadowFilterRadius = 0;
    float                  RTAOPower = 1;
    uint32_t               LightsIndex = UINT32_MAX;
    uint32_t               LightCount = 0;
    uint32_t               ActiveLightsIndex = UINT32_MAX;
    Vector4                EnvironmentTint = Vector4(1.0f);
    uint32_t               MainDirectionalIndex = UINT32_MAX;
    float                  MainDiffuseScale = 1;
    float                  MainSpecularScale = 1;
    uint32_t               MainExcludedPasses = 0;
};
static_assert(sizeof(LightingFrameData) == 176);

struct LightingResources
{
    const hz::GPUBuffer*  LightingFrame = nullptr;
    bool                  TraceShadows = false;
    float                 ShadowConeRadius = 0;
    const hz::GPUTexture* ShadowMap = nullptr;
    const hz::GPUSampler* ShadowSampler = nullptr;
    const hz::GPUTexture* RTShadowAO = nullptr;
    const hz::GPUTexture* RTReflection = nullptr;
    // Ambient occlusion for indirect light only; the pass never darkens direct light or the sky.
    const hz::GPUTexture* GTAO = nullptr;
    const hz::GPUTexture* Environment = nullptr;
    const hz::GPUTexture* Irradiance = nullptr;
    const hz::GPUTexture* BRDFLut = nullptr;
    const hz::GPUSampler* EnvironmentSampler = nullptr;
    const hz::GPUBuffer*  Lights = nullptr;
    const hz::GPUBuffer*  ActiveLights = nullptr;
    const hz::GPUBuffer*  ClusterData = nullptr;
    const hz::GPUBuffer*  TileMasks = nullptr;
};

struct DeferredGBuffer
{
    const hz::GPUTexture* Emissive = nullptr;
    const hz::GPUTexture* NormalMaterial = nullptr;
    const hz::GPUTexture* BaseColorMetallic = nullptr;
    const hz::GPUTexture* Depth = nullptr;
    const hz::GPUTexture* Motion = nullptr;
};

class DeferredLighting
{
public:
    static constexpr hz::Format ColorFormat = hz::Format::R32G32B32A32_SFLOAT;

    explicit DeferredLighting(hz::RenderContext& Context);
    void Execute(hz::CommandList& Commands, const hz::GPUBuffer& Frame, const DeferredGBuffer& GBuffer, const LightingResources& Resources,
                 const hz::GPUTexture& Color);

private:
    hz::GPUPipeline Pipeline;
};
