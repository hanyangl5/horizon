/* Copyright (c) 2026 Horizon */
#include "Graphics/RenderContext.h"
#include "Profiler/IProfiler.h"
#include "../../../RHI/Private/RendererResourceAPI.h"

#include <stdio.h>
#include <string.h>

#include <ThirdParty/stb/stb_ds.h>

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
    gpuProfilerToken = profilerToken;
    memset(pVertexBuffers, 0, sizeof(pVertexBuffers));
    memset(vertexStrides, 0, sizeof(vertexStrides));
    memset(vertexOffsets, 0, sizeof(vertexOffsets));
    vertexBufferCount = 0;
}

void hz::CommandList::clear()
{
    pCmd = nullptr;
    pCurrentRootSignature = nullptr;
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
        if (buffer->usage != RESOURCE_MEMORY_USAGE_GPU_ONLY)
            continue;
        ResourceState state = RESOURCE_STATE_UNORDERED_ACCESS;
        if (!storage)
        {
            state = RESOURCE_STATE_SHADER_RESOURCE;
            if (buffer->descriptors & (DESCRIPTOR_TYPE_VERTEX_BUFFER | DESCRIPTOR_TYPE_UNIFORM_BUFFER))
                state |= RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
            if (buffer->descriptors & DESCRIPTOR_TYPE_INDEX_BUFFER)
                state |= RESOURCE_STATE_INDEX_BUFFER;
            if (buffer->descriptors & DESCRIPTOR_TYPE_INDIRECT_BUFFER)
                state |= RESOURCE_STATE_INDIRECT_ARGUMENT;
        }
        if (buffer->state != state || storage)
        {
            const BufferBarrier bufferBarrier = { .pBuffer = buffer->pBuffer, .currentState = buffer->state, .newState = state };
            arrpush(bufferBarriers, bufferBarrier);
            buffer->state = state;
        }
    }
}

void hz::CommandList::barrier(const hz::Dependencies& dependencies, const hz::GPUBuffer* pIndirectBuffer)
{
    arrsetlen(bufferBarriers, 0);
    arrsetlen(textureBarriers, 0);
    arrsetlen(renderTargetBarriers, 0);

    const auto addTextureBarrier = [&](const hz::GPUTexture& texture, ResourceState state)
    {
        if (texture.state != state || state == RESOURCE_STATE_UNORDERED_ACCESS)
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
        addTextureBarrier(*texture, RESOURCE_STATE_SHADER_RESOURCE);
    for (const hz::GPUTexture* texture : dependencies.storageTextures)
        addTextureBarrier(*texture, RESOURCE_STATE_UNORDERED_ACCESS);
    addBufferBarriers(dependencies.buffers, false);
    addBufferBarriers(dependencies.storageBuffers, true);
    if (pIndirectBuffer && pIndirectBuffer->state != RESOURCE_STATE_INDIRECT_ARGUMENT)
    {
        const BufferBarrier indirectBarrier = {
            .pBuffer = pIndirectBuffer->pBuffer,
            .currentState = pIndirectBuffer->state,
            .newState = RESOURCE_STATE_INDIRECT_ARGUMENT,
        };
        arrpush(bufferBarriers, indirectBarrier);
        pIndirectBuffer->state = RESOURCE_STATE_INDIRECT_ARGUMENT;
    }

    barrier((uint32_t)arrlen(bufferBarriers), bufferBarriers, (uint32_t)arrlen(textureBarriers), textureBarriers,
            (uint32_t)arrlen(renderTargetBarriers), renderTargetBarriers);
}

void hz::CommandList::beginRendering(const hz::RenderPassDesc& desc, const hz::Dependencies& dependencies)
{
    ASSERT(desc.colorAttachments.count <= MAX_RENDER_TARGETS && (desc.colorAttachments.pData || !desc.colorAttachments.count));
    arrsetlen(bufferBarriers, 0);
    arrsetlen(textureBarriers, 0);
    arrsetlen(renderTargetBarriers, 0);

    const auto addTextureBarrier = [&](const hz::GPUTexture& texture, ResourceState state)
    {
        if (texture.state != state || state == RESOURCE_STATE_UNORDERED_ACCESS)
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
        addTextureBarrier(*texture, RESOURCE_STATE_SHADER_RESOURCE);
    for (const hz::GPUTexture* texture : dependencies.storageTextures)
        addTextureBarrier(*texture, RESOURCE_STATE_UNORDERED_ACCESS);
    addBufferBarriers(dependencies.buffers, false);
    addBufferBarriers(dependencies.storageBuffers, true);

    BindRenderTargetsDesc bind = { .renderTargetCount = desc.colorAttachments.count };
    for (uint32_t i = 0; i < desc.colorAttachments.count; ++i)
    {
        const hz::ColorAttachment& input = desc.colorAttachments.pData[i];
        addTextureBarrier(*input.pTexture, RESOURCE_STATE_RENDER_TARGET);
        bind.renderTargets[i] = {
            .pRenderTarget = input.pTexture->pRenderTarget,
            .loadAction = input.loadAction,
            .storeAction = input.storeAction,
            .clearValue = input.clearValue,
            .overrideClearValue = input.loadAction == LOAD_ACTION_CLEAR,
        };
    }

    if (desc.depthAttachment.pTexture)
    {
        const hz::DepthAttachment& input = desc.depthAttachment;
        addTextureBarrier(*input.pTexture, RESOURCE_STATE_DEPTH_WRITE);
        bind.depthStencil = {
            .pDepthStencil = input.pTexture->pRenderTarget,
            .loadAction = input.loadAction,
            .storeAction = input.storeAction,
            .clearValue = input.clearValue,
            .overrideClearValue = input.loadAction == LOAD_ACTION_CLEAR,
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
    cmdBindPipeline(pCmd, pipeline.pPipeline);
}

void hz::CommandList::setVertexBuffer(uint32_t slot, const hz::GPUBuffer& buffer, uint64_t offset, uint32_t stride)
{
    pVertexBuffers[slot] = buffer.pBuffer;
    vertexOffsets[slot] = offset;
    vertexStrides[slot] = stride;
    if (slot == vertexBufferCount)
        ++vertexBufferCount;
    cmdBindVertexBuffer(pCmd, vertexBufferCount, pVertexBuffers, vertexStrides, vertexOffsets);
}

void hz::CommandList::setIndexBuffer(const hz::GPUBuffer& buffer, uint64_t offset, IndexType type)
{
    cmdBindIndexBuffer(pCmd, buffer.pBuffer, type, offset);
}

void hz::CommandList::setViewport(float x, float y, float w, float h, float minD, float maxD)
{
    cmdSetViewport(pCmd, x, y, w, h, minD, maxD);
}
void hz::CommandList::setScissor(uint32_t x, uint32_t y, uint32_t w, uint32_t h) { cmdSetScissor(pCmd, x, y, w, h); }
void hz::CommandList::setPushConstants(uint32_t index, const void* data, uint32_t size)
{
    char descriptorName[32] = {};
    snprintf(descriptorName, sizeof(descriptorName), "RootConstant%u", index);
    const uint32_t descriptorIndex = getDescriptorIndexFromName(pCurrentRootSignature, descriptorName);

    uint8_t paddedData[256] = {};
    memcpy(paddedData, data, size);
    cmdBindPushConstants(pCmd, pCurrentRootSignature, descriptorIndex, paddedData);
}
void hz::CommandList::draw(uint32_t count, uint32_t first)
{
    cmdDraw(pCmd, count, first);
}
void hz::CommandList::drawIndexed(uint32_t count, uint32_t first, uint32_t vertex)
{
    cmdDrawIndexed(pCmd, count, first, vertex);
}

void hz::CommandList::drawIndirect(const hz::GPUBuffer& buffer, uint64_t offset, uint32_t drawCount)
{
    if (buffer.state != RESOURCE_STATE_INDIRECT_ARGUMENT)
    {
        BufferBarrier indirectBarrier = {
            .pBuffer = buffer.pBuffer,
            .currentState = buffer.state,
            .newState = RESOURCE_STATE_INDIRECT_ARGUMENT,
        };
        barrier(1, &indirectBarrier, 0, nullptr, 0, nullptr);
        buffer.state = RESOURCE_STATE_INDIRECT_ARGUMENT;
    }
    cmdExecuteIndirect(pCmd, pContext->pDrawIndirectSignature, drawCount, buffer.pBuffer, offset, nullptr, 0);
}

void hz::CommandList::drawIndexedIndirect(const hz::GPUBuffer& buffer, uint64_t offset, uint32_t drawCount)
{
    if (buffer.state != RESOURCE_STATE_INDIRECT_ARGUMENT)
    {
        BufferBarrier indirectBarrier = {
            .pBuffer = buffer.pBuffer,
            .currentState = buffer.state,
            .newState = RESOURCE_STATE_INDIRECT_ARGUMENT,
        };
        barrier(1, &indirectBarrier, 0, nullptr, 0, nullptr);
        buffer.state = RESOURCE_STATE_INDIRECT_ARGUMENT;
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

Buffer* hz::CommandList::createUploadBuffer(uint64_t size)
{
    const ::BufferDesc desc = {
        .size = size,
        .pName = "CommandList upload buffer",
        .memoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU,
        .flags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT,
        .queueType = QUEUE_TYPE_GRAPHICS,
        .startState = RESOURCE_STATE_GENERIC_READ,
    };
    Buffer* buffer = nullptr;
    addBuffer(pContext->pRenderer, &desc, &buffer);
    arrpush(pContext->commandSlots[slot].transientBuffers, buffer);
    return buffer;
}

void hz::CommandList::copyBuffer(const hz::GPUBuffer& dst, uint64_t dstOffset, const hz::GPUBuffer& src, uint64_t srcOffset, uint64_t size)
{
    const ResourceState dstState = dst.state;
    const ResourceState srcState = src.state;
    arrsetlen(bufferBarriers, 0);
    if (dst.usage == RESOURCE_MEMORY_USAGE_GPU_ONLY && dst.state != RESOURCE_STATE_COPY_DEST)
    {
        const BufferBarrier dstBarrier = {
            .pBuffer = dst.pBuffer,
            .currentState = dst.state,
            .newState = RESOURCE_STATE_COPY_DEST,
        };
        arrpush(bufferBarriers, dstBarrier);
        dst.state = RESOURCE_STATE_COPY_DEST;
    }
    if (src.usage == RESOURCE_MEMORY_USAGE_GPU_ONLY && src.state != RESOURCE_STATE_COPY_SOURCE)
    {
        const BufferBarrier srcBarrier = {
            .pBuffer = src.pBuffer,
            .currentState = src.state,
            .newState = RESOURCE_STATE_COPY_SOURCE,
        };
        arrpush(bufferBarriers, srcBarrier);
        src.state = RESOURCE_STATE_COPY_SOURCE;
    }
    barrier((uint32_t)arrlen(bufferBarriers), bufferBarriers, 0, nullptr, 0, nullptr);
    cmdUpdateBuffer(pCmd, dst.pBuffer, dstOffset, src.pBuffer, srcOffset, size);

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

void hz::CommandList::fillBuffer(const hz::GPUBuffer& dst, uint64_t dstOffset, uint32_t value, uint64_t size)
{
    const auto fill = [&](void* pData)
    {
        uint8_t* bytes = (uint8_t*)pData;
        for (uint64_t offset = 0; offset < size; offset += sizeof(value))
        {
            const size_t writeSize = (size_t)TF_MIN((uint64_t)sizeof(value), size - offset);
            memcpy(bytes + offset, &value, writeSize);
        }
    };

    if (dst.usage == RESOURCE_MEMORY_USAGE_CPU_TO_GPU)
    {
        fill((uint8_t*)dst.pBuffer->pCpuMappedAddress + dstOffset);
    }
    else
    {
        Buffer* upload = createUploadBuffer(size);
        fill(upload->pCpuMappedAddress);

        const ResourceState dstState = dst.state;
        BufferBarrier       dstBarrier = { .pBuffer = dst.pBuffer };
        if (dst.state != RESOURCE_STATE_COPY_DEST)
        {
            dstBarrier.currentState = dst.state;
            dstBarrier.newState = RESOURCE_STATE_COPY_DEST;
            barrier(1, &dstBarrier, 0, nullptr, 0, nullptr);
            dst.state = RESOURCE_STATE_COPY_DEST;
        }
        cmdUpdateBuffer(pCmd, dst.pBuffer, dstOffset, upload, 0, size);
        if (dst.state != dstState)
        {
            dstBarrier.currentState = dst.state;
            dstBarrier.newState = dstState;
            barrier(1, &dstBarrier, 0, nullptr, 0, nullptr);
            dst.state = dstState;
        }
    }
}

void hz::CommandList::updateBuffer(const hz::GPUBuffer& dst, uint64_t dstOffset, const void* pData, uint64_t size)
{
    if (dst.usage == RESOURCE_MEMORY_USAGE_CPU_TO_GPU)
    {
        memcpy((uint8_t*)dst.pBuffer->pCpuMappedAddress + dstOffset, pData, (size_t)size);
    }
    else
    {
        Buffer* upload = createUploadBuffer(size);
        memcpy(upload->pCpuMappedAddress, pData, (size_t)size);

        const ResourceState dstState = dst.state;
        BufferBarrier       dstBarrier = { .pBuffer = dst.pBuffer };
        if (dst.state != RESOURCE_STATE_COPY_DEST)
        {
            dstBarrier.currentState = dst.state;
            dstBarrier.newState = RESOURCE_STATE_COPY_DEST;
            barrier(1, &dstBarrier, 0, nullptr, 0, nullptr);
            dst.state = RESOURCE_STATE_COPY_DEST;
        }
        cmdUpdateBuffer(pCmd, dst.pBuffer, dstOffset, upload, 0, size);
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

    transition(dst, RESOURCE_STATE_COPY_DEST);
    transition(src, RESOURCE_STATE_COPY_SOURCE);
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
    gpuTimestampToken =
        gpuProfilerToken != PROFILE_INVALID_TOKEN ? cmdBeginGpuTimestampQuery(pCmd, gpuProfilerToken, name) : PROFILE_INVALID_TOKEN;
}
void hz::CommandList::endGpuTimestamp()
{
    if (gpuTimestampToken != PROFILE_INVALID_TOKEN)
        cmdEndGpuTimestampQuery(pCmd, gpuTimestampToken);
    gpuTimestampToken = 0;
}
