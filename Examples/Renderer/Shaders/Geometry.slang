#include "Common.slang"

cbuffer RootConstant0
{
    uint  instanceSlot;
    uint  materialSlot;
    uint  materialId;
    uint  frameIndex;
    uint  instancesIndex;
    uint  materialsIndex;
    uint  texturesIndex;
    uint  samplerIndex;
    uint  positionsIndex;
    uint  normalsIndex;
    uint  texcoordsIndex;
    uint  drawsIndex;
    uint  oitHeadsIndex;
    uint  oitCounterIndex;
    uint  oitNodesIndex;
    uint  oitMaxNodes;
    uint  lightingFrameIndex;
    uint  shadowMapIndex;
    uint  shadowSamplerIndex;
    uint  environmentIndex;
    uint  irradianceIndex;
    uint  brdfLutIndex;
    uint  environmentSamplerIndex;
    uint  oitOpaqueIndex;
    float oitOpacityBoost;
    uint  oitShowHeatmap;
    uint  oitOutputIndex;
    float oitTime;
};

float4x4 loadMatrix(ByteAddressBuffer buffer, uint offset)
{
    // Runtime matrices store four columns; construct then transpose HLSL's row vectors.
    return transpose(float4x4(asfloat(buffer.Load4(offset)), asfloat(buffer.Load4(offset + 16)), asfloat(buffer.Load4(offset + 32)),
                              asfloat(buffer.Load4(offset + 48))));
}

uint resolveTexture(uint2 id)
{
    if (id.x == INVALID_TEXTURE || id.y == 0)
    {
        return INVALID_TEXTURE;
    }
    ByteAddressBuffer textures = ResourceDescriptorHeap[texturesIndex];
    const uint        offset = id.x * 12;
    return all(textures.Load2(offset) == id) ? textures.Load(offset + 8) : INVALID_TEXTURE;
}

Material loadMaterial(uint slot)
{
    ByteAddressBuffer materials = ResourceDescriptorHeap[materialsIndex];
    const uint        offset = slot * 88;
    Material          material;
    material.baseColorTexture = resolveTexture(materials.Load2(offset + 8));
    material.normalTexture = resolveTexture(materials.Load2(offset + 16));
    material.metallicRoughnessTexture = resolveTexture(materials.Load2(offset + 24));
    material.emissiveTexture = resolveTexture(materials.Load2(offset + 32));
    material.baseColor = asfloat(materials.Load4(offset + 40));
    material.emissive = asfloat(materials.Load3(offset + 56));
    material.metallic = asfloat(materials.Load(offset + 68));
    material.roughness = asfloat(materials.Load(offset + 72));
    material.alphaCutoff = asfloat(materials.Load(offset + 76));
    material.alphaMode = materials.Load(offset + 80);
    material.doubleSided = materials.Load(offset + 84);
    return material;
}

float3 transformNormal(float4x4 world, float3 normal)
{
    const float3 x = float3(world[0][0], world[1][0], world[2][0]);
    const float3 y = float3(world[0][1], world[1][1], world[2][1]);
    const float3 z = float3(world[0][2], world[1][2], world[2][2]);
    const float3 cofactor = cross(y, z) * normal.x + cross(z, x) * normal.y + cross(x, y) * normal.z;
    return normalize(cofactor * (dot(x, cross(y, z)) < 0.0 ? -1.0 : 1.0));
}

struct PSInput
{
    float4                position: SV_Position;
    float3                worldPosition: TEXCOORD0;
    float3                normal: TEXCOORD1;
    float2                uv: TEXCOORD2;
    float4                currentClip: TEXCOORD3;
    float4                previousClip: TEXCOORD4;
    nointerpolation uint2 materialData: TEXCOORD5;
};

struct PSOutput
{
    float4 emissive: SV_Target0;
    float4 normalMaterial: SV_Target1;
    float4 baseColorMetallic: SV_Target2;
    float4 motionMaterialId: SV_Target3;
};

PSInput buildVertex(uint vertexId, uint instance, uint material, uint id, uint positionBuffer, uint normalBuffer, uint uvBuffer)
{
    StructuredBuffer<FrameData> Frame = ResourceDescriptorHeap[frameIndex];
    ByteAddressBuffer           Instances = ResourceDescriptorHeap[instancesIndex];
    ByteAddressBuffer           Positions = ResourceDescriptorHeap[NonUniformResourceIndex(positionBuffer)];
    ByteAddressBuffer           Normals = ResourceDescriptorHeap[NonUniformResourceIndex(normalBuffer)];
    ByteAddressBuffer           Texcoords = ResourceDescriptorHeap[NonUniformResourceIndex(uvBuffer)];
    float3                      position = asfloat(Positions.Load3(vertexId * 12));
    uint                        normal = Normals.Load(vertexId * 4);
    uint                        uv = Texcoords.Load(vertexId * 4);
    const float4x4              world = loadMatrix(Instances, instance * 192 + 32);
    const float4x4              previousWorld = loadMatrix(Instances, instance * 192 + 96);
    float4                      worldPosition = mul(world, float4(position, 1.0));
    PSInput                     output;
    output.currentClip = mul(Frame[0].viewProjection, worldPosition);
    output.previousClip = mul(Frame[0].previousViewProjection, mul(previousWorld, float4(position, 1.0)));
    output.position = output.currentClip;
    output.worldPosition = worldPosition.xyz;
    output.normal = transformNormal(world, decodeVertexNormal(normal));
    output.uv = float2(f16tof32(uv & 0xffffu), f16tof32(uv >> 16));
    output.materialData = uint2(material, id);
    return output;
}

PSInput VSMain(uint vertexId : SV_VertexID)
{
    return buildVertex(vertexId, instanceSlot, materialSlot, materialId, positionsIndex, normalsIndex, texcoordsIndex);
}

PSInput VSIndirect(uint vertexId : SV_VertexID)
{
    const uint        drawId = instanceSlot;
    ByteAddressBuffer draws = ResourceDescriptorHeap[drawsIndex];
    uint4             object = draws.Load4(drawId * 48);
    uint4             buffers = draws.Load4(drawId * 48 + 16);
    uint4             geometry = draws.Load4(drawId * 48 + 32);
    ByteAddressBuffer indices = ResourceDescriptorHeap[NonUniformResourceIndex(buffers.w)];
    uint              indexPosition = (geometry.y + vertexId) * geometry.x;
    uint              packed = indices.Load(indexPosition & ~3u);
    uint              index = geometry.x == 2 ? ((packed >> ((indexPosition & 2u) * 8u)) & 0xffffu) : packed;
    return buildVertex(uint(int(index) + asint(geometry.z)), object.x, object.y, object.z, buffers.x, buffers.y, buffers.z);
}

float3x3 cotangentFrame(float3 N, float3 p, float2 uv)
{
    float3 dp1 = ddx(p);
    float3 dp2 = ddy(p);
    float2 duv1 = ddx(uv);
    float2 duv2 = ddy(uv);

    float3 dp2perp = cross(dp2, N);
    float3 dp1perp = cross(N, dp1);
    float3 T = dp2perp * duv1.x + dp1perp * duv2.x;
    float3 B = dp2perp * duv1.y + dp1perp * duv2.y;

    float invmax = rsqrt(max(dot(T, T), dot(B, B)));

    float w = (dot(cross(N, T), B) < 0.0) ? -1.0 : 1.0;

    T = T * w;

    return transpose(float3x3(T * invmax, B * invmax, N));
}

float3 perturbNormal(float3 n, float3 v, float3 normalSample, float2 uv)
{
    float3   map = normalize(2.0 * normalSample - float3(1.0));
    float3x3 TBN = cotangentFrame(n, v, uv);
    return normalize(mul(TBN, map));
}

void runAlphaTest(float alpha, float alphaThreshold, float2 pixel)
{
    if (alphaThreshold > 0.0)
    {
        float4x4 thresholdMatrix =
            float4x4(1.0 / 17.0, 9.0 / 17.0, 3.0 / 17.0, 11.0 / 17.0, 13.0 / 17.0, 5.0 / 17.0, 15.0 / 17.0, 7.0 / 17.0, 4.0 / 17.0,
                     12.0 / 17.0, 2.0 / 17.0, 10.0 / 17.0, 16.0 / 17.0, 8.0 / 17.0, 14.0 / 17.0, 6.0 / 17.0);

        alpha = clamp(alpha - 0.5 * thresholdMatrix[int(fmod(pixel.x, 4.0))][int(fmod(pixel.y, 4.0))], 0.0, 1.0);

        if (alpha < alphaThreshold)
        {
            discard;
        }
    }
}

PSOutput PSMain(PSInput input, bool frontFace : SV_IsFrontFace)
{
    SamplerState SurfaceSampler = SamplerDescriptorHeap[samplerIndex];
    Material     material = loadMaterial(input.materialData.x);

    float4 baseColor = material.baseColor;
    if (material.baseColorTexture != INVALID_TEXTURE)
    {
        Texture2D<float4> BaseColor = ResourceDescriptorHeap[NonUniformResourceIndex(material.baseColorTexture)];
        baseColor *= BaseColor.Sample(SurfaceSampler, input.uv);
    }
    if (material.alphaMode == 1)
    {
        runAlphaTest(baseColor.a, material.alphaCutoff / max(32.0 * fwidth(float(input.uv.x)), 1.0), input.position.xy);
    }

    float3 n = normalize(input.normal);
    if (material.normalTexture != INVALID_TEXTURE)
    {
        Texture2D<float4> NormalMap = ResourceDescriptorHeap[NonUniformResourceIndex(material.normalTexture)];
        float3            normalSample = NormalMap.Sample(SurfaceSampler, input.uv).xyz;
        // Bistro uses BC5 normal maps: reconstruct Z before the reference RGB normal mapping.
        float2 normalXY = normalSample.xy * 2.0 - 1.0;
        normalSample.z = 0.5 + 0.5 * sqrt(saturate(1.0 - dot(normalXY, normalXY)));
        if (length(normalSample) > 0.5)
        {
            n = perturbNormal(n, input.worldPosition, normalSample, input.uv);
        }
    }

    n = frontFace ? n : -n;

    float roughness = material.roughness;
    float metallic = material.metallic;
    if (material.metallicRoughnessTexture != INVALID_TEXTURE)
    {
        Texture2D<float4> MetallicRoughness = ResourceDescriptorHeap[NonUniformResourceIndex(material.metallicRoughnessTexture)];
        float4            metallicRoughness = MetallicRoughness.Sample(SurfaceSampler, input.uv);
        roughness *= metallicRoughness.g;
        metallic *= metallicRoughness.b;
    }
    roughness = saturate(roughness);
    metallic = saturate(metallic);

    float3 emissive = material.emissive;
    if (material.emissiveTexture != INVALID_TEXTURE)
    {
        Texture2D<float4> Emissive = ResourceDescriptorHeap[NonUniformResourceIndex(material.emissiveTexture)];
        emissive *= Emissive.Sample(SurfaceSampler, input.uv).rgb;
    }

    float2 currentNdc = input.currentClip.xy / input.currentClip.w;
    float2 previousNdc = input.previousClip.xy / input.previousClip.w;
    float2 motion = (currentNdc - previousNdc) * float2(0.5, -0.5);

    // Disney/UE default: Specular 0.5 maps to a dielectric F0 of 0.04.
    float2 packedMaterial = packMaterialProperties(roughness, 0.5);

    PSOutput output;
    output.emissive = float4(emissive, 0.0);
    output.normalMaterial = float4(encodeOctNormal(n), packedMaterial.x, packedMaterial.y);
    output.baseColorMetallic = float4(baseColor.rgb, metallic);
    output.motionMaterialId = float4(encodeMotionVector(motion), min(input.materialData.y, 255u) / 255.0);
    return output;
}
