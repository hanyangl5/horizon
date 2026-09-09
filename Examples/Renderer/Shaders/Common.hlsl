static const uint INVALID_TEXTURE = 0xffffffffu;
static const float PI = 3.14159265359;

struct FrameData {
  float4x4 viewProjection;
  float4x4 previousViewProjection;
  float4x4 inverseViewProjection;
  float4 eye;
};

struct DrawData {
  float4x4 world;
  float4x4 normal;
  uint material;
  float alphaCutoff;
  uint2 padding;
};

struct Material {
  uint bHasBaseColor;
  uint bHasNormalMap;
  uint bHasMetallicRoughness;
  uint bHasEmissive;
  float4 baseColor;
  float3 emissive;
  float metallic;
  float roughness;
  float3 padding;
};

float2 signNotZero(float2 value) {
  return float2(value.x >= 0.0 ? 1.0 : -1.0,
                value.y >= 0.0 ? 1.0 : -1.0);
}

float2 encodeOctNormal(float3 normal) {
  normal /= dot(abs(normal), 1.0.xxx);
  if (normal.z < 0.0)
    normal.xy = (1.0 - abs(normal.yx)) * signNotZero(normal.xy);
  return normal.xy * 0.5 + 0.5;
}

float3 decodeOctNormal(float2 encoded) {
  float2 oct = encoded * 2.0 - 1.0;
  float3 normal = float3(oct, 1.0 - abs(oct.x) - abs(oct.y));
  if (normal.z < 0.0)
    normal.xy = (1.0 - abs(normal.yx)) * signNotZero(normal.xy);
  return normalize(normal);
}

float3 decodeVertexNormal(uint packed) {
  float2 oct = float2(packed & 0xffffu, packed >> 16) / 65535.0;
  return decodeOctNormal(oct);
}

float2 packMaterialProperties(float roughness, float specular) {
  uint packed = (uint)round(saturate(roughness) * 255.0);
  packed |= (uint)round(saturate(specular) * 15.0) << 8;
  return float2(float(packed & 0x3ffu) / 1023.0,
                float(packed >> 10) / 3.0);
}

float2 unpackMaterialProperties(float2 encoded) {
  uint packed = (uint)round(saturate(encoded.x) * 1023.0);
  packed |= (uint)round(saturate(encoded.y) * 3.0) << 10;
  return float2(float(packed & 0xffu) / 255.0,
                float((packed >> 8) & 0xfu) / 15.0);
}

float3 encodeMotionVector(float2 motion) {
  uint2 quantized = (uint2)round(saturate(motion * 0.5 + 0.5) * 4095.0);
  uint packed = quantized.x | (quantized.y << 12);
  return float3(packed & 0xffu, (packed >> 8) & 0xffu,
                (packed >> 16) & 0xffu) / 255.0;
}

float2 decodeMotionVector(float3 encoded) {
  uint3 bytes = (uint3)round(saturate(encoded) * 255.0);
  uint packed = bytes.x | (bytes.y << 8) | (bytes.z << 16);
  uint2 quantized = uint2(packed & 0xfffu, (packed >> 12) & 0xfffu);
  return float2(quantized) / 4095.0 * 2.0 - 1.0;
}
