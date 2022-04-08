// #version 450

// #define MAX_LIGHT_COUNT 512
// #define PI 3.14159265359
// #define eps 1e-6

// layout(location = 0) in vec3 worldPos;
// layout(location = 1) in vec3 worldNormal;
// layout(location = 2) in vec2 fragTexCoord;

// layout(location = 0) out vec3 outColor;

// // set 0: scene

// struct LightParams{
//     vec4 colorIntensity; // r, g, b, intensity
//     vec4 positionType; // x, y, z, type
//     vec4 direction;
//     vec4 radiusInnerOuter; // radius, innerradius, outerradius
// };

// layout(set = 0, binding = 1) uniform LightCountUb {
//     uint lightCount;
// }lightCountUb;

// layout(set = 0, binding = 2) uniform LightUb {
//     LightParams lights[MAX_LIGHT_COUNT];
// }lightUb;

// layout(set = 0, binding = 3) uniform CameraUb {
//     vec3 eyePos;
// }cameraUb;

// // set 1: material

// layout(set = 1, binding = 0) uniform MaterialParams {
//     bool hasBaseColor;
//     bool hasNormal;
//     bool hasMetallicRoughness;
//     // vec4 bcFactor;
//     // vec3 normalFactor;
//     // vec2 mrFactor;
// }materialParams;

// layout(set = 1, binding = 1) uniform sampler2D bcTexture;
// layout(set = 1, binding = 2) uniform sampler2D normalTexture;
// layout(set = 1, binding = 3) uniform sampler2D mrTexture;
// // set 2: mesh
// 、
// // -------------------------------------------------------

// // constants

// struct ScatteringContext {
//     vec3 lightDir;
//     vec3 planetCenter = vec3(0.0f);
//     float planetRadius = 6378.0f;
//     float atmosphereRadius = 6478.0f;
//     vec3 RaylieghMieScaleHeight = vec2(8.0f, 1.2f);
// }
// // ------------------------------------------------------

// float saturate(float x) {
//     return clamp(x, 0.0f , 1.0f);
// }

// vec2 raySphereIntersec(vec3 p, vec3 dir, float r) {
// 	float b = dot( p, dir );
// 	float c = dot( p, p ) - r * r;
	
// 	float d = b * b - c;
// 	if ( d < 0.0 ) {
// 		return vec2( MAX, -MAX );
// 	}
// 	d = sqrt( d );
	
// 	return vec2( -b - d, -b + d );
// }

// float PhaseRayleigh(float c2) {
//     float nom = 3.0f * (1 + c2);
//     float denom = 16.0f * PI;
//     return nom / denom;
// }

// // g: (0.75, 0.9)
// // henyey-greenstein phase function
// float PhaseMie_HG(float g, float c) {
//     float g2 = g * g;
//     float nom = 1.0f - g2;
//     float denom = pow(1 + g2 - 2 * g * c, 1.5f);
//     denom = denom * 4.0f * PI;
//     return nom / denom;
// }

// // Cornette-Shanks phase function
// float PhaseMie_CS(float g, float c) {
//     float g2 = g * g;
//     float nom = 3.0f * (1.0f - g2) * (1.0f + c * c);
//     float denom = pow(1 + g2 - 2 * g * c, 1.5f);
//     denom = denom * 8.0f * PI * (2.0f + g2);
//     return nom / denom;
// }

// float atmosphereDensity(float height, float atmosphereLength) {
//     return exp(-height / atmosphereLength);
// }

// float opticalDepth() {

// }


// vec3 scatteringFunc(vec3 viewDir, ScatteringContext context) {
//     vec3 color = vec3(0.0f);
//     vec3 p = raySphereIntersect();
//     float dist = length(p - worldPos);
//     const uint steps = 32;
//     float stepSize = dist / steps;
//     vec3 scatteringPoint = worldPos;
//     for(uint i = 0; i < steps; i++) {
//         scatteringPoint + = stepSize * viewDir;

//         color;
//     }

//     return color;
// }


// void main() {
//     vec3 V = - normalize(worldPos - cameraUb.eyePos);
//     vec3 N = normalize(worldNormal);
//     vec3 albedo = materialParams.hasBaseColor ? texture(bcTexture, fragTexCoord).xyz : vec3(1.0);
//     vec3 color = albedo;
//     outColor = color;
// }

#version 450

layout(location = 0) in vec2 fragTexCoord;

layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform sampler2D postionDepth;
layout(set = 0, binding = 1) uniform sampler2D normalRoughness;
layout(set = 0, binding = 2) uniform sampler2D albedoMetallic;

void main() {
    outColor = texture(albedoMetallic, fragTexCoord);
}