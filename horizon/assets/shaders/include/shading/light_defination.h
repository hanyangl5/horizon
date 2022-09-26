struct LightParams{
    float4 color_intensity; // r, g, b, intensity
    float4 position_type; // x, y, z, type
    float4 direction;
    float4 radius_inner_outer; // radius, innerradius, outerradius
};

#define MAX_DYNAMIC_LIGHT_COUNT 256

#define DIRECTIONAL_LIGHT 0
#define POINT_LIGHT 1
#define SPOT_LIGHT 2