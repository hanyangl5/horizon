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

#ifndef _SHADER_DEFS_H
#define _SHADER_DEFS_H

#ifdef NO_FSL_DEFINITIONS
    #define STATIC static
    #define struct NAME struct NAME
    #define TYPE NAME: SEM TYPE NAME
    #define cbuffer NAME: register(REG, FREQ) struct NAME
	#define float4x4 mat4
#endif

#define LIGHT_COUNT 128
#define LIGHT_SIZE 150.0f

#define LIGHT_CLUSTER_WIDTH 8
#define LIGHT_CLUSTER_HEIGHT 8

#define LIGHT_CLUSTER_COUNT_POS(ix, iy) ( ((iy)*LIGHT_CLUSTER_WIDTH)+(ix) )
#define LIGHT_CLUSTER_DATA_POS(il, ix, iy) ( LIGHT_CLUSTER_COUNT_POS(ix, iy)*LIGHT_COUNT + (il) )

//Override default VB geom sets
#define NUM_GEOMETRY_SETS 2          // 1 - opaque, 2 - alpha cutout
#define GEOMSET_OPAQUE 0
#define GEOMSET_ALPHA_CUTOUT 1

#include "../../../../../Common_3/Renderer/VisibilityBuffer2/Shaders/FSL/vb_structs.h.fsl"

struct MeshConstants
{
	uint indexOffset: None;
	uint vertexOffset: None;
	uint materialID: None;
	uint twoSided: None; //0 or 1
};

cbuffer PerFrameConstants: register(UPDATE_FREQ_PER_FRAME, b0)
{
	float4 camPos: None;
	//========================================
	float4 lightDir: None;
	//========================================
	float4 lightColor: None;
	//========================================
	float2 CameraPlane: None; //x : near, y : far
	uint lightingMode: None;
	uint outputMode: None;
	//========================================
	float2 twoOverRes: None;
	float esmControl: None;
	uint aoQuality: None;
	//========================================
	float2 frustumPlaneSizeNormalized: None;
	float2 depthTexSize: None;
	//========================================
	float aoIntensity: None;
	uint visualizeAo: None;
	uint smallScaleRaster: None;
	uint visualizeBinOccupancy: None;
};

struct LightData
{
	float4 position: None;
	float4 color: None;
};

#include "../../../../../Common_3/Renderer/VisibilityBuffer2/Shaders/FSL/vb_shader_defs.h.fsl"
#endif
