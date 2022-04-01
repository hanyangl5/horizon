#version 450

#define MAX_LIGHT_COUNT 512
#define PI 3.14159265359
#define eps 1e-6

layout(location = 0) in vec3 worldPos;
layout(location = 1) in vec3 worldNormal;
layout(location = 2) in vec2 fragTexCoord;

layout(location = 0) out vec3 outColor;

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

float saturate(float x) {
    return clamp(x, 0.0f , 1.0f);
}

float D_GGX(float a2, float NoH) {
	float d = ( NoH * a2 - NoH ) * NoH + 1.0f;
	return a2 / ( PI * d * d );
}

float G_Smith(float a2, float NoV, float NoL) {
    float Vis_SmithV = NoL * sqrt(NoV * (NoV - NoV * a2) + a2);
	float Vis_SmithL = NoV * sqrt(NoL * (NoL - NoL * a2) + a2);
	return 0.5 / sqrt(Vis_SmithV + Vis_SmithL);
}

vec3 F_Schlick(float VoH, vec3 F0) {
    return F0 + (vec3(1.0f) - F0) * pow(clamp(1.0 - VoH, 0.0, 1.0), 5.0);
}

struct BrdfContext{
    float a2;
    vec3 F0;
    float NoV;
    float LoH; // VoH
    float NoH;
    float NoL;
};

float diffuseBrdf(BrdfContext BrdfContext) {
    return 1.0f / PI;
}

vec3 specularBrdf(BrdfContext brdfCotext) {

    float D = D_GGX(brdfCotext.a2, brdfCotext.NoH);
    float G = G_Smith(brdfCotext.a2, brdfCotext.NoV, brdfCotext.NoL);
    vec3 F = F_Schlick(brdfCotext.LoH, brdfCotext.F0);
    return F * (D * G);
}

float distanceFalloff(float dist, float r, vec3 L) {
    // Brian Karis, 2013. Real Shading in Unreal Engine 4.
    float d2 = dist * dist;
    float r2 = r * r;
    float a = saturate(1.0f - (d2 * d2) / (r2 * r2));
    return a * a / max(d2, 1e-4);
}

float angleFalloff(float innerRadius, float outerRadius, vec3 direction, vec3 L) {
    float cosOuter = cos(outerRadius);
    float spotScale = 1.0 / max(cos(innerRadius) - cosOuter, 1e-4);
    float spotOffset = -cosOuter * spotScale;

    float cd = dot(normalize(-direction), L);
    float attenuation = clamp(cd * spotScale + spotOffset, 0.0, 1.0);
    return attenuation * attenuation;
}

vec3 radiance(LightParams light, vec3 N, vec3 V) {
    vec3 albedo = materialParams.hasBaseColor ? texture(bcTexture, fragTexCoord).xyz : vec3(1.0);
    vec3 normal = materialParams.hasNormal? texture(normalTexture, fragTexCoord).xyz : vec3(0.0);
    float metallic= materialParams.hasMetallicRoughness ? texture(mrTexture, fragTexCoord).x : 0.0f;
    float roughness = materialParams.hasMetallicRoughness ? texture(mrTexture, fragTexCoord).y : 1.0f;

    vec3 lightRadiance;
    vec3 L;

    // direct light
    if(light.positionType.w == 0.0f) {
        L = - normalize(light.direction.xyz);
        float lightAttenuation = 1.0f;
        lightRadiance = lightAttenuation * light.colorIntensity.xyz * light.colorIntensity.w;
    }
    // point light
    else if(light.positionType.w == 1.0f) {
        L = light.positionType.xyz - worldPos;
        float dist = length(L);
        L = normalize(L);

        float lightAttenuation = distanceFalloff(dist, light.radiusInnerOuter.x, L);

        lightRadiance = lightAttenuation * light.colorIntensity.xyz * light.colorIntensity.w;
    }
    // spot light
    else if (light.positionType.w == 2.0f) {

        L = light.positionType.xyz - worldPos;
        float dist = length(L);
        L = normalize(L);

        float lightAttenuation = distanceFalloff(dist, light.radiusInnerOuter.x, L) * angleFalloff(light.radiusInnerOuter.y, light.radiusInnerOuter.z, light.direction.xyz, L);

        lightRadiance = lightAttenuation * light.colorIntensity.xyz * light.colorIntensity.w;

    }

    vec3 H = normalize(V + L);
    BrdfContext brdfCotext;
    brdfCotext.a2 = roughness * roughness;
    brdfCotext.NoV = saturate(dot(N, V));
    brdfCotext.F0 = mix(vec3(0.04f), albedo, metallic);
    brdfCotext.LoH = saturate(dot(L, H)); // VoH
    brdfCotext.NoH = saturate(dot(N, H));
    brdfCotext.NoL = saturate(dot(N, L));

    vec3 ks = brdfCotext.F0;
    vec3 kd = (vec3(1.0) - ks) * (1.0f - metallic);
    vec3 brdf = (kd * albedo * diffuseBrdf(brdfCotext) + ks * specularBrdf(brdfCotext)) * brdfCotext.NoL;
    return  brdf * lightRadiance;
}


void main() {
    vec3 V = - normalize(worldPos - cameraUb.eyePos);
    vec3 N = normalize(worldNormal);

    vec3 color = vec3(0.0f);
    
    for(uint i = 0; i < lightCountUb.lightCount; i++) {
        color += radiance(lightUb.lights[i], N, V);
    }
    color = color / (color + vec3(1.0));
    color = pow(color, vec3(1.0/2.2)); 
    outColor = color;

}