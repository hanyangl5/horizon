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

cbuffer RootConstant0 : register(b0)
{
    uint uniformIndex;
    uint textureIndex;
    uint samplerIndex;
};

#ifndef SAMPLE_COUNT
#define SAMPLE_COUNT 1
#endif
struct PS_INPUT { float4 pos : SV_Position; float4 col : COLOR0; float2 uv : TEXCOORD0; };
float4 PS_MAIN(PS_INPUT input) : SV_Target0
{
#if SAMPLE_COUNT == 1
    Texture2D<float4> texture = ResourceDescriptorHeap[textureIndex];
    SamplerState surface = SamplerDescriptorHeap[samplerIndex];
    return input.col * texture.Sample(surface, input.uv);
#else
    Texture2DMS<float4, SAMPLE_COUNT> texture = ResourceDescriptorHeap[textureIndex];
    uint width, height, samples;
    texture.GetDimensions(width, height, samples);
    float4 color = 0;
    for (uint i = 0; i < SAMPLE_COUNT; ++i)
        color += texture.Load(uint2(input.uv * float2(width, height)), i);
    return input.col * color / SAMPLE_COUNT;
#endif
}
