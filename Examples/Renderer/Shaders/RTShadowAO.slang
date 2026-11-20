#include "RTShared.slang"

half TraceRTAO(float3 Origin, float3 N, float2 Pixel, RaytracingAccelerationStructure Scene)
{
    float3x3 Basis = GetTangentBasis(N);

    half Occluded = half(0.0);
    for (uint I = 0; I != AoSamples; ++I)
    {
        // Each frame takes its own run of blue-noise slices so the temporal filter receives fresh samples.
        float2  Rnd = BlueNoiseVec2(BlueNoiseIndex, SamplerIndex, Pixel, FrameNumber * AoSamples + I);
        RayDesc Ray = { Origin, 0.0, normalize(mul(Basis, CosineSampleHemisphere(Rnd).xyz)), AoRadius };
        Occluded += half(1.0 - TraceRTShadow(Scene, Ray, AlphaTestMethod != 0));
    }

    return half(1.0) - Occluded / half(AoSamples);
}

[shader("compute")]
[numthreads(16, 16, 1)]
void main(uint3 Thread : SV_DispatchThreadID)
{
    RWTexture2D<float2> Output = ResourceDescriptorHeap[OutputIndex];
    uint                Width;
    uint                Height;
    Output.GetDimensions(Width, Height);
    if (Thread.x >= Width || Thread.y >= Height)
    {
        return;
    }

    Texture2D<float>  Depth = ResourceDescriptorHeap[DepthIndex];
    Texture2D<float4> Normals = ResourceDescriptorHeap[NormalIndex];
    uint2             SourceSize;
    Depth.GetDimensions(SourceSize.x, SourceSize.y);
    const int2 Source = RTSourcePixel(int2(Thread.xy), int2(Width, Height), int2(SourceSize));
    float      D = Depth.Load(int3(Source, 0));
    if (D >= 1.0 || TlasIndex == INVALID_TEXTURE)
    {
        Output[Thread.xy] = 1.0;
        return;
    }

    StructuredBuffer<FrameData>         Frames = ResourceDescriptorHeap[FrameIndexBuffer];
    StructuredBuffer<LightingFrameData> Lights = ResourceDescriptorHeap[LightingIndex];
    RaytracingAccelerationStructure     Scene = ResourceDescriptorHeap[TlasIndex];
    float2 Pixel = float2(Thread.xy) + 0.5;
    float2 UV = (float2(Source) + 0.5) / float2(SourceSize);
    // Decode in float, like the deferred pass, so the shadow ray leaves along the shaded normal.
    float3 N = DecodeOctNormal(Normals.Load(int3(Source, 0)).xy);
    float3 WorldPos = ReconstructWorldPosition(UV, D, Frames[0].InverseViewProjection);
    float3 Origin = RTGBufferRayOrigin(UV, D, WorldPos, N, Frames[0].Eye.xyz, Frames[0].InverseViewProjection);

    half Shadow = half(1.0);
    half AO = half(1.0);

    if (ShadowsEnabled != 0)
    {
        float3  L = -normalize(Lights[0].LightDirection.xyz);
        float2  Rnd = BlueNoiseVec2(BlueNoiseIndex, SamplerIndex, Pixel, FrameNumber);
        float3  RayDir = GenerateSoftShadowRayDir(L, ShadowRadius, Rnd);
        RayDesc Ray = { Origin, 0.0, RayDir, 1e4 };
        if (dot(N, RayDir) <= 0.0)
        {
            Shadow = half(0.0);
        }
        else if (AlphaTestMethod != 2)
        {
            Shadow = half(TraceRTShadow(Scene, Ray, AlphaTestMethod != 0));
        }
        else
        {
            // Closest-hit retrace: every pass returns the nearest hit past TMin, so no occluder in front of it is skipped.
            RayQuery<RAY_FLAG_FORCE_OPAQUE | RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES> Rq;
            // Conservatively keep the shadow if the trace budget is exhausted.
            Shadow = half(0.0);
            for (uint Depth = 0; Depth < RetraceMaxDepth; ++Depth)
            {
                Rq.TraceRayInline(Scene, RAY_FLAG_NONE, 0xff, Ray);
                while (Rq.Proceed())
                {
                }
                if (Rq.CommittedStatus() == COMMITTED_NOTHING)
                {
                    Shadow = half(1.0);
                    break;
                }
                GpuDrawData Mesh = LoadRTDraw(uint(Rq.CommittedInstanceID()));
                if (Mesh.AlphaMode != 1)
                {
                    break;
                }
                ByteAddressBuffer Materials = ResourceDescriptorHeap[MaterialsIndex];
                GpuMaterialData   Material = Materials.Load<GpuMaterialData>(Mesh.Material * sizeof(GpuMaterialData));
                float Alpha = SampleRayTracingAlpha(Mesh, Material, Rq.CommittedPrimitiveIndex(), Rq.CommittedTriangleBarycentrics());
                if (Alpha >= Material.AlphaCutoff)
                {
                    break;
                }
                // Positive ray distances: advance one representable float without a fixed gap.
                Ray.TMin = asfloat(asuint(Rq.CommittedRayT()) + 1u);
                if (Ray.TMin >= Ray.TMax)
                {
                    Shadow = half(1.0);
                    break;
                }
            }
        }
    }

    if (AoEnabled != 0)
    {
        AO = TraceRTAO(Origin, N, Pixel, Scene);
    }

    Output[Thread.xy] = float2(Shadow, AO);
}
