#pragma once

#include "Core/IMath.h"
#include "Graphics/RenderContext.h"
#include "SceneView.h"

// Shared CPU/Slang layout, 128 bytes. Direction follows emitted rays; w is perpendicular illuminance in lux.
struct alignas(16) LightingFrameData
{
    Matrix4  shadowViewProjection = Matrix4::identity();
    Vector4  lightDirection = Vector4(0, -1, 0, 0);
    Vector4  lightColor = Vector4(1, 1, 1, 1);
    float    environmentIntensity = 1000; // cd/m^2 per unit HDR environment radiance.
    float    shadowBias = 0;
    uint32_t shadowEnabled = 0;
    uint32_t rtShadowEnabled = 0;
    uint32_t rtAOEnabled = 0;
    uint32_t environmentMipCount = 1;
    uint32_t frameIndex = 0;
    uint32_t shadowFilterRadius = 0;
};

struct LightingResources
{
    const hz::GPUBuffer*  pLightingFrame = nullptr;
    const hz::GPUTexture* pShadowMap = nullptr;
    const hz::GPUSampler* pShadowSampler = nullptr;
    const hz::GPUTexture* pRTShadowAO = nullptr;
    const hz::GPUTexture* pRTReflection = nullptr;
    const hz::GPUTexture* pEnvironment = nullptr;
    const hz::GPUTexture* pIrradiance = nullptr;
    const hz::GPUTexture* pBRDFLut = nullptr;
    const hz::GPUSampler* pEnvironmentSampler = nullptr;
};

struct DeferredGBuffer
{
    const hz::GPUTexture* pEmissive = nullptr;
    const hz::GPUTexture* pNormalMaterial = nullptr;
    const hz::GPUTexture* pBaseColorMetallic = nullptr;
    const hz::GPUTexture* pDepth = nullptr;
};

class DeferredLighting
{
public:
    explicit DeferredLighting(hz::RenderContext& context);
    void resize(uint32_t width, uint32_t height);

    const hz::GPUTexture& execute(hz::CommandList& commands, const hz::GPUBuffer& frame, const DeferredGBuffer& gbuffer,
                                  const SceneView& view, const LightingResources& resources);
    const hz::GPUTexture& getColor() const { return color; }

private:
    hz::RenderContext& context;
    hz::GPUPipeline    pipeline;
    hz::GPUTexture     color;
};
