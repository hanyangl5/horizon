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

#include "../../../Graphics/ShaderUtilities.h.hlsl"
#include "particle_shared.h.hlsl"
#include "particle_sets.h.hlsl"
#include "particle_utils.h.hlsl"

struct VSOutput
{
	float4 Position : SV_Position;
	float2 TexCoord : TEXCOORD0;
	FLAT(uint) InstanceId : TEXCOORD2;
	FLAT(uint) ParticleSetIndex : TEXCOORD2;
};

VSOutput VS_MAIN( uint vertexId : SV_VertexID )
{
	INIT_MAIN;

	VSOutput result;
	result.Position = float4(0,0,0,0);
	result.TexCoord = float2(0,0);
	result.InstanceId = 0;
	result.ParticleSetIndex = 0;

	// Each quad has 6 vertices
	uint instanceId = vertexId / 6;
	uint instanceVertexId = vertexId % 6;

	uint particleIdx = ParticlesToRasterize[instanceId];

	uint Bitfield = BitfieldBuffer[particleIdx];
	if ((Bitfield & PARTICLE_BITFIELD_IS_ALIVE) == 0)
	{
		RETURN(result);
	}

	ParticleData particleData = ParticlesDataBuffer[particleIdx];

	float4 VelocityAge;
	float3 position;

	UnpackParticle(GetParticleSet(Bitfield & PARTICLE_BITFIELD_SET_INDEX_MASK), particleData, position, VelocityAge);

	float scale = GetParticleSet(Bitfield & PARTICLE_BITFIELD_SET_INDEX_MASK).Size.w;
	const float3 vertexPosCache_indexed[4] =
	{
		float3(-scale, -scale, 0.0f),
		float3(-scale,  scale, 0.0f),
		float3( scale,  scale, 0.0f),
		float3( scale, -scale, 0.0f),
	};

	const float2 vertexTexCache_indexed[4] =
	{
		float2(0.0f, 1.0f),
		float2(0.0f, 0.0f),
		float2(1.0f, 0.0f),
		float2(1.0f, 1.0f),
	};

	const float3 vertexPosCache[6] =
	{
		vertexPosCache_indexed[0],
		vertexPosCache_indexed[1],
		vertexPosCache_indexed[2],
		vertexPosCache_indexed[2],
		vertexPosCache_indexed[3],
		vertexPosCache_indexed[0],
	};

	const float2 vertexTexCache[6] =
	{
		vertexTexCache_indexed[0],
		vertexTexCache_indexed[1],
		vertexTexCache_indexed[2],
		vertexTexCache_indexed[2],
		vertexTexCache_indexed[3],
		vertexTexCache_indexed[0],
	};

	float4 offset = float4(vertexPosCache[instanceVertexId], 1.0f);
	uint billboardMode = Bitfield & PARTICLE_BITFIELD_BILLBOARD_MODE_BITS_MASK;

	float3 right = getCol(ViewTransform, 0).xyz;
	float3 up = getCol(ViewTransform, 1).xyz;
	float3 forward = getCol(ViewTransform, 2).xyz;

	switch (billboardMode)
	{
		case PARTICLE_BITFIELD_BILLBOARD_MODE_SCREEN_ALIGNED:
		{
			result.Position = mul(ViewTransform, float4(position, 1.0f));
			result.Position.xyz += vertexPosCache[instanceVertexId].xyz;
		} break;
		case PARTICLE_BITFIELD_BILLBOARD_MODE_VELOCITY_ORIENTED:
		{
			up = normalize(VelocityAge.xyz);
			forward = float3(0.0f, 0.0f, 1.0f);
			right = cross(up, forward);
			forward = cross(up, right);
			float4x4 billboard = make_f4x4_cols(float4(right, 0), float4(up, 0), float4(forward, 0), float4(position, 1.0f));
			result.Position = mul(billboard, offset);
			result.Position = mul(ViewTransform, result.Position);
		} break;
		case PARTICLE_BITFIELD_BILLBOARD_MODE_HORIZONTAL:
		{
			forward = float3(0.0f, 1.0f, 0.0f);
			up = float3(0.0f, 0.0f, 1.0f);
			right = float3(1.0f, 0.0f, 0.0f);
			float4x4 billboard = make_f4x4_cols(float4(right, 0), float4(up, 0), float4(forward, 0), float4(position, 1.0f));
			result.Position = mul(billboard, offset);
			result.Position = mul(ViewTransform, result.Position);
		} break;
		default:
			break;
	}

	result.Position = mul(ProjTransform, result.Position);
	result.TexCoord = vertexTexCache[instanceVertexId];
	result.InstanceId = instanceId;
	result.ParticleSetIndex = Bitfield & PARTICLE_BITFIELD_SET_INDEX_MASK;

	RETURN(result);
}
