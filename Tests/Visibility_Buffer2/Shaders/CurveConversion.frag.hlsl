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

struct PsIn
{
    float4 position: SV_Position;
    float2 texCoord: TEXCOORD;
};

Texture2D<float4> SceneTex : register( UPDATE_FREQ_NONE, t0);
SamplerState  uSampler0 : register(UPDATE_FREQ_NONE, s0);
Texture2D<float4> GodRayTex : register(UPDATE_FREQ_NONE, s1);

struct FSOutput
{
	float4 FragmentOutput: SV_Target;
};

FSOutput PS_MAIN( PsIn In )
{
    INIT_MAIN;
    FSOutput Out;

	float4 sceneColor = SampleTex2D(SceneTex, uSampler0, In.texCoord);
	sceneColor.rgb += SampleTex2D(GodRayTex, uSampler0, In.texCoord).rgb;
    Out.FragmentOutput = float4(sceneColor.rgb, 1.0);

    return Out;
}
