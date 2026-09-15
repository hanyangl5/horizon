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

// Shader for simple shading with a point light
// for skeletons in Unit Tests Animation

#define MAX_INSTANCES 804

#include "resources.h.hlsl"

struct VSInput
{
    float4 Position : POSITION;
    float4 Normal : NORMAL;
};

struct VSOutput
{
    float4 Position : SV_Position;
    float4 QuadCoord : TEXCOORD0;
    float4 LightDirection : TEXCOORD2;
};

VSOutput VS_MAIN(VSInput In, uint InstanceID : SV_InstanceID)
{
    ConstantBuffer<SkeletonUniforms> uniforms = ResourceDescriptorHeap[uniformIndex];
    VSOutput Out;
    float4x4 matWorld = uniforms.toWorld[InstanceID];

    // extract joint scale and world position
    float   jointScale = length(getRow(matWorld,0).xyz);
    float4  jointPosition = getCol(matWorld,3);

#if FT_MULTIVIEW
    float4x4 tempMat = uniforms.mvp[VR_VIEW_ID];
#else
    float4x4 tempMat = uniforms.mvp;
#endif

    // extract side and up vectors to calculate billboard
    float4x4 tempViewMatrix = uniforms.viewMatrix;
    float3 vSide = getRow(tempViewMatrix,0).xyz;
    float3 vUp   = getRow(tempViewMatrix,1).xyz;
    
    // calculate billboarded vertex world position, always facing camera
    float4 worldPosition = jointPosition;
    worldPosition.xyz += In.Position.x * vSide * jointScale;
    worldPosition.xyz += In.Position.y * vUp * jointScale;
    Out.Position = mul(tempMat, worldPosition);

    // make sure we get quad coords from -1.0 to 1.0 for ps to cutout the circle
    Out.QuadCoord = float4(normalize(In.Position.xy) * 1.414f, 0.0f, 0.0f );
    float4 pos = mul(uniforms.toWorld[InstanceID], float4(In.Position.xyz, 1.0f));
    float3 lightDir = normalize(uniforms.lightPosition.xyz - pos.xyz);
    Out.LightDirection = float4(lightDir,0.0f);

    return Out;
}
