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
    if (draw.texcoords == INVALID_TEXTURE)
    {
        return 0.0;
    }
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

// A GBuffer position is off by the depth quantization error along the view ray, which grows with distance. Pulling the
// origin toward the eye by a few depth steps keeps it on the visible side of the surface without a fixed world-space gap;
// the floor covers float precision of large world coordinates near the camera.
float3 rtGBufferRayOrigin(float2 uv, float depth, float3 position, float3 normal, float3 eye, float4x4 inverseViewProjection)
{
    const float3 toEye = eye - position;
    const float3 magnitude = abs(position);
    const float  offset = max(length(reconstructWorldPosition(uv, max(depth - 8.0 / 16777215.0, 0.0), inverseViewProjection) - position),
                              max(max(magnitude.x, magnitude.y), max(magnitude.z, 1.0)) / 65536.0);
    normal = dot(normal, toEye) < 0.0 ? -normal : normal;
    return position + (normalize(toEye) + normal) * offset;
}

// Wächter and Binder, "A Fast and Robust Method for Avoiding Self-Intersection", Ray Tracing Gems chapter 6.
// For positions interpolated from triangle vertices; the normal is the geometric normal on the ray's side.
float3 rtOffsetHitPosition(float3 position, float3 normal)
{
    const float  origin = 1.0 / 32.0;
    const float  floatScale = 1.0 / 65536.0;
    const float  intScale = 256.0;
    const int3   offset = int3(intScale * normal);
    const float3 shifted = asfloat(asint(position) + select(position < 0.0, -offset, offset));
    return select(abs(position) < origin, position + floatScale * normal, shifted);
}

// Returns 1 when the ray reaches TMax. Masked geometry is alpha tested when alphaTest is set.
float traceRTShadow(RaytracingAccelerationStructure scene, RayDesc ray, bool alphaTest)
{
    RayQuery<RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH | RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES> rq;
    rq.TraceRayInline(scene, RAY_FLAG_NONE, 0xff, ray);
    while (rq.Proceed())
    {
        GpuDrawData mesh = loadRTDraw(uint(rq.CandidateInstanceID()));
        if (!alphaTest || mesh.alphaMode != 1)
        {
            rq.CommitNonOpaqueTriangleHit();
            continue;
        }
        ByteAddressBuffer materials = ResourceDescriptorHeap[materialsIndex];
        GpuMaterialData   material = materials.Load<GpuMaterialData>(mesh.material * sizeof(GpuMaterialData));
        if (sampleRayTracingAlpha(mesh, material, rq.CandidatePrimitiveIndex(), rq.CandidateTriangleBarycentrics()) >= material.alphaCutoff)
        {
            rq.CommitNonOpaqueTriangleHit();
        }
    }
    return rq.CommittedStatus() == COMMITTED_TRIANGLE_HIT ? 0.0 : 1.0;
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
