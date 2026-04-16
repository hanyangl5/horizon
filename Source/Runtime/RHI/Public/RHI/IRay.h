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

#include "IGraphics.h"

struct Renderer;
struct Raytracing;
struct Buffer;
struct Texture;
struct Cmd;
struct AccelerationStructure;
struct AccelerationStructureDescBottom;
struct RootSignature;
struct ShaderResource;
struct DescriptorData;
struct ID3D12Device5;

enum AccelerationStructureType : uint32_t
{
    ACCELERATION_STRUCTURE_TYPE_BOTTOM = 0,
    ACCELERATION_STRUCTURE_TYPE_TOP,
};

// Supported by DXR.
enum AccelerationStructureBuildFlags : uint32_t
{
    ACCELERATION_STRUCTURE_BUILD_FLAG_NONE = 0,
    ACCELERATION_STRUCTURE_BUILD_FLAG_ALLOW_UPDATE = 0x1,
    ACCELERATION_STRUCTURE_BUILD_FLAG_ALLOW_COMPACTION = 0x2,
    ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE = 0x4,
    ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_BUILD = 0x8,
    ACCELERATION_STRUCTURE_BUILD_FLAG_MINIMIZE_MEMORY = 0x10,
    ACCELERATION_STRUCTURE_BUILD_FLAG_PERFORM_UPDATE = 0x20,
};
MAKE_ENUM_FLAG(uint32_t, AccelerationStructureBuildFlags)

enum AccelerationStructureGeometryFlags : uint32_t
{
    ACCELERATION_STRUCTURE_GEOMETRY_FLAG_NONE = 0,
    ACCELERATION_STRUCTURE_GEOMETRY_FLAG_OPAQUE = 0x1,
    ACCELERATION_STRUCTURE_GEOMETRY_FLAG_NO_DUPLICATE_ANYHIT_INVOCATION = 0x2
};
MAKE_ENUM_FLAG(uint32_t, AccelerationStructureGeometryFlags)

enum AccelerationStructureInstanceFlags : uint32_t
{
    ACCELERATION_STRUCTURE_INSTANCE_FLAG_NONE = 0,
    ACCELERATION_STRUCTURE_INSTANCE_FLAG_TRIANGLE_CULL_DISABLE = 0x1,
    ACCELERATION_STRUCTURE_INSTANCE_FLAG_TRIANGLE_FRONT_COUNTERCLOCKWISE = 0x2,
    ACCELERATION_STRUCTURE_INSTANCE_FLAG_FORCE_OPAQUE = 0x4,
    ACCELERATION_STRUCTURE_INSTANCE_FLAG_FORCE_NON_OPAQUE = 0x8
};
MAKE_ENUM_FLAG(uint32_t, AccelerationStructureInstanceFlags)

struct AccelerationStructureInstanceDesc
{
    AccelerationStructure*             pBottomAS;
    /// Row major affine transform for transforming the vertices in the geometry stored in pAccelerationStructure
    float                              mTransform[12];
    /// User defined instanced ID which can be queried in the shader
    uint32_t                           mInstanceID;
    uint32_t                           mInstanceMask;
    uint32_t                           mInstanceContributionToHitGroupIndex;
    AccelerationStructureInstanceFlags mFlags;
};

struct AccelerationStructureGeometryDesc
{
    Buffer*                            pVertexBuffer;
    Buffer*                            pIndexBuffer;
    uint32_t                           mVertexOffset;
    uint32_t                           mVertexCount;
    uint32_t                           mVertexStride;
    TinyImageFormat                    mVertexFormat;
    uint32_t                           mIndexOffset;
    uint32_t                           mIndexCount;
    IndexType                          mIndexType;
    AccelerationStructureGeometryFlags mFlags;
};
/************************************************************************/
//	  Bottom Level Structures define the geometry data such as vertex buffers, index buffers
//	  Top Level Structures define the instance data for the geometry such as instance matrix, instance ID, ...
// #mDescCount - Number of geometries or instances in this structure
/************************************************************************/
struct AccelerationStructureDescBottom
{
    /// Number of geometries / instances in thie acceleration structure
    uint32_t                           mDescCount;
    /// Array of geometries in the bottom level acceleration structure
    AccelerationStructureGeometryDesc* pGeometryDescs;
};

struct AccelerationStructureDescTop
{
    uint32_t                           mDescCount;
    AccelerationStructureInstanceDesc* pInstanceDescs;
};

struct AccelerationStructureDesc
{
    AccelerationStructureType       mType;
    AccelerationStructureBuildFlags mFlags;
    union
    {
        AccelerationStructureDescBottom mBottom;
        AccelerationStructureDescTop    mTop;
    };
};

struct RaytracingBuildASDesc
{
    AccelerationStructure* pAccelerationStructure;
    bool                   mIssueRWBarrier;
};

FORGE_RENDERER_API bool FORGE_CALLCONV initRaytracing(Renderer* pRenderer, Raytracing** ppRaytracing);
FORGE_RENDERER_API void FORGE_CALLCONV removeRaytracing(Renderer* pRenderer, Raytracing* pRaytracing);

/// pScratchBufferSize - Holds the size of scratch buffer to be passed to cmdBuildAccelerationStructure
FORGE_RENDERER_API void FORGE_CALLCONV addAccelerationStructure(Raytracing* pRaytracing, const AccelerationStructureDesc* pDesc,
                                                                AccelerationStructure** ppAccelerationStructure);
FORGE_RENDERER_API void FORGE_CALLCONV removeAccelerationStructure(Raytracing* pRaytracing,
                                                                   AccelerationStructure* pAccelerationStructure);
/// Free the scratch memory allocated by acceleration structure after it has been built completely
/// Does not free acceleration structure
FORGE_RENDERER_API void FORGE_CALLCONV removeAccelerationStructureScratch(Raytracing* pRaytracing,
                                                                          AccelerationStructure* pAccelerationStructure);

FORGE_RENDERER_API void FORGE_CALLCONV cmdBuildAccelerationStructure(Cmd* pCmd, Raytracing* pRaytracing,
                                                                     RaytracingBuildASDesc* pDesc);
