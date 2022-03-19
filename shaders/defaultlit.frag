#version 450

#define MAX_LIGHT_COUNT 512
#define PI 3.14159265359;
layout(location = 0) in vec3 worldPos;
layout(location = 1) in vec3 worldNormal;
layout(location = 2) in vec2 fragTexCoord;

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

layout(set = 0, binding = 3) uniform CameraUb {
    vec3 eyePos;
}cameraUb;

// set 1: material

layout(set = 1, binding = 0) uniform MaterialParams {
    vec4 bcFactor;
    vec2 mrFactor;
}materialParams;

layout(set = 1, binding = 1) uniform sampler2D bcTexture;
layout(set = 1, binding = 2) uniform sampler2D normalTexture;
layout(set = 1, binding = 3) uniform sampler2D mrTexture;
// set 2: mesh

float D_GGX(float a2, float NoH) {
	// float d = ( NoH * a2 - NoH ) * NoH + 1;	// 2 mad
	// return a2 / ( PI*d*d );					// 4 mul, 1 rcp
    return 1.0f;
}

float G_Smith() {
	// float Vis_SmithV = NoV + sqrt( NoV * (NoV - NoV * a2) + a2 );
	// float Vis_SmithL = NoL + sqrt( NoL * (NoL - NoL * a2) + a2 );
	// return rcp( Vis_SmithV * Vis_SmithL );
    return 1.0f;
}

float F_Schilick(float VoH, vec3 F0) {
    // return F0 + (1.0 - F0) * pow(clamp(1.0 - VoH, 0.0, 1.0), 5.0);
    return 1.0f;
}

float brdf() {
    return 1.0f;
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
    vec3 viewDir = normalize(worldPos-cameraUb.eyePos);
    vec3 N = worldNormal;
    float NoV = dot(N,viewDir);
    vec3 color = vec3(0.0f);
    
    for(uint i = 0; i < lightCountUb.lightCount; i++) {
        color += radiance(lights[i]);
    }
    outColor = color;

}