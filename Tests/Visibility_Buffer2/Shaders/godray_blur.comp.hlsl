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

#define THREADX 16
#define THREADY 16

#define PASS_TYPE_HORIZONTAL 0
#define PASS_TYPE_VERTICAL 1
#define MAX_BLUR_KERNEL_SIZE 8

cbuffer BlurRootConstant : register(b0)
{
	uint mBlurPassType: None;
	uint mFilterRadius: None;
};

cbuffer BlurWeights: register(UPDATE_FREQ_NONE, b1)
{
	float4 mBlurWeights[MAX_BLUR_KERNEL_SIZE / 4]: None;
};

RWTexture2D(float4) godrayTextures[2] : register(UPDATE_FREQ_NONE, u0);

#if 1

GroupShared(float, g_shared_r[32][32]);
GroupShared(float, g_shared_g[32][32]);
GroupShared(float, g_shared_b[32][32]);

float3 HorizontalPass(int2 id)
{
	float3 result = float3(0.0f, 0.0f, 0.0f);

	int col = id.x % 16;
	int row = id.y % 16;

	float3 temp;

	temp = LoadRWTex2D(Get(godrayTextures[PASS_TYPE_HORIZONTAL]), id + int2(col - 8, 0)).rgb;
	g_shared_r[row + 8][2 * col + 0] = temp.r;
	g_shared_g[row + 8][2 * col + 0] = temp.g;
	g_shared_b[row + 8][2 * col + 0] = temp.b;
	temp = LoadRWTex2D(Get(godrayTextures[PASS_TYPE_HORIZONTAL]), id + int2(col - 7, 0)).rgb;
	g_shared_r[row + 8][2 * col + 1] = temp.r;
	g_shared_g[row + 8][2 * col + 1] = temp.g;
	g_shared_b[row + 8][2 * col + 1] = temp.b;

	GroupMemoryBarrier();

	float totalWeight = 0.0f;
	for (int i = -int(mFilterRadius); i <= int(mFilterRadius); i++)
	{
		float weight = Get(mBlurWeights[abs(i) / 4][abs(i) % 4]);
		float color_r = g_shared_r[row + 8][col + 8 + i];
		float color_g = g_shared_g[row + 8][col + 8 + i];
		float color_b = g_shared_b[row + 8][col + 8 + i];
		result += float3(color_r, color_g, color_b) * weight;
		totalWeight += weight;
	}
	return result / totalWeight;
}

float3 VerticalPass(int2 id)
{
	float3 result = float3(0.0f, 0.0f, 0.0f);

	int col = id.x % 16;
	int row = id.y % 16;

	float3 temp;
	
	temp = LoadRWTex2D(Get(godrayTextures[PASS_TYPE_VERTICAL]), id + int2(0, row - 8)).rgb;
	g_shared_r[2 * row + 0][col + 8] = temp.r;
	g_shared_g[2 * row + 0][col + 8] = temp.g;
	g_shared_b[2 * row + 0][col + 8] = temp.b;
	temp = LoadRWTex2D(Get(godrayTextures[PASS_TYPE_VERTICAL]), id + int2(0, row - 7)).rgb;
	g_shared_r[2 * row + 1][col + 8] = temp.r;
	g_shared_g[2 * row + 1][col + 8] = temp.g;
	g_shared_b[2 * row + 1][col + 8] = temp.b;

	GroupMemoryBarrier();

	float totalWeight = 0.0f;
	for (int i = -int(mFilterRadius); i <= int(mFilterRadius); i++)
	{
		float weight = Get(mBlurWeights[abs(i) / 4][abs(i) % 4]);
		float color_r = g_shared_r[row + 8 + i][col + 8];
		float color_g = g_shared_g[row + 8 + i][col + 8];
		float color_b = g_shared_b[row + 8 + i][col + 8];
		result += float3(color_r, color_g, color_b) * weight;
		totalWeight += weight;
	}
	return result / totalWeight;
}

#else

float3 HorizontalPass(int2 id)
{
	float3 result = float3(0.0f, 0.0f, 0.0f);

	float totalWeight = 0.0f;
	for (int i = -int(mFilterRadius); i <= int(mFilterRadius); i++)
	{
		float weight = Get(mBlurWeights[abs(i) / 4][abs(i) % 4]);
		result += LoadRWTex2D(Get(godrayTextures[PASS_TYPE_HORIZONTAL]), id + int2(i, 0)).rgb * weight;
		totalWeight += weight;
	}
	return result / totalWeight;
}

float3 VerticalPass(int2 id)
{
	float3 result = float3(0.0f, 0.0f, 0.0f);

	float totalWeight = 0.0f;
	for (int i = -int(mFilterRadius); i <= int(mFilterRadius); i++)
	{
		float weight = Get(mBlurWeights[abs(i) / 4][abs(i) % 4]);
		result += LoadRWTex2D(Get(godrayTextures[PASS_TYPE_VERTICAL]), id + int2(0, i)).rgb * weight;
		totalWeight += weight;
	}
	return result / totalWeight;
}

#endif

[numthreads(THREADX, THREADY, 1)]
void CS_MAIN( uint3 ThreadID : SV_DispatchThreadID) 
{
	INIT_MAIN;
	if(mBlurPassType == PASS_TYPE_HORIZONTAL)
	{
		float3 result = HorizontalPass(int2(threadID.xy));
		Write2D(Get(godrayTextures[PASS_TYPE_VERTICAL]), threadID.xy, float4(result, 1.0f));
	}
	else
	{
		float3 result = VerticalPass(int2(threadID.xy));
		Write2D(Get(godrayTextures[PASS_TYPE_HORIZONTAL]), threadID.xy, float4(result, 1.0f));
	}
	
	return;
}