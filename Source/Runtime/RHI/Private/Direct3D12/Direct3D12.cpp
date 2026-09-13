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

#define RENDERER_IMPLEMENTATION

#if defined(XBOX)
#include "../../../Xbox/Common_3/Graphics/Direct3D12/Direct3D12X.h"
#else
#define IID_ARGS IID_PPV_ARGS
#endif

// Pull in minimal Windows headers
#include <Windows.h>

#include <ThirdParty/stb/stb_ds.h>
#include <ThirdParty/bstrlib/bstrlib.h>
#include <ThirdParty/D3D12MemoryAllocator/include/D3D12MemAlloc.h>

#include "RHI/IGraphics.h"

#if defined(XBOX)
#include <pix3.h>
#else
#include <ThirdParty/winpixeventruntime/Include/WinPixEventRuntime/pix3.h>
#endif

#include <ThirdParty/tinyimageformat/tinyimageformat_base.h>
#include <ThirdParty/tinyimageformat/tinyimageformat_query.h>
#include <dxcapi.h>
#include <ThirdParty/DirectStorage/include/dstorage.h>
// #include <ThirdParty/renderdoc/renderdoc_app.h>

#include "Core/IFileSystem.h"
#include "Core/ILog.h"

#include "Core/IAlgorithm.h"
#include "Core/IMath.h"

#include "Direct3D12CapBuilder.h"
#include "Direct3D12Hooks.h"
#include "AgsHelper.h"
#include "NvApiHelper.h"

#if defined(AUTOMATED_TESTING)
#include "Application/IScreenshot.h"
#endif

#if defined(ENABLE_TRACY_MEMORY)
#include <tracy/TracyC.h>
#endif

#if !defined(_WINDOWS) && !defined(XBOX)
#error "Windows is needed!"
#endif

#if defined(ENABLE_GRAPHICS_DEBUG) && !defined(XBOX)
#include <dxgidebug.h>
#endif

//
// C++ is the only language supported by D3D12:
//   https://msdn.microsoft.com/en-us/library/windows/desktop/dn899120(v=vs.85).aspx
//
#if !defined(__cplusplus)
#error "D3D12 requires C++! Sorry!"
#endif

#include "Core/IMemory.h"
#include "../RendererResourceAPI.h"

#define D3D12_GPU_VIRTUAL_ADDRESS_NULL    ((D3D12_GPU_VIRTUAL_ADDRESS)0)
#define D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN ((D3D12_GPU_VIRTUAL_ADDRESS)-1)
#define D3D12_REQ_CONSTANT_BUFFER_SIZE    (D3D12_REQ_CONSTANT_BUFFER_ELEMENT_COUNT * 16u)
#define D3D12_DESCRIPTOR_ID_NONE          ((int32_t)-1)

#define MAX_COMPILE_ARGS                  64

static const wchar_t* d3d12_getShaderStageProfilePrefix(ShaderStage stage)
{
    switch (stage)
    {
    case SHADER_STAGE_VERT:
        return L"vs";
    case SHADER_STAGE_FRAG:
        return L"ps";
    case SHADER_STAGE_COMP:
        return L"cs";
    case SHADER_STAGE_HULL:
        return L"hs";
    case SHADER_STAGE_DOMN:
        return L"ds";
    case SHADER_STAGE_GEOM:
        return L"gs";
    default:
        return L"vs";
    }
}

static const wchar_t* d3d12_getShaderTargetProfileSuffix(ShaderTarget target)
{
    switch (target)
    {
    case SHADER_TARGET_5_1:
        return L"5_1";
    case SHADER_TARGET_6_0:
        return L"6_0";
    case SHADER_TARGET_6_1:
        return L"6_1";
    case SHADER_TARGET_6_2:
        return L"6_2";
    case SHADER_TARGET_6_3:
        return L"6_3";
    case SHADER_TARGET_6_4:
        return L"6_4";
    case SHADER_TARGET_6_5:
        return L"6_5";
    case SHADER_TARGET_6_6:
        return L"6_6";
    case SHADER_TARGET_6_7:
        return L"6_7";
    case SHADER_TARGET_6_8:
        return L"6_8";
    case SHADER_TARGET_6_9:
        return L"6_9";
    case SHADER_TARGET_6_10:
        return L"6_10";
    default:
        ASSERT(false);
        return L"6_0";
    }
}

static void d3d12_getShaderProfile(ShaderStage stage, ShaderTarget target, wchar_t* pOutProfile, size_t profileCount)
{
    swprintf_s(pOutProfile, profileCount, L"%s_%s", d3d12_getShaderStageProfilePrefix(stage), d3d12_getShaderTargetProfileSuffix(target));
}

static D3D_SHADER_MODEL d3d12_getD3DShaderModel(ShaderTarget target)
{
    switch (target)
    {
    case SHADER_TARGET_6_0:
        return D3D_SHADER_MODEL_6_0;
    case SHADER_TARGET_6_1:
        return D3D_SHADER_MODEL_6_1;
    case SHADER_TARGET_6_2:
        return D3D_SHADER_MODEL_6_2;
    case SHADER_TARGET_6_3:
        return D3D_SHADER_MODEL_6_3;
    case SHADER_TARGET_6_4:
        return D3D_SHADER_MODEL_6_4;
    case SHADER_TARGET_6_5:
        return D3D_SHADER_MODEL_6_5;
    case SHADER_TARGET_6_6:
        return D3D_SHADER_MODEL_6_6;
    case SHADER_TARGET_6_7:
        return D3D_SHADER_MODEL_6_7;
    case SHADER_TARGET_6_8:
        return D3D_SHADER_MODEL_6_8;
    case SHADER_TARGET_6_9:
        return D3D_SHADER_MODEL_6_9;
    case SHADER_TARGET_6_10:
        return D3D_SHADER_MODEL_6_10;
    default:
        ASSERT(false);
        return D3D_SHADER_MODEL_6_0;
    }
}

extern void d3d12_createShaderReflection(const uint8_t* shaderCode, uint32_t shaderSize, ShaderStage shaderStage,
                                         ShaderReflection* pOutReflection);

// stubs for durango because Direct3D12Raytracing.cpp is not used on XBOX
#if defined(D3D12_RAYTRACING_AVAILABLE)
extern void fillRaytracingDescriptorHandle(AccelerationStructure* pAccelerationStructure, DxDescriptorID* pOutId);
#endif
// Enabling DRED
#if defined(_WIN32) && defined(_DEBUG) && defined(DRED)
#define USE_DRED 1
#endif

static void SetObjectName(ID3D12Object* pObject, const char* pName)
{
    UNREF_PARAM(pObject);
    UNREF_PARAM(pName);
#if defined(ENABLE_GRAPHICS_DEBUG)
    if (!pName)
    {
        return;
    }

    wchar_t wName[MAX_DEBUG_NAME_LENGTH] = {};
    size_t  numConverted = 0;
    mbstowcs_s(&numConverted, wName, pName, MAX_DEBUG_NAME_LENGTH);
    pObject->SetName(wName);
#endif
}

enum D3D12MemoryTrackingMode : uint32_t
{
    D3D12_MEMORY_TRACKING_NONE = 0,
    D3D12_MEMORY_TRACKING_D3D12MA = 1,
    D3D12_MEMORY_TRACKING_RESOURCE = 2,
};

enum D3D12MemoryTrackingPool : uint32_t
{
    D3D12_MEMORY_TRACKING_POOL_UNKNOWN = 0,
    D3D12_MEMORY_TRACKING_POOL_DEFAULT = 1,
    D3D12_MEMORY_TRACKING_POOL_UPLOAD = 2,
    D3D12_MEMORY_TRACKING_POOL_READBACK = 3,
    D3D12_MEMORY_TRACKING_POOL_CUSTOM = 4,
    D3D12_MEMORY_TRACKING_POOL_GPU_UPLOAD = 5,
    D3D12_MEMORY_TRACKING_POOL_EXPLICIT_HEAP = 6,
};

D3D12_HEAP_TYPE util_to_heap_type(ResourceMemoryUsage memoryUsage);

#if defined(ENABLE_TRACY_MEMORY)
#ifndef HORIZON_TRACY_MEMORY_CALLSTACK_DEPTH
#define HORIZON_TRACY_MEMORY_CALLSTACK_DEPTH 16
#endif

static uint32_t d3d12_memory_pool_from_heap_type(D3D12_HEAP_TYPE heapType)
{
    switch (heapType)
    {
    case D3D12_HEAP_TYPE_DEFAULT:
        return D3D12_MEMORY_TRACKING_POOL_DEFAULT;
    case D3D12_HEAP_TYPE_UPLOAD:
        return D3D12_MEMORY_TRACKING_POOL_UPLOAD;
    case D3D12_HEAP_TYPE_READBACK:
        return D3D12_MEMORY_TRACKING_POOL_READBACK;
    case D3D12_HEAP_TYPE_CUSTOM:
        return D3D12_MEMORY_TRACKING_POOL_CUSTOM;
    case D3D12_HEAP_TYPE_GPU_UPLOAD:
        return D3D12_MEMORY_TRACKING_POOL_GPU_UPLOAD;
    default:
        return D3D12_MEMORY_TRACKING_POOL_UNKNOWN;
    }
}

static uint32_t d3d12_memory_pool_from_usage(ResourceMemoryUsage usage)
{
    return d3d12_memory_pool_from_heap_type(util_to_heap_type(usage));
}

static const char* d3d12_memory_pool_name(uint32_t pool)
{
    switch (pool)
    {
    case D3D12_MEMORY_TRACKING_POOL_DEFAULT:
        return "GPU/D3D12 Default";
    case D3D12_MEMORY_TRACKING_POOL_UPLOAD:
        return "GPU/D3D12 Upload";
    case D3D12_MEMORY_TRACKING_POOL_READBACK:
        return "GPU/D3D12 Readback";
    case D3D12_MEMORY_TRACKING_POOL_CUSTOM:
        return "GPU/D3D12 Custom";
    case D3D12_MEMORY_TRACKING_POOL_GPU_UPLOAD:
        return "GPU/D3D12 GPU Upload";
    case D3D12_MEMORY_TRACKING_POOL_EXPLICIT_HEAP:
        return "GPU/D3D12 Explicit Heap";
    default:
        return "GPU/D3D12 Unknown";
    }
}

static void d3d12_track_gpu_alloc(const void* ptr, uint64_t size, uint32_t pool)
{
    if (ptr && size)
    {
        TracyCAllocNS(ptr, (size_t)size, HORIZON_TRACY_MEMORY_CALLSTACK_DEPTH, d3d12_memory_pool_name(pool));
    }
}

static void d3d12_track_gpu_free(const void* ptr, uint32_t pool)
{
    if (ptr)
    {
        TracyCFreeNS(ptr, HORIZON_TRACY_MEMORY_CALLSTACK_DEPTH, d3d12_memory_pool_name(pool));
    }
}

static void d3d12_set_allocation_name(D3D12MA::Allocation* pAllocation, const char* pName)
{
    if (!pAllocation || !pName)
        return;

    wchar_t wName[MAX_DEBUG_NAME_LENGTH] = {};
    size_t  numConverted = 0;
    mbstowcs_s(&numConverted, wName, pName, MAX_DEBUG_NAME_LENGTH);
    pAllocation->SetName(wName);
}

static void d3d12_track_d3d12ma_alloc(D3D12MA::Allocation* pAllocation, const char* pName, uint32_t pool)
{
    if (!pAllocation)
        return;

    d3d12_set_allocation_name(pAllocation, pName);
    d3d12_track_gpu_alloc(pAllocation, pAllocation->GetSize(), pool);
}

static int64_t d3d12_plot_value(uint64_t value) { return value > (uint64_t)INT64_MAX ? INT64_MAX : (int64_t)value; }

void d3d12_plotMemoryStats(Renderer* pRenderer)
{
    if (!pRenderer || !pRenderer->dx.pResourceAllocator)
        return;

    static bool plotsConfigured = false;
    if (!plotsConfigured)
    {
        TracyCPlotConfig("GPU/D3D12 AllocationBytes", TracyPlotFormatMemory, 0, 1, 0x4E79A7);
        TracyCPlotConfig("GPU/D3D12 BlockBytes", TracyPlotFormatMemory, 0, 1, 0x59A14F);
        TracyCPlotConfig("GPU/D3D12 UnusedBytes", TracyPlotFormatMemory, 0, 1, 0xE15759);
        TracyCPlotConfig("GPU/D3D12 UnusedRanges", TracyPlotFormatNumber, 0, 1, 0xB07AA1);
        TracyCPlotConfig("GPU/D3D12 LargestUnusedRange", TracyPlotFormatMemory, 0, 1, 0xEDC948);
        TracyCPlotConfig("GPU/D3D12 Fragmentation %", TracyPlotFormatPercentage, 0, 1, 0xF28E2B);
        plotsConfigured = true;
    }

    D3D12MA::TotalStatistics stats = {};
    pRenderer->dx.pResourceAllocator->CalculateStatistics(&stats);

    const uint64_t blockBytes = stats.Total.Stats.BlockBytes;
    const uint64_t allocationBytes = stats.Total.Stats.AllocationBytes;
    const uint64_t unusedBytes = blockBytes > allocationBytes ? blockBytes - allocationBytes : 0;
    const float    fragmentationPercent = blockBytes ? ((float)unusedBytes * 100.0f) / (float)blockBytes : 0.0f;

    TracyCPlotI("GPU/D3D12 AllocationBytes", d3d12_plot_value(allocationBytes));
    TracyCPlotI("GPU/D3D12 BlockBytes", d3d12_plot_value(blockBytes));
    TracyCPlotI("GPU/D3D12 UnusedBytes", d3d12_plot_value(unusedBytes));
    TracyCPlotI("GPU/D3D12 UnusedRanges", d3d12_plot_value(stats.Total.UnusedRangeCount));
    TracyCPlotI("GPU/D3D12 LargestUnusedRange", d3d12_plot_value(stats.Total.UnusedRangeSizeMax));
    TracyCPlot("GPU/D3D12 Fragmentation %", fragmentationPercent);
}
#endif

// clang-format off
D3D12_BLEND_OP gDx12BlendOpTranslator[BlendMode::MAX_BLEND_MODES] =
{
	D3D12_BLEND_OP_ADD,
	D3D12_BLEND_OP_SUBTRACT,
	D3D12_BLEND_OP_REV_SUBTRACT,
	D3D12_BLEND_OP_MIN,
	D3D12_BLEND_OP_MAX,
};

D3D12_BLEND gDx12BlendConstantTranslator[BlendConstant::MAX_BLEND_CONSTANTS] =
{
	D3D12_BLEND_ZERO,
	D3D12_BLEND_ONE,
	D3D12_BLEND_SRC_COLOR,
	D3D12_BLEND_INV_SRC_COLOR,
	D3D12_BLEND_DEST_COLOR,
	D3D12_BLEND_INV_DEST_COLOR,
	D3D12_BLEND_SRC_ALPHA,
	D3D12_BLEND_INV_SRC_ALPHA,
	D3D12_BLEND_DEST_ALPHA,
	D3D12_BLEND_INV_DEST_ALPHA,
	D3D12_BLEND_SRC_ALPHA_SAT,
	D3D12_BLEND_BLEND_FACTOR,
	D3D12_BLEND_INV_BLEND_FACTOR,
};

D3D12_COMPARISON_FUNC gDx12ComparisonFuncTranslator[CompareMode::MAX_COMPARE_MODES] =
{
	D3D12_COMPARISON_FUNC_NEVER,
	D3D12_COMPARISON_FUNC_LESS,
	D3D12_COMPARISON_FUNC_EQUAL,
	D3D12_COMPARISON_FUNC_LESS_EQUAL,
	D3D12_COMPARISON_FUNC_GREATER,
	D3D12_COMPARISON_FUNC_NOT_EQUAL,
	D3D12_COMPARISON_FUNC_GREATER_EQUAL,
	D3D12_COMPARISON_FUNC_ALWAYS,
};

D3D12_STENCIL_OP gDx12StencilOpTranslator[StencilOp::MAX_STENCIL_OPS] =
{
	D3D12_STENCIL_OP_KEEP,
	D3D12_STENCIL_OP_ZERO,
	D3D12_STENCIL_OP_REPLACE,
	D3D12_STENCIL_OP_INVERT,
	D3D12_STENCIL_OP_INCR,
	D3D12_STENCIL_OP_DECR,
	D3D12_STENCIL_OP_INCR_SAT,
	D3D12_STENCIL_OP_DECR_SAT,
};

D3D12_CULL_MODE gDx12CullModeTranslator[MAX_CULL_MODES] =
{
	D3D12_CULL_MODE_NONE,
	D3D12_CULL_MODE_BACK,
	D3D12_CULL_MODE_FRONT,
};

D3D12_FILL_MODE gDx12FillModeTranslator[MAX_FILL_MODES] =
{
	D3D12_FILL_MODE_SOLID,
	D3D12_FILL_MODE_WIREFRAME,
};

const D3D12_COMMAND_LIST_TYPE gDx12CmdTypeTranslator[MAX_QUEUE_TYPE] =
{
	D3D12_COMMAND_LIST_TYPE_DIRECT,
	D3D12_COMMAND_LIST_TYPE_COPY,
	D3D12_COMMAND_LIST_TYPE_COMPUTE
};

const D3D12_COMMAND_QUEUE_PRIORITY gDx12QueuePriorityTranslator[QueuePriority::MAX_QUEUE_PRIORITY]
{
	D3D12_COMMAND_QUEUE_PRIORITY_NORMAL,
	D3D12_COMMAND_QUEUE_PRIORITY_HIGH,
#if !defined(XBOX)
	D3D12_COMMAND_QUEUE_PRIORITY_GLOBAL_REALTIME,
#endif
};
    // clang-format on

    // =================================================================================================
    // IMPLEMENTATION
    // =================================================================================================

#if defined(RENDERER_IMPLEMENTATION)

#if !defined(XBOX) && !defined(FORGE_D3D12_DYNAMIC_LOADING)
#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#endif

//-V:SAFE_FREE:779
#define SAFE_FREE(p_var)         \
    if ((p_var))                 \
    {                            \
        tf_free((void*)(p_var)); \
        p_var = NULL;            \
    }

#if defined(__cplusplus)
#define DECLARE_ZERO(type, var) type var = {};
#else
#define DECLARE_ZERO(type, var) type var = { 0 };
#endif

#ifndef SAFE_RELEASE
#define SAFE_RELEASE(p_var) \
    if (p_var)              \
    {                       \
        p_var->Release();   \
        p_var = NULL;       \
    }
#endif

#define CALC_SUBRESOURCE_INDEX(MipSlice, ArraySlice, PlaneSlice, MipLevels, ArraySize) \
    ((MipSlice) + ((ArraySlice) * (MipLevels)) + ((PlaneSlice) * (MipLevels) * (ArraySize)))

// Internal utility functions (may become external one day)
uint64_t                    util_dx12_determine_storage_counter_offset(uint64_t buffer_size);
DXGI_FORMAT                 util_to_dx12_uav_format(DXGI_FORMAT defaultFormat);
DXGI_FORMAT                 util_to_dx12_dsv_format(DXGI_FORMAT defaultFormat);
DXGI_FORMAT                 util_to_dx12_srv_format(DXGI_FORMAT defaultFormat);
DXGI_FORMAT                 util_to_dx12_stencil_format(DXGI_FORMAT defaultFormat);
DXGI_FORMAT                 util_to_dx12_swapchain_format(hz::Format format);
D3D12_SHADER_VISIBILITY     util_to_dx12_shader_visibility(ShaderStage stages);
D3D12_DESCRIPTOR_RANGE_TYPE util_to_dx12_descriptor_range(DescriptorType type);
D3D12_RESOURCE_STATES       util_to_dx12_resource_state(ResourceState state);
D3D12_FILTER
util_to_dx12_filter(FilterType minFilter, FilterType magFilter, MipMapMode mipMapMode, bool aniso, bool comparisonFilterEnabled);
D3D12_TEXTURE_ADDRESS_MODE    util_to_dx12_texture_address_mode(AddressMode addressMode);
D3D12_PRIMITIVE_TOPOLOGY_TYPE util_to_dx12_primitive_topology_type(PrimitiveTopology topology);
static bool                   is_directstorage_runtime_available();

//
// internal functions start with a capital letter / API starts with a small letter

// internal functions are capital first letter and capital letter of the next word

// Functions points for functions that need to be loaded
PFN_D3D12_CREATE_ROOT_SIGNATURE_DESERIALIZER           fnD3D12CreateRootSignatureDeserializer = NULL;
PFN_D3D12_SERIALIZE_VERSIONED_ROOT_SIGNATURE           fnD3D12SerializeVersionedRootSignature = NULL;
PFN_D3D12_CREATE_VERSIONED_ROOT_SIGNATURE_DESERIALIZER fnD3D12CreateVersionedRootSignatureDeserializer = NULL;
/************************************************************************/
// Descriptor Heap Defines
/************************************************************************/
struct DescriptorHeapProperties
{
    uint32_t                    maxDescriptors;
    D3D12_DESCRIPTOR_HEAP_FLAGS flags;
};

DescriptorHeapProperties gCpuDescriptorHeapProperties[D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES] = {
    { 1024 * 256, D3D12_DESCRIPTOR_HEAP_FLAG_NONE }, // CBV SRV UAV
    { 2048, D3D12_DESCRIPTOR_HEAP_FLAG_NONE },       // Sampler
    { 512, D3D12_DESCRIPTOR_HEAP_FLAG_NONE },        // RTV
    { 512, D3D12_DESCRIPTOR_HEAP_FLAG_NONE },        // DSV
};

struct NullDescriptors
{
    // Default NULL Descriptors for binding at empty descriptor slots to make sure all descriptors are bound at submit
    DxDescriptorID nullTextureSRV[TEXTURE_DIM_COUNT];
    DxDescriptorID nullTextureUAV[TEXTURE_DIM_COUNT];
    DxDescriptorID nullBufferSRV;
    DxDescriptorID nullBufferUAV;
    DxDescriptorID nullBufferCBV;
    DxDescriptorID nullSampler;
};
/************************************************************************/
// Descriptor Heap Structures
/************************************************************************/
/// CPU Visible Heap to store all the resources needing CPU read / write operations - Textures/Buffers/RTV
struct DescriptorHeap
{
    /// DX Heap
    ID3D12DescriptorHeap*       pHeap;
    /// Lock for multi-threaded descriptor allocations
    Mutex                       mutex;
    ID3D12Device*               pDevice;
    /// Start position in the heap
    D3D12_CPU_DESCRIPTOR_HANDLE startCpuHandle;
    D3D12_GPU_DESCRIPTOR_HANDLE startGpuHandle;
    // Bitmask to track free regions (set bit means occupied)
    uint32_t*                   pFlags;
    /// Description
    D3D12_DESCRIPTOR_HEAP_TYPE  type;
    uint32_t                    numDescriptors;
    /// Descriptor Increment Size
    uint32_t                    descriptorSize;
    // Usage
    uint32_t                    usedDescriptors;
};

struct DescriptorIndexMap
{
    char*    key;
    uint32_t value;
};

char* processErrorMessages(IDxcBlobEncoding* pEncoding)
{
    int32_t       percentCharactersCount = 0;
    IDxcBlobUtf8* pUtf8Blob = NULL;
    CHECK_HRESULT(pEncoding->QueryInterface(&pUtf8Blob));
    const char* bufferString = pUtf8Blob->GetStringPointer();
    for (int32_t i = 0; i < pUtf8Blob->GetStringLength(); ++i)
    {
        if (bufferString[i] == '%')
        {
            percentCharactersCount++;
        }
    }

    const size_t newSize = pUtf8Blob->GetStringLength() + percentCharactersCount + 1;

    char* errorMessages = (char*)tf_calloc(newSize, sizeof(char));
    for (int32_t i = 0, j = 0; i < newSize; ++i, ++j)
    {
        if (percentCharactersCount && bufferString[i] == '%')
        {
            errorMessages[i++] = '%';
            percentCharactersCount--;
        }

        errorMessages[i] = bufferString[j];
    }

    return errorMessages;
}

/************************************************************************/
// Static Descriptor Heap Implementation
/************************************************************************/
static void add_descriptor_heap(ID3D12Device* pDevice, const D3D12_DESCRIPTOR_HEAP_DESC* pDesc, DescriptorHeap** ppDescHeap)
{
    uint32_t numDescriptors = pDesc->NumDescriptors;
    hook_modify_descriptor_heap_size(pDesc->Type, &numDescriptors);

    // Keep 32 aligned for easy remove
    numDescriptors = round_up(numDescriptors, 32);

    const size_t sizeInBytes = (numDescriptors / 32) * sizeof(uint32_t);

    DescriptorHeap* pHeap = (DescriptorHeap*)tf_calloc(1, sizeof(*pHeap) + sizeInBytes);
    pHeap->pFlags = (uint32_t*)(pHeap + 1);
    pHeap->pDevice = pDevice;

    initMutex(&pHeap->mutex);

    D3D12_DESCRIPTOR_HEAP_DESC desc = *pDesc;
    desc.NumDescriptors = numDescriptors;

    CHECK_HRESULT(pDevice->CreateDescriptorHeap(&desc, IID_ARGS(&pHeap->pHeap)));

    pHeap->startCpuHandle = pHeap->pHeap->GetCPUDescriptorHandleForHeapStart();
    if (desc.Flags & D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE)
    {
        pHeap->startGpuHandle = pHeap->pHeap->GetGPUDescriptorHandleForHeapStart();
    }
    pHeap->numDescriptors = desc.NumDescriptors;
    pHeap->type = desc.Type;
    pHeap->descriptorSize = pDevice->GetDescriptorHandleIncrementSize(pHeap->type);

    *ppDescHeap = pHeap;
}

void reset_descriptor_heap(DescriptorHeap* pHeap)
{
    memset(pHeap->pFlags, 0, (pHeap->numDescriptors / 32) * sizeof(uint32_t));
    pHeap->usedDescriptors = 0;
}

static void remove_descriptor_heap(DescriptorHeap* pHeap)
{
    SAFE_RELEASE(pHeap->pHeap);
    destroyMutex(&pHeap->mutex);
    SAFE_FREE(pHeap);
}

void return_descriptor_handles_unlocked(DescriptorHeap* pHeap, DxDescriptorID handle, uint32_t count)
{
    if (D3D12_DESCRIPTOR_ID_NONE == handle || !count)
    {
        return;
    }

    for (uint32_t id = handle; id < handle + count; ++id)
    {
        const uint32_t i = id / 32;
        const uint32_t mask = ~(1 << (id % 32));
        pHeap->pFlags[i] &= mask;
    }

    pHeap->usedDescriptors -= count;
}

void return_descriptor_handles(DescriptorHeap* pHeap, DxDescriptorID handle, uint32_t count)
{
    MutexLock lock(pHeap->mutex);
    return_descriptor_handles_unlocked(pHeap, handle, count);
}

static DxDescriptorID consume_descriptor_handles(DescriptorHeap* pHeap, uint32_t descriptorCount)
{
    if (!descriptorCount)
    {
        return D3D12_DESCRIPTOR_ID_NONE;
    }

    MutexLock lock(pHeap->mutex);

    DxDescriptorID result = D3D12_DESCRIPTOR_ID_NONE;
    DxDescriptorID firstResult = D3D12_DESCRIPTOR_ID_NONE;
    uint32_t       foundCount = 0;

    for (uint32_t i = 0; i < pHeap->numDescriptors / 32; ++i)
    {
        const uint32_t flag = pHeap->pFlags[i];
        if (UINT32_MAX == flag)
        {
            return_descriptor_handles_unlocked(pHeap, firstResult, foundCount);
            foundCount = 0;
            result = D3D12_DESCRIPTOR_ID_NONE;
            firstResult = D3D12_DESCRIPTOR_ID_NONE;
            continue;
        }

        for (int32_t j = 0, mask = 1; j < 32; ++j, mask <<= 1)
        {
            if (!(flag & mask))
            {
                pHeap->pFlags[i] |= mask;
                result = i * 32 + j;

                ASSERT(result != D3D12_DESCRIPTOR_ID_NONE && "Out of descriptors");

                if (D3D12_DESCRIPTOR_ID_NONE == firstResult)
                {
                    firstResult = result;
                }

                ++foundCount;
                ++pHeap->usedDescriptors;

                if (foundCount == descriptorCount)
                {
                    return firstResult;
                }
            }
            // Non contiguous. Start scanning again from this point
            else if (foundCount)
            {
                return_descriptor_handles_unlocked(pHeap, firstResult, foundCount);
                foundCount = 0;
                result = D3D12_DESCRIPTOR_ID_NONE;
                firstResult = D3D12_DESCRIPTOR_ID_NONE;
            }
        }
    }

    ASSERT(result != D3D12_DESCRIPTOR_ID_NONE && "Out of descriptors");
    return firstResult;
}

static inline FORGE_CONSTEXPR D3D12_CPU_DESCRIPTOR_HANDLE descriptor_id_to_cpu_handle(DescriptorHeap* pHeap, DxDescriptorID id)
{
    return { pHeap->startCpuHandle.ptr + id * pHeap->descriptorSize };
}

static inline FORGE_CONSTEXPR D3D12_GPU_DESCRIPTOR_HANDLE descriptor_id_to_gpu_handle(DescriptorHeap* pHeap, DxDescriptorID id)
{
    return { pHeap->startGpuHandle.ptr + id * pHeap->descriptorSize };
}

static void copy_descriptor_handle(DescriptorHeap* pSrcHeap, DxDescriptorID srcId, DescriptorHeap* pDstHeap, DxDescriptorID dstId)
{
    ASSERT(pSrcHeap->type == pDstHeap->type);
    D3D12_CPU_DESCRIPTOR_HANDLE srcHandle = descriptor_id_to_cpu_handle(pSrcHeap, srcId);
    D3D12_CPU_DESCRIPTOR_HANDLE dstHandle = descriptor_id_to_cpu_handle(pDstHeap, dstId);
    pSrcHeap->pDevice->CopyDescriptorsSimple(1, dstHandle, srcHandle, pSrcHeap->type);
}
constexpr D3D12_DEPTH_STENCIL_DESC util_to_depth_desc(const DepthStateDesc* pDesc)
{
    ASSERT(pDesc->depthFunc < CompareMode::MAX_COMPARE_MODES);
    ASSERT(pDesc->stencilFrontFunc < CompareMode::MAX_COMPARE_MODES);
    ASSERT(pDesc->stencilFrontFail < StencilOp::MAX_STENCIL_OPS);
    ASSERT(pDesc->depthFrontFail < StencilOp::MAX_STENCIL_OPS);
    ASSERT(pDesc->stencilFrontPass < StencilOp::MAX_STENCIL_OPS);
    ASSERT(pDesc->stencilBackFunc < CompareMode::MAX_COMPARE_MODES);
    ASSERT(pDesc->stencilBackFail < StencilOp::MAX_STENCIL_OPS);
    ASSERT(pDesc->depthBackFail < StencilOp::MAX_STENCIL_OPS);
    ASSERT(pDesc->stencilBackPass < StencilOp::MAX_STENCIL_OPS);

    D3D12_DEPTH_STENCIL_DESC ret = {};
    ret.DepthEnable = (BOOL)pDesc->depthTest;
    ret.DepthWriteMask = pDesc->depthWrite ? D3D12_DEPTH_WRITE_MASK_ALL : D3D12_DEPTH_WRITE_MASK_ZERO;
    ret.DepthFunc = gDx12ComparisonFuncTranslator[pDesc->depthFunc];
    ret.StencilEnable = (BOOL)pDesc->stencilTest;
    ret.StencilReadMask = pDesc->stencilReadMask;
    ret.StencilWriteMask = pDesc->stencilWriteMask;
    ret.BackFace.StencilFunc = gDx12ComparisonFuncTranslator[pDesc->stencilBackFunc];
    ret.FrontFace.StencilFunc = gDx12ComparisonFuncTranslator[pDesc->stencilFrontFunc];
    ret.BackFace.StencilDepthFailOp = gDx12StencilOpTranslator[pDesc->depthBackFail];
    ret.FrontFace.StencilDepthFailOp = gDx12StencilOpTranslator[pDesc->depthFrontFail];
    ret.BackFace.StencilFailOp = gDx12StencilOpTranslator[pDesc->stencilBackFail];
    ret.FrontFace.StencilFailOp = gDx12StencilOpTranslator[pDesc->stencilFrontFail];
    ret.BackFace.StencilPassOp = gDx12StencilOpTranslator[pDesc->stencilBackPass];
    ret.FrontFace.StencilPassOp = gDx12StencilOpTranslator[pDesc->stencilFrontPass];

    return ret;
}

static inline FORGE_CONSTEXPR uint8_t ToColorWriteMask(ColorMask mask)
{
    uint8_t ret = 0;
    if (mask & COLOR_MASK_RED)
    {
        ret |= D3D12_COLOR_WRITE_ENABLE_RED;
    }
    if (mask & COLOR_MASK_GREEN)
    {
        ret |= D3D12_COLOR_WRITE_ENABLE_GREEN;
    }
    if (mask & COLOR_MASK_BLUE)
    {
        ret |= D3D12_COLOR_WRITE_ENABLE_BLUE;
    }
    if (mask & COLOR_MASK_ALPHA)
    {
        ret |= D3D12_COLOR_WRITE_ENABLE_ALPHA;
    }

    return ret;
}

constexpr D3D12_BLEND_DESC util_to_blend_desc(const BlendStateDesc* pDesc)
{
    int blendDescIndex = 0;
#if defined(ENABLE_GRAPHICS_DEBUG)

    for (int i = 0; i < MAX_RENDER_TARGET_ATTACHMENTS; ++i)
    {
        if (pDesc->renderTargetMask & (1 << i))
        {
            ASSERT(pDesc->srcFactors[blendDescIndex] < BlendConstant::MAX_BLEND_CONSTANTS);
            ASSERT(pDesc->dstFactors[blendDescIndex] < BlendConstant::MAX_BLEND_CONSTANTS);
            ASSERT(pDesc->srcAlphaFactors[blendDescIndex] < BlendConstant::MAX_BLEND_CONSTANTS);
            ASSERT(pDesc->dstAlphaFactors[blendDescIndex] < BlendConstant::MAX_BLEND_CONSTANTS);
            ASSERT(pDesc->blendModes[blendDescIndex] < BlendMode::MAX_BLEND_MODES);
            ASSERT(pDesc->blendAlphaModes[blendDescIndex] < BlendMode::MAX_BLEND_MODES);
        }

        if (pDesc->independentBlend)
            ++blendDescIndex;
    }

    blendDescIndex = 0;
#endif

    D3D12_BLEND_DESC ret = {};
    ret.AlphaToCoverageEnable = (BOOL)pDesc->alphaToCoverage;
    ret.IndependentBlendEnable = TRUE;
    for (int i = 0; i < MAX_RENDER_TARGET_ATTACHMENTS; i++)
    {
        if (pDesc->renderTargetMask & (1 << i))
        {
            BOOL blendEnable = (gDx12BlendConstantTranslator[pDesc->srcFactors[blendDescIndex]] != D3D12_BLEND_ONE ||
                                gDx12BlendConstantTranslator[pDesc->dstFactors[blendDescIndex]] != D3D12_BLEND_ZERO ||
                                gDx12BlendConstantTranslator[pDesc->srcAlphaFactors[blendDescIndex]] != D3D12_BLEND_ONE ||
                                gDx12BlendConstantTranslator[pDesc->dstAlphaFactors[blendDescIndex]] != D3D12_BLEND_ZERO);

            ret.RenderTarget[i].BlendEnable = blendEnable;
            ret.RenderTarget[i].RenderTargetWriteMask = ToColorWriteMask(pDesc->colorWriteMasks[blendDescIndex]);
            ret.RenderTarget[i].BlendOp = gDx12BlendOpTranslator[pDesc->blendModes[blendDescIndex]];
            ret.RenderTarget[i].SrcBlend = gDx12BlendConstantTranslator[pDesc->srcFactors[blendDescIndex]];
            ret.RenderTarget[i].DestBlend = gDx12BlendConstantTranslator[pDesc->dstFactors[blendDescIndex]];
            ret.RenderTarget[i].BlendOpAlpha = gDx12BlendOpTranslator[pDesc->blendAlphaModes[blendDescIndex]];
            ret.RenderTarget[i].SrcBlendAlpha = gDx12BlendConstantTranslator[pDesc->srcAlphaFactors[blendDescIndex]];
            ret.RenderTarget[i].DestBlendAlpha = gDx12BlendConstantTranslator[pDesc->dstAlphaFactors[blendDescIndex]];
        }

        if (pDesc->independentBlend)
            ++blendDescIndex;
    }

    return ret;
}

constexpr D3D12_RASTERIZER_DESC util_to_rasterizer_desc(const RasterizerStateDesc* pDesc)
{
    ASSERT(pDesc->fillMode < FillMode::MAX_FILL_MODES);
    ASSERT(pDesc->cullMode < CullMode::MAX_CULL_MODES);
    ASSERT(pDesc->frontFace == FRONT_FACE_CCW || pDesc->frontFace == FRONT_FACE_CW);

    D3D12_RASTERIZER_DESC ret = {};
    ret.FillMode = gDx12FillModeTranslator[pDesc->fillMode];
    ret.CullMode = gDx12CullModeTranslator[pDesc->cullMode];
    ret.FrontCounterClockwise = pDesc->frontFace == FRONT_FACE_CCW;
    ret.DepthBias = pDesc->depthBias;
    ret.DepthBiasClamp = 0.0f;
    ret.SlopeScaledDepthBias = pDesc->slopeScaledDepthBias;
    ret.DepthClipEnable = !pDesc->depthClampEnable;
    ret.MultisampleEnable = pDesc->multiSample ? TRUE : FALSE;
    ret.AntialiasedLineEnable = FALSE;
    ret.ForcedSampleCount = 0;
    ret.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

    return ret;
}
/************************************************************************/
/************************************************************************/

const DescriptorInfo* d3d12_get_descriptor(const RootSignature* pRootSignature, const char* pResName)
{
    const DescriptorIndexMap* pNode = shgetp_null(pRootSignature->pDescriptorNameToIndexMap, pResName);

    if (pNode)
    {
        return &pRootSignature->pDescriptors[pNode->value];
    }
    else
    {
        LOGF(LogLevel::eERROR, "Invalid descriptor param (%s)", pResName);
        return NULL;
    }
}
/************************************************************************/
// Globals
/************************************************************************/
static const uint32_t gDescriptorTableDWORDS = 1;
static const uint32_t gRootDescriptorDWORDS = 2;
static const uint32_t gMaxRootConstantsPerRootParam = 4U;
/************************************************************************/
// Logging functions
/************************************************************************/
// Proxy log callback
void                  internal_log(LogLevel level, const char* msg, const char* component) { LOGF(level, "%s ( %s )", component, msg); }

void AddSrv(Renderer* pRenderer, DescriptorHeap* pOptionalHeap, ID3D12Resource* pResource, const D3D12_SHADER_RESOURCE_VIEW_DESC* pSrvDesc,
            DxDescriptorID* pInOutId)
{
    DescriptorHeap* heap = pOptionalHeap ? pOptionalHeap : pRenderer->dx.pCPUDescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV];
    if (D3D12_DESCRIPTOR_ID_NONE == *pInOutId)
    {
        *pInOutId = consume_descriptor_handles(heap, 1);
    }
    pRenderer->dx.pDevice->CreateShaderResourceView(pResource, pSrvDesc, descriptor_id_to_cpu_handle(heap, *pInOutId));
}

static void AddBufferSrv(Renderer* pRenderer, DescriptorHeap* pOptionalHeap, ID3D12Resource* pBuffer, bool raw, uint32_t firstElement,
                         uint32_t elementCount, uint32_t stride, DxDescriptorID* pOutSrv)
{
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Buffer.FirstElement = firstElement;
    srvDesc.Buffer.NumElements = elementCount;
    srvDesc.Buffer.StructureByteStride = stride;
    srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
    srvDesc.Format = DXGI_FORMAT_UNKNOWN;
    if (raw)
    {
        srvDesc.Format = DXGI_FORMAT_R32_TYPELESS;
        srvDesc.Buffer.StructureByteStride = 0;
        srvDesc.Buffer.Flags |= D3D12_BUFFER_SRV_FLAG_RAW;
    }

    AddSrv(pRenderer, pOptionalHeap, pBuffer, &srvDesc, pOutSrv);
}

static void AddTypedBufferSrv(Renderer* pRenderer, DescriptorHeap* pOptionalHeap, ID3D12Resource* pBuffer, uint32_t firstElement,
                              uint32_t elementCount, hz::Format format, DxDescriptorID* pOutSrv)
{
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Buffer.FirstElement = firstElement;
    srvDesc.Buffer.NumElements = elementCount;
    srvDesc.Buffer.StructureByteStride = 0;
    srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
    srvDesc.Format = (DXGI_FORMAT)TinyImageFormat_ToDXGI_FORMAT((TinyImageFormat)format);
    srvDesc.Buffer.StructureByteStride = 0;

    AddSrv(pRenderer, pOptionalHeap, pBuffer, &srvDesc, pOutSrv);
}

static void AddUav(Renderer* pRenderer, DescriptorHeap* pOptionalHeap, ID3D12Resource* pResource, ID3D12Resource* pCounterResource,
                   const D3D12_UNORDERED_ACCESS_VIEW_DESC* pUavDesc, DxDescriptorID* pInOutId)
{
    DescriptorHeap* heap = pOptionalHeap ? pOptionalHeap : pRenderer->dx.pCPUDescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV];
    if (D3D12_DESCRIPTOR_ID_NONE == *pInOutId)
    {
        *pInOutId = consume_descriptor_handles(heap, 1);
    }
    pRenderer->dx.pDevice->CreateUnorderedAccessView(pResource, pCounterResource, pUavDesc, descriptor_id_to_cpu_handle(heap, *pInOutId));
}

static void AddBufferUav(Renderer* pRenderer, DescriptorHeap* pOptionalHeap, ID3D12Resource* pBuffer, ID3D12Resource* pCounterBuffer,
                         uint32_t counterOffset, bool raw, uint32_t firstElement, uint32_t elementCount, uint32_t stride,
                         DxDescriptorID* pOutUav)
{
    D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
    uavDesc.Format = DXGI_FORMAT_UNKNOWN;
    uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
    uavDesc.Buffer.FirstElement = firstElement;
    uavDesc.Buffer.NumElements = elementCount;
    uavDesc.Buffer.StructureByteStride = stride;
    uavDesc.Buffer.CounterOffsetInBytes = counterOffset;
    uavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;
    if (raw)
    {
        uavDesc.Format = DXGI_FORMAT_R32_TYPELESS;
        uavDesc.Buffer.StructureByteStride = 0;
        uavDesc.Buffer.Flags |= D3D12_BUFFER_UAV_FLAG_RAW;
    }

    AddUav(pRenderer, pOptionalHeap, pBuffer, pCounterBuffer, &uavDesc, pOutUav);
}

static void AddTypedBufferUav(Renderer* pRenderer, DescriptorHeap* pOptionalHeap, ID3D12Resource* pBuffer, uint32_t firstElement,
                              uint32_t elementCount, hz::Format format, DxDescriptorID* pOutUav)
{
    D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
    uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
    uavDesc.Buffer.FirstElement = firstElement;
    uavDesc.Buffer.NumElements = elementCount;
    uavDesc.Buffer.StructureByteStride = 0;
    uavDesc.Buffer.CounterOffsetInBytes = 0;
    uavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;
    uavDesc.Format = (DXGI_FORMAT)TinyImageFormat_ToDXGI_FORMAT((TinyImageFormat)format);
    D3D12_FEATURE_DATA_FORMAT_SUPPORT FormatSupport = { uavDesc.Format, D3D12_FORMAT_SUPPORT1_NONE, D3D12_FORMAT_SUPPORT2_NONE };
    HRESULT hr = pRenderer->dx.pDevice->CheckFeatureSupport(D3D12_FEATURE_FORMAT_SUPPORT, &FormatSupport, sizeof(FormatSupport));
    if (!SUCCEEDED(hr) || !(FormatSupport.Support2 & D3D12_FORMAT_SUPPORT2_UAV_TYPED_LOAD) ||
        !(FormatSupport.Support2 & D3D12_FORMAT_SUPPORT2_UAV_TYPED_STORE))
    {
        // Format does not support UAV Typed Load
        LOGF(LogLevel::eWARNING, "Cannot use Typed UAV for buffer format %u", (uint32_t)format);
        uavDesc.Format = DXGI_FORMAT_UNKNOWN;
    }

    AddUav(pRenderer, pOptionalHeap, pBuffer, NULL, &uavDesc, pOutUav);
}

static void AddCbv(Renderer* pRenderer, DescriptorHeap* pOptionalHeap, const D3D12_CONSTANT_BUFFER_VIEW_DESC* pCbvDesc,
                   DxDescriptorID* pInOutId)
{
    DescriptorHeap* heap = pOptionalHeap ? pOptionalHeap : pRenderer->dx.pCPUDescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV];
    if (D3D12_DESCRIPTOR_ID_NONE == *pInOutId)
    {
        *pInOutId = consume_descriptor_handles(heap, 1);
    }
    pRenderer->dx.pDevice->CreateConstantBufferView(pCbvDesc, descriptor_id_to_cpu_handle(heap, *pInOutId));
}

static void AddRtv(Renderer* pRenderer, DescriptorHeap* pOptionalHeap, ID3D12Resource* pResource, DXGI_FORMAT format, uint32_t mipSlice,
                   uint32_t arraySlice, DxDescriptorID* pInOutId)
{
    DescriptorHeap* heap = pOptionalHeap ? pOptionalHeap : pRenderer->dx.pCPUDescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_RTV];
    if (D3D12_DESCRIPTOR_ID_NONE == *pInOutId)
    {
        *pInOutId = consume_descriptor_handles(heap, 1);
    }
    D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
    D3D12_RESOURCE_DESC           desc = pResource->GetDesc();
    D3D12_RESOURCE_DIMENSION      type = desc.Dimension;

    rtvDesc.Format = format;

    switch (type)
    {
    case D3D12_RESOURCE_DIMENSION_BUFFER:
        break;
    case D3D12_RESOURCE_DIMENSION_TEXTURE1D:
        if (desc.DepthOrArraySize > 1)
        {
            rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE1DARRAY;
            rtvDesc.Texture1DArray.MipSlice = mipSlice;
            if (arraySlice != -1)
            {
                rtvDesc.Texture1DArray.ArraySize = 1;
                rtvDesc.Texture1DArray.FirstArraySlice = arraySlice;
            }
            else
            {
                rtvDesc.Texture1DArray.ArraySize = desc.DepthOrArraySize;
            }
        }
        else
        {
            rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE1D;
            rtvDesc.Texture1D.MipSlice = mipSlice;
        }
        break;
    case D3D12_RESOURCE_DIMENSION_TEXTURE2D:
        if (desc.SampleDesc.Count > 1)
        {
            if (desc.DepthOrArraySize > 1)
            {
                rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DMSARRAY;
                if (arraySlice != -1)
                {
                    rtvDesc.Texture2DMSArray.ArraySize = 1;
                    rtvDesc.Texture2DMSArray.FirstArraySlice = arraySlice;
                }
                else
                {
                    rtvDesc.Texture2DMSArray.ArraySize = desc.DepthOrArraySize;
                }
            }
            else
            {
                rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DMS;
            }
        }
        else
        {
            if (desc.DepthOrArraySize > 1)
            {
                rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DARRAY;
                rtvDesc.Texture2DArray.MipSlice = mipSlice;
                if (arraySlice != -1)
                {
                    rtvDesc.Texture2DArray.ArraySize = 1;
                    rtvDesc.Texture2DArray.FirstArraySlice = arraySlice;
                }
                else
                {
                    rtvDesc.Texture2DArray.ArraySize = desc.DepthOrArraySize;
                }
            }
            else
            {
                rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
                rtvDesc.Texture2D.MipSlice = mipSlice;
            }
        }
        break;
    case D3D12_RESOURCE_DIMENSION_TEXTURE3D:
        rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE3D;
        rtvDesc.Texture3D.MipSlice = mipSlice;
        if (arraySlice != -1)
        {
            rtvDesc.Texture3D.WSize = 1;
            rtvDesc.Texture3D.FirstWSlice = arraySlice;
        }
        else
        {
            rtvDesc.Texture3D.WSize = desc.DepthOrArraySize;
        }
        break;
    default:
        break;
    }

    pRenderer->dx.pDevice->CreateRenderTargetView(pResource, &rtvDesc, descriptor_id_to_cpu_handle(heap, *pInOutId));
}

static void AddDsv(Renderer* pRenderer, DescriptorHeap* pOptionalHeap, ID3D12Resource* pResource, DXGI_FORMAT format, uint32_t mipSlice,
                   uint32_t arraySlice, DxDescriptorID* pInOutId)
{
    DescriptorHeap* heap = pOptionalHeap ? pOptionalHeap : pRenderer->dx.pCPUDescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_DSV];
    if (D3D12_DESCRIPTOR_ID_NONE == *pInOutId)
    {
        *pInOutId = consume_descriptor_handles(heap, 1);
    }

    D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
    D3D12_RESOURCE_DESC           desc = pResource->GetDesc();
    D3D12_RESOURCE_DIMENSION      type = desc.Dimension;

    dsvDesc.Format = format;

    switch (type)
    {
    case D3D12_RESOURCE_DIMENSION_BUFFER:
        break;
    case D3D12_RESOURCE_DIMENSION_TEXTURE1D:
        if (desc.DepthOrArraySize > 1)
        {
            dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE1DARRAY;
            dsvDesc.Texture1DArray.MipSlice = mipSlice;
            if (arraySlice != -1)
            {
                dsvDesc.Texture1DArray.ArraySize = 1;
                dsvDesc.Texture1DArray.FirstArraySlice = arraySlice;
            }
            else
            {
                dsvDesc.Texture1DArray.ArraySize = desc.DepthOrArraySize;
            }
        }
        else
        {
            dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE1D;
            dsvDesc.Texture1D.MipSlice = mipSlice;
        }
        break;
    case D3D12_RESOURCE_DIMENSION_TEXTURE2D:
        if (desc.SampleDesc.Count > 1)
        {
            if (desc.DepthOrArraySize > 1)
            {
                dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DMSARRAY;
                if (arraySlice != -1)
                {
                    dsvDesc.Texture2DMSArray.ArraySize = 1;
                    dsvDesc.Texture2DMSArray.FirstArraySlice = arraySlice;
                }
                else
                {
                    dsvDesc.Texture2DMSArray.ArraySize = desc.DepthOrArraySize;
                }
            }
            else
            {
                dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DMS;
            }
        }
        else
        {
            if (desc.DepthOrArraySize > 1)
            {
                dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DARRAY;
                dsvDesc.Texture2DArray.MipSlice = mipSlice;
                if (arraySlice != -1)
                {
                    dsvDesc.Texture2DArray.ArraySize = 1;
                    dsvDesc.Texture2DArray.FirstArraySlice = arraySlice;
                }
                else
                {
                    dsvDesc.Texture2DArray.ArraySize = desc.DepthOrArraySize;
                }
            }
            else
            {
                dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
                dsvDesc.Texture2D.MipSlice = mipSlice;
            }
        }
        break;
    case D3D12_RESOURCE_DIMENSION_TEXTURE3D:
        ASSERT(false && "Cannot create 3D Depth Stencil");
        break;
    default:
        break;
    }

    pRenderer->dx.pDevice->CreateDepthStencilView(pResource, &dsvDesc, descriptor_id_to_cpu_handle(heap, *pInOutId));
}

static void AddSampler(Renderer* pRenderer, DescriptorHeap* pOptionalHeap, const D3D12_SAMPLER_DESC* pSamplerDesc, DxDescriptorID* pInOutId)
{
    DescriptorHeap* heap = pOptionalHeap ? pOptionalHeap : pRenderer->dx.pCPUDescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER];
    if (D3D12_DESCRIPTOR_ID_NONE == *pInOutId)
    {
        *pInOutId = consume_descriptor_handles(heap, 1);
    }
    pRenderer->dx.pDevice->CreateSampler(pSamplerDesc, descriptor_id_to_cpu_handle(heap, *pInOutId));
}

D3D12_DEPTH_STENCIL_DESC gDefaultDepthDesc = {};
D3D12_BLEND_DESC         gDefaultBlendDesc = {};
D3D12_RASTERIZER_DESC    gDefaultRasterizerDesc = {};

static void add_default_resources(Renderer* pRenderer)
{
    pRenderer->pNullDescriptors = (NullDescriptors*)tf_calloc(1, sizeof(NullDescriptors));
    for (uint32_t i = 0; i < TEXTURE_DIM_COUNT; ++i)
    {
        pRenderer->pNullDescriptors->nullTextureSRV[i] = D3D12_DESCRIPTOR_ID_NONE;
        pRenderer->pNullDescriptors->nullTextureUAV[i] = D3D12_DESCRIPTOR_ID_NONE;
    }
    pRenderer->pNullDescriptors->nullBufferSRV = D3D12_DESCRIPTOR_ID_NONE;
    pRenderer->pNullDescriptors->nullBufferUAV = D3D12_DESCRIPTOR_ID_NONE;
    pRenderer->pNullDescriptors->nullBufferCBV = D3D12_DESCRIPTOR_ID_NONE;
    pRenderer->pNullDescriptors->nullSampler = D3D12_DESCRIPTOR_ID_NONE;

    // Create NULL descriptors in case user does not specify some descriptors we can bind null descriptor handles at those points
    D3D12_SAMPLER_DESC samplerDesc = {};
    samplerDesc.AddressU = samplerDesc.AddressV = samplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
    AddSampler(pRenderer, NULL, &samplerDesc, &pRenderer->pNullDescriptors->nullSampler);

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = DXGI_FORMAT_R8_UINT;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

    D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
    uavDesc.Format = DXGI_FORMAT_R8_UINT;

    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE1D;
    uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE1D;
    AddSrv(pRenderer, NULL, NULL, &srvDesc, &pRenderer->pNullDescriptors->nullTextureSRV[TEXTURE_DIM_1D]);
    AddUav(pRenderer, NULL, NULL, NULL, &uavDesc, &pRenderer->pNullDescriptors->nullTextureUAV[TEXTURE_DIM_1D]);
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
    AddSrv(pRenderer, NULL, NULL, &srvDesc, &pRenderer->pNullDescriptors->nullTextureSRV[TEXTURE_DIM_2D]);
    AddUav(pRenderer, NULL, NULL, NULL, &uavDesc, &pRenderer->pNullDescriptors->nullTextureUAV[TEXTURE_DIM_2D]);
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DMS;
    AddSrv(pRenderer, NULL, NULL, &srvDesc, &pRenderer->pNullDescriptors->nullTextureSRV[TEXTURE_DIM_2DMS]);
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE3D;
    uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE3D;
    AddSrv(pRenderer, NULL, NULL, &srvDesc, &pRenderer->pNullDescriptors->nullTextureSRV[TEXTURE_DIM_3D]);
    AddUav(pRenderer, NULL, NULL, NULL, &uavDesc, &pRenderer->pNullDescriptors->nullTextureUAV[TEXTURE_DIM_3D]);
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE1DARRAY;
    uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE1DARRAY;
    AddSrv(pRenderer, NULL, NULL, &srvDesc, &pRenderer->pNullDescriptors->nullTextureSRV[TEXTURE_DIM_1D_ARRAY]);
    AddUav(pRenderer, NULL, NULL, NULL, &uavDesc, &pRenderer->pNullDescriptors->nullTextureUAV[TEXTURE_DIM_1D_ARRAY]);
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
    uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2DARRAY;
    AddSrv(pRenderer, NULL, NULL, &srvDesc, &pRenderer->pNullDescriptors->nullTextureSRV[TEXTURE_DIM_2D_ARRAY]);
    AddUav(pRenderer, NULL, NULL, NULL, &uavDesc, &pRenderer->pNullDescriptors->nullTextureUAV[TEXTURE_DIM_2D_ARRAY]);
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DMSARRAY;
    AddSrv(pRenderer, NULL, NULL, &srvDesc, &pRenderer->pNullDescriptors->nullTextureSRV[TEXTURE_DIM_2DMS_ARRAY]);
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
    AddSrv(pRenderer, NULL, NULL, &srvDesc, &pRenderer->pNullDescriptors->nullTextureSRV[TEXTURE_DIM_CUBE]);
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBEARRAY;
    AddSrv(pRenderer, NULL, NULL, &srvDesc, &pRenderer->pNullDescriptors->nullTextureSRV[TEXTURE_DIM_CUBE_ARRAY]);
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
    uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
    AddSrv(pRenderer, NULL, NULL, &srvDesc, &pRenderer->pNullDescriptors->nullBufferSRV);
    AddUav(pRenderer, NULL, NULL, NULL, &uavDesc, &pRenderer->pNullDescriptors->nullBufferUAV);
    AddCbv(pRenderer, NULL, NULL, &pRenderer->pNullDescriptors->nullBufferCBV);

    BlendStateDesc blendStateDesc = {};
    blendStateDesc.dstAlphaFactors[0] = BC_ZERO;
    blendStateDesc.dstFactors[0] = BC_ZERO;
    blendStateDesc.srcAlphaFactors[0] = BC_ONE;
    blendStateDesc.srcFactors[0] = BC_ONE;
    blendStateDesc.colorWriteMasks[0] = COLOR_MASK_ALL;
    blendStateDesc.renderTargetMask = BLEND_STATE_TARGET_ALL;
    blendStateDesc.independentBlend = false;
    gDefaultBlendDesc = util_to_blend_desc(&blendStateDesc);

    DepthStateDesc depthStateDesc = {};
    depthStateDesc.depthFunc = CMP_LEQUAL;
    depthStateDesc.depthTest = false;
    depthStateDesc.depthWrite = false;
    depthStateDesc.stencilBackFunc = CMP_ALWAYS;
    depthStateDesc.stencilFrontFunc = CMP_ALWAYS;
    depthStateDesc.stencilReadMask = 0xFF;
    depthStateDesc.stencilWriteMask = 0xFF;
    gDefaultDepthDesc = util_to_depth_desc(&depthStateDesc);

    RasterizerStateDesc rasterizerStateDesc = {};
    rasterizerStateDesc.cullMode = CULL_MODE_BACK;
    gDefaultRasterizerDesc = util_to_rasterizer_desc(&rasterizerStateDesc);
}

static void remove_default_resources(Renderer* pRenderer)
{
    return_descriptor_handles(pRenderer->dx.pCPUDescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER],
                              pRenderer->pNullDescriptors->nullSampler, 1);

    DescriptorHeap* heap = pRenderer->dx.pCPUDescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV];
    for (uint32_t i = 0; i < TEXTURE_DIM_COUNT; ++i)
    {
        return_descriptor_handles(heap, pRenderer->pNullDescriptors->nullTextureSRV[i], 1);
        return_descriptor_handles(heap, pRenderer->pNullDescriptors->nullTextureUAV[i], 1);
    }
    return_descriptor_handles(heap, pRenderer->pNullDescriptors->nullBufferSRV, 1);
    return_descriptor_handles(heap, pRenderer->pNullDescriptors->nullBufferUAV, 1);
    return_descriptor_handles(heap, pRenderer->pNullDescriptors->nullBufferCBV, 1);

    SAFE_FREE(pRenderer->pNullDescriptors);
}

/************************************************************************/
// Internal Root Signature Functions
/************************************************************************/
struct RootParameter
{
    ShaderResource  shaderResource;
    DescriptorInfo* pDescriptorInfo;
};

// For sort
// sort table by type (CBV/SRV/UAV) by register by space
static bool lessRootParameter(const RootParameter* pLhs, const RootParameter* pRhs)
{
    // swap operands to achieve descending order
    int results[3] = {
        (int)((int64_t)pRhs->pDescriptorInfo->type - (int64_t)pLhs->pDescriptorInfo->type),
        (int)((int64_t)pRhs->shaderResource.set - (int64_t)pLhs->shaderResource.set),
        (int)((int64_t)pRhs->shaderResource.reg - (int64_t)pLhs->shaderResource.reg),
    };

    for (int i = 0; i < 3; ++i)
    {
        if (results[i])
            return results[i] < 0;
    }
    return false;
}

DEFINE_SORT_ALGORITHMS_FOR_TYPE(static, RootParameter, lessRootParameter)

#undef CREATE_TEMP_ROOT_PARAM
#undef DESTROY_TEMP_ROOT_PARAM
#undef COPY_ROOT_PARAM

struct DescriptorInfoIndexNode
{
    DescriptorInfo* key;
    uint32_t        value;
};
struct UpdateFrequencyLayoutInfo
{
    // stb_ds array
    RootParameter*           cbvSrvUavTable;
    // stb_ds array
    RootParameter*           samplerTable;
    // stb_ds array
    RootParameter*           rootDescriptorParams;
    // stb_ds array
    RootParameter*           rootConstants;
    // stb_ds hash map
    DescriptorInfoIndexNode* descriptorIndexMap;
};

/// Calculates the total size of the root signature (in DWORDS) from the input layouts
uint32_t calculate_root_signature_size(UpdateFrequencyLayoutInfo* pLayouts, uint32_t numLayouts)
{
    uint32_t size = 0;
    for (uint32_t i = 0; i < numLayouts; ++i)
    {
        if (arrlen(pLayouts[i].cbvSrvUavTable))
            size += gDescriptorTableDWORDS;
        if (arrlen(pLayouts[i].samplerTable))
            size += gDescriptorTableDWORDS;

        for (ptrdiff_t c = 0; c < arrlen(pLayouts[i].rootDescriptorParams); ++c)
        {
            size += gRootDescriptorDWORDS;
        }
        for (ptrdiff_t c = 0; c < arrlen(pLayouts[i].rootConstants); ++c)
        {
            DescriptorInfo* pDesc = pLayouts[i].rootConstants[c].pDescriptorInfo;
            size += pDesc->size;
        }
    }

    return size;
}

/// Creates a root descriptor table parameter from the input table layout for root signature version 1_1
void create_descriptor_table(uint32_t numDescriptors, RootParameter* tableRef, D3D12_DESCRIPTOR_RANGE1* pRange,
                             D3D12_ROOT_PARAMETER1* pRootParam)
{
    pRootParam->ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    ShaderStage stageCount = SHADER_STAGE_NONE;
    for (uint32_t i = 0; i < numDescriptors; ++i)
    {
        const ShaderResource* res = &tableRef[i].shaderResource;
        const DescriptorInfo* desc = tableRef[i].pDescriptorInfo;
        pRange[i].BaseShaderRegister = res->reg;
        pRange[i].RegisterSpace = res->set;
        pRange[i].Flags = D3D12_DESCRIPTOR_RANGE_FLAG_NONE;
        pRange[i].NumDescriptors = desc->size;
        pRange[i].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
        pRange[i].RangeType = util_to_dx12_descriptor_range((DescriptorType)desc->type);
        stageCount |= res->used_stages;
    }
    pRootParam->ShaderVisibility = util_to_dx12_shader_visibility(stageCount);
    pRootParam->DescriptorTable.NumDescriptorRanges = numDescriptors;
    pRootParam->DescriptorTable.pDescriptorRanges = pRange;
}

/// Creates a root descriptor / root constant parameter for root signature version 1_1
void create_root_descriptor(const RootParameter* pDesc, D3D12_ROOT_PARAMETER1* pRootParam)
{
    pRootParam->ShaderVisibility = util_to_dx12_shader_visibility(pDesc->shaderResource.used_stages);
    pRootParam->ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    pRootParam->Descriptor.Flags = D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC;
    pRootParam->Descriptor.ShaderRegister = pDesc->shaderResource.reg;
    pRootParam->Descriptor.RegisterSpace = pDesc->shaderResource.set;
}

void create_root_constant(const RootParameter* pDesc, D3D12_ROOT_PARAMETER1* pRootParam)
{
    pRootParam->ShaderVisibility = util_to_dx12_shader_visibility(pDesc->shaderResource.used_stages);
    pRootParam->ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    pRootParam->Constants.Num32BitValues = pDesc->pDescriptorInfo->size;
    pRootParam->Constants.ShaderRegister = pDesc->shaderResource.reg;
    pRootParam->Constants.RegisterSpace = pDesc->shaderResource.set;
}
/************************************************************************/
// D3D12 Dynamic Loader
/************************************************************************/

typedef HRESULT (*PFN_D3D12_CREATE_DEVICE_FIXED)(_In_opt_ void* pAdapter, D3D_FEATURE_LEVEL MinimumFeatureLevel, _In_ REFIID riid,
                                                 _COM_Outptr_opt_ void** ppDevice);

#if defined(FORGE_D3D12_DYNAMIC_LOADING)

typedef HRESULT(WINAPI* PFN_D3D12_EnableExperimentalFeatures)(UINT, const IID*, void*, UINT*);

typedef HRESULT(WINAPI* PFN_D3D12_SerializeVersionedRootSignature)(const D3D12_VERSIONED_ROOT_SIGNATURE_DESC*, ID3DBlob**, ID3DBlob**);

typedef HRESULT(WINAPI* PFN_DXGI_CreateDXGIFactory2)(UINT, REFIID riid, void** ppFactory);

typedef HRESULT(WINAPI* PFN_DXGI_DXGIGetDebugInterface1)(UINT Flags, REFIID riid, _COM_Outptr_ void** pDebug);

static bool                                      gD3D12dllInited = false;
static PFN_D3D12_CREATE_DEVICE_FIXED             gPfnCreateDevice = NULL;
static PFN_D3D12_GET_DEBUG_INTERFACE             gPfnGetDebugInterface = NULL;
static PFN_D3D12_EnableExperimentalFeatures      gPfnEnableExperimentalFeatures = NULL;
static PFN_D3D12_SerializeVersionedRootSignature gPfnSerializeVersionedRootSignature = NULL;
static PFN_DXGI_CreateDXGIFactory2               gPfnCreateDXGIFactory2 = NULL;
static PFN_DXGI_DXGIGetDebugInterface1           gPfnDXGIGetDebugInterface1 = NULL;
static HMODULE                                   gD3D12dll = NULL;
static HMODULE                                   gDXGIdll = NULL;

#endif

void d3d12dll_exit()
{
#if defined(FORGE_D3D12_DYNAMIC_LOADING)
    if (gD3D12dll)
    {
        LOGF(LogLevel::eINFO, "Unloading d3d12.dll");
        gPfnCreateDevice = NULL;
        gPfnGetDebugInterface = NULL;
        gPfnEnableExperimentalFeatures = NULL;
        gPfnSerializeVersionedRootSignature = NULL;
        FreeLibrary(gD3D12dll);
        gD3D12dll = NULL;
    }

    if (gDXGIdll)
    {
        LOGF(LogLevel::eINFO, "Unloading dxgi.dll");
        gPfnCreateDXGIFactory2 = NULL;
        gPfnDXGIGetDebugInterface1 = NULL;
        FreeLibrary(gDXGIdll);
        gDXGIdll = NULL;
    }

    gD3D12dllInited = false;
#endif
}

bool d3d12dll_init()
{
#if defined(FORGE_D3D12_DYNAMIC_LOADING)

    if (gD3D12dllInited)
        return true;

    LOGF(LogLevel::eINFO, "Loading d3d12.dll");
    gD3D12dll = LoadLibraryExA("d3d12.dll", NULL, LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (gD3D12dll)
    {
        gPfnCreateDevice = (PFN_D3D12_CREATE_DEVICE_FIXED)(GetProcAddress(gD3D12dll, "D3D12CreateDevice"));
        gPfnGetDebugInterface = (PFN_D3D12_GET_DEBUG_INTERFACE)(GetProcAddress(gD3D12dll, "D3D12GetDebugInterface"));
        gPfnEnableExperimentalFeatures =
            (PFN_D3D12_EnableExperimentalFeatures)(GetProcAddress(gD3D12dll, "D3D12EnableExperimentalFeatures"));
        gPfnSerializeVersionedRootSignature =
            (PFN_D3D12_SerializeVersionedRootSignature)(GetProcAddress(gD3D12dll, "D3D12SerializeVersionedRootSignature"));
    }

    LOGF(LogLevel::eINFO, "Loading dxgi.dll");
    gDXGIdll = LoadLibraryExA("dxgi.dll", NULL, LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (gDXGIdll)
    {
        gPfnCreateDXGIFactory2 = (PFN_DXGI_CreateDXGIFactory2)(GetProcAddress(gDXGIdll, "CreateDXGIFactory2"));
        gPfnDXGIGetDebugInterface1 = (PFN_DXGI_DXGIGetDebugInterface1)(GetProcAddress(gDXGIdll, "DXGIGetDebugInterface1"));
    }

    gD3D12dllInited = gDXGIdll && gD3D12dll;
    if (!gD3D12dllInited)
        d3d12dll_exit();
    return gD3D12dllInited;
#else
    return true;
#endif
}
HRESULT WINAPI d3d12dll_CreateDevice(void* pAdapter, D3D_FEATURE_LEVEL MinimumFeatureLevel, REFIID riid, void** ppDevice) //-V835
{                                                                                                                         //-V835
#if defined(FORGE_D3D12_DYNAMIC_LOADING)
    if (gPfnCreateDevice)
        return gPfnCreateDevice(pAdapter, MinimumFeatureLevel, riid, ppDevice);
    return D3D12_ERROR_ADAPTER_NOT_FOUND;
#else
    return ((PFN_D3D12_CREATE_DEVICE_FIXED)D3D12CreateDevice)(pAdapter, MinimumFeatureLevel, riid, ppDevice);
#endif
}

HRESULT WINAPI d3d12dll_GetDebugInterface(REFIID riid, void** ppvDebug) //-V835
{                                                                       //-V835
#if defined(FORGE_D3D12_DYNAMIC_LOADING)
    if (gPfnGetDebugInterface)
        return gPfnGetDebugInterface(riid, ppvDebug);
    return E_FAIL;
#else
    return D3D12GetDebugInterface(riid, ppvDebug);
#endif
}

#if !defined(XBOX)
static HRESULT WINAPI d3d12dll_EnableExperimentalFeatures(UINT NumFeatures, const IID* pIIDs, void* pConfigurationStructs,
                                                          UINT* pConfigurationStructSizes)
{
#if defined(FORGE_D3D12_DYNAMIC_LOADING)
    if (gPfnEnableExperimentalFeatures)
        return gPfnEnableExperimentalFeatures(NumFeatures, pIIDs, pConfigurationStructs, pConfigurationStructSizes);
    return E_NOTIMPL;
#else
    return D3D12EnableExperimentalFeatures(NumFeatures, pIIDs, pConfigurationStructs, pConfigurationStructSizes);
#endif
}
#endif

static HRESULT d3d12dll_SerializeVersionedRootSignature(const D3D12_VERSIONED_ROOT_SIGNATURE_DESC* pRootSignature, ID3DBlob** ppBlob,
                                                        ID3DBlob** ppErrorBlob)
{
#if defined(FORGE_D3D12_DYNAMIC_LOADING)
    if (gPfnSerializeVersionedRootSignature)
        return gPfnSerializeVersionedRootSignature(pRootSignature, ppBlob, ppErrorBlob);
    return E_NOTIMPL;
#else
    return D3D12SerializeVersionedRootSignature(pRootSignature, ppBlob, ppErrorBlob);
#endif
}

#if !defined(XBOX)
static HRESULT WINAPI d3d12dll_CreateDXGIFactory2(UINT Flags, REFIID riid, void** ppFactory) //-V835
{                                                                                            //-V835
#if defined(FORGE_D3D12_DYNAMIC_LOADING)
    if (gPfnCreateDXGIFactory2)
        return gPfnCreateDXGIFactory2(Flags, riid, ppFactory);
    return DXGI_ERROR_UNSUPPORTED;
#else
    return CreateDXGIFactory2(Flags, riid, ppFactory);
#endif
}

HRESULT WINAPI d3d12dll_DXGIGetDebugInterface1(UINT Flags, REFIID riid, void** ppFactory) //-V835
{                                                                                         //-V835
#if defined(FORGE_D3D12_DYNAMIC_LOADING)
    if (gPfnDXGIGetDebugInterface1)
        return gPfnDXGIGetDebugInterface1(Flags, riid, ppFactory);
    return DXGI_ERROR_UNSUPPORTED;
#else
    return DXGIGetDebugInterface1(Flags, riid, ppFactory);
#endif
}
#endif

/************************************************************************/
// Internal utility functions
/************************************************************************/
D3D12_FILTER
util_to_dx12_filter(FilterType minFilter, FilterType magFilter, MipMapMode mipMapMode, bool aniso, bool comparisonFilterEnabled)
{
    if (aniso)
        return (comparisonFilterEnabled ? D3D12_FILTER_COMPARISON_ANISOTROPIC : D3D12_FILTER_ANISOTROPIC);

    // control bit : minFilter  magFilter   mipMapMode
    //   point   :   00	  00	   00
    //   linear  :   01	  01	   01
    // ex : trilinear == 010101
    int filter = (minFilter << 4) | (magFilter << 2) | mipMapMode;
    int baseFilter = comparisonFilterEnabled ? D3D12_FILTER_COMPARISON_MIN_MAG_MIP_POINT : D3D12_FILTER_MIN_MAG_MIP_POINT;
    return (D3D12_FILTER)(baseFilter + filter);
}

D3D12_HEAP_TYPE util_to_heap_type(ResourceMemoryUsage memoryUsage)
{
    switch (memoryUsage)
    {
    case RESOURCE_MEMORY_USAGE_GPU_ONLY:
        return D3D12_HEAP_TYPE_DEFAULT;
    case RESOURCE_MEMORY_USAGE_CPU_ONLY:
    case RESOURCE_MEMORY_USAGE_CPU_TO_GPU:
        return D3D12_HEAP_TYPE_UPLOAD;
    case RESOURCE_MEMORY_USAGE_GPU_TO_CPU:
        return D3D12_HEAP_TYPE_READBACK;
    case RESOURCE_MEMORY_USAGE_GPU_UPLOAD:
        return D3D12_HEAP_TYPE_GPU_UPLOAD;
    default:
        ASSERT(false);
        break;
    }

    return D3D12_HEAP_TYPE_DEFAULT;
}

D3D12_HEAP_FLAGS util_to_heap_flags(ResourceHeapCreationFlags flags)
{
    D3D12_HEAP_FLAGS outFlags = D3D12_HEAP_FLAG_NONE;

    if (flags & RESOURCE_HEAP_FLAG_SHARED)
        outFlags |= D3D12_HEAP_FLAG_SHARED;
    if (flags & RESOURCE_HEAP_FLAG_DENY_BUFFERS)
        outFlags |= D3D12_HEAP_FLAG_DENY_BUFFERS;
    if (flags & RESOURCE_HEAP_FLAG_ALLOW_DISPLAY)
        outFlags |= D3D12_HEAP_FLAG_ALLOW_DISPLAY;
    if (flags & RESOURCE_HEAP_FLAG_SHARED_CROSS_ADAPTER)
        outFlags |= D3D12_HEAP_FLAG_SHARED_CROSS_ADAPTER;
    if (flags & RESOURCE_HEAP_FLAG_DENY_RT_DS_TEXTURES)
        outFlags |= D3D12_HEAP_FLAG_DENY_RT_DS_TEXTURES;
    if (flags & RESOURCE_HEAP_FLAG_DENY_NON_RT_DS_TEXTURES)
        outFlags |= D3D12_HEAP_FLAG_DENY_NON_RT_DS_TEXTURES;
    if (flags & RESOURCE_HEAP_FLAG_HARDWARE_PROTECTED)
        outFlags |= D3D12_HEAP_FLAG_HARDWARE_PROTECTED;
    if (flags & RESOURCE_HEAP_FLAG_ALLOW_WRITE_WATCH)
        outFlags |= D3D12_HEAP_FLAG_ALLOW_WRITE_WATCH;

#if !defined(XBOX)
    if (flags & RESOURCE_HEAP_FLAG_ALLOW_SHADER_ATOMICS)
        outFlags |= D3D12_HEAP_FLAG_ALLOW_SHADER_ATOMICS;
#endif

    if (flags == RESOURCE_HEAP_FLAG_ALLOW_ALL_BUFFERS_AND_TEXTURES)
        outFlags = D3D12_HEAP_FLAG_ALLOW_ALL_BUFFERS_AND_TEXTURES;

    return outFlags;
}

D3D12_TEXTURE_ADDRESS_MODE util_to_dx12_texture_address_mode(AddressMode addressMode)
{
    switch (addressMode)
    {
    case ADDRESS_MODE_MIRROR:
        return D3D12_TEXTURE_ADDRESS_MODE_MIRROR;
    case ADDRESS_MODE_REPEAT:
        return D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    case ADDRESS_MODE_CLAMP_TO_EDGE:
        return D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    case ADDRESS_MODE_CLAMP_TO_BORDER:
        return D3D12_TEXTURE_ADDRESS_MODE_BORDER;
    default:
        return D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    }
}

D3D12_PRIMITIVE_TOPOLOGY_TYPE util_to_dx12_primitive_topology_type(PrimitiveTopology topology)
{
    switch (topology)
    {
    case PRIMITIVE_TOPO_POINT_LIST:
        return D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT;
    case PRIMITIVE_TOPO_LINE_LIST:
    case PRIMITIVE_TOPO_LINE_STRIP:
        return D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;
    case PRIMITIVE_TOPO_TRI_LIST:
    case PRIMITIVE_TOPO_TRI_STRIP:
        return D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    case PRIMITIVE_TOPO_PATCH_LIST:
        return D3D12_PRIMITIVE_TOPOLOGY_TYPE_PATCH;
    default:
        return D3D12_PRIMITIVE_TOPOLOGY_TYPE_UNDEFINED;
    }
}

uint64_t util_dx12_determine_storage_counter_offset(uint64_t buffer_size)
{
    uint64_t alignment = D3D12_UAV_COUNTER_PLACEMENT_ALIGNMENT;
    uint64_t result = (buffer_size + (alignment - 1)) & ~(alignment - 1);
    return result;
}

DXGI_FORMAT util_to_dx12_uav_format(DXGI_FORMAT defaultFormat)
{
    switch (defaultFormat)
    {
    case DXGI_FORMAT_R8G8B8A8_TYPELESS:
    case DXGI_FORMAT_R8G8B8A8_UNORM:
    case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
        return DXGI_FORMAT_R8G8B8A8_UNORM;

    case DXGI_FORMAT_B8G8R8A8_TYPELESS:
    case DXGI_FORMAT_B8G8R8A8_UNORM:
    case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
        return DXGI_FORMAT_B8G8R8A8_UNORM;

    case DXGI_FORMAT_B8G8R8X8_TYPELESS:
    case DXGI_FORMAT_B8G8R8X8_UNORM:
    case DXGI_FORMAT_B8G8R8X8_UNORM_SRGB:
        return DXGI_FORMAT_B8G8R8X8_UNORM;

    case DXGI_FORMAT_R32_TYPELESS:
    case DXGI_FORMAT_R32_FLOAT:
        return DXGI_FORMAT_R32_FLOAT;

#if defined(ENABLE_GRAPHICS_DEBUG)
    case DXGI_FORMAT_R32G8X24_TYPELESS:
    case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
    case DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS:
    case DXGI_FORMAT_X32_TYPELESS_G8X24_UINT:
    case DXGI_FORMAT_D32_FLOAT:
    case DXGI_FORMAT_R24G8_TYPELESS:
    case DXGI_FORMAT_D24_UNORM_S8_UINT:
    case DXGI_FORMAT_R24_UNORM_X8_TYPELESS:
    case DXGI_FORMAT_X24_TYPELESS_G8_UINT:
    case DXGI_FORMAT_D16_UNORM:
        LOGF(LogLevel::eERROR, "Requested a UAV format for a depth stencil format");
#endif

    default:
        return defaultFormat;
    }
}

DXGI_FORMAT util_to_dx12_dsv_format(DXGI_FORMAT defaultFormat)
{
    switch (defaultFormat)
    {
        // 32-bit Z w/ Stencil
    case DXGI_FORMAT_R32G8X24_TYPELESS:
    case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
    case DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS:
    case DXGI_FORMAT_X32_TYPELESS_G8X24_UINT:
        return DXGI_FORMAT_D32_FLOAT_S8X24_UINT;

        // No Stencil
    case DXGI_FORMAT_R32_TYPELESS:
    case DXGI_FORMAT_D32_FLOAT:
    case DXGI_FORMAT_R32_FLOAT:
        return DXGI_FORMAT_D32_FLOAT;

        // 24-bit Z
    case DXGI_FORMAT_R24G8_TYPELESS:
    case DXGI_FORMAT_D24_UNORM_S8_UINT:
    case DXGI_FORMAT_R24_UNORM_X8_TYPELESS:
    case DXGI_FORMAT_X24_TYPELESS_G8_UINT:
        return DXGI_FORMAT_D24_UNORM_S8_UINT;

        // 16-bit Z w/o Stencil
    case DXGI_FORMAT_R16_TYPELESS:
    case DXGI_FORMAT_D16_UNORM:
    case DXGI_FORMAT_R16_UNORM:
        return DXGI_FORMAT_D16_UNORM;

    default:
        return defaultFormat;
    }
}

DXGI_FORMAT util_to_dx12_srv_format(DXGI_FORMAT defaultFormat)
{
    switch (defaultFormat)
    {
        // 32-bit Z w/ Stencil
    case DXGI_FORMAT_R32G8X24_TYPELESS:
    case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
    case DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS:
    case DXGI_FORMAT_X32_TYPELESS_G8X24_UINT:
        return DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS;

        // No Stencil
    case DXGI_FORMAT_R32_TYPELESS:
    case DXGI_FORMAT_D32_FLOAT:
    case DXGI_FORMAT_R32_FLOAT:
        return DXGI_FORMAT_R32_FLOAT;

        // 24-bit Z
    case DXGI_FORMAT_R24G8_TYPELESS:
    case DXGI_FORMAT_D24_UNORM_S8_UINT:
    case DXGI_FORMAT_R24_UNORM_X8_TYPELESS:
    case DXGI_FORMAT_X24_TYPELESS_G8_UINT:
        return DXGI_FORMAT_R24_UNORM_X8_TYPELESS;

        // 16-bit Z w/o Stencil
    case DXGI_FORMAT_R16_TYPELESS:
    case DXGI_FORMAT_D16_UNORM:
    case DXGI_FORMAT_R16_UNORM:
        return DXGI_FORMAT_R16_UNORM;

    case DXGI_FORMAT_R8G8B8A8_TYPELESS:
        return DXGI_FORMAT_R8G8B8A8_UNORM;

    default:
        return defaultFormat;
    }
}

DXGI_FORMAT util_to_dx12_stencil_format(DXGI_FORMAT defaultFormat)
{
    switch (defaultFormat)
    {
        // 32-bit Z w/ Stencil
    case DXGI_FORMAT_R32G8X24_TYPELESS:
    case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
    case DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS:
    case DXGI_FORMAT_X32_TYPELESS_G8X24_UINT:
        return DXGI_FORMAT_X32_TYPELESS_G8X24_UINT;

        // 24-bit Z
    case DXGI_FORMAT_R24G8_TYPELESS:
    case DXGI_FORMAT_D24_UNORM_S8_UINT:
    case DXGI_FORMAT_R24_UNORM_X8_TYPELESS:
    case DXGI_FORMAT_X24_TYPELESS_G8_UINT:
        return DXGI_FORMAT_X24_TYPELESS_G8_UINT;

    default:
        return DXGI_FORMAT_UNKNOWN;
    }
}

DXGI_FORMAT util_to_dx12_swapchain_format(hz::Format const format)
{
    DXGI_FORMAT result = DXGI_FORMAT_UNKNOWN;

    // FLIP_DISCARD and FLIP_SEQEUNTIAL swapchain buffers only support these formats
    switch (format)
    {
    case hz::Format::R16G16B16A16_SFLOAT:
        result = DXGI_FORMAT_R16G16B16A16_FLOAT;
        break;
    case hz::Format::B8G8R8A8_UNORM:
    case hz::Format::B8G8R8A8_SRGB:
        result = DXGI_FORMAT_B8G8R8A8_UNORM;
        break;
    case hz::Format::R8G8B8A8_UNORM:
    case hz::Format::R8G8B8A8_SRGB:
        result = DXGI_FORMAT_R8G8B8A8_UNORM;
        break;
    case hz::Format::R10G10B10A2_UNORM:
        result = DXGI_FORMAT_R10G10B10A2_UNORM;
        break;
    default:
        break;
    }

    if (result == DXGI_FORMAT_UNKNOWN)
    {
        LOGF(LogLevel::eERROR, "Image Format (%u) not supported for creating swapchain buffer", (uint32_t)format);
    }

    return result;
}

DXGI_COLOR_SPACE_TYPE util_to_dx12_colorspace(ColorSpace colorspace)
{
    switch (colorspace)
    {
    case COLOR_SPACE_SDR_LINEAR:
    case COLOR_SPACE_SDR_SRGB:
        return DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709;
    case COLOR_SPACE_P2020:
        return DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020;
    case COLOR_SPACE_EXTENDED_SRGB:
        return DXGI_COLOR_SPACE_RGB_FULL_G10_NONE_P709;
    default:
        break;
    }

    LOGF(LogLevel::eERROR, "Color Space (%u) not supported for creating swapchain buffer", (uint32_t)colorspace);

    return DXGI_COLOR_SPACE_CUSTOM;
}

D3D12_SHADER_VISIBILITY util_to_dx12_shader_visibility(ShaderStage stages)
{
    D3D12_SHADER_VISIBILITY res = D3D12_SHADER_VISIBILITY_ALL;
    uint32_t                stageCount = 0;

    if (stages == SHADER_STAGE_COMP)
    {
        return D3D12_SHADER_VISIBILITY_ALL;
    }
    if (stages & SHADER_STAGE_VERT)
    {
        res = D3D12_SHADER_VISIBILITY_VERTEX;
        ++stageCount;
    }
    if (stages & SHADER_STAGE_GEOM)
    {
        res = D3D12_SHADER_VISIBILITY_GEOMETRY;
        ++stageCount;
    }
    if (stages & SHADER_STAGE_HULL)
    {
        res = D3D12_SHADER_VISIBILITY_HULL;
        ++stageCount;
    }
    if (stages & SHADER_STAGE_DOMN)
    {
        res = D3D12_SHADER_VISIBILITY_DOMAIN;
        ++stageCount;
    }
    if (stages & SHADER_STAGE_FRAG)
    {
        res = D3D12_SHADER_VISIBILITY_PIXEL;
        ++stageCount;
    }
    ASSERT(stageCount > 0);
    return stageCount > 1 ? D3D12_SHADER_VISIBILITY_ALL : res;
}

D3D12_DESCRIPTOR_RANGE_TYPE util_to_dx12_descriptor_range(DescriptorType type)
{
    switch (type)
    {
    case DESCRIPTOR_TYPE_UNIFORM_BUFFER:
    case DESCRIPTOR_TYPE_ROOT_CONSTANT:
        return D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
    case DESCRIPTOR_TYPE_RW_BUFFER:
    case DESCRIPTOR_TYPE_RW_TEXTURE:
        return D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
    case DESCRIPTOR_TYPE_SAMPLER:
        return D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER;
#ifdef D3D12_RAYTRACING_AVAILABLE
    case DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE:
#endif
    case DESCRIPTOR_TYPE_TEXTURE:
    case DESCRIPTOR_TYPE_BUFFER:
        return D3D12_DESCRIPTOR_RANGE_TYPE_SRV;

    default:
        ASSERT(false && "Invalid DescriptorInfo Type");
        return D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    }
}

D3D12_RESOURCE_STATES util_to_dx12_resource_state(ResourceState state)
{
    D3D12_RESOURCE_STATES ret = D3D12_RESOURCE_STATE_COMMON;

    // These states cannot be combined with other states so we just do an == check
    if (state == RESOURCE_STATE_GENERIC_READ)
        return D3D12_RESOURCE_STATE_GENERIC_READ;
    if (state == RESOURCE_STATE_COMMON)
        return D3D12_RESOURCE_STATE_COMMON;
    if (state == RESOURCE_STATE_PRESENT)
        return D3D12_RESOURCE_STATE_PRESENT;

    if (state & RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER)
        ret |= D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
    if (state & RESOURCE_STATE_INDEX_BUFFER)
        ret |= D3D12_RESOURCE_STATE_INDEX_BUFFER;
    if (state & RESOURCE_STATE_RENDER_TARGET)
        ret |= D3D12_RESOURCE_STATE_RENDER_TARGET;
    if (state & RESOURCE_STATE_UNORDERED_ACCESS)
        ret |= D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
    if (state & RESOURCE_STATE_DEPTH_WRITE)
        ret |= D3D12_RESOURCE_STATE_DEPTH_WRITE;
    else if (state & RESOURCE_STATE_DEPTH_READ)
        ret |= D3D12_RESOURCE_STATE_DEPTH_READ;
    if (state & RESOURCE_STATE_STREAM_OUT)
        ret |= D3D12_RESOURCE_STATE_STREAM_OUT;
    if (state & RESOURCE_STATE_INDIRECT_ARGUMENT)
        ret |= D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT;
    if (state & RESOURCE_STATE_COPY_DEST)
        ret |= D3D12_RESOURCE_STATE_COPY_DEST;
    if (state & RESOURCE_STATE_COPY_SOURCE)
        ret |= D3D12_RESOURCE_STATE_COPY_SOURCE;
    if (state & RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE)
        ret |= D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
    if (state & RESOURCE_STATE_PIXEL_SHADER_RESOURCE)
        ret |= D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
#ifdef D3D12_RAYTRACING_AVAILABLE
    if (state & (RESOURCE_STATE_ACCELERATION_STRUCTURE_READ | RESOURCE_STATE_ACCELERATION_STRUCTURE_WRITE))
        ret |= D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE;
#endif

    return ret;
}

static inline FORGE_CONSTEXPR D3D12_QUERY_HEAP_TYPE ToDX12QueryHeapType(QueryType type)
{
    switch (type)
    {
    case QUERY_TYPE_TIMESTAMP:
        return D3D12_QUERY_HEAP_TYPE_TIMESTAMP;
    case QUERY_TYPE_PIPELINE_STATISTICS:
        return D3D12_QUERY_HEAP_TYPE_PIPELINE_STATISTICS;
    case QUERY_TYPE_OCCLUSION:
        return D3D12_QUERY_HEAP_TYPE_OCCLUSION;
    default:
        return (D3D12_QUERY_HEAP_TYPE)-1;
    }
}

static inline FORGE_CONSTEXPR D3D12_QUERY_TYPE ToDX12QueryType(QueryType type)
{
    switch (type)
    {
    case QUERY_TYPE_TIMESTAMP:
        return D3D12_QUERY_TYPE_TIMESTAMP;
    case QUERY_TYPE_PIPELINE_STATISTICS:
        return D3D12_QUERY_TYPE_PIPELINE_STATISTICS;
    case QUERY_TYPE_OCCLUSION:
        return D3D12_QUERY_TYPE_OCCLUSION;
    default:
        return (D3D12_QUERY_TYPE)-1;
    }
}

static inline FORGE_CONSTEXPR uint32_t ToQueryWidth(QueryType type)
{
    switch (type)
    {
    case QUERY_TYPE_PIPELINE_STATISTICS:
        return sizeof(D3D12_QUERY_DATA_PIPELINE_STATISTICS);
    default:
        return sizeof(uint64_t);
    }
}

#if !defined(XBOX)
void util_enumerate_gpus(IDXGIFactory6* dxgiFactory, uint32_t* pGpuCount, GpuDesc* gpuDesc, bool* pFoundSoftwareAdapter)
{
    D3D_FEATURE_LEVEL feature_levels[5] = {
        D3D_FEATURE_LEVEL_12_2, D3D_FEATURE_LEVEL_12_1, D3D_FEATURE_LEVEL_12_0, D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0,
    };

    uint32_t       gpuCount = 0;
    IDXGIAdapter4* adapter = NULL;
    bool           foundSoftwareAdapter = false;

    // Find number of usable GPUs
    // Use DXGI6 interface which lets us specify gpu preference so we dont need to use NVOptimus or AMDPowerExpress exports
    for (UINT i = 0;
         DXGI_ERROR_NOT_FOUND != dxgiFactory->EnumAdapterByGpuPreference(i, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_ARGS(&adapter)); ++i)
    {
        if (gpuCount >= MAX_MULTIPLE_GPUS)
        {
            break;
        }

        DECLARE_ZERO(DXGI_ADAPTER_DESC3, desc);
        adapter->GetDesc3(&desc);

        // Ignore Microsoft Driver
        if (!(desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE))
        {
            for (uint32_t level = 0; level < sizeof(feature_levels) / sizeof(feature_levels[0]); ++level)
            {
                // Make sure the adapter can support a D3D12 device
                HRESULT res = (d3d12dll_CreateDevice(adapter, feature_levels[level], __uuidof(ID3D12Device), NULL));
                if (SUCCEEDED(res))
                {
                    GpuDesc  gpuDescTmp = {};
                    GpuDesc* pGpuDesc = gpuDesc ? &gpuDesc[gpuCount] : &gpuDescTmp;
                    HRESULT  hres = adapter->QueryInterface(IID_ARGS(&pGpuDesc->pGpu));
                    if (SUCCEEDED(hres))
                    {
                        if (gpuDesc)
                        {
                            ID3D12Device* device = NULL;
                            d3d12dll_CreateDevice(adapter, feature_levels[level], IID_PPV_ARGS(&device));
                            hook_fill_gpu_desc(device, feature_levels[level], pGpuDesc);
                            // get preset for current gpu description
                            pGpuDesc->preset = getGPUPresetLevel(pGpuDesc->vendorId, pGpuDesc->deviceId,
                                                                  getGPUVendorName(pGpuDesc->vendorId), pGpuDesc->name);
                            SAFE_RELEASE(device);
                        }
                        else
                        {
                            SAFE_RELEASE(pGpuDesc->pGpu);
                        }
                        ++gpuCount;
                        break;
                    }
                }
            }
        }
        else
        {
            foundSoftwareAdapter = true;
        }

        adapter->Release();
    }

    if (pGpuCount)
        *pGpuCount = gpuCount;

    if (pFoundSoftwareAdapter)
        *pFoundSoftwareAdapter = foundSoftwareAdapter;
}
#endif

static void QueryRaytracingSupport(ID3D12Device* pDevice, GPUSettings* pGpuSettings)
{
    UNREF_PARAM(pDevice);
    UNREF_PARAM(pGpuSettings);
#ifdef D3D12_RAYTRACING_AVAILABLE
    ASSERT(pDevice);
    D3D12_FEATURE_DATA_D3D12_OPTIONS5 opts5 = {};
    HRESULT                           hres = pDevice->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS5, &opts5, sizeof(opts5));
    if (SUCCEEDED(hres))
    {
        pGpuSettings->rayPipelineSupported = (opts5.RaytracingTier != D3D12_RAYTRACING_TIER_NOT_SUPPORTED);
#if defined(SCARLETT)
        pGpuSettings->rayQuerySupported = true;
#else
        pGpuSettings->rayQuerySupported = (opts5.RaytracingTier > D3D12_RAYTRACING_TIER_1_0);
#endif
        pGpuSettings->raytracingSupported = pGpuSettings->rayPipelineSupported || pGpuSettings->rayQuerySupported;
    }
#endif
}

static void Query64BitAtomicsSupport(ID3D12Device* pDevice, GPUSettings* pGpuSettings)
{
    ASSERT(pDevice);
    D3D12_FEATURE_DATA_D3D12_OPTIONS9 opts9 = {};
    HRESULT                           hres = pDevice->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS9, &opts9, sizeof(opts9));
    if (SUCCEEDED(hres))
    {
        pGpuSettings->m64BitAtomicsSupported = opts9.AtomicInt64OnTypedResourceSupported;
    }
}

void QueryGPUSettings(ID3D12Device* pDevice, const GpuDesc* pGpuDesc, GPUSettings* pSettings)
{
    GPUSettings& gpuSettings = *pSettings;
    setDefaultGPUSettings(pSettings);
    gpuSettings.uniformBufferAlignment = D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT;
    gpuSettings.uploadBufferTextureAlignment = D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT;
    gpuSettings.uploadBufferTextureRowAlignment = D3D12_TEXTURE_DATA_PITCH_ALIGNMENT;
    gpuSettings.multiDrawIndirect = true;
    gpuSettings.maxVertexInputBindings = 32U;

    // assign device ID
    gpuSettings.gpuVendorPreset.modelId = pGpuDesc->deviceId;
    // assign vendor ID
    gpuSettings.gpuVendorPreset.vendorId = pGpuDesc->vendorId;
    // assign Revision ID
    gpuSettings.gpuVendorPreset.revisionId = pGpuDesc->revisionId;
    // get name from api
    strncpy(gpuSettings.gpuVendorPreset.gpuName, pGpuDesc->name, MAX_GPU_VENDOR_STRING_LENGTH);
    // get preset
    gpuSettings.gpuVendorPreset.presetLevel = pGpuDesc->preset;
    // get VRAM
    gpuSettings.vram = pGpuDesc->dedicatedVideoMemory;
    // get wave lane count
    gpuSettings.waveLaneCount = pGpuDesc->featureDataOptions1.WaveLaneCountMin;
    gpuSettings.waveOpsSupported = pGpuDesc->featureDataOptions1.WaveOps ? true : false;
    gpuSettings.int64ShaderOpsSupported = pGpuDesc->featureDataOptions1.Int64ShaderOps ? true : false;
    gpuSettings.rovsSupported = pGpuDesc->featureDataOptions.ROVsSupported ? true : false;
#if defined(AMDAGS)
    gpuSettings.amdAsicFamily = agsGetAsicFamily(pGpuDesc->deviceId);
#endif
    gpuSettings.tessellationSupported = gpuSettings.geometryShaderSupported = true;

#if defined(XBOXONE)
    gpuSettings.waveOpsSupported = true;
    gpuSettings.waveOpsSupportFlags = WAVE_OPS_SUPPORT_FLAG_BASIC_BIT | WAVE_OPS_SUPPORT_FLAG_VOTE_BIT | WAVE_OPS_SUPPORT_FLAG_BALLOT_BIT |
                                       WAVE_OPS_SUPPORT_FLAG_SHUFFLE_BIT;
    gpuSettings.waveOpsSupportedStageFlags |= SHADER_STAGE_ALL_GRAPHICS | SHADER_STAGE_COMP;
#else
    if (gpuSettings.waveOpsSupported)
    {
        gpuSettings.waveOpsSupportFlags = WAVE_OPS_SUPPORT_FLAG_ALL;
        gpuSettings.waveOpsSupportedStageFlags = SHADER_STAGE_ALL_GRAPHICS | SHADER_STAGE_COMP;
    }
#endif

    gpuSettings.gpuMarkers = true;
    gpuSettings.hdrSupported = true;
    gpuSettings.indirectRootConstant = true;
    gpuSettings.builtinDrawID = false;
    gpuSettings.timestampQueries = true;
    gpuSettings.occlusionQueries = true;
    gpuSettings.pipelineStatsQueries = true;
    gpuSettings.softwareVRSSupported = true;
    gpuSettings.allowBufferTextureInSameHeap = pGpuDesc->featureDataOptions.ResourceHeapTier >= D3D12_RESOURCE_HEAP_TIER_2;
    gpuSettings.gpuUploadHeapSupported = pGpuDesc->featureDataOptions16.GPUUploadHeapSupported ? true : false;
    gpuSettings.executeIndirectIncrementingConstantSupported = true;
    gpuSettings.directStorageSupported = is_directstorage_runtime_available();
    // compute shader group count
    gpuSettings.maxTotalComputeThreads = D3D12_CS_THREAD_GROUP_MAX_THREADS_PER_GROUP;
    gpuSettings.maxComputeThreads[0] = D3D12_CS_THREAD_GROUP_MAX_X;
    gpuSettings.maxComputeThreads[1] = D3D12_CS_THREAD_GROUP_MAX_Y;
    gpuSettings.maxComputeThreads[2] = D3D12_CS_THREAD_GROUP_MAX_Z;

    // Determine root signature size for this gpu driver
    DXGI_ADAPTER_DESC adapterDesc;
    pGpuDesc->pGpu->GetDesc(&adapterDesc);

    // set default driver version as empty string
    gpuSettings.gpuVendorPreset.gpuDriverVersion[0] = '\0';
    if (gpuVendorEquals(adapterDesc.VendorId, "nvidia"))
    {
#if defined(NVAPI)
        if (NvAPI_Status::NVAPI_OK == gNvStatus)
        {
            snprintf(gpuSettings.gpuVendorPreset.gpuDriverVersion, MAX_GPU_VENDOR_STRING_LENGTH, "%lu.%lu",
                     gNvGpuInfo.driverVersion / 100, gNvGpuInfo.driverVersion % 100);
        }
#endif
    }
    else if (gpuVendorEquals(adapterDesc.VendorId, "amd"))
    {
#if defined(AMDAGS)
        if (AGSReturnCode::AGS_SUCCESS == gAgsStatus)
        {
            snprintf(gpuSettings.gpuVendorPreset.gpuDriverVersion, MAX_GPU_VENDOR_STRING_LENGTH, "%s", gAgsGpuInfo.driverVersion);
        }
#endif
    }
    // fallback to windows version (device manager number), works for intel not for nvidia
    else
    {
        IDXGIDevice*  dxgiDevice;
        LARGE_INTEGER umdVersion;
        HRESULT       hr = pGpuDesc->pGpu->CheckInterfaceSupport(__uuidof(dxgiDevice), &umdVersion);
        if (SUCCEEDED(hr))
        {
            // 31.0.101.5074 -> 101.5074, only use build number like it's done for vulkan
            // WORD product = HIWORD(umdVersion.HighPart);
            // WORD version = LOWORD(umdVersion.HighPart);
            WORD subVersion = HIWORD(umdVersion.LowPart);
            WORD build = LOWORD(umdVersion.LowPart);
            snprintf(gpuSettings.gpuVendorPreset.gpuDriverVersion, MAX_GPU_VENDOR_STRING_LENGTH, "%d.%d", subVersion, build);
        }
    }
    gpuSettings.maxRootSignatureDWORDS = 13;
    gpuSettings.maxBoundTextures = UINT32_MAX;

    QueryRaytracingSupport(pDevice, pSettings);
    Query64BitAtomicsSupport(pDevice, pSettings);
}

static void InitializeBufferDesc(Renderer* pRenderer, const BufferDesc* pDesc, D3D12_RESOURCE_DESC* desc)
{
    ASSERT(pRenderer);
    ASSERT(pDesc);
    ASSERT(desc);

    uint64_t allocationSize = pDesc->size;
    // Align the buffer size to multiples of 256
    if ((pDesc->descriptors & DESCRIPTOR_TYPE_UNIFORM_BUFFER))
    {
        allocationSize = round_up_64(allocationSize, pRenderer->pGpu->settings.uniformBufferAlignment);
    }

    desc->Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    // Keep this at 0 so the runtime/allocator can choose the proper alignment.
    // D3D12MA may enable tight alignment, and that flag requires Alignment == 0.
    // FIXME(hyl5): crash when set D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT
    desc->Width = allocationSize;
    desc->Height = 1;
    desc->DepthOrArraySize = 1;
    desc->MipLevels = 1;
    desc->Format = DXGI_FORMAT_UNKNOWN;
    desc->SampleDesc.Count = 1;
    desc->SampleDesc.Quality = 0;
    desc->Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    desc->Flags = D3D12_RESOURCE_FLAG_NONE;

    hook_modify_buffer_resource_desc(pDesc, desc);

    if (pDesc->descriptors & DESCRIPTOR_TYPE_RW_BUFFER)
    {
        desc->Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
    }

    // Adjust for padding
    UINT64 padded_size = 0;
    pRenderer->dx.pDevice->GetCopyableFootprints(desc, 0, 1, 0, NULL, NULL, NULL, &padded_size);
    allocationSize = (uint64_t)padded_size;
    desc->Width = allocationSize;

    if (RESOURCE_MEMORY_USAGE_GPU_TO_CPU == pDesc->memoryUsage)
    {
        desc->Flags |= D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE;
    }
}

static void InitializeTextureDesc(Renderer* pRenderer, const TextureDesc* pDesc, D3D12_RESOURCE_DESC* desc,
                                  ResourceState* pStartResourceState)
{
    ASSERT(pRenderer);
    ASSERT(pDesc);
    ASSERT(desc);

    D3D12_RESOURCE_DIMENSION res_dim = D3D12_RESOURCE_DIMENSION_UNKNOWN;
    if (pDesc->flags & TEXTURE_CREATION_FLAG_FORCE_2D)
    {
        ASSERT(pDesc->depth == 1);
        res_dim = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    }
    else if (pDesc->flags & TEXTURE_CREATION_FLAG_FORCE_3D)
    {
        res_dim = D3D12_RESOURCE_DIMENSION_TEXTURE3D;
    }
    else
    {
        if (pDesc->depth > 1)
            res_dim = D3D12_RESOURCE_DIMENSION_TEXTURE3D;
        else if (pDesc->height > 1)
            res_dim = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        else
            res_dim = D3D12_RESOURCE_DIMENSION_TEXTURE1D;
    }

    DXGI_FORMAT dxFormat = (DXGI_FORMAT)TinyImageFormat_ToDXGI_FORMAT((TinyImageFormat)pDesc->format);

    desc->Dimension = res_dim;
    // On PC, If Alignment is set to 0, the runtime will use 4MB for MSAA textures and 64KB for everything else.
    // On XBox, We have to explicitlly assign D3D12_DEFAULT_MSAA_RESOURCE_PLACEMENT_ALIGNMENT if MSAA is used
    desc->Alignment = (UINT)pDesc->sampleCount > 1 ? D3D12_DEFAULT_MSAA_RESOURCE_PLACEMENT_ALIGNMENT : 0;
    desc->Width = pDesc->width;
    desc->Height = pDesc->height;
    desc->DepthOrArraySize = (UINT16)(pDesc->arraySize != 1 ? pDesc->arraySize : pDesc->depth);
    desc->MipLevels = (UINT16)pDesc->mipLevels;
    desc->Format = (DXGI_FORMAT)TinyImageFormat_DXGI_FORMATToTypeless((TinyImageFormat_DXGI_FORMAT)dxFormat);
    desc->SampleDesc.Count = (UINT)pDesc->sampleCount;
    desc->SampleDesc.Quality = (UINT)pDesc->sampleQuality;
    desc->Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    desc->Flags = D3D12_RESOURCE_FLAG_NONE;

    D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS data;
    data.Format = desc->Format;
    data.Flags = D3D12_MULTISAMPLE_QUALITY_LEVELS_FLAG_NONE;
    data.SampleCount = desc->SampleDesc.Count;
    pRenderer->dx.pDevice->CheckFeatureSupport(D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS, &data, sizeof(data));
    while (data.NumQualityLevels == 0 && data.SampleCount > 0)
    {
        LOGF(LogLevel::eWARNING, "Sample Count (%u) not supported. Trying a lower sample count (%u)", data.SampleCount,
             data.SampleCount / 2);
        data.SampleCount = desc->SampleDesc.Count / 2;
        pRenderer->dx.pDevice->CheckFeatureSupport(D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS, &data, sizeof(data));
    }
    desc->SampleDesc.Count = data.SampleCount;

    ResourceState actualStartState = pDesc->startState;

    // Decide UAV flags
    if (pDesc->descriptors & DESCRIPTOR_TYPE_RW_TEXTURE)
    {
        desc->Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
    }

    // Decide render target flags
    if (pDesc->startState & RESOURCE_STATE_RENDER_TARGET)
    {
        desc->Flags |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
        actualStartState = (pDesc->startState > RESOURCE_STATE_RENDER_TARGET)
                               ? (pDesc->startState & (ResourceState)~RESOURCE_STATE_RENDER_TARGET)
                               : RESOURCE_STATE_RENDER_TARGET;
    }
    else if (pDesc->startState & RESOURCE_STATE_DEPTH_WRITE)
    {
        desc->Flags |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
        actualStartState = (pDesc->startState > RESOURCE_STATE_DEPTH_WRITE)
                               ? (pDesc->startState & (ResourceState)~RESOURCE_STATE_DEPTH_WRITE)
                               : RESOURCE_STATE_DEPTH_WRITE;
    }

    // Decide sharing flags
    if (pDesc->flags & TEXTURE_CREATION_FLAG_EXPORT_ADAPTER_BIT)
    {
        desc->Flags |= D3D12_RESOURCE_FLAG_ALLOW_CROSS_ADAPTER;
        desc->Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    }

#if defined(XBOX)
    if (pDesc->flags & TEXTURE_CREATION_FLAG_ALLOW_DISPLAY_TARGET)
    {
        desc->Format = dxFormat;
    }
#endif

    if (pDesc->flags & TEXTURE_CREATION_FLAG_ALLOW_DISPLAY_TARGET)
    {
        actualStartState = RESOURCE_STATE_PRESENT;
    }

    if (pStartResourceState)
        *pStartResourceState = actualStartState;
}

#if defined(_WINDOWS) && defined(FORGE_DEBUG)
void DebugMessageCallback(D3D12_MESSAGE_CATEGORY category, D3D12_MESSAGE_SEVERITY severity, D3D12_MESSAGE_ID id, LPCSTR pDescription,
                          void* pContext)
{
    UNREF_PARAM(pContext);
    D3D12_MESSAGE_ID ignoreMessageIDs[] = {
        // Trying to load a PSO that isn't in loaded library is expected. New PSOs will always trigger this warning.
        D3D12_MESSAGE_ID_LOADPIPELINE_NAMENOTFOUND,
        // Required when we want to Alias the same memory from multiple buffers and use them at the same time
        // (we know from the App that we don't read/write on the same memory at the same time)
        D3D12_MESSAGE_ID_HEAP_ADDRESS_RANGE_INTERSECTS_MULTIPLE_BUFFERS,
        // Just a baggage
        D3D12_MESSAGE_ID_CREATE_COMMANDLIST12,
        D3D12_MESSAGE_ID_DESTROY_COMMANDLIST12,
        D3D12_MESSAGE_ID_INVALID_SUBRESOURCE_STATE,
        // Bug in validation when using acceleration structure in compute or graphics pipeline
        // D3D12 ERROR: ID3D12CommandList::Dispatch: Static Descriptor SRV resource dimensions (UNKNOWN (11)) differs from that expected by
        // shader (D3D12_SRV_DIMENSION_BUFFER) UNKNOWN (11) is D3D12_SRV_DIMENSION_RAYTRACING_ACCELERATION_STRUCTURE
        D3D12_MESSAGE_ID_COMMAND_LIST_STATIC_DESCRIPTOR_RESOURCE_DIMENSION_MISMATCH,
        D3D12_MESSAGE_ID_RESOURCE_BARRIER_MISMATCHING_COMMAND_LIST_TYPE,
        D3D12_MESSAGE_ID_CREATERESOURCE_STATE_IGNORED,
    };

    for (uint32_t m = 0; m < TF_ARRAY_COUNT(ignoreMessageIDs); ++m)
        if (ignoreMessageIDs[m] == id)
            return;

    if (severity == D3D12_MESSAGE_SEVERITY_CORRUPTION)
    {
        LOGF(LogLevel::eERROR, "[Corruption] [%d] : %s (%d)", category, pDescription, id);
    }
    else if (severity == D3D12_MESSAGE_SEVERITY_ERROR)
    {
        LOGF(LogLevel::eERROR, "[Error] [%d] : %s (%d)", category, pDescription, id);
    }
    else if (severity == D3D12_MESSAGE_SEVERITY_WARNING)
    {
        LOGF(LogLevel::eWARNING, "[%d] : %s (%d)", category, pDescription, id);
    }
    else if (severity == D3D12_MESSAGE_SEVERITY_INFO)
    {
        LOGF(LogLevel::eINFO, "[%d] : %s (%d)", category, pDescription, id);
    }
}
#endif

/************************************************************************/
// Internal init functions
/************************************************************************/
#if !defined(XBOX)

// Note that Windows 10 Creator Update SDK is required for enabling Shader Model 6 feature.
static HRESULT EnableExperimentalShaderModels()
{
    static const GUID D3D12ExperimentalShaderModelsID = { /* 76f5573e-f13a-40f5-b297-81ce9e18933f */
                                                          0x76f5573e,
                                                          0xf13a,
                                                          0x40f5,
                                                          { 0xb2, 0x97, 0x81, 0xce, 0x9e, 0x18, 0x93, 0x3f }
    };

    return d3d12dll_EnableExperimentalFeatures(1, &D3D12ExperimentalShaderModelsID, NULL, NULL);
}
#endif

#if defined(XBOX)
UINT HANGBEGINCALLBACK(UINT64 Flags)
{
    LOGF(LogLevel::eINFO, "( %d )", Flags);
    return (UINT)Flags;
}

void HANGPRINTCALLBACK(const CHAR* strLine)
{
    LOGF(LogLevel::eINFO, "( %s )", strLine);
    return;
}

void HANGDUMPCALLBACK(const WCHAR* strFileName)
{
    UNREF_PARAM(strFileName);
    return;
}
#endif

static bool SelectBestGpu(Renderer* pRenderer, const RendererDesc* pDesc, D3D_FEATURE_LEVEL* pFeatureLevel, uint32_t* pOutGpuCount)
{
    UNREF_PARAM(pDesc);
    GPUSettings      gpuSettings[MAX_MULTIPLE_GPUS] = {};
    RendererContext* pContext = pRenderer->pContext;
    for (uint32_t i = 0; i < pContext->gpuCount; ++i)
    {
        gpuSettings[i] = pContext->gpus[i].settings;
    }

    uint32_t gpuIndex = util_select_best_gpu(gpuSettings, pContext->gpuCount);
    ASSERT(gpuIndex < pContext->gpuCount);

    // Get the latest and greatest feature level gpu
    pRenderer->pGpu = &pRenderer->pContext->gpus[gpuIndex];
    ASSERT(pRenderer->pGpu != NULL);

    bool driverValid = checkDriverRejectionSettings(&gpuSettings[gpuIndex]);
    if (!driverValid)
    {
        setRendererInitializationError("Driver rejection return invalid result.\nPlease, update your driver to the latest version.");
        return false;
    }

    // Print selected GPU information
    LOGF(LogLevel::eINFO, "GPU[%u] is selected as default GPU", gpuIndex);
    LOGF(LogLevel::eINFO, "Name of selected gpu: %s", pRenderer->pGpu->settings.gpuVendorPreset.gpuName);
    LOGF(LogLevel::eINFO, "Vendor id of selected gpu: %#x", pRenderer->pGpu->settings.gpuVendorPreset.vendorId);
    LOGF(LogLevel::eINFO, "Model id of selected gpu: %#x", pRenderer->pGpu->settings.gpuVendorPreset.modelId);
    LOGF(LogLevel::eINFO, "Revision id of selected gpu: %#x", pRenderer->pGpu->settings.gpuVendorPreset.revisionId);
    LOGF(LogLevel::eINFO, "Preset of selected gpu: %s", presetLevelToString(pRenderer->pGpu->settings.gpuVendorPreset.presetLevel));

    if (pFeatureLevel)
    {
        *pFeatureLevel = pContext->gpus[gpuIndex].settings.featureLevel;
    }

    *pOutGpuCount = pContext->gpuCount;

    return true;
}

static bool AddDevice(const RendererDesc* pDesc, Renderer* pRenderer)
{
    D3D_FEATURE_LEVEL supportedFeatureLevel = (D3D_FEATURE_LEVEL)0;
    uint32_t          gpuCount = 0;
    if (pDesc->pContext)
    {
        ASSERT(pDesc->gpuIndex < pDesc->pContext->gpuCount);

        pRenderer->pGpu = &pDesc->pContext->gpus[pDesc->gpuIndex];
        supportedFeatureLevel = pRenderer->pGpu->settings.featureLevel;
    }
    else
    {
        if (!SelectBestGpu(pRenderer, pDesc, &supportedFeatureLevel, &gpuCount))
        {
            return false;
        }
    }

    // Load functions
    {
        HMODULE module = hook_get_d3d12_module_handle();

        fnD3D12CreateRootSignatureDeserializer =
            (PFN_D3D12_CREATE_ROOT_SIGNATURE_DESERIALIZER)GetProcAddress(module, "D3D12SerializeVersionedRootSignature");

        fnD3D12SerializeVersionedRootSignature =
            (PFN_D3D12_SERIALIZE_VERSIONED_ROOT_SIGNATURE)GetProcAddress(module, "D3D12SerializeVersionedRootSignature");

        fnD3D12CreateVersionedRootSignatureDeserializer =
            (PFN_D3D12_CREATE_VERSIONED_ROOT_SIGNATURE_DESERIALIZER)GetProcAddress(module, "D3D12CreateVersionedRootSignatureDeserializer");
    }

#if defined(XBOX)
    pRenderer->dx.pDevice = pRenderer->pGpu->dx.pDevice;
#else
    CHECK_HRESULT(d3d12dll_CreateDevice(pRenderer->pGpu->dx.pGpu, supportedFeatureLevel, IID_ARGS(&pRenderer->dx.pDevice)));
#endif

#if defined(ENABLE_NSIGHT_AFTERMATH)
    SetAftermathDevice(pRenderer->dx.pDevice);
#endif

#if defined(_WINDOWS) && defined(FORGE_DEBUG)
    HRESULT hr = pRenderer->dx.pDevice->QueryInterface(IID_ARGS(&pRenderer->dx.pDebugValidation));
    pRenderer->dx.useDebugCallback = true;
    if (!SUCCEEDED(hr))
    {
        SAFE_RELEASE(pRenderer->dx.pDebugValidation);
        pRenderer->dx.useDebugCallback = false;
        hr = pRenderer->dx.pDevice->QueryInterface(__uuidof(ID3D12InfoQueue), IID_PPV_ARGS_Helper(&pRenderer->dx.pDebugValidation));
    }
    if (SUCCEEDED(hr))
    {
        pRenderer->dx.pDebugValidation->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, true);
        pRenderer->dx.pDebugValidation->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, false);
        pRenderer->dx.pDebugValidation->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, false);
        pRenderer->dx.pDebugValidation->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_INFO, false);
        pRenderer->dx.pDebugValidation->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_MESSAGE, false);

        constexpr uint32_t maxHideMessages = 32;
        uint32_t           hideMessageCount = 0;
        D3D12_MESSAGE_ID   hideMessages[maxHideMessages] = {};

        // Trying to load a PSO that isn't in loaded library is expected. New PSOs will always trigger this warning.
        hideMessages[hideMessageCount++] = D3D12_MESSAGE_ID_LOADPIPELINE_NAMENOTFOUND;

        // On Windows 11 there's a bug in the DXGI debug layer that triggers a false-positive on hybrid GPU
        // laptops during Present. The problem appears to be a race condition, so it may or may not happen.
        // Suppressing D3D12_MESSAGE_ID_RESOURCE_BARRIER_MISMATCHING_COMMAND_LIST_TYPE avoids this problem.
        // If we have >2 GPU's (eg. Laptop with integrated and dedicated GPU).
        if (gpuCount >= 2)
        {
            pRenderer->dx.suppressMismatchingCommandListDuringPresent = true;
        }

        if (hideMessageCount)
        {
            D3D12_INFO_QUEUE_FILTER filter = {};
            filter.DenyList.NumIDs = hideMessageCount;
            filter.DenyList.pIDList = hideMessages;
            pRenderer->dx.pDebugValidation->AddStorageFilterEntries(&filter);
        }

        D3D12_MESSAGE_ID hide[] = {
            // Required when we want to Alias the same memory from multiple buffers and use them at the same time
            // (we know from the App that we don't read/write on the same memory at the same time)
            D3D12_MESSAGE_ID_HEAP_ADDRESS_RANGE_INTERSECTS_MULTIPLE_BUFFERS,
            D3D12_MESSAGE_ID_CREATERESOURCE_STATE_IGNORED,
        };
        D3D12_INFO_QUEUE_FILTER filter = {};
        filter.DenyList.NumIDs = _countof(hide);
        filter.DenyList.pIDList = hide;
        pRenderer->dx.pDebugValidation->AddStorageFilterEntries(&filter);

        if (pRenderer->dx.useDebugCallback)
        {
            pRenderer->dx.pDebugValidation->SetMuteDebugOutput(true);
            // D3D12_MESSAGE_CALLBACK_IGNORE_FILTERS, will enable all message filtering in the callback function, no need to use Push/Pop,
            // but we stick with FLAG_NONE for failsafe
            HRESULT res = pRenderer->dx.pDebugValidation->RegisterMessageCallback(DebugMessageCallback, D3D12_MESSAGE_CALLBACK_FLAG_NONE,
                                                                                   pRenderer, &pRenderer->dx.callbackCookie);
            if (!SUCCEEDED(res))
            {
                internal_log(eERROR, "RegisterMessageCallback failed - disabling DirectX12 ID3D12InfoQueue1 debug callbacks", "AddDevice");
            }
        }
    }
#endif

#ifdef ENABLE_GRAPHICS_DEBUG
    SetObjectName(pRenderer->dx.pDevice, "Main Device");
#endif // ENABLE_GRAPHICS_DEBUG

    return true;
}

static void RemoveDevice(Renderer* pRenderer)
{
#if defined(_WINDOWS) && defined(FORGE_DEBUG)
    if (pRenderer->dx.pDebugValidation && pRenderer->pGpu->settings.suppressInvalidSubresourceStateAfterExit)
    {
        // bypass AMD driver issue with vk and dxgi swapchains resource states
        D3D12_MESSAGE_ID        hide[] = { D3D12_MESSAGE_ID_INVALID_SUBRESOURCE_STATE };
        D3D12_INFO_QUEUE_FILTER filter = {};
        filter.DenyList.NumIDs = 1;
        filter.DenyList.pIDList = hide;
        pRenderer->dx.pDebugValidation->PushStorageFilter(&filter);
    }
    if (pRenderer->dx.useDebugCallback)
        pRenderer->dx.pDebugValidation->UnregisterMessageCallback(pRenderer->dx.callbackCookie);
    SAFE_RELEASE(pRenderer->dx.pDebugValidation);
#endif

    SAFE_RELEASE(pRenderer->dx.pDevice);

#if defined(ENABLE_NSIGHT_AFTERMATH)
    DestroyAftermathTracker(&pRenderer->aftermathTracker);
#endif
}

void InitCommon(const RendererContextDesc* pDesc, RendererContext* pContext)
{
    UNREF_PARAM(pDesc);
    UNREF_PARAM(pContext);
    d3d12dll_init();

    AGSReturnCode agsRet = agsInit();
    if (AGSReturnCode::AGS_SUCCESS == agsRet)
    {
        agsPrintDriverInfo();
    }

    NvAPI_Status nvStatus = nvapiInit();
    if (NvAPI_Status::NVAPI_OK == nvStatus)
    {
        nvapiPrintDriverInfo();
    }

    // The D3D debug layer (as well as Microsoft PIX and other graphics debugger
    // tools using an injection library) is not compatible with Nsight Aftermath.
    // If Aftermath detects that any of these tools are present it will fail initialization.
#if defined(ENABLE_GRAPHICS_DEBUG) && defined(_WINDOWS) && !defined(USE_NSIGHT_AFTERMATH)
    // add debug layer if in debug mode
    if (SUCCEEDED(d3d12dll_GetDebugInterface(IID_ARGS(&pContext->dx.pDebug))))
    {
        hook_enable_debug_layer(pDesc, pContext);
    }
#endif

#if !defined(XBOX)
    UINT flags = 0;
#if defined(ENABLE_GRAPHICS_DEBUG)
    flags = DXGI_CREATE_FACTORY_DEBUG;
#endif

    CHECK_HRESULT(d3d12dll_CreateDXGIFactory2(flags, IID_ARGS(&pContext->dx.pDXGIFactory)));
#endif

#if defined(USE_DRED)
    if (SUCCEEDED(d3d12dll_GetDebugInterface(IID_ARGS(&pContext->dx.pDredSettings))))
    {
        // Turn on AutoBreadcrumbs and Page Fault reporting
        pContext->dx.pDredSettings->SetAutoBreadcrumbsEnablement(D3D12_DRED_ENABLEMENT_FORCED_ON);
        pContext->dx.pDredSettings->SetPageFaultEnablement(D3D12_DRED_ENABLEMENT_FORCED_ON);
    }
#endif
}

void ExitCommon(RendererContext* pContext)
{
    SAFE_RELEASE(pContext->dx.pDXGIFactory);
#if defined(ENABLE_GRAPHICS_DEBUG) && defined(_WINDOWS)
    SAFE_RELEASE(pContext->dx.pDebug);
#endif
#if defined(USE_DRED)
    SAFE_RELEASE(pContext->pDredSettings);
#endif

#if defined(ENABLE_GRAPHICS_DEBUG) && !defined(XBOX)
    IDXGIDebug1* dxgiDebug = NULL;
    if (SUCCEEDED(d3d12dll_DXGIGetDebugInterface1(0, IID_PPV_ARGS(&dxgiDebug))))
    {
        LOGF(eWARNING, "Printing live D3D12 objects to the debugger output window...");

        dxgiDebug->ReportLiveObjects(DXGI_DEBUG_ALL,
                                     DXGI_DEBUG_RLO_FLAGS(DXGI_DEBUG_RLO_DETAIL | DXGI_DEBUG_RLO_SUMMARY | DXGI_DEBUG_RLO_IGNORE_INTERNAL));
        SAFE_RELEASE(dxgiDebug);
    }
    else
    {
        LOGF(eWARNING, "Unable to retrieve the D3D12 DXGI debug interface, cannot print live D3D12 objects.");
    }
#endif

    nvapiExit();
    agsExit();
    d3d12dll_exit();
}

/************************************************************************/
// Renderer Context Init / Exit
/************************************************************************/
static uint32_t gRendererCount = 0;

void d3d12_initRendererContext(const char* appName, const RendererContextDesc* pDesc, RendererContext** ppContext)
{
    ASSERT(appName);
    ASSERT(pDesc);
    ASSERT(ppContext);
    ASSERT(gRendererCount == 0);

    RendererContext* pContext = (RendererContext*)tf_calloc_memalign(1, alignof(RendererContext), sizeof(RendererContext));
    ASSERT(pContext);

    for (uint32_t i = 0; i < TF_ARRAY_COUNT(pContext->gpus); ++i)
    {
        setDefaultGPUSettings(&pContext->gpus[i].settings);
    }

#if defined(XBOX)
    ID3D12Device* device = NULL;
    // Create the DX12 API device object.
    CHECK_HRESULT(hook_create_device(NULL, D3D_FEATURE_LEVEL_12_1, true, &device));

    // First, retrieve the underlying DXGI device from the D3D device.
    IDXGIDevice1* dxgiDevice;
    CHECK_HRESULT(device->QueryInterface(IID_ARGS(&dxgiDevice)));

    // Identify the physical adapter (GPU or card) this device is running on.
    IDXGIAdapter* dxgiAdapter;
    CHECK_HRESULT(dxgiDevice->GetAdapter(&dxgiAdapter));

    // And obtain the factory object that created it.
    CHECK_HRESULT(dxgiAdapter->GetParent(IID_ARGS(&pContext->dx.pDXGIFactory)));

    GpuInfo* gpu = &pContext->gpus[0];
    pContext->gpuCount = 1;
    dxgiAdapter->QueryInterface(IID_ARGS(&gpu->dx.pGpu));
    SAFE_RELEASE(dxgiAdapter);

    GpuDesc gpuDesc = {};
    gpuDesc.pGpu = gpu->dx.pGpu;
    hook_fill_gpu_desc(device, D3D_FEATURE_LEVEL_12_1, &gpuDesc);
    QueryGPUSettings(device, &gpuDesc, &gpu->settings);
    d3d12CapsBuilder(device, &gpu->capBits);
    gpu->dx.pDevice = device;
    applyGPUConfigurationRules(&gpu->settings, &gpu->capBits);
#else
    InitCommon(pDesc, pContext);

    bool foundSoftwareAdapter = false;

    // Find number of usable GPUs
    util_enumerate_gpus(pContext->dx.pDXGIFactory, &pContext->gpuCount, NULL, &foundSoftwareAdapter);

    // If the only adapter we found is a software adapter, log error message for QA
    if (!pContext->gpuCount && foundSoftwareAdapter)
    {
        LOGF(eERROR, "The only available GPU has DXGI_ADAPTER_FLAG_SOFTWARE. Early exiting");
        ASSERT(false);
        return;
    }

    ASSERT(pContext->gpuCount);
    GpuDesc gpuDesc[MAX_MULTIPLE_GPUS] = {};

    util_enumerate_gpus(pContext->dx.pDXGIFactory, &pContext->gpuCount, gpuDesc, NULL);
    ASSERT(pContext->gpuCount > 0);
    for (uint32_t i = 0; i < pContext->gpuCount; ++i)
    {
        ID3D12Device* device = NULL;
        // Create device to query additional properties.
        d3d12dll_CreateDevice(gpuDesc[i].pGpu, gpuDesc[i].maxSupportedFeatureLevel, IID_PPV_ARGS(&device));

        QueryGPUSettings(device, &gpuDesc[i], &pContext->gpus[i].settings);
        d3d12CapsBuilder(device, &pContext->gpus[i].capBits);

        pContext->gpus[i].dx.pGpu = gpuDesc[i].pGpu;
        pContext->gpus[i].settings.featureLevel = gpuDesc[i].maxSupportedFeatureLevel;
        pContext->gpus[i].settings.maxBoundTextures =
            gpuDesc[i].featureDataOptions.ResourceBindingTier == D3D12_RESOURCE_BINDING_TIER::D3D12_RESOURCE_BINDING_TIER_1 ? 128
                                                                                                                             : 1000000;

        applyGPUConfigurationRules(&pContext->gpus[i].settings, &pContext->gpus[i].capBits);

        LOGF(LogLevel::eINFO, "GPU[%u] detected. Vendor ID: %#x, Model ID: %#x, Revision ID: %#x, Preset: %s, GPU Name: %s", i,
             pContext->gpus[i].settings.gpuVendorPreset.vendorId, pContext->gpus[i].settings.gpuVendorPreset.modelId,
             pContext->gpus[i].settings.gpuVendorPreset.revisionId,
             presetLevelToString(pContext->gpus[i].settings.gpuVendorPreset.presetLevel),
             pContext->gpus[i].settings.gpuVendorPreset.gpuName);

        SAFE_RELEASE(device);
    }

#endif
    *ppContext = pContext;
}

void d3d12_exitRendererContext(RendererContext* pContext)
{
    ASSERT(pContext);
    for (uint32_t i = 0; i < pContext->gpuCount; ++i)
    {
        SAFE_RELEASE(pContext->gpus[i].dx.pGpu);
    }
    ExitCommon(pContext);
#if defined(XBOX)
    extern void hook_remove_device(ID3D12Device * pDevice);
    hook_remove_device(pContext->gpus[0].dx.pDevice);
#endif
    SAFE_FREE(pContext);
}
/************************************************************************/
// Renderer Init Remove
/************************************************************************/
void d3d12_initRenderer(const char* appName, const RendererDesc* pDesc, Renderer** ppRenderer)
{
    ASSERT(appName);
    ASSERT(pDesc);
    ASSERT(ppRenderer);

    Renderer* pRenderer = (Renderer*)tf_calloc_memalign(1, alignof(Renderer), sizeof(Renderer));
    ASSERT(pRenderer);

    pRenderer->rendererApi = RENDERER_API_D3D12;
    pRenderer->shaderTarget = pDesc->shaderTarget;
    pRenderer->pName = appName;

    // Initialize the D3D12 bits
    {
        if (!pDesc->pContext)
        {
            RendererContextDesc contextDesc = {};
            contextDesc.enableGpuBasedValidation = pDesc->enableGpuBasedValidation;
            contextDesc.dx.featureLevel = pDesc->dx.featureLevel;
            pRenderer->ownsContext = true;
            d3d12_initRendererContext(appName, &contextDesc, &pRenderer->pContext);
        }
        else
        {
            pRenderer->pContext = pDesc->pContext;
            pRenderer->ownsContext = false;
        }

#if defined(USE_NSIGHT_AFTERMATH)
        // Enable Nsight Aftermath GPU crash dump creation.
        // This needs to be done before the Vulkan device is created.
        CreateAftermathTracker(pRenderer->pName, &pRenderer->aftermathTracker);
#endif

        if (!AddDevice(pDesc, pRenderer))
        {
            *ppRenderer = NULL;
            return;
        }

#if !defined(XBOX)
        GPUPresetLevel selectedPreset = pRenderer->pGpu->settings.gpuVendorPreset.presetLevel;
        if (selectedPreset == GPU_PRESET_NONE || selectedPreset == GPU_PRESET_OFFICE)
        {
            RemoveDevice(pRenderer);
            SAFE_FREE(pRenderer);
            const char* reason =
                selectedPreset == GPU_PRESET_OFFICE ? "Selected GPU has an Office preset." : "Selected GPU has no usable preset.";
            setRendererInitializationError(reason);
            LOGF(LogLevel::eERROR, "%s", reason);

            ASSERT(selectedPreset != GPU_PRESET_NONE && selectedPreset != GPU_PRESET_OFFICE); //-V547

            *ppRenderer = NULL;
            return;
        }

        if (pRenderer->shaderTarget >= SHADER_TARGET_6_0)
        {
            // Query the level of support of Shader Model.
            D3D12_FEATURE_DATA_SHADER_MODEL   shaderModelSupport = { D3D_SHADER_MODEL_6_0 };
            D3D12_FEATURE_DATA_D3D12_OPTIONS1 waveIntrinsicsSupport = {};
            if (!SUCCEEDED(pRenderer->dx.pDevice->CheckFeatureSupport((D3D12_FEATURE)D3D12_FEATURE_SHADER_MODEL, &shaderModelSupport,
                                                                       sizeof(shaderModelSupport))))
            {
                return;
            }
            // Query the level of support of Wave Intrinsics.
            if (!SUCCEEDED(pRenderer->dx.pDevice->CheckFeatureSupport((D3D12_FEATURE)D3D12_FEATURE_D3D12_OPTIONS1, &waveIntrinsicsSupport,
                                                                       sizeof(waveIntrinsicsSupport))))
            {
                return;
            }

            // if (!pRenderer->pGpu->settings.enhancedBarriersSupported)
            // {
            //     RemoveDevice(pRenderer);
            //     SAFE_FREE(pRenderer);
            //     setRendererInitializationError("Selected GPU does not support D3D12 Enhanced Barriers.");
            //     LOGF(LogLevel::eERROR, "Selected GPU does not support D3D12 Enhanced Barriers.");
            //     *ppRenderer = NULL;
            //     return;
            // }
        }

#endif

        /************************************************************************/
        // Descriptor heaps
        /************************************************************************/
        pRenderer->dx.pCPUDescriptorHeaps = (DescriptorHeap**)tf_malloc(D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES * sizeof(DescriptorHeap*));
        pRenderer->dx.pCbvSrvUavHeaps = (DescriptorHeap**)tf_malloc(sizeof(DescriptorHeap*));
        pRenderer->dx.pSamplerHeaps = (DescriptorHeap**)tf_malloc(sizeof(DescriptorHeap*));

        for (uint32_t i = 0; i < D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES; ++i)
        {
            D3D12_DESCRIPTOR_HEAP_DESC desc = {};
            desc.Flags = gCpuDescriptorHeapProperties[i].flags;
            desc.NodeMask = 0; // CPU Descriptor Heap - Node mask is irrelevant
            desc.NumDescriptors = gCpuDescriptorHeapProperties[i].maxDescriptors;
            desc.Type = (D3D12_DESCRIPTOR_HEAP_TYPE)i;
            add_descriptor_heap(pRenderer->dx.pDevice, &desc, &pRenderer->dx.pCPUDescriptorHeaps[i]);
        }

        {
            D3D12_DESCRIPTOR_HEAP_DESC desc = {};
            desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
            desc.NodeMask = 0;

            desc.NumDescriptors = D3D12_MAX_SHADER_VISIBLE_DESCRIPTOR_HEAP_SIZE_TIER_1;
            desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
            add_descriptor_heap(pRenderer->dx.pDevice, &desc, &pRenderer->dx.pCbvSrvUavHeaps[0]);

            // Max sampler descriptor count
            desc.NumDescriptors = D3D12_MAX_SHADER_VISIBLE_SAMPLER_HEAP_SIZE;
            desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
            add_descriptor_heap(pRenderer->dx.pDevice, &desc, &pRenderer->dx.pSamplerHeaps[0]);
        }
        /************************************************************************/
        // Memory allocator
        /************************************************************************/
        D3D12MA::ALLOCATOR_DESC desc = {};
        desc.Flags = D3D12MA::ALLOCATOR_FLAG_NONE;
        desc.pDevice = pRenderer->dx.pDevice;
        desc.pAdapter = pRenderer->pGpu->dx.pGpu;

        D3D12MA::ALLOCATION_CALLBACKS allocationCallbacks = {};
        allocationCallbacks.pAllocate = [](size_t size, size_t alignment, void*) { return tf_memalign(alignment, size); };
        allocationCallbacks.pFree = [](void* ptr, void*) { tf_free(ptr); };
        desc.pAllocationCallbacks = &allocationCallbacks;
        CHECK_HRESULT(D3D12MA::CreateAllocator(&desc, &pRenderer->dx.pResourceAllocator));
    }
    /************************************************************************/
    /************************************************************************/
    add_default_resources(pRenderer);

    hook_post_init_renderer(pRenderer);

    ++gRendererCount;

    // Renderer is good!
    *ppRenderer = pRenderer;
}

void d3d12_exitRenderer(Renderer* pRenderer)
{
    ASSERT(pRenderer);
    --gRendererCount;

    remove_default_resources(pRenderer);

    // Destroy the Direct3D12 bits
    for (uint32_t i = 0; i < D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES; ++i)
    {
        remove_descriptor_heap(pRenderer->dx.pCPUDescriptorHeaps[i]);
    }

    remove_descriptor_heap(pRenderer->dx.pCbvSrvUavHeaps[0]);
    remove_descriptor_heap(pRenderer->dx.pSamplerHeaps[0]);
    SAFE_RELEASE(pRenderer->dx.pResourceAllocator);

    RemoveDevice(pRenderer);

    hook_post_remove_renderer(pRenderer);

    if (pRenderer->ownsContext)
    {
        d3d12_exitRendererContext(pRenderer->pContext);
    }

    // Free all the renderer components
    SAFE_FREE(pRenderer->dx.pCPUDescriptorHeaps);
    SAFE_FREE(pRenderer->dx.pCbvSrvUavHeaps);
    SAFE_FREE(pRenderer->dx.pSamplerHeaps);
    SAFE_FREE(pRenderer);
}
/************************************************************************/
// Resource Creation Functions
/************************************************************************/
void d3d12_addFence(Renderer* pRenderer, Fence** ppFence)
{
    // ASSERT that renderer is valid
    ASSERT(pRenderer);
    ASSERT(ppFence);

    // create a Fence and ASSERT that it is valid
    Fence* pFence = (Fence*)tf_calloc(1, sizeof(Fence));
    ASSERT(pFence);

    CHECK_HRESULT(pRenderer->dx.pDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_ARGS(&pFence->dx.pFence)));
    pFence->dx.fenceValue = 0;

    pFence->dx.pWaitIdleFenceEvent = CreateEvent(NULL, FALSE, FALSE, NULL);

    *ppFence = pFence;
}

void d3d12_removeFence(Renderer* pRenderer, Fence* pFence)
{
    // ASSERT that renderer is valid
    ASSERT(pRenderer);
    // ASSERT that given fence to remove is valid
    ASSERT(pFence);

    SAFE_RELEASE(pFence->dx.pFence);
    CloseHandle(pFence->dx.pWaitIdleFenceEvent);

    SAFE_FREE(pFence);
}

void d3d12_addSemaphore(Renderer* pRenderer, Semaphore** ppSemaphore)
{
    // ASSERT that renderer is valid
    ASSERT(pRenderer);
    ASSERT(ppSemaphore);

    // create a Fence and ASSERT that it is valid
    Semaphore* pSemaphore = (Semaphore*)tf_calloc(1, sizeof(Semaphore));
    ASSERT(pSemaphore);

    CHECK_HRESULT(pRenderer->dx.pDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_ARGS(&pSemaphore->dx.pFence)));
    pSemaphore->dx.fenceValue = 0;

    *ppSemaphore = pSemaphore;
}

void d3d12_removeSemaphore(Renderer* pRenderer, Semaphore* pSemaphore)
{
    // ASSERT that renderer is valid
    ASSERT(pRenderer);
    // ASSERT that given fence to remove is valid
    ASSERT(pSemaphore);

    SAFE_RELEASE(pSemaphore->dx.pFence);
    SAFE_FREE(pSemaphore);
}

void d3d12_addQueue(Renderer* pRenderer, QueueDesc* pDesc, Queue** ppQueue)
{
    ASSERT(pRenderer);
    ASSERT(pDesc);
    ASSERT(ppQueue);

    Queue* pQueue = (Queue*)tf_calloc(1, sizeof(Queue));
    ASSERT(pQueue);

    D3D12_COMMAND_QUEUE_DESC queueDesc = {};
    if (pDesc->flag & QUEUE_FLAG_DISABLE_GPU_TIMEOUT)
        queueDesc.Flags |= D3D12_COMMAND_QUEUE_FLAG_DISABLE_GPU_TIMEOUT;
    queueDesc.Type = gDx12CmdTypeTranslator[pDesc->type];
    queueDesc.Priority = gDx12QueuePriorityTranslator[pDesc->priority];
    queueDesc.NodeMask = 0;

    CHECK_HRESULT(hook_create_command_queue(pRenderer->dx.pDevice, &queueDesc, &pQueue->dx.pQueue));

    ULONG      refCount = pQueue->dx.pQueue->AddRef();
    const bool firstQueue = 2 == refCount;
    pQueue->dx.pQueue->Release();
    if (firstQueue)
    {
        char queueTypeBuffer[MAX_DEBUG_NAME_LENGTH] = {};
        if (!pDesc->pName)
        {
            const char* queueNames[] = {
                "GRAPHICS QUEUE",
                "INVALID",
                "COMPUTE QUEUE",
                "COPY QUEUE",
            };
            snprintf(queueTypeBuffer, MAX_DEBUG_NAME_LENGTH, "%s", queueNames[queueDesc.Type]);
        }
        SetObjectName(pQueue->dx.pQueue, pDesc->pName ? pDesc->pName : queueTypeBuffer);
    }

    pQueue->type = pDesc->type;
#if defined(_WINDOWS) && defined(FORGE_DEBUG)
    pQueue->dx.pRenderer = pRenderer;
#endif

    // Add queue fence. This fence will make sure we finish all GPU works before releasing the queue
    addFence(pRenderer, &pQueue->dx.pFence);

    *ppQueue = pQueue;
}

void d3d12_removeQueue(Renderer* pRenderer, Queue* pQueue)
{
    ASSERT(pQueue);

    // Make sure we finished all GPU works before we remove the queue
    waitQueueIdle(pQueue);

    removeFence(pRenderer, pQueue->dx.pFence);

    SAFE_RELEASE(pQueue->dx.pQueue);

    SAFE_FREE(pQueue);
}

struct DirectStorage
{
    Renderer*         pRenderer;
    HMODULE           pModule;
    IDStorageFactory* pFactory;
};

struct DirectStorageFile
{
    IDStorageFile* pFile;
};

struct DirectStorageQueue
{
    IDStorageQueue* pQueue;
};

struct DirectStorageStatusArray
{
    IDStorageStatusArray* pStatusArray;
};

typedef HRESULT(WINAPI* PFN_DStorageGetFactory)(REFIID riid, void** ppv);

static HMODULE load_directstorage_module()
{
    HMODULE module = LoadLibraryA("dstorage.dll");
    if (!module)
    {
        module = LoadLibraryA("dstoragecore.dll");
    }

    return module;
}

static bool is_directstorage_runtime_available()
{
    HMODULE module = load_directstorage_module();
    if (!module)
    {
        return false;
    }

    const bool hasFactory = GetProcAddress(module, "DStorageGetFactory") != NULL;
    FreeLibrary(module);
    return hasFactory;
}

bool d3d12_isGpuUploadHeapSupported(Renderer* pRenderer)
{
    ASSERT(pRenderer);
    return pRenderer && pRenderer->pGpu->settings.gpuUploadHeapSupported;
}

bool d3d12_isDirectStorageSupported(Renderer* pRenderer)
{
    UNREF_PARAM(pRenderer);
    return is_directstorage_runtime_available();
}

static DSTORAGE_PRIORITY util_to_dstorage_priority(DirectStoragePriority priority)
{
    switch (priority)
    {
    case DIRECT_STORAGE_PRIORITY_LOW:
        return DSTORAGE_PRIORITY_LOW;
    case DIRECT_STORAGE_PRIORITY_HIGH:
        return DSTORAGE_PRIORITY_HIGH;
    case DIRECT_STORAGE_PRIORITY_REALTIME:
        return DSTORAGE_PRIORITY_REALTIME;
    case DIRECT_STORAGE_PRIORITY_NORMAL:
    default:
        return DSTORAGE_PRIORITY_NORMAL;
    }
}

static DSTORAGE_REQUEST_SOURCE_TYPE util_to_dstorage_source_type(DirectStorageSourceType sourceType)
{
    return sourceType == DIRECT_STORAGE_SOURCE_MEMORY ? DSTORAGE_REQUEST_SOURCE_MEMORY : DSTORAGE_REQUEST_SOURCE_FILE;
}

static DSTORAGE_COMPRESSION_FORMAT util_to_dstorage_compression(DirectStorageCompressionFormat compression)
{
    switch (compression)
    {
    case DIRECT_STORAGE_COMPRESSION_GDEFLATE:
        return DSTORAGE_COMPRESSION_FORMAT_GDEFLATE;
    case DIRECT_STORAGE_COMPRESSION_NONE:
    default:
        return DSTORAGE_COMPRESSION_FORMAT_NONE;
    }
}

HRESULT d3d12_initDirectStorage(Renderer* pRenderer, const DirectStorageDesc* pDesc, DirectStorage** ppDirectStorage)
{
    ASSERT(pRenderer);
    ASSERT(ppDirectStorage);

    *ppDirectStorage = NULL;

    HMODULE module = load_directstorage_module();
    if (!module)
    {
        return HRESULT_FROM_WIN32(ERROR_MOD_NOT_FOUND);
    }

    PFN_DStorageGetFactory getFactory = (PFN_DStorageGetFactory)GetProcAddress(module, "DStorageGetFactory");
    if (!getFactory)
    {
        FreeLibrary(module);
        return HRESULT_FROM_WIN32(ERROR_PROC_NOT_FOUND);
    }

    IDStorageFactory* pFactory = NULL;
    HRESULT           hr = getFactory(__uuidof(IDStorageFactory), (void**)&pFactory);
    if (FAILED(hr))
    {
        FreeLibrary(module);
        return hr;
    }

    if (pDesc)
    {
        pFactory->SetDebugFlags((UINT32)pDesc->debugFlags);
        if (pDesc->stagingBufferSize)
        {
            hr = pFactory->SetStagingBufferSize(pDesc->stagingBufferSize);
            if (FAILED(hr))
            {
                SAFE_RELEASE(pFactory);
                FreeLibrary(module);
                return hr;
            }
        }
    }

    DirectStorage* pDirectStorage = (DirectStorage*)tf_calloc(1, sizeof(DirectStorage));
    ASSERT(pDirectStorage);
    pDirectStorage->pRenderer = pRenderer;
    pDirectStorage->pModule = module;
    pDirectStorage->pFactory = pFactory;

    *ppDirectStorage = pDirectStorage;
    return S_OK;
}

void d3d12_exitDirectStorage(DirectStorage* pDirectStorage)
{
    if (!pDirectStorage)
    {
        return;
    }

    SAFE_RELEASE(pDirectStorage->pFactory);
    if (pDirectStorage->pModule)
    {
        FreeLibrary(pDirectStorage->pModule);
    }

    SAFE_FREE(pDirectStorage);
}

HRESULT d3d12_addDirectStorageQueue(DirectStorage* pDirectStorage, const DirectStorageQueueDesc* pDesc, DirectStorageQueue** ppQueue)
{
    ASSERT(pDirectStorage);
    ASSERT(pDesc);
    ASSERT(ppQueue);

    *ppQueue = NULL;

    DSTORAGE_QUEUE_DESC queueDesc = {};
    queueDesc.SourceType = util_to_dstorage_source_type(pDesc->sourceType);
    queueDesc.Capacity = pDesc->capacity ? pDesc->capacity : DSTORAGE_MIN_QUEUE_CAPACITY;
    queueDesc.Priority = util_to_dstorage_priority(pDesc->priority);
    queueDesc.Name = pDesc->pName;
    queueDesc.Device = pDirectStorage->pRenderer->dx.pDevice;

    IDStorageQueue* pDxQueue = NULL;
    HRESULT         hr = pDirectStorage->pFactory->CreateQueue(&queueDesc, __uuidof(IDStorageQueue), (void**)&pDxQueue);
    if (FAILED(hr))
    {
        return hr;
    }

    DirectStorageQueue* pQueue = (DirectStorageQueue*)tf_calloc(1, sizeof(DirectStorageQueue));
    ASSERT(pQueue);
    pQueue->pQueue = pDxQueue;

    *ppQueue = pQueue;
    return S_OK;
}

void d3d12_removeDirectStorageQueue(DirectStorageQueue* pQueue)
{
    if (!pQueue)
    {
        return;
    }

    if (pQueue->pQueue)
    {
        pQueue->pQueue->Close();
    }
    SAFE_RELEASE(pQueue->pQueue);
    SAFE_FREE(pQueue);
}

HRESULT d3d12_openDirectStorageFile(DirectStorage* pDirectStorage, const wchar_t* pPath, DirectStorageFile** ppFile)
{
    ASSERT(pDirectStorage);
    ASSERT(pPath);
    ASSERT(ppFile);

    *ppFile = NULL;

    IDStorageFile* pDxFile = NULL;
    HRESULT        hr = pDirectStorage->pFactory->OpenFile(pPath, __uuidof(IDStorageFile), (void**)&pDxFile);
    if (FAILED(hr))
    {
        return hr;
    }

    DirectStorageFile* pFile = (DirectStorageFile*)tf_calloc(1, sizeof(DirectStorageFile));
    ASSERT(pFile);
    pFile->pFile = pDxFile;

    *ppFile = pFile;
    return S_OK;
}

void d3d12_closeDirectStorageFile(DirectStorageFile* pFile)
{
    if (!pFile)
    {
        return;
    }

    if (pFile->pFile)
    {
        pFile->pFile->Close();
    }
    SAFE_RELEASE(pFile->pFile);
    SAFE_FREE(pFile);
}

HRESULT d3d12_addDirectStorageStatusArray(DirectStorage* pDirectStorage, uint32_t capacity, const char* pName,
                                          DirectStorageStatusArray** ppStatusArray)
{
    ASSERT(pDirectStorage);
    ASSERT(ppStatusArray);

    *ppStatusArray = NULL;

    IDStorageStatusArray* pDxStatusArray = NULL;
    HRESULT hr = pDirectStorage->pFactory->CreateStatusArray(capacity, pName, __uuidof(IDStorageStatusArray), (void**)&pDxStatusArray);
    if (FAILED(hr))
    {
        return hr;
    }

    DirectStorageStatusArray* pStatusArray = (DirectStorageStatusArray*)tf_calloc(1, sizeof(DirectStorageStatusArray));
    ASSERT(pStatusArray);
    pStatusArray->pStatusArray = pDxStatusArray;

    *ppStatusArray = pStatusArray;
    return S_OK;
}

void d3d12_removeDirectStorageStatusArray(DirectStorageStatusArray* pStatusArray)
{
    if (!pStatusArray)
    {
        return;
    }

    SAFE_RELEASE(pStatusArray->pStatusArray);
    SAFE_FREE(pStatusArray);
}

bool d3d12_isDirectStorageStatusComplete(DirectStorageStatusArray* pStatusArray, uint32_t index)
{
    ASSERT(pStatusArray);
    return pStatusArray && pStatusArray->pStatusArray->IsComplete(index);
}

HRESULT d3d12_getDirectStorageStatus(DirectStorageStatusArray* pStatusArray, uint32_t index)
{
    ASSERT(pStatusArray);
    return pStatusArray ? pStatusArray->pStatusArray->GetHResult(index) : E_POINTER;
}

static void fill_dstorage_source(DSTORAGE_REQUEST* pDst, DirectStorageFile* pFile, const void* pMemory, uint64_t sourceOffset,
                                 uint32_t sourceSize)
{
    if (pFile)
    {
        pDst->Options.SourceType = DSTORAGE_REQUEST_SOURCE_FILE;
        pDst->Source.File.Source = pFile->pFile;
        pDst->Source.File.Offset = sourceOffset;
        pDst->Source.File.Size = sourceSize;
    }
    else
    {
        pDst->Options.SourceType = DSTORAGE_REQUEST_SOURCE_MEMORY;
        pDst->Source.Memory.Source = pMemory;
        pDst->Source.Memory.Size = sourceSize;
    }
}

void d3d12_directStorageEnqueueBufferRequest(DirectStorageQueue* pQueue, const DirectStorageBufferRequest* pRequest)
{
    ASSERT(pQueue);
    ASSERT(pRequest);
    ASSERT(pRequest->pBuffer);
    ASSERT(pRequest->pFile || pRequest->pMemory);

    DSTORAGE_REQUEST request = {};
    request.Options.CompressionFormat = util_to_dstorage_compression(pRequest->compressionFormat);
    request.Options.DestinationType = DSTORAGE_REQUEST_DESTINATION_BUFFER;
    fill_dstorage_source(&request, pRequest->pFile, pRequest->pMemory, pRequest->sourceOffset, pRequest->sourceSize);
    request.Destination.Buffer.Resource = pRequest->pBuffer->dx.pResource;
    request.Destination.Buffer.Offset = pRequest->destinationOffset;
    request.Destination.Buffer.Size = pRequest->destinationSize ? pRequest->destinationSize : pRequest->sourceSize;
    request.UncompressedSize = pRequest->uncompressedSize;
    request.CancellationTag = pRequest->cancellationTag;
    request.Name = pRequest->pName;

    pQueue->pQueue->EnqueueRequest(&request);
}

void d3d12_directStorageEnqueueTextureRequest(DirectStorageQueue* pQueue, const DirectStorageTextureRequest* pRequest)
{
    ASSERT(pQueue);
    ASSERT(pRequest);
    ASSERT(pRequest->pTexture);
    ASSERT(pRequest->pFile || pRequest->pMemory);

    DSTORAGE_REQUEST request = {};
    request.Options.CompressionFormat = util_to_dstorage_compression(pRequest->compressionFormat);
    request.Options.DestinationType = DSTORAGE_REQUEST_DESTINATION_TEXTURE_REGION;
    fill_dstorage_source(&request, pRequest->pFile, pRequest->pMemory, pRequest->sourceOffset, pRequest->sourceSize);
    request.Destination.Texture.Resource = pRequest->pTexture->dx.pResource;
    request.Destination.Texture.SubresourceIndex = pRequest->subresourceIndex;
    request.Destination.Texture.Region.left = pRequest->x;
    request.Destination.Texture.Region.top = pRequest->y;
    request.Destination.Texture.Region.front = pRequest->z;
    request.Destination.Texture.Region.right = pRequest->x + pRequest->width;
    request.Destination.Texture.Region.bottom = pRequest->y + pRequest->height;
    request.Destination.Texture.Region.back = pRequest->z + pRequest->depth;
    request.UncompressedSize = pRequest->uncompressedSize;
    request.CancellationTag = pRequest->cancellationTag;
    request.Name = pRequest->pName;

    pQueue->pQueue->EnqueueRequest(&request);
}

void d3d12_directStorageEnqueueStatus(DirectStorageQueue* pQueue, DirectStorageStatusArray* pStatusArray, uint32_t index)
{
    ASSERT(pQueue);
    ASSERT(pStatusArray);
    pQueue->pQueue->EnqueueStatus(pStatusArray->pStatusArray, index);
}

void d3d12_directStorageEnqueueSignal(DirectStorageQueue* pQueue, Fence* pFence, uint64_t value)
{
    ASSERT(pQueue);
    ASSERT(pFence);
    pQueue->pQueue->EnqueueSignal(pFence->dx.pFence, value);
}

void d3d12_directStorageSubmit(DirectStorageQueue* pQueue)
{
    ASSERT(pQueue);
    pQueue->pQueue->Submit();
}

void d3d12_addCmdPool(Renderer* pRenderer, const CmdPoolDesc* pDesc, CmdPool** ppCmdPool)
{
    // ASSERT that renderer is valid
    ASSERT(pRenderer);
    ASSERT(pDesc);
    ASSERT(ppCmdPool);

    // create one new CmdPool and add to renderer
    CmdPool* pCmdPool = (CmdPool*)tf_calloc(1, sizeof(CmdPool));
    ASSERT(pCmdPool);

    CHECK_HRESULT(
        pRenderer->dx.pDevice->CreateCommandAllocator(gDx12CmdTypeTranslator[pDesc->pQueue->type], IID_ARGS(&pCmdPool->pCmdAlloc)));

    pCmdPool->pQueue = pDesc->pQueue;

    *ppCmdPool = pCmdPool;
}

void d3d12_removeCmdPool(Renderer* pRenderer, CmdPool* pCmdPool)
{
    // check validity of given renderer and command pool
    ASSERT(pRenderer);
    ASSERT(pCmdPool);

    SAFE_RELEASE(pCmdPool->pCmdAlloc);
    SAFE_FREE(pCmdPool);
}

void d3d12_addCmd(Renderer* pRenderer, const CmdDesc* pDesc, Cmd** ppCmd)
{
    // verify that given pool is valid
    ASSERT(pRenderer);
    ASSERT(pDesc);
    ASSERT(ppCmd);

    // initialize to zero
    Cmd* pCmd = (Cmd*)tf_calloc_memalign(1, alignof(Cmd), sizeof(Cmd));
    ASSERT(pCmd);

    // set command pool of new command
    pCmd->dx.type = pDesc->pPool->pQueue->type;
    pCmd->pQueue = pDesc->pPool->pQueue;
    pCmd->pRenderer = pRenderer;

    pCmd->dx.pBoundHeaps[0] = pRenderer->dx.pCbvSrvUavHeaps[0];
    pCmd->dx.pBoundHeaps[1] = pRenderer->dx.pSamplerHeaps[0];

    pCmd->dx.pCmdPool = pDesc->pPool;

    uint32_t nodeMask = 0;

    if (QUEUE_TYPE_TRANSFER == pDesc->pPool->pQueue->type)
    {
        CHECK_HRESULT(hook_create_copy_cmd(pRenderer->dx.pDevice, nodeMask, pDesc->pPool->pCmdAlloc, pCmd));
    }
    else
    {
        ID3D12PipelineState* initialState = NULL;
        CHECK_HRESULT(pRenderer->dx.pDevice->CreateCommandList(nodeMask, gDx12CmdTypeTranslator[pCmd->dx.type], pDesc->pPool->pCmdAlloc,
                                                                initialState, __uuidof(pCmd->dx.pCmdList), (void**)&(pCmd->dx.pCmdList)));
    }

    // Command lists are addd in the recording state, but there is nothing
    // to record yet. The main loop expects it to be closed, so close it now.
    CHECK_HRESULT(pCmd->dx.pCmdList->Close());
    // CHECK_HRESULT(pCmd->dx.pCmdList->QueryInterface(IID_ARGS(&pCmd->dx.pBarrierCmdList)));

#ifdef ENABLE_GRAPHICS_DEBUG
    if (pDesc->pName)
        SetObjectName(pCmd->dx.pCmdList, pDesc->pName);
#endif // ENABLE_GRAPHICS_DEBUG

#if defined(ENABLE_GRAPHICS_DEBUG) && defined(_WINDOWS)
    pCmd->dx.pCmdList->QueryInterface(IID_ARGS(&pCmd->dx.pDebugCmdList));
#endif

    *ppCmd = pCmd;
}

void d3d12_removeCmd(Renderer* pRenderer, Cmd* pCmd)
{
    // verify that given command and pool are valid
    ASSERT(pRenderer);
    ASSERT(pCmd);

#if defined(ENABLE_GRAPHICS_DEBUG) && defined(_WINDOWS)
    SAFE_RELEASE(pCmd->dx.pDebugCmdList);
#endif
    // SAFE_RELEASE(pCmd->dx.pBarrierCmdList);

    if (QUEUE_TYPE_TRANSFER == pCmd->dx.type)
    {
        hook_remove_copy_cmd(pCmd);
    }
    else
    {
        SAFE_RELEASE(pCmd->dx.pCmdList);
    }

    SAFE_FREE(pCmd);
}

void d3d12_addCmd_n(Renderer* pRenderer, const CmdDesc* pDesc, uint32_t cmdCount, Cmd*** pppCmd)
{
    // verify that ***cmd is valid
    ASSERT(pRenderer);
    ASSERT(pDesc);
    ASSERT(cmdCount);
    ASSERT(pppCmd);

    Cmd** ppCmds = (Cmd**)tf_calloc(cmdCount, sizeof(Cmd*));
    ASSERT(ppCmds);

    // add n new cmds to given pool
    for (uint32_t i = 0; i < cmdCount; ++i)
    {
        ::addCmd(pRenderer, pDesc, &ppCmds[i]);
    }

    *pppCmd = ppCmds;
}

void d3d12_removeCmd_n(Renderer* pRenderer, uint32_t cmdCount, Cmd** ppCmds)
{
    // verify that given command list is valid
    ASSERT(ppCmds);

    // remove every given cmd in array
    for (uint32_t i = 0; i < cmdCount; ++i)
    {
        removeCmd(pRenderer, ppCmds[i]);
    }

    SAFE_FREE(ppCmds);
}

void d3d12_toggleVSync(Renderer* pRenderer, SwapChain** ppSwapChain)
{
    UNREF_PARAM(pRenderer);
    ASSERT(ppSwapChain);

    SwapChain* pSwapChain = *ppSwapChain;
    // set descriptor vsync boolean
    pSwapChain->enableVsync = !pSwapChain->enableVsync;
#if !defined(XBOX)
    if (!pSwapChain->enableVsync)
    {
        pSwapChain->dx.flags |= DXGI_PRESENT_ALLOW_TEARING;
    }
    else
    {
        pSwapChain->dx.flags &= ~DXGI_PRESENT_ALLOW_TEARING;
    }
#endif

    // toggle vsync present flag (this can go up to 4 but we don't need to refresh on nth vertical sync)
    pSwapChain->dx.syncInterval = (pSwapChain->dx.syncInterval + 1) % 2;
}

bool d3d12_getSwapchainFormatSupport(Renderer* pRenderer, Queue* pQueue, hz::Format format, ColorSpace colorspace);

void d3d12_addSwapChain(Renderer* pRenderer, const SwapChainDesc* pDesc, SwapChain** ppSwapChain)
{
    ASSERT(pRenderer);
    ASSERT(pDesc);
    ASSERT(ppSwapChain);
    ASSERT(pDesc->imageCount <= MAX_SWAPCHAIN_IMAGES);

    LOGF(LogLevel::eINFO, "Adding D3D12 swapchain @ %ux%u", pDesc->width, pDesc->height);

    SwapChain* pSwapChain = (SwapChain*)tf_calloc(1, sizeof(SwapChain) + pDesc->imageCount * sizeof(RenderTarget*));
    ASSERT(pSwapChain);
    pSwapChain->ppRenderTargets = (RenderTarget**)(pSwapChain + 1);
    ASSERT(pSwapChain->ppRenderTargets);

    pSwapChain->colorSpace = pDesc->colorSpace;
    pSwapChain->format = pDesc->colorFormat;

#if !defined(XBOX)
    pSwapChain->dx.syncInterval = pDesc->enableVsync ? 1 : 0;

    DXGI_SWAP_CHAIN_DESC1 desc = {};
    desc.Width = pDesc->width;
    desc.Height = pDesc->height;
    desc.Format = util_to_dx12_swapchain_format(pDesc->colorFormat);
    desc.Stereo = false;
    desc.SampleDesc.Count = 1; // If multisampling is needed, we'll resolve it later
    desc.SampleDesc.Quality = 0;
    desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.BufferCount = pDesc->imageCount;
    desc.Scaling = DXGI_SCALING_STRETCH;
    desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    desc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
    desc.Flags = 0;

    BOOL allowTearing = FALSE;
    pRenderer->pContext->dx.pDXGIFactory->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING, &allowTearing, sizeof(allowTearing));
    desc.Flags |= allowTearing ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0;

    pSwapChain->dx.flags |= (!pDesc->enableVsync && allowTearing) ? DXGI_PRESENT_ALLOW_TEARING : 0;

    IDXGISwapChain1* swapchain;

    HWND hwnd = (HWND)pDesc->windowHandle.window;

    CHECK_HRESULT(pRenderer->pContext->dx.pDXGIFactory->CreateSwapChainForHwnd(pDesc->ppPresentQueues[0]->dx.pQueue, hwnd, &desc, NULL,
                                                                                NULL, &swapchain));

    CHECK_HRESULT(pRenderer->pContext->dx.pDXGIFactory->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER));

    CHECK_HRESULT(swapchain->QueryInterface(IID_ARGS(&pSwapChain->dx.pSwapChain)));
    swapchain->Release();

    ID3D12Resource** buffers = (ID3D12Resource**)alloca(pDesc->imageCount * sizeof(ID3D12Resource*));

    // Create rendertargets from swapchain
    for (uint32_t i = 0; i < pDesc->imageCount; ++i)
    {
        CHECK_HRESULT(pSwapChain->dx.pSwapChain->GetBuffer(i, IID_ARGS(&buffers[i])));
    }

    DXGI_COLOR_SPACE_TYPE colorSpace = util_to_dx12_colorspace(pDesc->colorSpace);
    UINT                  colorSpaceSupport = 0;
    CHECK_HRESULT(pSwapChain->dx.pSwapChain->CheckColorSpaceSupport(colorSpace, &colorSpaceSupport));
    if ((colorSpaceSupport & DXGI_SWAP_CHAIN_COLOR_SPACE_SUPPORT_FLAG_PRESENT) == DXGI_SWAP_CHAIN_COLOR_SPACE_SUPPORT_FLAG_PRESENT)
    {
        pSwapChain->dx.pSwapChain->SetColorSpace1(colorSpace);
    }
#endif

    RenderTargetDesc descColor = {};
    descColor.width = pDesc->width;
    descColor.height = pDesc->height;
    descColor.depth = 1;
    descColor.arraySize = 1;
    descColor.format = pDesc->colorFormat;
    descColor.clearValue = pDesc->colorClearValue;
    descColor.sampleCount = SAMPLE_COUNT_1;
    descColor.sampleQuality = 0;
    descColor.pNativeHandle = NULL;
    descColor.flags = TEXTURE_CREATION_FLAG_ALLOW_DISPLAY_TARGET;
    descColor.startState = RESOURCE_STATE_PRESENT;
#if defined(XBOX)
    descColor.flags |= TEXTURE_CREATION_FLAG_OWN_MEMORY_BIT;
    pSwapChain->dx.pPresentQueue = pDesc->presentQueueCount ? pDesc->ppPresentQueues[0] : NULL;
#endif

    for (uint32_t i = 0; i < pDesc->imageCount; ++i)
    {
#if !defined(XBOX)
        descColor.pNativeHandle = (void*)buffers[i];
#endif
        ::addRenderTarget(pRenderer, &descColor, &pSwapChain->ppRenderTargets[i]);
    }

    pSwapChain->imageCount = pDesc->imageCount;
    pSwapChain->enableVsync = pDesc->enableVsync;

    *ppSwapChain = pSwapChain;
}

void d3d12_removeSwapChain(Renderer* pRenderer, SwapChain* pSwapChain)
{
#if defined(XBOX)
    hook_queue_present(pSwapChain->dx.pPresentQueue, NULL, 0);
#endif

    for (uint32_t i = 0; i < pSwapChain->imageCount; ++i)
    {
        ID3D12Resource* resource = pSwapChain->ppRenderTargets[i]->pTexture->dx.pResource;
        removeRenderTarget(pRenderer, pSwapChain->ppRenderTargets[i]);
#if !defined(XBOX)
        SAFE_RELEASE(resource);
#else
        (void)resource;
#endif
    }

#if !defined(XBOX)
    SAFE_RELEASE(pSwapChain->dx.pSwapChain);
#endif
    SAFE_FREE(pSwapChain);
}

void d3d12_addResourceHeap(Renderer* pRenderer, const ResourceHeapDesc* pDesc, ResourceHeap** ppHeap)
{
    ASSERT(pRenderer);
    ASSERT(pDesc);
    ASSERT(ppHeap);

    uint64_t allocationSize = pDesc->size;

    if ((pDesc->descriptors & DESCRIPTOR_TYPE_UNIFORM_BUFFER))
    {
        allocationSize = round_up_64(allocationSize, pRenderer->pGpu->settings.uniformBufferAlignment);
    }

    ResourceMemoryUsage memoryUsage = pDesc->memoryUsage;
    if (memoryUsage == RESOURCE_MEMORY_USAGE_GPU_UPLOAD && !pRenderer->pGpu->settings.gpuUploadHeapSupported)
    {
        ASSERTMSG(false, "GPU_UPLOAD/ReBAR heap requested for '%s' but unsupported.", pDesc->pName ? pDesc->pName : "<unnamed>");
    }

    D3D12_HEAP_DESC heapDesc = {};
    heapDesc.SizeInBytes = allocationSize;
    heapDesc.Alignment = pDesc->alignment;
    heapDesc.Properties.Type = util_to_heap_type(memoryUsage);
    heapDesc.Flags = util_to_heap_flags(pDesc->flags);

    heapDesc.Properties.CreationNodeMask = 1;
    heapDesc.Properties.VisibleNodeMask = 1;

    // Special heap flags
    hook_modify_heap_flags(pDesc->descriptors, &heapDesc.Flags);

    ID3D12Heap* pDxHeap = NULL;
    CHECK_HRESULT(pRenderer->dx.pDevice->CreateHeap(&heapDesc, IID_ARGS(&pDxHeap)));
    ASSERT(pDxHeap);

    SetObjectName(pDxHeap, pDesc->pName);

    ResourceHeap* pHeap = (ResourceHeap*)tf_calloc(1, sizeof(ResourceHeap));
    pHeap->dx.pHeap = pDxHeap;
    pHeap->size = pDesc->size;

#if defined(ENABLE_TRACY_MEMORY)
    pHeap->memoryTrackingPool = D3D12_MEMORY_TRACKING_POOL_EXPLICIT_HEAP;
    d3d12_track_gpu_alloc(pDxHeap, allocationSize, pHeap->memoryTrackingPool);
#endif

#if defined(XBOX)
    {
        D3D12_RESOURCE_DESC resDesc = {};
        resDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        resDesc.Alignment = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;
        resDesc.Width = 16;
        resDesc.Height = 1;
        resDesc.DepthOrArraySize = 1;
        resDesc.MipLevels = 1;
        resDesc.Format = DXGI_FORMAT_UNKNOWN;
        resDesc.SampleDesc.Count = 1;
        resDesc.SampleDesc.Quality = 0;
        resDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        resDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

        ID3D12Resource* resource = NULL;
        CHECK_HRESULT(pRenderer->dx.pDevice->CreatePlacedResource(pDxHeap,
                                                                   0, // AllocationLocalOffset
                                                                   &resDesc, D3D12_RESOURCE_STATE_COMMON,
                                                                   NULL, // pOptimizedClearValue
                                                                   IID_ARGS(&resource)));

        ASSERT(resource);
        pHeap->dx.ptr = resource->GetGPUVirtualAddress();

        // We just needed to create this resource to get the address to the memory
        resource->Release();
    }
#endif

    *ppHeap = pHeap;
}

void d3d12_removeResourceHeap(Renderer* pRenderer, ResourceHeap* pHeap)
{
    UNREF_PARAM(pRenderer);

#if defined(ENABLE_TRACY_MEMORY)
    d3d12_track_gpu_free(pHeap->dx.pHeap, pHeap->memoryTrackingPool);
#endif
    SAFE_RELEASE(pHeap->dx.pHeap);
    SAFE_FREE(pHeap);
}

void d3d12_getBufferSizeAlign(Renderer* pRenderer, const BufferDesc* pDesc, ResourceSizeAlign* pOut)
{
    UNREF_PARAM(pRenderer);
    ASSERT(pRenderer);
    ASSERT(pDesc);
    ASSERT(pOut);
    DECLARE_ZERO(D3D12_RESOURCE_DESC, desc);
    InitializeBufferDesc(pRenderer, pDesc, &desc);

    const UINT                           visibleMask = 1;
    const D3D12_RESOURCE_ALLOCATION_INFO allocInfo = pRenderer->dx.pDevice->GetResourceAllocationInfo(visibleMask, 1, &desc);

    pOut->size = allocInfo.SizeInBytes;
    pOut->alignment = allocInfo.Alignment;
}

void d3d12_getTextureSizeAlign(Renderer* pRenderer, const TextureDesc* pDesc, ResourceSizeAlign* pOut)
{
    UNREF_PARAM(pRenderer);
    ASSERT(pRenderer);
    ASSERT(pDesc);
    ASSERT(pOut);
    DECLARE_ZERO(D3D12_RESOURCE_DESC, desc);
    InitializeTextureDesc(pRenderer, pDesc, &desc, NULL);

    const UINT                           visibleMask = 1;
    const D3D12_RESOURCE_ALLOCATION_INFO allocInfo = pRenderer->dx.pDevice->GetResourceAllocationInfo(visibleMask, 1, &desc);

    pOut->size = allocInfo.SizeInBytes;
    pOut->alignment = allocInfo.Alignment;
}

void d3d12_addBuffer(Renderer* pRenderer, const BufferDesc* pDesc, Buffer** ppBuffer)
{
    // verify renderer validity
    ASSERT(pRenderer);
    // verify adding at least 1 buffer
    ASSERT(pDesc);
    ASSERT(ppBuffer);
    ASSERT(pDesc->size > 0);

    // initialize to zero
    Buffer* pBuffer = (Buffer*)tf_calloc_memalign(1, alignof(Buffer), sizeof(Buffer));
    pBuffer->dx.descriptors = D3D12_DESCRIPTOR_ID_NONE;
    ASSERT(ppBuffer);

    // add to renderer

    DECLARE_ZERO(D3D12_RESOURCE_DESC, desc);
    InitializeBufferDesc(pRenderer, pDesc, &desc);

    ResourceMemoryUsage memoryUsage = pDesc->memoryUsage;
    if (memoryUsage == RESOURCE_MEMORY_USAGE_GPU_UPLOAD && !pRenderer->pGpu->settings.gpuUploadHeapSupported)
    {
        ASSERTMSG(false, "GPU_UPLOAD/ReBAR buffer requested for '%s' but unsupported.", pDesc->pName ? pDesc->pName : "<unnamed>");
    }

    D3D12MA::ALLOCATION_DESC alloc_desc = {};
    alloc_desc.HeapType = util_to_heap_type(memoryUsage);

    if (pDesc->flags & BUFFER_CREATION_FLAG_OWN_MEMORY_BIT)
    {
        alloc_desc.Flags |= D3D12MA::ALLOCATION_FLAG_COMMITTED;
    }

    UINT creationNodeMask = 1;
    UINT visibleNodeMask = 1;

    // Special heap flags
    hook_modify_heap_flags(pDesc->descriptors, &alloc_desc.ExtraHeapFlags);

    ResourceState start_state = pDesc->startState;
    if (memoryUsage == RESOURCE_MEMORY_USAGE_CPU_TO_GPU || memoryUsage == RESOURCE_MEMORY_USAGE_CPU_ONLY ||
        memoryUsage == RESOURCE_MEMORY_USAGE_GPU_UPLOAD)
    {
        start_state = RESOURCE_STATE_GENERIC_READ;
    }
    else if (memoryUsage == RESOURCE_MEMORY_USAGE_GPU_TO_CPU)
    {
        start_state = RESOURCE_STATE_COPY_DEST;
    }

    D3D12_RESOURCE_STATES res_states = util_to_dx12_resource_state(start_state);

    // Create resource
    if (SUCCEEDED(hook_add_special_resource(pRenderer, &desc, NULL, res_states, pDesc->flags, pBuffer)))
    {
        LOGF(LogLevel::eINFO, "Allocated memory in device-specific RAM");
#if defined(ENABLE_TRACY_MEMORY)
        if (pBuffer->dx.pResource)
        {
            pBuffer->memoryTrackingMode = D3D12_MEMORY_TRACKING_RESOURCE;
            pBuffer->memoryTrackingPool = d3d12_memory_pool_from_usage(memoryUsage);
            d3d12_track_gpu_alloc(pBuffer->dx.pResource, desc.Width, pBuffer->memoryTrackingPool);
        }
#endif
    }
    // #TODO: This is not at all good but seems like virtual textures are using this
    // Remove as soon as possible
    else if (D3D12_HEAP_TYPE_DEFAULT != alloc_desc.HeapType && D3D12_HEAP_TYPE_GPU_UPLOAD != alloc_desc.HeapType &&
             (desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS))
    {
        ASSERT(!pDesc->pPlacement);
        LOGF(eWARNING, "Creating RWBuffer in Upload heap. GPU access might be slower than default");
        D3D12_HEAP_PROPERTIES heapProps = {};
        heapProps.Type = D3D12_HEAP_TYPE_CUSTOM;
        heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_WRITE_COMBINE;
        heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_L0;
        heapProps.VisibleNodeMask = visibleNodeMask;
        heapProps.CreationNodeMask = creationNodeMask;
        CHECK_HRESULT(pRenderer->dx.pDevice->CreateCommittedResource(&heapProps, alloc_desc.ExtraHeapFlags, &desc, res_states, NULL,
                                                                      IID_ARGS(&pBuffer->dx.pResource)));
#if defined(ENABLE_TRACY_MEMORY)
        pBuffer->memoryTrackingMode = D3D12_MEMORY_TRACKING_RESOURCE;
        pBuffer->memoryTrackingPool = d3d12_memory_pool_from_heap_type(heapProps.Type);
        d3d12_track_gpu_alloc(pBuffer->dx.pResource, desc.Width, pBuffer->memoryTrackingPool);
#endif
    }
    else
    {
        if (pDesc->pPlacement)
        {
            CHECK_HRESULT(hook_add_placed_resource(pRenderer, pDesc->pPlacement, &desc, NULL, res_states, &pBuffer->dx.pResource));
        }
        else
        {
            CHECK_HRESULT(pRenderer->dx.pResourceAllocator->CreateResource(&alloc_desc, &desc, res_states, NULL, &pBuffer->dx.pAllocation,
                                                                            IID_ARGS(&pBuffer->dx.pResource)));
#if defined(ENABLE_TRACY_MEMORY)
            pBuffer->memoryTrackingMode = D3D12_MEMORY_TRACKING_D3D12MA;
            pBuffer->memoryTrackingPool = d3d12_memory_pool_from_heap_type(alloc_desc.HeapType);
            d3d12_track_d3d12ma_alloc(pBuffer->dx.pAllocation, pDesc->pName, pBuffer->memoryTrackingPool);
#endif
        }
    }

    if (memoryUsage != RESOURCE_MEMORY_USAGE_GPU_ONLY && pDesc->flags & BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT)
    {
        pBuffer->dx.pResource->Map(0, NULL, &pBuffer->pCpuMappedAddress);
    }

    pBuffer->dx.gpuAddress = pBuffer->dx.pResource->GetGPUVirtualAddress();
#if defined(XBOX)
    pBuffer->pCpuMappedAddress = (void*)pBuffer->dx.gpuAddress;
#endif

    if (!(pDesc->flags & BUFFER_CREATION_FLAG_NO_DESCRIPTOR_VIEW_CREATION))
    {
        DescriptorHeap* pHeap = pRenderer->dx.pCPUDescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV];
        uint32_t        handleCount = ((pDesc->descriptors & DESCRIPTOR_TYPE_UNIFORM_BUFFER) ? 1 : 0) +
                               ((pDesc->descriptors & DESCRIPTOR_TYPE_BUFFER) ? 1 : 0) +
                               ((pDesc->descriptors & DESCRIPTOR_TYPE_RW_BUFFER) ? 1 : 0);
        pBuffer->dx.descriptors = consume_descriptor_handles(pHeap, handleCount);

        if (pDesc->descriptors & DESCRIPTOR_TYPE_UNIFORM_BUFFER)
        {
            pBuffer->dx.srvDescriptorOffset = 1;

            D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
            cbvDesc.BufferLocation = pBuffer->dx.gpuAddress;
            cbvDesc.SizeInBytes = (UINT)desc.Width;
            AddCbv(pRenderer, NULL, &cbvDesc, &pBuffer->dx.descriptors);
        }

        if (pDesc->descriptors & DESCRIPTOR_TYPE_BUFFER)
        {
            DxDescriptorID srv = pBuffer->dx.descriptors + pBuffer->dx.srvDescriptorOffset;
            pBuffer->dx.uavDescriptorOffset = pBuffer->dx.srvDescriptorOffset + 1;
            if (pDesc->format != hz::Format::UNDEFINED)
            {
                AddTypedBufferSrv(pRenderer, NULL, pBuffer->dx.pResource, pDesc->firstElement, pDesc->elementCount, pDesc->format,
                                  &srv);
            }
            else
            {
                const bool raw = DESCRIPTOR_TYPE_BUFFER_RAW == (pDesc->descriptors & DESCRIPTOR_TYPE_BUFFER_RAW);
                AddBufferSrv(pRenderer, NULL, pBuffer->dx.pResource, raw, pDesc->firstElement, pDesc->elementCount, pDesc->structStride,
                             &srv);
            }
        }

        if (pDesc->descriptors & DESCRIPTOR_TYPE_RW_BUFFER)
        {
            DxDescriptorID uav = pBuffer->dx.descriptors + pBuffer->dx.uavDescriptorOffset;
            if (pDesc->format != hz::Format::UNDEFINED)
            {
                AddTypedBufferUav(pRenderer, NULL, pBuffer->dx.pResource, pDesc->firstElement, pDesc->elementCount, pDesc->format,
                                  &uav);
            }
            else
            {
                const bool      raw = DESCRIPTOR_TYPE_RW_BUFFER_RAW == (pDesc->descriptors & DESCRIPTOR_TYPE_RW_BUFFER_RAW);
                ID3D12Resource* pCounterBuffer = pDesc->pCounterBuffer ? pDesc->pCounterBuffer->dx.pResource : NULL;
                AddBufferUav(pRenderer, NULL, pBuffer->dx.pResource, pCounterBuffer, 0, raw, pDesc->firstElement, pDesc->elementCount,
                             pDesc->structStride, &uav);
            }
        }
    }

    // Set name
    SetObjectName(pBuffer->dx.pResource, pDesc->pName);
#if defined(ENABLE_TRACY_MEMORY)
    if (pBuffer->memoryTrackingMode == D3D12_MEMORY_TRACKING_D3D12MA)
        d3d12_set_allocation_name(pBuffer->dx.pAllocation, pDesc->pName);
#endif

    pBuffer->size = (uint32_t)pDesc->size;
    pBuffer->memoryUsage = memoryUsage;
    pBuffer->descriptors = pDesc->descriptors;

    *ppBuffer = pBuffer;
}

void d3d12_removeBuffer(Renderer* pRenderer, Buffer* pBuffer)
{
    UNREF_PARAM(pRenderer);
    ASSERT(pRenderer);
    ASSERT(pBuffer);

    if (pBuffer->dx.descriptors != D3D12_DESCRIPTOR_ID_NONE)
    {
        uint32_t handleCount = ((pBuffer->descriptors & DESCRIPTOR_TYPE_UNIFORM_BUFFER) ? 1 : 0) +
                               ((pBuffer->descriptors & DESCRIPTOR_TYPE_BUFFER) ? 1 : 0) +
                               ((pBuffer->descriptors & DESCRIPTOR_TYPE_RW_BUFFER) ? 1 : 0);
        return_descriptor_handles(pRenderer->dx.pCPUDescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV], pBuffer->dx.descriptors,
                                  handleCount);
    }

#if !defined(XBOX)
    if (pBuffer->dx.markerBuffer)
    {
        SAFE_RELEASE(pBuffer->dx.pMarkerBufferHeap);
        VirtualFree(pBuffer->pCpuMappedAddress, 0, MEM_DECOMMIT);
    }
    else
#endif
    {
#if defined(ENABLE_TRACY_MEMORY)
        if (pBuffer->memoryTrackingMode == D3D12_MEMORY_TRACKING_D3D12MA)
        {
            d3d12_track_gpu_free(pBuffer->dx.pAllocation, pBuffer->memoryTrackingPool);
        }
#endif
        SAFE_RELEASE(pBuffer->dx.pAllocation);
    }
#if defined(ENABLE_TRACY_MEMORY)
    if (pBuffer->memoryTrackingMode == D3D12_MEMORY_TRACKING_RESOURCE)
    {
        d3d12_track_gpu_free(pBuffer->dx.pResource, pBuffer->memoryTrackingPool);
    }
#endif
    SAFE_RELEASE(pBuffer->dx.pResource);

    SAFE_FREE(pBuffer);
}

void d3d12_mapBuffer(Renderer* pRenderer, Buffer* pBuffer, ReadRange* pRange)
{
    UNREF_PARAM(pRenderer);
    ASSERT(pBuffer->memoryUsage != RESOURCE_MEMORY_USAGE_GPU_ONLY && "Trying to map non-cpu accessible resource");

    D3D12_RANGE range = { 0, pBuffer->size };
    if (pRange)
    {
        range.Begin += pRange->offset;
        range.End = range.Begin + pRange->size;
    }

    CHECK_HRESULT(pBuffer->dx.pResource->Map(0, &range, &pBuffer->pCpuMappedAddress));
}

void d3d12_unmapBuffer(Renderer* pRenderer, Buffer* pBuffer)
{
    UNREF_PARAM(pRenderer);
    ASSERT(pBuffer->memoryUsage != RESOURCE_MEMORY_USAGE_GPU_ONLY && "Trying to unmap non-cpu accessible resource");

    pBuffer->dx.pResource->Unmap(0, NULL);
    pBuffer->pCpuMappedAddress = NULL;
}

void d3d12_addTexture(Renderer* pRenderer, const TextureDesc* pDesc, Texture** ppTexture)
{
    ASSERT(pRenderer);
    ASSERT(pDesc && pDesc->width && pDesc->height && (pDesc->depth || pDesc->arraySize));
    if (pDesc->sampleCount > SAMPLE_COUNT_1 && pDesc->mipLevels > 1)
    {
        LOGF(LogLevel::eERROR, "Multi-Sampled textures cannot have mip maps");
        ASSERT(false);
        return;
    }

    // allocate new texture
    Texture* pTexture = (Texture*)tf_calloc_memalign(1, alignof(Texture), sizeof(Texture));
    pTexture->dx.descriptors = D3D12_DESCRIPTOR_ID_NONE;
    ASSERT(pTexture);

    if (pDesc->pNativeHandle)
    {
        pTexture->ownsImage = false;
        pTexture->dx.pResource = (ID3D12Resource*)pDesc->pNativeHandle;
    }
    else
    {
        pTexture->ownsImage = true;
    }

    // add to gpu
    D3D12_RESOURCE_DESC desc = {};

    DXGI_FORMAT dxFormat = (DXGI_FORMAT)TinyImageFormat_ToDXGI_FORMAT((TinyImageFormat)pDesc->format);

    DescriptorType descriptors = pDesc->descriptors;

    ASSERT(DXGI_FORMAT_UNKNOWN != dxFormat);

    if (NULL == pTexture->dx.pResource)
    {
        ResourceState actualStartState = pDesc->startState;
        InitializeTextureDesc(pRenderer, pDesc, &desc, &actualStartState);

        hook_modify_texture_resource_flags(pDesc->flags, &desc.Flags);

        DECLARE_ZERO(D3D12_CLEAR_VALUE, clearValue);
        clearValue.Format = dxFormat;
        if (desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL)
        {
            clearValue.DepthStencil.Depth = pDesc->clearValue.depth;
            clearValue.DepthStencil.Stencil = (UINT8)pDesc->clearValue.stencil;
        }
        else
        {
            clearValue.Color[0] = pDesc->clearValue.r;
            clearValue.Color[1] = pDesc->clearValue.g;
            clearValue.Color[2] = pDesc->clearValue.b;
            clearValue.Color[3] = pDesc->clearValue.a;
        }

        D3D12_CLEAR_VALUE*    pClearValue = NULL;
        D3D12_RESOURCE_STATES res_states = util_to_dx12_resource_state(actualStartState);

        if ((desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET) || (desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL))
        {
            pClearValue = &clearValue;
        }

        D3D12MA::ALLOCATION_DESC alloc_desc = {};
        alloc_desc.HeapType = D3D12_HEAP_TYPE_DEFAULT;
        if (pDesc->flags & TEXTURE_CREATION_FLAG_OWN_MEMORY_BIT)
            alloc_desc.Flags |= D3D12MA::ALLOCATION_FLAG_COMMITTED;

#if defined(XBOX)
        if (pDesc->flags & TEXTURE_CREATION_FLAG_ALLOW_DISPLAY_TARGET)
        {
            alloc_desc.ExtraHeapFlags |= D3D12_HEAP_FLAG_ALLOW_DISPLAY;
        }
#endif

        // Create resource
        if (SUCCEEDED(hook_add_special_resource(pRenderer, &desc, pClearValue, res_states, pDesc->flags, pTexture)))
        {
            LOGF(LogLevel::eINFO, "Allocated memory in special platform-specific RAM");
#if defined(ENABLE_TRACY_MEMORY)
            if (pTexture->dx.pResource)
            {
                const UINT                           visibleMask = 1;
                const D3D12_RESOURCE_ALLOCATION_INFO allocInfo = pRenderer->dx.pDevice->GetResourceAllocationInfo(visibleMask, 1, &desc);
                pTexture->memoryTrackingMode = D3D12_MEMORY_TRACKING_RESOURCE;
                pTexture->memoryTrackingPool = D3D12_MEMORY_TRACKING_POOL_DEFAULT;
                d3d12_track_gpu_alloc(pTexture->dx.pResource, allocInfo.SizeInBytes, pTexture->memoryTrackingPool);
            }
#endif
        }
        else
        {
            if (pDesc->pPlacement)
            {
                CHECK_HRESULT(
                    hook_add_placed_resource(pRenderer, pDesc->pPlacement, &desc, pClearValue, res_states, &pTexture->dx.pResource));
            }
            else
            {
                CHECK_HRESULT(pRenderer->dx.pResourceAllocator->CreateResource(
                    &alloc_desc, &desc, res_states, pClearValue, &pTexture->dx.pAllocation, IID_ARGS(&pTexture->dx.pResource)));
#if defined(ENABLE_TRACY_MEMORY)
                pTexture->memoryTrackingMode = D3D12_MEMORY_TRACKING_D3D12MA;
                pTexture->memoryTrackingPool = d3d12_memory_pool_from_heap_type(alloc_desc.HeapType);
                d3d12_track_d3d12ma_alloc(pTexture->dx.pAllocation, pDesc->pName, pTexture->memoryTrackingPool);
#endif
            }
        }
    }
    else
    {
        desc = pTexture->dx.pResource->GetDesc();
        dxFormat = desc.Format;
    }

    D3D12_SHADER_RESOURCE_VIEW_DESC  srvDesc = {};
    D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};

    switch (desc.Dimension)
    {
    case D3D12_RESOURCE_DIMENSION_TEXTURE1D:
    {
        if (desc.DepthOrArraySize > 1)
        {
            srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE1DARRAY;
            uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE1DARRAY;
            // SRV
            srvDesc.Texture1DArray.ArraySize = desc.DepthOrArraySize;
            srvDesc.Texture1DArray.FirstArraySlice = 0;
            srvDesc.Texture1DArray.MipLevels = desc.MipLevels;
            srvDesc.Texture1DArray.MostDetailedMip = 0;
            // UAV
            uavDesc.Texture1DArray.ArraySize = desc.DepthOrArraySize;
            uavDesc.Texture1DArray.FirstArraySlice = 0;
            uavDesc.Texture1DArray.MipSlice = 0;
        }
        else
        {
            srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE1D;
            uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE1D;
            // SRV
            srvDesc.Texture1D.MipLevels = desc.MipLevels;
            srvDesc.Texture1D.MostDetailedMip = 0;
            // UAV
            uavDesc.Texture1D.MipSlice = 0;
        }
        break;
    }
    case D3D12_RESOURCE_DIMENSION_TEXTURE2D:
    {
        if (DESCRIPTOR_TYPE_TEXTURE_CUBE == (descriptors & DESCRIPTOR_TYPE_TEXTURE_CUBE))
        {
            ASSERT(desc.DepthOrArraySize % 6 == 0);

            if (desc.DepthOrArraySize > 6)
            {
                srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBEARRAY;
                // SRV
                srvDesc.TextureCubeArray.First2DArrayFace = 0;
                srvDesc.TextureCubeArray.MipLevels = desc.MipLevels;
                srvDesc.TextureCubeArray.MostDetailedMip = 0;
                srvDesc.TextureCubeArray.NumCubes = desc.DepthOrArraySize / 6;
            }
            else
            {
                srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
                // SRV
                srvDesc.TextureCube.MipLevels = desc.MipLevels;
                srvDesc.TextureCube.MostDetailedMip = 0;
            }

            // UAV
            uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2DARRAY;
            uavDesc.Texture2DArray.ArraySize = desc.DepthOrArraySize;
            uavDesc.Texture2DArray.FirstArraySlice = 0;
            uavDesc.Texture2DArray.MipSlice = 0;
            uavDesc.Texture2DArray.PlaneSlice = 0;
        }
        else
        {
            if (desc.DepthOrArraySize > 1)
            {
                if (desc.SampleDesc.Count > SAMPLE_COUNT_1)
                {
                    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DMSARRAY;
                    // Cannot create a multisampled uav
                    // SRV
                    srvDesc.Texture2DMSArray.ArraySize = desc.DepthOrArraySize;
                    srvDesc.Texture2DMSArray.FirstArraySlice = 0;
                    // No UAV
                }
                else
                {
                    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
                    uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2DARRAY;
                    // SRV
                    srvDesc.Texture2DArray.ArraySize = desc.DepthOrArraySize;
                    srvDesc.Texture2DArray.FirstArraySlice = 0;
                    srvDesc.Texture2DArray.MipLevels = desc.MipLevels;
                    srvDesc.Texture2DArray.MostDetailedMip = 0;
                    srvDesc.Texture2DArray.PlaneSlice = 0;
                    // UAV
                    uavDesc.Texture2DArray.ArraySize = desc.DepthOrArraySize;
                    uavDesc.Texture2DArray.FirstArraySlice = 0;
                    uavDesc.Texture2DArray.MipSlice = 0;
                    uavDesc.Texture2DArray.PlaneSlice = 0;
                }
            }
            else
            {
                if (desc.SampleDesc.Count > SAMPLE_COUNT_1)
                {
                    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DMS;
                    // Cannot create a multisampled uav
                }
                else
                {
                    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
                    uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
                    // SRV
                    srvDesc.Texture2D.MipLevels = desc.MipLevels;
                    srvDesc.Texture2D.MostDetailedMip = 0;
                    srvDesc.Texture2D.PlaneSlice = 0;
                    // UAV
                    uavDesc.Texture2D.MipSlice = 0;
                    uavDesc.Texture2D.PlaneSlice = 0;
                }
            }
        }
        break;
    }
    case D3D12_RESOURCE_DIMENSION_TEXTURE3D:
    {
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE3D;
        uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE3D;
        // SRV
        srvDesc.Texture3D.MipLevels = desc.MipLevels;
        srvDesc.Texture3D.MostDetailedMip = 0;
        // UAV
        uavDesc.Texture3D.MipSlice = 0;
        uavDesc.Texture3D.FirstWSlice = 0;
        uavDesc.Texture3D.WSize = desc.DepthOrArraySize;
        break;
    }
    default:
        break;
    }

    DescriptorHeap* pHeap = pRenderer->dx.pCPUDescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV];
    uint32_t        handleCount = (descriptors & DESCRIPTOR_TYPE_TEXTURE) ? 1 : 0;
    handleCount += (descriptors & DESCRIPTOR_TYPE_RW_TEXTURE) ? pDesc->mipLevels : 0;
    pTexture->dx.descriptors = consume_descriptor_handles(pHeap, handleCount);

    if (descriptors & DESCRIPTOR_TYPE_TEXTURE)
    {
        ASSERT(srvDesc.ViewDimension != D3D12_SRV_DIMENSION_UNKNOWN);

        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.Format = util_to_dx12_srv_format(dxFormat);
        AddSrv(pRenderer, NULL, pTexture->dx.pResource, &srvDesc, &pTexture->dx.descriptors);
        ++pTexture->dx.uavStartIndex;
    }

    if (descriptors & DESCRIPTOR_TYPE_RW_TEXTURE)
    {
        uavDesc.Format = util_to_dx12_uav_format(dxFormat);
        for (uint32_t i = 0; i < pDesc->mipLevels; ++i)
        {
            DxDescriptorID handle = pTexture->dx.descriptors + i + pTexture->dx.uavStartIndex;

            uavDesc.Texture1DArray.MipSlice = i;
            if (desc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE3D)
                uavDesc.Texture3D.WSize = desc.DepthOrArraySize / (UINT)pow(2.0, int(i));
            AddUav(pRenderer, NULL, pTexture->dx.pResource, NULL, &uavDesc, &handle);
        }
    }

    SetObjectName(pTexture->dx.pResource, pDesc->pName);
#if defined(ENABLE_TRACY_MEMORY)
    d3d12_set_allocation_name(pTexture->dx.pAllocation, pDesc->pName);
#endif

    pTexture->dx.handleCount = handleCount;
    pTexture->mipLevels = pDesc->mipLevels;
    pTexture->width = pDesc->width;
    pTexture->height = pDesc->height;
    pTexture->depth = pDesc->depth;
    pTexture->uav = pDesc->descriptors & DESCRIPTOR_TYPE_RW_TEXTURE;
    pTexture->format = pDesc->format;
    pTexture->arraySizeMinusOne = pDesc->arraySize - 1;
    pTexture->sampleCount = pDesc->sampleCount;

    *ppTexture = pTexture;
}

void d3d12_removeTexture(Renderer* pRenderer, Texture* pTexture)
{
    ASSERT(pRenderer);
    ASSERT(pTexture);

    // return texture descriptors
    if (pTexture->dx.descriptors != D3D12_DESCRIPTOR_ID_NONE)
    {
        return_descriptor_handles(pRenderer->dx.pCPUDescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV], pTexture->dx.descriptors,
                                  pTexture->dx.handleCount);
    }

    if (pTexture->ownsImage)
    {
#if defined(ENABLE_TRACY_MEMORY)
        if (pTexture->memoryTrackingMode == D3D12_MEMORY_TRACKING_D3D12MA)
        {
            d3d12_track_gpu_free(pTexture->dx.pAllocation, pTexture->memoryTrackingPool);
        }
        else if (pTexture->memoryTrackingMode == D3D12_MEMORY_TRACKING_RESOURCE)
        {
            d3d12_track_gpu_free(pTexture->dx.pResource, pTexture->memoryTrackingPool);
        }
#endif
        SAFE_RELEASE(pTexture->dx.pAllocation);
        SAFE_RELEASE(pTexture->dx.pResource);
    }

    SAFE_FREE(pTexture);
}

void d3d12_addRenderTarget(Renderer* pRenderer, const RenderTargetDesc* pDesc, RenderTarget** ppRenderTarget)
{
    ASSERT(pRenderer);
    ASSERT(pDesc);
    ASSERT(ppRenderTarget);
    const bool isDepth = TinyImageFormat_HasDepth((TinyImageFormat)pDesc->format);
    ASSERT(!((isDepth) && (pDesc->descriptors & DESCRIPTOR_TYPE_RW_TEXTURE)) && "Cannot use depth stencil as UAV");

    ((RenderTargetDesc*)pDesc)->mipLevels = max(1U, pDesc->mipLevels);

    RenderTarget* pRenderTarget = (RenderTarget*)tf_calloc_memalign(1, alignof(RenderTarget), sizeof(RenderTarget));
    ASSERT(pRenderTarget);

    // add to gpu
    DXGI_FORMAT dxFormat = (DXGI_FORMAT)TinyImageFormat_ToDXGI_FORMAT((TinyImageFormat)pDesc->format);
    ASSERT(DXGI_FORMAT_UNKNOWN != dxFormat);

    TextureDesc textureDesc = {};
    textureDesc.arraySize = pDesc->arraySize;
    textureDesc.clearValue = pDesc->clearValue;
    textureDesc.depth = pDesc->depth;
    textureDesc.flags = pDesc->flags;
    textureDesc.format = pDesc->format;
    textureDesc.height = pDesc->height;
    textureDesc.mipLevels = pDesc->mipLevels;
    textureDesc.sampleCount = pDesc->sampleCount;
    textureDesc.sampleQuality = pDesc->sampleQuality;
    textureDesc.startState = pDesc->startState;

    if (!isDepth)
        textureDesc.startState |= RESOURCE_STATE_RENDER_TARGET;
    else
        textureDesc.startState |= RESOURCE_STATE_DEPTH_WRITE;

    textureDesc.width = pDesc->width;
    textureDesc.pNativeHandle = pDesc->pNativeHandle;
    textureDesc.pName = pDesc->pName;
    textureDesc.descriptors = pDesc->descriptors;
    if (!(pDesc->flags & TEXTURE_CREATION_FLAG_ALLOW_DISPLAY_TARGET))
    {
        // Create SRV by default for a render target
        textureDesc.descriptors |= DESCRIPTOR_TYPE_TEXTURE;
    }

    textureDesc.pPlacement = pDesc->pPlacement;

    addTexture(pRenderer, &textureDesc, &pRenderTarget->pTexture);

    D3D12_RESOURCE_DESC desc = pRenderTarget->pTexture->dx.pResource->GetDesc();

    uint32_t handleCount = desc.MipLevels;
    if ((pDesc->descriptors & DESCRIPTOR_TYPE_RENDER_TARGET_ARRAY_SLICES) ||
        (pDesc->descriptors & DESCRIPTOR_TYPE_RENDER_TARGET_DEPTH_SLICES))
        handleCount *= desc.DepthOrArraySize;
    handleCount += 1;

    DescriptorHeap* pHeap = isDepth ? pRenderer->dx.pCPUDescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_DSV]
                                    : pRenderer->dx.pCPUDescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_RTV];
    pRenderTarget->dx.descriptors = consume_descriptor_handles(pHeap, handleCount);

    if (isDepth)
        AddDsv(pRenderer, NULL, pRenderTarget->pTexture->dx.pResource, dxFormat, 0, (uint32_t)-1, &pRenderTarget->dx.descriptors);
    else
        AddRtv(pRenderer, NULL, pRenderTarget->pTexture->dx.pResource, dxFormat, 0, (uint32_t)-1, &pRenderTarget->dx.descriptors);

    for (uint32_t i = 0; i < desc.MipLevels; ++i)
    {
        if ((pDesc->descriptors & DESCRIPTOR_TYPE_RENDER_TARGET_ARRAY_SLICES) ||
            (pDesc->descriptors & DESCRIPTOR_TYPE_RENDER_TARGET_DEPTH_SLICES))
        {
            for (uint32_t j = 0; j < desc.DepthOrArraySize; ++j)
            {
                DxDescriptorID handle = pRenderTarget->dx.descriptors + (1 + i * desc.DepthOrArraySize + j);

                if (isDepth)
                    AddDsv(pRenderer, NULL, pRenderTarget->pTexture->dx.pResource, dxFormat, i, j, &handle);
                else
                    AddRtv(pRenderer, NULL, pRenderTarget->pTexture->dx.pResource, dxFormat, i, j, &handle);
            }
        }
        else
        {
            DxDescriptorID handle = pRenderTarget->dx.descriptors + 1 + i;

            if (isDepth)
                AddDsv(pRenderer, NULL, pRenderTarget->pTexture->dx.pResource, dxFormat, i, (uint32_t)-1, &handle);
            else
                AddRtv(pRenderer, NULL, pRenderTarget->pTexture->dx.pResource, dxFormat, i, (uint32_t)-1, &handle);
        }
    }

    pRenderTarget->width = pDesc->width;
    pRenderTarget->height = pDesc->height;
    pRenderTarget->arraySize = pDesc->arraySize;
    pRenderTarget->depth = pDesc->depth;
    pRenderTarget->mipLevels = pDesc->mipLevels;
    pRenderTarget->sampleCount = pDesc->sampleCount;
    pRenderTarget->sampleQuality = pDesc->sampleQuality;
    pRenderTarget->format = pDesc->format;
    pRenderTarget->clearValue = pDesc->clearValue;
    pRenderTarget->descriptors = pDesc->descriptors;

    *ppRenderTarget = pRenderTarget;
}

void d3d12_removeRenderTarget(Renderer* pRenderer, RenderTarget* pRenderTarget)
{
    bool const isDepth = TinyImageFormat_HasDepth((TinyImageFormat)pRenderTarget->format);

    removeTexture(pRenderer, pRenderTarget->pTexture);

    const uint32_t depthOrArraySize = (uint32_t)(pRenderTarget->arraySize * pRenderTarget->depth);
    uint32_t       handleCount = pRenderTarget->mipLevels;
    if ((pRenderTarget->descriptors & DESCRIPTOR_TYPE_RENDER_TARGET_ARRAY_SLICES) ||
        (pRenderTarget->descriptors & DESCRIPTOR_TYPE_RENDER_TARGET_DEPTH_SLICES))
        handleCount *= depthOrArraySize;
    handleCount += 1;

    !isDepth ? return_descriptor_handles(pRenderer->dx.pCPUDescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_RTV],
                                         pRenderTarget->dx.descriptors, handleCount)
             : return_descriptor_handles(pRenderer->dx.pCPUDescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_DSV],
                                         pRenderTarget->dx.descriptors, handleCount);

    SAFE_FREE(pRenderTarget);
}

void d3d12_addSampler(Renderer* pRenderer, const SamplerDesc* pDesc, Sampler** ppSampler)
{
    ASSERT(pRenderer);
    ASSERT(pRenderer->dx.pDevice);
    ASSERT(ppSampler);
    ASSERT(pDesc->compareFunc < MAX_COMPARE_MODES);

    // initialize to zero
    Sampler* pSampler = (Sampler*)tf_calloc_memalign(1, alignof(Sampler), sizeof(Sampler));
    pSampler->dx.descriptor = D3D12_DESCRIPTOR_ID_NONE;
    ASSERT(pSampler);

    // default sampler lod values
    // used if not overriden by setLodRange or not Linear mipmaps
    float minSamplerLod = 0;
    float maxSamplerLod = pDesc->mipMapMode == MIPMAP_MODE_LINEAR ? D3D12_FLOAT32_MAX : 0;
    // user provided lods
    if (pDesc->setLodRange)
    {
        minSamplerLod = pDesc->minLod;
        maxSamplerLod = pDesc->maxLod;
    }

    D3D12_SAMPLER_DESC desc = {};
    // add sampler to gpu
    desc.Filter = util_to_dx12_filter(pDesc->minFilter, pDesc->magFilter, pDesc->mipMapMode, pDesc->maxAnisotropy > 0.0f,
                                      (pDesc->compareFunc != CMP_NEVER ? true : false));
    desc.AddressU = util_to_dx12_texture_address_mode(pDesc->addressU);
    desc.AddressV = util_to_dx12_texture_address_mode(pDesc->addressV);
    desc.AddressW = util_to_dx12_texture_address_mode(pDesc->addressW);
    desc.MipLODBias = pDesc->mipLodBias;
    desc.MaxAnisotropy = max((UINT)pDesc->maxAnisotropy, 1U);
    desc.ComparisonFunc = gDx12ComparisonFuncTranslator[pDesc->compareFunc];
    desc.BorderColor[0] = 0.0f;
    desc.BorderColor[1] = 0.0f;
    desc.BorderColor[2] = 0.0f;
    desc.BorderColor[3] = 0.0f;
    desc.MinLOD = minSamplerLod;
    desc.MaxLOD = maxSamplerLod;

    pSampler->dx.desc = desc;
    AddSampler(pRenderer, NULL, &pSampler->dx.desc, &pSampler->dx.descriptor);

    *ppSampler = pSampler;
}

void d3d12_removeSampler(Renderer* pRenderer, Sampler* pSampler)
{
    ASSERT(pRenderer);
    ASSERT(pSampler);

    // Nop op
    return_descriptor_handles(pRenderer->dx.pCPUDescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER], pSampler->dx.descriptor, 1);

    SAFE_FREE(pSampler);
}
/************************************************************************/
// Shader Functions
/************************************************************************/
void d3d12_addShaderBinary(Renderer* pRenderer, const BinaryShaderDesc* pDesc, Shader** ppShaderProgram)
{
    ASSERT(pRenderer);
    ASSERT(pDesc && pDesc->stages);
    ASSERT(ppShaderProgram);

    size_t totalSize = sizeof(Shader);
    totalSize += sizeof(PipelineReflection);

    uint32_t reflectionCount = 0;
    for (uint32_t i = 0; i < SHADER_STAGE_COUNT; ++i)
    {
        ShaderStage                  stage_mask = (ShaderStage)(1 << i);
        const BinaryShaderStageDesc* pStage = NULL;
        if (stage_mask == (pDesc->stages & stage_mask))
        {
            switch (stage_mask)
            {
            case SHADER_STAGE_VERT:
                pStage = &pDesc->vert;
                break;
            case SHADER_STAGE_HULL:
                pStage = &pDesc->hull;
                break;
            case SHADER_STAGE_DOMN:
                pStage = &pDesc->domain;
                break;
            case SHADER_STAGE_GEOM:
                pStage = &pDesc->geom;
                break;
            case SHADER_STAGE_FRAG:
                pStage = &pDesc->frag;
                break;
            case SHADER_STAGE_COMP:
                pStage = &pDesc->comp;
                break;
            default:
                LOGF(LogLevel::eERROR, "Unknown shader stage %i", stage_mask);
                break;
            }

            totalSize += sizeof(ID3DBlob*);
            totalSize += sizeof(LPCWSTR);
            totalSize += (strlen(pStage->pEntryPoint) + 1) * sizeof(WCHAR); //-V522
            ++reflectionCount;
        }
    }

    Shader* pShaderProgram = (Shader*)tf_calloc(1, totalSize);
    ASSERT(pShaderProgram);

    pShaderProgram->pReflection = (PipelineReflection*)(pShaderProgram + 1); //-V1027
    pShaderProgram->dx.pShaderBlobs = (IDxcBlobEncoding**)(pShaderProgram->pReflection + 1);
    pShaderProgram->dx.pEntryNames = (LPCWSTR*)(pShaderProgram->dx.pShaderBlobs + reflectionCount);
    pShaderProgram->stages = pDesc->stages;

    uint8_t* mem = (uint8_t*)(pShaderProgram->dx.pEntryNames + reflectionCount);

    reflectionCount = 0;

    for (uint32_t i = 0; i < SHADER_STAGE_COUNT; ++i)
    {
        ShaderStage                  stage_mask = (ShaderStage)(1 << i);
        const BinaryShaderStageDesc* pStage = NULL;
        if (stage_mask == (pShaderProgram->stages & stage_mask))
        {
            switch (stage_mask)
            {
            case SHADER_STAGE_VERT:
                pStage = &pDesc->vert;
                break;
            case SHADER_STAGE_HULL:
                pStage = &pDesc->hull;
                break;
            case SHADER_STAGE_DOMN:
                pStage = &pDesc->domain;
                break;
            case SHADER_STAGE_GEOM:
                pStage = &pDesc->geom;
                break;
            case SHADER_STAGE_FRAG:
                pStage = &pDesc->frag;
                break;
            case SHADER_STAGE_COMP:
                pStage = &pDesc->comp;
                break;

            default:
                LOGF(LogLevel::eERROR, "Unknown shader stage %i", stage_mask);
                break;
            }

            IDxcUtils* pUtils;
            CHECK_HRESULT(DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&pUtils)));
            pUtils->CreateBlob(pStage->pByteCode, pStage->byteCodeSize, DXC_CP_ACP,
                               &pShaderProgram->dx.pShaderBlobs[reflectionCount]); //-V522
            pUtils->Release();

            d3d12_createShaderReflection((uint8_t*)(pShaderProgram->dx.pShaderBlobs[reflectionCount]->GetBufferPointer()),
                                         (uint32_t)pShaderProgram->dx.pShaderBlobs[reflectionCount]->GetBufferSize(), stage_mask,
                                         &pShaderProgram->pReflection->stageReflections[reflectionCount]);

            WCHAR* entryPointName = (WCHAR*)mem;
            mbstowcs((WCHAR*)entryPointName, pStage->pEntryPoint, strlen(pStage->pEntryPoint));
            pShaderProgram->dx.pEntryNames[reflectionCount] = entryPointName;
            mem += (strlen(pStage->pEntryPoint) + 1) * sizeof(WCHAR);

            reflectionCount++;
        }
    }

    createPipelineReflection(pShaderProgram->pReflection->stageReflections, reflectionCount, pShaderProgram->pReflection);

    *ppShaderProgram = pShaderProgram;
}

void d3d12_addShaderSource(Renderer* pRenderer, const ShaderSrcDesc* pDesc, Shader** ppShaderProgram)
{
    ASSERT(pRenderer);
    ASSERT(pDesc && pDesc->stages);
    ASSERT(ppShaderProgram);

    // Compile HLSL source contained in BinaryShaderDesc into DXIL blobs first.
    IDxcUtils*          pUtils = NULL;
    IDxcLibrary*        pLibrary = NULL;
    IDxcCompiler*       pCompiler = NULL;
    IDxcIncludeHandler* pIncludeHandler = NULL;

    CHECK_HRESULT(DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&pUtils)));
    CHECK_HRESULT(DxcCreateInstance(CLSID_DxcLibrary, IID_PPV_ARGS(&pLibrary)));
    CHECK_HRESULT(DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&pCompiler)));
    CHECK_HRESULT(pLibrary->CreateIncludeHandler(&pIncludeHandler));

    size_t totalSize = sizeof(Shader);
    totalSize += sizeof(PipelineReflection);

    uint32_t reflectionCount = 0;
    for (uint32_t i = 0; i < SHADER_STAGE_COUNT; ++i)
    {
        ShaderStage               stage_mask = (ShaderStage)(1 << i);
        const ShaderSrcStageDesc* pStage = NULL;
        if (stage_mask == (pDesc->stages & stage_mask))
        {
            switch (stage_mask)
            {
            case SHADER_STAGE_VERT:
                pStage = &pDesc->vert;
                break;
            case SHADER_STAGE_HULL:
                pStage = &pDesc->hull;
                break;
            case SHADER_STAGE_DOMN:
                pStage = &pDesc->domain;
                break;
            case SHADER_STAGE_GEOM:
                pStage = &pDesc->geom;
                break;
            case SHADER_STAGE_FRAG:
                pStage = &pDesc->frag;
                break;
            case SHADER_STAGE_COMP:
                pStage = &pDesc->comp;
                break;
            default:
                LOGF(LogLevel::eERROR, "Unknown shader stage %i", stage_mask);
                break;
            }

            totalSize += sizeof(ID3DBlob*);
            totalSize += sizeof(LPCWSTR);
            totalSize += (strlen(pStage->pEntryPoint) + 1) * sizeof(WCHAR); //-V522
            ++reflectionCount;
        }
    }

    Shader* pShaderProgram = (Shader*)tf_calloc(1, totalSize);
    ASSERT(pShaderProgram);

    pShaderProgram->pReflection = (PipelineReflection*)(pShaderProgram + 1); //-V1027
    pShaderProgram->dx.pShaderBlobs = (IDxcBlobEncoding**)(pShaderProgram->pReflection + 1);
    pShaderProgram->dx.pEntryNames = (LPCWSTR*)(pShaderProgram->dx.pShaderBlobs + reflectionCount);
    pShaderProgram->stages = pDesc->stages;

    uint8_t* mem = (uint8_t*)(pShaderProgram->dx.pEntryNames + reflectionCount);

    reflectionCount = 0;

    for (uint32_t i = 0; i < SHADER_STAGE_COUNT; ++i)
    {
        ShaderStage               stage_mask = (ShaderStage)(1 << i);
        const ShaderSrcStageDesc* pStage = NULL;
        if (stage_mask == (pShaderProgram->stages & stage_mask))
        {
            switch (stage_mask)
            {
            case SHADER_STAGE_VERT:
                pStage = &pDesc->vert;
                break;
            case SHADER_STAGE_HULL:
                pStage = &pDesc->hull;
                break;
            case SHADER_STAGE_DOMN:
                pStage = &pDesc->domain;
                break;
            case SHADER_STAGE_GEOM:
                pStage = &pDesc->geom;
                break;
            case SHADER_STAGE_FRAG:
                pStage = &pDesc->frag;
                break;
            case SHADER_STAGE_COMP:
                pStage = &pDesc->comp;
                break;

            default:
                LOGF(LogLevel::eERROR, "Unknown shader stage %i", stage_mask);
                break;
            }

            // Create source blob from HLSL text.
            IDxcBlobEncoding* pSourceBlob = NULL;
            CHECK_HRESULT(
                pLibrary->CreateBlobWithEncodingOnHeapCopy((LPCVOID)pStage->pByteCode, pStage->byteCodeSize, DXC_CP_ACP, &pSourceBlob));

            // Convert entry point to wide string.
            wchar_t entryPointWide[128] = {};
            if (pStage->pEntryPoint)
            {
                size_t converted = 0;
                mbstowcs_s(&converted, entryPointWide, TF_ARRAY_COUNT(entryPointWide), pStage->pEntryPoint, _TRUNCATE);
            }
            else
            {
                wcscpy_s(entryPointWide, TF_ARRAY_COUNT(entryPointWide), L"main");
            }

            wchar_t sourceNameWide[FS_MAX_PATH] = {};
            if (pStage->pName)
            {
                size_t converted = 0;
                mbstowcs_s(&converted, sourceNameWide, TF_ARRAY_COUNT(sourceNameWide), pStage->pName, _TRUNCATE);
            }

            wchar_t profile[16] = {};
            d3d12_getShaderProfile(stage_mask, (ShaderTarget)pRenderer->shaderTarget, profile, TF_ARRAY_COUNT(profile));

            // Build compile arguments.
            LPCWSTR  args[MAX_COMPILE_ARGS];
            uint32_t argCount = 0;

            args[argCount++] = L"-E";
            args[argCount++] = entryPointWide;
            args[argCount++] = L"-T";
            args[argCount++] = profile;

#if defined(ENABLE_GRAPHICS_DEBUG)
            args[argCount++] = L"-Zi";
            args[argCount++] = L"-Qembed_debug";
#endif

            IDxcOperationResult* pResult = NULL;
            HRESULT hr = pCompiler->Compile(pSourceBlob, sourceNameWide[0] ? sourceNameWide : NULL, entryPointWide, profile, args, argCount,
                                            NULL, 0, pIncludeHandler, &pResult);
            pSourceBlob->Release();

            if (FAILED(hr) || !pResult)
            {
                LOGF(LogLevel::eERROR, "Failed to compile HLSL shader for stage %i", stage_mask);
                continue;
            }

            HRESULT status = S_OK;
            pResult->GetStatus(&status);
            if (FAILED(status))
            {
                IDxcBlobEncoding* pError = NULL;
                if (SUCCEEDED(pResult->GetErrorBuffer(&pError)) && pError)
                {
                    LOGF(LogLevel::eERROR, "HLSL compilation error: %s", (const char*)pError->GetBufferPointer());
                    pError->Release();
                }
                pResult->Release();
                continue;
            }

            IDxcBlob* pCodeBlob = NULL;
            CHECK_HRESULT(pResult->GetResult(&pCodeBlob));
            pResult->Release();

            // Normalize the compiler output into the blob type the rest of the D3D12 backend already consumes.
            CHECK_HRESULT(pUtils->CreateBlob(pCodeBlob->GetBufferPointer(), (uint32_t)pCodeBlob->GetBufferSize(), DXC_CP_ACP,
                                             &pShaderProgram->dx.pShaderBlobs[reflectionCount]));
            pCodeBlob->Release();

            d3d12_createShaderReflection((uint8_t*)(pShaderProgram->dx.pShaderBlobs[reflectionCount]->GetBufferPointer()),
                                         (uint32_t)pShaderProgram->dx.pShaderBlobs[reflectionCount]->GetBufferSize(), stage_mask,
                                         &pShaderProgram->pReflection->stageReflections[reflectionCount]);

            WCHAR* entryPointName = (WCHAR*)mem;
            mbstowcs((WCHAR*)entryPointName, pStage->pEntryPoint, strlen(pStage->pEntryPoint));
            pShaderProgram->dx.pEntryNames[reflectionCount] = entryPointName;
            mem += (strlen(pStage->pEntryPoint) + 1) * sizeof(WCHAR);

            reflectionCount++;
        }
    }

    createPipelineReflection(pShaderProgram->pReflection->stageReflections, reflectionCount, pShaderProgram->pReflection);

    *ppShaderProgram = pShaderProgram;

    if (pIncludeHandler)
        pIncludeHandler->Release();
    if (pCompiler)
        pCompiler->Release();
    if (pLibrary)
        pLibrary->Release();
    if (pUtils)
        pUtils->Release();
}

void d3d12_removeShader(Renderer* pRenderer, Shader* pShaderProgram)
{
    UNREF_PARAM(pRenderer);

    // remove given shader
    for (uint32_t i = 0; i < pShaderProgram->pReflection->stageReflectionCount; ++i)
    {
        SAFE_RELEASE(pShaderProgram->dx.pShaderBlobs[i]);
    }
    destroyPipelineReflection(pShaderProgram->pReflection);

    SAFE_FREE(pShaderProgram);
}
/************************************************************************/
// Root Signature Functions
/************************************************************************/
void d3d12_addRootSignature(Renderer* pRenderer, const RootSignatureDesc* pRootSignatureDesc, RootSignature** ppRootSignature)
{
    ASSERT(pRenderer->pGpu->settings.maxRootSignatureDWORDS > 0);
    ASSERT(ppRootSignature);

    struct StaticSampler
    {
        ShaderResource* pShaderResource;
        Sampler*        pSampler;
    };

    struct StaticSamplerNode
    {
        char*    key;
        Sampler* value;
    };

    static constexpr uint32_t kMaxLayoutCount = DESCRIPTOR_UPDATE_FREQ_COUNT;
    UpdateFrequencyLayoutInfo layouts[kMaxLayoutCount] = {};
    ShaderResource*           shaderResources = NULL;
    uint32_t*                 constantSizes = NULL;
    StaticSampler*            staticSamplers = NULL;
    ShaderStage               shaderStages = SHADER_STAGE_NONE;
    bool                      useInputLayout = false;
    bool                      useViewHeapIndexing = false;
    bool                      useSamplerHeapIndexing = false;
    StaticSamplerNode*        staticSamplerMap = NULL;
    PipelineType              pipelineType = PIPELINE_TYPE_UNDEFINED;
    DescriptorIndexMap*       indexMap = NULL;
    sh_new_arena(staticSamplerMap);
    sh_new_arena(indexMap);

    for (uint32_t i = 0; i < pRootSignatureDesc->staticSamplerCount; ++i)
    {
        shput(staticSamplerMap, pRootSignatureDesc->ppStaticSamplerNames[i], pRootSignatureDesc->ppStaticSamplers[i]);
    }

    // Collect all unique shader resources in the given shaders
    // Resources are parsed by name (two resources named "XYZ" in two shaders will be considered the same resource)
    for (uint32_t sh = 0; sh < pRootSignatureDesc->shaderCount; ++sh)
    {
        PipelineReflection const* pReflection = pRootSignatureDesc->ppShaders[sh]->pReflection;

        for (uint32_t stage = 0; stage < pReflection->stageReflectionCount; ++stage)
        {
            useViewHeapIndexing |= pReflection->stageReflections[stage].cbvHeapIndexing;
            useSamplerHeapIndexing |= pReflection->stageReflections[stage].samplerHeapIndexing;
        }

        // Keep track of the used pipeline stages
        shaderStages |= pReflection->shaderStages;

        if (pReflection->shaderStages & SHADER_STAGE_COMP)
            pipelineType = PIPELINE_TYPE_COMPUTE;
        else
            pipelineType = PIPELINE_TYPE_GRAPHICS;

        if (pReflection->shaderStages & SHADER_STAGE_VERT)
        {
            if (pReflection->stageReflections[pReflection->vertexStageIndex].vertexInputsCount)
            {
                useInputLayout = true;
            }
        }
        for (uint32_t i = 0; i < pReflection->shaderResourceCount; ++i)
        {
            ShaderResource const* pRes = &pReflection->pShaderResources[i];

            DescriptorIndexMap* pNode = shgetp_null(indexMap, pRes->name);

            // Find all unique resources
            if (pNode == NULL)
            {
                ShaderResource* pFound = NULL;
                for (ptrdiff_t j = 0; j < arrlen(shaderResources); ++j)
                {
                    ShaderResource* pCurrent = &shaderResources[j];
                    if (pCurrent->type == pRes->type && (pCurrent->used_stages == pRes->used_stages) &&
                        (((pCurrent->reg ^ pRes->reg) | (pCurrent->set ^ pRes->set)) == 0))
                    {
                        pFound = pCurrent;
                        break;
                    }
                }
                if (!pFound)
                {
                    shput(indexMap, pRes->name, (uint32_t)arrlenu(shaderResources));

                    arrpush(shaderResources, *pRes);

                    uint32_t constantSize = 0;

                    if (pRes->type == DESCRIPTOR_TYPE_UNIFORM_BUFFER)
                    {
                        for (uint32_t v = 0; v < pReflection->variableCount; ++v)
                        {
                            if (pReflection->pVariables[v].parent_index == i)
                                constantSize += pReflection->pVariables[v].size;
                        }
                    }

                    // shaderStages |= pRes->used_stages;
                    arrpush(constantSizes, constantSize);
                }
                else
                {
                    ASSERT(pRes->type == pFound->type);
                    if (pRes->type != pFound->type)
                    {
                        LOGF(LogLevel::eERROR,
                             "\nFailed to create root signature\n"
                             "Shared shader resources %s and %s have mismatching types (%u) and (%u). All shader resources "
                             "sharing the same register and space addRootSignature "
                             "must have the same type",
                             pRes->name, pFound->name, (uint32_t)pRes->type, (uint32_t)pFound->type);
                        return;
                    }

                    uint32_t foundIndex = shget(indexMap, pFound->name);
                    shput(indexMap, pRes->name, foundIndex);

                    pFound->used_stages |= pRes->used_stages;
                }
            }
            // If the resource was already collected, just update the shader stage mask in case it is used in a different
            // shader stage in this case
            else
            {
                if (shaderResources[pNode->value].reg != pRes->reg) //-V::522, 595
                {
                    LOGF(LogLevel::eERROR,
                         "\nFailed to create root signature\n"
                         "Shared shader resource %s has mismatching register. All shader resources "
                         "shared by multiple shaders specified in addRootSignature "
                         "have the same register and space",
                         pRes->name);
                    return;
                }
                if (shaderResources[pNode->value].set != pRes->set) //-V::522, 595
                {
                    LOGF(LogLevel::eERROR,
                         "\nFailed to create root signature\n"
                         "Shared shader resource %s has mismatching space. All shader resources "
                         "shared by multiple shaders specified in addRootSignature "
                         "have the same register and space",
                         pRes->name);
                    return;
                }

                for (ptrdiff_t j = 0; j < arrlen(shaderResources); ++j)
                {
                    if (strcmp(shaderResources[j].name, pNode->key) == 0)
                    {
                        shaderResources[j].used_stages |= pRes->used_stages;
                        break;
                    }
                }
            }
        }
    }

    size_t totalSize = sizeof(RootSignature);
    totalSize += arrlenu(shaderResources) * sizeof(DescriptorInfo);

    RootSignature* pRootSignature = (RootSignature*)tf_calloc_memalign(1, alignof(RootSignature), totalSize);
    ASSERT(pRootSignature);

    if ((uint32_t)arrlenu(shaderResources))
    {
        pRootSignature->descriptorCount = (uint32_t)arrlenu(shaderResources);
    }

    pRootSignature->pDescriptors = (DescriptorInfo*)(pRootSignature + 1); //-V1027
    pRootSignature->pDescriptorNameToIndexMap = indexMap;
    ASSERT(pRootSignature->pDescriptorNameToIndexMap);

    pRootSignature->pipelineType = pipelineType;

    // Fill the descriptor array to be stored in the root signature
    for (uint32_t i = 0; i < (uint32_t)arrlenu(shaderResources); ++i)
    {
        DescriptorInfo* pDesc = &pRootSignature->pDescriptors[i];
        ShaderResource* pRes = &shaderResources[i];
        uint32_t        setIndex = pRes->set;
        if (pRes->size == 0 || setIndex >= DESCRIPTOR_UPDATE_FREQ_COUNT)
            setIndex = 0;

        DescriptorUpdateFrequency updateFreq = (DescriptorUpdateFrequency)setIndex;

        pDesc->size = pRes->size;
        pDesc->type = pRes->type;
        pDesc->dim = pRes->dim;
        pDesc->pName = pRes->name;
        pDesc->updateFrequency = updateFreq;

        if (pDesc->size == 0 && pDesc->type == DESCRIPTOR_TYPE_TEXTURE)
        {
            pDesc->size = pRootSignatureDesc->maxBindlessTextures;
        }

        // Find the D3D12 type of the descriptors
        if (pDesc->type == DESCRIPTOR_TYPE_SAMPLER)
        {
            // If the sampler is a static sampler, no need to put it in the descriptor table
            StaticSamplerNode* pNode = shgetp_null(staticSamplerMap, pDesc->pName);

            if (pNode)
            {
                LOGF(LogLevel::eINFO, "Descriptor (%s) : User specified Static Sampler", pDesc->pName);
                // Set the index to invalid value so we can use this later for error checking if user tries to update a static sampler
                pDesc->staticSampler = true;
                StaticSampler sampler = { pRes, pNode->value };
                arrpush(staticSamplers, sampler);
            }
            else
            {
                // In D3D12, sampler descriptors cannot be placed in a table containing view descriptors
                RootParameter param = { *pRes, pDesc };
                arrpush(layouts[setIndex].samplerTable, param);
            }
        }
        // No support for arrays of constant buffers to be used as root descriptors as this might bloat the root signature size
        else if (pDesc->type == DESCRIPTOR_TYPE_UNIFORM_BUFFER && pDesc->size == 1)
        {
            // D3D12 has no special syntax to declare root constants like Vulkan
            // So we assume that all constant buffers with the word "rootconstant", "pushconstant" (case insensitive) are root constants
            if (isDescriptorRootConstant(pRes->name))
            {
                // Make the root param a 32 bit constant if the user explicitly specifies it in the shader
                pDesc->rootDescriptor = 1;
                pDesc->type = DESCRIPTOR_TYPE_ROOT_CONSTANT;
                RootParameter param = { *pRes, pDesc };
                arrpush(layouts[setIndex].rootConstants, param);

                pDesc->size = constantSizes[i] / sizeof(uint32_t);
            }
            // If a user specified a uniform buffer to be used directly in the root signature change its type to
            // D3D12_ROOT_PARAMETER_TYPE_CBV Also log a message for debugging purpose
            else if (isDescriptorRootCbv(pRes->name))
            {
                RootParameter param = { *pRes, pDesc };
                arrpush(layouts[setIndex].rootDescriptorParams, param);
                pDesc->rootDescriptor = 1;

                LOGF(LogLevel::eINFO, "Descriptor (%s) : User specified D3D12_ROOT_PARAMETER_TYPE_CBV", pDesc->pName);
            }
            else
            {
                RootParameter param = { *pRes, pDesc };
                arrpush(layouts[setIndex].cbvSrvUavTable, param);
            }
        }
        else
        {
            RootParameter param = { *pRes, pDesc };
            arrpush(layouts[setIndex].cbvSrvUavTable, param);

#if defined(_WINDOWS) && defined(D3D12_RAYTRACING_AVAILABLE) && defined(FORGE_DEBUG)
            if (DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE == pDesc->type)
            {
                pRootSignature->dx.hasRayQueryAccelerationStructure = true;
            }
#endif
        }

        hmput(layouts[setIndex].descriptorIndexMap, pDesc, i);
    }

    // We should never reach inside this if statement. If we do, something got messed up
    if (pRenderer->pGpu->settings.maxRootSignatureDWORDS < calculate_root_signature_size(layouts, kMaxLayoutCount))
    {
        LOGF(LogLevel::eWARNING, "Root Signature size greater than the specified max size");
        ASSERT(false);
    }

    // D3D12 currently has two versions of root signatures (1_0, 1_1)
    // So we fill the structs of both versions and in the end use the structs compatible with the supported version
    constexpr uint32_t         kMaxResourceTableSize = 32;
    D3D12_DESCRIPTOR_RANGE1    cbvSrvUavRange[kMaxLayoutCount][kMaxResourceTableSize] = {};
    D3D12_DESCRIPTOR_RANGE1    samplerRange[kMaxLayoutCount][kMaxResourceTableSize] = {};
    D3D12_ROOT_PARAMETER1      rootParams[D3D12_MAX_ROOT_COST] = {};
    uint32_t                   rootParamCount = 0;
    D3D12_STATIC_SAMPLER_DESC* staticSamplerDescs = NULL;
    uint32_t                   staticSamplerCount = (uint32_t)arrlenu(staticSamplers);

    if (staticSamplerCount)
    {
        staticSamplerDescs = (D3D12_STATIC_SAMPLER_DESC*)alloca(staticSamplerCount * sizeof(D3D12_STATIC_SAMPLER_DESC));

        for (uint32_t i = 0; i < staticSamplerCount; ++i)
        {
            D3D12_SAMPLER_DESC& desc = staticSamplers[i].pSampler->dx.desc;
            staticSamplerDescs[i].Filter = desc.Filter;
            staticSamplerDescs[i].AddressU = desc.AddressU;
            staticSamplerDescs[i].AddressV = desc.AddressV;
            staticSamplerDescs[i].AddressW = desc.AddressW;
            staticSamplerDescs[i].MipLODBias = desc.MipLODBias;
            staticSamplerDescs[i].MaxAnisotropy = desc.MaxAnisotropy;
            staticSamplerDescs[i].ComparisonFunc = desc.ComparisonFunc;
            staticSamplerDescs[i].MinLOD = desc.MinLOD;
            staticSamplerDescs[i].MaxLOD = desc.MaxLOD;
            staticSamplerDescs[i].BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;

            ShaderResource* samplerResource = staticSamplers[i].pShaderResource;
            staticSamplerDescs[i].RegisterSpace = samplerResource->set;
            staticSamplerDescs[i].ShaderRegister = samplerResource->reg;
            staticSamplerDescs[i].ShaderVisibility = util_to_dx12_shader_visibility(samplerResource->used_stages);
        }
    }

    for (uint32_t i = 0; i < kMaxLayoutCount; ++i)
    {
        if (arrlen(layouts[i].cbvSrvUavTable))
        {
            ASSERT(arrlenu(layouts[i].cbvSrvUavTable) <= kMaxResourceTableSize);
            ++rootParamCount;
        }
        if (arrlen(layouts[i].samplerTable))
        {
            ASSERT(arrlenu(layouts[i].samplerTable) <= kMaxResourceTableSize);
            ++rootParamCount;
        }
    }

    pRootSignature->descriptorCount = (uint32_t)arrlenu(shaderResources);

    for (uint32_t i = 0; i < kMaxLayoutCount; ++i)
    {
        rootParamCount += (uint32_t)arrlenu(layouts[i].rootConstants);
        rootParamCount += (uint32_t)arrlenu(layouts[i].rootDescriptorParams);
    }

    rootParamCount = 0;

    // Start collecting root parameters
    // Start with root descriptors since they will be the most frequently updated descriptors
    // This also makes sure that if we spill, the root descriptors in the front of the root signature will most likely still remain in the
    // root Collect all root descriptors Put most frequently changed params first
    for (uint32_t i = kMaxLayoutCount; i-- > 0U;)
    {
        UpdateFrequencyLayoutInfo& layout = layouts[i];
        if (arrlen(layout.rootDescriptorParams))
        {
            ASSERT(1 == arrlen(layout.rootDescriptorParams));

            uint32_t rootDescriptorIndex = 0;

            for (ptrdiff_t descIndex = 0; descIndex < arrlen(layout.rootDescriptorParams); ++descIndex)
            {
                RootParameter* pDesc = &layout.rootDescriptorParams[descIndex];
                pDesc->pDescriptorInfo->handleIndex = rootParamCount;

                D3D12_ROOT_PARAMETER1 rootParam;
                create_root_descriptor(pDesc, &rootParam);

                rootParams[rootParamCount++] = rootParam;

                ++rootDescriptorIndex;
            }
        }
    }

    uint32_t rootConstantIndex = 0;

    // Collect all root constants
    for (uint32_t setIndex = 0; setIndex < kMaxLayoutCount; ++setIndex)
    {
        UpdateFrequencyLayoutInfo& layout = layouts[setIndex];

        if (!arrlen(layout.rootConstants))
            continue;

        for (ptrdiff_t i = 0; i < arrlen(layouts[setIndex].rootConstants); ++i)
        {
            RootParameter* pDesc = &layout.rootConstants[i];
            pDesc->pDescriptorInfo->handleIndex = rootParamCount;

            D3D12_ROOT_PARAMETER1 rootParam;
            create_root_constant(pDesc, &rootParam);

            rootParams[rootParamCount++] = rootParam;

            if (pDesc->pDescriptorInfo->size > gMaxRootConstantsPerRootParam)
            {
                // 64 DWORDS for NVIDIA, 16 for AMD but 3 are used by driver so we get 13 SGPR
                // DirectX12
                // Root descriptors - 2
                // Root constants - Number of 32 bit constants
                // Descriptor tables - 1
                // Static samplers - 0
                LOGF(LogLevel::eINFO, "Root constant (%s) has (%u) 32 bit values. It is recommended to have root constant number <= %u",
                     pDesc->pDescriptorInfo->pName, pDesc->pDescriptorInfo->size, gMaxRootConstantsPerRootParam);
            }

            ++rootConstantIndex;
        }
    }

    // prevent warnings due to unused static function (the func is defined inside of the the sort impl generator macro)
    size_t (*func)(RootParameter*, size_t, size_t) = partitionRootParameter;
    (void)func;

    // Collect descriptor table parameters
    // Put most frequently changed descriptor tables in the front of the root signature
    for (uint32_t i = kMaxLayoutCount; i-- > 0U;)
    {
        UpdateFrequencyLayoutInfo& layout = layouts[i];

        // Fill the descriptor table layout for the view descriptor table of this update frequency
        if (arrlen(layout.cbvSrvUavTable))
        {
            // sort table by type (CBV/SRV/UAV) by register by space
            sortRootParameter(layout.cbvSrvUavTable, arrlenu(layout.cbvSrvUavTable));

            D3D12_ROOT_PARAMETER1 rootParam;
            create_descriptor_table((uint32_t)arrlenu(layout.cbvSrvUavTable), layout.cbvSrvUavTable, cbvSrvUavRange[i], &rootParam);

            // Store some of the binding info which will be required later when binding the descriptor table
            // We need the root index when calling SetRootDescriptorTable
            pRootSignature->dx.viewDescriptorTableRootIndices[i] = (uint8_t)rootParamCount;
            pRootSignature->dx.viewDescriptorCounts[i] = (uint16_t)arrlenu(layout.cbvSrvUavTable);

            for (ptrdiff_t descIndex = 0; descIndex < arrlen(layout.cbvSrvUavTable); ++descIndex)
            {
                DescriptorInfo* pDesc = layout.cbvSrvUavTable[descIndex].pDescriptorInfo;

                // Store the d3d12 related info in the descriptor to avoid constantly calling the util_to_dx mapping functions
                pDesc->rootDescriptor = 0;
                pDesc->handleIndex = pRootSignature->dx.cumulativeViewDescriptorCounts[i];

                // Store the cumulative descriptor count so we can just fetch this value later when allocating descriptor handles
                // This avoids unnecessary loops in the future to find the unfolded number of descriptors (includes shader resource arrays)
                // in the descriptor table
                pRootSignature->dx.cumulativeViewDescriptorCounts[i] += pDesc->size;
            }

            rootParams[rootParamCount++] = rootParam;
        }

        // Fill the descriptor table layout for the sampler descriptor table of this update frequency
        if (arrlen(layout.samplerTable))
        {
            D3D12_ROOT_PARAMETER1 rootParam;
            create_descriptor_table((uint32_t)arrlenu(layout.samplerTable), layout.samplerTable, samplerRange[i], &rootParam);

            // Store some of the binding info which will be required later when binding the descriptor table
            // We need the root index when calling SetRootDescriptorTable
            pRootSignature->dx.samplerDescriptorTableRootIndices[i] = (uint8_t)rootParamCount;
            pRootSignature->dx.samplerDescriptorCounts[i] = (uint16_t)arrlenu(layout.samplerTable);
            // table.pDescriptorIndices = (uint32_t*)tf_calloc(table.descriptorCount, sizeof(uint32_t));

            for (ptrdiff_t descIndex = 0; descIndex < arrlen(layout.samplerTable); ++descIndex)
            {
                DescriptorInfo* pDesc = layout.samplerTable[descIndex].pDescriptorInfo;

                // Store the d3d12 related info in the descriptor to avoid constantly calling the util_to_dx mapping functions
                pDesc->rootDescriptor = 0;
                pDesc->handleIndex = pRootSignature->dx.cumulativeSamplerDescriptorCounts[i];

                // Store the cumulative descriptor count so we can just fetch this value later when allocating descriptor handles
                // This avoids unnecessary loops in the future to find the unfolded number of descriptors (includes shader resource arrays)
                // in the descriptor table
                pRootSignature->dx.cumulativeSamplerDescriptorCounts[i] += pDesc->size;
            }

            rootParams[rootParamCount++] = rootParam;
        }
    }

    // Specify the deny flags to avoid unnecessary shader stages being notified about descriptor modifications
    D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags = D3D12_ROOT_SIGNATURE_FLAG_NONE;
    if (useInputLayout)
        rootSignatureFlags |= D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
    if (!(shaderStages & SHADER_STAGE_VERT))
        rootSignatureFlags |= D3D12_ROOT_SIGNATURE_FLAG_DENY_VERTEX_SHADER_ROOT_ACCESS;
    if (!(shaderStages & SHADER_STAGE_HULL))
        rootSignatureFlags |= D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS;
    if (!(shaderStages & SHADER_STAGE_DOMN))
        rootSignatureFlags |= D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS;
    if (!(shaderStages & SHADER_STAGE_GEOM))
        rootSignatureFlags |= D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;
    if (!(shaderStages & SHADER_STAGE_FRAG))
        rootSignatureFlags |= D3D12_ROOT_SIGNATURE_FLAG_DENY_PIXEL_SHADER_ROOT_ACCESS;
    if (useViewHeapIndexing)
        rootSignatureFlags |= D3D12_ROOT_SIGNATURE_FLAG_CBV_SRV_UAV_HEAP_DIRECTLY_INDEXED;
    if (useSamplerHeapIndexing)
        rootSignatureFlags |= D3D12_ROOT_SIGNATURE_FLAG_SAMPLER_HEAP_DIRECTLY_INDEXED;

    hook_modify_rootsignature_flags(shaderStages, &rootSignatureFlags);

    ID3DBlob* error = NULL;
    ID3DBlob* rootSignatureString = NULL;
    DECLARE_ZERO(D3D12_VERSIONED_ROOT_SIGNATURE_DESC, desc);
    desc.Version = D3D_ROOT_SIGNATURE_VERSION_1_1;
    desc.Desc_1_1.NumParameters = rootParamCount;
    desc.Desc_1_1.pParameters = rootParams;
    desc.Desc_1_1.NumStaticSamplers = staticSamplerCount;
    desc.Desc_1_1.pStaticSamplers = staticSamplerDescs;
    desc.Desc_1_1.Flags = rootSignatureFlags;

    HRESULT hr = d3d12dll_SerializeVersionedRootSignature(&desc, &rootSignatureString, &error);

    if (!SUCCEEDED(hr))
    {
        LOGF(LogLevel::eERROR, "Failed to serialize root signature with error (%s)", (char*)error->GetBufferPointer());
    }

    const HRESULT createRootSignatureResult = pRenderer->dx.pDevice->CreateRootSignature(
        0, rootSignatureString->GetBufferPointer(), rootSignatureString->GetBufferSize(), IID_ARGS(&pRootSignature->dx.pRootSignature));
    if (FAILED(createRootSignatureResult))
        LOGF(LogLevel::eERROR, "D3D12 device removal reason while creating root signature: 0x%08X",
             (uint32_t)pRenderer->dx.pDevice->GetDeviceRemovedReason());
    CHECK_HRESULT(createRootSignatureResult);

    SAFE_RELEASE(error);
    SAFE_RELEASE(rootSignatureString);
    for (uint32_t i = 0; i < kMaxLayoutCount; ++i)
    {
        UpdateFrequencyLayoutInfo* pLayout = &layouts[i];
        arrfree(pLayout->cbvSrvUavTable);
        arrfree(pLayout->samplerTable);
        arrfree(pLayout->rootDescriptorParams);
        arrfree(pLayout->rootConstants);
        hmfree(pLayout->descriptorIndexMap);
    }

    arrfree(shaderResources);
    arrfree(constantSizes);
    arrfree(staticSamplers);
    shfree(staticSamplerMap);

    *ppRootSignature = pRootSignature;
}

void d3d12_removeRootSignature(Renderer* pRenderer, RootSignature* pRootSignature)
{
    UNREF_PARAM(pRenderer);
    shfree(pRootSignature->pDescriptorNameToIndexMap);
    SAFE_RELEASE(pRootSignature->dx.pRootSignature);

    SAFE_FREE(pRootSignature);
}

uint32_t d3d12_getDescriptorIndexFromName(const RootSignature* pRootSignature, const char* pName)
{
    // for (uint32_t i = 0; i < pRootSignature->descriptorCount; ++i)
    // {
    //     if (!strcmp(pName, pRootSignature->pDescriptors[i].pName))
    //         return i;
    // }

    const DescriptorIndexMap* pNode = shgetp_null(pRootSignature->pDescriptorNameToIndexMap, pName);
    return pNode ? pNode->value : UINT32_MAX;
}

/************************************************************************/
// Descriptor Set Functions
/************************************************************************/
void d3d12_addDescriptorSet(Renderer* pRenderer, const DescriptorSetDesc* pDesc, DescriptorSet** ppDescriptorSet)
{
    ASSERT(pRenderer);
    ASSERT(pDesc);
    ASSERT(ppDescriptorSet);

    const RootSignature*            pRootSignature = pDesc->pRootSignature;
    const DescriptorUpdateFrequency updateFreq = pDesc->updateFrequency;
    const uint32_t                  nodeIndex = 0;
    const uint32_t                  cbvSrvUavDescCount = pRootSignature->dx.cumulativeViewDescriptorCounts[updateFreq];
    const uint32_t                  samplerDescCount = pRootSignature->dx.cumulativeSamplerDescriptorCounts[updateFreq];

    DescriptorSet* pDescriptorSet = (DescriptorSet*)tf_calloc_memalign(1, alignof(DescriptorSet), sizeof(DescriptorSet));
    ASSERT(pDescriptorSet);

    pDescriptorSet->dx.pRootSignature = pRootSignature;
    pDescriptorSet->dx.updateFrequency = updateFreq;
    pDescriptorSet->dx.maxSets = pDesc->maxSets;
    pDescriptorSet->dx.cbvSrvUavRootIndex = pRootSignature->dx.viewDescriptorTableRootIndices[updateFreq];
    pDescriptorSet->dx.samplerRootIndex = pRootSignature->dx.samplerDescriptorTableRootIndices[updateFreq];
    pDescriptorSet->dx.cbvSrvUavHandle = D3D12_DESCRIPTOR_ID_NONE;
    pDescriptorSet->dx.samplerHandle = D3D12_DESCRIPTOR_ID_NONE;
    pDescriptorSet->dx.pipelineType = pRootSignature->pipelineType;

    if (cbvSrvUavDescCount || samplerDescCount)
    {
        if (cbvSrvUavDescCount)
        {
            DescriptorHeap* pSrcHeap = pRenderer->dx.pCPUDescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV];
            DescriptorHeap* pHeap = pRenderer->dx.pCbvSrvUavHeaps[nodeIndex];
            pDescriptorSet->dx.cbvSrvUavHandle = consume_descriptor_handles(pHeap, cbvSrvUavDescCount * pDesc->maxSets);
            pDescriptorSet->dx.cbvSrvUavStride = cbvSrvUavDescCount;

            for (uint32_t i = 0; i < pRootSignature->descriptorCount; ++i)
            {
                const DescriptorInfo* pDescInfo = &pRootSignature->pDescriptors[i];
                if (!pDescInfo->rootDescriptor && pDescInfo->type != DESCRIPTOR_TYPE_SAMPLER &&
                    (int)pDescInfo->updateFrequency == updateFreq)
                {
                    DescriptorType type = (DescriptorType)pDescInfo->type;
                    DxDescriptorID srcHandle = D3D12_DESCRIPTOR_ID_NONE;
                    switch (type)
                    {
                    case DESCRIPTOR_TYPE_TEXTURE:
                        srcHandle = pRenderer->pNullDescriptors->nullTextureSRV[pDescInfo->dim];
                        break;
                    case DESCRIPTOR_TYPE_BUFFER:
                        srcHandle = pRenderer->pNullDescriptors->nullBufferSRV;
                        break;
                    case DESCRIPTOR_TYPE_RW_TEXTURE:
                        srcHandle = pRenderer->pNullDescriptors->nullTextureUAV[pDescInfo->dim];
                        break;
                    case DESCRIPTOR_TYPE_RW_BUFFER:
                        srcHandle = pRenderer->pNullDescriptors->nullBufferUAV;
                        break;
                    case DESCRIPTOR_TYPE_UNIFORM_BUFFER:
                        srcHandle = pRenderer->pNullDescriptors->nullBufferCBV;
                        break;
                    default:
                        break;
                    }

#ifdef D3D12_RAYTRACING_AVAILABLE
                    if (pDescInfo->type != DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE)
#endif
                    {
                        ASSERT(srcHandle != D3D12_DESCRIPTOR_ID_NONE);

                        for (uint32_t s = 0; s < pDesc->maxSets; ++s)
                            for (uint32_t j = 0; j < pDescInfo->size; ++j)
                                copy_descriptor_handle(pSrcHeap, srcHandle, pHeap,
                                                       pDescriptorSet->dx.cbvSrvUavHandle + s * pDescriptorSet->dx.cbvSrvUavStride +
                                                           pDescInfo->handleIndex + j);
                    }
                }
            }
        }
        if (samplerDescCount)
        {
            DescriptorHeap* pSrcHeap = pRenderer->dx.pCPUDescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER];
            DescriptorHeap* pHeap = pRenderer->dx.pSamplerHeaps[nodeIndex];
            pDescriptorSet->dx.samplerHandle = consume_descriptor_handles(pHeap, samplerDescCount * pDesc->maxSets);
            pDescriptorSet->dx.samplerStride = samplerDescCount;
            for (uint32_t i = 0; i < pDesc->maxSets; ++i)
            {
                for (uint32_t j = 0; j < samplerDescCount; ++j)
                    copy_descriptor_handle(pSrcHeap, pRenderer->pNullDescriptors->nullSampler, pHeap,
                                           pDescriptorSet->dx.samplerHandle + i * pDescriptorSet->dx.samplerStride + j);
            }
        }
    }

    *ppDescriptorSet = pDescriptorSet;
}

void d3d12_removeDescriptorSet(Renderer* pRenderer, DescriptorSet* pDescriptorSet)
{
    ASSERT(pRenderer);
    ASSERT(pDescriptorSet);

    if (pDescriptorSet->dx.cbvSrvUavHandle != D3D12_DESCRIPTOR_ID_NONE)
    {
        return_descriptor_handles(pRenderer->dx.pCbvSrvUavHeaps[0], pDescriptorSet->dx.cbvSrvUavHandle,
                                  pDescriptorSet->dx.cbvSrvUavStride * pDescriptorSet->dx.maxSets);
    }

    if (pDescriptorSet->dx.samplerHandle != D3D12_DESCRIPTOR_ID_NONE)
    {
        return_descriptor_handles(pRenderer->dx.pSamplerHeaps[0], pDescriptorSet->dx.samplerHandle,
                                  pDescriptorSet->dx.samplerStride * pDescriptorSet->dx.maxSets);
    }

    pDescriptorSet->dx.cbvSrvUavHandle = D3D12_DESCRIPTOR_ID_NONE;
    pDescriptorSet->dx.samplerHandle = D3D12_DESCRIPTOR_ID_NONE;

    SAFE_FREE(pDescriptorSet);
}

#if defined(ENABLE_GRAPHICS_DEBUG) || defined(PVS_STUDIO)
#define VALIDATE_DESCRIPTOR(descriptor, msgFmt, ...)                           \
    if (!VERIFYMSG((descriptor), "%s : " msgFmt, __FUNCTION__, ##__VA_ARGS__)) \
    {                                                                          \
        continue;                                                              \
    }
#else
#define VALIDATE_DESCRIPTOR(descriptor, ...)
#endif

void d3d12_updateDescriptorSet(Renderer* pRenderer, uint32_t index, DescriptorSet* pDescriptorSet, uint32_t count,
                               const DescriptorData* pParams)
{
    ASSERT(pRenderer);
    ASSERT(pDescriptorSet);
    ASSERT(index < pDescriptorSet->dx.maxSets);

    const RootSignature*            pRootSignature = pDescriptorSet->dx.pRootSignature;
    const DescriptorUpdateFrequency updateFreq = (DescriptorUpdateFrequency)pDescriptorSet->dx.updateFrequency;
    const uint32_t                  nodeIndex = 0;

    for (uint32_t i = 0; i < count; ++i)
    {
        const DescriptorData* pParam = pParams + i;
        uint32_t              paramIndex = pParam->bindByIndex ? pParam->index : UINT32_MAX;

        VALIDATE_DESCRIPTOR(pParam->pName || (paramIndex != UINT32_MAX), "DescriptorData has NULL name and invalid index");

        const DescriptorInfo* pDesc = NULL;
        if (paramIndex != UINT32_MAX)
        {
            pDesc = pRootSignature->pDescriptors + paramIndex;
            VALIDATE_DESCRIPTOR(pDesc, "Invalid descriptor with param index (%u)", paramIndex);
        }
        else
        {
            const DescriptorIndexMap* pNode = pParam->pName ? shgetp_null(pRootSignature->pDescriptorNameToIndexMap, pParam->pName) : NULL;
            if (!pNode)
            {
                LOGF(LogLevel::eWARNING,
                     "Skipping descriptor param (%s): not found in root signature, likely optimized out or from a newer binding path",
                     pParam->pName ? pParam->pName : "<NULL>");
                continue;
            }
            pDesc = &pRootSignature->pDescriptors[pNode->value];
        }

        const DescriptorType type = (DescriptorType)pDesc->type; //-V522
        const uint32_t       arrayStart = pParam->arrayOffset;
        const uint32_t       arrayCount = max(1U, pParam->count);

        VALIDATE_DESCRIPTOR((int)pDesc->updateFrequency == updateFreq, "Descriptor (%s) - Mismatching update frequency and register space",
                            pDesc->pName);

        if (pDesc->rootDescriptor)
        {
            VALIDATE_DESCRIPTOR(false,
                                "Descriptor (%s) - Trying to update a root cbv through updateDescriptorSet. All root cbvs must be updated "
                                "through cmdBindDescriptorSetWithRootCbvs",
                                pDesc->pName);
        }
        else if (type == DESCRIPTOR_TYPE_SAMPLER)
        {
            // Index is invalid when descriptor is a static sampler
            VALIDATE_DESCRIPTOR(
                !pDesc->staticSampler,
                "Trying to update a static sampler (%s). All static samplers must be set in addRootSignature and cannot be updated later",
                pDesc->pName);

            VALIDATE_DESCRIPTOR(pParam->ppSamplers, "NULL Sampler (%s)", pDesc->pName);

            for (uint32_t arr = 0; arr < arrayCount; ++arr)
            {
                VALIDATE_DESCRIPTOR((uintptr_t)pParam->ppSamplers[arr] != D3D12_GPU_VIRTUAL_ADDRESS_NULL, "NULL Sampler (%s [%u] )",
                                    pDesc->pName, arr);

                copy_descriptor_handle(pRenderer->dx.pCPUDescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER],
                                       pParam->ppSamplers[arr]->dx.descriptor, pRenderer->dx.pSamplerHeaps[nodeIndex],
                                       pDescriptorSet->dx.samplerHandle + index * pDescriptorSet->dx.samplerStride +
                                           pDesc->handleIndex + arrayStart + arr);
            }
        }
        else
        {
            switch (type)
            {
            case DESCRIPTOR_TYPE_TEXTURE:
            {
                VALIDATE_DESCRIPTOR(pParam->ppTextures, "NULL Texture (%s)", pDesc->pName);

                for (uint32_t arr = 0; arr < arrayCount; ++arr)
                {
                    VALIDATE_DESCRIPTOR(pParam->ppTextures[arr], "NULL Texture (%s [%u] )", pDesc->pName, arr);

                    copy_descriptor_handle(pRenderer->dx.pCPUDescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV],
                                           pParam->ppTextures[arr]->dx.descriptors, pRenderer->dx.pCbvSrvUavHeaps[nodeIndex],
                                           pDescriptorSet->dx.cbvSrvUavHandle + index * pDescriptorSet->dx.cbvSrvUavStride +
                                               pDesc->handleIndex + arrayStart + arr);
                }
                break;
            }
            case DESCRIPTOR_TYPE_RW_TEXTURE:
            {
                VALIDATE_DESCRIPTOR(pParam->ppTextures, "NULL RW Texture (%s)", pDesc->pName);

                if (pParam->bindMipChain)
                {
                    VALIDATE_DESCRIPTOR(pParam->ppTextures[0], "NULL RW Texture (%s)", pDesc->pName);
                    for (uint32_t arr = 0; arr < pParam->ppTextures[0]->mipLevels; ++arr)
                    {
                        DxDescriptorID srcId = pParam->ppTextures[0]->dx.descriptors + arr + pParam->ppTextures[0]->dx.uavStartIndex;

                        copy_descriptor_handle(pRenderer->dx.pCPUDescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV], srcId,
                                               pRenderer->dx.pCbvSrvUavHeaps[nodeIndex],
                                               pDescriptorSet->dx.cbvSrvUavHandle + index * pDescriptorSet->dx.cbvSrvUavStride +
                                                   pDesc->handleIndex + arrayStart + arr);
                    }
                }
                else
                {
                    for (uint32_t arr = 0; arr < arrayCount; ++arr)
                    {
                        VALIDATE_DESCRIPTOR(pParam->ppTextures[arr], "NULL RW Texture (%s [%u] )", pDesc->pName, arr);

                        DxDescriptorID srcId =
                            pParam->ppTextures[arr]->dx.descriptors + pParam->uavMipSlice + pParam->ppTextures[arr]->dx.uavStartIndex;

                        copy_descriptor_handle(pRenderer->dx.pCPUDescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV], srcId,
                                               pRenderer->dx.pCbvSrvUavHeaps[nodeIndex],
                                               pDescriptorSet->dx.cbvSrvUavHandle + index * pDescriptorSet->dx.cbvSrvUavStride +
                                                   pDesc->handleIndex + arrayStart + arr);
                    }
                }
                break;
            }
            case DESCRIPTOR_TYPE_BUFFER:
            case DESCRIPTOR_TYPE_BUFFER_RAW:
            {
                VALIDATE_DESCRIPTOR(pParam->ppBuffers, "NULL Buffer (%s)", pDesc->pName);

                if (pParam->pRanges)
                {
                    const bool raw = DESCRIPTOR_TYPE_BUFFER_RAW == type;
                    for (uint32_t arr = 0; arr < arrayCount; ++arr)
                    {
                        DescriptorDataRange range = pParam->pRanges[arr];
                        VALIDATE_DESCRIPTOR(pParam->ppBuffers[arr], "NULL Buffer (%s [%u] )", pDesc->pName, arr);
                        VALIDATE_DESCRIPTOR(range.size > 0, "Descriptor (%s) - pRanges[%u].size is zero", pDesc->pName, arr);
                        if (!raw)
                        {
                            VALIDATE_DESCRIPTOR(range.structStride > 0, "Descriptor (%s) - pRanges[%u].structStride is zero",
                                                pDesc->pName, arr);
                        }
                        const uint32_t setStart = index * pDescriptorSet->dx.cbvSrvUavStride;
                        const uint32_t stride = raw ? sizeof(uint32_t) : range.structStride;
                        DxDescriptorID srv = pDescriptorSet->dx.cbvSrvUavHandle + setStart + (pDesc->handleIndex + arrayStart + arr);
                        AddBufferSrv(pRenderer, pRenderer->dx.pCbvSrvUavHeaps[nodeIndex], pParam->ppBuffers[arr]->dx.pResource, raw,
                                     range.offset / stride, range.size / stride, stride, &srv);
                    }
                }
                else
                {
                    for (uint32_t arr = 0; arr < arrayCount; ++arr)
                    {
                        VALIDATE_DESCRIPTOR(pParam->ppBuffers[arr], "NULL Buffer (%s [%u] )", pDesc->pName, arr);

                        DxDescriptorID srcId = pParam->ppBuffers[arr]->dx.descriptors + pParam->ppBuffers[arr]->dx.srvDescriptorOffset;

                        copy_descriptor_handle(pRenderer->dx.pCPUDescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV], srcId,
                                               pRenderer->dx.pCbvSrvUavHeaps[nodeIndex],
                                               pDescriptorSet->dx.cbvSrvUavHandle + index * pDescriptorSet->dx.cbvSrvUavStride +
                                                   pDesc->handleIndex + arrayStart + arr);
                    }
                }
                break;
            }
            case DESCRIPTOR_TYPE_RW_BUFFER:
            case DESCRIPTOR_TYPE_RW_BUFFER_RAW:
            {
                VALIDATE_DESCRIPTOR(pParam->ppBuffers, "NULL RW Buffer (%s)", pDesc->pName);

                if (pParam->pRanges)
                {
                    const bool raw = DESCRIPTOR_TYPE_RW_BUFFER_RAW == type;
                    for (uint32_t arr = 0; arr < arrayCount; ++arr)
                    {
                        DescriptorDataRange range = pParam->pRanges[arr];
                        VALIDATE_DESCRIPTOR(pParam->ppBuffers[arr], "NULL RW Buffer (%s [%u] )", pDesc->pName, arr);
                        VALIDATE_DESCRIPTOR(range.size > 0, "Descriptor (%s) - pRanges[%u].size is zero", pDesc->pName, arr);
                        if (!raw)
                        {
                            VALIDATE_DESCRIPTOR(range.structStride > 0, "Descriptor (%s) - pRanges[%u].structStride is zero",
                                                pDesc->pName, arr);
                        }
                        const uint32_t setStart = index * pDescriptorSet->dx.cbvSrvUavStride;
                        const uint32_t stride = raw ? sizeof(uint32_t) : range.structStride;
                        DxDescriptorID uav = pDescriptorSet->dx.cbvSrvUavHandle + setStart + (pDesc->handleIndex + arrayStart + arr);
                        AddBufferUav(pRenderer, pRenderer->dx.pCbvSrvUavHeaps[nodeIndex], pParam->ppBuffers[arr]->dx.pResource, NULL, 0,
                                     raw, range.offset / stride, range.size / stride, stride, &uav);
                    }
                }
                else
                {
                    for (uint32_t arr = 0; arr < arrayCount; ++arr)
                    {
                        VALIDATE_DESCRIPTOR(pParam->ppBuffers[arr], "NULL RW Buffer (%s [%u] )", pDesc->pName, arr);

                        DxDescriptorID srcId = pParam->ppBuffers[arr]->dx.descriptors + pParam->ppBuffers[arr]->dx.uavDescriptorOffset;

                        copy_descriptor_handle(pRenderer->dx.pCPUDescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV], srcId,
                                               pRenderer->dx.pCbvSrvUavHeaps[nodeIndex],
                                               pDescriptorSet->dx.cbvSrvUavHandle + index * pDescriptorSet->dx.cbvSrvUavStride +
                                                   pDesc->handleIndex + arrayStart + arr);
                    }
                }
                break;
            }
            case DESCRIPTOR_TYPE_UNIFORM_BUFFER:
            {
                VALIDATE_DESCRIPTOR(pParam->ppBuffers, "NULL Uniform Buffer (%s)", pDesc->pName);

                if (pParam->pRanges)
                {
                    for (uint32_t arr = 0; arr < arrayCount; ++arr)
                    {
                        DescriptorDataRange range = pParam->pRanges[arr];
                        VALIDATE_DESCRIPTOR(pParam->ppBuffers[arr], "NULL Uniform Buffer (%s [%u] )", pDesc->pName, arr);
                        VALIDATE_DESCRIPTOR(range.size > 0, "Descriptor (%s) - pRanges[%u].size is zero", pDesc->pName, arr);
                        VALIDATE_DESCRIPTOR(range.size <= D3D12_REQ_CONSTANT_BUFFER_SIZE,
                                            "Descriptor (%s) - pRanges[%u].size is %u which exceeds max size %u", pDesc->pName, arr,
                                            range.size, D3D12_REQ_CONSTANT_BUFFER_SIZE);

                        D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
                        cbvDesc.BufferLocation = pParam->ppBuffers[arr]->dx.gpuAddress + range.offset;
                        cbvDesc.SizeInBytes = range.size;
                        uint32_t       setStart = index * pDescriptorSet->dx.cbvSrvUavStride;
                        DxDescriptorID cbv = pDescriptorSet->dx.cbvSrvUavHandle + setStart + (pDesc->handleIndex + arrayStart + arr);
                        AddCbv(pRenderer, pRenderer->dx.pCbvSrvUavHeaps[nodeIndex], &cbvDesc, &cbv);
                    }
                }
                else
                {
                    for (uint32_t arr = 0; arr < arrayCount; ++arr)
                    {
                        VALIDATE_DESCRIPTOR(pParam->ppBuffers[arr], "NULL Uniform Buffer (%s [%u] )", pDesc->pName, arr);
                        VALIDATE_DESCRIPTOR(pParam->ppBuffers[arr]->size <= D3D12_REQ_CONSTANT_BUFFER_SIZE,
                                            "Descriptor (%s) - pParam->ppBuffers[%u]->size is %llu which exceeds max size %u",
                                            pDesc->pName, arr, pParam->ppBuffers[arr]->size, D3D12_REQ_CONSTANT_BUFFER_SIZE);

                        copy_descriptor_handle(pRenderer->dx.pCPUDescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV],
                                               pParam->ppBuffers[arr]->dx.descriptors, pRenderer->dx.pCbvSrvUavHeaps[nodeIndex],
                                               pDescriptorSet->dx.cbvSrvUavHandle + index * pDescriptorSet->dx.cbvSrvUavStride +
                                                   pDesc->handleIndex + arrayStart + arr);
                    }
                }
                break;
            }
#ifdef D3D12_RAYTRACING_AVAILABLE
            case DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE:
            {
                VALIDATE_DESCRIPTOR(pParam->ppAccelerationStructures, "NULL Acceleration Structure (%s)", pDesc->pName);

                for (uint32_t arr = 0; arr < arrayCount; ++arr)
                {
                    VALIDATE_DESCRIPTOR(pParam->ppAccelerationStructures[arr], "Acceleration Structure (%s [%u] )", pDesc->pName, arr);

                    DxDescriptorID handle = D3D12_DESCRIPTOR_ID_NONE;
                    fillRaytracingDescriptorHandle(pParam->ppAccelerationStructures[arr], &handle);

                    VALIDATE_DESCRIPTOR(handle != D3D12_DESCRIPTOR_ID_NONE, "Invalid Acceleration Structure (%s [%u] )", pDesc->pName, arr);

                    copy_descriptor_handle(pRenderer->dx.pCPUDescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV], handle,
                                           pRenderer->dx.pCbvSrvUavHeaps[nodeIndex],
                                           pDescriptorSet->dx.cbvSrvUavHandle + index * pDescriptorSet->dx.cbvSrvUavStride +
                                               pDesc->handleIndex + arrayStart + arr);
                }
                break;
            }
#endif
            default:
                break;
            }
        }
    }
}

static bool ResetRootSignature(Cmd* pCmd, PipelineType type, const RootSignature* pRootSignature)
{
    if (pCmd->dx.pBoundRootSignature && pCmd->dx.pBoundRootSignature->dx.pRootSignature == pRootSignature->dx.pRootSignature)
    {
        return false;
    }

    // Set root signature if the current one differs from pRootSignature
    pCmd->dx.pBoundRootSignature = pRootSignature;

    if (type == PIPELINE_TYPE_GRAPHICS)
        pCmd->dx.pCmdList->SetGraphicsRootSignature(pRootSignature->dx.pRootSignature);
    else
        pCmd->dx.pCmdList->SetComputeRootSignature(pRootSignature->dx.pRootSignature);

    for (uint32_t i = 0; i < DESCRIPTOR_UPDATE_FREQ_COUNT; ++i)
    {
        pCmd->dx.pBoundDescriptorSets[i] = NULL;
        pCmd->dx.boundDescriptorSetIndices[i] = (uint16_t)-1;
    }

    return true;
}

void d3d12_cmdBindDescriptorSet(Cmd* pCmd, uint32_t index, DescriptorSet* pDescriptorSet)
{
    ASSERT(pCmd);
    ASSERT(pDescriptorSet);
    ASSERT(index < pDescriptorSet->dx.maxSets);

    const DescriptorUpdateFrequency updateFreq = (DescriptorUpdateFrequency)pDescriptorSet->dx.updateFrequency;

    // Set root signature if the current one differs from pRootSignature
    ResetRootSignature(pCmd, (PipelineType)pDescriptorSet->dx.pipelineType, pDescriptorSet->dx.pRootSignature);

    if (pCmd->dx.boundDescriptorSetIndices[pDescriptorSet->dx.updateFrequency] != index ||
        pCmd->dx.pBoundDescriptorSets[pDescriptorSet->dx.updateFrequency] != pDescriptorSet)
    {
        pCmd->dx.pBoundDescriptorSets[pDescriptorSet->dx.updateFrequency] = pDescriptorSet;
        pCmd->dx.boundDescriptorSetIndices[pDescriptorSet->dx.updateFrequency] = (uint16_t)index;

        // Bind the descriptor tables associated with this DescriptorSet
        if (pDescriptorSet->dx.pipelineType == PIPELINE_TYPE_GRAPHICS)
        {
            if (pDescriptorSet->dx.cbvSrvUavHandle != D3D12_DESCRIPTOR_ID_NONE)
            {
                pCmd->dx.pCmdList->SetGraphicsRootDescriptorTable(
                    pDescriptorSet->dx.cbvSrvUavRootIndex,
                    descriptor_id_to_gpu_handle(pCmd->dx.pBoundHeaps[0],
                                                pDescriptorSet->dx.cbvSrvUavHandle + index * pDescriptorSet->dx.cbvSrvUavStride));
            }

            if (pDescriptorSet->dx.samplerHandle != D3D12_DESCRIPTOR_ID_NONE)
            {
                pCmd->dx.pCmdList->SetGraphicsRootDescriptorTable(
                    pDescriptorSet->dx.samplerRootIndex,
                    descriptor_id_to_gpu_handle(pCmd->dx.pBoundHeaps[1],
                                                pDescriptorSet->dx.samplerHandle + index * pDescriptorSet->dx.samplerStride));
            }
        }
        else
        {
            if (pDescriptorSet->dx.cbvSrvUavHandle != D3D12_DESCRIPTOR_ID_NONE)
            {
                pCmd->dx.pCmdList->SetComputeRootDescriptorTable(
                    pDescriptorSet->dx.cbvSrvUavRootIndex,
                    descriptor_id_to_gpu_handle(pCmd->dx.pBoundHeaps[0],
                                                pDescriptorSet->dx.cbvSrvUavHandle + index * pDescriptorSet->dx.cbvSrvUavStride));
            }

            if (pDescriptorSet->dx.samplerHandle != D3D12_DESCRIPTOR_ID_NONE)
            {
                pCmd->dx.pCmdList->SetComputeRootDescriptorTable(
                    pDescriptorSet->dx.samplerRootIndex,
                    descriptor_id_to_gpu_handle(pCmd->dx.pBoundHeaps[1],
                                                pDescriptorSet->dx.samplerHandle + index * pDescriptorSet->dx.samplerStride));
            }
        }
    }
}

void d3d12_cmdBindPushConstants(Cmd* pCmd, RootSignature* pRootSignature, uint32_t paramIndex, const void* pConstants)
{
    ASSERT(pCmd);
    ASSERT(pConstants);
    ASSERT(pRootSignature);
    ASSERT(paramIndex >= 0 && paramIndex < pRootSignature->descriptorCount);

    // Set root signature if the current one differs from pRootSignature
    ResetRootSignature(pCmd, pRootSignature->pipelineType, pRootSignature);

    const DescriptorInfo* pDesc = pRootSignature->pDescriptors + paramIndex;
    ASSERT(pDesc);
    ASSERT(DESCRIPTOR_TYPE_ROOT_CONSTANT == pDesc->type);

    if (pRootSignature->pipelineType == PIPELINE_TYPE_GRAPHICS)
        pCmd->dx.pCmdList->SetGraphicsRoot32BitConstants(pDesc->handleIndex, pDesc->size, pConstants, 0);
    else
        pCmd->dx.pCmdList->SetComputeRoot32BitConstants(pDesc->handleIndex, pDesc->size, pConstants, 0);
}

void d3d12_cmdBindDescriptorSetWithRootCbvs(Cmd* pCmd, uint32_t index, DescriptorSet* pDescriptorSet, uint32_t count,
                                            const DescriptorData* pParams)
{
    ASSERT(pCmd);
    ASSERT(pDescriptorSet);
    ASSERT(pParams);

    d3d12_cmdBindDescriptorSet(pCmd, index, pDescriptorSet);

    const RootSignature* pRootSignature = pDescriptorSet->dx.pRootSignature;

    for (uint32_t i = 0; i < count; ++i)
    {
        const DescriptorData* pParam = pParams + i;
        uint32_t              paramIndex = pParam->bindByIndex ? pParam->index : UINT32_MAX;

        const DescriptorInfo* pDesc =
            (paramIndex != UINT32_MAX) ? (pRootSignature->pDescriptors + paramIndex) : d3d12_get_descriptor(pRootSignature, pParam->pName);
        if (paramIndex != UINT32_MAX)
        {
            VALIDATE_DESCRIPTOR(pDesc, "Invalid descriptor with param index (%u)", paramIndex);
        }
        else
        {
            VALIDATE_DESCRIPTOR(pDesc, "Invalid descriptor with param name (%s)", pParam->pName);
        }

        VALIDATE_DESCRIPTOR(pDesc->rootDescriptor, "Descriptor (%s) - must be a root cbv", pDesc->pName);
        VALIDATE_DESCRIPTOR(pParam->count <= 1, "Descriptor (%s) - cmdBindDescriptorSetWithRootCbvs does not support arrays",
                            pDesc->pName);
        VALIDATE_DESCRIPTOR(pParam->pRanges, "Descriptor (%s) - pRanges must be provided for cmdBindDescriptorSetWithRootCbvs",
                            pDesc->pName);

        DescriptorDataRange       range = pParam->pRanges[0];
        D3D12_GPU_VIRTUAL_ADDRESS address = pParam->ppBuffers[0]->dx.gpuAddress + range.offset;

        VALIDATE_DESCRIPTOR(range.size > 0, "Descriptor (%s) - pRanges->size is zero", pDesc->pName);
        VALIDATE_DESCRIPTOR(range.size <= D3D12_REQ_CONSTANT_BUFFER_SIZE, "Descriptor (%s) - pRanges->size is %u which exceeds max %u",
                            pDesc->pName, range.size, D3D12_REQ_CONSTANT_BUFFER_SIZE);

        if (pRootSignature->pipelineType == PIPELINE_TYPE_GRAPHICS)
        {
            pCmd->dx.pCmdList->SetGraphicsRootConstantBufferView(pDesc->handleIndex, address); //-V522
        }
        else
        {
            pCmd->dx.pCmdList->SetComputeRootConstantBufferView(pDesc->handleIndex, address); //-V522
        }
    }
}
/************************************************************************/
// Pipeline State Functions
/************************************************************************/
void addGraphicsPipeline(Renderer* pRenderer, const PipelineDesc* pMainDesc, Pipeline** ppPipeline)
{
    ASSERT(pRenderer);
    ASSERT(ppPipeline);
    ASSERT(pMainDesc);

    const GraphicsPipelineDesc* pDesc = &pMainDesc->graphicsDesc;

    ASSERT(pDesc->pShaderProgram);
    ASSERT(pDesc->pRootSignature);

    // allocate new pipeline
    Pipeline* pPipeline = (Pipeline*)tf_calloc_memalign(1, alignof(Pipeline), sizeof(Pipeline));
    ASSERT(pPipeline);

    const Shader*       pShaderProgram = pDesc->pShaderProgram;
    const VertexLayout* pVertexLayout = pDesc->pVertexLayout;

#ifndef DISABLE_PIPELINE_LIBRARY
    ID3D12PipelineLibrary* psoCache = pMainDesc->pCache ? pMainDesc->pCache->dx.pLibrary : NULL;

    size_t psoShaderHash = 0;
    size_t psoRenderHash = 0;
#endif

    pPipeline->dx.type = PIPELINE_TYPE_GRAPHICS;
    pPipeline->dx.pRootSignature = pDesc->pRootSignature;

    // add to gpu
    DECLARE_ZERO(D3D12_SHADER_BYTECODE, VS);
    DECLARE_ZERO(D3D12_SHADER_BYTECODE, PS);
    DECLARE_ZERO(D3D12_SHADER_BYTECODE, DS);
    DECLARE_ZERO(D3D12_SHADER_BYTECODE, HS);
    DECLARE_ZERO(D3D12_SHADER_BYTECODE, GS);
    if (pShaderProgram->stages & SHADER_STAGE_VERT)
    {
        VS.BytecodeLength = pShaderProgram->dx.pShaderBlobs[pShaderProgram->pReflection->vertexStageIndex]->GetBufferSize();
        VS.pShaderBytecode = pShaderProgram->dx.pShaderBlobs[pShaderProgram->pReflection->vertexStageIndex]->GetBufferPointer();
    }
    if (pShaderProgram->stages & SHADER_STAGE_FRAG)
    {
        PS.BytecodeLength = pShaderProgram->dx.pShaderBlobs[pShaderProgram->pReflection->pixelStageIndex]->GetBufferSize();
        PS.pShaderBytecode = pShaderProgram->dx.pShaderBlobs[pShaderProgram->pReflection->pixelStageIndex]->GetBufferPointer();
    }
    if (pShaderProgram->stages & SHADER_STAGE_HULL)
    {
        HS.BytecodeLength = pShaderProgram->dx.pShaderBlobs[pShaderProgram->pReflection->hullStageIndex]->GetBufferSize();
        HS.pShaderBytecode = pShaderProgram->dx.pShaderBlobs[pShaderProgram->pReflection->hullStageIndex]->GetBufferPointer();
    }
    if (pShaderProgram->stages & SHADER_STAGE_DOMN)
    {
        DS.BytecodeLength = pShaderProgram->dx.pShaderBlobs[pShaderProgram->pReflection->domainStageIndex]->GetBufferSize();
        DS.pShaderBytecode = pShaderProgram->dx.pShaderBlobs[pShaderProgram->pReflection->domainStageIndex]->GetBufferPointer();
    }
    if (pShaderProgram->stages & SHADER_STAGE_GEOM)
    {
        GS.BytecodeLength = pShaderProgram->dx.pShaderBlobs[pShaderProgram->pReflection->geometryStageIndex]->GetBufferSize();
        GS.pShaderBytecode = pShaderProgram->dx.pShaderBlobs[pShaderProgram->pReflection->geometryStageIndex]->GetBufferPointer();
    }

    DECLARE_ZERO(D3D12_STREAM_OUTPUT_DESC, stream_output_desc);
    stream_output_desc.pSODeclaration = NULL;
    stream_output_desc.NumEntries = 0;
    stream_output_desc.pBufferStrides = NULL;
    stream_output_desc.NumStrides = 0;
    stream_output_desc.RasterizedStream = 0;

    DECLARE_ZERO(D3D12_DEPTH_STENCILOP_DESC, depth_stencilop_desc);
    depth_stencilop_desc.StencilFailOp = D3D12_STENCIL_OP_KEEP;
    depth_stencilop_desc.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
    depth_stencilop_desc.StencilPassOp = D3D12_STENCIL_OP_KEEP;
    depth_stencilop_desc.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;

    uint32_t input_elementCount = 0;
    DECLARE_ZERO(D3D12_INPUT_ELEMENT_DESC, input_elements[MAX_VERTEX_ATTRIBS]);

    DECLARE_ZERO(char, semantic_names[MAX_VERTEX_ATTRIBS][MAX_SEMANTIC_NAME_LENGTH]);
    // Make sure there's attributes
    if (pVertexLayout != NULL)
    {
        ASSERT(pVertexLayout->attribCount && pVertexLayout->bindingCount);

        for (uint32_t attrib_index = 0; attrib_index < pVertexLayout->attribCount; ++attrib_index)
        {
            const VertexAttrib* attrib = &(pVertexLayout->attribs[attrib_index]);

            ASSERT(SEMANTIC_UNDEFINED != attrib->semantic);
            ASSERT(attrib->binding < pVertexLayout->bindingCount);

            if (attrib->semanticNameLength > 0)
            {
                uint32_t name_length = min((uint32_t)MAX_SEMANTIC_NAME_LENGTH, attrib->semanticNameLength);
                strncpy_s(semantic_names[attrib_index], attrib->semanticName, name_length);
            }
            else
            {
                switch (attrib->semantic)
                {
                case SEMANTIC_POSITION:
                    strcpy_s(semantic_names[attrib_index], "POSITION");
                    break;
                case SEMANTIC_NORMAL:
                    strcpy_s(semantic_names[attrib_index], "NORMAL");
                    break;
                case SEMANTIC_COLOR:
                    strcpy_s(semantic_names[attrib_index], "COLOR");
                    break;
                case SEMANTIC_TANGENT:
                    strcpy_s(semantic_names[attrib_index], "TANGENT");
                    break;
                case SEMANTIC_BITANGENT:
                    strcpy_s(semantic_names[attrib_index], "BITANGENT");
                    break;
                case SEMANTIC_JOINTS:
                    strcpy_s(semantic_names[attrib_index], "JOINTS");
                    break;
                case SEMANTIC_WEIGHTS:
                    strcpy_s(semantic_names[attrib_index], "WEIGHTS");
                    break;
                case SEMANTIC_CUSTOM:
                    strcpy_s(semantic_names[attrib_index], "CUSTOM");
                    break;
                case SEMANTIC_TEXCOORD0:
                case SEMANTIC_TEXCOORD1:
                case SEMANTIC_TEXCOORD2:
                case SEMANTIC_TEXCOORD3:
                case SEMANTIC_TEXCOORD4:
                case SEMANTIC_TEXCOORD5:
                case SEMANTIC_TEXCOORD6:
                case SEMANTIC_TEXCOORD7:
                case SEMANTIC_TEXCOORD8:
                case SEMANTIC_TEXCOORD9:
                    strcpy_s(semantic_names[attrib_index], "TEXCOORD");
                    break;
                default:
                    ASSERT(false);
                    break;
                }
            }

            UINT semantic_index = 0;
            switch (attrib->semantic)
            {
            case SEMANTIC_TEXCOORD0:
                semantic_index = 0;
                break;
            case SEMANTIC_TEXCOORD1:
                semantic_index = 1;
                break;
            case SEMANTIC_TEXCOORD2:
                semantic_index = 2;
                break;
            case SEMANTIC_TEXCOORD3:
                semantic_index = 3;
                break;
            case SEMANTIC_TEXCOORD4:
                semantic_index = 4;
                break;
            case SEMANTIC_TEXCOORD5:
                semantic_index = 5;
                break;
            case SEMANTIC_TEXCOORD6:
                semantic_index = 6;
                break;
            case SEMANTIC_TEXCOORD7:
                semantic_index = 7;
                break;
            case SEMANTIC_TEXCOORD8:
                semantic_index = 8;
                break;
            case SEMANTIC_TEXCOORD9:
                semantic_index = 9;
                break;
            default:
                break;
            }

            input_elements[input_elementCount].SemanticName = semantic_names[attrib_index];
            input_elements[input_elementCount].SemanticIndex = semantic_index;

            input_elements[input_elementCount].Format = (DXGI_FORMAT)TinyImageFormat_ToDXGI_FORMAT((TinyImageFormat)attrib->format);
            input_elements[input_elementCount].InputSlot = attrib->binding;
            input_elements[input_elementCount].AlignedByteOffset = attrib->offset;
            if (pVertexLayout->bindings[attrib->binding].rate == VERTEX_BINDING_RATE_INSTANCE)
            {
                input_elements[input_elementCount].InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA;
                input_elements[input_elementCount].InstanceDataStepRate = 1;
            }
            else
            {
                input_elements[input_elementCount].InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
                input_elements[input_elementCount].InstanceDataStepRate = 0;
            }

#ifndef DISABLE_PIPELINE_LIBRARY
            if (psoCache)
            {
                psoRenderHash = tf_mem_hash<uint8_t>((uint8_t*)&attrib->semantic, sizeof(ShaderSemantic), psoRenderHash);
                psoRenderHash = tf_mem_hash<uint8_t>((uint8_t*)&attrib->format, sizeof(hz::Format), psoRenderHash);
                psoRenderHash = tf_mem_hash<uint8_t>((uint8_t*)&attrib->binding, sizeof(uint32_t), psoRenderHash);
                psoRenderHash = tf_mem_hash<uint8_t>((uint8_t*)&attrib->location, sizeof(uint32_t), psoRenderHash);
                psoRenderHash = tf_mem_hash<uint8_t>((uint8_t*)&attrib->offset, sizeof(uint32_t), psoRenderHash);
                psoRenderHash = tf_mem_hash<uint8_t>((uint8_t*)&pVertexLayout->bindings[attrib->binding].rate, sizeof(VertexBindingRate),
                                                     psoRenderHash);
            }
#endif

            ++input_elementCount;
        }
    }

    DECLARE_ZERO(D3D12_INPUT_LAYOUT_DESC, input_layout_desc);
    input_layout_desc.pInputElementDescs = input_elementCount ? input_elements : NULL;
    input_layout_desc.NumElements = input_elementCount;

    uint32_t render_target_count = min(pDesc->renderTargetCount, (uint32_t)MAX_RENDER_TARGET_ATTACHMENTS);
    render_target_count = min(render_target_count, (uint32_t)D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT);

    DECLARE_ZERO(DXGI_SAMPLE_DESC, sample_desc);
    sample_desc.Count = (UINT)(pDesc->sampleCount);
    sample_desc.Quality = (UINT)(pDesc->sampleQuality);

    DECLARE_ZERO(D3D12_CACHED_PIPELINE_STATE, cached_pso_desc);
    cached_pso_desc.pCachedBlob = NULL;
    cached_pso_desc.CachedBlobSizeInBytes = 0;

    DECLARE_ZERO(D3D12_GRAPHICS_PIPELINE_STATE_DESC, pipeline_state_desc);
    pipeline_state_desc.pRootSignature = pDesc->pRootSignature->dx.pRootSignature;
    pipeline_state_desc.VS = VS;
    pipeline_state_desc.PS = PS;
    pipeline_state_desc.DS = DS;
    pipeline_state_desc.HS = HS;
    pipeline_state_desc.GS = GS;
    pipeline_state_desc.StreamOutput = stream_output_desc;
    pipeline_state_desc.BlendState = pDesc->pBlendState ? util_to_blend_desc(pDesc->pBlendState) : gDefaultBlendDesc;
    pipeline_state_desc.SampleMask = UINT_MAX;
    pipeline_state_desc.RasterizerState =
        pDesc->pRasterizerState ? util_to_rasterizer_desc(pDesc->pRasterizerState) : gDefaultRasterizerDesc;
    pipeline_state_desc.DepthStencilState = pDesc->pDepthState ? util_to_depth_desc(pDesc->pDepthState) : gDefaultDepthDesc;

    pipeline_state_desc.InputLayout = input_layout_desc;
    pipeline_state_desc.IBStripCutValue = D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED;
    pipeline_state_desc.PrimitiveTopologyType = util_to_dx12_primitive_topology_type(pDesc->primitiveTopo);
    pipeline_state_desc.NumRenderTargets = render_target_count;
    pipeline_state_desc.DSVFormat = (DXGI_FORMAT)TinyImageFormat_ToDXGI_FORMAT((TinyImageFormat)pDesc->depthStencilFormat);

    pipeline_state_desc.SampleDesc = sample_desc;
    pipeline_state_desc.CachedPSO = cached_pso_desc;
    pipeline_state_desc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;

    for (uint32_t attrib_index = 0; attrib_index < render_target_count; ++attrib_index)
    {
        pipeline_state_desc.RTVFormats[attrib_index] = (DXGI_FORMAT)TinyImageFormat_ToDXGI_FORMAT((TinyImageFormat)pDesc->pColorFormats[attrib_index]);
    }

    pipeline_state_desc.NodeMask = 0;

    HRESULT result = E_FAIL;

#ifndef DISABLE_PIPELINE_LIBRARY
    wchar_t pipelineName[MAX_DEBUG_NAME_LENGTH + 32] = {};
    if (psoCache)
    {
        psoShaderHash = tf_mem_hash<uint8_t>((uint8_t*)VS.pShaderBytecode, VS.BytecodeLength, psoShaderHash);
        psoShaderHash = tf_mem_hash<uint8_t>((uint8_t*)PS.pShaderBytecode, PS.BytecodeLength, psoShaderHash);
        psoShaderHash = tf_mem_hash<uint8_t>((uint8_t*)HS.pShaderBytecode, HS.BytecodeLength, psoShaderHash);
        psoShaderHash = tf_mem_hash<uint8_t>((uint8_t*)DS.pShaderBytecode, DS.BytecodeLength, psoShaderHash);
        psoShaderHash = tf_mem_hash<uint8_t>((uint8_t*)GS.pShaderBytecode, GS.BytecodeLength, psoShaderHash);

        psoRenderHash = tf_mem_hash<uint8_t>((uint8_t*)&pipeline_state_desc.BlendState, sizeof(D3D12_BLEND_DESC), psoRenderHash);
        psoRenderHash =
            tf_mem_hash<uint8_t>((uint8_t*)&pipeline_state_desc.DepthStencilState, sizeof(D3D12_DEPTH_STENCIL_DESC), psoRenderHash);
        psoRenderHash = tf_mem_hash<uint8_t>((uint8_t*)&pipeline_state_desc.RasterizerState, sizeof(D3D12_RASTERIZER_DESC), psoRenderHash);

        psoRenderHash =
            tf_mem_hash<uint8_t>((uint8_t*)pipeline_state_desc.RTVFormats, render_target_count * sizeof(DXGI_FORMAT), psoRenderHash);
        psoRenderHash = tf_mem_hash<uint8_t>((uint8_t*)&pipeline_state_desc.DSVFormat, sizeof(DXGI_FORMAT), psoRenderHash);
        psoRenderHash = tf_mem_hash<uint8_t>((uint8_t*)&pipeline_state_desc.PrimitiveTopologyType, sizeof(D3D12_PRIMITIVE_TOPOLOGY_TYPE),
                                             psoRenderHash);
        psoRenderHash = tf_mem_hash<uint8_t>((uint8_t*)&pipeline_state_desc.SampleDesc, sizeof(DXGI_SAMPLE_DESC), psoRenderHash);
        psoRenderHash = tf_mem_hash<uint8_t>((uint8_t*)&pipeline_state_desc.NodeMask, sizeof(UINT), psoRenderHash);

        swprintf(pipelineName, L"%S_S%zuR%zu", (pMainDesc->pName ? pMainDesc->pName : "GRAPHICSPSO"), psoShaderHash, psoRenderHash);
        result = psoCache->LoadGraphicsPipeline(pipelineName, &pipeline_state_desc, IID_ARGS(&pPipeline->dx.pPipelineState));
    }
#endif

    if (!SUCCEEDED(result))
    {
        CHECK_HRESULT(hook_create_graphics_pipeline_state(pRenderer->dx.pDevice, &pipeline_state_desc, pMainDesc->pPipelineExtensions,
                                                          pMainDesc->extensionCount, &pPipeline->dx.pPipelineState));

#ifndef DISABLE_PIPELINE_LIBRARY
        if (psoCache)
        {
            CHECK_HRESULT(psoCache->StorePipeline(pipelineName, pPipeline->dx.pPipelineState));
        }
#endif
    }

    D3D_PRIMITIVE_TOPOLOGY topology = D3D_PRIMITIVE_TOPOLOGY_UNDEFINED;
    switch (pDesc->primitiveTopo)
    {
    case PRIMITIVE_TOPO_POINT_LIST:
        topology = D3D_PRIMITIVE_TOPOLOGY_POINTLIST;
        break;
    case PRIMITIVE_TOPO_LINE_LIST:
        topology = D3D_PRIMITIVE_TOPOLOGY_LINELIST;
        break;
    case PRIMITIVE_TOPO_LINE_STRIP:
        topology = D3D_PRIMITIVE_TOPOLOGY_LINESTRIP;
        break;
    case PRIMITIVE_TOPO_TRI_LIST:
        topology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
        break;
    case PRIMITIVE_TOPO_TRI_STRIP:
        topology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;
        break;
    case PRIMITIVE_TOPO_PATCH_LIST:
    {
        const PipelineReflection* pReflection = pDesc->pShaderProgram->pReflection;
        uint32_t                  controlPoint = pReflection->stageReflections[pReflection->hullStageIndex].numControlPoint;
        topology = (D3D_PRIMITIVE_TOPOLOGY)(D3D_PRIMITIVE_TOPOLOGY_1_CONTROL_POINT_PATCHLIST + (controlPoint - 1));
    }
    break;

    default:
        break;
    }

    ASSERT(D3D_PRIMITIVE_TOPOLOGY_UNDEFINED != topology);
    pPipeline->dx.primitiveTopology = topology;

    *ppPipeline = pPipeline;
}

void addComputePipeline(Renderer* pRenderer, const PipelineDesc* pMainDesc, Pipeline** ppPipeline)
{
    ASSERT(pRenderer);
    ASSERT(ppPipeline);
    ASSERT(pMainDesc);

    const ComputePipelineDesc* pDesc = &pMainDesc->computeDesc;

    ASSERT(pDesc->pShaderProgram);
    ASSERT(pDesc->pRootSignature);
    ASSERT(pDesc->pShaderProgram->dx.pShaderBlobs[0]);

    // allocate new pipeline
    Pipeline* pPipeline = (Pipeline*)tf_calloc_memalign(1, alignof(Pipeline), sizeof(Pipeline));
    ASSERT(pPipeline);

    pPipeline->dx.type = PIPELINE_TYPE_COMPUTE;
    pPipeline->dx.pRootSignature = pDesc->pRootSignature;

    // add pipeline specifying its for compute purposes
    DECLARE_ZERO(D3D12_SHADER_BYTECODE, CS);
    CS.BytecodeLength = pDesc->pShaderProgram->dx.pShaderBlobs[0]->GetBufferSize();
    CS.pShaderBytecode = pDesc->pShaderProgram->dx.pShaderBlobs[0]->GetBufferPointer();

    DECLARE_ZERO(D3D12_CACHED_PIPELINE_STATE, cached_pso_desc);
    cached_pso_desc.pCachedBlob = NULL;
    cached_pso_desc.CachedBlobSizeInBytes = 0;

    DECLARE_ZERO(D3D12_COMPUTE_PIPELINE_STATE_DESC, pipeline_state_desc);
    pipeline_state_desc.pRootSignature = pDesc->pRootSignature->dx.pRootSignature;
    pipeline_state_desc.CS = CS;
    pipeline_state_desc.CachedPSO = cached_pso_desc;

#if !defined(XBOX)
    pipeline_state_desc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
#endif

    pipeline_state_desc.NodeMask = 0;

    HRESULT result = E_FAIL;
#ifndef DISABLE_PIPELINE_LIBRARY
    ID3D12PipelineLibrary* psoCache = pMainDesc->pCache ? pMainDesc->pCache->dx.pLibrary : NULL;
    wchar_t                pipelineName[MAX_DEBUG_NAME_LENGTH + 32] = {};

    if (psoCache)
    {
        size_t psoShaderHash = 0;
        psoShaderHash = tf_mem_hash<uint8_t>((uint8_t*)CS.pShaderBytecode, CS.BytecodeLength, psoShaderHash);

        swprintf(pipelineName, L"%S_S%zu", (pMainDesc->pName ? pMainDesc->pName : "COMPUTEPSO"), psoShaderHash);
        result = psoCache->LoadComputePipeline(pipelineName, &pipeline_state_desc, IID_ARGS(&pPipeline->dx.pPipelineState));
    }
#endif

    if (!SUCCEEDED(result))
    {
        CHECK_HRESULT(hook_create_compute_pipeline_state(pRenderer->dx.pDevice, &pipeline_state_desc, pMainDesc->pPipelineExtensions,
                                                         pMainDesc->extensionCount, &pPipeline->dx.pPipelineState));

#ifndef DISABLE_PIPELINE_LIBRARY
        if (psoCache)
        {
            CHECK_HRESULT(psoCache->StorePipeline(pipelineName, pPipeline->dx.pPipelineState));
        }
#endif
    }

    *ppPipeline = pPipeline;
}

void d3d12_addPipeline(Renderer* pRenderer, const PipelineDesc* pDesc, Pipeline** ppPipeline)
{
    switch (pDesc->type)
    {
    case (PIPELINE_TYPE_COMPUTE):
    {
        addComputePipeline(pRenderer, pDesc, ppPipeline);
        break;
    }
    case (PIPELINE_TYPE_GRAPHICS):
    {
        addGraphicsPipeline(pRenderer, pDesc, ppPipeline);
        break;
    }
    default:
    {
        ASSERTFAIL("Unknown pipeline type %i", pDesc->type);
        *ppPipeline = {};
        break;
    }
    }
}

void d3d12_removePipeline(Renderer* pRenderer, Pipeline* pPipeline)
{
    ASSERT(pRenderer);
    ASSERT(pPipeline);

    // delete pipeline from device
    hook_remove_pipeline(pPipeline);

    SAFE_FREE(pPipeline);
}

void d3d12_addPipelineCache(Renderer* pRenderer, const PipelineCacheDesc* pDesc, PipelineCache** ppPipelineCache)
{
    UNREF_PARAM(pRenderer);
    UNREF_PARAM(pDesc);
    UNREF_PARAM(ppPipelineCache);
#ifndef DISABLE_PIPELINE_LIBRARY
    ASSERT(pRenderer);
    ASSERT(pDesc);
    ASSERT(ppPipelineCache);

    PipelineCache* pPipelineCache = (PipelineCache*)tf_calloc(1, sizeof(PipelineCache));
    ASSERT(pPipelineCache);

    if (pDesc->size)
    {
        // D3D12 does not copy pipeline cache data. We have to keep it around until the cache is alive
        pPipelineCache->dx.pData = tf_malloc(pDesc->size);
        memcpy(pPipelineCache->dx.pData, pDesc->pData, pDesc->size);
    }

    D3D12_FEATURE_DATA_SHADER_CACHE feature = {};
    HRESULT result = pRenderer->dx.pDevice->CheckFeatureSupport(D3D12_FEATURE_SHADER_CACHE, &feature, sizeof(feature));
    if (SUCCEEDED(result))
    {
        result = E_NOTIMPL;
        if (feature.SupportFlags & D3D12_SHADER_CACHE_SUPPORT_LIBRARY)
        {
            ID3D12Device1* device1 = NULL;
            result = pRenderer->dx.pDevice->QueryInterface(IID_ARGS(&device1));
            if (SUCCEEDED(result))
            {
                result = device1->CreatePipelineLibrary(pPipelineCache->dx.pData, pDesc->size, IID_ARGS(&pPipelineCache->dx.pLibrary));
            }
            SAFE_RELEASE(device1);
        }
    }

    if (!SUCCEEDED(result))
    {
        LOGF(eWARNING, "Pipeline Cache Library feature is not present. Pipeline Cache will be disabled");
    }

    *ppPipelineCache = pPipelineCache;
#endif
}

void d3d12_removePipelineCache(Renderer* pRenderer, PipelineCache* pPipelineCache)
{
    UNREF_PARAM(pRenderer);
    UNREF_PARAM(pPipelineCache);
#ifndef DISABLE_PIPELINE_LIBRARY
    ASSERT(pRenderer);
    ASSERT(pPipelineCache);

    SAFE_RELEASE(pPipelineCache->dx.pLibrary);
    SAFE_FREE(pPipelineCache->dx.pData);
    SAFE_FREE(pPipelineCache);
#endif
}

void d3d12_getPipelineCacheData(Renderer* pRenderer, PipelineCache* pPipelineCache, size_t* pSize, void* pData)
{
    UNREF_PARAM(pRenderer);
    UNREF_PARAM(pPipelineCache);
    UNREF_PARAM(pData);
    ASSERT(pSize);

    *pSize = 0;

#ifndef DISABLE_PIPELINE_LIBRARY
    ASSERT(pRenderer);
    ASSERT(pPipelineCache);

    if (pPipelineCache->dx.pLibrary)
    {
        *pSize = pPipelineCache->dx.pLibrary->GetSerializedSize();
        if (pData)
        {
            CHECK_HRESULT(pPipelineCache->dx.pLibrary->Serialize(pData, *pSize));
        }
    }
#endif
}
/************************************************************************/
// Command buffer Functions
/************************************************************************/
void d3d12_resetCmdPool(Renderer* pRenderer, CmdPool* pCmdPool)
{
    ASSERT(pRenderer);
    ASSERT(pCmdPool);

    CHECK_HRESULT(pCmdPool->pCmdAlloc->Reset());
}

void d3d12_beginCmd(Cmd* pCmd)
{
    ASSERT(pCmd);
    ASSERT(pCmd->dx.pCmdList);

    CHECK_HRESULT(pCmd->dx.pCmdList->Reset(pCmd->dx.pCmdPool->pCmdAlloc, NULL));

    if (pCmd->dx.type != QUEUE_TYPE_TRANSFER)
    {
        ID3D12DescriptorHeap* heaps[] = {
            pCmd->dx.pBoundHeaps[0]->pHeap,
            pCmd->dx.pBoundHeaps[1]->pHeap,
        };
        pCmd->dx.pCmdList->SetDescriptorHeaps(2, heaps);

        pCmd->dx.boundHeapStartHandles[0] = pCmd->dx.pBoundHeaps[0]->pHeap->GetGPUDescriptorHandleForHeapStart();
        pCmd->dx.boundHeapStartHandles[1] = pCmd->dx.pBoundHeaps[1]->pHeap->GetGPUDescriptorHandleForHeapStart();
    }

    // Reset CPU side data
    pCmd->dx.pBoundRootSignature = NULL;
    for (uint32_t i = 0; i < DESCRIPTOR_UPDATE_FREQ_COUNT; ++i)
    {
        pCmd->dx.pBoundDescriptorSets[i] = NULL;
        pCmd->dx.boundDescriptorSetIndices[i] = (uint16_t)-1;
    }

#if defined(XBOX)
    pCmd->dx.sampleCount = 0;
#endif
}

void d3d12_endCmd(Cmd* pCmd)
{
    ASSERT(pCmd);
    ASSERT(pCmd->dx.pCmdList);

    CHECK_HRESULT(pCmd->dx.pCmdList->Close());
}

void d3d12_cmdBindRenderTargets(Cmd* pCmd, const BindRenderTargetsDesc* pDesc)
{
    ASSERT(pCmd);
    ASSERT(pCmd->dx.pCmdList);

    if (!pDesc)
    {
        return;
    }

    if (!pDesc->renderTargetCount && !pDesc->depthStencil.pDepthStencil)
    {
        pCmd->dx.pCmdList->OMSetRenderTargets(0, NULL, FALSE, NULL);
        return;
    }

    D3D12_CPU_DESCRIPTOR_HANDLE dsv = {};
    D3D12_CPU_DESCRIPTOR_HANDLE rtvs[MAX_RENDER_TARGET_ATTACHMENTS] = {};
    const bool                  hasDepth = pDesc->depthStencil.pDepthStencil;

    for (uint32_t i = 0; i < pDesc->renderTargetCount; ++i)
    {
        const BindRenderTargetDesc* desc = &pDesc->renderTargets[i];
#if defined(XBOX)
        pCmd->dx.sampleCount = desc->pRenderTarget->sampleCount;
#endif
        if (!desc->useMipSlice && !desc->useArraySlice)
        {
            rtvs[i] = descriptor_id_to_cpu_handle(pCmd->pRenderer->dx.pCPUDescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_RTV],
                                                  desc->pRenderTarget->dx.descriptors);
        }
        else
        {
            uint32_t handle = 0;
            if (desc->useMipSlice)
            {
                if (desc->useArraySlice)
                {
                    handle = 1 + desc->mipSlice * (uint32_t)desc->pRenderTarget->arraySize + desc->arraySlice;
                }
                else
                {
                    handle = 1 + desc->mipSlice;
                }
            }
            else if (desc->useArraySlice)
            {
                handle = 1 + desc->arraySlice;
            }

            rtvs[i] = descriptor_id_to_cpu_handle(pCmd->pRenderer->dx.pCPUDescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_RTV],
                                                  desc->pRenderTarget->dx.descriptors + handle);
        }

        if (desc->loadAction == LOAD_ACTION_CLEAR)
        {
            const float* clearValue = desc->overrideClearValue ? &desc->clearValue.r : &desc->pRenderTarget->clearValue.r;
            pCmd->dx.pCmdList->ClearRenderTargetView(rtvs[i], clearValue, 0, NULL);
        }
    }

    if (hasDepth)
    {
        const BindDepthTargetDesc* desc = &pDesc->depthStencil;
#if defined(XBOX)
        pCmd->dx.sampleCount = desc->pDepthStencil->sampleCount;
#endif

        if (!desc->useMipSlice && !desc->useArraySlice)
        {
            dsv = descriptor_id_to_cpu_handle(pCmd->pRenderer->dx.pCPUDescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_DSV],
                                              desc->pDepthStencil->dx.descriptors);
        }
        else
        {
            uint32_t handle = 0;
            if (desc->useMipSlice)
            {
                if (desc->useArraySlice)
                {
                    handle = 1 + desc->mipSlice * (uint32_t)desc->pDepthStencil->arraySize + desc->arraySlice;
                }
                else
                {
                    handle = 1 + desc->mipSlice;
                }
            }
            else if (desc->useArraySlice)
            {
                handle = 1 + desc->arraySlice;
            }

            dsv = descriptor_id_to_cpu_handle(pCmd->pRenderer->dx.pCPUDescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_DSV],
                                              desc->pDepthStencil->dx.descriptors + handle);
        }

        ASSERT(dsv.ptr != D3D12_GPU_VIRTUAL_ADDRESS_NULL);
        if (desc->loadAction == LOAD_ACTION_CLEAR || desc->loadActionStencil == LOAD_ACTION_CLEAR)
        {
            D3D12_CLEAR_FLAGS flags = (D3D12_CLEAR_FLAGS)0;
            if (desc->loadAction == LOAD_ACTION_CLEAR)
            {
                flags |= D3D12_CLEAR_FLAG_DEPTH;
            }
            if (desc->loadActionStencil == LOAD_ACTION_CLEAR)
            {
                flags |= D3D12_CLEAR_FLAG_STENCIL;
                ASSERT(TinyImageFormat_HasStencil((TinyImageFormat)desc->pDepthStencil->format));
            }
            ASSERT(flags > 0);
            const ClearValue* clearValue = desc->overrideClearValue ? &desc->clearValue : &desc->pDepthStencil->clearValue;
            pCmd->dx.pCmdList->ClearDepthStencilView(dsv, flags, clearValue->depth, (UINT8)clearValue->stencil, 0, NULL);
        }
    }

    pCmd->dx.pCmdList->OMSetRenderTargets(pDesc->renderTargetCount, rtvs, FALSE, dsv.ptr != D3D12_GPU_VIRTUAL_ADDRESS_NULL ? &dsv : NULL);
}

void d3d12_cmdSetViewport(Cmd* pCmd, float x, float y, float width, float height, float minDepth, float maxDepth)
{
    ASSERT(pCmd);

    // set new viewport
    ASSERT(pCmd->dx.pCmdList);

    D3D12_VIEWPORT viewport;
    viewport.TopLeftX = x;
    viewport.TopLeftY = y;
    viewport.Width = width;
    viewport.Height = height;
    viewport.MinDepth = minDepth;
    viewport.MaxDepth = maxDepth;

    pCmd->dx.pCmdList->RSSetViewports(1, &viewport);
}

void d3d12_cmdSetScissor(Cmd* pCmd, uint32_t x, uint32_t y, uint32_t width, uint32_t height)
{
    ASSERT(pCmd);

    // set new scissor values
    ASSERT(pCmd->dx.pCmdList);

    D3D12_RECT scissor;
    scissor.left = x;
    scissor.top = y;
    scissor.right = x + width;
    scissor.bottom = y + height;

    pCmd->dx.pCmdList->RSSetScissorRects(1, &scissor);
}

void d3d12_cmdSetStencilReferenceValue(Cmd* pCmd, uint32_t val)
{
    ASSERT(pCmd);
    ASSERT(pCmd->dx.pCmdList);

    pCmd->dx.pCmdList->OMSetStencilRef(val);
}

void d3d12_cmdSetSampleLocations(Cmd* pCmd, SampleCount samples_count, uint32_t grid_size_x, uint32_t grid_size_y,
                                 SampleLocations* locations)
{
    ASSERT(pCmd);
    ASSERT(pCmd->dx.pCmdList);

    uint32_t sampleLocationsCount = samples_count * grid_size_x * grid_size_y;
    ASSERT(sampleLocationsCount <= 16);

    D3D12_SAMPLE_POSITION samplePositions[16] = {};
    for (uint32_t i = 0; i < sampleLocationsCount; ++i)
        samplePositions[i] = { locations[i].x, locations[i].y };

    pCmd->dx.pCmdList->SetSamplePositions(samples_count, grid_size_x * grid_size_y, samplePositions);
}

void d3d12_cmdBindPipeline(Cmd* pCmd, Pipeline* pPipeline)
{
    ASSERT(pCmd);
    ASSERT(pPipeline);

    // bind given pipeline
    ASSERT(pCmd->dx.pCmdList);

    if (pPipeline->dx.type == PIPELINE_TYPE_GRAPHICS)
    {
        ASSERT(pPipeline->dx.pPipelineState);
        ResetRootSignature(pCmd, pPipeline->dx.type, pPipeline->dx.pRootSignature);
        pCmd->dx.pCmdList->IASetPrimitiveTopology(pPipeline->dx.primitiveTopology);
        pCmd->dx.pCmdList->SetPipelineState(pPipeline->dx.pPipelineState);
    }
    else
    {
        ASSERT(pPipeline->dx.pPipelineState);
        ResetRootSignature(pCmd, pPipeline->dx.type, pPipeline->dx.pRootSignature);
        pCmd->dx.pCmdList->SetPipelineState(pPipeline->dx.pPipelineState);
    }
}

void d3d12_cmdBindIndexBuffer(Cmd* pCmd, Buffer* pBuffer, uint32_t indexType, uint64_t offset)
{
    ASSERT(pCmd);
    ASSERT(pBuffer);
    ASSERT(pCmd->dx.pCmdList);
    ASSERT(D3D12_GPU_VIRTUAL_ADDRESS_NULL != pBuffer->dx.gpuAddress);

    D3D12_INDEX_BUFFER_VIEW ibView = {};
    ibView.BufferLocation = pBuffer->dx.gpuAddress + offset;
    ibView.Format = (INDEX_TYPE_UINT16 == indexType) ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT;
    ibView.SizeInBytes = (UINT)(pBuffer->size - offset);

    // bind given index buffer
    pCmd->dx.pCmdList->IASetIndexBuffer(&ibView);
}

void d3d12_cmdBindVertexBuffer(Cmd* pCmd, uint32_t bufferCount, Buffer** ppBuffers, const uint32_t* pStrides, const uint64_t* pOffsets)
{
    ASSERT(pCmd);
    ASSERT(0 != bufferCount);
    ASSERT(ppBuffers);
    ASSERT(pCmd->dx.pCmdList);
    // bind given vertex buffer

    DECLARE_ZERO(D3D12_VERTEX_BUFFER_VIEW, views[MAX_VERTEX_ATTRIBS]);
    for (uint32_t i = 0; i < bufferCount; ++i)
    {
        ASSERT(D3D12_GPU_VIRTUAL_ADDRESS_NULL != ppBuffers[i]->dx.gpuAddress);

        views[i].BufferLocation = (ppBuffers[i]->dx.gpuAddress + (pOffsets ? pOffsets[i] : 0));
        views[i].SizeInBytes = (UINT)(ppBuffers[i]->size - (pOffsets ? pOffsets[i] : 0));
        views[i].StrideInBytes = (UINT)pStrides[i];
    }

    pCmd->dx.pCmdList->IASetVertexBuffers(0, bufferCount, views);
}

void d3d12_cmdDraw(Cmd* pCmd, uint32_t vertexCount, uint32_t firstVertex)
{
    ASSERT(pCmd);

    // draw given vertices
    ASSERT(pCmd->dx.pCmdList);

    pCmd->dx.pCmdList->DrawInstanced((UINT)vertexCount, (UINT)1, (UINT)firstVertex, (UINT)0);
}

void d3d12_cmdDrawInstanced(Cmd* pCmd, uint32_t vertexCount, uint32_t firstVertex, uint32_t instanceCount, uint32_t firstInstance)
{
    ASSERT(pCmd);

    // draw given vertices
    ASSERT(pCmd->dx.pCmdList);

    pCmd->dx.pCmdList->DrawInstanced((UINT)vertexCount, (UINT)instanceCount, (UINT)firstVertex, (UINT)firstInstance);
}

void d3d12_cmdDrawIndexed(Cmd* pCmd, uint32_t indexCount, uint32_t firstIndex, uint32_t firstVertex)
{
    ASSERT(pCmd);

    // draw indexed mesh
    ASSERT(pCmd->dx.pCmdList);

    pCmd->dx.pCmdList->DrawIndexedInstanced((UINT)indexCount, (UINT)1, (UINT)firstIndex, (UINT)firstVertex, (UINT)0);
}

void d3d12_cmdDrawIndexedInstanced(Cmd* pCmd, uint32_t indexCount, uint32_t firstIndex, uint32_t instanceCount, uint32_t firstVertex,
                                   uint32_t firstInstance)
{
    ASSERT(pCmd);

    // draw indexed mesh
    ASSERT(pCmd->dx.pCmdList);

    pCmd->dx.pCmdList->DrawIndexedInstanced((UINT)indexCount, (UINT)instanceCount, (UINT)firstIndex, (UINT)firstVertex,
                                             (UINT)firstInstance);
}

void d3d12_cmdDispatch(Cmd* pCmd, uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ)
{
    ASSERT(pCmd);

    // dispatch given command
    ASSERT(pCmd->dx.pCmdList != NULL);

#if defined(_WINDOWS) && defined(D3D12_RAYTRACING_AVAILABLE) && defined(FORGE_DEBUG)
    // Bug in validation when using acceleration structure in compute or graphics pipeline
    // D3D12 ERROR: ID3D12CommandList::Dispatch: Static Descriptor SRV resource dimensions (UNKNOWN (11)) differs from that expected by
    // shader (D3D12_SRV_DIMENSION_BUFFER) UNKNOWN (11) is D3D12_SRV_DIMENSION_RAYTRACING_ACCELERATION_STRUCTURE
    if (pCmd->pRenderer->dx.pDebugValidation && pCmd->dx.pBoundRootSignature->dx.hasRayQueryAccelerationStructure)
    {
        D3D12_MESSAGE_ID        hide[] = { D3D12_MESSAGE_ID_COMMAND_LIST_STATIC_DESCRIPTOR_RESOURCE_DIMENSION_MISMATCH };
        D3D12_INFO_QUEUE_FILTER filter = {};
        filter.DenyList.NumIDs = 1;
        filter.DenyList.pIDList = hide;
        pCmd->pRenderer->dx.pDebugValidation->PushStorageFilter(&filter);
    }
#endif

    hook_dispatch(pCmd, groupCountX, groupCountY, groupCountZ);

#if defined(_WINDOWS) && defined(D3D12_RAYTRACING_AVAILABLE) && defined(FORGE_DEBUG)
    if (pCmd->pRenderer->dx.pDebugValidation && pCmd->dx.pBoundRootSignature->dx.hasRayQueryAccelerationStructure)
    {
        pCmd->pRenderer->dx.pDebugValidation->PopStorageFilter();
    }
#endif
}

// void d3d12_cmdBarrier(Cmd* pCmd, const BarrierDesc* pDesc)
// {

// }

void d3d12_cmdResourceBarrier(Cmd* pCmd, uint32_t numBufferBarriers, BufferBarrier* pBufferBarriers, uint32_t numTextureBarriers,
                              TextureBarrier* pTextureBarriers, uint32_t numRtBarriers, RenderTargetBarrier* pRtBarriers)
{
    D3D12_RESOURCE_BARRIER* barriers =
        (D3D12_RESOURCE_BARRIER*)alloca((numBufferBarriers + numTextureBarriers + numRtBarriers) * sizeof(D3D12_RESOURCE_BARRIER));
    uint32_t transitionCount = 0;

#if defined(ENABLE_GRAPHICS_DEBUG) && defined(_WINDOWS)
    ID3D12DebugCommandList* debugCmd = pCmd->dx.pDebugCmdList;
#endif

    for (uint32_t i = 0; i < numBufferBarriers; ++i)
    {
        BufferBarrier*          pTransBarrier = &pBufferBarriers[i];
        D3D12_RESOURCE_BARRIER* pBarrier = &barriers[transitionCount];
        Buffer*                 pBuffer = pTransBarrier->pBuffer;

        // Only transition GPU visible resources.
        // Note: General CPU_TO_GPU resources have to stay in generic read state. They are created in upload heap.
        // There is one corner case: CPU_TO_GPU resources with UAV usage can have state transition. And they are created in custom heap.
        if (pBuffer->memoryUsage == RESOURCE_MEMORY_USAGE_GPU_ONLY || pBuffer->memoryUsage == RESOURCE_MEMORY_USAGE_GPU_TO_CPU ||
            (pBuffer->memoryUsage == RESOURCE_MEMORY_USAGE_CPU_TO_GPU && (pBuffer->descriptors & DESCRIPTOR_TYPE_RW_BUFFER)) ||
            (pBuffer->memoryUsage == RESOURCE_MEMORY_USAGE_GPU_UPLOAD && (pBuffer->descriptors & DESCRIPTOR_TYPE_RW_BUFFER)))
        {
            // if (!(pBuffer->currentState & pTransBarrier->newState) && pBuffer->currentState != pTransBarrier->newState)
            if (RESOURCE_STATE_UNORDERED_ACCESS == pTransBarrier->currentState &&
                RESOURCE_STATE_UNORDERED_ACCESS == pTransBarrier->newState)
            {
                pBarrier->Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
                pBarrier->Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
                pBarrier->UAV.pResource = pBuffer->dx.pResource;
                ++transitionCount;
            }
#ifdef D3D12_RAYTRACING_AVAILABLE
            else if ((RESOURCE_STATE_ACCELERATION_STRUCTURE_WRITE & pTransBarrier->currentState) &&
                     (RESOURCE_STATE_ACCELERATION_STRUCTURE_READ & pTransBarrier->newState))
            {
                pBarrier->Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
                pBarrier->Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
                pBarrier->UAV.pResource = pBuffer->dx.pResource;
                ++transitionCount;
            }
#endif
            else
            {
                pBarrier->Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
                pBarrier->Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
                if (pTransBarrier->beginOnly)
                {
                    pBarrier->Flags = D3D12_RESOURCE_BARRIER_FLAG_BEGIN_ONLY;
                }
                else if (pTransBarrier->endOnly)
                {
                    pBarrier->Flags = D3D12_RESOURCE_BARRIER_FLAG_END_ONLY;
                }
                pBarrier->Transition.pResource = pBuffer->dx.pResource;
                pBarrier->Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
                pBarrier->Transition.StateBefore = util_to_dx12_resource_state(pTransBarrier->currentState);
                pBarrier->Transition.StateAfter = util_to_dx12_resource_state(pTransBarrier->newState);

                ++transitionCount;

#if defined(ENABLE_GRAPHICS_DEBUG) && defined(_WINDOWS)
                if (debugCmd)
                {
                    debugCmd->AssertResourceState(pBarrier->Transition.pResource, pBarrier->Transition.Subresource,
                                                  pBarrier->Transition.StateBefore);
                }
#endif
            }
        }
    }

    for (uint32_t i = 0; i < numTextureBarriers; ++i)
    {
        TextureBarrier*         pTrans = &pTextureBarriers[i];
        D3D12_RESOURCE_BARRIER* pBarrier = &barriers[transitionCount];
        Texture*                pTexture = pTrans->pTexture;

        if (RESOURCE_STATE_UNORDERED_ACCESS == pTrans->currentState && RESOURCE_STATE_UNORDERED_ACCESS == pTrans->newState)
        {
            pBarrier->Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
            pBarrier->Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
            pBarrier->UAV.pResource = pTexture->dx.pResource;
            ++transitionCount;
        }
        else
        {
            pBarrier->Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            pBarrier->Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
            if (pTrans->beginOnly)
            {
                pBarrier->Flags = D3D12_RESOURCE_BARRIER_FLAG_BEGIN_ONLY;
            }
            else if (pTrans->endOnly)
            {
                pBarrier->Flags = D3D12_RESOURCE_BARRIER_FLAG_END_ONLY;
            }
            pBarrier->Transition.pResource = pTexture->dx.pResource;
            pBarrier->Transition.Subresource = pTrans->subresourceBarrier
                                                   ? CALC_SUBRESOURCE_INDEX(pTrans->mipLevel, pTrans->arrayLayer, 0, pTexture->mipLevels,
                                                                            pTexture->arraySizeMinusOne + 1)
                                                   : D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
            if (pTrans->acquire)
                pBarrier->Transition.StateBefore = D3D12_RESOURCE_STATE_COMMON;
            else
                pBarrier->Transition.StateBefore = util_to_dx12_resource_state(pTrans->currentState);

            if (pTrans->release)
                pBarrier->Transition.StateAfter = D3D12_RESOURCE_STATE_COMMON;
            else
                pBarrier->Transition.StateAfter = util_to_dx12_resource_state(pTrans->newState);

            ++transitionCount;

#if defined(ENABLE_GRAPHICS_DEBUG) && defined(_WINDOWS)
            if (debugCmd)
            {
                debugCmd->AssertResourceState(pBarrier->Transition.pResource, pBarrier->Transition.Subresource,
                                              pBarrier->Transition.StateBefore);
            }
#endif
        }
    }

    for (uint32_t i = 0; i < numRtBarriers; ++i)
    {
        RenderTargetBarrier*    pTrans = &pRtBarriers[i];
        D3D12_RESOURCE_BARRIER* pBarrier = &barriers[transitionCount];
        Texture*                pTexture = pTrans->pRenderTarget->pTexture;

        if (RESOURCE_STATE_UNORDERED_ACCESS == pTrans->currentState && RESOURCE_STATE_UNORDERED_ACCESS == pTrans->newState)
        {
            pBarrier->Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
            pBarrier->Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
            pBarrier->UAV.pResource = pTexture->dx.pResource;
            ++transitionCount;
        }
        else
        {
            pBarrier->Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            pBarrier->Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
            if (pTrans->beginOnly)
            {
                pBarrier->Flags = D3D12_RESOURCE_BARRIER_FLAG_BEGIN_ONLY;
            }
            else if (pTrans->endOnly)
            {
                pBarrier->Flags = D3D12_RESOURCE_BARRIER_FLAG_END_ONLY;
            }
            pBarrier->Transition.pResource = pTexture->dx.pResource;
            pBarrier->Transition.Subresource = pTrans->subresourceBarrier
                                                   ? CALC_SUBRESOURCE_INDEX(pTrans->mipLevel, pTrans->arrayLayer, 0, pTexture->mipLevels,
                                                                            pTexture->arraySizeMinusOne + 1)
                                                   : D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
            if (pTrans->acquire)
                pBarrier->Transition.StateBefore = D3D12_RESOURCE_STATE_COMMON;
            else
                pBarrier->Transition.StateBefore = util_to_dx12_resource_state(pTrans->currentState);

            if (pTrans->release)
                pBarrier->Transition.StateAfter = D3D12_RESOURCE_STATE_COMMON;
            else
                pBarrier->Transition.StateAfter = util_to_dx12_resource_state(pTrans->newState);

            ++transitionCount;

#if defined(ENABLE_GRAPHICS_DEBUG) && defined(_WINDOWS)
            if (debugCmd)
            {
                debugCmd->AssertResourceState(pBarrier->Transition.pResource, pBarrier->Transition.Subresource,
                                              pBarrier->Transition.StateBefore);
            }
#endif
        }
    }

    if (transitionCount)
    {
#if defined(XBOX)
        if (pCmd->dx.dma.pCmdList)
        {
            pCmd->dx.dma.pCmdList->ResourceBarrier(transitionCount, barriers);
        }
        else
#endif
        {
            pCmd->dx.pCmdList->ResourceBarrier(transitionCount, barriers);
        }
    }
}

void d3d12_cmdUpdateBuffer(Cmd* pCmd, Buffer* pBuffer, uint64_t dstOffset, Buffer* pSrcBuffer, uint64_t srcOffset, uint64_t size)
{
    ASSERT(pCmd);
    ASSERT(pSrcBuffer);
    ASSERT(pSrcBuffer->dx.pResource);
    ASSERT(pBuffer);
    ASSERT(pBuffer->dx.pResource);
    ASSERT(dstOffset <= pBuffer->size && size <= pBuffer->size - dstOffset);
    ASSERT(srcOffset <= pSrcBuffer->size && size <= pSrcBuffer->size - srcOffset);

#if defined(XBOX)
    if (pCmd->dx.dma.pCmdList)
    {
        pCmd->dx.dma.pCmdList->CopyBufferRegion(pBuffer->dx.pResource, dstOffset, pSrcBuffer->dx.pResource, srcOffset, size);
    }
    else
#endif
    {
        pCmd->dx.pCmdList->CopyBufferRegion(pBuffer->dx.pResource, dstOffset, pSrcBuffer->dx.pResource, srcOffset, size);
    }
}

void d3d12_cmdCopyTexture(Cmd* pCmd, Texture* pDstTexture, Texture* pSrcTexture)
{
    ASSERT(pCmd);
    ASSERT(pDstTexture && pDstTexture->dx.pResource);
    ASSERT(pSrcTexture && pSrcTexture->dx.pResource);

    const D3D12_RESOURCE_DESC dstDesc = pDstTexture->dx.pResource->GetDesc();
    const D3D12_RESOURCE_DESC srcDesc = pSrcTexture->dx.pResource->GetDesc();
    ASSERT(dstDesc.Dimension == srcDesc.Dimension && dstDesc.Width == srcDesc.Width && dstDesc.Height == srcDesc.Height &&
           dstDesc.DepthOrArraySize == srcDesc.DepthOrArraySize && dstDesc.MipLevels == srcDesc.MipLevels &&
           dstDesc.Format == srcDesc.Format && dstDesc.SampleDesc.Count == srcDesc.SampleDesc.Count &&
           dstDesc.SampleDesc.Quality == srcDesc.SampleDesc.Quality);

    pCmd->dx.pCmdList->CopyResource(pDstTexture->dx.pResource, pSrcTexture->dx.pResource);
}

struct SubresourceDataDesc
{
    uint64_t srcOffset;
    uint32_t mipLevel;
    uint32_t arrayLayer;
};

void d3d12_cmdUpdateSubresource(Cmd* pCmd, Texture* pTexture, Buffer* pSrcBuffer, const SubresourceDataDesc* pDesc)
{
    uint32_t subresource =
        CALC_SUBRESOURCE_INDEX(pDesc->mipLevel, pDesc->arrayLayer, 0, pTexture->mipLevels, pTexture->arraySizeMinusOne + 1);
    D3D12_RESOURCE_DESC resourceDesc = pTexture->dx.pResource->GetDesc();
    // FIXME(hyl5): crash otherwise when dbg layer is enabled
    if (resourceDesc.Flags & D3D12_RESOURCE_FLAG_USE_TIGHT_ALIGNMENT)
    {
        resourceDesc.Alignment = 0;
    }

    D3D12_TEXTURE_COPY_LOCATION src = {};
    D3D12_TEXTURE_COPY_LOCATION dst = {};
    src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    src.pResource = pSrcBuffer->dx.pResource;
    pCmd->pRenderer->dx.pDevice->GetCopyableFootprints(&resourceDesc, subresource, 1, pDesc->srcOffset, &src.PlacedFootprint, NULL, NULL,
                                                        NULL);
    src.PlacedFootprint.Offset = pDesc->srcOffset;
    dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    dst.pResource = pTexture->dx.pResource;
    dst.SubresourceIndex = subresource;
#if defined(XBOX)
    if (pCmd->dx.dma.pCmdList)
    {
        pCmd->dx.dma.pCmdList->CopyTextureRegion(&dst, 0, 0, 0, &src, NULL);
    }
    else
#endif
    {
        pCmd->dx.pCmdList->CopyTextureRegion(&dst, 0, 0, 0, &src, NULL);
    }
}

void d3d12_cmdCopySubresource(Cmd* pCmd, Buffer* pDstBuffer, Texture* pTexture, const SubresourceDataDesc* pDesc)
{
    uint32_t subresource =
        CALC_SUBRESOURCE_INDEX(pDesc->mipLevel, pDesc->arrayLayer, 0, pTexture->mipLevels, pTexture->arraySizeMinusOne + 1);
    D3D12_RESOURCE_DESC resourceDesc = pTexture->dx.pResource->GetDesc();
    // FIXME(hyl5): crash otherwise when dbg layer is enabled
    if (resourceDesc.Flags & D3D12_RESOURCE_FLAG_USE_TIGHT_ALIGNMENT)
    {
        resourceDesc.Alignment = 0;
    }

    D3D12_TEXTURE_COPY_LOCATION src = {};
    D3D12_TEXTURE_COPY_LOCATION dst = {};
    src.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    src.pResource = pTexture->dx.pResource;
    src.SubresourceIndex = subresource;
    dst.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    dst.pResource = pDstBuffer->dx.pResource;
    pCmd->pRenderer->dx.pDevice->GetCopyableFootprints(&resourceDesc, subresource, 1, pDesc->srcOffset, &dst.PlacedFootprint, NULL, NULL,
                                                        NULL);
    dst.PlacedFootprint.Offset = pDesc->srcOffset;
#if defined(XBOX)
    if (pCmd->dx.dma.pCmdList)
    {
        pCmd->dx.dma.pCmdList->CopyTextureRegion(&dst, 0, 0, 0, &src, NULL);
    }
    else
#endif
    {
        pCmd->dx.pCmdList->CopyTextureRegion(&dst, 0, 0, 0, &src, NULL);
    }
}

/************************************************************************/
// Queue Fence Semaphore Functions
/************************************************************************/
void d3d12_acquireNextImage(Renderer* pRenderer, SwapChain* pSwapChain, Semaphore* pSignalSemaphore, Fence* pFence,
                            uint32_t* pSwapChainImageIndex)
{
    UNREF_PARAM(pSignalSemaphore);
    UNREF_PARAM(pFence);
    ASSERT(pRenderer);
    ASSERT(pSwapChainImageIndex);

    // get latest backbuffer image
    HRESULT hr = hook_acquire_next_image(pRenderer->dx.pDevice, pSwapChain);
    if (FAILED(hr))
    {
        LOGF(LogLevel::eERROR, "Failed to acquire next image");
        *pSwapChainImageIndex = UINT32_MAX;
        return;
    }

    *pSwapChainImageIndex = hook_get_swapchain_image_index(pSwapChain);
}

void d3d12_queueSubmit(Queue* pQueue, const QueueSubmitDesc* pDesc)
{
    ASSERT(pDesc);

    uint32_t    cmdCount = pDesc->cmdCount;
    Cmd**       pCmds = pDesc->ppCmds;
    Fence*      pFence = pDesc->pSignalFence;
    uint32_t    waitSemaphoreCount = pDesc->waitSemaphoreCount;
    Semaphore** ppWaitSemaphores = pDesc->ppWaitSemaphores;
    uint32_t    signalSemaphoreCount = pDesc->signalSemaphoreCount;
    Semaphore** ppSignalSemaphores = pDesc->ppSignalSemaphores;

    // ASSERT that given cmd list and given params are valid
    ASSERT(pQueue);
    ASSERT(cmdCount > 0);
    ASSERT(pCmds);
    if (waitSemaphoreCount > 0)
    {
        ASSERT(ppWaitSemaphores);
    }
    if (signalSemaphoreCount > 0)
    {
        ASSERT(ppSignalSemaphores);
    }

    // execute given command list
    ASSERT(pQueue->dx.pQueue);

    ID3D12CommandList** cmds = (ID3D12CommandList**)alloca(cmdCount * sizeof(ID3D12CommandList*));
    for (uint32_t i = 0; i < cmdCount; ++i)
    {
        cmds[i] = pCmds[i]->dx.pCmdList;
    }

    for (uint32_t i = 0; i < waitSemaphoreCount; ++i)
    {
        CHECK_HRESULT(pQueue->dx.pQueue->Wait(ppWaitSemaphores[i]->dx.pFence, ppWaitSemaphores[i]->dx.fenceValue));
    }

    pQueue->dx.pQueue->ExecuteCommandLists(cmdCount, cmds);

    if (pFence)
    {
        CHECK_HRESULT(hook_signal(pQueue, pFence->dx.pFence, ++pFence->dx.fenceValue));
    }

    for (uint32_t i = 0; i < signalSemaphoreCount; ++i)
    {
        CHECK_HRESULT(hook_signal(pQueue, ppSignalSemaphores[i]->dx.pFence, ++ppSignalSemaphores[i]->dx.fenceValue));
    }
}

void d3d12_queuePresent(Queue* pQueue, const QueuePresentDesc* pDesc)
{
    if (!pDesc->pSwapChain)
    {
        return;
    }

#if defined(_WINDOWS) && defined(FORGE_DEBUG)
    decltype(pQueue->dx.pRenderer->dx)* pRenderer = &pQueue->dx.pRenderer->dx;
    if (pRenderer->pDebugValidation && pRenderer->suppressMismatchingCommandListDuringPresent)
    {
        D3D12_MESSAGE_ID        hide[] = { D3D12_MESSAGE_ID_RESOURCE_BARRIER_MISMATCHING_COMMAND_LIST_TYPE };
        D3D12_INFO_QUEUE_FILTER filter = {};
        filter.DenyList.NumIDs = 1;
        filter.DenyList.pIDList = hide;
        pRenderer->pDebugValidation->PushStorageFilter(&filter);
    }
#endif

#if defined(AUTOMATED_TESTING)
    // take a screenshot
    captureScreenshot(pDesc->pSwapChain, pDesc->index, true, false);
#endif

    SwapChain* pSwapChain = pDesc->pSwapChain;
    HRESULT    hr = hook_queue_present(pQueue, pSwapChain, pDesc->index);

#if defined(_WINDOWS) && defined(FORGE_DEBUG)
    if (pRenderer->pDebugValidation && pRenderer->suppressMismatchingCommandListDuringPresent)
    {
        pRenderer->pDebugValidation->PopStorageFilter();
    }
#endif

    if (FAILED(hr))
    {
#if defined(_WINDOWS)
        ID3D12Device* device = NULL;
        pSwapChain->dx.pSwapChain->GetDevice(IID_ARGS(&device));
        HRESULT removeHr = device->GetDeviceRemovedReason();

        if (!VERIFY(SUCCEEDED(removeHr)))
        {
            threadSleep(5000); // Wait for a few seconds to allow the driver to come back online before doing a reset.
            ResetDesc resetDesc;
            resetDesc.type = RESET_TYPE_DEVICE_LOST;
            requestReset(&resetDesc);
        }

#if defined(ENABLE_NSIGHT_AFTERMATH)
        // DXGI_ERROR error notification is asynchronous to the NVIDIA display
        // driver's GPU crash handling. Give the Nsight Aftermath GPU crash dump
        // thread some time to do its work before terminating the process.
        threadSleep(3000);
#endif

#if defined(USE_DRED)
        ID3D12DeviceRemovedExtendedData* pDread;
        if (SUCCEEDED(device->QueryInterface(IID_ARGS(&pDread))))
        {
            D3D12_DRED_AUTO_BREADCRUMBS_OUTPUT breadcrumbs;
            if (SUCCEEDED(pDread->GetAutoBreadcrumbsOutput(&breadcrumbs)))
            {
                LOGF(LogLevel::eINFO, "Gathered auto-breadcrumbs output.");
            }

            D3D12_DRED_PAGE_FAULT_OUTPUT pageFault;
            if (SUCCEEDED(pDread->GetPageFaultAllocationOutput(&pageFault)))
            {
                LOGF(LogLevel::eINFO, "Gathered page fault allocation output.");
            }
        }
        pDread->Release();
#endif
        device->Release();
#endif
        LOGF(LogLevel::eERROR, "Failed to present swapchain render target");
    }
}

static inline void GetFenceStatus(Fence* pFence, FenceStatus* pFenceStatus)
{
    if (!pFence->dx.fenceValue)
    {
        *pFenceStatus = FENCE_STATUS_NOTSUBMITTED;
    }
    else if (pFence->dx.pFence->GetCompletedValue() < pFence->dx.fenceValue)
    {
        *pFenceStatus = FENCE_STATUS_INCOMPLETE;
    }
    else
    {
        *pFenceStatus = FENCE_STATUS_COMPLETE;
    }
}

static void WaitForFences(uint32_t fenceCount, Fence** ppFences)
{
    // Wait for fence completion
    for (uint32_t i = 0; i < fenceCount; ++i)
    {
        FenceStatus fenceStatus;
        GetFenceStatus(ppFences[i], &fenceStatus);
        uint64_t fenceValue = ppFences[i]->dx.fenceValue;
        if (fenceStatus == FENCE_STATUS_INCOMPLETE)
        {
            ppFences[i]->dx.pFence->SetEventOnCompletion(fenceValue, ppFences[i]->dx.pWaitIdleFenceEvent);
            WaitForSingleObject(ppFences[i]->dx.pWaitIdleFenceEvent, INFINITE);
        }
    }
}

void d3d12_getFenceStatus(Renderer* pRenderer, Fence* pFence, FenceStatus* pFenceStatus)
{
    UNREF_PARAM(pRenderer);
    GetFenceStatus(pFence, pFenceStatus);
}

void d3d12_waitForFences(Renderer* pRenderer, uint32_t fenceCount, Fence** ppFences)
{
    UNREF_PARAM(pRenderer);
    WaitForFences(fenceCount, ppFences);
}

void d3d12_waitQueueIdle(Queue* pQueue)
{
    hook_signal_flush(pQueue, pQueue->dx.pFence->dx.pFence, ++pQueue->dx.pFence->dx.fenceValue);
    WaitForFences(1, &pQueue->dx.pFence);
}
/************************************************************************/
// Utility functions
/************************************************************************/
hz::Format d3d12_getSupportedSwapchainFormat(Renderer* pRenderer, const SwapChainDesc* pDesc, ColorSpace colorSpace)
{
    return hook_get_recommended_swapchain_format(pRenderer, pDesc, colorSpace);
}

uint32_t d3d12_getRecommendedSwapchainImageCount(Renderer* pRenderer, const WindowHandle* hwnd)
{
    UNREF_PARAM(pRenderer);
    UNREF_PARAM(hwnd);
#if defined(XBOX)
    return 2;
#else
    return 3;
#endif
}
/************************************************************************/
// Execute Indirect Implementation
/************************************************************************/
void d3d12_addIndirectCommandSignature(Renderer* pRenderer, const CommandSignatureDesc* pDesc, CommandSignature** ppCommandSignature)
{
    ASSERT(pRenderer);
    ASSERT(pDesc);
    ASSERT(pDesc->pArgDescs);
    ASSERT(ppCommandSignature);

    CommandSignature* pCommandSignature = (CommandSignature*)tf_calloc(1, sizeof(CommandSignature));
    ASSERT(pCommandSignature);

    bool                 needRootSignature = false;
    // calculate size through arguement types
    uint32_t             commandStride = 0;
    IndirectArgumentType drawType = INDIRECT_ARG_INVALID;

    D3D12_INDIRECT_ARGUMENT_DESC* argumentDescs =
        (D3D12_INDIRECT_ARGUMENT_DESC*)alloca((pDesc->indirectArgCount + 1) * sizeof(D3D12_INDIRECT_ARGUMENT_DESC));

    for (uint32_t i = 0; i < pDesc->indirectArgCount; ++i)
    {
        const DescriptorInfo* desc = NULL;
        if (pDesc->pArgDescs[i].type > INDIRECT_DISPATCH)
        {
            ASSERT(pDesc->pArgDescs[i].index < pDesc->pRootSignature->descriptorCount);

            desc = &pDesc->pRootSignature->pDescriptors[pDesc->pArgDescs[i].index];
            ASSERT(desc);
        }

        switch (pDesc->pArgDescs[i].type)
        {
        case INDIRECT_CONSTANT:
            argumentDescs[i].Type = D3D12_INDIRECT_ARGUMENT_TYPE_CONSTANT;
            argumentDescs[i].Constant.RootParameterIndex = desc->handleIndex; //-V522
            argumentDescs[i].Constant.DestOffsetIn32BitValues = 0;
            argumentDescs[i].Constant.Num32BitValuesToSet = desc->size;
            commandStride += sizeof(UINT) * argumentDescs[i].Constant.Num32BitValuesToSet;
            needRootSignature = true;
            break;
        case INDIRECT_UNORDERED_ACCESS_VIEW:
            argumentDescs[i].Type = D3D12_INDIRECT_ARGUMENT_TYPE_UNORDERED_ACCESS_VIEW;
            argumentDescs[i].UnorderedAccessView.RootParameterIndex = desc->handleIndex;
            commandStride += sizeof(D3D12_GPU_VIRTUAL_ADDRESS);
            needRootSignature = true;
            break;
        case INDIRECT_SHADER_RESOURCE_VIEW:
            argumentDescs[i].Type = D3D12_INDIRECT_ARGUMENT_TYPE_SHADER_RESOURCE_VIEW;
            argumentDescs[i].ShaderResourceView.RootParameterIndex = desc->handleIndex;
            commandStride += sizeof(D3D12_GPU_VIRTUAL_ADDRESS);
            needRootSignature = true;
            break;
        case INDIRECT_CONSTANT_BUFFER_VIEW:
            argumentDescs[i].Type = D3D12_INDIRECT_ARGUMENT_TYPE_CONSTANT_BUFFER_VIEW;
            argumentDescs[i].ConstantBufferView.RootParameterIndex = desc->handleIndex;
            commandStride += sizeof(D3D12_GPU_VIRTUAL_ADDRESS);
            needRootSignature = true;
            break;
        case INDIRECT_INCREMENTING_CONSTANT:
            argumentDescs[i].Type = D3D12_INDIRECT_ARGUMENT_TYPE_INCREMENTING_CONSTANT;
            argumentDescs[i].IncrementingConstant.RootParameterIndex = desc->handleIndex;
            argumentDescs[i].IncrementingConstant.DestOffsetIn32BitValues = pDesc->pArgDescs[i].rootConstantDestOffsetIn32BitValues;
            needRootSignature = true;
            break;
        case INDIRECT_VERTEX_BUFFER:
            argumentDescs[i].Type = D3D12_INDIRECT_ARGUMENT_TYPE_VERTEX_BUFFER_VIEW;
            argumentDescs[i].VertexBuffer.Slot = desc->handleIndex;
            commandStride += sizeof(D3D12_VERTEX_BUFFER_VIEW);
            needRootSignature = true;
            break;
        case INDIRECT_INDEX_BUFFER:
            argumentDescs[i].Type = D3D12_INDIRECT_ARGUMENT_TYPE_INDEX_BUFFER_VIEW;
            argumentDescs[i].VertexBuffer.Slot = desc->handleIndex;
            commandStride += sizeof(D3D12_INDEX_BUFFER_VIEW);
            needRootSignature = true;
            break;
        case INDIRECT_DRAW:
            argumentDescs[i].Type = D3D12_INDIRECT_ARGUMENT_TYPE_DRAW;
            commandStride += sizeof(IndirectDrawArguments);
            // Only one draw command allowed. So make sure no other draw command args in the list
            ASSERT(INDIRECT_ARG_INVALID == drawType);
            drawType = INDIRECT_DRAW;
            break;
        case INDIRECT_DRAW_INDEX:
            argumentDescs[i].Type = D3D12_INDIRECT_ARGUMENT_TYPE_DRAW_INDEXED;
            commandStride += sizeof(IndirectDrawIndexArguments);
            ASSERT(INDIRECT_ARG_INVALID == drawType);
            drawType = INDIRECT_DRAW_INDEX;
            break;
        case INDIRECT_DISPATCH:
            argumentDescs[i].Type = D3D12_INDIRECT_ARGUMENT_TYPE_DISPATCH;
            commandStride += sizeof(IndirectDispatchArguments);
            ASSERT(INDIRECT_ARG_INVALID == drawType);
            drawType = INDIRECT_DISPATCH;
            break;
        default:
            ASSERT(false);
            break;
        }
    }

    if (needRootSignature)
    {
        ASSERT(pDesc->pRootSignature);
    }

    D3D12_COMMAND_SIGNATURE_DESC commandSignatureDesc = {};
    commandSignatureDesc.pArgumentDescs = argumentDescs;
    commandSignatureDesc.NumArgumentDescs = pDesc->indirectArgCount;
    commandSignatureDesc.ByteStride = commandStride;
    commandSignatureDesc.NodeMask = 0;

    uint32_t alignedStride = round_up(commandStride, 16);
    if (!pDesc->packed && alignedStride != commandStride)
    {
        hook_modify_command_signature_desc(&commandSignatureDesc, alignedStride - commandStride);
    }

    CHECK_HRESULT(pRenderer->dx.pDevice->CreateCommandSignature(&commandSignatureDesc,
                                                                 needRootSignature ? pDesc->pRootSignature->dx.pRootSignature : NULL,
                                                                 IID_ARGS(&pCommandSignature->pHandle)));
    pCommandSignature->stride = commandSignatureDesc.ByteStride;
    pCommandSignature->drawType = drawType;

    *ppCommandSignature = pCommandSignature;
}

void d3d12_removeIndirectCommandSignature(Renderer* pRenderer, CommandSignature* pCommandSignature)
{
    UNREF_PARAM(pRenderer);
    SAFE_RELEASE(pCommandSignature->pHandle);
    SAFE_FREE(pCommandSignature);
}

void d3d12_cmdExecuteIndirect(Cmd* pCmd, CommandSignature* pCommandSignature, uint maxCommandCount, Buffer* pIndirectBuffer,
                              uint64_t bufferOffset, Buffer* pCounterBuffer, uint64_t counterBufferOffset)
{
    ASSERT(pCommandSignature);
    ASSERT(pIndirectBuffer);

#if defined(_WINDOWS) && defined(D3D12_RAYTRACING_AVAILABLE) && defined(FORGE_DEBUG)
    if (pCmd->pRenderer->dx.pDebugValidation && pCmd->dx.pBoundRootSignature->dx.hasRayQueryAccelerationStructure)
    {
        D3D12_MESSAGE_ID        hide[] = { D3D12_MESSAGE_ID_COMMAND_LIST_STATIC_DESCRIPTOR_RESOURCE_DIMENSION_MISMATCH };
        D3D12_INFO_QUEUE_FILTER filter = {};
        filter.DenyList.NumIDs = 1;
        filter.DenyList.pIDList = hide;
        pCmd->pRenderer->dx.pDebugValidation->PushStorageFilter(&filter);
    }
#endif

    if (!pCounterBuffer)
        pCmd->dx.pCmdList->ExecuteIndirect(pCommandSignature->pHandle, maxCommandCount, pIndirectBuffer->dx.pResource, bufferOffset, NULL,
                                            0);
    else
        pCmd->dx.pCmdList->ExecuteIndirect(pCommandSignature->pHandle, maxCommandCount, pIndirectBuffer->dx.pResource, bufferOffset,
                                            pCounterBuffer->dx.pResource, counterBufferOffset);

#if defined(_WINDOWS) && defined(D3D12_RAYTRACING_AVAILABLE) && defined(FORGE_DEBUG)
    if (pCmd->pRenderer->dx.pDebugValidation && pCmd->dx.pBoundRootSignature->dx.hasRayQueryAccelerationStructure)
    {
        pCmd->pRenderer->dx.pDebugValidation->PopStorageFilter();
    }
#endif
}
/************************************************************************/
// Query Heap Implementation
/************************************************************************/
void d3d12_getTimestampFrequency(Queue* pQueue, double* pFrequency)
{
    ASSERT(pQueue);
    ASSERT(pFrequency);

    UINT64 freq = 0;
    pQueue->dx.pQueue->GetTimestampFrequency(&freq);
    *pFrequency = (double)freq;
}

void d3d12_addQueryPool(Renderer* pRenderer, const QueryPoolDesc* pDesc, QueryPool** ppQueryPool)
{
    ASSERT(pRenderer);
    ASSERT(pDesc);
    ASSERT(ppQueryPool);

    QueryPool* pQueryPool = (QueryPool*)tf_calloc(1, sizeof(QueryPool));
    ASSERT(pQueryPool);

    const uint32_t queryCount = pDesc->queryCount * (QUERY_TYPE_TIMESTAMP == pDesc->type ? 2 : 1);

    pQueryPool->dx.type = ToDX12QueryType(pDesc->type);
    pQueryPool->count = queryCount;
    pQueryPool->stride = ToQueryWidth(pDesc->type);

    D3D12_QUERY_HEAP_DESC desc = {};
    desc.Count = queryCount;
    desc.NodeMask = 0;
    desc.Type = ToDX12QueryHeapType(pDesc->type);
    pRenderer->dx.pDevice->CreateQueryHeap(&desc, IID_ARGS(&pQueryPool->dx.pQueryHeap));
    SetObjectName(pQueryPool->dx.pQueryHeap, pDesc->pName);

    BufferDesc bufDesc = {};
    bufDesc.memoryUsage = RESOURCE_MEMORY_USAGE_GPU_TO_CPU;
    bufDesc.flags = BUFFER_CREATION_FLAG_OWN_MEMORY_BIT;
    bufDesc.elementCount = queryCount;
    bufDesc.size = queryCount * pQueryPool->stride;
    bufDesc.structStride = pQueryPool->stride;
    bufDesc.pName = pDesc->pName;
    bufDesc.startState = RESOURCE_STATE_COPY_DEST;
    addBuffer(pRenderer, &bufDesc, &pQueryPool->dx.pReadbackBuffer);

    *ppQueryPool = pQueryPool;
}

void d3d12_removeQueryPool(Renderer* pRenderer, QueryPool* pQueryPool)
{
    UNREF_PARAM(pRenderer);
    SAFE_RELEASE(pQueryPool->dx.pQueryHeap);
    removeBuffer(pRenderer, pQueryPool->dx.pReadbackBuffer);

    SAFE_FREE(pQueryPool);
}

void d3d12_cmdBeginQuery(Cmd* pCmd, QueryPool* pQueryPool, QueryDesc* pQuery)
{
    const D3D12_QUERY_TYPE type = pQueryPool->dx.type;
    switch (type)
    {
    case D3D12_QUERY_TYPE_TIMESTAMP:
    {
        const uint32_t index = pQuery->index * 2;
        pCmd->dx.pCmdList->EndQuery(pQueryPool->dx.pQueryHeap, type, index);
        break;
    }
    case D3D12_QUERY_TYPE_OCCLUSION:
    {
#if defined(XBOX)
        extern void SetOcclusionQueryControl(Cmd * pCmd, uint32_t sampleCount);
        ASSERT(pCmd->dx.sampleCount != 0);
        SetOcclusionQueryControl(pCmd, pCmd->dx.sampleCount);
#endif
        const uint32_t index = pQuery->index;
        pCmd->dx.pCmdList->BeginQuery(pQueryPool->dx.pQueryHeap, type, index);
        break;
    }
    case D3D12_QUERY_TYPE_PIPELINE_STATISTICS:
    {
        const uint32_t index = pQuery->index;
        pCmd->dx.pCmdList->BeginQuery(pQueryPool->dx.pQueryHeap, type, index);
        break;
    }
    default:
        ASSERT(false && "Not implemented");
    }
}

void d3d12_cmdEndQuery(Cmd* pCmd, QueryPool* pQueryPool, QueryDesc* pQuery)
{
    const D3D12_QUERY_TYPE type = pQueryPool->dx.type;
    uint32_t               index = pQuery->index * 2 + 1;
    switch (type)
    {
    case D3D12_QUERY_TYPE_TIMESTAMP:
    {
        index = pQuery->index * 2 + 1;
        pCmd->dx.pCmdList->EndQuery(pQueryPool->dx.pQueryHeap, type, index);
        break;
    }
    case D3D12_QUERY_TYPE_OCCLUSION:
    {
#if defined(XBOX)
        extern void SetOcclusionQueryControl(Cmd * pCmd, uint32_t sampleCount);
        SetOcclusionQueryControl(pCmd, 0);
#endif
        index = pQuery->index;
        pCmd->dx.pCmdList->EndQuery(pQueryPool->dx.pQueryHeap, type, index);
        break;
    }
    case D3D12_QUERY_TYPE_PIPELINE_STATISTICS:
    {
        index = pQuery->index;
        pCmd->dx.pCmdList->EndQuery(pQueryPool->dx.pQueryHeap, type, index);
        break;
    }
    default:
        ASSERT(false && "Not implemented");
    }
}

void d3d12_cmdResolveQuery(Cmd* pCmd, QueryPool* pQueryPool, uint32_t startQuery, uint32_t queryCount)
{
    ASSERT(pCmd);
    ASSERT(pQueryPool);
    ASSERT(queryCount);

    hook_pre_resolve_query(pCmd);

    const uint32_t internalQueryCount = (D3D12_QUERY_TYPE_TIMESTAMP == pQueryPool->dx.type ? 2 : 1);

    pCmd->dx.pCmdList->ResolveQueryData(pQueryPool->dx.pQueryHeap, pQueryPool->dx.type, startQuery * internalQueryCount,
                                         queryCount * internalQueryCount, pQueryPool->dx.pReadbackBuffer->dx.pResource,
                                         (uint64_t)startQuery * internalQueryCount * pQueryPool->stride);
}

void d3d12_cmdResetQuery(Cmd* pCmd, QueryPool* pQueryPool, uint32_t startQuery, uint32_t queryCount)
{
    UNREF_PARAM(pCmd);
    UNREF_PARAM(pQueryPool);
    UNREF_PARAM(startQuery);
    UNREF_PARAM(queryCount);
}

void d3d12_getQueryData(Renderer* pRenderer, QueryPool* pQueryPool, uint32_t queryIndex, QueryData* pOutData)
{
    ASSERT(pRenderer);
    ASSERT(pQueryPool);
    ASSERT(pOutData);

    const D3D12_QUERY_TYPE type = pQueryPool->dx.type;
    *pOutData = {};
    pOutData->valid = true;

    const uint32_t queryCount = (D3D12_QUERY_TYPE_TIMESTAMP == pQueryPool->dx.type ? 2 : 1);
    ReadRange      range = {};
    range.offset = queryIndex * queryCount * pQueryPool->stride;
    range.size = queryCount * pQueryPool->stride;
    mapBuffer(pRenderer, pQueryPool->dx.pReadbackBuffer, &range);
    uint64_t* queries = (uint64_t*)((uint8_t*)pQueryPool->dx.pReadbackBuffer->pCpuMappedAddress + range.offset);

    switch (type)
    {
    case D3D12_QUERY_TYPE_TIMESTAMP:
    {
        pOutData->beginTimestamp = queries[0];
        pOutData->endTimestamp = queries[1];
        break;
    }
    case D3D12_QUERY_TYPE_OCCLUSION:
    {
        pOutData->occlusionCounts = queries[0];
        break;
    }
    case D3D12_QUERY_TYPE_PIPELINE_STATISTICS:
    {
        COMPILE_ASSERT(sizeof(pOutData->pipelineStats) == sizeof(D3D12_QUERY_DATA_PIPELINE_STATISTICS));
        memcpy(&pOutData->pipelineStats, queries, sizeof(pOutData->pipelineStats));
        break;
    }
    default:
        ASSERT(false && "Not implemented");
    }

    unmapBuffer(pRenderer, pQueryPool->dx.pReadbackBuffer);
}
/************************************************************************/
// Memory Stats Implementation
/************************************************************************/
void d3d12_calculateMemoryStats(Renderer* pRenderer, char** stats)
{
    ASSERT(pRenderer);
    ASSERT(stats);

    WCHAR* wstats = NULL;
    pRenderer->dx.pResourceAllocator->BuildStatsString(&wstats, TRUE);
    const int utf8Size = WideCharToMultiByte(CP_UTF8, 0, wstats, -1, NULL, 0, NULL, NULL);
    if (utf8Size > 0)
    {
        *stats = (char*)tf_malloc((size_t)utf8Size);
        if (*stats)
        {
            WideCharToMultiByte(CP_UTF8, 0, wstats, -1, *stats, utf8Size, NULL, NULL);
        }
    }
    else
    {
        *stats = (char*)tf_malloc(1);
        if (*stats)
            (*stats)[0] = '\0';
    }
    pRenderer->dx.pResourceAllocator->FreeStatsString(wstats);
}

void d3d12_calculateMemoryUse(Renderer* pRenderer, uint64_t* usedBytes, uint64_t* totalAllocatedBytes)
{
    D3D12MA::TotalStatistics stats;
    pRenderer->dx.pResourceAllocator->CalculateStatistics(&stats);
    *usedBytes = stats.Total.Stats.AllocationBytes;
    *totalAllocatedBytes = stats.Total.Stats.BlockBytes;
}

void d3d12_freeMemoryStats(Renderer* pRenderer, char* stats)
{
    UNREF_PARAM(pRenderer);
    tf_free(stats);
}
/************************************************************************/
// Debug Marker Implementation
/************************************************************************/
void d3d12_cmdBeginDebugMarker(Cmd* pCmd, float r, float g, float b, const char* pName)
{
    UNREF_PARAM(pCmd);
    UNREF_PARAM(r);
    UNREF_PARAM(g);
    UNREF_PARAM(b);
    UNREF_PARAM(pName);
    // note: USE_PIX isn't the ideal test because we might be doing a debug build where pix
    // is not installed, or a variety of other reasons. It should be a separate #ifdef flag?
#if defined(USE_PIX)
    // color is in B8G8R8X8 format where X is padding
    PIXBeginEvent(pCmd->dx.pCmdList, PIX_COLOR((BYTE)(r * 255), (BYTE)(g * 255), (BYTE)(b * 255)), pName);
#endif
}

void d3d12_cmdEndDebugMarker(Cmd* pCmd)
{
    UNREF_PARAM(pCmd);
#if defined(USE_PIX)
    PIXEndEvent(pCmd->dx.pCmdList);
#endif
}

void d3d12_cmdAddDebugMarker(Cmd* pCmd, float r, float g, float b, const char* pName)
{
    UNREF_PARAM(pCmd);
    UNREF_PARAM(r);
    UNREF_PARAM(g);
    UNREF_PARAM(b);
    UNREF_PARAM(pName);
#if defined(USE_PIX)
    // color is in B8G8R8X8 format where X is padding
    PIXSetMarker(pCmd->dx.pCmdList, PIX_COLOR((BYTE)(r * 255), (BYTE)(g * 255), (BYTE)(b * 255)), pName);
#endif
#if defined(ENABLE_NSIGHT_AFTERMATH)
    SetAftermathMarker(&pCmd->pRenderer->aftermathTracker, pCmd->dx.pCmdList, pName);
#endif
}

void d3d12_cmdWriteMarker(Cmd* pCmd, const MarkerDesc* pDesc)
{
    ASSERT(pCmd);
    ASSERT(pDesc);
    ASSERT(pDesc->pBuffer);

#if defined(XBOX)
    extern void hook_cmd_write_marker(Cmd*, const MarkerDesc*);
    hook_cmd_write_marker(pCmd, pDesc);
#else
    D3D12_GPU_VIRTUAL_ADDRESS            gpuAddress = pDesc->pBuffer->dx.pResource->GetGPUVirtualAddress() + pDesc->offset;
    D3D12_WRITEBUFFERIMMEDIATE_PARAMETER wbParam = {};
    D3D12_WRITEBUFFERIMMEDIATE_MODE      wbMode = D3D12_WRITEBUFFERIMMEDIATE_MODE_DEFAULT;
    if (pDesc->flags & MARKER_FLAG_WAIT_FOR_WRITE)
    {
        wbMode = D3D12_WRITEBUFFERIMMEDIATE_MODE_MARKER_OUT;
    }
    wbParam.Dest = gpuAddress;
    wbParam.Value = pDesc->value;
    ((ID3D12GraphicsCommandList2*)pCmd->dx.pCmdList)->WriteBufferImmediate(1, &wbParam, &wbMode);
#endif
}
/************************************************************************/
// Resource Debug Naming Interface
/************************************************************************/
void d3d12_setBufferName(Renderer* pRenderer, Buffer* pBuffer, const char* pName)
{
    UNREF_PARAM(pRenderer);
    UNREF_PARAM(pBuffer);
    UNREF_PARAM(pName);
#if defined(ENABLE_GRAPHICS_DEBUG)
    ASSERT(pRenderer);
    ASSERT(pBuffer);
    ASSERT(pName);
    SetObjectName(pBuffer->dx.pResource, pName);
#endif
#if defined(ENABLE_TRACY_MEMORY)
    if (pBuffer && pName && pBuffer->memoryTrackingMode == D3D12_MEMORY_TRACKING_D3D12MA)
    {
        d3d12_set_allocation_name(pBuffer->dx.pAllocation, pName);
    }
#endif
}

void d3d12_setTextureName(Renderer* pRenderer, Texture* pTexture, const char* pName)
{
    UNREF_PARAM(pRenderer);
    UNREF_PARAM(pTexture);
    UNREF_PARAM(pName);
#if defined(ENABLE_GRAPHICS_DEBUG)
    ASSERT(pRenderer);
    ASSERT(pTexture);
    ASSERT(pName);
    SetObjectName(pTexture->dx.pResource, pName);
#endif
#if defined(ENABLE_TRACY_MEMORY)
    if (pTexture && pName)
    {
        d3d12_set_allocation_name(pTexture->dx.pAllocation, pName);
    }
#endif
}

void d3d12_setRenderTargetName(Renderer* pRenderer, RenderTarget* pRenderTarget, const char* pName)
{
    setTextureName(pRenderer, pRenderTarget->pTexture, pName);
}

void d3d12_setPipelineName(Renderer* pRenderer, Pipeline* pPipeline, const char* pName)
{
    UNREF_PARAM(pRenderer);
    UNREF_PARAM(pPipeline);
    UNREF_PARAM(pName);
#if defined(ENABLE_GRAPHICS_DEBUG)
    ASSERT(pRenderer);
    ASSERT(pPipeline);
    ASSERT(pName);
    SetObjectName(pPipeline->dx.pPipelineState, pName);
#endif
}

#endif

void initD3D12Renderer(const char* appName, const RendererDesc* pSettings, Renderer** ppRenderer)
{
    d3d12_initRenderer(appName, pSettings, ppRenderer);
}

void exitD3D12Renderer(Renderer* pRenderer)
{
    ASSERT(pRenderer);

    d3d12_exitRenderer(pRenderer);
}

void initD3D12RendererContext(const char* appName, const RendererContextDesc* pSettings, RendererContext** ppContext)
{
    // No need to initialize API function pointers, initRenderer MUST be called before using anything else anyway.
    d3d12_initRendererContext(appName, pSettings, ppContext);
}

void exitD3D12RendererContext(RendererContext* pContext)
{
    ASSERT(pContext);

    d3d12_exitRendererContext(pContext);
}
#endif
