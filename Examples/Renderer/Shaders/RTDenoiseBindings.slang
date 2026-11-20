// Horizon resource bindings for the ray traced shadow/AO denoise kernels.
#pragma once
#include "Common.slang"
#include "Math.slang"

cbuffer RootConstant0
{
    uint  FrameIndex;
    uint  InputIndex;
    uint  HistoryIndex;
    uint  DepthIndex;
    uint  PreviousDepthIndex;
    uint  NormalIndex;
    uint  OutputIndex;
    uint  HistoryValid;
    float TemporalAlpha;
    float DepthThreshold;
    float PlaneSigma;
    uint  StepWidth;
};

// Clip-space w is the view depth for the renderer's perspective projections.
float ViewDepth(float4x4 ViewProjection, float3 WorldPosition) { return mul(ViewProjection, float4(WorldPosition, 1.0)).w; }
