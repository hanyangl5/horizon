#pragma once

#include "Math.slang"

static const uint  INVALID_TEXTURE = 0xffffffffu;

struct FrameData
{
    float4x4 ViewProjection;
    float4x4 PreviousViewProjection;
    float4x4 InverseViewProjection;
    float4   Eye;
    float    Near, Far;
    uint     Width, Height;
};

// The ray traced effects run at a reduced resolution. Each of their texels samples one full-resolution GBuffer pixel,
// so tracing, denoising and the lighting upsample read the same surface.
int2 RTSourcePixel(int2 Pixel, int2 Size, int2 SourceSize) { return min(Pixel * SourceSize / Size, SourceSize - 1); }

float2 SignNotZero(float2 Value) { return float2(Value.x >= 0.0 ? 1.0 : -1.0, Value.y >= 0.0 ? 1.0 : -1.0); }

float2 EncodeOctNormal(float3 Normal)
{
    Normal /= dot(abs(Normal), 1.0.xxx);
    if (Normal.z < 0.0)
    {
        Normal.xy = (1.0 - abs(Normal.yx)) * SignNotZero(Normal.xy);
    }
    return Normal.xy * 0.5 + 0.5;
}

float3 DecodeOctNormal(float2 Encoded)
{
    float2 Oct = Encoded * 2.0 - 1.0;
    float3 Normal = float3(Oct, 1.0 - abs(Oct.x) - abs(Oct.y));
    if (Normal.z < 0.0)
    {
        Normal.xy = (1.0 - abs(Normal.yx)) * SignNotZero(Normal.xy);
    }
    return normalize(Normal);
}

float3 DecodeVertexNormal(uint Packed)
{
    float2 Oct = float2(Packed & 0xffffu, Packed >> 16) / 65535.0;
    return DecodeOctNormal(Oct);
}

// Specular uses 4 bits with a 1/16 step so the default 0.5 dielectric factor round-trips exactly;
// the deferred path and the ray traced one must agree on F0.
float2 PackMaterialProperties(float Roughness, float Specular)
{
    uint Packed = (uint)round(saturate(Roughness) * 255.0);
    Packed |= (uint)min(round(saturate(Specular) * 16.0), 15.0) << 8;
    return float2(float(Packed & 0x3ffu) / 1023.0, float(Packed >> 10) / 3.0);
}

float2 UnpackMaterialProperties(float2 Encoded)
{
    uint Packed = (uint)round(saturate(Encoded.x) * 1023.0);
    Packed |= (uint)round(saturate(Encoded.y) * 3.0) << 10;
    return float2(float(Packed & 0xffu) / 255.0, float((Packed >> 8) & 0xfu) / 16.0);
}

float3 EncodeMotionVector(float2 Motion)
{
    uint2 Quantized = (uint2)round(saturate(Motion * 0.5 + 0.5) * 4095.0);
    uint  Packed = Quantized.x | (Quantized.y << 12);
    return float3(Packed & 0xffu, (Packed >> 8) & 0xffu, (Packed >> 16) & 0xffu) / 255.0;
}

float2 DecodeMotionVector(float3 Encoded)
{
    uint3 Bytes = (uint3)round(saturate(Encoded) * 255.0);
    uint  Packed = Bytes.x | (Bytes.y << 8) | (Bytes.z << 16);
    uint2 Quantized = uint2(Packed & 0xfffu, (Packed >> 12) & 0xfffu);
    return float2(Quantized) / 4095.0 * 2.0 - 1.0;
}
