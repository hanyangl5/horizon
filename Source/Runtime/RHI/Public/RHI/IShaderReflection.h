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

#include "RHI/IGraphics.h"

#include <ctype.h>

static const uint32_t MAX_SHADER_STAGE_COUNT = 5;

enum TextureDimension : uint32_t
{
    TEXTURE_DIM_1D,
    TEXTURE_DIM_2D,
    TEXTURE_DIM_2DMS,
    TEXTURE_DIM_3D,
    TEXTURE_DIM_CUBE,
    TEXTURE_DIM_1D_ARRAY,
    TEXTURE_DIM_2D_ARRAY,
    TEXTURE_DIM_2DMS_ARRAY,
    TEXTURE_DIM_CUBE_ARRAY,
    TEXTURE_DIM_COUNT,
    TEXTURE_DIM_UNDEFINED,
};

struct VertexInput
{
    // resource name
    const char* name;

    // The size of the attribute
    uint32_t size;

    // name size
    uint32_t name_size;
};

struct ShaderResource
{
    // resource Type
    DescriptorType type;

    // The resource set for binding frequency
    uint32_t set;

    // The resource binding location
    uint32_t reg;

    // The size of the resource. This will be the DescriptorInfo array size for textures
    uint32_t size;

    // what stages use this resource
    ShaderStage used_stages;

    // resource name
    const char* name;

    // name size
    uint32_t name_size;

    // 1D / 2D / Array / MSAA / ...
    TextureDimension dim;
};

struct ShaderVariable
{
    // Variable name
    const char* name;

    // parents resource index
    uint32_t parent_index;

    // The offset of the Variable.
    uint32_t offset;

    // The size of the Variable.
    uint32_t size;

    // name size
    uint32_t name_size;
};

struct ShaderReflection
{
    // single large allocation for names to reduce number of allocations
    char*           pNamePool;
    VertexInput*    pVertexInputs;
    ShaderResource* pShaderResources;
    ShaderVariable* pVariables;
    // char*           pEntryPoint;

    ShaderStage mShaderStage;

    uint32_t mNamePoolSize;
    uint32_t mVertexInputsCount;
    uint32_t mShaderResourceCount;
    uint32_t mVariableCount;

    // Thread group size for compute shader
    uint32_t mNumThreadsPerGroup[3];

    uint32_t mOutputRenderTargetTypesMask;

    // number of tessellation control point
    uint32_t mNumControlPoint;
    bool     mCbvHeapIndexing;
    bool     mSamplerHeapIndexing;
};

struct PipelineReflection
{
    ShaderStage      mShaderStages;
    // the individual stages reflection data.
    ShaderReflection mStageReflections[MAX_SHADER_STAGE_COUNT];
    uint32_t         mStageReflectionCount;

    uint32_t mVertexStageIndex;
    uint32_t mHullStageIndex;
    uint32_t mDomainStageIndex;
    uint32_t mGeometryStageIndex;
    uint32_t mPixelStageIndex;

    ShaderResource* pShaderResources;
    uint32_t        mShaderResourceCount;

    ShaderVariable* pVariables;
    uint32_t        mVariableCount;
};

FORGE_RENDERER_API void destroyShaderReflection(ShaderReflection* pReflection);

FORGE_RENDERER_API void createPipelineReflection(ShaderReflection* pReflection, uint32_t stageCount, PipelineReflection* pOutReflection);
FORGE_RENDERER_API void destroyPipelineReflection(PipelineReflection* pReflection);

inline bool isDescriptorRootConstant(const char* resourceName)
{
    char     lower[MAX_RESOURCE_NAME_LENGTH] = {};
    uint32_t length = (uint32_t)strlen(resourceName);
    for (uint32_t i = 0; i < length; ++i)
    {
        lower[i] = (char)tolower(resourceName[i]);
    }
    return strstr(lower, "rootconstant") || strstr(lower, "pushconstant");
}

inline bool isDescriptorRootCbv(const char* resourceName)
{
    char     lower[MAX_RESOURCE_NAME_LENGTH] = {};
    uint32_t length = (uint32_t)strlen(resourceName);
    for (uint32_t i = 0; i < length; ++i)
    {
        lower[i] = (char)tolower(resourceName[i]);
    }
    return strstr(lower, "rootcbv");
}

// void serializeReflection(File* pInFile, Reflection* pReflection);
// void deserializeReflection(File* pOutFile, Reflection* pReflection);
