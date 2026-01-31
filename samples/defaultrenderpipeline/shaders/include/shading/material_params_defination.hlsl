#ifndef MATERIAL_PARAMS_DEFINATION_HLSL
#define MATERIAL_PARAMS_DEFINATION_HLSL

#define HAS_BASE_COLOR 0x01
#define HAS_NORMAL 0x10
#define HAS_METALLIC_ROUGHNESS 0x100
#define HAS_EMISSIVE 0x1000
#define HAS_ALPHA 0x10000

struct MaterialProperties {
    float3 albedo;
    float3 normal;
    float3 f0;
    float metallic;
    float roughness;
    float roughness2;
    float3 emissive;
    float anisotropic;
    float sheen;
    float sheen_tint;
    float subsurface;
    float clearcoat;
    float clearcoat_gloss;
};

#define BLEND_STATE_OPAQUE 0
#define BLEND_STATE_MASKED 1

#endif
