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

#include "../GraphicsConfig.h"

#ifdef DIRECT3D12

// Socket is used in microprofile this header need to be included before d3d12 headers
#include <WinSock2.h>

#ifdef XBOX
#include "../../../Xbox/Common_3/Graphics/Direct3D12/Direct3D12X.h"
#else
#define IID_ARGS IID_PPV_ARGS
#endif

// OS
#include <ThirdParty/stb/stb_ds.h>

#include "Core/ILog.h"

#include "Core/IMath.h"

// Renderer
#include <ThirdParty/tinyimageformat/tinyimageformat_apis.h>

#include "RHI/IGraphics.h"
#include "RHI/IRay.h"

#include "Direct3D12Hooks.h"

#include "Core/IMemory.h"
#include "../RendererResourceAPI.h"

// check if WindowsSDK is used which supports raytracing
#ifdef D3D12_RAYTRACING_AVAILABLE

// Enable experimental features and return if they are supported.
// To test them being supported we need to check both their enablement as well as device creation afterwards.
inline bool EnableD3D12ExperimentalFeatures(UUID* experimentalFeatures, uint32_t featureCount)
{
    ID3D12Device* testDevice = NULL;
    bool          ret = SUCCEEDED(D3D12EnableExperimentalFeatures(featureCount, experimentalFeatures, NULL, NULL)) &&
               SUCCEEDED(D3D12CreateDevice(NULL, D3D_FEATURE_LEVEL_11_0, IID_ARGS(&testDevice)));
    if (ret)
        testDevice->Release();

    return ret;
}

// Enable experimental features required for compute-based raytracing fallback.
// This will set active D3D12 devices to DEVICE_REMOVED state.
// Returns bool whether the call succeeded and the device supports the feature.
inline bool EnableComputeRaytracingFallback()
{
    UUID experimentalFeatures[] = { D3D12ExperimentalShaderModels };
    return EnableD3D12ExperimentalFeatures(experimentalFeatures, 1);
}

/************************************************************************/
// Utility Functions Declarations
/************************************************************************/
D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS util_to_dx_acceleration_structure_build_flags(AccelerationStructureBuildFlags flags);
D3D12_RAYTRACING_GEOMETRY_FLAGS                     util_to_dx_geometry_flags(AccelerationStructureGeometryFlags flags);
D3D12_RAYTRACING_INSTANCE_FLAGS                     util_to_dx_instance_flags(AccelerationStructureInstanceFlags flags);
/************************************************************************/
// Forge Raytracing Implementation using DXR
/************************************************************************/
struct Raytracing
{
    Renderer*             pRenderer;
    ID3D12Device5*        prDevice;
    D3D12_RAYTRACING_TIER tier;
};

struct AccelerationStructure
{
    Buffer* pASBuffer;
    Buffer* pScratchBuffer;
    union
    {
        struct
        {
            D3D12_RAYTRACING_GEOMETRY_DESC* pGeometryDescs;
        };
        struct
        {
            Buffer* pInstanceDescBuffer;
        };
    };
    uint32_t                                            descCount;
    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS flags;
    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE        type;
};

struct RaytracingShaderTable
{
    Pipeline*                   pPipeline;
    Buffer*                     pBuffer;
    D3D12_GPU_DESCRIPTOR_HANDLE viewGpuDescriptorHandle[DESCRIPTOR_UPDATE_FREQ_COUNT];
    D3D12_GPU_DESCRIPTOR_HANDLE samplerGpuDescriptorHandle[DESCRIPTOR_UPDATE_FREQ_COUNT];
    uint32_t                    viewDescriptorCount[DESCRIPTOR_UPDATE_FREQ_COUNT];
    uint32_t                    samplerDescriptorCount[DESCRIPTOR_UPDATE_FREQ_COUNT];
    uint64_t                    maxEntrySize;
    uint64_t                    missRecordSize;
    uint64_t                    hitGroupRecordSize;
};

bool d3d12_initRaytracing(Renderer* pRenderer, Raytracing** ppRaytracing)
{
    ASSERT(pRenderer);
    ASSERT(ppRaytracing);

    if (!pRenderer->pGpu->settings.raytracingSupported)
    {
        return false;
    }

    Raytracing* pRaytracing = (Raytracing*)tf_calloc(1, sizeof(*pRaytracing));
    ASSERT(pRaytracing);

    pRaytracing->pRenderer = pRenderer;
    pRenderer->dx.pDevice->QueryInterface(IID_ARGS(&pRaytracing->prDevice));

    D3D12_FEATURE_DATA_D3D12_OPTIONS5 opts5 = {};
    HRESULT hres = pRenderer->dx.pDevice->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS5, &opts5, sizeof(opts5));
    ASSERT(SUCCEEDED(hres));
    pRaytracing->tier = opts5.RaytracingTier;

    *ppRaytracing = pRaytracing;
    return true;
}

void d3d12_removeRaytracing(Renderer* pRenderer, Raytracing* pRaytracing)
{
    ASSERT(pRenderer);
    ASSERT(pRaytracing);

    if (pRaytracing->prDevice)
        pRaytracing->prDevice->Release();

    tf_free(pRaytracing);
}

static inline FORGE_CONSTEXPR D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE ToDXRASType(AccelerationStructureType type)
{
    return ACCELERATION_STRUCTURE_TYPE_BOTTOM == type ? D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL
                                                      : D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;
}

void d3d12_addAccelerationStructure(Raytracing* pRaytracing, const AccelerationStructureDesc* pDesc,
                                    AccelerationStructure** ppAccelerationStructure)
{
    ASSERT(pRaytracing);
    ASSERT(pDesc);
    ASSERT(ppAccelerationStructure);

    size_t memSize = sizeof(AccelerationStructure);
    if (ACCELERATION_STRUCTURE_TYPE_BOTTOM == pDesc->type)
    {
        memSize += pDesc->bottom.descCount * sizeof(D3D12_RAYTRACING_GEOMETRY_DESC);
    }

    AccelerationStructure* pAS = (AccelerationStructure*)tf_calloc(1, memSize);
    ASSERT(pAS);

    pAS->flags = util_to_dx_acceleration_structure_build_flags(pDesc->flags);
    pAS->type = ToDXRASType(pDesc->type);

    uint32_t scratchBufferSize = 0;

    if (ACCELERATION_STRUCTURE_TYPE_BOTTOM == pDesc->type)
    {
        pAS->descCount = pDesc->bottom.descCount;
        pAS->pGeometryDescs = (D3D12_RAYTRACING_GEOMETRY_DESC*)(pAS + 1); //-V1027
        for (uint32_t j = 0; j < pAS->descCount; ++j)
        {
            AccelerationStructureGeometryDesc* pGeom = &pDesc->bottom.pGeometryDescs[j];
            D3D12_RAYTRACING_GEOMETRY_DESC*    pGeomD3D12 = &pAS->pGeometryDescs[j];

            pGeomD3D12->Flags = util_to_dx_geometry_flags(pGeom->flags);

            if (pGeom->indexCount)
            {
                ASSERT(pGeom->pIndexBuffer);
                pGeomD3D12->Triangles.IndexBuffer = pGeom->pIndexBuffer->dx.gpuAddress + pGeom->indexOffset;
                pGeomD3D12->Triangles.IndexCount = pGeom->indexCount;
                pGeomD3D12->Triangles.IndexFormat = (pGeom->indexType == INDEX_TYPE_UINT16 ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT);
            }

            ASSERT(pGeom->pVertexBuffer);
            ASSERT(pGeom->vertexCount);

            pGeomD3D12->Triangles.VertexBuffer.StartAddress = pGeom->pVertexBuffer->dx.gpuAddress + pGeom->vertexOffset;
            pGeomD3D12->Triangles.VertexBuffer.StrideInBytes = pGeom->vertexStride;
            pGeomD3D12->Triangles.VertexCount = pGeom->vertexCount;
            pGeomD3D12->Triangles.VertexFormat = (DXGI_FORMAT)TinyImageFormat_ToDXGI_FORMAT((TinyImageFormat)pGeom->vertexFormat);
            /*
            Format of the vertices in VertexBuffer. Must be one of the following:

            DXGI_FORMAT_R32G32_FLOAT - third component is assumed 0
            DXGI_FORMAT_R32G32B32_FLOAT
            DXGI_FORMAT_R16G16_FLOAT - third component is assumed 0
            DXGI_FORMAT_R16G16B16A16_FLOAT - A16 component is ignored, other data can be packed there, such as setting vertex stride to 6
            bytes. DXGI_FORMAT_R16G16_SNORM - third component is assumed 0 DXGI_FORMAT_R16G16B16A16_SNORM - A16 component is ignored, other
            data can be packed there, such as setting vertex stride to 6 bytes. Tier 1.1 devices support the following additional formats:

            DXGI_FORMAT_R16G16B16A16_UNORM - A16 component is ignored, other data can be packed there, such as setting vertex stride to 6
            bytes DXGI_FORMAT_R16G16_UNORM - third component assumed 0 DXGI_FORMAT_R10G10B10A2_UNORM - A2 component is ignored, stride must
            be 4 bytes DXGI_FORMAT_R8G8B8A8_UNORM - A8 component is ignored, other data can be packed there, such as setting vertex stride
            to 3 bytes DXGI_FORMAT_R8G8_UNORM - third component assumed 0 DXGI_FORMAT_R8G8B8A8_SNORM - A8 component is ignored, other data
            can be packed there, such as setting vertex stride to 3 bytes DXGI_FORMAT_R8G8_SNORM - third component assumed 0
            */
            ASSERT(DXGI_FORMAT_R32G32_FLOAT == pGeomD3D12->Triangles.VertexFormat ||
                   DXGI_FORMAT_R32G32B32_FLOAT == pGeomD3D12->Triangles.VertexFormat ||
                   DXGI_FORMAT_R16G16_FLOAT == pGeomD3D12->Triangles.VertexFormat ||
                   DXGI_FORMAT_R16G16B16A16_FLOAT == pGeomD3D12->Triangles.VertexFormat ||
                   DXGI_FORMAT_R16G16_SNORM == pGeomD3D12->Triangles.VertexFormat ||
                   DXGI_FORMAT_R16G16B16A16_SNORM == pGeomD3D12->Triangles.VertexFormat ||
                   ((pRaytracing->tier > D3D12_RAYTRACING_TIER_1_0)
                        ? (DXGI_FORMAT_R16G16B16A16_UNORM == pGeomD3D12->Triangles.VertexFormat ||
                           DXGI_FORMAT_R16G16_UNORM == pGeomD3D12->Triangles.VertexFormat ||
                           DXGI_FORMAT_R10G10B10A2_UNORM == pGeomD3D12->Triangles.VertexFormat ||
                           DXGI_FORMAT_R8G8B8A8_UNORM == pGeomD3D12->Triangles.VertexFormat ||
                           DXGI_FORMAT_R8G8_UNORM == pGeomD3D12->Triangles.VertexFormat ||
                           DXGI_FORMAT_R8G8B8A8_SNORM == pGeomD3D12->Triangles.VertexFormat ||
                           DXGI_FORMAT_R8G8_SNORM == pGeomD3D12->Triangles.VertexFormat)
                        : false));
        }
        /************************************************************************/
        // Get the size requirement for the Acceleration Structures
        /************************************************************************/
        D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS prebuildDesc = {};
        prebuildDesc.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
        prebuildDesc.Flags = pAS->flags;
        prebuildDesc.NumDescs = pAS->descCount;
        prebuildDesc.pGeometryDescs = pAS->pGeometryDescs;
        prebuildDesc.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;

        D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO info = {};
        pRaytracing->prDevice->GetRaytracingAccelerationStructurePrebuildInfo(&prebuildDesc, &info);

        /************************************************************************/
        // Allocate Acceleration Structure Buffer
        /************************************************************************/
        BufferDesc bufferDesc = {};
        bufferDesc.descriptors = DESCRIPTOR_TYPE_RW_BUFFER;
        bufferDesc.memoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
        bufferDesc.flags = BUFFER_CREATION_FLAG_OWN_MEMORY_BIT | BUFFER_CREATION_FLAG_NO_DESCRIPTOR_VIEW_CREATION;
        bufferDesc.structStride = 0;
        bufferDesc.firstElement = 0;
        bufferDesc.elementCount = (uint32_t)(info.ResultDataMaxSizeInBytes / sizeof(UINT32));
        bufferDesc.size = info.ResultDataMaxSizeInBytes;
        bufferDesc.startState = RESOURCE_STATE_ACCELERATION_STRUCTURE_WRITE;
        addBuffer(pRaytracing->pRenderer, &bufferDesc, &pAS->pASBuffer);
        /************************************************************************/
        // Store the scratch buffer size so user can create the scratch buffer accordingly
        /************************************************************************/
        scratchBufferSize = (UINT)info.ScratchDataSizeInBytes > scratchBufferSize ? (UINT)info.ScratchDataSizeInBytes : scratchBufferSize;
    }
    else
    {
        pAS->descCount = pDesc->top.descCount;
        /************************************************************************/
        // Get the size requirement for the Acceleration Structures
        /************************************************************************/
        D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS prebuildDesc = {};
        prebuildDesc.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
        prebuildDesc.Flags = util_to_dx_acceleration_structure_build_flags(pDesc->flags);
        prebuildDesc.NumDescs = pDesc->top.descCount;
        prebuildDesc.pGeometryDescs = NULL;
        prebuildDesc.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;

        D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO info = {};
        pRaytracing->prDevice->GetRaytracingAccelerationStructurePrebuildInfo(&prebuildDesc, &info);

        /************************************************************************/
        /*  Construct buffer with instances descriptions                        */
        /************************************************************************/
        D3D12_RAYTRACING_INSTANCE_DESC* instanceDescs = NULL;
        arrsetlen(instanceDescs, pDesc->top.descCount);
        for (uint32_t i = 0; i < pDesc->top.descCount; ++i)
        {
            AccelerationStructureInstanceDesc* pInst = &pDesc->top.pInstanceDescs[i];
            ASSERT(pInst->pBottomAS);

            const Buffer* pASBuffer = pInst->pBottomAS->pASBuffer;
            instanceDescs[i].AccelerationStructure = pASBuffer->dx.pResource->GetGPUVirtualAddress();
            instanceDescs[i].Flags = util_to_dx_instance_flags(pInst->flags);
            instanceDescs[i].InstanceContributionToHitGroupIndex = pInst->instanceContributionToHitGroupIndex;
            instanceDescs[i].InstanceID = pInst->instanceID;
            instanceDescs[i].InstanceMask = pInst->instanceMask;

            memcpy(instanceDescs[i].Transform, pInst->transform, sizeof(float[12])); //-V595
        }

        BufferDesc instanceDesc = {};
        instanceDesc.memoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
        instanceDesc.flags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
        instanceDesc.size = arrlenu(instanceDescs) * sizeof(instanceDescs[0]);
        addBuffer(pRaytracing->pRenderer, &instanceDesc, &pAS->pInstanceDescBuffer);
        if (arrlen(instanceDescs))
        {
            memcpy(pAS->pInstanceDescBuffer->pCpuMappedAddress, instanceDescs, instanceDesc.size);
        }
        arrfree(instanceDescs);
        /************************************************************************/
        // Allocate Acceleration Structure Buffer
        /************************************************************************/
        BufferDesc bufferDesc = {};
        bufferDesc.descriptors = DESCRIPTOR_TYPE_RW_BUFFER_RAW | DESCRIPTOR_TYPE_BUFFER_RAW;
        bufferDesc.memoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
        bufferDesc.flags = BUFFER_CREATION_FLAG_OWN_MEMORY_BIT;
        bufferDesc.structStride = 0;
        bufferDesc.firstElement = 0;
        bufferDesc.elementCount = (uint32_t)(info.ResultDataMaxSizeInBytes / sizeof(UINT32));
        bufferDesc.size = info.ResultDataMaxSizeInBytes;
        bufferDesc.startState = RESOURCE_STATE_ACCELERATION_STRUCTURE_WRITE;
        addBuffer(pRaytracing->pRenderer, &bufferDesc, &pAS->pASBuffer);

        extern void AddSrv(Renderer*, DescriptorHeap*, ID3D12Resource*, const D3D12_SHADER_RESOURCE_VIEW_DESC*, DxDescriptorID*);

        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_RAYTRACING_ACCELERATION_STRUCTURE;
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.Format = DXGI_FORMAT_UNKNOWN;
        srvDesc.RaytracingAccelerationStructure.Location = pAS->pASBuffer->dx.gpuAddress;
        DxDescriptorID srv = pAS->pASBuffer->dx.descriptors + pAS->pASBuffer->dx.srvDescriptorOffset;
        AddSrv(pRaytracing->pRenderer, NULL, NULL, &srvDesc, &srv);

        scratchBufferSize = (UINT)info.ScratchDataSizeInBytes;
    }

    // Create scratch buffer
    BufferDesc scratchBufferDesc = {};
    scratchBufferDesc.descriptors = DESCRIPTOR_TYPE_RW_BUFFER;
    scratchBufferDesc.memoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
    scratchBufferDesc.startState = RESOURCE_STATE_COMMON;
    scratchBufferDesc.flags = BUFFER_CREATION_FLAG_NO_DESCRIPTOR_VIEW_CREATION;
    scratchBufferDesc.size = scratchBufferSize;
    addBuffer(pRaytracing->pRenderer, &scratchBufferDesc, &pAS->pScratchBuffer);

    *ppAccelerationStructure = pAS;
}

void d3d12_removeAccelerationStructure(Raytracing* pRaytracing, AccelerationStructure* pAccelerationStructure)
{
    ASSERT(pRaytracing);
    ASSERT(pAccelerationStructure);

    removeBuffer(pRaytracing->pRenderer, pAccelerationStructure->pASBuffer);
    if (D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL == pAccelerationStructure->type)
    {
        removeBuffer(pRaytracing->pRenderer, pAccelerationStructure->pInstanceDescBuffer);
    }
    if (pAccelerationStructure->pScratchBuffer)
    {
        removeBuffer(pRaytracing->pRenderer, pAccelerationStructure->pScratchBuffer);
        pAccelerationStructure->pScratchBuffer = NULL;
    }

    tf_free(pAccelerationStructure);
}

void d3d12_removeAccelerationStructureScratch(Raytracing* pRaytracing, AccelerationStructure* pAccelerationStructure)
{
    if (!pAccelerationStructure->pScratchBuffer)
    {
        return;
    }
    removeBuffer(pRaytracing->pRenderer, pAccelerationStructure->pScratchBuffer);
    pAccelerationStructure->pScratchBuffer = NULL;
}

/************************************************************************/
// Raytracing Command Buffer Functions Implementation
/************************************************************************/
void d3d12_cmdBuildAccelerationStructure(Cmd* pCmd, Raytracing* pRaytracing, RaytracingBuildASDesc* pDesc)
{
    UNREF_PARAM(pRaytracing);
    ASSERT(pDesc);

    // TODO: Cache the DXR command list interface on Cmd instead of querying it for every AS build.
    ID3D12GraphicsCommandList4* dxrCmd = NULL;
    pCmd->dx.pCmdList->QueryInterface(IID_ARGS(&dxrCmd));
    ASSERT(dxrCmd);

    AccelerationStructure*                             as = pDesc->pAccelerationStructure;
    const D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE type = as->type;

    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC buildDesc = {};
    buildDesc.Inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
    buildDesc.Inputs.Type = type;
    buildDesc.DestAccelerationStructureData = as->pASBuffer->dx.pResource->GetGPUVirtualAddress();
    buildDesc.Inputs.Flags = as->flags;
    buildDesc.Inputs.pGeometryDescs = NULL;

    if (type == D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL)
    {
        buildDesc.Inputs.pGeometryDescs = as->pGeometryDescs;
    }
    else if (type == D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL)
    {
        buildDesc.Inputs.InstanceDescs = as->pInstanceDescBuffer->dx.pResource->GetGPUVirtualAddress();
    }

    buildDesc.Inputs.NumDescs = as->descCount;
    buildDesc.ScratchAccelerationStructureData = as->pScratchBuffer->dx.pResource->GetGPUVirtualAddress();

    dxrCmd->BuildRaytracingAccelerationStructure(&buildDesc, 0, NULL);

    if (pDesc->issueRWBarrier)
    {
        BufferBarrier barrier = { as->pASBuffer, RESOURCE_STATE_ACCELERATION_STRUCTURE_WRITE, RESOURCE_STATE_ACCELERATION_STRUCTURE_READ };
        cmdResourceBarrier(pCmd, 1, &barrier, 0, NULL, 0, NULL);
    }

    dxrCmd->Release();
}

/************************************************************************/
// Utility Functions Implementation
/************************************************************************/
D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS util_to_dx_acceleration_structure_build_flags(AccelerationStructureBuildFlags flags)
{
    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS ret = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_NONE;
    if (flags & ACCELERATION_STRUCTURE_BUILD_FLAG_ALLOW_COMPACTION)
        ret |= D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_ALLOW_COMPACTION;
    if (flags & ACCELERATION_STRUCTURE_BUILD_FLAG_ALLOW_UPDATE)
        ret |= D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_ALLOW_UPDATE;
    if (flags & ACCELERATION_STRUCTURE_BUILD_FLAG_MINIMIZE_MEMORY)
        ret |= D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_MINIMIZE_MEMORY;
    if (flags & ACCELERATION_STRUCTURE_BUILD_FLAG_PERFORM_UPDATE)
        ret |= D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PERFORM_UPDATE;
    if (flags & ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_BUILD)
        ret |= D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_BUILD;
    if (flags & ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE)
        ret |= D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;

    return ret;
}

D3D12_RAYTRACING_GEOMETRY_FLAGS util_to_dx_geometry_flags(AccelerationStructureGeometryFlags flags)
{
    D3D12_RAYTRACING_GEOMETRY_FLAGS ret = D3D12_RAYTRACING_GEOMETRY_FLAG_NONE;
    if (flags & ACCELERATION_STRUCTURE_GEOMETRY_FLAG_OPAQUE)
        ret |= D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE;
    if (flags & ACCELERATION_STRUCTURE_GEOMETRY_FLAG_NO_DUPLICATE_ANYHIT_INVOCATION)
        ret |= D3D12_RAYTRACING_GEOMETRY_FLAG_NO_DUPLICATE_ANYHIT_INVOCATION;

    return ret;
}

D3D12_RAYTRACING_INSTANCE_FLAGS util_to_dx_instance_flags(AccelerationStructureInstanceFlags flags)
{
    D3D12_RAYTRACING_INSTANCE_FLAGS ret = D3D12_RAYTRACING_INSTANCE_FLAG_NONE;
    if (flags & ACCELERATION_STRUCTURE_INSTANCE_FLAG_FORCE_OPAQUE)
        ret |= D3D12_RAYTRACING_INSTANCE_FLAG_FORCE_OPAQUE;
    if (flags & ACCELERATION_STRUCTURE_INSTANCE_FLAG_TRIANGLE_CULL_DISABLE)
        ret |= D3D12_RAYTRACING_INSTANCE_FLAG_TRIANGLE_CULL_DISABLE;
    if (flags & ACCELERATION_STRUCTURE_INSTANCE_FLAG_TRIANGLE_FRONT_COUNTERCLOCKWISE)
        ret |= D3D12_RAYTRACING_INSTANCE_FLAG_TRIANGLE_FRONT_COUNTERCLOCKWISE;

    return ret;
}

void fillRaytracingDescriptorHandle(AccelerationStructure* pAccelerationStructure, DxDescriptorID* pOutId)
{
    *pOutId = pAccelerationStructure->pASBuffer->dx.descriptors + pAccelerationStructure->pASBuffer->dx.srvDescriptorOffset;
}

#endif

void initD3D12RaytracingFunctions() {}

#endif
