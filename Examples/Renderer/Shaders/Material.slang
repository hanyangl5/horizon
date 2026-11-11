#pragma once
#include "Common.slang"

// Matches the asset tables uploaded by GpuScene.
struct GpuMaterialData
{
    uint2  id;
    uint   baseColorTexture;
    uint   normalTexture;
    uint   metallicRoughnessTexture;
    uint   emissiveTexture;
    float4 baseColorFactor;
    float3 emissiveFactor;
    float  metallicFactor;
    float  roughnessFactor;
    float  alphaCutoff;
    uint   alphaMode;
    uint   doubleSided;
};
