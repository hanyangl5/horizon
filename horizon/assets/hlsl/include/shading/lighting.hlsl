#include "shading_models.hlsl"

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

float4 RadianceDirectionalLight(MaterialProperties mat, LightParams light, float3 n, float3 v, float3 world_pos) {
    float3 attenuation;
    float3 light_dir;

    light_dir = -normalize(light.direction.xyz);
    attenuation = light.color * light.intensity;

    if (dot(n, light_dir) < 0.0) {
        return float4(0.0, 0.0, 0.0, 0.0);
    }

    BXDF bxdf;
    InitBXDF(bxdf, n, v, light_dir);

    float3 radiance = Brdf_Opaque_Default(mat, bxdf) * attenuation * bxdf.NoL;
    return float4(radiance, 1.0);
}


float4 RadianceLocalLight(MaterialProperties mat, LightParams light, float3 n, float3 v, float3 world_pos) {
    float3 attenuation;
    float3 light_dir;
    light_dir = light.position - world_pos;
    float dist = length(light_dir);
    light_dir = normalize(light_dir);
    float distance_attenuation = DistanceFalloff(dist, light.falloff, light_dir);

    if (light.type == POINT_LIGHT) {
        attenuation = distance_attenuation * light.color * light.intensity;
    } else if (light.type == SPOT_LIGHT) {
        float angle_attenuation =
            AngleFalloff(light.spot_cone_inner_outer.x, light.spot_cone_inner_outer.y, light.direction, light_dir);
        attenuation = distance_attenuation * angle_attenuation * light.color * light.intensity;
    }

    if (dot(n, light_dir) < 0.0) {
        return float4(0.0, 0.0, 0.0, 0.0);
    }

    BXDF bxdf;
    InitBXDF(bxdf, n, v, light_dir);

    float3 radiance = Brdf_Opaque_Default(mat, bxdf) * attenuation * bxdf.NoL;
    return float4(radiance, 1.0);
}

