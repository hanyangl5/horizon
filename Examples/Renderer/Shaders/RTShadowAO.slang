#include "RTShared.slang"

half TraceRTAO(float3 origin, float3 n, float2 pixel, RaytracingAccelerationStructure scene)
{
    float3x3 basis = getTangentBasis(n);

    half occluded = half(0.0);
    for (uint i = 0; i != aoSamples; ++i)
    {
        // Each frame takes its own run of blue-noise slices so the temporal filter receives fresh samples.
        float2  rnd = blueNoiseVec2(blueNoiseIndex, samplerIndex, pixel, frameNumber * aoSamples + i);
        RayDesc ray = { origin, 0.0, normalize(mul(basis, cosineSampleHemisphere(rnd).xyz)), aoRadius };
        occluded += half(1.0 - traceRTShadow(scene, ray, alphaTestMethod != 0));
    }

    return half(1.0) - occluded / half(aoSamples);
}

[shader("compute")]
[numthreads(16, 16, 1)]
void main(uint3 thread : SV_DispatchThreadID)
{
    RWTexture2D<float2> output = ResourceDescriptorHeap[outputIndex];
    uint                width;
    uint                height;
    output.GetDimensions(width, height);
    if (thread.x >= width || thread.y >= height)
    {
        return;
    }

    Texture2D<float>  depth = ResourceDescriptorHeap[depthIndex];
    Texture2D<float4> normals = ResourceDescriptorHeap[normalIndex];
    float             d = depth.Load(int3(thread.xy, 0));
    if (d >= 1.0 || tlasIndex == INVALID_TEXTURE)
    {
        output[thread.xy] = 1.0;
        return;
    }

    StructuredBuffer<FrameData>         frames = ResourceDescriptorHeap[frameIndexBuffer];
    StructuredBuffer<LightingFrameData> lights = ResourceDescriptorHeap[lightingIndex];
    RaytracingAccelerationStructure     scene = ResourceDescriptorHeap[tlasIndex];
    float2 pixel = float2(thread.xy) + 0.5;
    float2 uv = pixel / float2(width, height);
    // Decode in float, like the deferred pass, so the shadow ray leaves along the shaded normal.
    float3 n = decodeOctNormal(normals.Load(int3(thread.xy, 0)).xy);
    float3 worldPos = reconstructWorldPosition(uv, d, frames[0].inverseViewProjection);
    float3 origin = rtGBufferRayOrigin(uv, d, worldPos, n, frames[0].eye.xyz, frames[0].inverseViewProjection);

    half shadow = half(1.0);
    half ao = half(1.0);

    if (shadowsEnabled != 0)
    {
        float3  l = -normalize(lights[0].lightDirection.xyz);
        float2  rnd = blueNoiseVec2(blueNoiseIndex, samplerIndex, pixel, frameNumber);
        float3  rayDir = generateSoftShadowRayDir(l, shadowRadius, rnd);
        RayDesc ray = { origin, 0.0, rayDir, 1e4 };
        if (dot(n, rayDir) <= 0.0)
        {
            shadow = half(0.0);
        }
        else if (alphaTestMethod != 2)
        {
            shadow = half(traceRTShadow(scene, ray, alphaTestMethod != 0));
        }
        else
        {
            // Closest-hit retrace: every pass returns the nearest hit past TMin, so no occluder in front of it is skipped.
            RayQuery<RAY_FLAG_FORCE_OPAQUE | RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES> rq;
            // Conservatively keep the shadow if the trace budget is exhausted.
            shadow = half(0.0);
            for (uint depth = 0; depth < retraceMaxDepth; ++depth)
            {
                rq.TraceRayInline(scene, RAY_FLAG_NONE, 0xff, ray);
                while (rq.Proceed())
                {
                }
                if (rq.CommittedStatus() == COMMITTED_NOTHING)
                {
                    shadow = half(1.0);
                    break;
                }
                GpuDrawData mesh = loadRTDraw(uint(rq.CommittedInstanceID()));
                if (mesh.alphaMode != 1)
                {
                    break;
                }
                ByteAddressBuffer materials = ResourceDescriptorHeap[materialsIndex];
                GpuMaterialData   material = materials.Load<GpuMaterialData>(mesh.material * sizeof(GpuMaterialData));
                float alpha = sampleRayTracingAlpha(mesh, material, rq.CommittedPrimitiveIndex(), rq.CommittedTriangleBarycentrics());
                if (alpha >= material.alphaCutoff)
                {
                    break;
                }
                // Positive ray distances: advance one representable float without a fixed gap.
                ray.TMin = asfloat(asuint(rq.CommittedRayT()) + 1u);
                if (ray.TMin >= ray.TMax)
                {
                    shadow = half(1.0);
                    break;
                }
            }
        }
    }

    if (aoEnabled != 0)
    {
        ao = TraceRTAO(origin, n, pixel, scene);
    }

    output[thread.xy] = float2(shadow, ao);
}
