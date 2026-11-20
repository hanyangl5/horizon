#pragma once

#include "Core/IMath.h"
#include "Graphics/RenderContext.h"
#include "Scene/SceneView.h"

// Shared CPU/Slang layout, 144 bytes. Direction follows emitted rays; w is perpendicular illuminance in lux.
// Fields mirror the shader block, so they keep the shader's names.
struct alignas(16) LightingFrameData
{
    Matrix4  ShadowViewProjection = Matrix4::identity();
    Vector4  LightDirection = Vector4(0, -1, 0, 0);
    Vector4  LightColor = Vector4(1, 1, 1, 1);
    float    EnvironmentIntensity = 1000; // cd/m^2 per unit HDR environment radiance.
    float    ShadowBias = 0;
    uint32_t ShadowEnabled = 0;
    uint32_t RTShadowEnabled = 0;
    uint32_t RTAOEnabled = 0;
    uint32_t EnvironmentMipCount = 1;
    uint32_t FrameIndex = 0;
    uint32_t ShadowFilterRadius = 0;
    float    RTAOPower = 1;
    uint32_t Padding0 = 0;
    uint32_t Padding1 = 0;
    uint32_t Padding2 = 0;
};
static_assert(sizeof(LightingFrameData) == 144);

struct LightingResources
{
    const hz::GPUBuffer*  LightingFrame = nullptr;
    const hz::GPUTexture* ShadowMap = nullptr;
    const hz::GPUSampler* ShadowSampler = nullptr;
    const hz::GPUTexture* RTShadowAO = nullptr;
    const hz::GPUTexture* RTReflection = nullptr;
    // Ambient occlusion for indirect light only; the pass never darkens direct light or the sky.
    const hz::GPUTexture* SSAO = nullptr;
    const hz::GPUTexture* Environment = nullptr;
    const hz::GPUTexture* Irradiance = nullptr;
    const hz::GPUTexture* BRDFLut = nullptr;
    const hz::GPUSampler* EnvironmentSampler = nullptr;
};

struct DeferredGBuffer
{
    const hz::GPUTexture* Emissive = nullptr;
    const hz::GPUTexture* NormalMaterial = nullptr;
    const hz::GPUTexture* BaseColorMetallic = nullptr;
    const hz::GPUTexture* Depth = nullptr;
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
