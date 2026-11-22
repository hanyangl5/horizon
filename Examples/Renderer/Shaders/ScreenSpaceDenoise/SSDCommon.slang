// Bindless resources and parameters shared by the denoise passes.
#pragma once
#include "../Common.slang"

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
    uint  MotionIndex;
};
