#pragma once
#include "Common.slang"

// Matches the asset tables uploaded by GpuScene.
struct GpuMaterialData
{
    uint2  Id;
    uint   BaseColorTexture;
    uint   NormalTexture;
    uint   MetallicRoughnessTexture;
    uint   EmissiveTexture;
    float4 BaseColorFactor;
    float3 EmissiveFactor;
    float  MetallicFactor;
    float  RoughnessFactor;
    float  AlphaCutoff;
    uint   AlphaMode;
    uint   DoubleSided;
    uint   Tags;
};
