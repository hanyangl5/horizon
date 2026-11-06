// Horizon resource bindings for the original Cookbook denoise kernels.
#pragma once
#include "Common.slang"
#include "Math.slang"

cbuffer RootConstant0
{
    uint  frameIndex;
    uint  inputIndex;
    uint  historyIndex;
    uint  depthIndex;
    uint  previousDepthIndex;
    uint  outputIndex;
    uint  samplerIndex;
    uint  historyValid;
    float temporalAlpha;
    float depthThreshold;
    uint  horizontal;
    uint  stepWidth;
};

float4 textureBindless2D(uint textureId, uint samplerId, float2 uv)
{
    Texture2D<float4> texture = ResourceDescriptorHeap[textureId];
    SamplerState      sampler = SamplerDescriptorHeap[samplerId];
    return texture.SampleLevel(sampler, uv, 0);
}

int2 textureBindlessSize2D(uint textureId)
{
    Texture2D<float4> texture = ResourceDescriptorHeap[textureId];
    uint              width;
    uint              height;
    texture.GetDimensions(width, height);
    return int2(width, height);
}
