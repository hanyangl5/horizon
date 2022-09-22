#include "shading_models.h"

float DistanceFalloff(float dist, float r, float3 light_dir) {
    // Brian Karis, 2013. Real Shading in Unreal Engine 4.
    float d2 = dist * dist;
    float r2 = r * r;
    float a = saturate(1.0f - (d2 * d2) / (r2 * r2));
    return a * a / max(d2, 1e-4);
}

float AngleFalloff(float innerRadius, float outerRadius, float3 direction, float3 light_dir) {
    float cosOuter = cos(outerRadius);
    float spotScale = 1.0 / max(cos(innerRadius) - cosOuter, 1e-4);
    float spotOffset = -cosOuter * spotScale;

    float cd = dot(normalize(-direction), light_dir);
    float attenuation = clamp(cd * spotScale + spotOffset, 0.0, 1.0);
    return attenuation * attenuation;
}

float4 Radiance(MaterialProperties mat, LightParams light, float3 n, float3 v, float3 world_pos) {
    float3 attenuation;
    float3 light_dir;

    if(asuint(light.position_type).w == DIRECTIONAL_LIGHT) {
        light_dir = -light.direction.xyz;
        attenuation = light.color_intensity.xyz * light.color_intensity.w;
    } else if(asuint(light.position_type).w == POINT_LIGHT) {
        light_dir = light.position_type.xyz - world_pos;
        float dist = length(light_dir);
        light_dir = normalize(light_dir);

        float lightAttenuation = DistanceFalloff(dist, light.radius_inner_outer.x, light_dir);

        attenuation = lightAttenuation * light.color_intensity.xyz * light.color_intensity.w;
    } else if (asuint(light.position_type).w == SPOT_LIGHT) {

        light_dir = light.position_type.xyz - world_pos;
        float dist = length(light_dir);
        light_dir = normalize(light_dir);

        float lightAttenuation = DistanceFalloff(dist, light.radius_inner_outer.x, light_dir) * AngleFalloff(light.radius_inner_outer.y, light.radius_inner_outer.z, light.direction.xyz, light_dir);

        attenuation = lightAttenuation * light.color_intensity.xyz * light.color_intensity.w;

    }
    BXDF bxdf;
    InitBXDF(bxdf, n, v, light_dir);
    
    float3 radiance = OpaqueBrdf(mat, bxdf) * attenuation * bxdf.NoL;

    return float4(radiance, 1.0);
}