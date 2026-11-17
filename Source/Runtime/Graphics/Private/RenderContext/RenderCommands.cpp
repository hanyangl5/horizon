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

void hz::CommandList::Reset(Cmd* cmd, uint64_t profilerToken)
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

void hz::CommandList::Barrier(uint32_t bufferCount, BufferBarrier* pBufferBarriers, uint32_t textureCount, TextureBarrier* pTextureBarriers,
                              uint32_t renderTargetCount, RenderTargetBarrier* pRenderTargetBarriers)
{
    if (bufferCount || textureCount || renderTargetCount)
        cmdResourceBarrier(pCmd, bufferCount, pBufferBarriers, textureCount, pTextureBarriers, renderTargetCount, pRenderTargetBarriers);
}

void hz::CommandList::AddBufferBarriers(hz::Span<const hz::GPUBuffer*> buffers, bool storage)
{
    for (const hz::GPUBuffer* buffer : buffers)
    {
        if (!buffer || buffer->usage != RESOURCE_MEMORY_USAGE_GPU_ONLY)
        {
            continue;
        }
        ResourceState state = RESOURCE_STATE_UNORDERED_ACCESS;
        if (!storage)
        {
            state = RESOURCE_STATE_SHADER_RESOURCE;
            if (buffer->Descriptors & (DESCRIPTOR_TYPE_VERTEX_BUFFER | DESCRIPTOR_TYPE_UNIFORM_BUFFER))
            {
                state |= RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
            }
            if (buffer->Descriptors & DESCRIPTOR_TYPE_INDEX_BUFFER)
            {
                state |= RESOURCE_STATE_INDEX_BUFFER;
            }
            if (buffer->Descriptors & DESCRIPTOR_TYPE_INDIRECT_BUFFER)
            {
                state |= RESOURCE_STATE_INDIRECT_ARGUMENT;
            }
        }
        if (buffer->state != state || storage)
        {
            const BufferBarrier bufferBarrier = { .pBuffer = buffer->pBuffer, .currentState = buffer->state, .newState = state };
            arrpush(bufferBarriers, bufferBarrier);
            buffer->state = state;
        }
    }
}

void hz::CommandList::AddIndirectBarriers(Span<const GPUBuffer*> buffers)
{
    for (const GPUBuffer* buffer : buffers)
    {
        if (!buffer || (buffer->state & RESOURCE_STATE_INDIRECT_ARGUMENT))
        {
            continue;
        }
        const BufferBarrier transition = { .pBuffer = buffer->pBuffer,
                                           .currentState = buffer->state,
                                           .newState = RESOURCE_STATE_INDIRECT_ARGUMENT };
        arrpush(bufferBarriers, transition);
        buffer->state = RESOURCE_STATE_INDIRECT_ARGUMENT;
    }
}

void hz::CommandList::Barrier(const hz::Dependencies& dependencies, const hz::GPUBuffer* pIndirectBuffer)
{
    if (dependencies.pShared)
    {
        Barrier(*dependencies.pShared, nullptr);
    }
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
    {
        if (texture)
        {
            addTextureBarrier(*texture, RESOURCE_STATE_SHADER_RESOURCE);
        }
    }
    for (const hz::GPUTexture* texture : dependencies.storageTextures)
    {
        if (texture)
        {
            addTextureBarrier(*texture, RESOURCE_STATE_UNORDERED_ACCESS);
        }
    }
    AddBufferBarriers(dependencies.buffers, false);
    AddBufferBarriers(dependencies.storageBuffers, true);
    AddIndirectBarriers(dependencies.indirectBuffers);
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

    Barrier((uint32_t)arrlen(bufferBarriers), bufferBarriers, (uint32_t)arrlen(textureBarriers), textureBarriers,
            (uint32_t)arrlen(renderTargetBarriers), renderTargetBarriers);
}

void hz::CommandList::BeginRendering(const hz::RenderPassDesc& desc, const hz::Dependencies& dependencies)
{
    ASSERT(desc.colorAttachments.Count <= MAX_RENDER_TARGETS && (desc.colorAttachments.Data || !desc.colorAttachments.Count));
    if (dependencies.pShared)
    {
        Barrier(*dependencies.pShared, nullptr);
    }
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
    {
        if (texture)
        {
            addTextureBarrier(*texture, RESOURCE_STATE_SHADER_RESOURCE);
        }
    }
    for (const hz::GPUTexture* texture : dependencies.storageTextures)
    {
        if (texture)
        {
            addTextureBarrier(*texture, RESOURCE_STATE_UNORDERED_ACCESS);
        }
    }
    AddBufferBarriers(dependencies.buffers, false);
    AddBufferBarriers(dependencies.storageBuffers, true);
    AddIndirectBarriers(dependencies.indirectBuffers);

    BindRenderTargetsDesc bind = { .renderTargetCount = desc.colorAttachments.Count };
    for (uint32_t i = 0; i < desc.colorAttachments.Count; ++i)
    {
        const hz::ColorAttachment& input = desc.colorAttachments.Data[i];
        addTextureBarrier(*input.pTexture, RESOURCE_STATE_RENDER_TARGET);
        bind.renderTargets[i] = {
            .pRenderTarget = input.pTexture->pRenderTarget,
            .loadAction = input.loadAction,
            .storeAction = input.storeAction,
            .ClearValue = input.ClearValue,
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
            .loadActionStencil = TinyImageFormat_HasStencil((TinyImageFormat)input.pTexture->pRenderTarget->Format) ? input.loadAction
                                                                                                                    : LOAD_ACTION_DONTCARE,
            .storeAction = input.storeAction,
            .ClearValue = input.ClearValue,
            .overrideClearValue = input.loadAction == LOAD_ACTION_CLEAR,
        };
    }

    Barrier((uint32_t)arrlen(bufferBarriers), bufferBarriers, (uint32_t)arrlen(textureBarriers), textureBarriers,
            (uint32_t)arrlen(renderTargetBarriers), renderTargetBarriers);
    cmdBindRenderTargets(pCmd, &bind);
}
void hz::CommandList::EndRendering() { cmdBindRenderTargets(pCmd, nullptr); }

void hz::CommandList::SetPipeline(const hz::GPUPipeline& pipeline)
{
    pCurrentRootSignature = pipeline.pRootSignature;
    pCurrentDrawIndirectSignature = pipeline.pDrawIndirectSignature;
    cmdBindPipeline(pCmd, pipeline.pPipeline);
}

void hz::CommandList::SetVertexBuffer(uint32_t bindingIndex, const hz::GPUBuffer& buffer, uint64_t offset, uint32_t stride)
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

void hz::CommandList::SetIndexBuffer(const hz::GPUBuffer& buffer, uint64_t offset, IndexType Type)
{
    cmdBindIndexBuffer(pCmd, buffer.pBuffer, Type, offset);
}

void hz::CommandList::SetViewport(float x, float y, float w, float h, float minD, float maxD)
{
    cmdSetViewport(pCmd, x, y, w, h, minD, maxD);
}
void hz::CommandList::SetScissor(uint32_t x, uint32_t y, uint32_t w, uint32_t h) { cmdSetScissor(pCmd, x, y, w, h); }
void hz::CommandList::SetRootConstant(Span<const uint32_t> values, uint32_t offsetIn32BitValues)
{
    ASSERT(pCurrentRootSignature && values.Data && values.Count);
    const uint32_t descriptorIndex = getDescriptorIndexFromName(pCurrentRootSignature, "RootConstant0");
    ASSERT(descriptorIndex != UINT32_MAX);
    // C++ mirrors of the shader block are maintained by hand; catch a struct that outgrew the reflected block.
    ASSERTMSG(offsetIn32BitValues + values.Count <= pCurrentRootSignature->pDescriptors[descriptorIndex].Size,
              "Root constants [%u, %u) exceed the shader's RootConstant0 block of %u values", offsetIn32BitValues,
              offsetIn32BitValues + values.Count, pCurrentRootSignature->pDescriptors[descriptorIndex].Size);
    cmdBindPushConstants(pCmd, pCurrentRootSignature, descriptorIndex, values.Data, values.Count, offsetIn32BitValues);
}

void hz::CommandList::Draw(uint32_t Count, uint32_t first) { cmdDraw(pCmd, Count, first); }
void hz::CommandList::DrawIndexed(uint32_t Count, uint32_t first, uint32_t vertex) { cmdDrawIndexed(pCmd, Count, first, vertex); }

void hz::CommandList::DrawIndirect(const hz::GPUBuffer& buffer, uint64_t offset, uint32_t drawCount)
{
    if (!(buffer.state & RESOURCE_STATE_INDIRECT_ARGUMENT))
    {
        BufferBarrier indirectBarrier = {
            .pBuffer = buffer.pBuffer,
            .currentState = buffer.state,
            .newState = RESOURCE_STATE_INDIRECT_ARGUMENT,
        };
        Barrier(1, &indirectBarrier, 0, nullptr, 0, nullptr);
        buffer.state = RESOURCE_STATE_INDIRECT_ARGUMENT;
    }
    cmdExecuteIndirect(pCmd, pCurrentDrawIndirectSignature ? pCurrentDrawIndirectSignature : pContext->pDrawIndirectSignature, drawCount,
                       buffer.pBuffer, offset, nullptr, 0);
}

void hz::CommandList::DrawIndexedIndirect(const hz::GPUBuffer& buffer, uint64_t offset, uint32_t drawCount)
{
    if (!(buffer.state & RESOURCE_STATE_INDIRECT_ARGUMENT))
    {
        BufferBarrier indirectBarrier = {
            .pBuffer = buffer.pBuffer,
            .currentState = buffer.state,
            .newState = RESOURCE_STATE_INDIRECT_ARGUMENT,
        };
        Barrier(1, &indirectBarrier, 0, nullptr, 0, nullptr);
        buffer.state = RESOURCE_STATE_INDIRECT_ARGUMENT;
    }
    cmdExecuteIndirect(pCmd, pContext->pDrawIndexedIndirectSignature, drawCount, buffer.pBuffer, offset, nullptr, 0);
}

void hz::CommandList::Dispatch(uint32_t x, uint32_t y, uint32_t z, const hz::Dependencies& dependencies)
{
    Barrier(dependencies, nullptr);
    cmdDispatch(pCmd, x, y, z);
}

void hz::CommandList::DispatchIndirect(const hz::GPUBuffer& buffer, uint64_t offset, const hz::Dependencies& dependencies)
{
    Barrier(dependencies, &buffer);
    cmdExecuteIndirect(pCmd, pContext->pDispatchIndirectSignature, 1, buffer.pBuffer, offset, nullptr, 0);
}

Buffer* hz::CommandList::CreateUploadBuffer(uint64_t Size)
{
    const ::BufferDesc desc = {
        .Size = Size,
        .Name = "CommandList Upload buffer",
        .MemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU,
        .Flags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT,
        .QueueType = QUEUE_TYPE_GRAPHICS,
        .StartState = RESOURCE_STATE_GENERIC_READ,
    };
    Buffer* buffer = nullptr;
    addBuffer(pContext->pRenderer, &desc, &buffer);
    arrpush(pContext->commandResources[commandIndex].transientBuffers, buffer);
    return buffer;
}

void hz::CommandList::CopyBuffer(const hz::GPUBuffer& dst, uint64_t dstOffset, const hz::GPUBuffer& src, uint64_t srcOffset, uint64_t Size)
{
    const BufferCopyRegion region = { .srcOffset = srcOffset, .dstOffset = dstOffset, .Size = Size };
    CopyBufferRegions(dst, src, { &region, 1 });
}

void hz::CommandList::CopyBufferRegions(const hz::GPUBuffer& dst, const hz::GPUBuffer& src, Span<BufferCopyRegion> regions)
{
    if (!regions.Count)
    {
        return;
    }
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
    Barrier((uint32_t)arrlen(bufferBarriers), bufferBarriers, 0, nullptr, 0, nullptr);
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
    Barrier((uint32_t)arrlen(bufferBarriers), bufferBarriers, 0, nullptr, 0, nullptr);
}

void hz::CommandList::FillBuffer(const hz::GPUBuffer& dst, uint64_t dstOffset, uint32_t value, uint64_t Size)
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

    if (dst.usage == RESOURCE_MEMORY_USAGE_CPU_TO_GPU)
    {
        fill((uint8_t*)dst.pBuffer->pCpuMappedAddress + dstOffset);
    }
    else
    {
        Buffer* Upload = CreateUploadBuffer(Size);
        fill(Upload->pCpuMappedAddress);

        const ResourceState dstState = dst.state;
        BufferBarrier       dstBarrier = { .pBuffer = dst.pBuffer };
        if (dst.state != RESOURCE_STATE_COPY_DEST)
        {
            dstBarrier.currentState = dst.state;
            dstBarrier.newState = RESOURCE_STATE_COPY_DEST;
            Barrier(1, &dstBarrier, 0, nullptr, 0, nullptr);
            dst.state = RESOURCE_STATE_COPY_DEST;
        }
        cmdUpdateBuffer(pCmd, dst.pBuffer, dstOffset, Upload, 0, Size);
        if (dst.state != dstState)
        {
            dstBarrier.currentState = dst.state;
            dstBarrier.newState = dstState;
            Barrier(1, &dstBarrier, 0, nullptr, 0, nullptr);
            dst.state = dstState;
        }
    }
}

void hz::CommandList::UpdateBuffer(const hz::GPUBuffer& dst, uint64_t dstOffset, const void* Data, uint64_t Size)
{
    if (dst.usage == RESOURCE_MEMORY_USAGE_CPU_TO_GPU)
    {
        memcpy((uint8_t*)dst.pBuffer->pCpuMappedAddress + dstOffset, Data, (size_t)Size);
    }
    else
    {
        Buffer* Upload = CreateUploadBuffer(Size);
        memcpy(Upload->pCpuMappedAddress, Data, (size_t)Size);

        const ResourceState dstState = dst.state;
        BufferBarrier       dstBarrier = { .pBuffer = dst.pBuffer };
        if (dst.state != RESOURCE_STATE_COPY_DEST)
        {
            dstBarrier.currentState = dst.state;
            dstBarrier.newState = RESOURCE_STATE_COPY_DEST;
            Barrier(1, &dstBarrier, 0, nullptr, 0, nullptr);
            dst.state = RESOURCE_STATE_COPY_DEST;
        }
        cmdUpdateBuffer(pCmd, dst.pBuffer, dstOffset, Upload, 0, Size);
        if (dst.state != dstState)
        {
            dstBarrier.currentState = dst.state;
            dstBarrier.newState = dstState;
            Barrier(1, &dstBarrier, 0, nullptr, 0, nullptr);
            dst.state = dstState;
        }
    }
}

void hz::CommandList::CopyTexture(const hz::GPUTexture& dst, const hz::GPUTexture& src)
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
    Barrier(0, nullptr, (uint32_t)arrlen(textureBarriers), textureBarriers, (uint32_t)arrlen(renderTargetBarriers), renderTargetBarriers);
    cmdCopyTexture(pCmd, dst.pTexture, src.pTexture);

    arrsetlen(textureBarriers, 0);
    arrsetlen(renderTargetBarriers, 0);
    transition(dst, dstState);
    transition(src, srcState);
    Barrier(0, nullptr, (uint32_t)arrlen(textureBarriers), textureBarriers, (uint32_t)arrlen(renderTargetBarriers), renderTargetBarriers);
}

void hz::CommandList::PushDebugGroupLabel(const char* label, uint32_t colorRGBA) const
{
    const float r = (float)((colorRGBA >> 0) & 0xff) / 255.0f;
    const float g = (float)((colorRGBA >> 8) & 0xff) / 255.0f;
    const float b = (float)((colorRGBA >> 16) & 0xff) / 255.0f;
    cmdBeginDebugMarker(pCmd, r, g, b, label);
}

void hz::CommandList::InsertDebugEventLabel(const char* label, uint32_t colorRGBA) const
{
    const float r = (float)((colorRGBA >> 0) & 0xff) / 255.0f;
    const float g = (float)((colorRGBA >> 8) & 0xff) / 255.0f;
    const float b = (float)((colorRGBA >> 16) & 0xff) / 255.0f;
    cmdAddDebugMarker(pCmd, r, g, b, label);
}

void hz::CommandList::PopDebugGroupLabel() const { cmdEndDebugMarker(pCmd); }

void hz::CommandList::BeginGpuFrameProfile()
{
    if (gpuProfilerToken != PROFILE_INVALID_TOKEN)
        cmdBeginGpuFrameProfile(pCmd, gpuProfilerToken);
}
void hz::CommandList::EndGpuFrameProfile()
{
    if (gpuProfilerToken != PROFILE_INVALID_TOKEN)
        cmdEndGpuFrameProfile(pCmd, gpuProfilerToken);
}
void hz::CommandList::BeginGpuTimestamp(const char* name)
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
void hz::CommandList::EndGpuTimestamp()
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
