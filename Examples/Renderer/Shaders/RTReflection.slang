#include "RTShared.slang"

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
    float2 uv = (float2(pixel) + 0.5) / float2(size);
    // Load depth and normal from the same full-resolution pixel, without blending across silhouettes.
    int2   depthSize = int2(width, height);
    int2   sourcePixel = min(int2(uv * float2(depthSize)), depthSize - 1);
    float  depth = depthTexture.Load(int3(sourcePixel, 0));
    if (depth >= 0.9999 || tlasIndex == INVALID_TEXTURE)
    {
        output[pixel] = float4(0.0);
        return;
    }
    RaytracingAccelerationStructure scene = ResourceDescriptorHeap[tlasIndex];
    float2                          sourceUV = (float2(sourcePixel) + 0.5) / float2(depthSize);
    float3                          n = decodeOctNormal(normals.Load(int3(sourcePixel, 0)).xy);
    float3                          worldPos = reconstructWorldPosition(sourceUV, depth, frame[0].inverseViewProjection);
    float3                          viewDir = normalize(frame[0].eye.xyz - worldPos);
    RayDesc                         ray;
    ray.Origin = worldPos + n * 0.01;

    ray.Direction = reflect(-viewDir, n);
    ray.TMin = 0.01;
    ray.TMax = 1e6;
    RayPayload payload;
    payload.color = float3(0.0);
    payload.depth = 0;
    float3 accumulatedColor = float3(0.0, 0.0, 0.0);
    {
        TraceRay(scene, RAY_FLAG_FORCE_OPAQUE, 0xff, 0, 0, 0, ray, payload);
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
    RTDraw            draw = loadRTDraw(InstanceID());
    uint3             indices = loadRTTriangle(draw, PrimitiveIndex());
    float3            bary = float3(1.0 - attributes.barycentrics.x - attributes.barycentrics.y, attributes.barycentrics);
    ByteAddressBuffer normals = ResourceDescriptorHeap[NonUniformResourceIndex(draw.normals)];
    float3 normal = decodeVertexNormal(normals.Load(indices.x * 4)) * bary.x + decodeVertexNormal(normals.Load(indices.y * 4)) * bary.y +
                    decodeVertexNormal(normals.Load(indices.z * 4)) * bary.z;
    float3 N = normalize(mul(normal, (float3x3)WorldToObject3x4()));
    float3 V = -normalize(WorldRayDirection());
    if (dot(N, V) < 0.0)
    {
        N = -N;
    }
    float2       uv = interpolateRTUV(draw, indices, attributes.barycentrics);
    Material     material = loadRTMaterial(draw.material);
    SamplerState sampler = SamplerDescriptorHeap[samplerIndex];
    float3       baseColor = material.baseColor.rgb;
    if (material.baseColorTexture != INVALID_TEXTURE)
    {
        Texture2D<float4> texture = ResourceDescriptorHeap[NonUniformResourceIndex(material.baseColorTexture)];
        baseColor *= texture.SampleLevel(sampler, uv, 0).rgb;
    }
    float metallic = material.metallic;
    float roughness = material.roughness;
    if (material.metallicRoughnessTexture != INVALID_TEXTURE)
    {
        Texture2D<float4> texture = ResourceDescriptorHeap[NonUniformResourceIndex(material.metallicRoughnessTexture)];
        float4            mr = texture.SampleLevel(sampler, uv, 0);
        metallic *= mr.b;
        roughness *= mr.g;
    }
    float3 emissive = material.emissive;
    if (material.emissiveTexture != INVALID_TEXTURE)
    {
        Texture2D<float4> texture = ResourceDescriptorHeap[NonUniformResourceIndex(material.emissiveTexture)];
        emissive *= texture.SampleLevel(sampler, uv, 0).rgb;
    }
    StructuredBuffer<LightingFrameData> light = ResourceDescriptorHeap[lightingIndex];
    float3                              L = -normalize(light[0].lightDirection.xyz);
    PBRLighting lighting = evaluatePBRLighting(baseColor, metallic, roughness, N, V, L, 1.0, 1.0, light[0], rtLightingBindings(), false);
    // Reference renderer uses one unshadowed bounce and no recursive TraceRay in closest-hit.
    payload.color = emissive + (lighting.direct + lighting.indirectDiffuse + lighting.indirectSpecular);
}
