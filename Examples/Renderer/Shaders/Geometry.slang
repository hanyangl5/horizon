#include "Common.slang"
#include "SceneData.slang"

cbuffer RootConstant0
{
    uint  instanceIndex;
    uint  materialIndex;
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

#include "Material.slang"

float4x4 LoadMatrix(ByteAddressBuffer buffer, uint offset)
{
    // Runtime matrices store four columns; construct then transpose HLSL's row vectors.
    return transpose(float4x4(asfloat(buffer.Load4(offset)), asfloat(buffer.Load4(offset + 16)), asfloat(buffer.Load4(offset + 32)),
                              asfloat(buffer.Load4(offset + 48))));
}

float3 TransformNormal(float4x4 world, float3 Normal)
{
    const float3 x = float3(world[0][0], world[1][0], world[2][0]);
    const float3 y = float3(world[0][1], world[1][1], world[2][1]);
    const float3 z = float3(world[0][2], world[1][2], world[2][2]);
    const float3 cofactor = cross(y, z) * Normal.x + cross(z, x) * Normal.y + cross(x, y) * Normal.z;
    return normalize(cofactor * (dot(x, cross(y, z)) < 0.0 ? -1.0 : 1.0));
}

struct PSInput
{
    float4                Position: SV_Position;
    float3                WorldPosition: TEXCOORD0;
    float3                Normal: TEXCOORD1;
    float2                Uv: TEXCOORD2;
    float4                CurrentClip: TEXCOORD3;
    float4                PreviousClip: TEXCOORD4;
    nointerpolation uint2 MaterialData: TEXCOORD5;
};

struct PSOutput
{
    float4 Emissive: SV_Target0;
    float4 NormalMaterial: SV_Target1;
    float4 BaseColorMetallic: SV_Target2;
    float4 MotionMaterialId: SV_Target3;
};

PSInput BuildVertex(uint vertexId, uint instance, uint material, uint id, uint positionBuffer, uint normalBuffer, uint uvBuffer)
{
    StructuredBuffer<FrameData> Frame = ResourceDescriptorHeap[frameIndex];
    ByteAddressBuffer           Instances = ResourceDescriptorHeap[instancesIndex];
    ByteAddressBuffer           Positions = ResourceDescriptorHeap[NonUniformResourceIndex(positionBuffer)];
    ByteAddressBuffer           Normals = ResourceDescriptorHeap[NonUniformResourceIndex(normalBuffer)];
    ByteAddressBuffer           Texcoords = ResourceDescriptorHeap[NonUniformResourceIndex(uvBuffer)];
    float3                      Position = asfloat(Positions.Load3(vertexId * sizeof(float3)));
    uint                        Normal = Normals.Load(vertexId * 4);
    uint                        Uv = Texcoords.Load(vertexId * 4);
    const float4x4              world = LoadMatrix(Instances, instance * 192 + 32);
    const float4x4              previousWorld = LoadMatrix(Instances, instance * 192 + 96);
    float4                      WorldPosition = mul(world, float4(Position, 1.0));
    PSInput                     output;
    output.CurrentClip = mul(Frame[0].viewProjection, WorldPosition);
    output.PreviousClip = mul(Frame[0].previousViewProjection, mul(previousWorld, float4(Position, 1.0)));
    output.Position = output.CurrentClip;
    output.WorldPosition = WorldPosition.xyz;
    output.Normal = TransformNormal(world, decodeVertexNormal(Normal));
    output.Uv = float2(f16tof32(Uv & 0xffffu), f16tof32(Uv >> 16));
    output.MaterialData = uint2(material, id);
    return output;
}

PSInput VSMain(uint vertexId : SV_VertexID)
{
    return BuildVertex(vertexId, instanceIndex, materialIndex, materialId, positionsIndex, normalsIndex, texcoordsIndex);
}

PSInput VSIndirect(uint vertexId : SV_VertexID)
{
    const uint        drawId = instanceIndex;
    ByteAddressBuffer draws = ResourceDescriptorHeap[drawsIndex];
    uint4             object = draws.Load4(drawId * 48);
    uint4             buffers = draws.Load4(drawId * 48 + 16);
    uint4             geometry = draws.Load4(drawId * 48 + 32);
    ByteAddressBuffer indices = ResourceDescriptorHeap[NonUniformResourceIndex(buffers.w)];
    uint              indexPosition = (geometry.y + vertexId) * geometry.x;
    uint              packed = indices.Load(indexPosition & ~3u);
    uint              index = geometry.x == 2 ? ((packed >> ((indexPosition & 2u) * 8u)) & 0xffffu) : packed;
    return BuildVertex(uint(int(index) + asint(geometry.z)), object.x, object.y, object.z, buffers.x, buffers.y, buffers.z);
}

float3x3 CotangentFrame(float3 N, float3 p, float2 Uv)
{
    float3 dp1 = ddx(p);
    float3 dp2 = ddy(p);
    float2 duv1 = ddx(Uv);
    float2 duv2 = ddy(Uv);

    float3 dp2perp = cross(dp2, N);
    float3 dp1perp = cross(N, dp1);
    float3 T = dp2perp * duv1.x + dp1perp * duv2.x;
    float3 B = dp2perp * duv1.y + dp1perp * duv2.y;

    float invmax = rsqrt(max(max(dot(T, T), dot(B, B)), 1e-20));

    float w = (dot(cross(N, T), B) < 0.0) ? -1.0 : 1.0;

    T = T * w;

    return transpose(float3x3(T * invmax, B * invmax, N));
}

float3 PerturbNormal(float3 n, float3 v, float3 normalSample, float2 Uv)
{
    float3   map = normalize(2.0 * normalSample - float3(1.0));
    float3x3 TBN = CotangentFrame(n, v, Uv);
    float3 perturbed = mul(TBN, map);
    return dot(perturbed, perturbed) > 1e-20 ? normalize(perturbed) : n;
}

void RunAlphaTest(float alpha, float alphaThreshold, float2 pixel)
{
    if (alphaThreshold > 0.0)
    {
        // http://alex-charlton.com/posts/Dithering_on_the_GPU/
        // https://forums.khronos.org/showthread.php/5091-screen-door-transparency
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
    SamplerState      SurfaceSampler = SamplerDescriptorHeap[samplerIndex];
    ByteAddressBuffer materials = ResourceDescriptorHeap[materialsIndex];
    GpuMaterialData   material = materials.Load<GpuMaterialData>(input.MaterialData.x * sizeof(GpuMaterialData));

    float4 baseColor = material.baseColorFactor;
    if (material.baseColorTexture != INVALID_TEXTURE)
    {
        Texture2D<float4> BaseColor = ResourceDescriptorHeap[NonUniformResourceIndex(material.baseColorTexture)];
        baseColor *= BaseColor.Sample(SurfaceSampler, input.Uv);
    }
    if (material.alphaMode == 1)
    {
        RunAlphaTest(baseColor.a, material.alphaCutoff / max(32.0 * fwidth(float(input.Uv.x)), 1.0), input.Position.xy);
    }

    float3 n = normalize(input.Normal);
    if (material.normalTexture != INVALID_TEXTURE)
    {
        Texture2D<float4> NormalMap = ResourceDescriptorHeap[NonUniformResourceIndex(material.normalTexture)];
        float3            normalSample = NormalMap.Sample(SurfaceSampler, input.Uv).xyz;
        // Bistro uses BC5 Normal maps: reconstruct Z before the reference RGB Normal mapping.
        float2 normalXY = normalSample.xy * 2.0 - 1.0;
        normalSample.z = 0.5 + 0.5 * sqrt(saturate(1.0 - dot(normalXY, normalXY)));
        if (length(normalSample) > 0.5)
        {
            n = PerturbNormal(n, input.WorldPosition, normalSample, input.Uv);
        }
    }

    n = frontFace ? n : -n;

    float roughness = material.roughnessFactor;
    float metallic = material.metallicFactor;
    if (material.metallicRoughnessTexture != INVALID_TEXTURE)
    {
        Texture2D<float4> MetallicRoughness = ResourceDescriptorHeap[NonUniformResourceIndex(material.metallicRoughnessTexture)];
        float4            metallicRoughness = MetallicRoughness.Sample(SurfaceSampler, input.Uv);
        roughness *= metallicRoughness.g;
        metallic *= metallicRoughness.b;
    }
    roughness = saturate(roughness);
    metallic = saturate(metallic);

    float3 Emissive = material.emissiveFactor;
    if (material.emissiveTexture != INVALID_TEXTURE)
    {
        Texture2D<float4> EmissiveTexture = ResourceDescriptorHeap[NonUniformResourceIndex(material.emissiveTexture)];
        Emissive *= EmissiveTexture.Sample(SurfaceSampler, input.Uv).rgb;
    }

    float2 currentNdc = input.CurrentClip.xy / input.CurrentClip.w;
    float2 previousNdc = input.PreviousClip.xy / input.PreviousClip.w;
    float2 motion = (currentNdc - previousNdc) * float2(0.5, -0.5);

    // Disney/UE default: Specular 0.5 maps to a dielectric F0 of 0.04.
    float2 packedMaterial = packMaterialProperties(roughness, 0.5);

    PSOutput output;
    output.Emissive = float4(Emissive, 0.0);
    output.NormalMaterial = float4(encodeOctNormal(n), packedMaterial.x, packedMaterial.y);
    output.BaseColorMetallic = float4(baseColor.rgb, metallic);
    output.MotionMaterialId = float4(encodeMotionVector(motion), min(input.MaterialData.y, 255u) / 255.0);
    return output;
}
