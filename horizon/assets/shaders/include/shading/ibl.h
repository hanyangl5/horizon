
float3 ComputeIBL(MaterialProperties mat,
	float NoV, float3 N, float3 V, TexCube(float4) iemCubemap, TexCube(float4) pmremCubemap, Tex2D(float4) environmentBRDF, SamplerState ibl_sampler)
{
    float3 albedo = mat.albedo;
    float metalness = mat.metallic;
    float roughness = mat.roughness;

	float3 F0 = float3(0.04f, 0.04f, 0.04f);
    float3 diffuse = (1.0 - metalness) * albedo;
	float3 specular = lerp(F0, albedo, metalness);

	float3 R = normalize(reflect(-V, N));

	float3 irradiance = SampleTexCube(iemCubemap, ibl_sampler, N).rgb;
	float3 radiance = SampleLvlTexCube(pmremCubemap, ibl_sampler, R, roughness * 10.0).rgb;
	float2 brdf = SampleTex2D(environmentBRDF, ibl_sampler, float2(NoV, roughness)).rg;

	return irradiance * diffuse + radiance * (specular * brdf.x + brdf.y);
}