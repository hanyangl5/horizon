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
#include "Core/ILog.h"
#include "RHI/IGraphics.h"
#include "RHI/IRay.h"
#include "RendererResourceAPI.h"

#include "GraphicsConfig.h"

/************************************************************************/
// Internal initialization settings
/************************************************************************/

bool               gRendererUnsupported = false;
const char*        pRendererUnsupportedReason = "";
PlatformParameters gPlatformParameters = { RENDERER_API_D3D12 };

/************************************************************************/
// Internal initialization functions
/************************************************************************/
extern void initD3D12Renderer(const char* appName, const RendererDesc* pSettings, Renderer** ppRenderer);
extern void initD3D12RaytracingFunctions();
extern void exitD3D12Renderer(Renderer* pRenderer);
extern void initD3D12RendererContext(const char* appName, const RendererContextDesc* pSettings, RendererContext** ppContext);
extern void exitD3D12RendererContext(RendererContext* pContext);
extern void d3d12_addFence(Renderer* pRenderer, Fence** ppFence);
extern void d3d12_removeFence(Renderer* pRenderer, Fence* pFence);
extern void d3d12_addSemaphore(Renderer* pRenderer, Semaphore** ppSemaphore);
extern void d3d12_removeSemaphore(Renderer* pRenderer, Semaphore* pSemaphore);
extern void d3d12_addQueue(Renderer* pRenderer, QueueDesc* pDesc, Queue** ppQueue);
extern void d3d12_removeQueue(Renderer* pRenderer, Queue* pQueue);
extern void d3d12_addSwapChain(Renderer* pRenderer, const SwapChainDesc* pDesc, SwapChain** ppSwapChain);
extern void d3d12_removeSwapChain(Renderer* pRenderer, SwapChain* pSwapChain);
extern void d3d12_addResourceHeap(Renderer* pRenderer, const ResourceHeapDesc* pDesc, ResourceHeap** ppHeap);
extern void d3d12_removeResourceHeap(Renderer* pRenderer, ResourceHeap* pHeap);
extern void d3d12_addCmdPool(Renderer* pRenderer, const CmdPoolDesc* pDesc, CmdPool** ppCmdPool);
extern void d3d12_removeCmdPool(Renderer* pRenderer, CmdPool* pCmdPool);
extern void d3d12_addCmd(Renderer* pRenderer, const CmdDesc* pDesc, Cmd** ppCmd);
extern void d3d12_removeCmd(Renderer* pRenderer, Cmd* pCmd);
extern void d3d12_addCmd_n(Renderer* pRenderer, const CmdDesc* pDesc, uint32_t cmdCount, Cmd*** pppCmds);
extern void d3d12_removeCmd_n(Renderer* pRenderer, uint32_t cmdCount, Cmd** ppCmds);
extern void d3d12_addRenderTarget(Renderer* pRenderer, const RenderTargetDesc* pDesc, RenderTarget** ppRenderTarget);
extern void d3d12_removeRenderTarget(Renderer* pRenderer, RenderTarget* pRenderTarget);
extern void d3d12_addSampler(Renderer* pRenderer, const SamplerDesc* pDesc, Sampler** ppSampler);
extern void d3d12_removeSampler(Renderer* pRenderer, Sampler* pSampler);
extern void d3d12_addShaderSource(Renderer* pRenderer, const ShaderSrcDesc* pDesc, Shader** ppShaderProgram);
extern void d3d12_addShaderBinary(Renderer* pRenderer, const BinaryShaderDesc* pDesc, Shader** ppShaderProgram);
extern void d3d12_removeShader(Renderer* pRenderer, Shader* pShaderProgram);
extern void d3d12_addRootSignature(Renderer* pRenderer, const RootSignatureDesc* pDesc, RootSignature** ppRootSignature);
extern void d3d12_removeRootSignature(Renderer* pRenderer, RootSignature* pRootSignature);
extern uint32_t d3d12_getDescriptorIndexFromName(const RootSignature* pRootSignature, const char* pName);
extern void d3d12_addPipeline(Renderer* pRenderer, const PipelineDesc* pPipelineSettings, Pipeline** ppPipeline);
extern void d3d12_removePipeline(Renderer* pRenderer, Pipeline* pPipeline);
extern void d3d12_addPipelineCache(Renderer* pRenderer, const PipelineCacheDesc* pDesc, PipelineCache** ppPipelineCache);
extern void d3d12_getPipelineCacheData(Renderer* pRenderer, PipelineCache* pPipelineCache, size_t* pSize, void* pData);
#if defined(SHADER_STATS_AVAILABLE)
extern void d3d12_addPipelineStats(Renderer* pRenderer, Pipeline* pPipeline, bool generateDisassembly, PipelineStats* pOutStats);
extern void d3d12_removePipelineStats(Renderer* pRenderer, PipelineStats* pStats);
#endif
extern void d3d12_removePipelineCache(Renderer* pRenderer, PipelineCache* pPipelineCache);
extern void d3d12_addDescriptorSet(Renderer* pRenderer, const DescriptorSetDesc* pDesc, DescriptorSet** ppDescriptorSet);
extern void d3d12_removeDescriptorSet(Renderer* pRenderer, DescriptorSet* pDescriptorSet);
extern void d3d12_updateDescriptorSet(Renderer* pRenderer, uint32_t index, DescriptorSet* pDescriptorSet, uint32_t count, const DescriptorData* pParams);
extern void d3d12_resetCmdPool(Renderer* pRenderer, CmdPool* pCmdPool);
extern void d3d12_beginCmd(Cmd* pCmd);
extern void d3d12_endCmd(Cmd* pCmd);
extern void d3d12_cmdBindRenderTargets(Cmd* pCmd, const BindRenderTargetsDesc* pDesc);
extern void d3d12_cmdSetSampleLocations(Cmd* pCmd, SampleCount samplesCount, uint32_t gridSizeX, uint32_t gridSizeY, SampleLocations* plocations);
extern void d3d12_cmdSetViewport(Cmd* pCmd, float x, float y, float width, float height, float minDepth, float maxDepth);
extern void d3d12_cmdSetScissor(Cmd* pCmd, uint32_t x, uint32_t y, uint32_t width, uint32_t height);
extern void d3d12_cmdSetStencilReferenceValue(Cmd* pCmd, uint32_t val);
extern void d3d12_cmdBindPipeline(Cmd* pCmd, Pipeline* pPipeline);
extern void d3d12_cmdBindDescriptorSet(Cmd* pCmd, uint32_t index, DescriptorSet* pDescriptorSet);
extern void d3d12_cmdBindPushConstants(Cmd* pCmd, RootSignature* pRootSignature, uint32_t paramIndex, const void* pConstants);
extern void d3d12_cmdBindDescriptorSetWithRootCbvs(Cmd* pCmd, uint32_t index, DescriptorSet* pDescriptorSet, uint32_t count, const DescriptorData* pParams);
extern void d3d12_cmdBindIndexBuffer(Cmd* pCmd, Buffer* pBuffer, uint32_t indexType, uint64_t offset);
extern void d3d12_cmdBindVertexBuffer(Cmd* pCmd, uint32_t bufferCount, Buffer** ppBuffers, const uint32_t* pStrides, const uint64_t* pOffsets);
extern void d3d12_cmdDraw(Cmd* pCmd, uint32_t vertexCount, uint32_t firstVertex);
extern void d3d12_cmdDrawInstanced(Cmd* pCmd, uint32_t vertexCount, uint32_t firstVertex, uint32_t instanceCount, uint32_t firstInstance);
extern void d3d12_cmdDrawIndexed(Cmd* pCmd, uint32_t indexCount, uint32_t firstIndex, uint32_t firstVertex);
extern void d3d12_cmdDrawIndexedInstanced(Cmd* pCmd, uint32_t indexCount, uint32_t firstIndex, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance);
extern void d3d12_cmdDispatch(Cmd* pCmd, uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ);
extern void d3d12_cmdResourceBarrier(Cmd* pCmd, uint32_t bufferBarrierCount, BufferBarrier* pBufferBarriers, uint32_t textureBarrierCount, TextureBarrier* pTextureBarriers, uint32_t rtBarrierCount, RenderTargetBarrier* pRtBarriers);
extern void d3d12_acquireNextImage(Renderer* pRenderer, SwapChain* pSwapChain, Semaphore* pSignalSemaphore, Fence* pFence, uint32_t* pImageIndex);
extern void d3d12_queueSubmit(Queue* pQueue, const QueueSubmitDesc* pDesc);
extern void d3d12_queuePresent(Queue* pQueue, const QueuePresentDesc* pDesc);
extern void d3d12_waitQueueIdle(Queue* pQueue);
extern void d3d12_getFenceStatus(Renderer* pRenderer, Fence* pFence, FenceStatus* pFenceStatus);
extern void d3d12_waitForFences(Renderer* pRenderer, uint32_t fenceCount, Fence** ppFences);
extern void d3d12_toggleVSync(Renderer* pRenderer, SwapChain** ppSwapchain);
extern TinyImageFormat d3d12_getSupportedSwapchainFormat(Renderer* pRenderer, const SwapChainDesc* pDesc, ColorSpace colorSpace);
extern uint32_t d3d12_getRecommendedSwapchainImageCount(Renderer* pRenderer, const WindowHandle* hwnd);
extern void d3d12_addIndirectCommandSignature(Renderer* pRenderer, const CommandSignatureDesc* pDesc, CommandSignature** ppCommandSignature);
extern void d3d12_removeIndirectCommandSignature(Renderer* pRenderer, CommandSignature* pCommandSignature);
extern void d3d12_cmdExecuteIndirect(Cmd* pCmd, CommandSignature* pCommandSignature, unsigned int maxCommandCount, Buffer* pIndirectBuffer, uint64_t bufferOffset, Buffer* pCounterBuffer, uint64_t counterBufferOffset);
extern void d3d12_getTimestampFrequency(Queue* pQueue, double* pFrequency);
extern void d3d12_addQueryPool(Renderer* pRenderer, const QueryPoolDesc* pDesc, QueryPool** ppQueryPool);
extern void d3d12_removeQueryPool(Renderer* pRenderer, QueryPool* pQueryPool);
extern void d3d12_cmdBeginQuery(Cmd* pCmd, QueryPool* pQueryPool, QueryDesc* pQuery);
extern void d3d12_cmdEndQuery(Cmd* pCmd, QueryPool* pQueryPool, QueryDesc* pQuery);
extern void d3d12_cmdResolveQuery(Cmd* pCmd, QueryPool* pQueryPool, uint32_t startQuery, uint32_t queryCount);
extern void d3d12_cmdResetQuery(Cmd* pCmd, QueryPool* pQueryPool, uint32_t startQuery, uint32_t queryCount);
extern void d3d12_getQueryData(Renderer* pRenderer, QueryPool* pQueryPool, uint32_t queryIndex, QueryData* pOutData);
extern void d3d12_calculateMemoryStats(Renderer* pRenderer, char** ppStats);
extern void d3d12_calculateMemoryUse(Renderer* pRenderer, uint64_t* usedBytes, uint64_t* totalAllocatedBytes);
extern void d3d12_freeMemoryStats(Renderer* pRenderer, char* pStats);
extern void d3d12_cmdBeginDebugMarker(Cmd* pCmd, float r, float g, float b, const char* pName);
extern void d3d12_cmdEndDebugMarker(Cmd* pCmd);
extern void d3d12_cmdAddDebugMarker(Cmd* pCmd, float r, float g, float b, const char* pName);
extern void d3d12_cmdWriteMarker(Cmd* pCmd, const MarkerDesc* pDesc);
extern void d3d12_setBufferName(Renderer* pRenderer, Buffer* pBuffer, const char* pName);
extern void d3d12_setTextureName(Renderer* pRenderer, Texture* pTexture, const char* pName);
extern void d3d12_setRenderTargetName(Renderer* pRenderer, RenderTarget* pRenderTarget, const char* pName);
extern void d3d12_setPipelineName(Renderer* pRenderer, Pipeline* pPipeline, const char* pName);
extern bool d3d12_initRaytracing(Renderer* pRenderer, Raytracing** ppRaytracing);
extern void d3d12_removeRaytracing(Renderer* pRenderer, Raytracing* pRaytracing);
extern void d3d12_addAccelerationStructure(Raytracing* pRaytracing, const AccelerationStructureDesc* pDesc, AccelerationStructure** ppAccelerationStructure);
extern void d3d12_removeAccelerationStructure(Raytracing* pRaytracing, AccelerationStructure* pAccelerationStructure);
extern void d3d12_removeAccelerationStructureScratch(Raytracing* pRaytracing, AccelerationStructure* pAccelerationStructure);
extern void d3d12_cmdBuildAccelerationStructure(Cmd* pCmd, Raytracing* pRaytracing, RaytracingBuildASDesc* pDesc);
extern void d3d12_getBufferSizeAlign(Renderer* pRenderer, const BufferDesc* pDesc, ResourceSizeAlign* pOut);
extern void d3d12_getTextureSizeAlign(Renderer* pRenderer, const TextureDesc* pDesc, ResourceSizeAlign* pOut);
extern void d3d12_addBuffer(Renderer* pRenderer, const BufferDesc* pDesc, Buffer** ppBuffer);
extern void d3d12_removeBuffer(Renderer* pRenderer, Buffer* pBuffer);
extern void d3d12_mapBuffer(Renderer* pRenderer, Buffer* pBuffer, ReadRange* pRange);
extern void d3d12_unmapBuffer(Renderer* pRenderer, Buffer* pBuffer);
extern void d3d12_cmdUpdateBuffer(Cmd* pCmd, Buffer* pBuffer, uint64_t dstOffset, Buffer* pSrcBuffer, uint64_t srcOffset, uint64_t size);
extern void d3d12_cmdUpdateSubresource(Cmd* pCmd, Texture* pTexture, Buffer* pSrcBuffer, const SubresourceDataDesc* pSubresourceDesc);
extern void d3d12_cmdCopySubresource(Cmd* pCmd, Buffer* pDstBuffer, Texture* pTexture, const SubresourceDataDesc* pSubresourceDesc);
extern void d3d12_addTexture(Renderer* pRenderer, const TextureDesc* pDesc, Texture** ppTexture);
extern void d3d12_removeTexture(Renderer* pRenderer, Texture* pTexture);

bool d3d12dll_init();

static bool apiIsUnsupported(const RendererApi api)
{
    return api == RENDERER_API_D3D12 && !d3d12dll_init();
}

void setRendererInitializationError(const char* reason)
{
    gRendererUnsupported = true;
    pRendererUnsupportedReason = reason;
}

bool hasRendererInitializationError(const char** outReason)
{
    *outReason = pRendererUnsupportedReason;
    return gRendererUnsupported;
}

FORGE_RENDERER_API void FORGE_CALLCONV addFence(Renderer* pRenderer, Fence** ppFence)
{
    d3d12_addFence(pRenderer, ppFence);
}

FORGE_RENDERER_API void FORGE_CALLCONV removeFence(Renderer* pRenderer, Fence* pFence)
{
    d3d12_removeFence(pRenderer, pFence);
}

FORGE_RENDERER_API void FORGE_CALLCONV addSemaphore(Renderer* pRenderer, Semaphore** ppSemaphore)
{
    d3d12_addSemaphore(pRenderer, ppSemaphore);
}

FORGE_RENDERER_API void FORGE_CALLCONV removeSemaphore(Renderer* pRenderer, Semaphore* pSemaphore)
{
    d3d12_removeSemaphore(pRenderer, pSemaphore);
}

FORGE_RENDERER_API void FORGE_CALLCONV addQueue(Renderer* pRenderer, QueueDesc* pQDesc, Queue** ppQueue)
{
    d3d12_addQueue(pRenderer, pQDesc, ppQueue);
}

FORGE_RENDERER_API void FORGE_CALLCONV removeQueue(Renderer* pRenderer, Queue* pQueue)
{
    d3d12_removeQueue(pRenderer, pQueue);
}

FORGE_RENDERER_API void FORGE_CALLCONV addSwapChain(Renderer* pRenderer, const SwapChainDesc* pDesc, SwapChain** ppSwapChain)
{
    d3d12_addSwapChain(pRenderer, pDesc, ppSwapChain);
}

FORGE_RENDERER_API void FORGE_CALLCONV removeSwapChain(Renderer* pRenderer, SwapChain* pSwapChain)
{
    d3d12_removeSwapChain(pRenderer, pSwapChain);
}

FORGE_RENDERER_API void FORGE_CALLCONV addResourceHeap(Renderer* pRenderer, const ResourceHeapDesc* pDesc, ResourceHeap** ppHeap)
{
    d3d12_addResourceHeap(pRenderer, pDesc, ppHeap);
}

FORGE_RENDERER_API void FORGE_CALLCONV removeResourceHeap(Renderer* pRenderer, ResourceHeap* pHeap)
{
    d3d12_removeResourceHeap(pRenderer, pHeap);
}

FORGE_RENDERER_API void FORGE_CALLCONV addCmdPool(Renderer* pRenderer, const CmdPoolDesc* pDesc, CmdPool** ppCmdPool)
{
    d3d12_addCmdPool(pRenderer, pDesc, ppCmdPool);
}

FORGE_RENDERER_API void FORGE_CALLCONV removeCmdPool(Renderer* pRenderer, CmdPool* pCmdPool)
{
    d3d12_removeCmdPool(pRenderer, pCmdPool);
}

FORGE_RENDERER_API void FORGE_CALLCONV addCmd(Renderer* pRenderer, const CmdDesc* pDesc, Cmd** ppCmd)
{
    d3d12_addCmd(pRenderer, pDesc, ppCmd);
}

FORGE_RENDERER_API void FORGE_CALLCONV removeCmd(Renderer* pRenderer, Cmd* pCmd)
{
    d3d12_removeCmd(pRenderer, pCmd);
}

FORGE_RENDERER_API void FORGE_CALLCONV addCmd_n(Renderer* pRenderer, const CmdDesc* pDesc, uint32_t cmdCount, Cmd*** pppCmds)
{
    d3d12_addCmd_n(pRenderer, pDesc, cmdCount, pppCmds);
}

FORGE_RENDERER_API void FORGE_CALLCONV removeCmd_n(Renderer* pRenderer, uint32_t cmdCount, Cmd** ppCmds)
{
    d3d12_removeCmd_n(pRenderer, cmdCount, ppCmds);
}

FORGE_RENDERER_API void FORGE_CALLCONV addRenderTarget(Renderer* pRenderer, const RenderTargetDesc* pDesc, RenderTarget** ppRenderTarget)
{
    d3d12_addRenderTarget(pRenderer, pDesc, ppRenderTarget);
}

FORGE_RENDERER_API void FORGE_CALLCONV removeRenderTarget(Renderer* pRenderer, RenderTarget* pRenderTarget)
{
    d3d12_removeRenderTarget(pRenderer, pRenderTarget);
}

FORGE_RENDERER_API void FORGE_CALLCONV addSampler(Renderer* pRenderer, const SamplerDesc* pDesc, Sampler** ppSampler)
{
    d3d12_addSampler(pRenderer, pDesc, ppSampler);
}

FORGE_RENDERER_API void FORGE_CALLCONV removeSampler(Renderer* pRenderer, Sampler* pSampler)
{
    d3d12_removeSampler(pRenderer, pSampler);
}

FORGE_RENDERER_API void FORGE_CALLCONV addShaderSource(Renderer* pRenderer, const ShaderSrcDesc* pDesc, Shader** ppShaderProgram)
{
    d3d12_addShaderSource(pRenderer, pDesc, ppShaderProgram);
}

FORGE_RENDERER_API void FORGE_CALLCONV addShaderBinary(Renderer* pRenderer, const BinaryShaderDesc* pDesc, Shader** ppShaderProgram)
{
    d3d12_addShaderBinary(pRenderer, pDesc, ppShaderProgram);
}

FORGE_RENDERER_API void FORGE_CALLCONV removeShader(Renderer* pRenderer, Shader* pShaderProgram)
{
    d3d12_removeShader(pRenderer, pShaderProgram);
}

FORGE_RENDERER_API void FORGE_CALLCONV addRootSignature(Renderer* pRenderer, const RootSignatureDesc* pDesc, RootSignature** ppRootSignature)
{
    d3d12_addRootSignature(pRenderer, pDesc, ppRootSignature);
}

FORGE_RENDERER_API void FORGE_CALLCONV removeRootSignature(Renderer* pRenderer, RootSignature* pRootSignature)
{
    d3d12_removeRootSignature(pRenderer, pRootSignature);
}

FORGE_RENDERER_API uint32_t FORGE_CALLCONV getDescriptorIndexFromName(const RootSignature* pRootSignature, const char* pName)
{
    return d3d12_getDescriptorIndexFromName(pRootSignature, pName);
}

FORGE_RENDERER_API void FORGE_CALLCONV addPipeline(Renderer* pRenderer, const PipelineDesc* pPipelineSettings, Pipeline** ppPipeline)
{
    d3d12_addPipeline(pRenderer, pPipelineSettings, ppPipeline);
}

FORGE_RENDERER_API void FORGE_CALLCONV removePipeline(Renderer* pRenderer, Pipeline* pPipeline)
{
    d3d12_removePipeline(pRenderer, pPipeline);
}

FORGE_RENDERER_API void FORGE_CALLCONV addPipelineCache(Renderer* pRenderer, const PipelineCacheDesc* pDesc, PipelineCache** ppPipelineCache)
{
    d3d12_addPipelineCache(pRenderer, pDesc, ppPipelineCache);
}

FORGE_RENDERER_API void FORGE_CALLCONV getPipelineCacheData(Renderer* pRenderer, PipelineCache* pPipelineCache, size_t* pSize, void* pData)
{
    d3d12_getPipelineCacheData(pRenderer, pPipelineCache, pSize, pData);
}

#if defined(SHADER_STATS_AVAILABLE)
FORGE_RENDERER_API void FORGE_CALLCONV addPipelineStats(Renderer* pRenderer, Pipeline* pPipeline, bool generateDisassembly, PipelineStats* pOutStats)
{
    d3d12_addPipelineStats(pRenderer, pPipeline, generateDisassembly, pOutStats);
}

FORGE_RENDERER_API void FORGE_CALLCONV removePipelineStats(Renderer* pRenderer, PipelineStats* pStats)
{
    d3d12_removePipelineStats(pRenderer, pStats);
}
#endif

FORGE_RENDERER_API void FORGE_CALLCONV removePipelineCache(Renderer* pRenderer, PipelineCache* pPipelineCache)
{
    d3d12_removePipelineCache(pRenderer, pPipelineCache);
}

FORGE_RENDERER_API void FORGE_CALLCONV addDescriptorSet(Renderer* pRenderer, const DescriptorSetDesc* pDesc, DescriptorSet** ppDescriptorSet)
{
    d3d12_addDescriptorSet(pRenderer, pDesc, ppDescriptorSet);
}

FORGE_RENDERER_API void FORGE_CALLCONV removeDescriptorSet(Renderer* pRenderer, DescriptorSet* pDescriptorSet)
{
    d3d12_removeDescriptorSet(pRenderer, pDescriptorSet);
}

FORGE_RENDERER_API void FORGE_CALLCONV updateDescriptorSet(Renderer* pRenderer, uint32_t index, DescriptorSet* pDescriptorSet, uint32_t count, const DescriptorData* pParams)
{
    d3d12_updateDescriptorSet(pRenderer, index, pDescriptorSet, count, pParams);
}

FORGE_RENDERER_API void FORGE_CALLCONV resetCmdPool(Renderer* pRenderer, CmdPool* pCmdPool)
{
    d3d12_resetCmdPool(pRenderer, pCmdPool);
}

FORGE_RENDERER_API void FORGE_CALLCONV beginCmd(Cmd* pCmd)
{
    d3d12_beginCmd(pCmd);
}

FORGE_RENDERER_API void FORGE_CALLCONV endCmd(Cmd* pCmd)
{
    d3d12_endCmd(pCmd);
}

FORGE_RENDERER_API void FORGE_CALLCONV cmdBindRenderTargets(Cmd* pCmd, const BindRenderTargetsDesc* pDesc)
{
    d3d12_cmdBindRenderTargets(pCmd, pDesc);
}

FORGE_RENDERER_API void FORGE_CALLCONV cmdSetSampleLocations(Cmd* pCmd, SampleCount samplesCount, uint32_t gridSizeX, uint32_t gridSizeY, SampleLocations* plocations)
{
    d3d12_cmdSetSampleLocations(pCmd, samplesCount, gridSizeX, gridSizeY, plocations);
}

FORGE_RENDERER_API void FORGE_CALLCONV cmdSetViewport(Cmd* pCmd, float x, float y, float width, float height, float minDepth, float maxDepth)
{
    d3d12_cmdSetViewport(pCmd, x, y, width, height, minDepth, maxDepth);
}

FORGE_RENDERER_API void FORGE_CALLCONV cmdSetScissor(Cmd* pCmd, uint32_t x, uint32_t y, uint32_t width, uint32_t height)
{
    d3d12_cmdSetScissor(pCmd, x, y, width, height);
}

FORGE_RENDERER_API void FORGE_CALLCONV cmdSetStencilReferenceValue(Cmd* pCmd, uint32_t val)
{
    d3d12_cmdSetStencilReferenceValue(pCmd, val);
}

FORGE_RENDERER_API void FORGE_CALLCONV cmdBindPipeline(Cmd* pCmd, Pipeline* pPipeline)
{
    d3d12_cmdBindPipeline(pCmd, pPipeline);
}

FORGE_RENDERER_API void FORGE_CALLCONV cmdBindDescriptorSet(Cmd* pCmd, uint32_t index, DescriptorSet* pDescriptorSet)
{
    d3d12_cmdBindDescriptorSet(pCmd, index, pDescriptorSet);
}

FORGE_RENDERER_API void FORGE_CALLCONV cmdBindPushConstants(Cmd* pCmd, RootSignature* pRootSignature, uint32_t paramIndex, const void* pConstants)
{
    d3d12_cmdBindPushConstants(pCmd, pRootSignature, paramIndex, pConstants);
}

FORGE_RENDERER_API void FORGE_CALLCONV cmdBindDescriptorSetWithRootCbvs(Cmd* pCmd, uint32_t index, DescriptorSet* pDescriptorSet, uint32_t count, const DescriptorData* pParams)
{
    d3d12_cmdBindDescriptorSetWithRootCbvs(pCmd, index, pDescriptorSet, count, pParams);
}

FORGE_RENDERER_API void FORGE_CALLCONV cmdBindIndexBuffer(Cmd* pCmd, Buffer* pBuffer, uint32_t indexType, uint64_t offset)
{
    d3d12_cmdBindIndexBuffer(pCmd, pBuffer, indexType, offset);
}

FORGE_RENDERER_API void FORGE_CALLCONV cmdBindVertexBuffer(Cmd* pCmd, uint32_t bufferCount, Buffer** ppBuffers, const uint32_t* pStrides, const uint64_t* pOffsets)
{
    d3d12_cmdBindVertexBuffer(pCmd, bufferCount, ppBuffers, pStrides, pOffsets);
}

FORGE_RENDERER_API void FORGE_CALLCONV cmdDraw(Cmd* pCmd, uint32_t vertexCount, uint32_t firstVertex)
{
    d3d12_cmdDraw(pCmd, vertexCount, firstVertex);
}

FORGE_RENDERER_API void FORGE_CALLCONV cmdDrawInstanced(Cmd* pCmd, uint32_t vertexCount, uint32_t firstVertex, uint32_t instanceCount, uint32_t firstInstance)
{
    d3d12_cmdDrawInstanced(pCmd, vertexCount, firstVertex, instanceCount, firstInstance);
}

FORGE_RENDERER_API void FORGE_CALLCONV cmdDrawIndexed(Cmd* pCmd, uint32_t indexCount, uint32_t firstIndex, uint32_t firstVertex)
{
    d3d12_cmdDrawIndexed(pCmd, indexCount, firstIndex, firstVertex);
}

FORGE_RENDERER_API void FORGE_CALLCONV cmdDrawIndexedInstanced(Cmd* pCmd, uint32_t indexCount, uint32_t firstIndex, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance)
{
    d3d12_cmdDrawIndexedInstanced(pCmd, indexCount, firstIndex, instanceCount, firstVertex, firstInstance);
}

FORGE_RENDERER_API void FORGE_CALLCONV cmdDispatch(Cmd* pCmd, uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ)
{
    d3d12_cmdDispatch(pCmd, groupCountX, groupCountY, groupCountZ);
}

FORGE_RENDERER_API void FORGE_CALLCONV cmdResourceBarrier(Cmd* pCmd, uint32_t bufferBarrierCount, BufferBarrier* pBufferBarriers, uint32_t textureBarrierCount, TextureBarrier* pTextureBarriers, uint32_t rtBarrierCount, RenderTargetBarrier* pRtBarriers)
{
    d3d12_cmdResourceBarrier(pCmd, bufferBarrierCount, pBufferBarriers, textureBarrierCount, pTextureBarriers, rtBarrierCount, pRtBarriers);
}

FORGE_RENDERER_API void FORGE_CALLCONV acquireNextImage(Renderer* pRenderer, SwapChain* pSwapChain, Semaphore* pSignalSemaphore, Fence* pFence, uint32_t* pImageIndex)
{
    d3d12_acquireNextImage(pRenderer, pSwapChain, pSignalSemaphore, pFence, pImageIndex);
}

FORGE_RENDERER_API void FORGE_CALLCONV queueSubmit(Queue* pQueue, const QueueSubmitDesc* pDesc)
{
    d3d12_queueSubmit(pQueue, pDesc);
}

FORGE_RENDERER_API void FORGE_CALLCONV queuePresent(Queue* pQueue, const QueuePresentDesc* pDesc)
{
    d3d12_queuePresent(pQueue, pDesc);
}

FORGE_RENDERER_API void FORGE_CALLCONV waitQueueIdle(Queue* pQueue)
{
    d3d12_waitQueueIdle(pQueue);
}

FORGE_RENDERER_API void FORGE_CALLCONV getFenceStatus(Renderer* pRenderer, Fence* pFence, FenceStatus* pFenceStatus)
{
    d3d12_getFenceStatus(pRenderer, pFence, pFenceStatus);
}

FORGE_RENDERER_API void FORGE_CALLCONV waitForFences(Renderer* pRenderer, uint32_t fenceCount, Fence** ppFences)
{
    d3d12_waitForFences(pRenderer, fenceCount, ppFences);
}

FORGE_RENDERER_API void FORGE_CALLCONV toggleVSync(Renderer* pRenderer, SwapChain** ppSwapchain)
{
    d3d12_toggleVSync(pRenderer, ppSwapchain);
}

FORGE_RENDERER_API TinyImageFormat FORGE_CALLCONV getSupportedSwapchainFormat(Renderer* pRenderer, const SwapChainDesc* pDesc, ColorSpace colorSpace)
{
    return d3d12_getSupportedSwapchainFormat(pRenderer, pDesc, colorSpace);
}

FORGE_RENDERER_API uint32_t FORGE_CALLCONV getRecommendedSwapchainImageCount(Renderer* pRenderer, const WindowHandle* hwnd)
{
    return d3d12_getRecommendedSwapchainImageCount(pRenderer, hwnd);
}

FORGE_RENDERER_API void FORGE_CALLCONV addIndirectCommandSignature(Renderer* pRenderer, const CommandSignatureDesc* pDesc, CommandSignature** ppCommandSignature)
{
    d3d12_addIndirectCommandSignature(pRenderer, pDesc, ppCommandSignature);
}

FORGE_RENDERER_API void FORGE_CALLCONV removeIndirectCommandSignature(Renderer* pRenderer, CommandSignature* pCommandSignature)
{
    d3d12_removeIndirectCommandSignature(pRenderer, pCommandSignature);
}

FORGE_RENDERER_API void FORGE_CALLCONV cmdExecuteIndirect(Cmd* pCmd, CommandSignature* pCommandSignature, unsigned int maxCommandCount, Buffer* pIndirectBuffer, uint64_t bufferOffset, Buffer* pCounterBuffer, uint64_t counterBufferOffset)
{
    d3d12_cmdExecuteIndirect(pCmd, pCommandSignature, maxCommandCount, pIndirectBuffer, bufferOffset, pCounterBuffer, counterBufferOffset);
}

FORGE_RENDERER_API void FORGE_CALLCONV getTimestampFrequency(Queue* pQueue, double* pFrequency)
{
    d3d12_getTimestampFrequency(pQueue, pFrequency);
}

FORGE_RENDERER_API void FORGE_CALLCONV addQueryPool(Renderer* pRenderer, const QueryPoolDesc* pDesc, QueryPool** ppQueryPool)
{
    d3d12_addQueryPool(pRenderer, pDesc, ppQueryPool);
}

FORGE_RENDERER_API void FORGE_CALLCONV removeQueryPool(Renderer* pRenderer, QueryPool* pQueryPool)
{
    d3d12_removeQueryPool(pRenderer, pQueryPool);
}

FORGE_RENDERER_API void FORGE_CALLCONV cmdBeginQuery(Cmd* pCmd, QueryPool* pQueryPool, QueryDesc* pQuery)
{
    d3d12_cmdBeginQuery(pCmd, pQueryPool, pQuery);
}

FORGE_RENDERER_API void FORGE_CALLCONV cmdEndQuery(Cmd* pCmd, QueryPool* pQueryPool, QueryDesc* pQuery)
{
    d3d12_cmdEndQuery(pCmd, pQueryPool, pQuery);
}

FORGE_RENDERER_API void FORGE_CALLCONV cmdResolveQuery(Cmd* pCmd, QueryPool* pQueryPool, uint32_t startQuery, uint32_t queryCount)
{
    d3d12_cmdResolveQuery(pCmd, pQueryPool, startQuery, queryCount);
}

FORGE_RENDERER_API void FORGE_CALLCONV cmdResetQuery(Cmd* pCmd, QueryPool* pQueryPool, uint32_t startQuery, uint32_t queryCount)
{
    d3d12_cmdResetQuery(pCmd, pQueryPool, startQuery, queryCount);
}

FORGE_RENDERER_API void FORGE_CALLCONV getQueryData(Renderer* pRenderer, QueryPool* pQueryPool, uint32_t queryIndex, QueryData* pOutData)
{
    d3d12_getQueryData(pRenderer, pQueryPool, queryIndex, pOutData);
}

FORGE_RENDERER_API void FORGE_CALLCONV calculateMemoryStats(Renderer* pRenderer, char** ppStats)
{
    d3d12_calculateMemoryStats(pRenderer, ppStats);
}

FORGE_RENDERER_API void FORGE_CALLCONV calculateMemoryUse(Renderer* pRenderer, uint64_t* usedBytes, uint64_t* totalAllocatedBytes)
{
    d3d12_calculateMemoryUse(pRenderer, usedBytes, totalAllocatedBytes);
}

FORGE_RENDERER_API void FORGE_CALLCONV freeMemoryStats(Renderer* pRenderer, char* pStats)
{
    d3d12_freeMemoryStats(pRenderer, pStats);
}

FORGE_RENDERER_API void FORGE_CALLCONV cmdBeginDebugMarker(Cmd* pCmd, float r, float g, float b, const char* pName)
{
    d3d12_cmdBeginDebugMarker(pCmd, r, g, b, pName);
}

FORGE_RENDERER_API void FORGE_CALLCONV cmdEndDebugMarker(Cmd* pCmd)
{
    d3d12_cmdEndDebugMarker(pCmd);
}

FORGE_RENDERER_API void FORGE_CALLCONV cmdAddDebugMarker(Cmd* pCmd, float r, float g, float b, const char* pName)
{
    d3d12_cmdAddDebugMarker(pCmd, r, g, b, pName);
}

FORGE_RENDERER_API void FORGE_CALLCONV cmdWriteMarker(Cmd* pCmd, const MarkerDesc* pDesc)
{
    d3d12_cmdWriteMarker(pCmd, pDesc);
}

FORGE_RENDERER_API void FORGE_CALLCONV setBufferName(Renderer* pRenderer, Buffer* pBuffer, const char* pName)
{
    d3d12_setBufferName(pRenderer, pBuffer, pName);
}

FORGE_RENDERER_API void FORGE_CALLCONV setTextureName(Renderer* pRenderer, Texture* pTexture, const char* pName)
{
    d3d12_setTextureName(pRenderer, pTexture, pName);
}

FORGE_RENDERER_API void FORGE_CALLCONV setRenderTargetName(Renderer* pRenderer, RenderTarget* pRenderTarget, const char* pName)
{
    d3d12_setRenderTargetName(pRenderer, pRenderTarget, pName);
}

FORGE_RENDERER_API void FORGE_CALLCONV setPipelineName(Renderer* pRenderer, Pipeline* pPipeline, const char* pName)
{
    d3d12_setPipelineName(pRenderer, pPipeline, pName);
}

FORGE_RENDERER_API bool FORGE_CALLCONV initRaytracing(Renderer* pRenderer, Raytracing** ppRaytracing)
{
    return d3d12_initRaytracing(pRenderer, ppRaytracing);
}

FORGE_RENDERER_API void FORGE_CALLCONV removeRaytracing(Renderer* pRenderer, Raytracing* pRaytracing)
{
    d3d12_removeRaytracing(pRenderer, pRaytracing);
}

FORGE_RENDERER_API void FORGE_CALLCONV addAccelerationStructure(Raytracing* pRaytracing, const AccelerationStructureDesc* pDesc, AccelerationStructure** ppAccelerationStructure)
{
    d3d12_addAccelerationStructure(pRaytracing, pDesc, ppAccelerationStructure);
}

FORGE_RENDERER_API void FORGE_CALLCONV removeAccelerationStructure(Raytracing* pRaytracing, AccelerationStructure* pAccelerationStructure)
{
    d3d12_removeAccelerationStructure(pRaytracing, pAccelerationStructure);
}

FORGE_RENDERER_API void FORGE_CALLCONV removeAccelerationStructureScratch(Raytracing* pRaytracing, AccelerationStructure* pAccelerationStructure)
{
    d3d12_removeAccelerationStructureScratch(pRaytracing, pAccelerationStructure);
}

FORGE_RENDERER_API void FORGE_CALLCONV cmdBuildAccelerationStructure(Cmd* pCmd, Raytracing* pRaytracing, RaytracingBuildASDesc* pDesc)
{
    d3d12_cmdBuildAccelerationStructure(pCmd, pRaytracing, pDesc);
}

void FORGE_CALLCONV getBufferSizeAlign(Renderer* pRenderer, const BufferDesc* pDesc, ResourceSizeAlign* pOut)
{
    d3d12_getBufferSizeAlign(pRenderer, pDesc, pOut);
}

void FORGE_CALLCONV getTextureSizeAlign(Renderer* pRenderer, const TextureDesc* pDesc, ResourceSizeAlign* pOut)
{
    d3d12_getTextureSizeAlign(pRenderer, pDesc, pOut);
}

void FORGE_CALLCONV addBuffer(Renderer* pRenderer, const BufferDesc* pDesc, Buffer** ppBuffer)
{
    d3d12_addBuffer(pRenderer, pDesc, ppBuffer);
}

void FORGE_CALLCONV removeBuffer(Renderer* pRenderer, Buffer* pBuffer)
{
    d3d12_removeBuffer(pRenderer, pBuffer);
}

void FORGE_CALLCONV mapBuffer(Renderer* pRenderer, Buffer* pBuffer, ReadRange* pRange)
{
    d3d12_mapBuffer(pRenderer, pBuffer, pRange);
}

void FORGE_CALLCONV unmapBuffer(Renderer* pRenderer, Buffer* pBuffer)
{
    d3d12_unmapBuffer(pRenderer, pBuffer);
}

void FORGE_CALLCONV cmdUpdateBuffer(Cmd* pCmd, Buffer* pBuffer, uint64_t dstOffset, Buffer* pSrcBuffer, uint64_t srcOffset, uint64_t size)
{
    d3d12_cmdUpdateBuffer(pCmd, pBuffer, dstOffset, pSrcBuffer, srcOffset, size);
}

void FORGE_CALLCONV cmdUpdateSubresource(Cmd* pCmd, Texture* pTexture, Buffer* pSrcBuffer, const SubresourceDataDesc* pSubresourceDesc)
{
    d3d12_cmdUpdateSubresource(pCmd, pTexture, pSrcBuffer, pSubresourceDesc);
}

void FORGE_CALLCONV cmdCopySubresource(Cmd* pCmd, Buffer* pDstBuffer, Texture* pTexture, const SubresourceDataDesc* pSubresourceDesc)
{
    d3d12_cmdCopySubresource(pCmd, pDstBuffer, pTexture, pSubresourceDesc);
}

void FORGE_CALLCONV addTexture(Renderer* pRenderer, const TextureDesc* pDesc, Texture** ppTexture)
{
    d3d12_addTexture(pRenderer, pDesc, ppTexture);
}

void FORGE_CALLCONV removeTexture(Renderer* pRenderer, Texture* pTexture)
{
    d3d12_removeTexture(pRenderer, pTexture);
}

static void initRendererAPI(const char* appName, const RendererDesc* pSettings, Renderer** ppRenderer, const RendererApi api)
{
    switch (api)
    {
    case RENDERER_API_D3D12:
        initD3D12RaytracingFunctions();
        initD3D12Renderer(appName, pSettings, ppRenderer);
        break;
    default:
        LOGF(LogLevel::eERROR, "No Renderer API defined!");
        break;
    }
}

static void exitRendererAPI(Renderer* pRenderer, const RendererApi api)
{
    switch (api)
    {
    case RENDERER_API_D3D12:
        exitD3D12Renderer(pRenderer);
        break;
    default:
        LOGF(LogLevel::eERROR, "No Renderer API defined!");
        break;
    }
}

static void initRendererContextAPI(const char* appName, const RendererContextDesc* pSettings, RendererContext** ppContext,
                                   const RendererApi api)
{
    switch (api)
    {
    case RENDERER_API_D3D12:
        initD3D12RendererContext(appName, pSettings, ppContext);
        break;
    default:
        LOGF(LogLevel::eERROR, "No Renderer API defined!");
        break;
    }
}

static void exitRendererContextAPI(RendererContext* pContext, const RendererApi api)
{
    switch (api)
    {
    case RENDERER_API_D3D12:
        exitD3D12RendererContext(pContext);
        break;
    default:
        LOGF(LogLevel::eERROR, "No Renderer API defined!");
        break;
    }
}

void initRendererContext(const char* appName, const RendererContextDesc* pSettings, RendererContext** ppContext)
{
    ASSERT(ppContext);
    ASSERT(*ppContext == NULL);

    ASSERT(pSettings);

    // no need for extendedSettings for configuring gpu, applyExtendedSettings is not called in this function
    ExtendedSettings* extendedSettings = NULL;
    addGPUConfigurationRules(extendedSettings);

    // Init requested renderer API
    if (!apiIsUnsupported(gPlatformParameters.mSelectedRendererApi))
    {
        initRendererContextAPI(appName, pSettings, ppContext, gPlatformParameters.mSelectedRendererApi);
    }
    else
    {
        LOGF(LogLevel::eWARNING, "Requested Graphics API has been marked as disabled and/or not supported in the Renderer's descriptor!");
        LOGF(LogLevel::eWARNING, "Falling back to the first available API...");
    }

    removeGPUConfigurationRules();
}

void exitRendererContext(RendererContext* pContext)
{
    ASSERT(pContext);

    exitRendererContextAPI(pContext, gPlatformParameters.mSelectedRendererApi);
}

void setupPlatformParameters(Renderer* pRenderer)
{
    gPlatformParameters.mAvailableGpuCount = 0;
    gPlatformParameters.mSelectedGpuIndex = 0;

    // update available gpus and renderer api
    if (pRenderer != NULL)
    {
        uint32_t gpuCount = pRenderer->pContext->mGpuCount;
        ASSERT(gpuCount <= MAX_MULTIPLE_GPUS);
        gPlatformParameters.mSelectedRendererApi = pRenderer->mRendererApi;
        gPlatformParameters.mAvailableGpuCount = gpuCount;
        gPlatformParameters.mSelectedGpuIndex = (uint32_t)(pRenderer->pGpu - pRenderer->pContext->mGpus);
        for (uint32_t i = 0; i < gpuCount; ++i)
        {
            GPUSettings& gpuSettings = pRenderer->pContext->mGpus[i].mSettings;
            strncpy(gPlatformParameters.ppAvailableGpuNames[i], gpuSettings.mGpuVendorPreset.mGpuName, MAX_GPU_VENDOR_STRING_LENGTH);
            gPlatformParameters.pAvailableGpuIds[i] = gpuSettings.mGpuVendorPreset.mModelId;
        }
    }
}

void initRenderer(const char* appName, const RendererDesc* pSettings, Renderer** ppRenderer)
{
    ASSERT(ppRenderer);
    ASSERT(*ppRenderer == NULL);

    ASSERT(pSettings);

    addGPUConfigurationRules(pSettings->pExtendedSettings);

    // Init requested renderer API
    if (!apiIsUnsupported(gPlatformParameters.mSelectedRendererApi))
    {
        initRendererAPI(appName, pSettings, ppRenderer, gPlatformParameters.mSelectedRendererApi);
    }
    else
    {
        LOGF(LogLevel::eWARNING, "Requested Graphics API has been marked as disabled and/or not supported in the Renderer's descriptor!");
        LOGF(LogLevel::eWARNING, "Falling back to the first available API...");
    }

    // set available gpus and renderer api
    setupPlatformParameters(*ppRenderer);
    // configure the user's settings using the newly created device
    if (pSettings->pExtendedSettings && *ppRenderer)
    {
        setupExtendedSettings(pSettings->pExtendedSettings, &(*ppRenderer)->pGpu->mSettings);
    }

    removeGPUConfigurationRules();
}

void exitRenderer(Renderer* pRenderer)
{
    ASSERT(pRenderer);

    exitRendererAPI(pRenderer, pRenderer->mRendererApi);
    gPlatformParameters.mAvailableGpuCount = 0;
    gPlatformParameters.mSelectedGpuIndex = 0;
}
