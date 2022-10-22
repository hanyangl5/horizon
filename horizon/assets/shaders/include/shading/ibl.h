
// float3 ComputeIBL(MaterialProperties mat,
// 	float NoV, float3 N, float3 V, TexCube(float4) iemCubemap, TexCube(float4) pmremCubemap, Tex2D(float4)
// environmentBRDF, SamplerState ibl_sampler)
// {
//     float3 albedo = mat.albedo;
//     float metalness = mat.metallic;
//     float roughness = mat.roughness;

// 	float3 F0 = float3(0.04f, 0.04f, 0.04f);
//     float3 diffuse = (1.0 - metalness) * albedo;
// 	float3 specular = lerp(F0, albedo, metalness);

// 	float3 R = normalize(reflect(-V, N));

// 	float3 irradiance = SampleTexCube(iemCubemap, ibl_sampler, N).rgb;
// 	float3 radiance = SampleLvlTexCube(pmremCubemap, ibl_sampler, R, roughness * 10.0).rgb;
// 	float2 brdf = SampleTex2D(environmentBRDF, ibl_sampler, float2(NoV, roughness)).rg;

// 	return irradiance * diffuse + radiance * (specular * brdf.x + brdf.y);
// }

float3 Irradiance_SphericalHarmonics(float3 sh[9], float3 normal) {
    return max(sh[0] + sh[1] * (normal.y) + sh[2] * (normal.z) + sh[3] * (normal.x) + sh[4] * (normal.y * normal.x) +
                   sh[5] * (normal.y * normal.z) + sh[6] * (3.0 * normal.z * normal.z - 1.0) +
                   sh[7] * (normal.z * normal.x) + sh[8] * (normal.x * normal.x - normal.y * normal.y),
               0.0);
}