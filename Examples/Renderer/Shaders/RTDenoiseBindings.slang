// Horizon resource bindings for the ray traced shadow/AO denoise kernels.
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
    uint  normalIndex;
    uint  outputIndex;
    uint  historyValid;
    float temporalAlpha;
    float depthThreshold;
    float planeSigma;
    uint  stepWidth;
};

// Clip-space w is the view depth for the renderer's perspective projections.
float viewDepth(float4x4 viewProjection, float3 worldPosition) { return mul(viewProjection, float4(worldPosition, 1.0)).w; }
