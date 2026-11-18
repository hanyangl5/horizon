// Cookbook RT material reconstruction and sampling; see ../Cookbook-LICENSE.txt.
#pragma once
#include "Common.slang"
#include "SceneData.slang"
#include "Math.slang"
#include "LightingBRDF.slang"

cbuffer RootConstant0
{
    uint  frameIndexBuffer;
    uint  lightingIndex;
    uint  drawsIndex;
    uint  materialsIndex;
    uint  texturesIndex;
    uint  samplerIndex;
    uint  tlasIndex;
    uint  normalIndex;
    uint  depthIndex;
    uint  outputIndex;
    uint  blueNoiseIndex;
    uint  environmentIndex;
    uint  irradianceIndex;
    uint  brdfIndex;
    uint  environmentSamplerIndex;
    uint  shadowsEnabled;
    uint  aoEnabled;
    uint  aoSamples;
    uint  frameNumber;
    uint  alphaTestMethod;
    uint  retraceMaxDepth;
    float aoRadius;
    float shadowRadius;
};

#include "Material.slang"

#include "RTTrace.slang"

float2 blueNoiseVec2(uint texBlueNoise, uint smpl, float2 screenCoord, uint frameIndex)
{
    uint2             wrappedXY = uint2(screenCoord) & uint2(127);
    uint              wrappedZ = frameIndex & 63u;
    float2            uv = (float2(float(wrappedXY.x), float(wrappedZ * 128u + wrappedXY.y)) + float2(0.5)) / float2(128.0, 8192.0);
    Texture2D<float4> texture = ResourceDescriptorHeap[texBlueNoise];
    SamplerState      sampler = SamplerDescriptorHeap[smpl];
    return texture.SampleLevel(sampler, uv, 0).rg;
}

LightingBindings rtLightingBindings()
{
    LightingBindings b;
    b.shadowMap = b.shadowSampler = b.rtShadowAO = b.rtReflection = INVALID_TEXTURE;
    b.environment = environmentIndex;
    b.irradiance = irradianceIndex;
    b.brdfLut = brdfIndex;
    b.environmentSampler = environmentSamplerIndex;
    return b;
}
