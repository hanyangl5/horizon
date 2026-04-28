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

#include "../Private/GraphicsConfig.h"
#ifdef ENABLE_NSIGHT_AFTERMATH
#include "../ThirdParty/PrivateNvidia/NsightAftermath/include/AftermathTracker.h"
#endif
#include <ThirdParty/tinyimageformat/tinyimageformat_base.h>

#include "Core/ILog.h"
#include "Core/IThread.h"
#include "Platform/IOperatingSystem.h"

#ifdef __cplusplus
#ifndef MAKE_ENUM_FLAG
#define MAKE_ENUM_FLAG(TYPE, ENUM_TYPE)                                                                                      \
    inline FORGE_CONSTEXPR ENUM_TYPE operator|(ENUM_TYPE a, ENUM_TYPE b) { return ENUM_TYPE(((TYPE)a) | ((TYPE)b)); }        \
    inline ENUM_TYPE&                operator|=(ENUM_TYPE& a, ENUM_TYPE b) { return (ENUM_TYPE&)(((TYPE&)a) |= ((TYPE)b)); } \
    inline FORGE_CONSTEXPR ENUM_TYPE operator&(ENUM_TYPE a, ENUM_TYPE b) { return ENUM_TYPE(((TYPE)a) & ((TYPE)b)); }        \
    inline ENUM_TYPE&                operator&=(ENUM_TYPE& a, ENUM_TYPE b) { return (ENUM_TYPE&)(((TYPE&)a) &= ((TYPE)b)); } \
    inline FORGE_CONSTEXPR ENUM_TYPE operator~(ENUM_TYPE a) { return ENUM_TYPE(~((TYPE)a)); }                                \
    inline FORGE_CONSTEXPR ENUM_TYPE operator^(ENUM_TYPE a, ENUM_TYPE b) { return ENUM_TYPE(((TYPE)a) ^ ((TYPE)b)); }        \
    inline ENUM_TYPE&                operator^=(ENUM_TYPE& a, ENUM_TYPE b) { return (ENUM_TYPE&)(((TYPE&)a) ^= ((TYPE)b)); }
#endif
#else
#define MAKE_ENUM_FLAG(TYPE, ENUM_TYPE)
#endif

//
// default capability levels of the renderer
//
#if !defined(RENDERER_CUSTOM_MAX)
enum
{
    MAX_INSTANCE_EXTENSIONS = 64,
    MAX_DEVICE_EXTENSIONS = 64,
    /// Max number of GPUs in SLI or Cross-Fire
    MAX_LINKED_GPUS = 4,
    /// Max number of GPUs in unlinked mode
    MAX_UNLINKED_GPUS = 4,
    /// Max number of GPus for either linked or unlinked mode. must update WindowsBase::setupPlatformUI accordingly
    MAX_MULTIPLE_GPUS = 4,
    MAX_RENDER_TARGET_ATTACHMENTS = 8,
    MAX_VERTEX_BINDINGS = 15,
    MAX_VERTEX_ATTRIBS = 15,
    MAX_RESOURCE_NAME_LENGTH = 256,
    MAX_SEMANTIC_NAME_LENGTH = 128,
    MAX_DEBUG_NAME_LENGTH = 128,
    MAX_MIP_LEVELS = 0xFFFFFFFF,
    MAX_SWAPCHAIN_IMAGES = 3,
    MAX_GPU_VENDOR_STRING_LENGTH = 256, // max size for GPUVendorPreset strings
    MAX_SAMPLE_LOCATIONS = 16,
};
#endif

namespace D3D12MA
{
class Allocator;
class Allocation;
} // namespace D3D12MA
typedef int32_t DxDescriptorID;
#if defined(Xbox)
#include "../../../Xbox/Common_3/Graphics/Direct3D12/Direct3D12X.h"
#endif

enum RendererApi : uint32_t
{
    RENDERER_API_D3D12 = 0,
    RENDERER_API_COUNT
};

enum QueueType : uint32_t
{
    QUEUE_TYPE_GRAPHICS = 0,
    QUEUE_TYPE_TRANSFER,
    QUEUE_TYPE_COMPUTE,
    MAX_QUEUE_TYPE
};

enum QueueFlag : uint32_t
{
    QUEUE_FLAG_NONE = 0x0,
    QUEUE_FLAG_DISABLE_GPU_TIMEOUT = 0x1,
    QUEUE_FLAG_INIT_MICROPROFILE = 0x2,
    MAX_QUEUE_FLAG = 0xFFFFFFFF
};
MAKE_ENUM_FLAG(uint32_t, QueueFlag)

enum QueuePriority : uint32_t
{
    QUEUE_PRIORITY_NORMAL,
    QUEUE_PRIORITY_HIGH,
    QUEUE_PRIORITY_GLOBAL_REALTIME,
    MAX_QUEUE_PRIORITY
};

enum LoadActionType : uint32_t
{
    LOAD_ACTION_DONTCARE,
    LOAD_ACTION_LOAD,
    LOAD_ACTION_CLEAR,
    MAX_LOAD_ACTION
};

enum StoreActionType : uint32_t
{
    // Store is the most common use case so keep that as default
    STORE_ACTION_STORE,
    STORE_ACTION_DONTCARE,
    STORE_ACTION_NONE,
#if defined(USE_MSAA_RESOLVE_ATTACHMENTS)
    // Resolve into pResolveAttachment and also store the MSAA attachment (rare - maybe used for debug)
    STORE_ACTION_RESOLVE_STORE,
    // Resolve into pResolveAttachment and discard MSAA attachment (most common use case for resolve)
    STORE_ACTION_RESOLVE_DONTCARE,
#endif
    MAX_STORE_ACTION
};

typedef void (*LogFn)(LogLevel, const char*, const char*);

enum ResourceState : uint32_t
{
    RESOURCE_STATE_UNDEFINED = 0,
    RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER = 0x1,
    RESOURCE_STATE_INDEX_BUFFER = 0x2,
    RESOURCE_STATE_RENDER_TARGET = 0x4,
    RESOURCE_STATE_UNORDERED_ACCESS = 0x8,
    RESOURCE_STATE_DEPTH_WRITE = 0x10,
    RESOURCE_STATE_DEPTH_READ = 0x20,
    RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE = 0x40,
    RESOURCE_STATE_PIXEL_SHADER_RESOURCE = 0x80,
    RESOURCE_STATE_SHADER_RESOURCE = 0x40 | 0x80,
    RESOURCE_STATE_STREAM_OUT = 0x100,
    RESOURCE_STATE_INDIRECT_ARGUMENT = 0x200,
    RESOURCE_STATE_COPY_DEST = 0x400,
    RESOURCE_STATE_COPY_SOURCE = 0x800,
    RESOURCE_STATE_GENERIC_READ = (((((0x1 | 0x2) | 0x40) | 0x80) | 0x200) | 0x800),
    RESOURCE_STATE_PRESENT = 0x1000,
    RESOURCE_STATE_COMMON = 0x2000,
    RESOURCE_STATE_ACCELERATION_STRUCTURE_READ = 0x4000,
    RESOURCE_STATE_ACCELERATION_STRUCTURE_WRITE = 0x8000,
#if defined(QUEST_VR)
    RESOURCE_STATE_SHADING_RATE_SOURCE = 0x10000,
#endif
};
MAKE_ENUM_FLAG(uint32_t, ResourceState)

/// Choosing Memory Type
enum ResourceMemoryUsage : uint32_t
{
    /// No intended memory usage specified.
    RESOURCE_MEMORY_USAGE_UNKNOWN = 0,
    /// Memory will be used on device only, no need to be mapped on host.
    RESOURCE_MEMORY_USAGE_GPU_ONLY = 1,
    /// Memory will be mapped on host. Could be used for transfer to device.
    RESOURCE_MEMORY_USAGE_CPU_ONLY = 2,
    /// Memory will be used for frequent (dynamic) updates from host and reads on device.
    RESOURCE_MEMORY_USAGE_CPU_TO_GPU = 3,
    /// Memory will be used for writing on device and readback on host.
    RESOURCE_MEMORY_USAGE_GPU_TO_CPU = 4,
    /// CPU-visible GPU-local upload memory. On D3D12 this maps to GPU_UPLOAD heaps when ReBAR is supported.
    RESOURCE_MEMORY_USAGE_GPU_UPLOAD = 5,
    RESOURCE_MEMORY_USAGE_COUNT,
    RESOURCE_MEMORY_USAGE_MAX_ENUM = 0x7FFFFFFF
};

struct PlatformParameters
{
    // RendererAPI
    RendererApi mSelectedRendererApi;
    // Available GPU capabilities
    char        ppAvailableGpuNames[MAX_MULTIPLE_GPUS][MAX_GPU_VENDOR_STRING_LENGTH];
    uint32_t    pAvailableGpuIds[MAX_MULTIPLE_GPUS];
    uint32_t    mAvailableGpuCount;
    uint32_t    mSelectedGpuIndex;
    // Could add swap chain size, render target format, ...
    uint32_t    mPreferedGpuId;
};

// Forward declarations
struct RendererContext;
struct Renderer;
struct Queue;
struct Pipeline;
struct Buffer;
struct Texture;
struct RenderTarget;
struct Shader;
struct RootSignature;
struct DescriptorSet;
struct DescriptorIndexMap;
struct PipelineCache;
struct DirectStorage;
struct DirectStorageFile;
struct DirectStorageQueue;
struct DirectStorageStatusArray;

// Raytracing
struct Raytracing;
struct RaytracingHitGroup;
struct AccelerationStructure;

struct IndirectDrawArguments
{
    uint32_t mVertexCount;
    uint32_t mInstanceCount;
    uint32_t mStartVertex;
    uint32_t mStartInstance;
};

struct IndirectDrawIndexArguments
{
    uint32_t mIndexCount;
    uint32_t mInstanceCount;
    uint32_t mStartIndex;
    uint32_t mVertexOffset;
    uint32_t mStartInstance;
};

struct IndirectDispatchArguments
{
    uint32_t mGroupCountX;
    uint32_t mGroupCountY;
    uint32_t mGroupCountZ;
};

#define INDIRECT_DRAW_ELEM_INDEX(m)       (offsetof(IndirectDrawArguments, m) / sizeof(uint32_t))
#define INDIRECT_DRAW_INDEX_ELEM_INDEX(m) (offsetof(IndirectDrawIndexArguments, m) / sizeof(uint32_t))
#define INDIRECT_DISPATCH_ELEM_INDEX(m)   (offsetof(IndirectDispatchArguments, m) / sizeof(uint32_t))

enum IndirectArgumentType : uint32_t
{
    INDIRECT_ARG_INVALID,
    INDIRECT_DRAW,
    INDIRECT_DRAW_INDEX,
    INDIRECT_DISPATCH,
    INDIRECT_VERTEX_BUFFER,
    INDIRECT_INDEX_BUFFER,
    INDIRECT_CONSTANT,
    INDIRECT_CONSTANT_BUFFER_VIEW,   // only for dx
    INDIRECT_SHADER_RESOURCE_VIEW,   // only for dx
    INDIRECT_UNORDERED_ACCESS_VIEW,  // only for dx
    INDIRECT_COMMAND_BUFFER,         // metal ICB
    INDIRECT_COMMAND_BUFFER_RESET,   // metal ICB reset
    INDIRECT_COMMAND_BUFFER_OPTIMIZE // metal ICB optimization
};
/************************************************/

enum DescriptorType : uint32_t
{
    DESCRIPTOR_TYPE_UNDEFINED = 0,
    DESCRIPTOR_TYPE_SAMPLER = 0x01,
    // SRV Read only texture
    DESCRIPTOR_TYPE_TEXTURE = (DESCRIPTOR_TYPE_SAMPLER << 1),
    /// UAV Texture
    DESCRIPTOR_TYPE_RW_TEXTURE = (DESCRIPTOR_TYPE_TEXTURE << 1),
    // SRV Read only buffer
    DESCRIPTOR_TYPE_BUFFER = (DESCRIPTOR_TYPE_RW_TEXTURE << 1),
    DESCRIPTOR_TYPE_BUFFER_RAW = (DESCRIPTOR_TYPE_BUFFER | (DESCRIPTOR_TYPE_BUFFER << 1)),
    /// UAV Buffer
    DESCRIPTOR_TYPE_RW_BUFFER = (DESCRIPTOR_TYPE_BUFFER << 2),
    DESCRIPTOR_TYPE_RW_BUFFER_RAW = (DESCRIPTOR_TYPE_RW_BUFFER | (DESCRIPTOR_TYPE_RW_BUFFER << 1)),
    /// Uniform buffer
    DESCRIPTOR_TYPE_UNIFORM_BUFFER = (DESCRIPTOR_TYPE_RW_BUFFER << 2),
    /// Push constant / Root constant
    DESCRIPTOR_TYPE_ROOT_CONSTANT = (DESCRIPTOR_TYPE_UNIFORM_BUFFER << 1),
    /// IA
    DESCRIPTOR_TYPE_VERTEX_BUFFER = (DESCRIPTOR_TYPE_ROOT_CONSTANT << 1),
    DESCRIPTOR_TYPE_INDEX_BUFFER = (DESCRIPTOR_TYPE_VERTEX_BUFFER << 1),
    DESCRIPTOR_TYPE_INDIRECT_BUFFER = (DESCRIPTOR_TYPE_INDEX_BUFFER << 1),
    /// Cubemap SRV
    DESCRIPTOR_TYPE_TEXTURE_CUBE = (DESCRIPTOR_TYPE_TEXTURE | (DESCRIPTOR_TYPE_INDIRECT_BUFFER << 1)),
    /// RTV / DSV per mip slice
    DESCRIPTOR_TYPE_RENDER_TARGET_MIP_SLICES = (DESCRIPTOR_TYPE_INDIRECT_BUFFER << 2),
    /// RTV / DSV per array slice
    DESCRIPTOR_TYPE_RENDER_TARGET_ARRAY_SLICES = (DESCRIPTOR_TYPE_RENDER_TARGET_MIP_SLICES << 1),
    /// RTV / DSV per depth slice
    DESCRIPTOR_TYPE_RENDER_TARGET_DEPTH_SLICES = (DESCRIPTOR_TYPE_RENDER_TARGET_ARRAY_SLICES << 1),
    DESCRIPTOR_TYPE_INDIRECT_COMMAND_BUFFER = (DESCRIPTOR_TYPE_RENDER_TARGET_DEPTH_SLICES << 1),
    /// Raytracing acceleration structure
    DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE = (DESCRIPTOR_TYPE_INDIRECT_COMMAND_BUFFER << 1),
};
MAKE_ENUM_FLAG(uint32_t, DescriptorType)

enum SampleCount : uint32_t
{
    SAMPLE_COUNT_1 = 1,
    SAMPLE_COUNT_2 = 2,
    SAMPLE_COUNT_4 = 4,
    SAMPLE_COUNT_8 = 8,
    SAMPLE_COUNT_16 = 16,
    SAMPLE_COUNT_COUNT = 5,
};

enum ShaderStage : uint32_t
{
    SHADER_STAGE_NONE = 0,
    SHADER_STAGE_VERT = 0X00000001,
    SHADER_STAGE_TESC = 0X00000002,
    SHADER_STAGE_TESE = 0X00000004,
    SHADER_STAGE_GEOM = 0X00000008,
    SHADER_STAGE_FRAG = 0X00000010,
    SHADER_STAGE_COMP = 0X00000020,
    SHADER_STAGE_ALL_GRAPHICS = ((uint32_t)SHADER_STAGE_VERT | (uint32_t)SHADER_STAGE_TESC | (uint32_t)SHADER_STAGE_TESE |
                                 (uint32_t)SHADER_STAGE_GEOM | (uint32_t)SHADER_STAGE_FRAG),
    SHADER_STAGE_HULL = SHADER_STAGE_TESC,
    SHADER_STAGE_DOMN = SHADER_STAGE_TESE,
    SHADER_STAGE_COUNT = 6,
};

enum ShaderStageIndex : uint32_t
{
    SHADER_STAGE_INDEX_VERT = 0,
    SHADER_STAGE_INDEX_TESC,
    SHADER_STAGE_INDEX_TESE,
    SHADER_STAGE_INDEX_GEOM,
    SHADER_STAGE_INDEX_FRAG,
    SHADER_STAGE_INDEX_COMP,
    SHADER_STAGE_INDEX_HULL = SHADER_STAGE_INDEX_TESC,
    SHADER_STAGE_INDEX_DOMN = SHADER_STAGE_INDEX_TESE,
};
MAKE_ENUM_FLAG(uint32_t, ShaderStage)

// This include is placed here because it uses data types defined previously in this file
// and forward enums are not allowed for some compilers (Xcode).
#include "IShaderReflection.h"

enum PrimitiveTopology : uint32_t
{
    PRIMITIVE_TOPO_POINT_LIST = 0,
    PRIMITIVE_TOPO_LINE_LIST,
    PRIMITIVE_TOPO_LINE_STRIP,
    PRIMITIVE_TOPO_TRI_LIST,
    PRIMITIVE_TOPO_TRI_STRIP,
    PRIMITIVE_TOPO_PATCH_LIST,
    PRIMITIVE_TOPO_COUNT,
};

enum IndexType : uint32_t
{
    INDEX_TYPE_UINT32 = 0,
    INDEX_TYPE_UINT16,
};

enum ShaderSemantic : uint32_t
{
    SEMANTIC_UNDEFINED = 0,
    SEMANTIC_POSITION,
    SEMANTIC_NORMAL,
    SEMANTIC_COLOR,
    SEMANTIC_TANGENT,
    SEMANTIC_BITANGENT,
    SEMANTIC_JOINTS,
    SEMANTIC_WEIGHTS,
    SEMANTIC_CUSTOM,
    SEMANTIC_TEXCOORD0,
    SEMANTIC_TEXCOORD1,
    SEMANTIC_TEXCOORD2,
    SEMANTIC_TEXCOORD3,
    SEMANTIC_TEXCOORD4,
    SEMANTIC_TEXCOORD5,
    SEMANTIC_TEXCOORD6,
    SEMANTIC_TEXCOORD7,
    SEMANTIC_TEXCOORD8,
    SEMANTIC_TEXCOORD9,
    MAX_SEMANTICS
};

enum BlendConstant : uint32_t
{
    BC_ZERO = 0,
    BC_ONE,
    BC_SRC_COLOR,
    BC_ONE_MINUS_SRC_COLOR,
    BC_DST_COLOR,
    BC_ONE_MINUS_DST_COLOR,
    BC_SRC_ALPHA,
    BC_ONE_MINUS_SRC_ALPHA,
    BC_DST_ALPHA,
    BC_ONE_MINUS_DST_ALPHA,
    BC_SRC_ALPHA_SATURATE,
    BC_BLEND_FACTOR,
    BC_ONE_MINUS_BLEND_FACTOR,
    MAX_BLEND_CONSTANTS
};

enum BlendMode : uint32_t
{
    BM_ADD,
    BM_SUBTRACT,
    BM_REVERSE_SUBTRACT,
    BM_MIN,
    BM_MAX,
    MAX_BLEND_MODES,
};

enum CompareMode : uint32_t
{
    CMP_NEVER,
    CMP_LESS,
    CMP_EQUAL,
    CMP_LEQUAL,
    CMP_GREATER,
    CMP_NOTEQUAL,
    CMP_GEQUAL,
    CMP_ALWAYS,
    MAX_COMPARE_MODES,
};

enum StencilOp : uint32_t
{
    STENCIL_OP_KEEP,
    STENCIL_OP_SET_ZERO,
    STENCIL_OP_REPLACE,
    STENCIL_OP_INVERT,
    STENCIL_OP_INCR,
    STENCIL_OP_DECR,
    STENCIL_OP_INCR_SAT,
    STENCIL_OP_DECR_SAT,
    MAX_STENCIL_OPS,
};

enum ColorMask : uint8_t
{
    COLOR_MASK_NONE = 0x0,
    COLOR_MASK_RED = 0x1,
    COLOR_MASK_GREEN = 0x2,
    COLOR_MASK_BLUE = 0x4,
    COLOR_MASK_ALPHA = 0x8,
    COLOR_MASK_ALL = (COLOR_MASK_RED | COLOR_MASK_GREEN | COLOR_MASK_BLUE | COLOR_MASK_ALPHA),
};
MAKE_ENUM_FLAG(uint8_t, ColorMask)

// Blend states are always attached to one of the eight or more render targets that
// are in a MRT
// Mask constants
enum BlendStateTargets : uint32_t
{
    BLEND_STATE_TARGET_0 = 0x1,
    BLEND_STATE_TARGET_1 = 0x2,
    BLEND_STATE_TARGET_2 = 0x4,
    BLEND_STATE_TARGET_3 = 0x8,
    BLEND_STATE_TARGET_4 = 0x10,
    BLEND_STATE_TARGET_5 = 0x20,
    BLEND_STATE_TARGET_6 = 0x40,
    BLEND_STATE_TARGET_7 = 0x80,
    BLEND_STATE_TARGET_ALL = 0xFF,
};
MAKE_ENUM_FLAG(uint32_t, BlendStateTargets)

enum CullMode : uint32_t
{
    CULL_MODE_NONE = 0,
    CULL_MODE_BACK,
    CULL_MODE_FRONT,
    CULL_MODE_BOTH,
    MAX_CULL_MODES
};

enum FrontFace : uint32_t
{
    FRONT_FACE_CCW = 0,
    FRONT_FACE_CW
};

enum FillMode : uint32_t
{
    FILL_MODE_SOLID,
    FILL_MODE_WIREFRAME,
    MAX_FILL_MODES
};

enum PipelineType : uint32_t
{
    PIPELINE_TYPE_UNDEFINED = 0,
    PIPELINE_TYPE_COMPUTE,
    PIPELINE_TYPE_GRAPHICS,
    PIPELINE_TYPE_COUNT,
};

enum FilterType : uint32_t
{
    FILTER_NEAREST = 0,
    FILTER_LINEAR,
};

enum AddressMode : uint32_t
{
    ADDRESS_MODE_MIRROR,
    ADDRESS_MODE_REPEAT,
    ADDRESS_MODE_CLAMP_TO_EDGE,
    ADDRESS_MODE_CLAMP_TO_BORDER
};

enum MipMapMode : uint32_t
{
    MIPMAP_MODE_NEAREST = 0,
    MIPMAP_MODE_LINEAR
};

typedef union ClearValue
{
    struct
    {
        float r;
        float g;
        float b;
        float a;
    };
    struct
    {
        float    depth;
        uint32_t stencil;
    };
} ClearValue;

enum BufferCreationFlags : uint32_t
{
    /// Default flag (Buffer will use aliased memory, buffer will not be cpu accessible until mapBuffer is called)
    BUFFER_CREATION_FLAG_NONE = 0x0,
    /// Buffer will allocate its own memory (COMMITTED resource)
    BUFFER_CREATION_FLAG_OWN_MEMORY_BIT = 0x1,
    /// Buffer will be persistently mapped
    BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT = 0x2,
    /// Use ESRAM to store this buffer
    BUFFER_CREATION_FLAG_ESRAM = 0x4,
    /// Flag to specify not to allocate descriptors for the resource
    BUFFER_CREATION_FLAG_NO_DESCRIPTOR_VIEW_CREATION = 0x8,

    BUFFER_CREATION_FLAG_ACCELERATION_STRUCTURE_BUILD_INPUT = 0x10,
    BUFFER_CREATION_FLAG_SHADER_DEVICE_ADDRESS = 0x20,
    BUFFER_CREATION_FLAG_SHADER_BINDING_TABLE = 0x40,
    BUFFER_CREATION_FLAG_MARKER = 0x80,

};
MAKE_ENUM_FLAG(uint32_t, BufferCreationFlags)

enum TextureCreationFlags : uint32_t
{
    /// Default flag (Texture will use default allocation strategy decided by the api specific allocator)
    TEXTURE_CREATION_FLAG_NONE = 0,
    /// Texture will allocate its own memory (COMMITTED resource)
    TEXTURE_CREATION_FLAG_OWN_MEMORY_BIT = 0x01,
    /// Texture will be allocated in memory which can be shared among multiple processes
    TEXTURE_CREATION_FLAG_EXPORT_BIT = 0x02,
    /// Texture will be allocated in memory which can be shared among multiple gpus
    TEXTURE_CREATION_FLAG_EXPORT_ADAPTER_BIT = 0x04,
    /// Texture will be imported from a handle created in another process
    TEXTURE_CREATION_FLAG_IMPORT_BIT = 0x08,
    /// Use ESRAM to store this texture
    TEXTURE_CREATION_FLAG_ESRAM = 0x10,
    /// Use on-tile memory to store this texture
    TEXTURE_CREATION_FLAG_ON_TILE = 0x20,
    /// Prevent compression meta data from generating (XBox)
    TEXTURE_CREATION_FLAG_NO_COMPRESSION = 0x40,
    /// Force 2D instead of automatically determining dimension based on width, height, depth
    TEXTURE_CREATION_FLAG_FORCE_2D = 0x80,
    /// Force 3D instead of automatically determining dimension based on width, height, depth
    TEXTURE_CREATION_FLAG_FORCE_3D = 0x100,
    /// Display target
    TEXTURE_CREATION_FLAG_ALLOW_DISPLAY_TARGET = 0x200,
    /// Create an sRGB texture.
    TEXTURE_CREATION_FLAG_SRGB = 0x400,
    /// Create a normal map texture
    TEXTURE_CREATION_FLAG_NORMAL_MAP = 0x800,
    /// Fast clear
    TEXTURE_CREATION_FLAG_FAST_CLEAR = 0x1000,
    /// Fragment mask
    TEXTURE_CREATION_FLAG_FRAG_MASK = 0x2000,
    /// Doubles the amount of array layers of the texture when rendering VR. Also forces the texture to be a 2D Array texture.
    TEXTURE_CREATION_FLAG_VR_MULTIVIEW = 0x4000,
    /// Binds the FFR fragment density if this texture is used as a render target.
    TEXTURE_CREATION_FLAG_VR_FOVEATED_RENDERING = 0x8000,
#if defined(USE_MSAA_RESOLVE_ATTACHMENTS)
    /// Creates resolve attachment for auto resolve (MSAA on tiled architecture - Resolve can be done on tile through render pass)
    TEXTURE_CREATION_FLAG_CREATE_RESOLVE_ATTACHMENT = 0x10000,
#endif
    TEXTURE_CREATION_FLAG_SAMPLE_LOCATIONS_COMPATIBLE = 0x20000
};
MAKE_ENUM_FLAG(uint32_t, TextureCreationFlags)

// Used for swapchain
enum ColorSpace : uint32_t
{
    COLOR_SPACE_SDR_LINEAR = 0x0,
    COLOR_SPACE_SDR_SRGB,
    COLOR_SPACE_P2020,         // BT2020 color space with PQ EOTF
    COLOR_SPACE_EXTENDED_SRGB, // Extended sRGB with linear EOTF
};

// Material Unit test use this enum to index a shader table
static_assert(GPU_PRESET_COUNT == 7);

struct BufferBarrier
{
    Buffer*       pBuffer;
    ResourceState mCurrentState;
    ResourceState mNewState;
    uint8_t       mBeginOnly : 1;
    uint8_t       mEndOnly : 1;
};

struct TextureBarrier
{
    Texture*      pTexture;
    ResourceState mCurrentState;
    ResourceState mNewState;
    uint8_t       mBeginOnly : 1;
    uint8_t       mEndOnly : 1;
    uint8_t       mAcquire : 1;
    uint8_t       mRelease : 1;
    uint8_t       mQueueType : 5;
    /// Specifiy whether following barrier targets particular subresource
    uint8_t       mSubresourceBarrier : 1;
    /// Following values are ignored if mSubresourceBarrier is false
    uint8_t       mMipLevel : 7;
    uint16_t      mArrayLayer;
};

struct RenderTargetBarrier
{
    RenderTarget* pRenderTarget;
    ResourceState mCurrentState;
    ResourceState mNewState;
    uint8_t       mBeginOnly : 1;
    uint8_t       mEndOnly : 1;
    uint8_t       mAcquire : 1;
    uint8_t       mRelease : 1;
    uint8_t       mQueueType : 5;
    /// Specifiy whether following barrier targets particular subresource
    uint8_t       mSubresourceBarrier : 1;
    /// Following values are ignored if mSubresourceBarrier is false
    uint8_t       mMipLevel : 7;
    uint16_t      mArrayLayer;
};

struct ReadRange
{
    uint64_t mOffset;
    uint64_t mSize;
};

enum QueryType : uint32_t
{
    QUERY_TYPE_TIMESTAMP = 0,
    QUERY_TYPE_OCCLUSION,
    QUERY_TYPE_PIPELINE_STATISTICS,
    QUERY_TYPE_COUNT,
};

struct QueryPoolDesc
{
    const char* pName;
    QueryType   mType;
    uint32_t    mQueryCount;
    uint32_t    mNodeIndex;
};

struct QueryDesc
{
    uint32_t mIndex;
};

struct QueryPool
{
    struct
    {
        ID3D12QueryHeap* pQueryHeap;
        Buffer*          pReadbackBuffer;
        D3D12_QUERY_TYPE mType;
    } mDx;
    uint32_t mCount;
    uint32_t mStride;
};

struct PipelineStatisticsQueryData
{
    uint64_t mIAVertices;
    uint64_t mIAPrimitives;
    uint64_t mVSInvocations;
    uint64_t mGSInvocations;
    uint64_t mGSPrimitives;
    uint64_t mCInvocations;
    uint64_t mCPrimitives;
    uint64_t mPSInvocations;
    uint64_t mHSInvocations;
    uint64_t mDSInvocations;
    uint64_t mCSInvocations;
};

struct QueryData
{
    union
    {
        struct
        {
            PipelineStatisticsQueryData mPipelineStats;
        };
        struct
        {
            uint64_t mBeginTimestamp;
            uint64_t mEndTimestamp;
        };
        uint64_t mOcclusionCounts;
    };
    bool mValid;
};

enum ResourceHeapCreationFlags : uint32_t
{
    RESOURCE_HEAP_FLAG_NONE = 0,
    RESOURCE_HEAP_FLAG_SHARED = 0x1,
    RESOURCE_HEAP_FLAG_DENY_BUFFERS = 0x2,
    RESOURCE_HEAP_FLAG_ALLOW_DISPLAY = 0x4,
    RESOURCE_HEAP_FLAG_SHARED_CROSS_ADAPTER = 0x8,
    RESOURCE_HEAP_FLAG_DENY_RT_DS_TEXTURES = 0x10,
    RESOURCE_HEAP_FLAG_DENY_NON_RT_DS_TEXTURES = 0x20,
    RESOURCE_HEAP_FLAG_HARDWARE_PROTECTED = 0x40,
    RESOURCE_HEAP_FLAG_ALLOW_WRITE_WATCH = 0x80,
    RESOURCE_HEAP_FLAG_ALLOW_SHADER_ATOMICS = 0x100,

    // These are convenience aliases to manage resource heap tier restrictions. They cannot be bitwise OR'ed together cleanly.
    RESOURCE_HEAP_FLAG_ALLOW_ALL_BUFFERS_AND_TEXTURES = 0x200,
    RESOURCE_HEAP_FLAG_ALLOW_ONLY_BUFFERS = RESOURCE_HEAP_FLAG_DENY_NON_RT_DS_TEXTURES | RESOURCE_HEAP_FLAG_DENY_RT_DS_TEXTURES,
    RESOURCE_HEAP_FLAG_ALLOW_ONLY_NON_RT_DS_TEXTURES = RESOURCE_HEAP_FLAG_DENY_BUFFERS | RESOURCE_HEAP_FLAG_DENY_RT_DS_TEXTURES,
    RESOURCE_HEAP_FLAG_ALLOW_ONLY_RT_DS_TEXTURES = RESOURCE_HEAP_FLAG_DENY_BUFFERS | RESOURCE_HEAP_FLAG_DENY_NON_RT_DS_TEXTURES,
};

struct ResourceHeapDesc
{
    uint64_t mSize;
    uint64_t mAlignment;

    ResourceMemoryUsage       mMemoryUsage;
    DescriptorType            mDescriptors;
    ResourceHeapCreationFlags mFlags;

    uint32_t    mNodeIndex;
    uint32_t    mSharedNodeIndexCount;
    uint32_t*   pSharedNodeIndices;
    const char* pName;
};

struct alignas(64) ResourceHeap
{
    struct
    {
        ID3D12Heap* pHeap;
    } mDx;

    uint64_t mSize;
};

struct ResourceSizeAlign
{
    uint64_t mSize;
    uint64_t mAlignment;
};

struct ResourcePlacement
{
    ResourceHeap* pHeap;
    uint64_t      mOffset;
};

/// Data structure holding necessary info to create a Buffer
struct BufferDesc
{
    /// Optional placement (addBuffer will place/bind buffer in this memory instead of allocating space)
    ResourcePlacement*  pPlacement;
    /// Size of the buffer (in bytes)
    uint64_t            mSize;
    /// Set this to specify a counter buffer for this buffer (applicable to BUFFER_USAGE_STORAGE_SRV, BUFFER_USAGE_STORAGE_UAV)
    struct Buffer*      pCounterBuffer;
    /// Index of the first element accessible by the SRV/UAV (applicable to BUFFER_USAGE_STORAGE_SRV, BUFFER_USAGE_STORAGE_UAV)
    uint32_t            mFirstElement;
    /// Number of elements in the buffer (applicable to BUFFER_USAGE_STORAGE_SRV, BUFFER_USAGE_STORAGE_UAV)
    uint32_t            mElementCount;
    /// Size of each element (in bytes) in the buffer (applicable to BUFFER_USAGE_STORAGE_SRV, BUFFER_USAGE_STORAGE_UAV)
    uint32_t            mStructStride;
    /// Alignment
    uint32_t            mAlignment;
    /// Debug name used in gpu profile
    const char*         pName;
    uint32_t*           pSharedNodeIndices;
    /// Decides which memory heap buffer will use (default, upload, readback)
    ResourceMemoryUsage mMemoryUsage;
    /// Creation flags of the buffer
    BufferCreationFlags mFlags;
    /// What type of queue the buffer is owned by
    QueueType           mQueueType;
    /// What state will the buffer get created in
    ResourceState       mStartState;
    /// Format of the buffer (applicable to typed storage buffers (Buffer<T>)
    TinyImageFormat     mFormat;
    /// Flags specifying the suitable usage of this buffer (Uniform buffer, Vertex Buffer, Index Buffer,...)
    DescriptorType      mDescriptors;
    /// The index of the GPU in SLI/Cross-Fire that owns this buffer, or the Renderer index in unlinked mode.
    uint32_t            mNodeIndex;
    uint32_t            mSharedNodeIndexCount;
};

struct alignas(64) Buffer
{
    /// CPU address of the mapped buffer (applicable to buffers created in CPU accessible heaps (CPU, CPU_TO_GPU, GPU_TO_CPU)
    void* pCpuMappedAddress;
    struct
    {
        /// GPU Address - Cache to avoid calls to ID3D12Resource::GetGpuVirtualAddress
        D3D12_GPU_VIRTUAL_ADDRESS mGpuAddress;
        /// Descriptor handle of the CBV in a CPU visible descriptor heap (applicable to BUFFER_USAGE_UNIFORM)
        DxDescriptorID            mDescriptors;
        /// Offset from mDescriptors for srv descriptor handle
        uint8_t                   mSrvDescriptorOffset;
        /// Offset from mDescriptors for uav descriptor handle
        uint8_t                   mUavDescriptorOffset;
        uint8_t                   mMarkerBuffer : 1;
        /// Native handle of the underlying resource
        ID3D12Resource*           pResource;
        union
        {
            ID3D12Heap*          pMarkerBufferHeap;
            /// Contains resource allocation info such as parent heap, offset in heap
            D3D12MA::Allocation* pAllocation;
        };
    } mDx;
    uint64_t mSize : 32;
    uint64_t mDescriptors : 20;
    uint64_t mMemoryUsage : 3;
    uint64_t mNodeIndex : 4;
};
// One cache line
static_assert(sizeof(Buffer) == 8 * sizeof(uint64_t));

/// Data structure holding necessary info to create a Texture
struct TextureDesc
{
    /// Optional placement (addTexture will place/bind buffer in this memory instead of allocating space)
    ResourcePlacement*   pPlacement;
    /// Optimized clear value (recommended to use this same value when clearing the rendertarget)
    ClearValue           mClearValue;
    /// Pointer to native texture handle if the texture does not own underlying resource
    const void*          pNativeHandle;
    /// Debug name used in gpu profile
    const char*          pName;
    /// GPU indices to share this texture
    uint32_t*            pSharedNodeIndices;
    /// Texture creation flags (decides memory allocation strategy, sharing access,...)
    TextureCreationFlags mFlags;
    /// Width
    uint32_t             mWidth;
    /// Height
    uint32_t             mHeight;
    /// Depth (Should be 1 if not a mType is not TEXTURE_TYPE_3D)
    uint32_t             mDepth;
    /// Texture array size (Should be 1 if texture is not a texture array or cubemap)
    uint32_t             mArraySize;
    /// Number of mip levels
    uint32_t             mMipLevels;
    /// Number of multisamples per pixel (currently Textures created with mUsage TEXTURE_USAGE_SAMPLED_IMAGE only support SAMPLE_COUNT_1)
    SampleCount          mSampleCount;
    /// The image quality level. The higher the quality, the lower the performance. The valid range is between zero and the value
    /// appropriate for mSampleCount
    uint32_t             mSampleQuality;
    ///  image format
    TinyImageFormat      mFormat;
    /// What state will the texture get created in
    ResourceState        mStartState;
    /// Descriptor creation
    DescriptorType       mDescriptors;
    /// Number of GPUs to share this texture
    uint32_t             mSharedNodeIndexCount;
    /// GPU which will own this texture
    uint32_t             mNodeIndex;
};

struct alignas(64) Texture
{
    struct
    {
        /// Descriptor handle of the SRV in a CPU visible descriptor heap (applicable to TEXTURE_USAGE_SAMPLED_IMAGE)
        DxDescriptorID       mDescriptors;
        /// Native handle of the underlying resource
        ID3D12Resource*      pResource;
        /// Contains resource allocation info such as parent heap, offset in heap
        D3D12MA::Allocation* pAllocation;
        uint32_t             mHandleCount : 24;
        uint32_t             mUavStartIndex;
    } mDx;
    /// Current state of the buffer
    uint32_t mWidth : 16;
    uint32_t mHeight : 16;
    uint32_t mDepth : 16;
    uint32_t mMipLevels : 5;
    uint32_t mArraySizeMinusOne : 11;
    uint32_t mFormat : 8;
    /// Flags specifying which aspects (COLOR,DEPTH,STENCIL) are included in the pImageView
    uint32_t mAspectMask : 4;
    uint32_t mNodeIndex : 4;
    uint32_t mSampleCount : 5;
    uint32_t mUav : 1;
    /// This value will be false if the underlying resource is not owned by the texture (swapchain textures,...)
    uint32_t mOwnsImage : 1;
};
// One cache line
static_assert(sizeof(Texture) == 8 * sizeof(uint64_t));

struct RenderTargetDesc
{
    /// Optional placement (addRenderTarget will place/bind buffer in this memory instead of allocating space)
    ResourcePlacement*   pPlacement;
    /// Texture creation flags (decides memory allocation strategy, sharing access,...)
    TextureCreationFlags mFlags;
    /// Width
    uint32_t             mWidth;
    /// Height
    uint32_t             mHeight;
    /// Depth (Should be 1 if not a mType is not TEXTURE_TYPE_3D)
    uint32_t             mDepth;
    /// Texture array size (Should be 1 if texture is not a texture array or cubemap)
    uint32_t             mArraySize;
    /// Number of mip levels
    uint32_t             mMipLevels;
    /// MSAA
    SampleCount          mSampleCount;
    /// Internal image format
    TinyImageFormat      mFormat;
    /// What state will the texture get created in
    ResourceState        mStartState;
    /// Optimized clear value (recommended to use this same value when clearing the rendertarget)
    ClearValue           mClearValue;
    /// The image quality level. The higher the quality, the lower the performance. The valid range is between zero and the value
    /// appropriate for mSampleCount
    uint32_t             mSampleQuality;
    /// Descriptor creation
    DescriptorType       mDescriptors;
    const void*          pNativeHandle;
    /// Debug name used in gpu profile
    const char*          pName;
    /// GPU indices to share this texture
    uint32_t*            pSharedNodeIndices;
    /// Number of GPUs to share this texture
    uint32_t             mSharedNodeIndexCount;
    /// GPU which will own this texture
    uint32_t             mNodeIndex;
};

struct alignas(64) RenderTarget
{
    Texture* pTexture;
    struct
    {
        DxDescriptorID mDescriptors;
    } mDx;
#if defined(USE_MSAA_RESOLVE_ATTACHMENTS)
    RenderTarget* pResolveAttachment;
#endif
    ClearValue      mClearValue;
    uint32_t        mArraySize : 16;
    uint32_t        mDepth : 16;
    uint32_t        mWidth : 16;
    uint32_t        mHeight : 16;
    uint32_t        mDescriptors : 20;
    uint32_t        mMipLevels : 10;
    uint32_t        mSampleQuality : 5;
    TinyImageFormat mFormat;
    SampleCount     mSampleCount;
    bool            mVRMultiview;
    bool            mVRFoveatedRendering;
};
static_assert(sizeof(RenderTarget) <= 32 * sizeof(uint64_t));

struct SampleLocations
{
    int8_t mX;
    int8_t mY;
};

struct SamplerDesc
{
    FilterType  mMinFilter;
    FilterType  mMagFilter;
    MipMapMode  mMipMapMode;
    AddressMode mAddressU;
    AddressMode mAddressV;
    AddressMode mAddressW;
    float       mMipLodBias;
    bool        mSetLodRange;
    float       mMinLod;
    float       mMaxLod;
    float       mMaxAnisotropy;
    CompareMode mCompareFunc;
};

struct alignas(16) Sampler
{
    struct
    {
        /// Description for creating the Sampler descriptor for this sampler
        D3D12_SAMPLER_DESC mDesc;
        /// Descriptor handle of the Sampler in a CPU visible descriptor heap
        DxDescriptorID     mDescriptor;
    } mDx;
};
static_assert(sizeof(Sampler) == 8 * sizeof(uint64_t));

enum DescriptorUpdateFrequency : uint32_t
{
    DESCRIPTOR_UPDATE_FREQ_NONE = 0,
    DESCRIPTOR_UPDATE_FREQ_PER_FRAME,
    DESCRIPTOR_UPDATE_FREQ_PER_BATCH,
    DESCRIPTOR_UPDATE_FREQ_PER_DRAW,
    DESCRIPTOR_UPDATE_FREQ_COUNT,
};

/// Data structure holding the layout for a descriptor
struct alignas(16) DescriptorInfo
{
    const char* pName;
    uint32_t    mType;
    uint32_t    mDim : 4;
    uint32_t    mRootDescriptor : 1;
    uint32_t    mStaticSampler : 1;
    uint32_t    mUpdateFrequency : 3;
    uint32_t    mSize;
    uint32_t    mHandleIndex;
    struct
    {
        uint64_t mPadA;
    } mDx;
};
static_assert(sizeof(DescriptorInfo) == 4 * sizeof(uint64_t));

enum RootSignatureFlags : uint32_t
{
    /// Default flag
    ROOT_SIGNATURE_FLAG_NONE = 0,
};
MAKE_ENUM_FLAG(uint32_t, RootSignatureFlags)

struct RootSignatureDesc
{
    Shader**           ppShaders;
    uint32_t           mShaderCount;
    uint32_t           mMaxBindlessTextures;
    const char**       ppStaticSamplerNames;
    Sampler**          ppStaticSamplers;
    uint32_t           mStaticSamplerCount;
    RootSignatureFlags mFlags;
};

struct alignas(64) RootSignature
{
    /// Number of descriptors declared in the root signature layout
    uint32_t            mDescriptorCount;
    /// Graphics or Compute
    PipelineType        mPipelineType;
    /// Array of all descriptors declared in the root signature layout
    DescriptorInfo*     pDescriptors;
    /// Translates hash of descriptor name to descriptor index in pDescriptors array
    DescriptorIndexMap* pDescriptorNameToIndexMap;
    struct
    {
        ID3D12RootSignature* pRootSignature;
        uint8_t              mViewDescriptorTableRootIndices[DESCRIPTOR_UPDATE_FREQ_COUNT];
        uint8_t              mSamplerDescriptorTableRootIndices[DESCRIPTOR_UPDATE_FREQ_COUNT];
        uint32_t             mCumulativeViewDescriptorCounts[DESCRIPTOR_UPDATE_FREQ_COUNT];
        uint32_t             mCumulativeSamplerDescriptorCounts[DESCRIPTOR_UPDATE_FREQ_COUNT];
        uint16_t             mViewDescriptorCounts[DESCRIPTOR_UPDATE_FREQ_COUNT];
        uint16_t             mSamplerDescriptorCounts[DESCRIPTOR_UPDATE_FREQ_COUNT];
#if defined(_WINDOWS) && defined(D3D12_RAYTRACING_AVAILABLE) && defined(FORGE_DEBUG)
        bool mHasRayQueryAccelerationStructure;
#endif
    } mDx;
};
// 2 cache lines
static_assert(sizeof(RootSignature) <= 16 * sizeof(uint64_t));

struct DescriptorDataRange
{
    uint32_t mOffset;
    uint32_t mSize;
    // Specify different structured buffer stride (ignored for raw buffer - ByteAddressBuffer)
    uint32_t mStructStride;
};

struct DescriptorData
{
    /// User can either set name of descriptor or index (index in pRootSignature->pDescriptors array)
    /// Name of descriptor
    const char* pName;
    /// Number of array entries to update (array size of ppTextures/ppBuffers/...)
    uint32_t    mCount : 31;
    /// Dst offset into the array descriptor (useful for updating few entries in a large array)
    // Example: to update 6th entry in a bindless texture descriptor, mArrayOffset will be 6 and mCount will be 1)
    uint32_t    mArrayOffset : 20;
    // Index in pRootSignature->pDescriptors array - Cache index using getDescriptorIndexFromName to avoid using string checks at runtime
    uint32_t    mIndex : 10;
    uint32_t    mBindByIndex : 1;

    // Range to bind (buffer offset, size)
    DescriptorDataRange* pRanges;

    // Binds stencil only descriptor instead of color/depth
    bool     mBindStencilResource : 1;
    // When binding UAV, control the mip slice to to bind for UAV (example - generating mipmaps in a compute shader)
    uint16_t mUAVMipSlice;
    // Binds entire mip chain as array of UAV
    bool     mBindMipChain;
    /// Array of resources containing descriptor handles or constant to be used in ring buffer memory - DescriptorRange can hold only one
    /// resource type array
    union
    {
        /// Array of texture descriptors (srv and uav textures)
        Texture**               ppTextures;
        /// Array of sampler descriptors
        Sampler**               ppSamplers;
        /// Array of buffer descriptors (srv, uav and cbv buffers)
        Buffer**                ppBuffers;
        /// Custom binding (raytracing acceleration structure ...)
        AccelerationStructure** ppAccelerationStructures;
    };
};

struct alignas(64) DescriptorSet
{
    struct
    {
        /// Start handle to cbv srv uav descriptor table
        DxDescriptorID       mCbvSrvUavHandle;
        /// Start handle to sampler descriptor table
        DxDescriptorID       mSamplerHandle;
        /// Stride of the cbv srv uav descriptor table (number of descriptors * descriptor size)
        uint32_t             mCbvSrvUavStride;
        /// Stride of the sampler descriptor table (number of descriptors * descriptor size)
        uint32_t             mSamplerStride;
        const RootSignature* pRootSignature;
        uint32_t             mMaxSets : 16;
        uint32_t             mUpdateFrequency : 3;
        uint32_t             mNodeIndex : 4;
        uint32_t             mCbvSrvUavRootIndex : 4;
        uint32_t             mSamplerRootIndex : 4;
        uint32_t             mPipelineType : 3;
    } mDx;
};

struct CmdPoolDesc
{
    Queue* pQueue;
    bool   mTransient;
};

struct CmdPool
{
    ID3D12CommandAllocator* pCmdAlloc;

    Queue* pQueue;
};

struct CmdDesc
{
    CmdPool* pPool;
    bool     mSecondary;
#ifdef ENABLE_GRAPHICS_DEBUG
    const char* pName;
#endif // ENABLE_GRAPHICS_DEBUG
};

enum MarkerFlags : uint8_t
{
    /// Default flag
    MARKER_FLAG_NONE = 0,
    MARKER_FLAG_WAIT_FOR_WRITE = 0x1,
};
MAKE_ENUM_FLAG(uint8_t, MarkerFlags)

struct MarkerDesc
{
    Buffer*     pBuffer;
    uint32_t    mOffset;
    uint32_t    mValue;
    MarkerFlags mFlags;
};

#if !defined(PROSPERO)
#define GPU_MARKER_SIZE                        sizeof(uint32_t)
#define GPU_MARKER_VALUE(markerBuffer, offset) (*((uint32_t*)markerBuffer->pCpuMappedAddress) + ((offset) / GPU_MARKER_SIZE))
#endif

#if !defined(GFX_ESRAM_ALLOCATIONS)
#define ESRAM_BEGIN_ALLOC(...)
#define ESRAM_CURRENT_OFFSET(...) 0u
#define ESRAM_END_ALLOC(...)
#define ESRAM_RESET_ALLOCS(...)
#endif

struct alignas(64) Cmd
{
    struct
    {
        ID3D12GraphicsCommandList1* pCmdList;
#if defined(ENABLE_GRAPHICS_DEBUG) && defined(_WINDOWS)
        // For resource state validation
        ID3D12DebugCommandList* pDebugCmdList;
#endif
        // Cached in beginCmd to avoid fetching them during rendering
        struct DescriptorHeap*      pBoundHeaps[2];
        D3D12_GPU_DESCRIPTOR_HANDLE mBoundHeapStartHandles[2];

        // Command buffer state
        const RootSignature* pBoundRootSignature;
        DescriptorSet*       pBoundDescriptorSets[DESCRIPTOR_UPDATE_FREQ_COUNT];
        uint16_t             mBoundDescriptorSetIndices[DESCRIPTOR_UPDATE_FREQ_COUNT];
        uint32_t             mNodeIndex : 4;
        uint32_t             mType : 3;
        CmdPool*             pCmdPool;
    } mDx;
    Renderer* pRenderer;
    Queue*    pQueue;
};
static_assert(sizeof(Cmd) <= 64 * sizeof(uint64_t));

enum FenceStatus : uint32_t
{
    FENCE_STATUS_COMPLETE = 0,
    FENCE_STATUS_INCOMPLETE,
    FENCE_STATUS_NOTSUBMITTED,
};

struct Fence
{
    struct
    {
        ID3D12Fence* pFence;
        HANDLE       pWaitIdleFenceEvent;
        uint64_t     mFenceValue;
    } mDx;
};

struct Semaphore
{
    // DirectX12 does not have a concept of semaphores
    // All synchronization is done using fences
    // Simulate semaphore signal and wait using DirectX12 fences
    struct
    {
        ID3D12Fence* pFence;
        HANDLE       pWaitIdleFenceEvent;
        uint64_t     mFenceValue;
    } mDx;
};

struct QueueDesc
{
    QueueType     mType;
    QueueFlag     mFlag;
    QueuePriority mPriority;
    uint32_t      mNodeIndex;
    const char*   pName;
};

struct Queue
{
    struct
    {
        ID3D12CommandQueue* pQueue;
        Fence*              pFence;
#if defined(_WINDOWS) && defined(FORGE_DEBUG)
        // To silence mismatching command list on Windows 11 multi GPU
        Renderer* pRenderer;
#endif
    } mDx;
    uint32_t mType : 3;
    uint32_t mNodeIndex : 4;
};

struct ShaderConstant
{
    const void* pValue;
    uint32_t    mIndex;
    uint32_t    mSize;
};

struct BinaryShaderStageDesc
{
    const char* pName;
    /// Byte code array
    void*       pByteCode;
    uint32_t    mByteCodeSize;
    const char* pEntryPoint;
};

struct BinaryShaderDesc
{
    ShaderStage           mStages;
    /// Specify whether shader will own byte code memory
    uint32_t              mOwnByteCode : 1;
    BinaryShaderStageDesc mVert;
    BinaryShaderStageDesc mFrag;
    BinaryShaderStageDesc mGeom;
    BinaryShaderStageDesc mHull;
    BinaryShaderStageDesc mDomain;
    BinaryShaderStageDesc mComp;
    const ShaderConstant* pConstants;
    uint32_t              mConstantCount;
#if defined(QUEST_VR)
    bool mIsMultiviewVR : 1;
#endif
};

struct ShaderSrcStageDesc
{
    const char* pName;
    /// Byte code array
    void*       pByteCode;
    uint32_t    mByteCodeSize;
    const char* pEntryPoint;
};

struct ShaderSrcDesc
{
    ShaderStage           mStages;
    /// Specify whether shader will own byte code memory
    uint32_t              mOwnByteCode : 1;
    ShaderSrcStageDesc    mVert;
    ShaderSrcStageDesc    mFrag;
    ShaderSrcStageDesc    mGeom;
    ShaderSrcStageDesc    mHull;
    ShaderSrcStageDesc    mDomain;
    ShaderSrcStageDesc    mComp;
    const ShaderConstant* pConstants;
    uint32_t              mConstantCount;
#if defined(QUEST_VR)
    bool mIsMultiviewVR : 1;
#endif
};

struct Shader
{
    ShaderStage mStages : 31;
    bool        mIsMultiviewVR : 1;
    uint32_t    mNumThreadsPerGroup[3];
    uint32_t    mOutputRenderTargetTypesMask;
    struct
    {
        IDxcBlobEncoding** pShaderBlobs;
        LPCWSTR*           pEntryNames;
    } mDx;
    PipelineReflection* pReflection;
};

struct BlendStateDesc
{
    /// Source blend factor per render target.
    BlendConstant     mSrcFactors[MAX_RENDER_TARGET_ATTACHMENTS];
    /// Destination blend factor per render target.
    BlendConstant     mDstFactors[MAX_RENDER_TARGET_ATTACHMENTS];
    /// Source alpha blend factor per render target.
    BlendConstant     mSrcAlphaFactors[MAX_RENDER_TARGET_ATTACHMENTS];
    /// Destination alpha blend factor per render target.
    BlendConstant     mDstAlphaFactors[MAX_RENDER_TARGET_ATTACHMENTS];
    /// Blend mode per render target.
    BlendMode         mBlendModes[MAX_RENDER_TARGET_ATTACHMENTS];
    /// Alpha blend mode per render target.
    BlendMode         mBlendAlphaModes[MAX_RENDER_TARGET_ATTACHMENTS];
    /// Write mask per render target.
    ColorMask         mColorWriteMasks[MAX_RENDER_TARGET_ATTACHMENTS];
    /// Mask that identifies the render targets affected by the blend state.
    BlendStateTargets mRenderTargetMask;
    /// Set whether alpha to coverage should be enabled.
    bool              mAlphaToCoverage;
    /// Set whether each render target has an unique blend function. When false the blend function in slot 0 will be used for all render
    /// targets.
    bool              mIndependentBlend;
};

struct DepthStateDesc
{
    bool        mDepthTest;
    bool        mDepthWrite;
    CompareMode mDepthFunc;
    bool        mStencilTest;
    uint8_t     mStencilReadMask;
    uint8_t     mStencilWriteMask;
    CompareMode mStencilFrontFunc;
    StencilOp   mStencilFrontFail;
    StencilOp   mDepthFrontFail;
    StencilOp   mStencilFrontPass;
    CompareMode mStencilBackFunc;
    StencilOp   mStencilBackFail;
    StencilOp   mDepthBackFail;
    StencilOp   mStencilBackPass;
};

struct RasterizerStateDesc
{
    CullMode  mCullMode;
    int32_t   mDepthBias;
    float     mSlopeScaledDepthBias;
    FillMode  mFillMode;
    FrontFace mFrontFace;
    bool      mMultiSample;
    bool      mScissor;
    bool      mDepthClampEnable;
};

enum VertexBindingRate : uint32_t
{
    VERTEX_BINDING_RATE_VERTEX = 0,
    VERTEX_BINDING_RATE_INSTANCE = 1,
    VERTEX_BINDING_RATE_COUNT,
};

struct VertexBinding
{
    uint32_t          mStride;
    VertexBindingRate mRate;
};

struct VertexAttrib
{
    ShaderSemantic  mSemantic;
    uint32_t        mSemanticNameLength;
    char            mSemanticName[MAX_SEMANTIC_NAME_LENGTH];
    TinyImageFormat mFormat;
    uint32_t        mBinding;
    uint32_t        mLocation;
    uint32_t        mOffset;
};

struct VertexLayout
{
    VertexBinding mBindings[MAX_VERTEX_BINDINGS];
    VertexAttrib  mAttribs[MAX_VERTEX_ATTRIBS];
    uint32_t      mBindingCount;
    uint32_t      mAttribCount;
};

struct GraphicsPipelineDesc
{
    Shader*              pShaderProgram;
    RootSignature*       pRootSignature;
    VertexLayout*        pVertexLayout;
    BlendStateDesc*      pBlendState;
    DepthStateDesc*      pDepthState;
    RasterizerStateDesc* pRasterizerState;
    TinyImageFormat*     pColorFormats;
#if defined(USE_MSAA_RESOLVE_ATTACHMENTS)
    /// Used to specify resolve attachment for render pass
    StoreActionType* pColorResolveActions;
#endif
    uint32_t          mRenderTargetCount;
    SampleCount       mSampleCount;
    uint32_t          mSampleQuality;
    TinyImageFormat   mDepthStencilFormat;
    PrimitiveTopology mPrimitiveTopo;
    bool              mSupportIndirectCommandBuffer;
    bool              mVRFoveatedRendering;
    bool              mUseCustomSampleLocations;
};

struct ComputePipelineDesc
{
    Shader*        pShaderProgram;
    RootSignature* pRootSignature;
};

struct PipelineDesc
{
    union
    {
        ComputePipelineDesc  mComputeDesc;
        GraphicsPipelineDesc mGraphicsDesc;
    };
    PipelineCache* pCache;
    void*          pPipelineExtensions;
    const char*    pName;
    PipelineType   mType;
    uint32_t       mExtensionCount;
};

struct alignas(64) Pipeline
{
    struct
    {
        ID3D12PipelineState*   pPipelineState;
        const RootSignature*   pRootSignature;
        PipelineType           mType;
        D3D_PRIMITIVE_TOPOLOGY mPrimitiveTopology;
    } mDx;
};
// One cache line
static_assert(sizeof(Pipeline) == 8 * sizeof(uint64_t));

enum PipelineCacheFlags : uint32_t
{
    PIPELINE_CACHE_FLAG_NONE = 0x0,
    PIPELINE_CACHE_FLAG_EXTERNALLY_SYNCHRONIZED = 0x1,
};
MAKE_ENUM_FLAG(uint32_t, PipelineCacheFlags);

struct PipelineCacheDesc
{
    /// Initial pipeline cache data (can be NULL which means empty pipeline cache)
    void*              pData;
    /// Initial pipeline cache size
    size_t             mSize;
    PipelineCacheFlags mFlags;
};

struct PipelineCache
{
    struct
    {
        ID3D12PipelineLibrary* pLibrary;
        void*                  pData;
    } mDx;
};

#if defined(SHADER_STATS_AVAILABLE)
struct ShaderStats
{
    uint32_t mUsedVgprs;
    uint32_t mUsedSgprs;
    uint32_t mLdsSizePerLocalWorkGroup;
    uint32_t mLdsUsageSizeInBytes;
    uint32_t mScratchMemUsageInBytes;
    uint32_t mPhysicalVgprs;
    uint32_t mPhysicalSgprs;
    uint32_t mAvailableVgprs;
    uint32_t mAvailableSgprs;
    uint32_t mComputeWorkGroupSize[3];
    bool     mValid;
};

struct PipelineStats
{
    ShaderStats mStats[SHADER_STAGE_COUNT];
};
#endif

enum SwapChainCreationFlags : uint32_t
{
    SWAP_CHAIN_CREATION_FLAG_NONE = 0x0,
    SWAP_CHAIN_CREATION_FLAG_ENABLE_FOVEATED_RENDERING_VR = 0x1,
};
MAKE_ENUM_FLAG(uint32_t, SwapChainCreationFlags);

struct SwapChainDesc
{
    /// Window handle
    WindowHandle           mWindowHandle;
    /// Queues which should be allowed to present
    Queue**                ppPresentQueues;
    /// Number of present queues
    uint32_t               mPresentQueueCount;
    /// Number of backbuffers in this swapchain
    uint32_t               mImageCount;
    /// Width of the swapchain
    uint32_t               mWidth;
    /// Height of the swapchain
    uint32_t               mHeight;
    /// Color format of the swapchain
    TinyImageFormat        mColorFormat;
    /// Clear value
    ClearValue             mColorClearValue;
    /// Swapchain creation flags
    SwapChainCreationFlags mFlags;
    /// Set whether swap chain will be presented using vsync
    bool                   mEnableVsync;
    /// We can toggle to using FLIP model if app desires.
    bool                   mUseFlipSwapEffect;
    /// Optional colorspace for HDR
    ColorSpace             mColorSpace;
};

struct SwapChain
{
    /// Render targets created from the swapchain back buffers
    RenderTarget** ppRenderTargets;
    struct
    {
        /// Use IDXGISwapChain3 for now since IDXGISwapChain4
        /// isn't supported by older devices.
        IDXGISwapChain3* pSwapChain;
        /// Sync interval to specify how interval for vsync
        uint32_t         mSyncInterval : 3;
        uint32_t         mFlags : 10;
    } mDx;
    uint32_t        mImageCount : 8;
    uint32_t        mEnableVsync : 1;
    ColorSpace      mColorSpace : 4;
    TinyImageFormat mFormat : 8;
};

enum ShaderTarget : uint32_t
{
    // 5.1 is supported on all DX12 hardware
    SHADER_TARGET_5_1,
    SHADER_TARGET_6_0,
    SHADER_TARGET_6_1,
    SHADER_TARGET_6_2,
    SHADER_TARGET_6_3, // required for Raytracing
    SHADER_TARGET_6_4, // required for VRS
};

enum GpuMode : uint32_t
{
    GPU_MODE_SINGLE = 0,
    GPU_MODE_LINKED,
    GPU_MODE_UNLINKED,
};

struct RendererDesc
{
    struct
    {
        D3D_FEATURE_LEVEL mFeatureLevel;
    } mDx;

    ShaderTarget mShaderTarget;
    GpuMode      mGpuMode;

    /// Apps may want to query additional state for their applications. That information is transferred through here.
    ExtendedSettings* pExtendedSettings;

    /// Required when creating unlinked multiple renderers. Optional otherwise, can be used for explicit GPU selection.
    RendererContext* pContext;
    uint32_t         mGpuIndex;

    /// This results in new validation not possible during API calls on the CPU, by creating patched shaders that have validation added
    /// directly to the shader. However, it can slow things down a lot, especially for applications with numerous PSOs. Time to see the
    /// first render frame may take several minutes
    bool mEnableGpuBasedValidation;
#if defined(SHADER_STATS_AVAILABLE)
    bool mEnableShaderStats;
#endif
};

struct GPUVendorPreset
{
    uint32_t       mVendorId;
    uint32_t       mModelId;
    uint32_t       mRevisionId; // Optional as not all gpu's have that. Default is : 0x00
    GPUPresetLevel mPresetLevel;
    char           mVendorName[MAX_GPU_VENDOR_STRING_LENGTH];
    char           mGpuName[MAX_GPU_VENDOR_STRING_LENGTH]; // If GPU Name is missing then value will be empty string
    char           mGpuDriverVersion[MAX_GPU_VENDOR_STRING_LENGTH];
    char           mGpuDriverDate[MAX_GPU_VENDOR_STRING_LENGTH];
    uint32_t       mRTCoresCount;
};

enum FormatCapability : uint32_t
{
    FORMAT_CAP_NONE = 0,
    FORMAT_CAP_LINEAR_FILTER = 0x1,
    FORMAT_CAP_READ = 0x2,
    FORMAT_CAP_WRITE = 0x4,
    FORMAT_CAP_READ_WRITE = 0x8,
    FORMAT_CAP_RENDER_TARGET = 0x10,
};
MAKE_ENUM_FLAG(uint32_t, FormatCapability);

struct GPUCapBits
{
    FormatCapability mFormatCaps[TinyImageFormat_Count];
};

enum DefaultResourceAlignment : uint32_t
{
    RESOURCE_BUFFER_ALIGNMENT = 4U,
};

enum WaveOpsSupportFlags : uint32_t
{
    WAVE_OPS_SUPPORT_FLAG_NONE = 0x0,
    WAVE_OPS_SUPPORT_FLAG_BASIC_BIT = 0x00000001,
    WAVE_OPS_SUPPORT_FLAG_VOTE_BIT = 0x00000002,
    WAVE_OPS_SUPPORT_FLAG_ARITHMETIC_BIT = 0x00000004,
    WAVE_OPS_SUPPORT_FLAG_BALLOT_BIT = 0x00000008,
    WAVE_OPS_SUPPORT_FLAG_SHUFFLE_BIT = 0x00000010,
    WAVE_OPS_SUPPORT_FLAG_SHUFFLE_RELATIVE_BIT = 0x00000020,
    WAVE_OPS_SUPPORT_FLAG_CLUSTERED_BIT = 0x00000040,
    WAVE_OPS_SUPPORT_FLAG_QUAD_BIT = 0x00000080,
    WAVE_OPS_SUPPORT_FLAG_PARTITIONED_BIT_NV = 0x00000100,
    WAVE_OPS_SUPPORT_FLAG_ALL = 0x7FFFFFFF
};
MAKE_ENUM_FLAG(uint32_t, WaveOpsSupportFlags);

// update availableGpuProperties in GraphicsConfig.cpp if you made changes to this list
struct GPUSettings
{
    uint64_t            mVRAM;
    uint32_t            mUniformBufferAlignment;
    uint32_t            mUploadBufferTextureAlignment;
    uint32_t            mUploadBufferTextureRowAlignment;
    uint32_t            mMaxVertexInputBindings;
    uint32_t            mMaxRootSignatureDWORDS;
    uint32_t            mWaveLaneCount;
    WaveOpsSupportFlags mWaveOpsSupportFlags;
    GPUVendorPreset     mGpuVendorPreset;
    ShaderStage         mWaveOpsSupportedStageFlags;

    uint32_t          mMaxTotalComputeThreads;
    uint32_t          mMaxComputeThreads[3];
    uint32_t          mMultiDrawIndirect : 1;
    uint32_t          mIndirectRootConstant : 1;
    uint32_t          mBuiltinDrawID : 1;
    uint32_t          mIndirectCommandBuffer : 1;
    uint32_t          mROVsSupported : 1;
    uint32_t          mTessellationSupported : 1;
    uint32_t          mGeometryShaderSupported : 1;
    uint32_t          mGpuMarkers : 1;
    uint32_t          mHDRSupported : 1;
    uint32_t          mTimestampQueries : 1;
    uint32_t          mOcclusionQueries : 1;
    uint32_t          mPipelineStatsQueries : 1;
    uint32_t          mAllowBufferTextureInSameHeap : 1;
    uint32_t          mRaytracingSupported : 1;
    uint32_t          mRayPipelineSupported : 1;
    uint32_t          mRayQuerySupported : 1;
    uint32_t          mSoftwareVRSSupported : 1;
    uint32_t          mPrimitiveIdSupported : 1;
    uint32_t          m64BitAtomicsSupported : 1;
    D3D_FEATURE_LEVEL mFeatureLevel;
    uint32_t          mSuppressInvalidSubresourceStateAfterExit : 1;
    uint32_t          mMaxBoundTextures;
    uint32_t          mSamplerAnisotropySupported : 1;
    uint32_t          mGraphicsQueueSupported : 1;
    uint32_t          mGpuUploadHeapSupported : 1;
    uint32_t          mDirectStorageSupported : 1;
    uint32_t          mAmdAsicFamily;
};

struct alignas(64) Renderer
{
    struct
    {
        // API specific descriptor heap and memory allocator
        struct DescriptorHeap** pCPUDescriptorHeaps;
        struct DescriptorHeap** pCbvSrvUavHeaps;
        struct DescriptorHeap** pSamplerHeaps;
        D3D12MA::Allocator*     pResourceAllocator;
        ID3D12Device*           pDevice;
#if defined(_WINDOWS) && defined(FORGE_DEBUG)
        ID3D12InfoQueue1* pDebugValidation;
        DWORD             mCallbackCookie;
        bool              mUseDebugCallback;
        bool              mSuppressMismatchingCommandListDuringPresent;
#endif
    } mDx;

#if defined(ENABLE_NSIGHT_AFTERMATH)
    // GPU crash dump tracker using Nsight Aftermath instrumentation
    AftermathTracker mAftermathTracker;
#endif
    struct NullDescriptors* pNullDescriptors;
    struct RendererContext* pContext;
    const struct GpuInfo*   pGpu;
    const char*             pName;
    RendererApi             mRendererApi;
    uint32_t                mLinkedNodeCount : 4;
    uint32_t                mUnlinkedRendererIndex : 4;
    uint32_t                mGpuMode : 3;
    uint32_t                mShaderTarget : 4;
    uint32_t                mOwnsContext : 1;
};
// 3 cache lines
static_assert(sizeof(Renderer) <= 24 * sizeof(uint64_t));

struct RendererContextDesc
{
    struct
    {
        D3D_FEATURE_LEVEL mFeatureLevel;
    } mDx;
    bool mEnableGpuBasedValidation;
#if defined(SHADER_STATS_AVAILABLE)
    bool mEnableShaderStats;
#endif
};

struct GpuInfo
{
    struct
    {
        IDXGIAdapter4* pGpu;
    } mDx;
    GPUSettings mSettings;
    GPUCapBits  mCapBits;
};

struct RendererContext
{
    struct
    {
        IDXGIFactory6* pDXGIFactory;
        ID3D12Debug*   pDebug;
#if defined(_WINDOWS) && defined(DRED)
        ID3D12DeviceRemovedExtendedDataSettings* pDredSettings;
#endif
    } mDx;
    GpuInfo  mGpus[MAX_MULTIPLE_GPUS];
    uint32_t mGpuCount;
};

enum DirectStoragePriority : int8_t
{
    DIRECT_STORAGE_PRIORITY_LOW = -1,
    DIRECT_STORAGE_PRIORITY_NORMAL = 0,
    DIRECT_STORAGE_PRIORITY_HIGH = 1,
    DIRECT_STORAGE_PRIORITY_REALTIME = 2,
};

enum DirectStorageSourceType : uint32_t
{
    DIRECT_STORAGE_SOURCE_FILE = 0,
    DIRECT_STORAGE_SOURCE_MEMORY = 1,
};

enum DirectStorageDebugFlags : uint32_t
{
    DIRECT_STORAGE_DEBUG_NONE = 0,
    DIRECT_STORAGE_DEBUG_SHOW_ERRORS = 0x1,
    DIRECT_STORAGE_DEBUG_BREAK_ON_ERROR = 0x2,
    DIRECT_STORAGE_DEBUG_RECORD_OBJECT_NAMES = 0x4,
};
MAKE_ENUM_FLAG(uint32_t, DirectStorageDebugFlags)

enum DirectStorageCompressionFormat : uint32_t
{
    DIRECT_STORAGE_COMPRESSION_NONE = 0,
    DIRECT_STORAGE_COMPRESSION_GDEFLATE = 1,
};

struct DirectStorageDesc
{
    uint32_t                mStagingBufferSize;
    DirectStorageDebugFlags mDebugFlags;
};

struct DirectStorageQueueDesc
{
    DirectStorageSourceType mSourceType;
    uint16_t                mCapacity;
    DirectStoragePriority   mPriority;
    const char*             pName;
};

struct DirectStorageBufferRequest
{
    DirectStorageFile*              pFile;
    const void*                     pMemory;
    uint64_t                        mSourceOffset;
    uint32_t                        mSourceSize;
    Buffer*                         pBuffer;
    uint64_t                        mDestinationOffset;
    uint32_t                        mDestinationSize;
    uint32_t                        mUncompressedSize;
    uint64_t                        mCancellationTag;
    DirectStorageCompressionFormat  mCompressionFormat;
    const char*                     pName;
};

struct DirectStorageTextureRequest
{
    DirectStorageFile*              pFile;
    const void*                     pMemory;
    uint64_t                        mSourceOffset;
    uint32_t                        mSourceSize;
    Texture*                        pTexture;
    uint32_t                        mSubresourceIndex;
    uint32_t                        mX;
    uint32_t                        mY;
    uint32_t                        mZ;
    uint32_t                        mWidth;
    uint32_t                        mHeight;
    uint32_t                        mDepth;
    uint32_t                        mUncompressedSize;
    uint64_t                        mCancellationTag;
    DirectStorageCompressionFormat  mCompressionFormat;
    const char*                     pName;
};

// Indirect command structure define
struct IndirectArgument
{
    IndirectArgumentType mType;
    uint32_t             mOffset;
};

struct IndirectArgumentDescriptor
{
    IndirectArgumentType mType;
    uint32_t             mIndex;
    uint32_t             mByteSize;
};

struct CommandSignatureDesc
{
    RootSignature*              pRootSignature;
    IndirectArgumentDescriptor* pArgDescs;
    uint32_t                    mIndirectArgCount;
    /// Set to true if indirect argument struct should not be aligned to 16 bytes
    bool                        mPacked;
};

struct CommandSignature
{
#if defined(DIRECT3D12)
    ID3D12CommandSignature* pHandle;
#endif
    IndirectArgumentType mDrawType;
    uint32_t             mStride;
};

struct DescriptorSetDesc
{
    RootSignature*            pRootSignature;
    DescriptorUpdateFrequency mUpdateFrequency;
    uint32_t                  mMaxSets;
    uint32_t                  mNodeIndex;
};

struct QueueSubmitDesc
{
    Cmd**       ppCmds;
    Fence*      pSignalFence;
    Semaphore** ppWaitSemaphores;
    Semaphore** ppSignalSemaphores;
    uint32_t    mCmdCount;
    uint32_t    mWaitSemaphoreCount;
    uint32_t    mSignalSemaphoreCount;
    bool        mSubmitDone;
};

struct QueuePresentDesc
{
    SwapChain*  pSwapChain;
    Semaphore** ppWaitSemaphores;
    uint32_t    mWaitSemaphoreCount;
    uint8_t     mIndex;
    bool        mSubmitDone;
};

struct BindRenderTargetDesc
{
    RenderTarget*   pRenderTarget;
    LoadActionType  mLoadAction;
    StoreActionType mStoreAction;
    ClearValue      mClearValue;
    LoadActionType  mLoadActionStencil;
    StoreActionType mStoreActionStencil;
    uint32_t        mArraySlice;
    uint32_t        mMipSlice : 10;
    uint32_t        mOverrideClearValue : 1;
    uint32_t        mUseArraySlice : 1;
    uint32_t        mUseMipSlice : 1;
};

struct BindDepthTargetDesc
{
    RenderTarget*   pDepthStencil;
    LoadActionType  mLoadAction;
    LoadActionType  mLoadActionStencil;
    StoreActionType mStoreAction;
    StoreActionType mStoreActionStencil;
    ClearValue      mClearValue;
    uint32_t        mArraySlice;
    uint32_t        mMipSlice : 10;
    uint32_t        mOverrideClearValue : 1;
    uint32_t        mUseArraySlice : 1;
    uint32_t        mUseMipSlice : 1;
};

struct BindRenderTargetsDesc
{
    uint32_t             mRenderTargetCount;
    BindRenderTargetDesc mRenderTargets[MAX_RENDER_TARGET_ATTACHMENTS];
    BindDepthTargetDesc  mDepthStencil;
    // Explicit viewport for empty render pass
    uint32_t             mExtent[2];
};

// clang-format off
// Utilities functions
FORGE_RENDERER_API void setRendererInitializationError(const char* reason);
FORGE_RENDERER_API bool hasRendererInitializationError(const char** outReason);

// API functions

// Multiple renderer API (optional)
FORGE_RENDERER_API void FORGE_CALLCONV initRendererContext(const char* appName, const RendererContextDesc* pSettings, RendererContext** ppContext);
FORGE_RENDERER_API void FORGE_CALLCONV exitRendererContext(RendererContext* pContext);

// allocates memory and initializes the renderer -> returns pRenderer
//
FORGE_RENDERER_API void FORGE_CALLCONV initRenderer(const char* appName, const RendererDesc* pSettings, Renderer** ppRenderer);
FORGE_RENDERER_API void FORGE_CALLCONV exitRenderer(Renderer* pRenderer);

FORGE_RENDERER_API void FORGE_CALLCONV addFence(Renderer* pRenderer, Fence** ppFence);
FORGE_RENDERER_API void FORGE_CALLCONV removeFence(Renderer* pRenderer, Fence* pFence);

FORGE_RENDERER_API void FORGE_CALLCONV addSemaphore(Renderer* pRenderer, Semaphore** ppSemaphore);
FORGE_RENDERER_API void FORGE_CALLCONV removeSemaphore(Renderer* pRenderer, Semaphore* pSemaphore);

FORGE_RENDERER_API void FORGE_CALLCONV addQueue(Renderer* pRenderer, QueueDesc* pQDesc, Queue** ppQueue);
FORGE_RENDERER_API void FORGE_CALLCONV removeQueue(Renderer* pRenderer, Queue* pQueue);

FORGE_RENDERER_API void FORGE_CALLCONV addSwapChain(Renderer* pRenderer, const SwapChainDesc* pDesc, SwapChain** ppSwapChain);
FORGE_RENDERER_API void FORGE_CALLCONV removeSwapChain(Renderer* pRenderer, SwapChain* pSwapChain);

// memory functions
FORGE_RENDERER_API void FORGE_CALLCONV addResourceHeap(Renderer* pRenderer, const ResourceHeapDesc* pDesc, ResourceHeap** ppHeap);
FORGE_RENDERER_API void FORGE_CALLCONV removeResourceHeap(Renderer* pRenderer, ResourceHeap* pHeap);
FORGE_RENDERER_API bool FORGE_CALLCONV isGpuUploadHeapSupported(Renderer* pRenderer);

// command pool functions
FORGE_RENDERER_API void FORGE_CALLCONV addCmdPool(Renderer* pRenderer, const CmdPoolDesc* pDesc, CmdPool** ppCmdPool);
FORGE_RENDERER_API void FORGE_CALLCONV removeCmdPool(Renderer* pRenderer, CmdPool* pCmdPool);
FORGE_RENDERER_API void FORGE_CALLCONV addCmd(Renderer* pRenderer, const CmdDesc* pDesc, Cmd** ppCmd);
FORGE_RENDERER_API void FORGE_CALLCONV removeCmd(Renderer* pRenderer, Cmd* pCmd);
FORGE_RENDERER_API void FORGE_CALLCONV addCmd_n(Renderer* pRenderer, const CmdDesc* pDesc, uint32_t cmdCount, Cmd*** pppCmds);
FORGE_RENDERER_API void FORGE_CALLCONV removeCmd_n(Renderer* pRenderer, uint32_t cmdCount, Cmd** ppCmds);

//
// All buffer, texture loading handled by resource system -> IResourceLoader.*
//

FORGE_RENDERER_API void FORGE_CALLCONV addRenderTarget(Renderer* pRenderer, const RenderTargetDesc* pDesc, RenderTarget** ppRenderTarget);
FORGE_RENDERER_API void FORGE_CALLCONV removeRenderTarget(Renderer* pRenderer, RenderTarget* pRenderTarget);
FORGE_RENDERER_API void FORGE_CALLCONV addSampler(Renderer* pRenderer, const SamplerDesc* pDesc, Sampler** ppSampler);
FORGE_RENDERER_API void FORGE_CALLCONV removeSampler(Renderer* pRenderer, Sampler* pSampler);

// shader functions
FORGE_RENDERER_API void FORGE_CALLCONV addShaderSource(Renderer* pRenderer, const ShaderSrcDesc* pDesc, Shader** ppShaderProgram);
FORGE_RENDERER_API void FORGE_CALLCONV addShaderBinary(Renderer* pRenderer, const BinaryShaderDesc* pDesc, Shader** ppShaderProgram);
FORGE_RENDERER_API void FORGE_CALLCONV removeShader(Renderer* pRenderer, Shader* pShaderProgram);

FORGE_RENDERER_API void FORGE_CALLCONV addRootSignature(Renderer* pRenderer, const RootSignatureDesc* pDesc, RootSignature** ppRootSignature);
FORGE_RENDERER_API void FORGE_CALLCONV removeRootSignature(Renderer* pRenderer, RootSignature* pRootSignature);
FORGE_RENDERER_API uint32_t FORGE_CALLCONV getDescriptorIndexFromName(const RootSignature* pRootSignature, const char* pName);

// pipeline functions
FORGE_RENDERER_API void FORGE_CALLCONV addPipeline(Renderer* pRenderer, const PipelineDesc* pPipelineSettings, Pipeline** ppPipeline);
FORGE_RENDERER_API void FORGE_CALLCONV removePipeline(Renderer* pRenderer, Pipeline* pPipeline);
FORGE_RENDERER_API void FORGE_CALLCONV addPipelineCache(Renderer* pRenderer, const PipelineCacheDesc* pDesc, PipelineCache** ppPipelineCache);
FORGE_RENDERER_API void FORGE_CALLCONV getPipelineCacheData(Renderer* pRenderer, PipelineCache* pPipelineCache, size_t* pSize, void* pData);
#if defined(SHADER_STATS_AVAILABLE)
FORGE_RENDERER_API void FORGE_CALLCONV addPipelineStats(Renderer* pRenderer, Pipeline* pPipeline, bool generateDisassembly, PipelineStats* pOutStats);
FORGE_RENDERER_API void FORGE_CALLCONV removePipelineStats(Renderer* pRenderer, PipelineStats* pStats);
#endif
FORGE_RENDERER_API void FORGE_CALLCONV removePipelineCache(Renderer* pRenderer, PipelineCache* pPipelineCache);

// Descriptor Set functions
FORGE_RENDERER_API void FORGE_CALLCONV addDescriptorSet(Renderer* pRenderer, const DescriptorSetDesc* pDesc, DescriptorSet** ppDescriptorSet);
FORGE_RENDERER_API void FORGE_CALLCONV removeDescriptorSet(Renderer* pRenderer, DescriptorSet* pDescriptorSet);
FORGE_RENDERER_API void FORGE_CALLCONV updateDescriptorSet(Renderer* pRenderer, uint32_t index, DescriptorSet* pDescriptorSet, uint32_t count, const DescriptorData* pParams);

// command buffer functions
FORGE_RENDERER_API void FORGE_CALLCONV resetCmdPool(Renderer* pRenderer, CmdPool* pCmdPool);
FORGE_RENDERER_API void FORGE_CALLCONV beginCmd(Cmd* pCmd);
FORGE_RENDERER_API void FORGE_CALLCONV endCmd(Cmd* pCmd);
FORGE_RENDERER_API void FORGE_CALLCONV cmdBindRenderTargets(Cmd* pCmd, const BindRenderTargetsDesc* pDesc);
FORGE_RENDERER_API void FORGE_CALLCONV cmdSetSampleLocations(Cmd* pCmd, SampleCount samplesCount, uint32_t gridSizeX, uint32_t gridSizeY, SampleLocations* plocations);
FORGE_RENDERER_API void FORGE_CALLCONV cmdSetViewport(Cmd* pCmd, float x, float y, float width, float height, float minDepth, float maxDepth);
FORGE_RENDERER_API void FORGE_CALLCONV cmdSetScissor(Cmd* pCmd, uint32_t x, uint32_t y, uint32_t width, uint32_t height);
FORGE_RENDERER_API void FORGE_CALLCONV cmdSetStencilReferenceValue(Cmd* pCmd, uint32_t val);
FORGE_RENDERER_API void FORGE_CALLCONV cmdBindPipeline(Cmd* pCmd, Pipeline* pPipeline);
FORGE_RENDERER_API void FORGE_CALLCONV cmdBindDescriptorSet(Cmd* pCmd, uint32_t index, DescriptorSet* pDescriptorSet);
FORGE_RENDERER_API void FORGE_CALLCONV cmdBindPushConstants(Cmd* pCmd, RootSignature* pRootSignature, uint32_t paramIndex, const void* pConstants);
FORGE_RENDERER_API void FORGE_CALLCONV cmdBindDescriptorSetWithRootCbvs(Cmd* pCmd, uint32_t index, DescriptorSet* pDescriptorSet, uint32_t count, const DescriptorData* pParams);
FORGE_RENDERER_API void FORGE_CALLCONV cmdBindIndexBuffer(Cmd* pCmd, Buffer* pBuffer, uint32_t indexType, uint64_t offset);
FORGE_RENDERER_API void FORGE_CALLCONV cmdBindVertexBuffer(Cmd* pCmd, uint32_t bufferCount, Buffer** ppBuffers, const uint32_t* pStrides, const uint64_t* pOffsets);
FORGE_RENDERER_API void FORGE_CALLCONV cmdDraw(Cmd* pCmd, uint32_t vertexCount, uint32_t firstVertex);
FORGE_RENDERER_API void FORGE_CALLCONV cmdDrawInstanced(Cmd* pCmd, uint32_t vertexCount, uint32_t firstVertex, uint32_t instanceCount, uint32_t firstInstance);
FORGE_RENDERER_API void FORGE_CALLCONV cmdDrawIndexed(Cmd* pCmd, uint32_t indexCount, uint32_t firstIndex, uint32_t firstVertex);
FORGE_RENDERER_API void FORGE_CALLCONV cmdDrawIndexedInstanced(Cmd* pCmd, uint32_t indexCount, uint32_t firstIndex, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance);
FORGE_RENDERER_API void FORGE_CALLCONV cmdDispatch(Cmd* pCmd, uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ);

// Transition Commands
FORGE_RENDERER_API void FORGE_CALLCONV cmdResourceBarrier(Cmd* pCmd, uint32_t bufferBarrierCount, BufferBarrier* pBufferBarriers, uint32_t textureBarrierCount, TextureBarrier* pTextureBarriers, uint32_t rtBarrierCount, RenderTargetBarrier* pRtBarriers);

// queue/fence/swapchain functions
FORGE_RENDERER_API void FORGE_CALLCONV acquireNextImage(Renderer* pRenderer, SwapChain* pSwapChain, Semaphore* pSignalSemaphore, Fence* pFence, uint32_t* pImageIndex);
FORGE_RENDERER_API void FORGE_CALLCONV queueSubmit(Queue* pQueue, const QueueSubmitDesc* pDesc);
FORGE_RENDERER_API void FORGE_CALLCONV queuePresent(Queue* pQueue, const QueuePresentDesc* pDesc);
FORGE_RENDERER_API void FORGE_CALLCONV waitQueueIdle(Queue* pQueue);
FORGE_RENDERER_API void FORGE_CALLCONV getFenceStatus(Renderer* pRenderer, Fence* pFence, FenceStatus* pFenceStatus);
FORGE_RENDERER_API void FORGE_CALLCONV waitForFences(Renderer* pRenderer, uint32_t fenceCount, Fence** ppFences);
FORGE_RENDERER_API void FORGE_CALLCONV toggleVSync(Renderer* pRenderer, SwapChain** ppSwapchain);

/************************************************************************/
// DirectStorage Interface
/************************************************************************/
FORGE_RENDERER_API bool FORGE_CALLCONV isDirectStorageSupported(Renderer* pRenderer);
FORGE_RENDERER_API HRESULT FORGE_CALLCONV initDirectStorage(Renderer* pRenderer, const DirectStorageDesc* pDesc,
                                                           DirectStorage** ppDirectStorage);
FORGE_RENDERER_API void FORGE_CALLCONV exitDirectStorage(DirectStorage* pDirectStorage);
FORGE_RENDERER_API HRESULT FORGE_CALLCONV addDirectStorageQueue(DirectStorage* pDirectStorage, const DirectStorageQueueDesc* pDesc,
                                                               DirectStorageQueue** ppQueue);
FORGE_RENDERER_API void FORGE_CALLCONV removeDirectStorageQueue(DirectStorageQueue* pQueue);
FORGE_RENDERER_API HRESULT FORGE_CALLCONV openDirectStorageFile(DirectStorage* pDirectStorage, const wchar_t* pPath,
                                                               DirectStorageFile** ppFile);
FORGE_RENDERER_API void FORGE_CALLCONV closeDirectStorageFile(DirectStorageFile* pFile);
FORGE_RENDERER_API HRESULT FORGE_CALLCONV addDirectStorageStatusArray(DirectStorage* pDirectStorage, uint32_t capacity, const char* pName,
                                                                     DirectStorageStatusArray** ppStatusArray);
FORGE_RENDERER_API void FORGE_CALLCONV removeDirectStorageStatusArray(DirectStorageStatusArray* pStatusArray);
FORGE_RENDERER_API bool FORGE_CALLCONV isDirectStorageStatusComplete(DirectStorageStatusArray* pStatusArray, uint32_t index);
FORGE_RENDERER_API HRESULT FORGE_CALLCONV getDirectStorageStatus(DirectStorageStatusArray* pStatusArray, uint32_t index);
FORGE_RENDERER_API void FORGE_CALLCONV directStorageEnqueueBufferRequest(DirectStorageQueue* pQueue,
                                                                        const DirectStorageBufferRequest* pRequest);
FORGE_RENDERER_API void FORGE_CALLCONV directStorageEnqueueTextureRequest(DirectStorageQueue* pQueue,
                                                                         const DirectStorageTextureRequest* pRequest);
FORGE_RENDERER_API void FORGE_CALLCONV directStorageEnqueueStatus(DirectStorageQueue* pQueue, DirectStorageStatusArray* pStatusArray,
                                                                 uint32_t index);
FORGE_RENDERER_API void FORGE_CALLCONV directStorageEnqueueSignal(DirectStorageQueue* pQueue, Fence* pFence, uint64_t value);
FORGE_RENDERER_API void FORGE_CALLCONV directStorageSubmit(DirectStorageQueue* pQueue);

//Returns the recommended format for the swapchain.
//If true is passed for the hintHDR parameter, it will return an HDR format IF the platform supports it
//If false is passed or the platform does not support HDR a non HDR format is returned.
//If true is passed for the hintSrgb parameter, it will return format that is will do gamma correction automatically
//If false is passed for the hintSrgb parameter the gamma correction should be done as a postprocess step before submitting image to swapchain
FORGE_RENDERER_API TinyImageFormat FORGE_CALLCONV getSupportedSwapchainFormat(Renderer* pRenderer, const SwapChainDesc* pDesc, ColorSpace colorSpace);
FORGE_RENDERER_API uint32_t FORGE_CALLCONV getRecommendedSwapchainImageCount(Renderer* pRenderer, const WindowHandle* hwnd);

//indirect Draw functions
FORGE_RENDERER_API void FORGE_CALLCONV addIndirectCommandSignature(Renderer* pRenderer, const CommandSignatureDesc* pDesc, CommandSignature** ppCommandSignature);
FORGE_RENDERER_API void FORGE_CALLCONV removeIndirectCommandSignature(Renderer* pRenderer, CommandSignature* pCommandSignature);
FORGE_RENDERER_API void FORGE_CALLCONV cmdExecuteIndirect(Cmd* pCmd, CommandSignature* pCommandSignature, unsigned int maxCommandCount, Buffer* pIndirectBuffer, uint64_t bufferOffset, Buffer* pCounterBuffer, uint64_t counterBufferOffset);

/************************************************************************/
// GPU Query Interface
/************************************************************************/
FORGE_RENDERER_API void FORGE_CALLCONV getTimestampFrequency(Queue* pQueue, double* pFrequency);
FORGE_RENDERER_API void FORGE_CALLCONV addQueryPool(Renderer* pRenderer, const QueryPoolDesc* pDesc, QueryPool** ppQueryPool);
FORGE_RENDERER_API void FORGE_CALLCONV removeQueryPool(Renderer* pRenderer, QueryPool* pQueryPool);
FORGE_RENDERER_API void FORGE_CALLCONV cmdBeginQuery(Cmd* pCmd, QueryPool* pQueryPool, QueryDesc* pQuery);
FORGE_RENDERER_API void FORGE_CALLCONV cmdEndQuery(Cmd* pCmd, QueryPool* pQueryPool, QueryDesc* pQuery);
FORGE_RENDERER_API void FORGE_CALLCONV cmdResolveQuery(Cmd* pCmd, QueryPool* pQueryPool, uint32_t startQuery, uint32_t queryCount);
FORGE_RENDERER_API void FORGE_CALLCONV cmdResetQuery(Cmd* pCmd, QueryPool* pQueryPool, uint32_t startQuery, uint32_t queryCount);
FORGE_RENDERER_API void FORGE_CALLCONV getQueryData(Renderer* pRenderer, QueryPool* pQueryPool, uint32_t queryIndex, QueryData* pOutData);
/************************************************************************/
// Stats Info Interface
/************************************************************************/
FORGE_RENDERER_API void FORGE_CALLCONV calculateMemoryStats(Renderer* pRenderer, char** ppStats);
FORGE_RENDERER_API void FORGE_CALLCONV calculateMemoryUse(Renderer* pRenderer, uint64_t* usedBytes, uint64_t* totalAllocatedBytes);
FORGE_RENDERER_API void FORGE_CALLCONV freeMemoryStats(Renderer* pRenderer, char* pStats);
/************************************************************************/
// Debug Marker Interface
/************************************************************************/
FORGE_RENDERER_API void FORGE_CALLCONV cmdBeginDebugMarker(Cmd* pCmd, float r, float g, float b, const char* pName);
FORGE_RENDERER_API void FORGE_CALLCONV cmdEndDebugMarker(Cmd* pCmd);
FORGE_RENDERER_API void FORGE_CALLCONV cmdAddDebugMarker(Cmd* pCmd, float r, float g, float b, const char* pName);
FORGE_RENDERER_API void FORGE_CALLCONV cmdWriteMarker(Cmd* pCmd, const MarkerDesc* pDesc);
/************************************************************************/
// Resource Debug Naming Interface
/************************************************************************/
FORGE_RENDERER_API void FORGE_CALLCONV setBufferName(Renderer* pRenderer, Buffer* pBuffer, const char* pName);
FORGE_RENDERER_API void FORGE_CALLCONV setTextureName(Renderer* pRenderer, Texture* pTexture, const char* pName);
FORGE_RENDERER_API void FORGE_CALLCONV setRenderTargetName(Renderer* pRenderer, RenderTarget* pRenderTarget, const char* pName);
FORGE_RENDERER_API void FORGE_CALLCONV setPipelineName(Renderer* pRenderer, Pipeline* pPipeline, const char* pName);
/************************************************************************/
/************************************************************************/
// clang-format on
