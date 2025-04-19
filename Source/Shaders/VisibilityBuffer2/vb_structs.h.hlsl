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

#ifndef _VB_STRUCTS_H
#define _VB_STRUCTS_H

#ifdef NO_FSL_DEFINITIONS
	#define STATIC static
	#define struct NAME struct NAME
	#define DATA(TYPE, NAME, SEM) TYPE NAME
	#define cbuffer NAME: register(REG, FREQ) struct NAME
	#define float4x4 mat4
	#define uint uint32_t
#endif

// Input to pre skin vertexes shader
struct PreSkinBatchData
{
	/**********************************/
	// Per Batch
	/**********************************/
	uint vertexCount : None;         // Number of vertexes to be skinned by this batch (usually equals SKIN_BATCH_SIZE)

	// Output vertexes are written in the range [outputVertexOffset, outputVertexOffset + vertexCount)
	uint outputVertexOffset : None;

	// Offsets to the start of the mesh in the huge Vertex Buffers
	// Position offset is different to Joint offset because the Joints/Weights buffers are smaller
	uint vertexPositionOffset : None;
	uint vertexJointsOffset : None;

	/**********************************/
	// Per Instance
	/**********************************/
	uint jointMatrixOffset : None; 	// Offset into the matrix buffer that contains all matrixes for all animated objects
											
	// Padding
	uint pad0 : None;
	uint pad1 : None;
	uint pad2 : None;
};

// Buffer allocation needs to satisfy alignment requirements for the buffers, therefore the buffer doesn't necessarily start on the first
// pre-skinned output vertex slot. This structure contains the offsets to apply into the output buffers in pre_skin_vertexes, adding 
// these offsets gives us the real start where we have to write the pre-skinned vertexes.
struct PreSkinBufferOffsets
{
	uint vertexOffset : None;
};

// Input to triangle filtering shader
struct FilterBatchData
{
	uint meshIndex : None;	  // Index into meshConstants
	uint indexOffset : None;	  // Index relative to the meshConstants[meshIndex].indexOffset
	uint faceCount : None;      // Number of faces in this small batch
	uint outputIndexOffset : None; // Offset into the output index buffer
	uint drawBatchStart : None;	// First slot for the current draw call
	uint accumDrawIndex : None;
	uint instanceDataIndex : None;   // Instance specific data, usually an index into instanceData buffer but could also contain more encoded data in different bits
	uint geometrySet : None; // We only use 2 bits from this variable, the rest is free

	// Note: we could combine faceCount and geometrySet to add more data in this struct
	//   - geometrySet: 2 bits (we have 3 geometry sets: OPAQUE, ALPHA_CUTOUT, ALPHA_BLEND)
	//   - faceCount: 9 bits (this depends on FILTER_BATCH_SIZE, but it's usually 256, so 9 bits are enough)
	//   - 21 bits free
	//uint geometrySet_faceCount : None; 
};

struct CullingViewPort
{
	float2 windowSize :  None
	uint sampleCount : None;
	uint pad : None;
};

struct Transform
{
	float4x4 mvp : None;
	float4x4 invVP : None;
	float4x4 vp : None;
	float4x4 view : None;
	float4x4 projection : None;
	float2 cameraPlane : None; //x : near, y : far
    float2 _pad0 : None;
};

#endif //!_VB_STRUCTS_H
