#include "Common.hlsl"

StructuredBuffer<FrameData> Frame : register(t0);
StructuredBuffer<DrawData> Draws : register(t1);
StructuredBuffer<Material> Materials : register(t2);
Texture2D BaseColor : register(t3);
Texture2D NormalMap : register(t4);
Texture2D MetallicRoughness : register(t5);
Texture2D Emissive : register(t6);
SamplerState SurfaceSampler : register(s0);
cbuffer RootConstant0 : register(b0) { uint drawId; };

struct VSInput {
  float3 position : POSITION;
  uint normal : NORMAL;
  uint uv : TEXCOORD0;
};

struct PSInput {
  float4 position : SV_Position;
  float3 worldPosition : TEXCOORD0;
  float3 normal : TEXCOORD1;
  float2 uv : TEXCOORD2;
  noperspective float4 currentClip : TEXCOORD3;
  noperspective float4 previousClip : TEXCOORD4;
};

struct PSOutput {
  float4 emissive : SV_Target0;
  float4 normalMaterial : SV_Target1;
  float4 baseColorMetallic : SV_Target2;
  float4 motionMaterialId : SV_Target3;
};

PSInput VSMain(VSInput input) {
  DrawData draw = Draws[drawId];
  float4 worldPosition = mul(draw.world, float4(input.position, 1.0));

  PSInput output;
  output.currentClip = mul(Frame[0].viewProjection, worldPosition);
  output.previousClip = mul(Frame[0].previousViewProjection, worldPosition);
  output.position = output.currentClip;
  output.worldPosition = worldPosition.xyz;
  output.normal = normalize(mul((float3x3)draw.normal,
                                decodeVertexNormal(input.normal)));
  output.uv = float2(f16tof32(input.uv & 0xffffu),
                     f16tof32(input.uv >> 16));
  return output;
}

float3 applyNormalMap(PSInput input, float3 normal) {
  float3 positionDx = ddx(input.worldPosition);
  float3 positionDy = ddy(input.worldPosition);
  float2 uvDx = ddx(input.uv);
  float2 uvDy = ddy(input.uv);
  float3 positionDyPerp = cross(positionDy, normal);
  float3 positionDxPerp = cross(normal, positionDx);
  float3 tangent = positionDyPerp * uvDx.x + positionDxPerp * uvDy.x;
  float3 bitangent = positionDyPerp * uvDx.y + positionDxPerp * uvDy.y;
  float scale = rsqrt(max(max(dot(tangent, tangent), dot(bitangent, bitangent)),
                          1e-10));

  float2 mappedXY = NormalMap.Sample(SurfaceSampler, input.uv).xy * 2.0 - 1.0;
  float3 mapped = float3(mappedXY,
                         sqrt(saturate(1.0 - dot(mappedXY, mappedXY))));
  return normalize(tangent * (mapped.x * scale) +
                   bitangent * (mapped.y * scale) + normal * mapped.z);
}

PSOutput PSMain(PSInput input, bool frontFace : SV_IsFrontFace) {
  DrawData draw = Draws[drawId];
  Material material = Materials[draw.material];

  float4 baseColor = material.baseColor;
  if (material.bHasBaseColor != INVALID_TEXTURE)
    baseColor *= BaseColor.Sample(SurfaceSampler, input.uv);
  clip(baseColor.a - draw.alphaCutoff);

  float3 normal = normalize(input.normal) * (frontFace ? 1.0 : -1.0);
  if (material.bHasNormalMap != INVALID_TEXTURE)
    normal = applyNormalMap(input, normal);

  float roughness = material.roughness;
  float metallic = material.metallic;
  if (material.bHasMetallicRoughness != INVALID_TEXTURE) {
    float4 metallicRoughness =
        MetallicRoughness.Sample(SurfaceSampler, input.uv);
    roughness *= metallicRoughness.g;
    metallic *= metallicRoughness.b;
  }
  roughness = clamp(roughness, 0.08, 1.0);
  metallic = saturate(metallic);

  float3 emissive = material.emissive;
  if (material.bHasEmissive != INVALID_TEXTURE)
    emissive *= Emissive.Sample(SurfaceSampler, input.uv).rgb;

  float2 currentNdc = input.currentClip.xy / input.currentClip.w;
  float2 previousNdc = input.previousClip.xy / input.previousClip.w;
  float2 motion = (currentNdc - previousNdc) * float2(0.5, -0.5);

  // Disney/UE default: Specular 0.5 maps to a dielectric F0 of 0.04.
  float2 packedMaterial = packMaterialProperties(roughness, 0.5);

  PSOutput output;
  output.emissive = float4(emissive, 0.0);
  output.normalMaterial =
      float4(encodeOctNormal(normal), packedMaterial.x, packedMaterial.y);
  output.baseColorMetallic = float4(baseColor.rgb, metallic);
  output.motionMaterialId =
      float4(encodeMotionVector(motion), min(draw.material, 255u) / 255.0);
  return output;
}
