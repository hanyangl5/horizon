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

#define LIGHT_COUNT                        128
#define LIGHT_SIZE                         150.0f

#define LIGHT_CLUSTER_WIDTH                8
#define LIGHT_CLUSTER_HEIGHT               8

#define LIGHT_CLUSTER_COUNT_POS(ix, iy)    (((iy) * LIGHT_CLUSTER_WIDTH) + (ix))
#define LIGHT_CLUSTER_DATA_POS(il, ix, iy) (LIGHT_CLUSTER_COUNT_POS(ix, iy) * LIGHT_COUNT + (il))

// Override default VB geom sets
//#define NUM_GEOMETRY_SETS                  2 // 1 - opaque, 2 - alpha cutout
//#define GEOMSET_OPAQUE                     0
//#define GEOMSET_ALPHA_CUTOUT               1

//#include <Graphics/IVisibilityBuffer.h>

//#define NO_FSL_DEFINITIONS
#include <Graphics/IVisibilityBuffer2.h>
struct MeshConstants
{
    uint32_t indexOffset;
    uint32_t vertexOffset;
    uint32_t materialID;
    uint32_t twoSided; // 0 or 1
};

struct PerFrameConstants
{
    float4 camPos;
    //========================================
    float4 lightDir;
    //========================================
    float4 lightColor;
    //========================================
    float2 CameraPlane; // x : near, y : far
    uint32_t lightingMode;
    uint32_t outputMode;
    //========================================
    float2 twoOverRes;
    float esmControl;
    uint32_t aoQuality;
    //========================================
    float2 frustumPlaneSizeNormalized;
    float2 depthTexSize;
    //========================================
    float aoIntensity;
    uint32_t visualizeAo;
    uint32_t smallScaleRaster;
    uint32_t visualizeBinOccupancy;
};

struct LightData
{
    float4 position;
    float4 color;
};

#endif
