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

#ifndef vb_resources_h
#define vb_resources_h

SamplerState textureSampler : register(UPDATE_FREQ_NONE, s0);
#if defined(METAL) || defined(ORBIS) || defined(PROSPERO)
	Texture2D<float4> diffuseMaps[INSTANCE_BUFFER_SIZE] : register( UPDATE_FREQ_NONE, t6);
#else
	Texture2D<float4> diffuseMaps[INSTANCE_BUFFER_SIZE] : register( space4, t6);
#endif

ByteAddressBuffer vertexPositionBuffer : register(UPDATE_FREQ_NONE, t0);
ByteAddressBuffer filteredIndexBuffer : register(UPDATE_FREQ_PER_FRAME, t1);
StructuredBuffer<uint> indirectFilteredBatches : register(UPDATE_FREQ_PER_FRAME, t2);
ByteAddressBuffer vertexTexCoordBuffer : register(UPDATE_FREQ_NONE, t5);

cbuffer RootConstantViewInfo : register(b0)
{
	uint view: None;
    int targetWidth: None;
    int targetHeight: None;
};

#if defined(INDIRECT_ROOT_CONSTANT)
	cbuffer indirectRootConstant : register(b1)
	{
		uint indirectDrawId: None;
	};
	#define getDrawID() indirectDrawId
#else
    #define getDrawID() In.drawId
#endif

struct VsInVBAlphaTested
{
    float3 position: POSITION;
    uint texCoord: TEXCOORD;
};

struct PsInVBAlphaTested
{
	float4 position: SV_Position;
	float2 texCoord: TEXCOORD0;
	FLAT(uint) vbData: TEXCOORD2;;
	FLAT(uint) batchData: TEXCOORD2;
};

#endif /* vb_resources_h */
