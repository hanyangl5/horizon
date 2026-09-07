struct FrameData { float4x4 viewProjection; float4 eye; };
struct DrawData { float4x4 world; float4x4 normal; uint material; float alphaCutoff; uint2 padding; };
struct Material {
    uint baseTexture; uint normalTexture; uint metallicRoughnessTexture; uint emissiveTexture;
    float4 baseColor; float3 emissive; float metallic; float roughness; float3 padding;
};
StructuredBuffer<FrameData> Frame : register(t0);
StructuredBuffer<DrawData> Draws : register(t1);
StructuredBuffer<Material> Materials : register(t2);
Texture2D BaseColor : register(t3);
Texture2D NormalMap : register(t4);
Texture2D MetallicRoughness : register(t5);
Texture2D Emissive : register(t6);
SamplerState SurfaceSampler : register(s0);
cbuffer RootConstant0 : register(b0) { uint drawId; };
struct VSInput { float3 position : POSITION; uint normal : NORMAL; uint uv : TEXCOORD0; };
struct Varyings {
    float4 position : SV_Position; float3 worldPosition : TEXCOORD0;
    float3 normal : TEXCOORD1; float2 uv : TEXCOORD2;
};

float3 decodeNormal(uint packed) {
    float2 e = float2(packed & 65535, packed >> 16) / 65535.0 * 2.0 - 1.0;
    float3 n = float3(e, 1.0 - abs(e.x) - abs(e.y));
    if (n.z < 0) n.xy = (1.0 - abs(n.yx)) * float2(n.x >= 0 ? 1 : -1, n.y >= 0 ? 1 : -1);
    return normalize(n);
}

Varyings VSMain(VSInput input) {
    DrawData d = Draws[drawId];
    Varyings o;
    float4 world = mul(d.world, float4(input.position, 1));
    o.position = mul(Frame[0].viewProjection, world);
    o.worldPosition = world.xyz;
    o.normal = normalize(mul((float3x3)d.normal, decodeNormal(input.normal)));
    o.uv = float2(f16tof32(input.uv & 65535), f16tof32(input.uv >> 16));
    return o;
}
float4 PSMain(Varyings input, bool frontFace : SV_IsFrontFace) : SV_Target0 {
    DrawData d = Draws[drawId];
    Material m = Materials[d.material];
    float4 base = m.baseColor;
    if (m.baseTexture != 0xffffffff) base *= BaseColor.Sample(SurfaceSampler, input.uv);
    clip(base.a - d.alphaCutoff);
    float3 N = normalize(input.normal) * (frontFace ? 1.0 : -1.0);
    if (m.normalTexture != 0xffffffff) {
        float3 q1 = ddx(input.worldPosition), q2 = ddy(input.worldPosition);
        float2 st1 = ddx(input.uv), st2 = ddy(input.uv);
        float3 T = q1 * st2.y - q2 * st1.y;
        float3 B = q2 * st1.x - q1 * st2.x;
        float invLength = rsqrt(max(max(dot(T,T), dot(B,B)), 1e-10));
        float2 normalXY = NormalMap.Sample(SurfaceSampler, input.uv).xy * 2.0 - 1.0;
        float3 mapped = float3(normalXY, sqrt(saturate(1.0-dot(normalXY, normalXY))));
        N = normalize(T * invLength * mapped.x + B * invLength * mapped.y + N * mapped.z);
    }
    float roughness = m.roughness, metallic = m.metallic;
    if (m.metallicRoughnessTexture != 0xffffffff) {
        float4 mr = MetallicRoughness.Sample(SurfaceSampler, input.uv);
        roughness *= mr.g; metallic *= mr.b;
    }
    roughness = clamp(roughness, 0.08, 1.0);
    float3 L = normalize(float3(-0.4, 0.8, 0.3));
    float3 V = normalize(Frame[0].eye.xyz - input.worldPosition);
    float3 H = normalize(L + V);
    float nl = saturate(dot(N,L)), nv = max(saturate(dot(N,V)), 0.001);
    float nh = saturate(dot(N,H)), vh = saturate(dot(V,H));
    float a = roughness * roughness, a2 = a*a;
    float denom = nh*nh*(a2-1.0)+1.0;
    float D = a2 / max(3.14159265*denom*denom, 1e-5);
    float k = (roughness+1.0)*(roughness+1.0)/8.0;
    float G = nv/(nv*(1-k)+k) * nl/(nl*(1-k)+k);
    float3 F = lerp(0.04.xxx, base.rgb, metallic);
    F += (1.0-F)*pow(1.0-vh, 5.0);
    float3 color = ((1-F)*(1-metallic)*base.rgb/3.14159265 + D*G*F/max(4*nv*nl, 0.001))*nl*3.0;
    color += base.rgb * lerp(float3(0.10,0.09,0.08), float3(0.28,0.34,0.42), N.y*0.5+0.5);
    float3 emission = m.emissive;
    if (m.emissiveTexture != 0xffffffff) emission *= Emissive.Sample(SurfaceSampler, input.uv).rgb;
    color += emission;
    color = color / (1.0 + color);
    return float4(color, 1);
}