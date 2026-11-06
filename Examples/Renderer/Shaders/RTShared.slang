// Cookbook RT material reconstruction and sampling; see ../Cookbook-LICENSE.txt.
#pragma once
#include "Common.slang"
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

struct RTDraw
{
    uint instance;
    uint material;
    uint alphaMode;
    uint positions;
    uint normals;
    uint texcoords;
    uint indices;
    uint indexStride;
    uint firstIndex;
    int  vertexOffset;
};

RTDraw loadRTDraw(uint index)
{
    ByteAddressBuffer draws = ResourceDescriptorHeap[drawsIndex];
    uint4             object = draws.Load4(index * 48);
    uint4             buffers = draws.Load4(index * 48 + 16);
    uint4             geometry = draws.Load4(index * 48 + 32);
    RTDraw            result;
    result.instance = object.x;
    result.material = object.y;
    result.alphaMode = object.w;
    result.positions = buffers.x;
    result.normals = buffers.y;
    result.texcoords = buffers.z;
    result.indices = buffers.w;
    result.indexStride = geometry.x;
    result.firstIndex = geometry.y;
    result.vertexOffset = asint(geometry.z);
    return result;
}

uint resolveRTTexture(uint2 id)
{
    if (id.x == INVALID_TEXTURE || id.y == 0)
    {
        return INVALID_TEXTURE;
    }
    ByteAddressBuffer textures = ResourceDescriptorHeap[texturesIndex];
    uint              offset = id.x * 12;
    return all(textures.Load2(offset) == id) ? textures.Load(offset + 8) : INVALID_TEXTURE;
}

Material loadRTMaterial(uint index)
{
    ByteAddressBuffer materials = ResourceDescriptorHeap[materialsIndex];
    uint              offset = index * 88;
    Material          m;
    m.baseColorTexture = resolveRTTexture(materials.Load2(offset + 8));
    m.normalTexture = resolveRTTexture(materials.Load2(offset + 16));
    m.metallicRoughnessTexture = resolveRTTexture(materials.Load2(offset + 24));
    m.emissiveTexture = resolveRTTexture(materials.Load2(offset + 32));
    m.baseColor = asfloat(materials.Load4(offset + 40));
    m.emissive = asfloat(materials.Load3(offset + 56));
    m.metallic = asfloat(materials.Load(offset + 68));
    m.roughness = asfloat(materials.Load(offset + 72));
    m.alphaCutoff = asfloat(materials.Load(offset + 76));
    m.alphaMode = materials.Load(offset + 80);
    m.doubleSided = materials.Load(offset + 84);
    return m;
}

uint loadRTIndex(RTDraw draw, uint position)
{
    ByteAddressBuffer indices = ResourceDescriptorHeap[NonUniformResourceIndex(draw.indices)];
    uint              offset = (draw.firstIndex + position) * draw.indexStride;
    uint              packed = indices.Load(offset & ~3u);
    uint              value = draw.indexStride == 2 ? (packed >> ((offset & 2u) * 8u)) & 0xffffu : packed;
    return uint(int(value) + draw.vertexOffset);
}

uint3 loadRTTriangle(RTDraw draw, uint primitive)
{
    return uint3(loadRTIndex(draw, primitive * 3), loadRTIndex(draw, primitive * 3 + 1), loadRTIndex(draw, primitive * 3 + 2));
}

float2 loadRTUV(RTDraw draw, uint vertex)
{
    ByteAddressBuffer texcoords = ResourceDescriptorHeap[NonUniformResourceIndex(draw.texcoords)];
    uint              packed = texcoords.Load(vertex * 4);
    return float2(f16tof32(packed & 0xffffu), f16tof32(packed >> 16));
}

float2 interpolateRTUV(RTDraw draw, uint3 indices, float2 attributes)
{
    float3 bary = float3(1.0 - attributes.x - attributes.y, attributes);
    return loadRTUV(draw, indices.x) * bary.x + loadRTUV(draw, indices.y) * bary.y + loadRTUV(draw, indices.z) * bary.z;
}

float sampleRayTracingAlpha(RTDraw mesh, uint primitiveIndex, float2 barycentrics)
{
    Material mat = loadRTMaterial(mesh.material);
    float    alpha = mat.baseColor.a;
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
