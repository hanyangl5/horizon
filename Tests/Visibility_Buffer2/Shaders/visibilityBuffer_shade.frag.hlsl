/*
 * Copyright (c) 2017-2024 The Forge Interactive Inc.
 * 
 * This file is part of The-Forge
 * (see https://github.com/ConfettiFX/The-Forge).
 * 
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 * 
 *   http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
*/

// USERMACRO: SAMPLE_COUNT [1,2,4]
// Uncomment this definition to use ray differentials method for calculating
// gradients instead of screen-space projected triangles method.
//#define USE_RAY_DIFFERENTIALS

#include "shading.h.hlsl"
#include "../../../../../Common_3/Renderer/VisibilityBuffer2/Shaders/FSL/vb_shading_utilities.h.hlsl"

#include "triangle_binning.h.hlsl"

// This shader loads draw / triangle Id per pixel and reconstruct interpolated vertex data.

struct VSOutput
{
	float4 position: SV_Position;
	float2 screenPos: TEXCOORD0;
};

// Static descriptors

#if SAMPLE_COUNT > 1
    Tex2DMS(float, SAMPLE_COUNT) depthTex : register(UPDATE_FREQ_NONE, t100);
#else
    Texture2D<float> depthTex : register(UPDATE_FREQ_NONE, t100);
#endif

Texture2D<float> shadowMap : register(UPDATE_FREQ_NONE, t101);

#if defined(METAL) || defined(ORBIS) || defined(PROSPERO)
	Texture2D<float4> diffuseMaps[INSTANCE_BUFFER_SIZE] : register( UPDATE_FREQ_NONE, t0);
	Texture2D<float4> normalMaps[INSTANCE_BUFFER_SIZE] : register(  UPDATE_FREQ_NONE, t1);
	Texture2D<float4> specularMaps[INSTANCE_BUFFER_SIZE] : register(UPDATE_FREQ_NONE, t2);
#else
	Texture2D<float4> diffuseMaps[INSTANCE_BUFFER_SIZE] : register( space4, t0);
	Texture2D<float4> normalMaps[INSTANCE_BUFFER_SIZE] : register(  space5, t0);
	Texture2D<float4> specularMaps[INSTANCE_BUFFER_SIZE] : register(space6, t0);
#endif

ByteAddressBuffer vertexPos : register(          UPDATE_FREQ_NONE, t10);
ByteAddressBuffer vertexTexCoord : register(     UPDATE_FREQ_NONE, t11);
ByteAddressBuffer vertexNormal : register(       UPDATE_FREQ_NONE, t12);
StructuredBuffer<uint> binBuffer : register(UPDATE_FREQ_PER_FRAME, t14);

StructuredBuffer<uint> indirectFilteredBatches : register(UPDATE_FREQ_PER_FRAME, t15);
StructuredBuffer<MeshConstants> meshConstantsBuffer : register(UPDATE_FREQ_NONE, t17);
StructuredBuffer<LightData> lights : register(                 UPDATE_FREQ_NONE, t19);

ByteAddressBuffer lightClustersCount : register(UPDATE_FREQ_PER_FRAME, t20);
ByteAddressBuffer lightClusters : register(     UPDATE_FREQ_PER_FRAME, t21);

SamplerState textureSampler : register(UPDATE_FREQ_NONE, s0);
SamplerState depthSampler : register(UPDATE_FREQ_NONE, s1);

StructuredBuffer<uint64_t> visibilityBuffer : register(UPDATE_FREQ_PER_FRAME, t1);


ByteAddressBuffer indexDataBuffer : register(UPDATE_FREQ_NONE, t2);

float4 PS_MAIN( VSOutput In, SV_SampleIndex(uint) i )
{
	INIT_MAIN;

	uint index = VisibilityBufferOffset(VIEW_CAMERA, depthTexSize[0], In.position.x, In.position.y);
	uint64_t packedDepthVBId = visibilityBuffer[index];

	uint triangleData = 0;
	float depth = 0.0f;
	unpackDepthVBId(packedDepthVBId, depth, triangleData);
	uint triangleIndex = GetTriIndexFromTriData(triangleData);

	// Early exit if this pixel doesn't contain triangle data
	if (INVALID_TRIANGLE_DATA == triangleData)
	{
		discard;
	}

	if (false && In.position.y < 5000)
	{
		RETURN(float4((triangleData % 128) / 128.0f, (triangleData %32)/32.0f, depth*10, 1));
	}

	// Extract packed data
	uint batchID = GetBatchIdFromTriData(triangleData);
	uint alpha1_opaque0 = GetGeomSetFromTriData(triangleData);

	uint index0 = indexDataBuffer.Load((triangleIndex * 3 + 0) << 2);
	uint index1 = indexDataBuffer.Load((triangleIndex * 3 + 1) << 2);
	uint index2 = indexDataBuffer.Load((triangleIndex * 3 + 2) << 2);

	// Load vertex data of the 3 vertices
	float3 v0pos = asfloat(vertexPos.Load4(index0 * 12)).xyz;
	float3 v1pos = asfloat(vertexPos.Load4(index1 * 12)).xyz;
	float3 v2pos = asfloat(vertexPos.Load4(index2 * 12)).xyz;

	// Transform positions to clip space
	float4 pos0 = mul(transform[VIEW_CAMERA].mvp, float4(v0pos, 1.0f));
	float4 pos1 = mul(transform[VIEW_CAMERA].mvp, float4(v1pos, 1.0f));
	float4 pos2 = mul(transform[VIEW_CAMERA].mvp, float4(v2pos, 1.0f));

	float4 wPos0 = mul(transform[VIEW_CAMERA].invVP,pos0);
	float4 wPos1 = mul(transform[VIEW_CAMERA].invVP,pos1);
	float4 wPos2 = mul(transform[VIEW_CAMERA].invVP,pos2);

	float2 two_over_windowsize = twoOverRes;

	// Compute partial derivatives and baycentric coordinates.
	// This is necessary to interpolate triangle attributes per pixel.
	BarycentricDeriv derivativesOut = CalcFullBary(pos0,pos1,pos2,In.screenPos, two_over_windowsize);

	f3x2 texCoords = make_f3x2_cols(
			unpack2Floats(vertexTexCoord.Load(index0 << 2)) ,
			unpack2Floats(vertexTexCoord.Load(index1 << 2)) ,
			unpack2Floats(vertexTexCoord.Load(index2 << 2)) 
	);

	// Interpolated 1/w (one_over_w) for all three vertices of the triangle
	// using the barycentric coordinates and the delta vector
	float w = dot(float3(pos0.w, pos1.w, pos2.w),derivativesOut.m_lambda);

	// Reconstruct the Z value at this screen point performing only the necessary matrix * vector multiplication
	// operations that involve computing Z
	float z = w * getElem(transform[VIEW_CAMERA].projection, 2, 2) + getElem(transform[VIEW_CAMERA].projection, 3, 2);

	// Calculate the world position coordinates:
	// First the projected coordinates at this point are calculated using In.screenPos and the computed Z value at this point.
	// Then, multiplying the perspective projected coordinates by the inverse view-projection matrix (invVP) produces world coordinates
	float3 position = mul(transform[VIEW_CAMERA].invVP, float4(In.screenPos * w, z, w)).xyz;

#if defined(USE_RAY_DIFFERENTIALS)
	float3 positionDX = mul(transform[VIEW_CAMERA].invVP, float4((In.screenPos+two_over_windowsize.x/2) * w, z, w)).xyz;
	float3 positionDY = mul(transform[VIEW_CAMERA].invVP, float4((In.screenPos+two_over_windowsize.y/2) * w, z, w)).xyz;

	derivativesOut = CalcRayBary(wPos0.xyz,wPos1.xyz,wPos2.xyz,position,positionDX,positionDY,
												camPos.xyz);
#endif
	// Get the material id from the per batch indirection data buffer. 
	uint materialID = meshConstantsBuffer[batchID].materialID;
	// uint batchData = indirectFilteredBatches[BATCH_DISPATCH_ARGUMENTS_OFFSET + batchID];
	// uint materialID = (batchData & MATERIAL_ID_MASK);

	// Interpolate texture coordinates and calculate the gradients for texture sampling with mipmapping support
	GradientInterpolationResults results = Interpolate2DWithDeriv(derivativesOut,texCoords);
	
	float2 texCoordDX = results.dx;
	float2 texCoordDY = results.dy;
	float2 texCoord = results.interp;

	// CALCULATE PIXEL COLOR USING INTERPOLATED ATTRIBUTES
	// Reconstruct normal map Z from X and Y
	// "NonUniformResourceIndex" is a "pseudo" function see
	// http://asawicki.info/news_1608_direct3d_12_-_watch_out_for_non-uniform_resource_index.html

	// Get textures from arrays.
	float4 normalMapRG;
	float4 diffuseColor;
	float4 specularColor;
	BeginNonUniformResourceIndex(materialID, MAX_TEXTURE_UNITS);
		normalMapRG   = SampleGradTex2D(normalMaps[materialID],   textureSampler, texCoord, texCoordDX, texCoordDY);
		diffuseColor  = SampleGradTex2D(diffuseMaps[materialID],  textureSampler, texCoord, texCoordDX, texCoordDY);
		specularColor = SampleGradTex2D(specularMaps[materialID], textureSampler, texCoord, texCoordDX, texCoordDY);
	EndNonUniformResourceIndex();

	float3 reconstructedNormalMap;
	reconstructedNormalMap.xy = normalMapRG.ga * 2.0f - 1.0f;
	reconstructedNormalMap.z = sqrt(saturate(1.0f - dot(reconstructedNormalMap.xy, reconstructedNormalMap.xy)));

	// NORMAL INTERPOLATION
	float3x3 normals = make_f3x3_rows(
		decodeDir(unpackUnorm2x16(vertexNormal.Load(index0 << 2))),
		decodeDir(unpackUnorm2x16(vertexNormal.Load(index1 << 2))),
		decodeDir(unpackUnorm2x16(vertexNormal.Load(index2 << 2)))
	);
	float3 normal = normalize(InterpolateWithDeriv_float3x3(derivativesOut, normals));
	
	//Calculate pixel normal and tangent vectors
	f3x3 wPositions = make_f3x3_cols(
			wPos0.xyz,
			wPos1.xyz,
			wPos2.xyz
	);

	DerivativesOutput wPosDer = Cal3DDeriv(derivativesOut, wPositions);
	DerivativesOutput uvDer = { float3(results.dx, 0.0),  float3(results.dy, 0.0) };
	normal = perturb_normal(reconstructedNormalMap, normal, wPosDer, uvDer);

	// Sample Diffuse color
	float4 posLS = mul(transform[VIEW_SHADOW].vp, float4(position, 1.0f));
	
	float Roughness = clamp(specularColor.a, 0.05f, 0.99f);
	float Metallic = specularColor.b;

	float ao = calculateAoContrib(
		depthTexSize,
		aoIntensity,
		aoQuality,
		CameraPlane.y,
		CameraPlane.x,
		frustumPlaneSizeNormalized,
		In.position.xy,
		depthTex,
		depthSampler);

	bool isTwoSided = (alpha1_opaque0 == 1) && bool(meshConstantsBuffer[materialID].twoSided);
	bool isBackFace = false;

	float3 ViewVec = normalize(camPos.xyz - position.xyz);
	
	//if it is backface
	//this should be < 0 but our mesh's edge normals are smoothed, badly
	if (isTwoSided && dot(normal, ViewVec) < 0.0f)
	{
		//flip normal
		normal = -normal;
		isBackFace = true;
	}

	float3 HalfVec = normalize(ViewVec - lightDir.xyz);
	float3 ReflectVec = reflect(-ViewVec, normal);
	float NoV = saturate(dot(normal, ViewVec));

	float NoL = dot(normal, -lightDir.xyz);	

	// Deal with two faced materials
	NoL = (isTwoSided ? abs(NoL) : saturate(NoL));

	float3 shadedColor = f3(0.0f);

	// calculate color contribution from specular lighting
	float3 F0 = f3(0.08); // 0.08 is the index of refraction
	float3 SpecularColor = lerp(F0, diffuseColor.rgb, Metallic);
	float3 DiffuseColor = lerp(diffuseColor.rgb, f3(0.0), Metallic);

	float shadowFactor = 1.0f;
	float fLightingMode = saturate(float(lightingMode));

	shadedColor = calculateIllumination(
		    normal,
		    ViewVec,
			HalfVec,
			ReflectVec,
			NoL,
			NoV,
			camPos.xyz,
			esmControl,
			lightDir.xyz,
			posLS,
			position,
			shadowMap,
			DiffuseColor,
			SpecularColor,
			Roughness,
			Metallic,
			depthSampler,
			isBackFace,
			fLightingMode,
			shadowFactor);
	
	shadedColor = shadedColor * lightColor.rgb * lightColor.a * NoL;
	if (visualizeAo > 0)
		shadedColor = f3(ao);
	
	// point lights
	// Find the light cluster for the current pixel
	uint2 clusterCoords = uint2(floor((In.screenPos * 0.5f + 0.5f) * float2(LIGHT_CLUSTER_WIDTH, LIGHT_CLUSTER_HEIGHT)));

	uint numLightsInCluster = lightClustersCount, LIGHT_CLUSTER_COUNT_POS(clusterCoords.x.Load(clusterCoords.y) << 2);

	// Accumulate light contributions
	for (uint j = 0; j < numLightsInCluster; ++j)
	{
		uint lightId = lightClusters, LIGHT_CLUSTER_DATA_POS(j, clusterCoords.x.Load(clusterCoords.y) << 2);

		shadedColor += pointLightShade(
		normal,
		ViewVec,
		HalfVec,
		ReflectVec,
		NoL,
		NoV,
		lights[lightId].position.xyz,
		lights[lightId].color.xyz,
		camPos.xyz,
		lightDir.xyz,
		posLS,
		position,
		DiffuseColor,
		SpecularColor,
		Roughness,
		Metallic,		
		isBackFace,
		fLightingMode);
	}

	float ambientIntencity = 0.05f * ao;
	float3 ambient = diffuseColor.rgb * ambientIntencity;

	float3 FinalColor = shadedColor + ambient;

	// debug bin occupancy overlay
	if(visualizeBinOccupancy > 0)
	{
		float2 binCoord = float2(In.position.x / BIN_SIZE, In.position.y / BIN_SIZE);
		uint tx = uint(binCoord[0]);
		uint ty = uint(binCoord[1]);
		uint triangleCount = binBuffer[BinBufferViewOffset(VIEW_CAMERA) + TIDX(tx, ty)];
		float3 empty = float3(0, 1, 0), full = float3(1, 0, 0);
		float binOccupancy = triangleCount / float(TILE_CAPACITY);
		float3 occupancuVis = float3(binOccupancy, 1.0f - binOccupancy, 0);
		if ((abs(frac(binCoord.x) - 0.5f) > 0.495f) || (abs(frac(binCoord.y) - 0.5f) > 0.495f))
			binOccupancy = 0.75f;
		FinalColor.rgb = lerp(FinalColor.rgb, occupancuVis, binOccupancy);
		if (binOccupancy >= 1.0f)
			FinalColor.rgb = float3(1, 0, 1);
	}

	RETURN(float4(FinalColor, 1.0));
}
