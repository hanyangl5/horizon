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
#ifndef _PARTICLE_SHARED_H
#define _PARTICLE_SHARED_H

#include "particle_defs.h.hlsl"

RWStructuredBuffer<uint> BitfieldBuffer : register(UPDATE_FREQ_PER_FRAME, u53);
RWStructuredBuffer<ParticleData> ParticlesDataBuffer : register(UPDATE_FREQ_PER_FRAME, u52);

// Amount of particles for each section: LightNShadow - Light - Standard
RWStructuredBuffer<uint4> ParticleCountsBuffer : register(UPDATE_FREQ_PER_FRAME, u55);

// Start indices of the various sections: Alive - Dead - Inactive. We only need the Dead start index and the Inactive start index
// The first element contains the value of the previous frame, the second one contains the value of the current frame
RWStructuredBuffer<uint> ParticleSectionsIndices : register(UPDATE_FREQ_PER_FRAME, u56);

// For each particle set, y tells whether it is visible, x if it was visible in the last frame
RWStructuredBuffer<uint> ParticleSetVisibility : register(UPDATE_FREQ_PER_FRAME, u57);

// Amount of particles to be rasterized by the hardware rasterizer
RWStructuredBuffer<uint> ParticlesToRasterizeCount : register(UPDATE_FREQ_PER_FRAME, u60);
// Indices of the particles to be rasterized by the hardware rasterizer
RWStructuredBuffer<uint> ParticlesToRasterize : register(UPDATE_FREQ_PER_FRAME, u68);

// Array of textures for each particle set
Texture2D<float4> ParticleTextures[MAX_PARTICLE_SET_COUNT] : register(UPDATE_FREQ_NONE, t59);

RWTexture2D(float4) ColorBuffer : register(UPDATE_FREQ_PER_FRAME, u59);
Texture2D<float> DepthBuffer : register(UPDATE_FREQ_PER_FRAME, t69);
SamplerState NearestClampSampler : register(UPDATE_FREQ_NONE, s0);
SamplerState LinearClampSampler : register(UPDATE_FREQ_NONE, s1);


// Per pixel linked list for sorting transparent pixels
RWStructuredBuffer<PackedParticleTransparencyNode> TransparencyList : register(UPDATE_FREQ_PER_FRAME, u66);
RWStructuredBuffer<uint> TransparencyListHeads : register(UPDATE_FREQ_PER_FRAME, u67);


#endif
