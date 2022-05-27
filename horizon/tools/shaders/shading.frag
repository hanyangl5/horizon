#version 450

#define MAX_LIGHT_COUNT 512
#define PI 3.14159265359
#define eps 1e-6

layout(location = 0) out vec3 outColor;

// set 0: scene

struct LightParams{
    vec4 colorIntensity; // r, g, b, intensity
    vec4 positionType; // x, y, z, type
    vec4 direction;
    vec4 radiusInnerOuter; // radius, innerradius, outerradius
};

layout(set = 0, binding = 0) uniform LightCountUb {
    uint lightCount;
}m_light_count_ub;

layout(set = 0, binding = 1) uniform LightUb {
    LightParams lights[MAX_LIGHT_COUNT];
}m_light_ub;

layout(set = 0, binding = 2) uniform CameraUb {
    vec3 eyePos;
}m_camera_ub;

layout(set = 0, binding = 3) uniform sampler2D position_depth;
layout(set = 0, binding = 4) uniform sampler2D normal_roughness;
layout(set = 0, binding = 5) uniform sampler2D albedo_metallic;

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
	return 0.5 / (eps + sqrt(Vis_SmithV + Vis_SmithL));
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

vec3 radiance(LightParams light, vec3 N, vec3 V, vec3 world_pos, vec3 albedo, float metallic, float roughness) {
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
        L = light.positionType.xyz - world_pos;
        float dist = length(L);
        L = normalize(L);

        float lightAttenuation = distanceFalloff(dist, light.radiusInnerOuter.x, L);

        lightRadiance = lightAttenuation * light.colorIntensity.xyz * light.colorIntensity.w;
    }
    // spot light
    else if (light.positionType.w == 2.0f) {

        L = light.positionType.xyz - world_pos;
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

    vec2 frag_coord = gl_FragCoord.xy/vec2(1920.0f,1080.0f);
    vec4 position_depth_color = texture(position_depth, frag_coord);
    vec4 albedo_metallic_color = texture(albedo_metallic, frag_coord);
    vec4 normal_roughness_color = texture(normal_roughness, frag_coord);

    vec3 world_pos = position_depth_color.rgb;
    vec3 albedo = albedo_metallic_color.rgb;
    float metallic = albedo_metallic_color.a;
    float roughness = normal_roughness_color.a;
    vec3 world_normal = normal_roughness_color.rgb;

    vec3 V = - normalize(world_pos - m_camera_ub.eyePos);
    vec3 N = normalize(world_normal);

    vec3 color = vec3(0.0f);
    
    for(uint i = 0; i < m_light_count_ub.lightCount; i++) {
        color += radiance(m_light_ub.lights[i], N, V, world_pos ,albedo, metallic, roughness);
    }
    outColor = color;
    
}