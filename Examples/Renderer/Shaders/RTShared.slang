// Cookbook RT material reconstruction and sampling; see ../Cookbook-LICENSE.txt.
#pragma once
#include "Common.slang"
#include "SceneData.slang"
#include "Math.slang"
#include "LightingBRDF.slang"

cbuffer RootConstant0
{
    uint  FrameIndexBuffer;
    uint  LightingIndex;
    uint  DrawsIndex;
    uint  MaterialsIndex;
    uint  TexturesIndex;
    uint  SamplerIndex;
    uint  TlasIndex;
    uint  NormalIndex;
    uint  DepthIndex;
    uint  OutputIndex;
    uint  BlueNoiseIndex;
    uint  EnvironmentIndex;
    uint  IrradianceIndex;
    uint  BrdfIndex;
    uint  EnvironmentSamplerIndex;
    uint  ShadowsEnabled;
    uint  AoEnabled;
    uint  AoSamples;
    uint  FrameNumber;
    uint  AlphaTestMethod;
    uint  RetraceMaxDepth;
    float AoRadius;
    float ShadowRadius;
#ifdef RT_LOCAL_SHADOW
    uint LocalShadowSlots, LocalShadowCounts, LocalShadowTotal;
#endif
};

#include "Material.slang"

GpuDrawData LoadRTDraw(uint Index)
{
    ByteAddressBuffer Draws = ResourceDescriptorHeap[DrawsIndex];
    uint Offset = Index * sizeof(GpuDrawData);
    uint4 Object = Draws.Load4(Offset);
    uint4 Buffers = Draws.Load4(Offset + sizeof(uint4));
    uint4 Geometry = Draws.Load4(Offset + 2 * sizeof(uint4));
    GpuDrawData Draw;
    Draw.Instance = Object.x;
    Draw.Material = Object.y;
    Draw.MaterialId = Object.z;
    Draw.AlphaMode = Object.w;
    Draw.Positions = Buffers.x;
    Draw.Normals = Buffers.y;
    Draw.Texcoords = Buffers.z;
    Draw.Indices = Buffers.w;
    Draw.IndexStride = Geometry.x;
    Draw.FirstIndex = Geometry.y;
    Draw.VertexOffset = asint(Geometry.z);
    Draw.IndexCount = Geometry.w;
    return Draw;
}

uint3 LoadRTTriangle(GpuDrawData Draw, uint Primitive)
{
    ByteAddressBuffer Indices = ResourceDescriptorHeap[NonUniformResourceIndex(Draw.Indices)];
    uint Offset = (Draw.FirstIndex + Primitive * 3) * Draw.IndexStride;
    uint3 Triangle;
    if (Draw.IndexStride == 2)
    {
        uint2 Packed = Indices.Load2(Offset & ~3u);
        Triangle = (Offset & 2u) == 0
            ? uint3(Packed.x & 0xffffu, Packed.x >> 16, Packed.y & 0xffffu)
            : uint3(Packed.x >> 16, Packed.y & 0xffffu, Packed.y >> 16);
    }
    else
    {
        Triangle = Indices.Load3(Offset);
    }
    return uint3(int3(Triangle) + Draw.VertexOffset);
}

float2 LoadRTUV(GpuDrawData Draw, uint Vertex)
{
    if (Draw.Texcoords == INVALID_TEXTURE)
    {
        return 0.0;
    }
    ByteAddressBuffer Texcoords = ResourceDescriptorHeap[NonUniformResourceIndex(Draw.Texcoords)];
    uint              Packed = Texcoords.Load(Vertex * 4);
    return float2(f16tof32(Packed & 0xffffu), f16tof32(Packed >> 16));
}

float2 InterpolateRTUV(GpuDrawData Draw, uint3 Indices, float2 Attributes)
{
    float3 Bary = float3(1.0 - Attributes.x - Attributes.y, Attributes);
    return LoadRTUV(Draw, Indices.x) * Bary.x + LoadRTUV(Draw, Indices.y) * Bary.y + LoadRTUV(Draw, Indices.z) * Bary.z;
}

float SampleRayTracingAlpha(GpuDrawData Mesh, GpuMaterialData Mat, uint PrimitiveIndex, float2 Barycentrics)
{
    float Alpha = Mat.BaseColorFactor.a;
    if (Mat.BaseColorTexture != INVALID_TEXTURE)
    {
        Texture2D<float4> Texture = ResourceDescriptorHeap[NonUniformResourceIndex(Mat.BaseColorTexture)];
        SamplerState      Sampler = SamplerDescriptorHeap[SamplerIndex];
        float2            UV = InterpolateRTUV(Mesh, LoadRTTriangle(Mesh, PrimitiveIndex), Barycentrics);
        Alpha *= Texture.SampleLevel(Sampler, UV, 0).a;
    }
    return Alpha;
}

// A GBuffer position is off by the depth quantization error along the view ray, which grows with distance. Pulling the
// origin toward the eye by a few depth steps keeps it on the visible side of the surface without a fixed world-space gap;
// the floor covers float precision of large world coordinates near the camera.
float3 RTGBufferRayOrigin(float2 UV, float Depth, float3 Position, float3 Normal, float3 Eye, float4x4 InverseViewProjection)
{
    const float3 ToEye = Eye - Position;
    const float3 Magnitude = abs(Position);
    const float  Offset = max(length(ReconstructWorldPosition(UV, max(Depth - 8.0 / 16777215.0, 0.0), InverseViewProjection) - Position),
                              max(max(Magnitude.x, Magnitude.y), max(Magnitude.z, 1.0)) / 65536.0);
    Normal = dot(Normal, ToEye) < 0.0 ? -Normal : Normal;
    return Position + (normalize(ToEye) + Normal) * Offset;
}

// Wächter and Binder, "A Fast and Robust Method for Avoiding Self-Intersection", Ray Tracing Gems chapter 6.
// For positions interpolated from triangle vertices; the normal is the geometric normal on the ray's side.
float3 RTOffsetHitPosition(float3 Position, float3 Normal)
{
    const float  Origin = 1.0 / 32.0;
    const float  FloatScale = 1.0 / 65536.0;
    const float  IntScale = 256.0;
    const int3   Offset = int3(IntScale * Normal);
    const float3 Shifted = asfloat(asint(Position) + select(Position < 0.0, -Offset, Offset));
    return select(abs(Position) < Origin, Position + FloatScale * Normal, Shifted);
}

// Returns 1 when the ray reaches TMax. Masked geometry is alpha tested when alphaTest is set.
float TraceRTShadow(RaytracingAccelerationStructure Scene, RayDesc Ray, bool AlphaTest)
{
    RayQuery<RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH | RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES> Rq;
    Rq.TraceRayInline(Scene, RAY_FLAG_NONE, 0xff, Ray);
    while (Rq.Proceed())
    {
        GpuDrawData Mesh = LoadRTDraw(uint(Rq.CandidateInstanceID()));
        if (!AlphaTest || Mesh.AlphaMode != 1)
        {
            Rq.CommitNonOpaqueTriangleHit();
            continue;
        }
        ByteAddressBuffer Materials = ResourceDescriptorHeap[MaterialsIndex];
        GpuMaterialData   Material = Materials.Load<GpuMaterialData>(Mesh.Material * sizeof(GpuMaterialData));
        if (SampleRayTracingAlpha(Mesh, Material, Rq.CandidatePrimitiveIndex(), Rq.CandidateTriangleBarycentrics()) >= Material.AlphaCutoff)
        {
            Rq.CommitNonOpaqueTriangleHit();
        }
    }
    return Rq.CommittedStatus() == COMMITTED_TRIANGLE_HIT ? 0.0 : 1.0;
}

LightingBindings RTLightingBindings()
{
    LightingBindings B;
    B.ShadowMap = B.ShadowSampler = B.RtShadowAO = B.RtReflection = INVALID_TEXTURE;
    B.Environment = EnvironmentIndex;
    B.Irradiance = IrradianceIndex;
    B.BrdfLut = BrdfIndex;
    B.EnvironmentSampler = EnvironmentSamplerIndex;
    return B;
}
