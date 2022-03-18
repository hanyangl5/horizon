#version 450

#define MAX_LIGHT_COUNT 512

layout(location = 0) in vec2 fragTexCoord;

layout(location = 0) out vec4 outColor;

// set 0: scene

struct LightParams{
    vec4 colorIntensity; // r, g, b, intensity
    vec4 positionType; // x, y, z, type
    vec4 direction;
    vec4 radiusInnerOuter; // radius, innerradius, outerradius
};

layout(set = 0, binding = 1) uniform LightCountUb {
    uint lightCount;
}lightCountUb;

layout(set = 0, binding = 2) uniform LightUb {
    LightParams lights[MAX_LIGHT_COUNT];
}lightUb;


// set 1: material

layout(set = 1, binding = 0) uniform MaterialParams {
    vec4 bcFactor;
    vec2 mrFactor;
}materialParams;

layout(set = 1, binding = 1) uniform sampler2D bcTexture;
layout(set = 1, binding = 2) uniform sampler2D normalTexture;
layout(set = 1, binding = 3) uniform sampler2D mrTexture;
// set 2: mesh

float brdf() {
    return 0.0f;
}

vec3 radiance(LightParams light) {

    vec3 albedo = texture(bcTexture, fragTexCoord).xyz;
    vec3 normal = texture(normalTexture, fragTexCoord).xyz;
    float metallic= texture(mrTexture, fragTexCoord).x;
    float roughness = texture(mrTexture, fragTexCoord).y;

    if(light.positionType.w == 0.0f) {

    }
    else if(light.positionType.w == 1.0f) {

    }
    else if (light.positionType.w == 2.0f) {

    }
    return light.colorIntensity.w * light.colorIntensity.xyz * albedo;
}

void main() {

    vec3 color = vec3(0.0f);
    
    for(uint i = 0; i < lightCountUb.lightCount; i++) {
        color += radiance(lights[i]);
    }
    outColor = color;

}