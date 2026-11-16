/* Copyright (c) 2026 Horizon */
#include "Graphics/RenderContext.h"
#include "Profiler/IProfiler.h"
#include "../../../RHI/Private/RendererResourceAPI.h"

#include <stdio.h>
#include <string.h>

#include <ThirdParty/stb/stb_ds.h>
#include <ThirdParty/tinyimageformat/tinyimageformat_query.h>

hz::CommandList::~CommandList()
{
    arrfree(bufferBarriers);
    arrfree(textureBarriers);
    arrfree(renderTargetBarriers);
}

void hz::CommandList::reset(Cmd* cmd, uint64_t profilerToken)
{
    pCmd = cmd;
    pCurrentRootSignature = nullptr;
    pCurrentDrawIndirectSignature = nullptr;
    gpuProfilerToken = profilerToken;
    memset(pVertexBuffers, 0, sizeof(pVertexBuffers));
    memset(vertexStrides, 0, sizeof(vertexStrides));
    memset(vertexOffsets, 0, sizeof(vertexOffsets));
    vertexBufferCount = 0;
}

void hz::CommandList::Clear()
{
    pCmd = nullptr;
    pCurrentRootSignature = nullptr;
    pCurrentDrawIndirectSignature = nullptr;
}

void hz::CommandList::barrier(uint32_t bufferCount, BufferBarrier* pBufferBarriers, uint32_t textureCount, TextureBarrier* pTextureBarriers,
                              uint32_t renderTargetCount, RenderTargetBarrier* pRenderTargetBarriers)
{
    if (bufferCount || textureCount || renderTargetCount)
        cmdResourceBarrier(pCmd, bufferCount, pBufferBarriers, textureCount, pTextureBarriers, renderTargetCount, pRenderTargetBarriers);
}

void hz::CommandList::addBufferBarriers(hz::Span<const hz::GPUBuffer*> buffers, bool storage)
{
    for (const hz::GPUBuffer* buffer : buffers)
    {
        if (!buffer || buffer->usage != MemoryUsageGpuOnly)
            continue;
        ResourceState state = ResourceStateUnorderedAccess;
        if (!storage)
        {
            state = ResourceStateShaderResource;
            if (buffer->Descriptors & (DescriptorTypeVertexBuffer | DescriptorTypeUniformBuffer))
                state |= ResourceStateVertexAndConstantBuffer;
            if (buffer->Descriptors & DescriptorTypeIndexBuffer)
                state |= ResourceStateIndexBuffer;
            if (buffer->Descriptors & DescriptorTypeIndirectBuffer)
                state |= ResourceStateIndirectArgument;
        }
        if (buffer->state != state || storage)
        {
            const BufferBarrier bufferBarrier = { .pBuffer = buffer->pBuffer, .currentState = buffer->state, .newState = state };
            arrpush(bufferBarriers, bufferBarrier);
            buffer->state = state;
        }
    }
}

void hz::CommandList::addIndirectBarriers(Span<const GPUBuffer*> buffers)
{
    for (const GPUBuffer* buffer : buffers)
    {
        if (!buffer || (buffer->state & ResourceStateIndirectArgument))
        {
            continue;
        }
        const BufferBarrier transition = { .pBuffer = buffer->pBuffer,
                                           .currentState = buffer->state,
                                           .newState = ResourceStateIndirectArgument };
        arrpush(bufferBarriers, transition);
        buffer->state = ResourceStateIndirectArgument;
    }
}

void hz::CommandList::barrier(const hz::Dependencies& dependencies, const hz::GPUBuffer* pIndirectBuffer)
{
    if (dependencies.pShared)
    {
        barrier(*dependencies.pShared, nullptr);
    }
    arrsetlen(bufferBarriers, 0);
    arrsetlen(textureBarriers, 0);
    arrsetlen(renderTargetBarriers, 0);

    const auto addTextureBarrier = [&](const hz::GPUTexture& texture, ResourceState state)
    {
        if (texture.state != state || state == ResourceStateUnorderedAccess)
        {
            if (texture.pRenderTarget)
            {
                const RenderTargetBarrier renderTargetBarrier = {
                    .pRenderTarget = texture.pRenderTarget,
                    .currentState = texture.state,
                    .newState = state,
                };
                arrpush(renderTargetBarriers, renderTargetBarrier);
            }
            else
            {
                const TextureBarrier textureBarrier = {
                    .pTexture = texture.pTexture,
                    .currentState = texture.state,
                    .newState = state,
                };
                arrpush(textureBarriers, textureBarrier);
            }
            texture.state = state;
        }
    };

    for (const hz::GPUTexture* texture : dependencies.sampledTextures)
    {
        if (texture)
        {
            addTextureBarrier(*texture, ResourceStateShaderResource);
        }
    }
    for (const hz::GPUTexture* texture : dependencies.storageTextures)
    {
        if (texture)
        {
            addTextureBarrier(*texture, ResourceStateUnorderedAccess);
        }
    }
    addBufferBarriers(dependencies.buffers, false);
    addBufferBarriers(dependencies.storageBuffers, true);
    addIndirectBarriers(dependencies.indirectBuffers);
    if (pIndirectBuffer && pIndirectBuffer->state != ResourceStateIndirectArgument)
    {
        const BufferBarrier indirectBarrier = {
            .pBuffer = pIndirectBuffer->pBuffer,
            .currentState = pIndirectBuffer->state,
            .newState = ResourceStateIndirectArgument,
        };
        arrpush(bufferBarriers, indirectBarrier);
        pIndirectBuffer->state = ResourceStateIndirectArgument;
    }

    barrier((uint32_t)arrlen(bufferBarriers), bufferBarriers, (uint32_t)arrlen(textureBarriers), textureBarriers,
            (uint32_t)arrlen(renderTargetBarriers), renderTargetBarriers);
}

void hz::CommandList::beginRendering(const hz::RenderPassDesc& desc, const hz::Dependencies& dependencies)
{
    ASSERT(desc.colorAttachments.Count <= MAX_RENDER_TARGETS && (desc.colorAttachments.Data || !desc.colorAttachments.Count));
    if (dependencies.pShared)
    {
        barrier(*dependencies.pShared, nullptr);
    }
    arrsetlen(bufferBarriers, 0);
    arrsetlen(textureBarriers, 0);
    arrsetlen(renderTargetBarriers, 0);

    const auto addTextureBarrier = [&](const hz::GPUTexture& texture, ResourceState state)
    {
        if (texture.state != state || state == ResourceStateUnorderedAccess)
        {
            if (texture.pRenderTarget)
            {
                const RenderTargetBarrier renderTargetBarrier = {
                    .pRenderTarget = texture.pRenderTarget,
                    .currentState = texture.state,
                    .newState = state,
                };
                arrpush(renderTargetBarriers, renderTargetBarrier);
            }
            else
            {
                const TextureBarrier textureBarrier = {
                    .pTexture = texture.pTexture,
                    .currentState = texture.state,
                    .newState = state,
                };
                arrpush(textureBarriers, textureBarrier);
            }
            texture.state = state;
        }
    };

    for (const hz::GPUTexture* texture : dependencies.sampledTextures)
    {
        if (texture)
        {
            addTextureBarrier(*texture, ResourceStateShaderResource);
        }
    }
    for (const hz::GPUTexture* texture : dependencies.storageTextures)
    {
        if (texture)
        {
            addTextureBarrier(*texture, ResourceStateUnorderedAccess);
        }
    }
    addBufferBarriers(dependencies.buffers, false);
    addBufferBarriers(dependencies.storageBuffers, true);
    addIndirectBarriers(dependencies.indirectBuffers);

    BindRenderTargetsDesc bind = { .renderTargetCount = desc.colorAttachments.Count };
    for (uint32_t i = 0; i < desc.colorAttachments.Count; ++i)
    {
        const hz::ColorAttachment& input = desc.colorAttachments.Data[i];
        addTextureBarrier(*input.pTexture, ResourceStateRenderTarget);
        bind.renderTargets[i] = {
            .pRenderTarget = input.pTexture->pRenderTarget,
            .loadAction = input.loadAction,
            .storeAction = input.storeAction,
            .ClearValue = input.ClearValue,
            .overrideClearValue = input.loadAction == LoadActionClear,
        };
    }

    if (desc.depthAttachment.pTexture)
    {
        const hz::DepthAttachment& input = desc.depthAttachment;
        addTextureBarrier(*input.pTexture, ResourceStateDepthWrite);
        bind.depthStencil = {
            .pDepthStencil = input.pTexture->pRenderTarget,
            .loadAction = input.loadAction,
            .loadActionStencil = TinyImageFormat_HasStencil((TinyImageFormat)input.pTexture->pRenderTarget->Format) ? input.loadAction
                                                                                                                    : LoadActionDontCare,
            .storeAction = input.storeAction,
            .ClearValue = input.ClearValue,
            .overrideClearValue = input.loadAction == LoadActionClear,
        };
    }

    barrier((uint32_t)arrlen(bufferBarriers), bufferBarriers, (uint32_t)arrlen(textureBarriers), textureBarriers,
            (uint32_t)arrlen(renderTargetBarriers), renderTargetBarriers);
    cmdBindRenderTargets(pCmd, &bind);
}
void hz::CommandList::endRendering() { cmdBindRenderTargets(pCmd, nullptr); }

void hz::CommandList::setPipeline(const hz::GPUPipeline& pipeline)
{
    pCurrentRootSignature = pipeline.pRootSignature;
    pCurrentDrawIndirectSignature = pipeline.pDrawIndirectSignature;
    cmdBindPipeline(pCmd, pipeline.pPipeline);
}

void hz::CommandList::setVertexBuffer(uint32_t bindingIndex, const hz::GPUBuffer& buffer, uint64_t offset, uint32_t stride)
{
    pVertexBuffers[bindingIndex] = buffer.pBuffer;
    vertexOffsets[bindingIndex] = offset;
    vertexStrides[bindingIndex] = stride;
    if (bindingIndex == vertexBufferCount)
    {
        ++vertexBufferCount;
    }
    cmdBindVertexBuffer(pCmd, vertexBufferCount, pVertexBuffers, vertexStrides, vertexOffsets);
}

void hz::CommandList::setIndexBuffer(const hz::GPUBuffer& buffer, uint64_t offset, IndexType Type)
{
    cmdBindIndexBuffer(pCmd, buffer.pBuffer, Type, offset);
}

void hz::CommandList::setViewport(float x, float y, float w, float h, float minD, float maxD)
{
    cmdSetViewport(pCmd, x, y, w, h, minD, maxD);
}
void hz::CommandList::setScissor(uint32_t x, uint32_t y, uint32_t w, uint32_t h) { cmdSetScissor(pCmd, x, y, w, h); }
void hz::CommandList::setRootConstant(Span<const uint32_t> values, uint32_t offsetIn32BitValues)
{
    ASSERT(pCurrentRootSignature && values.Data && values.Count);
    const uint32_t descriptorIndex = getDescriptorIndexFromName(pCurrentRootSignature, "RootConstant0");
    ASSERT(descriptorIndex != UINT32_MAX);
    cmdBindPushConstants(pCmd, pCurrentRootSignature, descriptorIndex, values.Data, values.Count, offsetIn32BitValues);
}

void hz::CommandList::draw(uint32_t Count, uint32_t first) { cmdDraw(pCmd, Count, first); }
void hz::CommandList::drawIndexed(uint32_t Count, uint32_t first, uint32_t vertex) { cmdDrawIndexed(pCmd, Count, first, vertex); }

void hz::CommandList::drawIndirect(const hz::GPUBuffer& buffer, uint64_t offset, uint32_t drawCount)
{
    if (!(buffer.state & ResourceStateIndirectArgument))
    {
        BufferBarrier indirectBarrier = {
            .pBuffer = buffer.pBuffer,
            .currentState = buffer.state,
            .newState = ResourceStateIndirectArgument,
        };
        barrier(1, &indirectBarrier, 0, nullptr, 0, nullptr);
        buffer.state = ResourceStateIndirectArgument;
    }
    cmdExecuteIndirect(pCmd, pCurrentDrawIndirectSignature ? pCurrentDrawIndirectSignature : pContext->pDrawIndirectSignature, drawCount,
                       buffer.pBuffer, offset, nullptr, 0);
}

void hz::CommandList::drawIndexedIndirect(const hz::GPUBuffer& buffer, uint64_t offset, uint32_t drawCount)
{
    if (!(buffer.state & ResourceStateIndirectArgument))
    {
        BufferBarrier indirectBarrier = {
            .pBuffer = buffer.pBuffer,
            .currentState = buffer.state,
            .newState = ResourceStateIndirectArgument,
        };
        barrier(1, &indirectBarrier, 0, nullptr, 0, nullptr);
        buffer.state = ResourceStateIndirectArgument;
    }
    cmdExecuteIndirect(pCmd, pContext->pDrawIndexedIndirectSignature, drawCount, buffer.pBuffer, offset, nullptr, 0);
}

void hz::CommandList::dispatch(uint32_t x, uint32_t y, uint32_t z, const hz::Dependencies& dependencies)
{
    barrier(dependencies, nullptr);
    cmdDispatch(pCmd, x, y, z);
}

void hz::CommandList::dispatchIndirect(const hz::GPUBuffer& buffer, uint64_t offset, const hz::Dependencies& dependencies)
{
    barrier(dependencies, &buffer);
    cmdExecuteIndirect(pCmd, pContext->pDispatchIndirectSignature, 1, buffer.pBuffer, offset, nullptr, 0);
}

Buffer* hz::CommandList::createUploadBuffer(uint64_t Size)
{
    const ::BufferDesc desc = {
        .Size = Size,
        .Name = "CommandList upload buffer",
        .MemoryUsage = MemoryUsageCpuToGpu,
        .Flags = BufferCreationFlagPersistentMapBit,
        .QueueType = QueueTypeGraphics,
        .StartState = ResourceStateGenericRead,
    };
    Buffer* buffer = nullptr;
    addBuffer(pContext->pRenderer, &desc, &buffer);
    arrpush(pContext->commandResources[commandIndex].transientBuffers, buffer);
    return buffer;
}

void hz::CommandList::copyBuffer(const hz::GPUBuffer& dst, uint64_t dstOffset, const hz::GPUBuffer& src, uint64_t srcOffset, uint64_t Size)
{
    const BufferCopyRegion region = { .srcOffset = srcOffset, .dstOffset = dstOffset, .Size = Size };
    copyBufferRegions(dst, src, { &region, 1 });
}

void hz::CommandList::copyBufferRegions(const hz::GPUBuffer& dst, const hz::GPUBuffer& src, Span<BufferCopyRegion> regions)
{
    if (!regions.Count)
    {
        return;
    }
    const ResourceState dstState = dst.state;
    const ResourceState srcState = src.state;
    arrsetlen(bufferBarriers, 0);
    if (dst.usage == MemoryUsageGpuOnly && dst.state != ResourceStateCopyDest)
    {
        const BufferBarrier dstBarrier = {
            .pBuffer = dst.pBuffer,
            .currentState = dst.state,
            .newState = ResourceStateCopyDest,
        };
        arrpush(bufferBarriers, dstBarrier);
        dst.state = ResourceStateCopyDest;
    }
    if (src.usage == MemoryUsageGpuOnly && src.state != ResourceStateCopySource)
    {
        const BufferBarrier srcBarrier = {
            .pBuffer = src.pBuffer,
            .currentState = src.state,
            .newState = ResourceStateCopySource,
        };
        arrpush(bufferBarriers, srcBarrier);
        src.state = ResourceStateCopySource;
    }
    barrier((uint32_t)arrlen(bufferBarriers), bufferBarriers, 0, nullptr, 0, nullptr);
    for (const BufferCopyRegion& region : regions)
    {
        cmdUpdateBuffer(pCmd, dst.pBuffer, region.dstOffset, src.pBuffer, region.srcOffset, region.Size);
    }

    arrsetlen(bufferBarriers, 0);
    if (dst.state != dstState)
    {
        const BufferBarrier dstBarrier = {
            .pBuffer = dst.pBuffer,
            .currentState = dst.state,
            .newState = dstState,
        };
        arrpush(bufferBarriers, dstBarrier);
        dst.state = dstState;
    }
    if (src.state != srcState)
    {
        const BufferBarrier srcBarrier = {
            .pBuffer = src.pBuffer,
            .currentState = src.state,
            .newState = srcState,
        };
        arrpush(bufferBarriers, srcBarrier);
        src.state = srcState;
    }
    barrier((uint32_t)arrlen(bufferBarriers), bufferBarriers, 0, nullptr, 0, nullptr);
}

void hz::CommandList::fillBuffer(const hz::GPUBuffer& dst, uint64_t dstOffset, uint32_t value, uint64_t Size)
{
    const auto fill = [&](void* Data)
    {
        uint8_t* bytes = (uint8_t*)Data;
        for (uint64_t offset = 0; offset < Size; offset += sizeof(value))
        {
            const size_t writeSize = (size_t)TF_MIN((uint64_t)sizeof(value), Size - offset);
            memcpy(bytes + offset, &value, writeSize);
        }
    };

    if (dst.usage == MemoryUsageCpuToGpu)
    {
        fill((uint8_t*)dst.pBuffer->pCpuMappedAddress + dstOffset);
    }
    else
    {
        Buffer* upload = createUploadBuffer(Size);
        fill(upload->pCpuMappedAddress);

        const ResourceState dstState = dst.state;
        BufferBarrier       dstBarrier = { .pBuffer = dst.pBuffer };
        if (dst.state != ResourceStateCopyDest)
        {
            dstBarrier.currentState = dst.state;
            dstBarrier.newState = ResourceStateCopyDest;
            barrier(1, &dstBarrier, 0, nullptr, 0, nullptr);
            dst.state = ResourceStateCopyDest;
        }
        cmdUpdateBuffer(pCmd, dst.pBuffer, dstOffset, upload, 0, Size);
        if (dst.state != dstState)
        {
            dstBarrier.currentState = dst.state;
            dstBarrier.newState = dstState;
            barrier(1, &dstBarrier, 0, nullptr, 0, nullptr);
            dst.state = dstState;
        }
    }
}

void hz::CommandList::updateBuffer(const hz::GPUBuffer& dst, uint64_t dstOffset, const void* Data, uint64_t Size)
{
    if (dst.usage == MemoryUsageCpuToGpu)
    {
        memcpy((uint8_t*)dst.pBuffer->pCpuMappedAddress + dstOffset, Data, (size_t)Size);
    }
    else
    {
        Buffer* upload = createUploadBuffer(Size);
        memcpy(upload->pCpuMappedAddress, Data, (size_t)Size);

        const ResourceState dstState = dst.state;
        BufferBarrier       dstBarrier = { .pBuffer = dst.pBuffer };
        if (dst.state != ResourceStateCopyDest)
        {
            dstBarrier.currentState = dst.state;
            dstBarrier.newState = ResourceStateCopyDest;
            barrier(1, &dstBarrier, 0, nullptr, 0, nullptr);
            dst.state = ResourceStateCopyDest;
        }
        cmdUpdateBuffer(pCmd, dst.pBuffer, dstOffset, upload, 0, Size);
        if (dst.state != dstState)
        {
            dstBarrier.currentState = dst.state;
            dstBarrier.newState = dstState;
            barrier(1, &dstBarrier, 0, nullptr, 0, nullptr);
            dst.state = dstState;
        }
    }
}

void hz::CommandList::copyTexture(const hz::GPUTexture& dst, const hz::GPUTexture& src)
{
    const ResourceState dstState = dst.state;
    const ResourceState srcState = src.state;
    arrsetlen(textureBarriers, 0);
    arrsetlen(renderTargetBarriers, 0);
    const auto transition = [&](const hz::GPUTexture& texture, ResourceState state)
    {
        if (texture.state != state && texture.pRenderTarget)
        {
            const RenderTargetBarrier textureBarrier = {
                .pRenderTarget = texture.pRenderTarget,
                .currentState = texture.state,
                .newState = state,
            };
            arrpush(renderTargetBarriers, textureBarrier);
        }
        else if (texture.state != state)
        {
            const TextureBarrier textureBarrier = {
                .pTexture = texture.pTexture,
                .currentState = texture.state,
                .newState = state,
            };
            arrpush(textureBarriers, textureBarrier);
        }
        texture.state = state;
    };

    transition(dst, ResourceStateCopyDest);
    transition(src, ResourceStateCopySource);
    barrier(0, nullptr, (uint32_t)arrlen(textureBarriers), textureBarriers, (uint32_t)arrlen(renderTargetBarriers), renderTargetBarriers);
    cmdCopyTexture(pCmd, dst.pTexture, src.pTexture);

    arrsetlen(textureBarriers, 0);
    arrsetlen(renderTargetBarriers, 0);
    transition(dst, dstState);
    transition(src, srcState);
    barrier(0, nullptr, (uint32_t)arrlen(textureBarriers), textureBarriers, (uint32_t)arrlen(renderTargetBarriers), renderTargetBarriers);
}

void hz::CommandList::pushDebugGroupLabel(const char* label, uint32_t colorRGBA) const
{
    const float r = (float)((colorRGBA >> 0) & 0xff) / 255.0f;
    const float g = (float)((colorRGBA >> 8) & 0xff) / 255.0f;
    const float b = (float)((colorRGBA >> 16) & 0xff) / 255.0f;
    cmdBeginDebugMarker(pCmd, r, g, b, label);
}

void hz::CommandList::insertDebugEventLabel(const char* label, uint32_t colorRGBA) const
{
    const float r = (float)((colorRGBA >> 0) & 0xff) / 255.0f;
    const float g = (float)((colorRGBA >> 8) & 0xff) / 255.0f;
    const float b = (float)((colorRGBA >> 16) & 0xff) / 255.0f;
    cmdAddDebugMarker(pCmd, r, g, b, label);
}

void hz::CommandList::popDebugGroupLabel() const { cmdEndDebugMarker(pCmd); }

void hz::CommandList::beginGpuFrameProfile()
{
    if (gpuProfilerToken != PROFILE_INVALID_TOKEN)
        cmdBeginGpuFrameProfile(pCmd, gpuProfilerToken);
}
void hz::CommandList::endGpuFrameProfile()
{
    if (gpuProfilerToken != PROFILE_INVALID_TOKEN)
        cmdEndGpuFrameProfile(pCmd, gpuProfilerToken);
}
void hz::CommandList::beginGpuTimestamp(const char* name)
{
    if (gpuProfilerToken != PROFILE_INVALID_TOKEN)
    {
        cmdBeginGpuTimestampQuery(pCmd, gpuProfilerToken, name);
    }
    else
    {
        cmdBeginDebugMarker(pCmd, 1, 1, 0, name);
    }
}
void hz::CommandList::endGpuTimestamp()
{
    if (gpuProfilerToken != PROFILE_INVALID_TOKEN)
    {
        cmdEndGpuTimestampQuery(pCmd, gpuProfilerToken);
    }
    else
    {
        cmdEndDebugMarker(pCmd);
    }
}
