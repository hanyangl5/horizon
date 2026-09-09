#include "Common.hlsl"

Texture2D<float4> EmissiveBuffer : register(t0);
Texture2D<float4> NormalMaterialBuffer : register(t1);
Texture2D<float4> BaseColorMetallicBuffer : register(t2);
Texture2D<float> DepthBuffer : register(t3);
StructuredBuffer<FrameData> Frame : register(t4);

struct PSInput {
  float4 position : SV_Position;
  float2 uv : TEXCOORD0;
};

PSInput VSMain(uint vertexId : SV_VertexID) {
  float2 uv = float2((vertexId << 1) & 2, vertexId & 2);
  PSInput output;
  output.position = float4(uv * float2(2.0, -2.0) + float2(-1.0, 1.0),
                           0.0, 1.0);
  output.uv = uv;
  return output;
}

float3 reconstructWorldPosition(float2 uv, float depth) {
  float4 clip = float4(uv * float2(2.0, -2.0) + float2(-1.0, 1.0),
                       depth, 1.0);
  float4 world = mul(Frame[0].inverseViewProjection, clip);
  return world.xyz / world.w;
}

float distributionGGX(float NoH, float roughness) {
  float alpha = roughness * roughness;
  float alphaSquared = alpha * alpha;
  float denominator = NoH * NoH * (alphaSquared - 1.0) + 1.0;
  return alphaSquared / max(PI * denominator * denominator, 1e-6);
}

float visibilitySmithGGX(float NoV, float NoL, float roughness) {
  float alpha = roughness * roughness;
  float alphaSquared = alpha * alpha;
  float lambdaV = NoL *
      sqrt(max(NoV * NoV * (1.0 - alphaSquared) + alphaSquared, 0.0));
  float lambdaL = NoV *
      sqrt(max(NoL * NoL * (1.0 - alphaSquared) + alphaSquared, 0.0));
  return 0.5 / max(lambdaV + lambdaL, 1e-6);
}

float3 fresnelSchlick(float VoH, float3 f0) {
  float factor = pow(1.0 - VoH, 5.0);
  return f0 + (1.0 - f0) * factor;
}

float3 acesFilm(float3 color) {
  return saturate((color * (2.51 * color + 0.03)) /
                  (color * (2.43 * color + 0.59) + 0.14));
}

float4 PSMain(PSInput input) : SV_Target0 {
  int2 pixel = int2(input.position.xy);
  float depth = DepthBuffer.Load(int3(pixel, 0));
  if (depth >= 1.0)
    return float4(0.02, 0.035, 0.055, 1.0);

  float3 emissive = EmissiveBuffer.Load(int3(pixel, 0)).rgb;
  float4 normalMaterial = NormalMaterialBuffer.Load(int3(pixel, 0));
  float4 baseColorMetallic =
      BaseColorMetallicBuffer.Load(int3(pixel, 0));
  float3 normal = decodeOctNormal(normalMaterial.rg);
  float2 material = unpackMaterialProperties(normalMaterial.ba);
  float roughness = material.x;
  float specular = material.y;
  float3 baseColor = baseColorMetallic.rgb;
  float metallic = baseColorMetallic.a;

  float3 worldPosition = reconstructWorldPosition(input.uv, depth);
  float3 view = normalize(Frame[0].eye.xyz - worldPosition);
  float3 light = normalize(float3(-0.4, 0.8, 0.3));
  float3 halfVector = normalize(view + light);
  float NoV = max(dot(normal, view), 1e-4);
  float NoL = saturate(dot(normal, light));
  float NoH = saturate(dot(normal, halfVector));
  float VoH = saturate(dot(view, halfVector));

  float3 f0 = lerp((0.08 * specular).xxx, baseColor, metallic);
  float3 fresnel = fresnelSchlick(VoH, f0);
  float distribution = distributionGGX(NoH, roughness);
  float visibility = visibilitySmithGGX(NoV, NoL, roughness);
  float3 specularBrdf = distribution * visibility * fresnel;
  float3 diffuseBrdf =
      (1.0 - fresnel) * (1.0 - metallic) * baseColor / PI;

  const float3 sunColor = float3(1.0, 0.96, 0.88) * 3.0;
  float3 color = (diffuseBrdf + specularBrdf) * sunColor * NoL;
  color += emissive;

  return float4(acesFilm(color), 1.0);
}
