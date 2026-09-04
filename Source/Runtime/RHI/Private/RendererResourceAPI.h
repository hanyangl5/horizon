#pragma once

#include "RHI/IGraphics.h"

struct SubresourceDataDesc;

// enum BarrierSyncStage : uint64_t
// {
//     // No synchronization scope. Must be paired with RESOURCE_ACCESS_NO_ACCESS.
//     BARRIER_STAGE_NONE = 0,
//     // All GPU work scopes.
//     BARRIER_STAGE_ALL = 0x1,
//     // Entire graphics draw pipeline.
//     BARRIER_STAGE_DRAW = 0x2,
//     // Index input and vertex/index fetch.
//     BARRIER_STAGE_INDEX_INPUT = 0x4,
//     // Vertex, hull, domain, geometry, amplification, and mesh shading.
//     BARRIER_STAGE_VERTEX_SHADING = 0x8,
//     // Pixel shading.
//     BARRIER_STAGE_PIXEL_SHADING = 0x10,
//     // Depth/stencil testing and updates.
//     BARRIER_STAGE_DEPTH_STENCIL = 0x20,
//     // Render target output.
//     BARRIER_STAGE_RENDER_TARGET = 0x40,
//     // Compute shading.
//     BARRIER_STAGE_COMPUTE_SHADING = 0x80,
//     // Raytracing dispatch.
//     BARRIER_STAGE_RAYTRACING = 0x100,
//     // Copy commands.
//     BARRIER_STAGE_COPY = 0x200,
//     // Resolve commands.
//     BARRIER_STAGE_RESOLVE = 0x400,
//     // ExecuteIndirect argument consumption.
//     BARRIER_STAGE_EXECUTE_INDIRECT = 0x800,
//     // Predication argument consumption. D3D12 aliases this with execute-indirect.
//     BARRIER_STAGE_PREDICATION = BARRIER_STAGE_EXECUTE_INDIRECT,
//     // All shader execution scopes.
//     BARRIER_STAGE_ALL_SHADING = 0x1000,
//     // Non-pixel shader scopes: vertex shading, compute shading, and raytracing.
//     BARRIER_STAGE_NON_PIXEL_SHADING = 0x2000,
//     // EmitRaytracingAccelerationStructurePostbuildInfo.
//     BARRIER_STAGE_EMIT_RAYTRACING_ACCELERATION_STRUCTURE_POSTBUILD_INFO = 0x4000,
//     // ClearUnorderedAccessView operations.
//     BARRIER_STAGE_CLEAR_UNORDERED_ACCESS_VIEW = 0x8000,
//     // Video decode work.
//     BARRIER_STAGE_VIDEO_DECODE = 0x100000,
//     // Video process work.
//     BARRIER_STAGE_VIDEO_PROCESS = 0x200000,
//     // Video encode work.
//     BARRIER_STAGE_VIDEO_ENCODE = 0x400000,
//     // BuildRaytracingAccelerationStructure.
//     BARRIER_STAGE_BUILD_RAYTRACING_ACCELERATION_STRUCTURE = 0x800000,
//     // CopyRaytracingAccelerationStructure.
//     BARRIER_STAGE_COPY_RAYTRACING_ACCELERATION_STRUCTURE = 0x1000000,
//     // ConvertLinearAlgebraMatrix.
//     BARRIER_STAGE_CONVERT_LINEAR_ALGEBRA_MATRIX = 0x20000000,
//     // Split barrier marker scope.
//     BARRIER_STAGE_SPLIT = 0x80000000,

//     // Legacy runtime alias for the full graphics draw pipeline.
//     BARRIER_STAGE_ALL_GRAPHICS = BARRIER_STAGE_DRAW,
//     // Legacy runtime alias for vertex shading.
//     BARRIER_STAGE_VERTEX_SHADER = BARRIER_STAGE_VERTEX_SHADING,
//     // Legacy runtime alias for pixel shading.
//     BARRIER_STAGE_PIXEL_SHADER = BARRIER_STAGE_PIXEL_SHADING,
//     // Legacy runtime alias for compute shading.
//     BARRIER_STAGE_COMPUTE_SHADER = BARRIER_STAGE_COMPUTE_SHADING,
//     // Legacy runtime alias for render target output.
//     BARRIER_STAGE_COLOR_OUTPUT = BARRIER_STAGE_RENDER_TARGET,
//     // Legacy runtime alias for acceleration structure builds.
//     BARRIER_STAGE_ACCELERATION_STRUCTURE_BUILD = BARRIER_STAGE_BUILD_RAYTRACING_ACCELERATION_STRUCTURE,
// };
// MAKE_ENUM_FLAG(uint64_t, BarrierSyncStage)

// enum BarrierResourceAccess : uint64_t
// {
//     // Any layout-compatible access. Avoid as AccessBefore unless a broad flush is intended.
//     RESOURCE_ACCESS_COMMON = 0,
//     // Vertex buffer read.
//     RESOURCE_ACCESS_VERTEX_BUFFER = 0x1,
//     // Constant buffer read.
//     RESOURCE_ACCESS_CONSTANT_BUFFER = 0x2,
//     // Index buffer read.
//     RESOURCE_ACCESS_INDEX_BUFFER = 0x4,
//     // Render target read/write access.
//     RESOURCE_ACCESS_RENDER_TARGET = 0x8,
//     // UAV read/write access.
//     RESOURCE_ACCESS_UNORDERED_ACCESS = 0x10,
//     // Depth/stencil write access.
//     RESOURCE_ACCESS_DEPTH_STENCIL_WRITE = 0x20,
//     // Depth/stencil read access.
//     RESOURCE_ACCESS_DEPTH_STENCIL_READ = 0x40,
//     // Shader resource read access.
//     RESOURCE_ACCESS_SHADER_RESOURCE = 0x80,
//     // Stream-output write access.
//     RESOURCE_ACCESS_STREAM_OUTPUT = 0x100,
//     // ExecuteIndirect argument read access.
//     RESOURCE_ACCESS_INDIRECT_ARGUMENT = 0x200,
//     // Predication argument read access. D3D12 aliases this with indirect arguments.
//     RESOURCE_ACCESS_PREDICATION = RESOURCE_ACCESS_INDIRECT_ARGUMENT,
//     // Copy destination write access.
//     RESOURCE_ACCESS_COPY_DEST = 0x400,
//     // Copy source read access.
//     RESOURCE_ACCESS_COPY_SOURCE = 0x800,
//     // Resolve destination write access.
//     RESOURCE_ACCESS_RESOLVE_DEST = 0x1000,
//     // Resolve source read access.
//     RESOURCE_ACCESS_RESOLVE_SOURCE = 0x2000,
//     // Raytracing acceleration structure read access.
//     RESOURCE_ACCESS_RAYTRACING_ACCELERATION_STRUCTURE_READ = 0x4000,
//     // Raytracing acceleration structure write access.
//     RESOURCE_ACCESS_RAYTRACING_ACCELERATION_STRUCTURE_WRITE = 0x8000,
//     // Shading-rate image read access.
//     RESOURCE_ACCESS_SHADING_RATE_SOURCE = 0x10000,
//     // Video decode read access.
//     RESOURCE_ACCESS_VIDEO_DECODE_READ = 0x20000,
//     // Video decode write access.
//     RESOURCE_ACCESS_VIDEO_DECODE_WRITE = 0x40000,
//     // Video process read access.
//     RESOURCE_ACCESS_VIDEO_PROCESS_READ = 0x80000,
//     // Video process write access.
//     RESOURCE_ACCESS_VIDEO_PROCESS_WRITE = 0x100000,
//     // Video encode read access.
//     RESOURCE_ACCESS_VIDEO_ENCODE_READ = 0x200000,
//     // Video encode write access.
//     RESOURCE_ACCESS_VIDEO_ENCODE_WRITE = 0x400000,
//     // Global barrier access mask.
//     RESOURCE_ACCESS_GLOBAL = 0x40000000,
//     // No resource memory access. Must be paired with BARRIER_STAGE_NONE.
//     RESOURCE_ACCESS_NO_ACCESS = 0x80000000,

//     // Legacy runtime alias for no memory access.
//     RESOURCE_ACCESS_NONE = RESOURCE_ACCESS_NO_ACCESS,
//     // Legacy runtime alias for UAV access.
//     RESOURCE_ACCESS_SHADER_UAV = RESOURCE_ACCESS_UNORDERED_ACCESS,
//     // Legacy runtime alias for acceleration structure read access.
//     RESOURCE_ACCESS_ACCELERATION_STRUCTURE_READ = RESOURCE_ACCESS_RAYTRACING_ACCELERATION_STRUCTURE_READ,
//     // Legacy runtime alias for acceleration structure write access.
//     RESOURCE_ACCESS_ACCELERATION_STRUCTURE_WRITE = RESOURCE_ACCESS_RAYTRACING_ACCELERATION_STRUCTURE_WRITE,
//     // Legacy runtime alias for present/no-access transitions.
//     RESOURCE_ACCESS_PRESENT = RESOURCE_ACCESS_NO_ACCESS,
// };
// MAKE_ENUM_FLAG(uint64_t, BarrierResourceAccess)

// enum BarrierTextureLayout : uint32_t
// {
//     // Undefined texture layout. Contents may be discarded or uninitialized.
//     TEXTURE_LAYOUT_UNDEFINED = 0xffffffff,
//     // Queue-neutral common layout.
//     TEXTURE_LAYOUT_COMMON = 0,
//     // Present layout. D3D12 aliases this with COMMON.
//     TEXTURE_LAYOUT_PRESENT = TEXTURE_LAYOUT_COMMON,
//     // Generic read layout.
//     TEXTURE_LAYOUT_GENERIC_READ = 1,
//     // Render target layout.
//     TEXTURE_LAYOUT_RENDER_TARGET = 2,
//     // UAV layout.
//     TEXTURE_LAYOUT_UNORDERED_ACCESS = 3,
//     // Writable depth/stencil layout.
//     TEXTURE_LAYOUT_DEPTH_STENCIL_WRITE = 4,
//     // Read-only depth/stencil layout.
//     TEXTURE_LAYOUT_DEPTH_STENCIL_READ = 5,
//     // Shader resource layout.
//     TEXTURE_LAYOUT_SHADER_RESOURCE = 6,
//     // Copy source layout.
//     TEXTURE_LAYOUT_COPY_SOURCE = 7,
//     // Copy destination layout.
//     TEXTURE_LAYOUT_COPY_DEST = 8,
//     // Resolve source layout.
//     TEXTURE_LAYOUT_RESOLVE_SOURCE = 9,
//     // Resolve destination layout.
//     TEXTURE_LAYOUT_RESOLVE_DEST = 10,
//     // Shading-rate image layout.
//     TEXTURE_LAYOUT_SHADING_RATE_SOURCE = 11,
//     // Video decode read layout.
//     TEXTURE_LAYOUT_VIDEO_DECODE_READ = 12,
//     // Video decode write layout.
//     TEXTURE_LAYOUT_VIDEO_DECODE_WRITE = 13,
//     // Video process read layout.
//     TEXTURE_LAYOUT_VIDEO_PROCESS_READ = 14,
//     // Video process write layout.
//     TEXTURE_LAYOUT_VIDEO_PROCESS_WRITE = 15,
//     // Video encode read layout.
//     TEXTURE_LAYOUT_VIDEO_ENCODE_READ = 16,
//     // Video encode write layout.
//     TEXTURE_LAYOUT_VIDEO_ENCODE_WRITE = 17,
//     // Direct queue common layout.
//     TEXTURE_LAYOUT_DIRECT_QUEUE_COMMON = 18,
//     // Direct queue generic read layout.
//     TEXTURE_LAYOUT_DIRECT_QUEUE_GENERIC_READ = 19,
//     // Direct queue UAV layout.
//     TEXTURE_LAYOUT_DIRECT_QUEUE_UNORDERED_ACCESS = 20,
//     // Direct queue shader resource layout.
//     TEXTURE_LAYOUT_DIRECT_QUEUE_SHADER_RESOURCE = 21,
//     // Direct queue copy source layout.
//     TEXTURE_LAYOUT_DIRECT_QUEUE_COPY_SOURCE = 22,
//     // Direct queue copy destination layout.
//     TEXTURE_LAYOUT_DIRECT_QUEUE_COPY_DEST = 23,
//     // Compute queue common layout.
//     TEXTURE_LAYOUT_COMPUTE_QUEUE_COMMON = 24,
//     // Compute queue generic read layout.
//     TEXTURE_LAYOUT_COMPUTE_QUEUE_GENERIC_READ = 25,
//     // Compute queue UAV layout.
//     TEXTURE_LAYOUT_COMPUTE_QUEUE_UNORDERED_ACCESS = 26,
//     // Compute queue shader resource layout.
//     TEXTURE_LAYOUT_COMPUTE_QUEUE_SHADER_RESOURCE = 27,
//     // Compute queue copy source layout.
//     TEXTURE_LAYOUT_COMPUTE_QUEUE_COPY_SOURCE = 28,
//     // Compute queue copy destination layout.
//     TEXTURE_LAYOUT_COMPUTE_QUEUE_COPY_DEST = 29,
//     // Direct queue generic-read layout that compute queues may also access.
//     TEXTURE_LAYOUT_DIRECT_QUEUE_GENERIC_READ_COMPUTE_QUEUE_ACCESSIBLE = 31,
// };

// enum BarrierFlags : uint32_t
// {
//     BARRIER_FLAG_NONE = 0,
//     BARRIER_FLAG_BEGIN_ONLY = 0x1,
//     BARRIER_FLAG_END_ONLY = 0x2,
//     BARRIER_FLAG_DISCARD = 0x4,
// };
// MAKE_ENUM_FLAG(uint32_t, BarrierFlags)

// struct ResourceState_2
// {
//     BarrierSyncStage      mStage = BARRIER_STAGE_NONE;
//     BarrierResourceAccess mAccess = RESOURCE_ACCESS_NONE;
//     BarrierTextureLayout  mLayout = TEXTURE_LAYOUT_UNDEFINED;
// };

// static constexpr ResourceState_2 RESOURCE_USAGE_UNDEFINED = {};
// static constexpr ResourceState_2 RESOURCE_USAGE_PRESENT = { BARRIER_STAGE_NONE, RESOURCE_ACCESS_PRESENT, TEXTURE_LAYOUT_PRESENT };
// static constexpr ResourceState_2 RESOURCE_USAGE_COMMON = { BARRIER_STAGE_NONE, RESOURCE_ACCESS_NONE, TEXTURE_LAYOUT_COMMON };

// struct GlobalBarrier2
// {
//     ResourceState_2 mUsageBefore;
//     ResourceState_2 mUsageAfter;
// };

// struct BufferBarrier2
// {
//     Buffer*              pBuffer;
//     ResourceState_2 mUsageBefore;
//     ResourceState_2 mUsageAfter;
//     uint64_t             mOffset;
//     uint64_t             mSize;
//     BarrierFlags         mFlags = BARRIER_FLAG_NONE;
// };

// struct TextureBarrier2
// {
//     Texture*             pTexture;
//     ResourceState_2 mUsageBefore;
//     ResourceState_2 mUsageAfter;
//     uint8_t              mSubresourceBarrier : 1;
//     uint8_t              mMipLevel : 7;
//     uint8_t              mMipCount;
//     uint16_t             mArrayLayer;
//     uint16_t             mArrayLayerCount;
//     uint8_t              mPlaneSlice;
//     uint8_t              mPlaneSliceCount;
//     BarrierFlags         mFlags = BARRIER_FLAG_NONE;
// };

// struct RenderTargetBarrier2
// {
//     RenderTarget*        pRenderTarget;
//     ResourceState_2 mUsageBefore;
//     ResourceState_2 mUsageAfter;
//     uint8_t              mSubresourceBarrier : 1;
//     uint8_t              mMipLevel : 7;
//     uint8_t              mMipCount;
//     uint16_t             mArrayLayer;
//     uint16_t             mArrayLayerCount;
//     uint8_t              mPlaneSlice;
//     uint8_t              mPlaneSliceCount;
//     BarrierFlags         mFlags = BARRIER_FLAG_NONE;
// };

// struct BarrierDesc
// {
//     uint32_t                    mGlobalBarrierCount;
//     const GlobalBarrier2*       pGlobalBarriers;
//     uint32_t                    mBufferBarrierCount;
//     const BufferBarrier2*       pBufferBarriers;
//     uint32_t                    mTextureBarrierCount;
//     const TextureBarrier2*      pTextureBarriers;
//     uint32_t                    mRenderTargetBarrierCount;
//     const RenderTargetBarrier2* pRenderTargetBarriers;
// };

void FORGE_CALLCONV getBufferSizeAlign(Renderer* pRenderer, const BufferDesc* pDesc, ResourceSizeAlign* pOut);
void FORGE_CALLCONV getTextureSizeAlign(Renderer* pRenderer, const TextureDesc* pDesc, ResourceSizeAlign* pOut);
void FORGE_CALLCONV addBuffer(Renderer* pRenderer, const BufferDesc* pDesc, Buffer** ppBuffer);
void FORGE_CALLCONV removeBuffer(Renderer* pRenderer, Buffer* pBuffer);
void FORGE_CALLCONV mapBuffer(Renderer* pRenderer, Buffer* pBuffer, ReadRange* pRange);
void FORGE_CALLCONV unmapBuffer(Renderer* pRenderer, Buffer* pBuffer);
void FORGE_CALLCONV cmdUpdateBuffer(Cmd* pCmd, Buffer* pBuffer, uint64_t dstOffset, Buffer* pSrcBuffer, uint64_t srcOffset, uint64_t size);
void FORGE_CALLCONV cmdCopyTexture(Cmd* pCmd, Texture* pDstTexture, Texture* pSrcTexture);
void FORGE_CALLCONV cmdUpdateSubresource(Cmd* pCmd, Texture* pTexture, Buffer* pSrcBuffer, const SubresourceDataDesc* pSubresourceDesc);
void FORGE_CALLCONV cmdCopySubresource(Cmd* pCmd, Buffer* pDstBuffer, Texture* pTexture, const SubresourceDataDesc* pSubresourceDesc);
// void FORGE_CALLCONV cmdBarrier(Cmd* pCmd, const BarrierDesc* pDesc);
void FORGE_CALLCONV addTexture(Renderer* pRenderer, const TextureDesc* pDesc, Texture** ppTexture);
void FORGE_CALLCONV removeTexture(Renderer* pRenderer, Texture* pTexture);
