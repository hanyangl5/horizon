#include "RTShared.slang"
#include "Common.slang"

struct RayPayload
{
    float3 color;
    uint   depth;
};

[shader("raygeneration")]
void rayGenMain()
{
    RWTexture2D<float4>         output = ResourceDescriptorHeap[outputIndex];
    Texture2D<float>            depthTexture = ResourceDescriptorHeap[depthIndex];
    Texture2D<float4>           normals = ResourceDescriptorHeap[normalIndex];
    StructuredBuffer<FrameData> frame = ResourceDescriptorHeap[frameIndexBuffer];
    uint                        width;
    uint                        height;
    depthTexture.GetDimensions(width, height);

    uint2  pixel = DispatchRaysIndex().xy;
    uint2  size = DispatchRaysDimensions().xy;
    // Load depth and normal from the same full-resolution pixel, without blending across silhouettes.
    int2   depthSize = int2(width, height);
    int2   sourcePixel = rtSourcePixel(int2(pixel), int2(size), depthSize);
    float  depth = depthTexture.Load(int3(sourcePixel, 0));
    if (depth >= 1.0 || tlasIndex == INVALID_TEXTURE)
    {
        output[pixel] = float4(0.0);
        return;
    }
    RaytracingAccelerationStructure scene = ResourceDescriptorHeap[tlasIndex];
    float2                          sourceUV = (float2(sourcePixel) + 0.5) / float2(depthSize);
    float4                          g1 = normals.Load(int3(sourcePixel, 0));
    float3                          n = decodeOctNormal(g1.xy);
    float2                          properties = unpackMaterialProperties(float2(g1.ba));
    float3                          worldPos = reconstructWorldPosition(sourceUV, depth, frame[0].inverseViewProjection);
    float3                          viewDir = normalize(frame[0].eye.xyz - worldPos);
    if (properties.x >= kRTReflectionMaxRoughness)
    {
        output[pixel] = float4(0.0);
        return;
    }
    RayDesc ray = { rtGBufferRayOrigin(sourceUV, depth, worldPos, n, frame[0].eye.xyz, frame[0].inverseViewProjection), 0.0,
                    reflect(-viewDir, n), 1e6 };
    RayPayload payload;
    payload.color = float3(0.0);
    payload.depth = 0;
    float3 accumulatedColor = float3(0.0, 0.0, 0.0);
    {
        TraceRay(scene, RAY_FLAG_NONE, 0xff, 0, 0, 0, ray, payload);
        accumulatedColor += payload.color;
        payload.depth++;
    }
    // Unweighted incoming radiance. The receiving surface's Fresnel is applied in Lighting.
    output[pixel] = float4(accumulatedColor, 1.0);
}

[shader("miss")]
void missMain(inout RayPayload payload)
{
    payload.color = 0;
    payload.depth = 0xff;
    if (environmentIndex == INVALID_TEXTURE || environmentSamplerIndex == INVALID_TEXTURE)
    {
        return;
    }
    StructuredBuffer<LightingFrameData> light = ResourceDescriptorHeap[lightingIndex];
    TextureCube<float4>                 environment = ResourceDescriptorHeap[environmentIndex];
    SamplerState                        sampler = SamplerDescriptorHeap[environmentSamplerIndex];
    float3                              direction = WorldRayDirection();
    payload.color =
        environment.SampleLevel(sampler, float3(-direction.x, direction.y, -direction.z), 0).rgb * max(light[0].environmentIntensity, 0.0);
}

[shader("closesthit")]
void closestHitMain(inout RayPayload payload, BuiltInTriangleIntersectionAttributes attributes)
{
    GpuDrawData draw = loadRTDraw(InstanceID());
    uint3             indices = loadRTTriangle(draw, PrimitiveIndex());
    float3            bary = float3(1.0 - attributes.barycentrics.x - attributes.barycentrics.y, attributes.barycentrics);
    float3            V = -normalize(WorldRayDirection());
    ByteAddressBuffer positions = ResourceDescriptorHeap[NonUniformResourceIndex(draw.positions)];
    const float3      p0 = asfloat(positions.Load3(indices.x * sizeof(float3)));
    const float3      p1 = asfloat(positions.Load3(indices.y * sizeof(float3)));
    const float3      p2 = asfloat(positions.Load3(indices.z * sizeof(float3)));
    float3            geometricNormal = normalize(mul(cross(p1 - p0, p2 - p0), (float3x3)WorldToObject3x4()));
    geometricNormal = dot(geometricNormal, V) < 0.0 ? -geometricNormal : geometricNormal;
    // Meshes without a normal stream shade with the geometric normal.
    float3 N = geometricNormal;
    if (draw.normals != INVALID_TEXTURE)
    {
        ByteAddressBuffer normals = ResourceDescriptorHeap[NonUniformResourceIndex(draw.normals)];
        const float3      normal = decodeVertexNormal(normals.Load(indices.x * 4)) * bary.x +
                              decodeVertexNormal(normals.Load(indices.y * 4)) * bary.y + decodeVertexNormal(normals.Load(indices.z * 4)) * bary.z;
        N = normalize(mul(normal, (float3x3)WorldToObject3x4()));
        N = dot(N, V) < 0.0 ? -N : N;
    }
    const float3 hitPosition =
        rtOffsetHitPosition(mul(ObjectToWorld3x4(), float4(p0 * bary.x + p1 * bary.y + p2 * bary.z, 1.0)), geometricNormal);
    float2            uv = interpolateRTUV(draw, indices, attributes.barycentrics);
    ByteAddressBuffer materials = ResourceDescriptorHeap[materialsIndex];
    GpuMaterialData   material = materials.Load<GpuMaterialData>(draw.material * sizeof(GpuMaterialData));
    SamplerState      sampler = SamplerDescriptorHeap[samplerIndex];
    float3            baseColor = material.baseColorFactor.rgb;
    if (material.baseColorTexture != INVALID_TEXTURE)
    {
        Texture2D<float4> texture = ResourceDescriptorHeap[NonUniformResourceIndex(material.baseColorTexture)];
        baseColor *= texture.SampleLevel(sampler, uv, 0).rgb;
    }
    float metallic = material.metallicFactor;
    float roughness = material.roughnessFactor;
    if (material.metallicRoughnessTexture != INVALID_TEXTURE)
    {
        Texture2D<float4> texture = ResourceDescriptorHeap[NonUniformResourceIndex(material.metallicRoughnessTexture)];
        float4            mr = texture.SampleLevel(sampler, uv, 0);
        metallic *= mr.b;
        roughness *= mr.g;
    }
    float3 emissive = material.emissiveFactor;
    if (material.emissiveTexture != INVALID_TEXTURE)
    {
        Texture2D<float4> texture = ResourceDescriptorHeap[NonUniformResourceIndex(material.emissiveTexture)];
        emissive *= texture.SampleLevel(sampler, uv, 0).rgb;
    }
    StructuredBuffer<LightingFrameData> light = ResourceDescriptorHeap[lightingIndex];
    float3                              L = -normalize(light[0].lightDirection.xyz);
    float                               visibility = 1.0;
    if (shadowsEnabled != 0)
    {
        RaytracingAccelerationStructure scene = ResourceDescriptorHeap[tlasIndex];
        RayDesc                         ray = { hitPosition, 0.0, L, 1e4 };
        visibility = dot(geometricNormal, L) > 0.0 ? traceRTShadow(scene, ray, alphaTestMethod != 0) : 0.0;
    }
    PBRLighting lighting =
        evaluatePBRLighting(baseColor, metallic, roughness, N, V, L, visibility, 1.0, light[0], rtLightingBindings(), false);
    // One bounce: the hit is shadowed by an inline query, without recursive TraceRay.
    payload.color = emissive + (lighting.direct + lighting.indirectDiffuse);
}

// Only masked geometry invokes Any-Hit; opaque geometry is accepted by traversal.
[shader("anyhit")]
void anyHitFoliage(inout RayPayload payload, in BuiltInTriangleIntersectionAttributes attribs)
{
    GpuDrawData mesh = loadRTDraw(InstanceID());
    ByteAddressBuffer materials = ResourceDescriptorHeap[materialsIndex];
    GpuMaterialData   material = materials.Load<GpuMaterialData>(mesh.material * sizeof(GpuMaterialData));
    float             alpha = sampleRayTracingAlpha(mesh, material, PrimitiveIndex(), attribs.barycentrics);
    if (alpha < material.alphaCutoff)
    {
        IgnoreHit();
    }
}
