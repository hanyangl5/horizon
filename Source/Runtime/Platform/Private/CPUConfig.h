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

#pragma once
#include <Core/IConfig.h>

#include <stdbool.h>

#if defined(_x86_64) || defined(__x86_64__) || defined(_M_X64) || defined(__x86_64) || defined(__SSE2__) || defined(STBIR_SSE) || \
    defined(_M_IX86_FP) || defined(__i386) || defined(__i386__) || defined(_M_IX86) || defined(_X86_)
#include <ThirdParty/cpu_features/include/cpuinfo_x86.h>
#endif
#if defined(_M_ARM64) || defined(__aarch64__) || defined(__arm64__)
#include <ThirdParty/cpu_features/include/cpuinfo_aarch64.h>
#endif
//#include <ThirdParty/cpu_features/include/cpuinfo_arm.h>

typedef enum
{
    SIMD_SSE3,
    SIMD_SSE4_1,
    SIMD_SSE4_2,
    SIMD_AVX,
    SIMD_AVX2,
    SIMD_NEON
} SimdIntrinsic;

typedef struct
{
    char          mName[512];
    SimdIntrinsic mSimd;
#if defined(_x86_64) || defined(__x86_64__) || defined(_M_X64) || defined(__x86_64) || defined(__SSE2__) || defined(STBIR_SSE) || \
    defined(_M_IX86_FP) || defined(__i386) || defined(__i386__) || defined(_M_IX86) || defined(_X86_)
    X86FeaturesEnum      s;
    X86Features          mFeaturesX86;
    X86Microarchitecture mArchitectureX86;
#endif
#if defined(_M_ARM64) || defined(__aarch64__) || defined(__arm64__)
    Aarch64Features mFeaturesAarch64;
#endif
} CpuInfo;

#if defined(ANDROID)
#include <jni.h>
bool initCpuInfo(CpuInfo* outCpuInfo, JNIEnv* pJavaEnv);
#else
bool initCpuInfo(CpuInfo* outCpuInfo);
#endif

FORGE_API CpuInfo* getCpuInfo(void);
