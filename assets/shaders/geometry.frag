#version 450

#define MAX_LIGHT_COUNT 512
#define PI 3.14159265359
#define eps 1e-6

layout(location = 0) in vec3 worldPos;
layout(location = 1) in vec3 worldNormal;
layout(location = 2) in vec2 fragTexCoord;

//layout(location = 0) out vec3 outColor;
layout(location = 0) out vec4 positionDepth;
layout(location = 1) out vec4 normalRoughness;
layout(location = 2) out vec4 albedoMetallic;

// set 0: scene
layout(set = 0, binding = 0) uniform SceneUb {
    mat4 view, proj;
    vec2 nearFar;
} sceneUb;
// set 1: material

layout(set = 1, binding = 0) uniform MaterialParams {
    bool hasBaseColor;
    bool hasNormal;
    bool hasMetallicRoughness;
    // vec4 bcFactor;
    // vec3 normalFactor;
    // vec2 mrFactor;
}materialParams;

layout(set = 1, binding = 1) uniform sampler2D bcTexture;
layout(set = 1, binding = 2) uniform sampler2D normalTexture;
layout(set = 1, binding = 3) uniform sampler2D mrTexture;
// set 2: mesh

// -------------------------------------------------------


float linearDepth(float depth)
{
    float nearPlane = sceneUb.nearFar.x;
    float farPlane = sceneUb.nearFar.y;
	float z = depth * 2.0f - 1.0f; 
	return (2.0f * nearPlane * farPlane) / (farPlane + nearPlane - z * (farPlane - nearPlane));	
}


void main() {
    
    vec3 albedo = materialParams.hasBaseColor ? texture(bcTexture, fragTexCoord).xyz : vec3(1.0);
    vec3 normal = materialParams.hasNormal? texture(normalTexture, fragTexCoord).xyz : vec3(0.0);
    float metallic= materialParams.hasMetallicRoughness ? texture(mrTexture, fragTexCoord).x : 0.0f;
    float roughness = materialParams.hasMetallicRoughness ? texture(mrTexture, fragTexCoord).y : 1.0f;
    
    positionDepth = vec4(worldPos, linearDepth(gl_FragCoord.z));
    normalRoughness = vec4(normalize(worldNormal), roughness);
    albedoMetallic = vec4(albedo, roughness);
    
}