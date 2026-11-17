#pragma once

#include "Core/IMath.h"
#include "Graphics/RenderContext.h"
#include "Scene/SceneView.h"

// Frame block for the directional light, its shadow map and the environment calibration. It is uploaded
// by ShadowMap and mirrors LightingFrameData in LightingBRDF.slang field for field.
struct alignas(16) LightingFrameData
{
    Matrix4  shadowViewProjection = Matrix4::identity();
    Vector4  lightDirection = Vector4(0, -1, 0, 0); // xyz follows the emitted light, w is illuminance in lux.
    Vector4  lightColor = Vector4(1, 1, 1, 1);
    float    environmentIntensity = 1000; // cd/m^2 per unit HDR environment radiance.
    float    shadowBias = 0;
    uint32_t shadowEnabled = 0;
    uint32_t rtShadowEnabled = 0;
    uint32_t rtAOEnabled = 0;
    uint32_t environmentMipCount = 1;
    uint32_t frameIndex = 0;
    uint32_t shadowFilterRadius = 0;
    float    rtAOPower = 1;
    uint32_t padding0 = 0;
    uint32_t padding1 = 0;
    uint32_t padding2 = 0;
};
static_assert(sizeof(LightingFrameData) == 144);

// Optional inputs to the deferred passes. A null or invalid entry is not bound and its descriptor index is
// UINT32_MAX, so every consumer must treat it as absent instead of sampling garbage.
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

// Geometry inputs written by the GBuffer pass; see GBuffer.md for the channel layouts.
struct DeferredGBuffer
{
    const hz::GPUTexture* Emissive = nullptr;
    const hz::GPUTexture* NormalMaterial = nullptr;
    const hz::GPUTexture* BaseColorMetallic = nullptr;
    // RGB motion vectors over [-1, 1] screen UV and A8 material ID; temporal reuse reprojects with it.
    const hz::GPUTexture* MotionMaterialId = nullptr;
    const hz::GPUTexture* Depth = nullptr;
};
