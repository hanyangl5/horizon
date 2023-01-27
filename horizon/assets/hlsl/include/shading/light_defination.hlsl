struct LightParams {
    float3 color; // r, g, b, intensity
    float intensity;
    float3 position;   // x, y, z, type
    uint type;
    float3 direction;
    float falloff;
    float2 spot_cone_inner_outer; // radius, innerradius, outerradius
    float2 radius_length;
    float3 orientation;
    float pad;
};

#define MAX_DYNAMIC_LIGHT_COUNT 256

#define DIRECTIONAL_LIGHT 0
#define POINT_LIGHT 1
#define SPOT_LIGHT 2