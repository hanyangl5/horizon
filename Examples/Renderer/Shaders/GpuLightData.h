#pragma once

struct GpuLightData
{
    float4 PositionAndInvRadius;
    float4 DirectionAndType;
    float4 Color;
    float4 HalfAxisX;
    float4 HalfAxisY;
    float2 SpotAngles;
    float  SourceRadius;
    float  SoftSourceRadius;
    float  SourceLength;
    uint2  Id;
    uint2  Texture;
};

// ByteAddressBuffer records follow the 16-byte alignment of the C++ Vector4.
static const uint GpuLightStride = (sizeof(GpuLightData) + 15u) & ~15u;
