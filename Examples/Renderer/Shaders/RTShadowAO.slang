#include "RTShared.slang"

half traceRTAO16(float3 p, half3 n, float2 pixel, RaytracingAccelerationStructure scene)
{
    float3x3 basis = getTangentBasis(n);

    half occluded = half(0.0);
    for (uint i = 0; i != aoSamples; ++i)
    {
        float2 rnd = blueNoiseVec2(blueNoiseIndex, samplerIndex, pixel, frameNumber + i);
        float3 dir = normalize(mul(basis, cosineSampleHemisphere(rnd).xyz));

        RayQuery<RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH | RAY_FLAG_FORCE_OPAQUE | RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES> rq;
        RayDesc ray = { p + float3(n) * 0.03, 0.03, dir, aoRadius };
        rq.TraceRayInline(scene, RAY_FLAG_NONE, 0xff, ray);
        while (rq.Proceed())
        {
        }
        occluded += (rq.CommittedStatus() != COMMITTED_NOTHING) ? half(1.0) : half(0.0);
    }

    half ao = half(1.0) - occluded / half(aoSamples);
    return half(pow(clamp(float(ao), 0.0, 1.0), aoPower));
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
    if (d >= 0.9999 || tlasIndex == INVALID_TEXTURE)
    {
        output[thread.xy] = 1.0;
        return;
    }

    StructuredBuffer<FrameData>         frames = ResourceDescriptorHeap[frameIndexBuffer];
    StructuredBuffer<LightingFrameData> lights = ResourceDescriptorHeap[lightingIndex];
    RaytracingAccelerationStructure     scene = ResourceDescriptorHeap[tlasIndex];
    float2                              pixel = float2(thread.xy) + 0.5;
    float2                              uv = pixel / float2(width, height);
    half4                               g1 = half4(normals.Load(int3(thread.xy, 0)));
    half3                               n = decodeOctahedron16(g1.xy);
    float3                              worldPos = reconstructWorldPosition(uv, d, frames[0].inverseViewProjection);

    half shadow = half(1.0);
    half ao = half(1.0);

    if (shadowsEnabled != 0)
    {
        float3 l = -normalize(lights[0].lightDirection.xyz);
        float2 rnd = blueNoiseVec2(blueNoiseIndex, samplerIndex, pixel, frameNumber);
        float3 rayDir = generateSoftShadowRayDir(l, shadowRadius, rnd);

        if (dot(float3(n), rayDir) > 0.0)
        {
            if (alphaTestMethod != 2)
            {
                RayQuery<RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH | RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES> rq;

                RayDesc ray = { worldPos + float3(n) * 0.03, 0.03, rayDir, 1e4 };
                rq.TraceRayInline(scene, RAY_FLAG_NONE, 0xff, ray);
                while (rq.Proceed())
                {
                    uint customIndex = uint(rq.CandidateInstanceID());
                    if (alphaTestMethod == 0 || loadRTDraw(customIndex).alphaMode != 1)
                    {
                        rq.CommitNonOpaqueTriangleHit();
                    }
                    else
                    {
                        RTDraw mesh = loadRTDraw(customIndex);
                        float  alpha = sampleRayTracingAlpha(mesh, rq.CandidatePrimitiveIndex(), rq.CandidateTriangleBarycentrics());
                        if (alpha >= loadRTMaterial(mesh.material).alphaCutoff)
                        {
                            rq.CommitNonOpaqueTriangleHit();
                        }
                    }
                }

                if (rq.CommittedStatus() == COMMITTED_TRIANGLE_HIT)
                {
                    // e.g. to get distance:
                    // const float dist = rayQueryGetIntersectionTEXT(query, false);
                    shadow = half(0.0);
                }
                else
                {
                    shadow = half(1.0);
                }
            }
            else
            {
                // Experimental first-hit retrace: the returned hit may not be the nearest intersection.
                RayQuery<RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH | RAY_FLAG_FORCE_OPAQUE | RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES> rq;
                RayDesc ray = { worldPos + float3(n) * 0.03, 0.03, rayDir, 1e4 };
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
                    uint customIndex = uint(rq.CommittedInstanceID());
                    if (loadRTDraw(customIndex).alphaMode != 1)
                    {
                        break;
                    }
                    RTDraw mesh = loadRTDraw(customIndex);
                    float  alpha = sampleRayTracingAlpha(mesh, rq.CommittedPrimitiveIndex(), rq.CommittedTriangleBarycentrics());
                    if (alpha >= loadRTMaterial(mesh.material).alphaCutoff)
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
        else
        {
            shadow = half(0.0);
        }
    }

    if (aoEnabled != 0)
    {
        ao = traceRTAO16(worldPos, n, pixel, scene);
    }

    output[thread.xy] = float2(shadow, ao);
}
