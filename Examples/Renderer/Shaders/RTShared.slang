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
    float aoPower;
    float shadowRadius;
};

#include "Material.slang"

GpuDrawData loadRTDraw(uint index)
{
    ByteAddressBuffer draws = ResourceDescriptorHeap[drawsIndex];
    uint offset = index * sizeof(GpuDrawData);
    uint4 object = draws.Load4(offset);
    uint4 buffers = draws.Load4(offset + sizeof(uint4));
    uint4 geometry = draws.Load4(offset + 2 * sizeof(uint4));
    GpuDrawData draw;
    draw.instance = object.x;
    draw.material = object.y;
    draw.materialId = object.z;
    draw.alphaMode = object.w;
    draw.positions = buffers.x;
    draw.normals = buffers.y;
    draw.texcoords = buffers.z;
    draw.indices = buffers.w;
    draw.indexStride = geometry.x;
    draw.firstIndex = geometry.y;
    draw.vertexOffset = asint(geometry.z);
    draw.indexCount = geometry.w;
    return draw;
}

uint3 loadRTTriangle(GpuDrawData draw, uint primitive)
{
    ByteAddressBuffer indices = ResourceDescriptorHeap[NonUniformResourceIndex(draw.indices)];
    uint offset = (draw.firstIndex + primitive * 3) * draw.indexStride;
    uint3 triangle;
    if (draw.indexStride == 2)
    {
        uint2 packed = indices.Load2(offset & ~3u);
        triangle = (offset & 2u) == 0
            ? uint3(packed.x & 0xffffu, packed.x >> 16, packed.y & 0xffffu)
            : uint3(packed.x >> 16, packed.y & 0xffffu, packed.y >> 16);
    }
    else
    {
        triangle = indices.Load3(offset);
    }
    return uint3(int3(triangle) + draw.vertexOffset);
}

float2 loadRTUV(GpuDrawData draw, uint vertex)
{
    ByteAddressBuffer texcoords = ResourceDescriptorHeap[NonUniformResourceIndex(draw.texcoords)];
    uint              packed = texcoords.Load(vertex * 4);
    return float2(f16tof32(packed & 0xffffu), f16tof32(packed >> 16));
}

float2 interpolateRTUV(GpuDrawData draw, uint3 indices, float2 attributes)
{
    float3 bary = float3(1.0 - attributes.x - attributes.y, attributes);
    return loadRTUV(draw, indices.x) * bary.x + loadRTUV(draw, indices.y) * bary.y + loadRTUV(draw, indices.z) * bary.z;
}

float sampleRayTracingAlpha(GpuDrawData mesh, GpuMaterialData mat, uint primitiveIndex, float2 barycentrics)
{
    float alpha = mat.baseColorFactor.a;
    if (mat.baseColorTexture != INVALID_TEXTURE)
    {
        Texture2D<float4> texture = ResourceDescriptorHeap[NonUniformResourceIndex(mat.baseColorTexture)];
        SamplerState      sampler = SamplerDescriptorHeap[samplerIndex];
        float2            uv = interpolateRTUV(mesh, loadRTTriangle(mesh, primitiveIndex), barycentrics);
        alpha *= texture.SampleLevel(sampler, uv, 0).a;
    }
    return alpha;
}

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
