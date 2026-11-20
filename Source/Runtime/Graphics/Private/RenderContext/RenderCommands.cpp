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

void hz::CommandList::Reset(Cmd* CmdValue, uint64_t ProfilerToken)
{
    pCmd = CmdValue;
    pCurrentRootSignature = nullptr;
    pCurrentDrawIndirectSignature = nullptr;
    gpuProfilerToken = ProfilerToken;
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

void hz::CommandList::Barrier(uint32_t BufferCount, BufferBarrier* BufferBarriers, uint32_t TextureCount, TextureBarrier* TextureBarriers,
                              uint32_t RenderTargetCount, RenderTargetBarrier* RenderTargetBarriers)
{
    if (BufferCount || TextureCount || RenderTargetCount)
        cmdResourceBarrier(pCmd, BufferCount, BufferBarriers, TextureCount, TextureBarriers, RenderTargetCount, RenderTargetBarriers);
}

void hz::CommandList::AddBufferBarriers(hz::Span<const hz::GPUBuffer*> Buffers, bool Storage)
{
    for (const hz::GPUBuffer* BufferValue : Buffers)
    {
        if (!BufferValue || BufferValue->usage != RESOURCE_MEMORY_USAGE_GPU_ONLY)
        {
            continue;
        }
        ResourceState StateValue = RESOURCE_STATE_UNORDERED_ACCESS;
        if (!Storage)
        {
            StateValue = RESOURCE_STATE_SHADER_RESOURCE;
            if (BufferValue->Descriptors & (DESCRIPTOR_TYPE_VERTEX_BUFFER | DESCRIPTOR_TYPE_UNIFORM_BUFFER))
            {
                StateValue |= RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
            }
            if (BufferValue->Descriptors & DESCRIPTOR_TYPE_INDEX_BUFFER)
            {
                StateValue |= RESOURCE_STATE_INDEX_BUFFER;
            }
            if (BufferValue->Descriptors & DESCRIPTOR_TYPE_INDIRECT_BUFFER)
            {
                StateValue |= RESOURCE_STATE_INDIRECT_ARGUMENT;
            }
        }
        if (BufferValue->state != StateValue || Storage)
        {
            const BufferBarrier BufferBarrierValue = { .pBuffer = BufferValue->pBuffer, .currentState = BufferValue->state, .newState = StateValue };
            arrpush(bufferBarriers, BufferBarrierValue);
            BufferValue->state = StateValue;
        }
    }
}

void hz::CommandList::AddIndirectBarriers(Span<const GPUBuffer*> Buffers)
{
    for (const GPUBuffer* BufferValue : Buffers)
    {
        if (!BufferValue || (BufferValue->state & RESOURCE_STATE_INDIRECT_ARGUMENT))
        {
            continue;
        }
        const BufferBarrier Transition = { .pBuffer = BufferValue->pBuffer,
                                           .currentState = BufferValue->state,
                                           .newState = RESOURCE_STATE_INDIRECT_ARGUMENT };
        arrpush(bufferBarriers, Transition);
        BufferValue->state = RESOURCE_STATE_INDIRECT_ARGUMENT;
    }
}

void hz::CommandList::Barrier(const hz::Dependencies& DependenciesValue, const hz::GPUBuffer* IndirectBuffer)
{
    if (DependenciesValue.pShared)
    {
        Barrier(*DependenciesValue.pShared, nullptr);
    }
    arrsetlen(bufferBarriers, 0);
    arrsetlen(textureBarriers, 0);
    arrsetlen(renderTargetBarriers, 0);

    const auto AddTextureBarrier = [&](const hz::GPUTexture& TextureValue, ResourceState StateValue)
    {
        if (TextureValue.state != StateValue || StateValue == RESOURCE_STATE_UNORDERED_ACCESS)
        {
            if (TextureValue.pRenderTarget)
            {
                const RenderTargetBarrier RenderTargetBarrierValue = {
                    .pRenderTarget = TextureValue.pRenderTarget,
                    .currentState = TextureValue.state,
                    .newState = StateValue,
                };
                arrpush(renderTargetBarriers, RenderTargetBarrierValue);
            }
            else
            {
                const TextureBarrier TextureBarrierValue = {
                    .pTexture = TextureValue.pTexture,
                    .currentState = TextureValue.state,
                    .newState = StateValue,
                };
                arrpush(textureBarriers, TextureBarrierValue);
            }
            TextureValue.state = StateValue;
        }
    };

    for (const hz::GPUTexture* TextureValue : DependenciesValue.sampledTextures)
    {
        if (TextureValue)
        {
            AddTextureBarrier(*TextureValue, RESOURCE_STATE_SHADER_RESOURCE);
        }
    }
    for (const hz::GPUTexture* TextureValue : DependenciesValue.storageTextures)
    {
        if (TextureValue)
        {
            AddTextureBarrier(*TextureValue, RESOURCE_STATE_UNORDERED_ACCESS);
        }
    }
    AddBufferBarriers(DependenciesValue.buffers, false);
    AddBufferBarriers(DependenciesValue.storageBuffers, true);
    AddIndirectBarriers(DependenciesValue.indirectBuffers);
    if (IndirectBuffer && IndirectBuffer->state != RESOURCE_STATE_INDIRECT_ARGUMENT)
    {
        const BufferBarrier IndirectBarrier = {
            .pBuffer = IndirectBuffer->pBuffer,
            .currentState = IndirectBuffer->state,
            .newState = RESOURCE_STATE_INDIRECT_ARGUMENT,
        };
        arrpush(bufferBarriers, IndirectBarrier);
        IndirectBuffer->state = RESOURCE_STATE_INDIRECT_ARGUMENT;
    }

    Barrier((uint32_t)arrlen(bufferBarriers), bufferBarriers, (uint32_t)arrlen(textureBarriers), textureBarriers,
            (uint32_t)arrlen(renderTargetBarriers), renderTargetBarriers);
}

void hz::CommandList::BeginRendering(const hz::RenderPassDesc& Desc, const hz::Dependencies& DependenciesValue)
{
    ASSERT(Desc.colorAttachments.Count <= MAX_RENDER_TARGETS && (Desc.colorAttachments.Data || !Desc.colorAttachments.Count));
    if (DependenciesValue.pShared)
    {
        Barrier(*DependenciesValue.pShared, nullptr);
    }
    arrsetlen(bufferBarriers, 0);
    arrsetlen(textureBarriers, 0);
    arrsetlen(renderTargetBarriers, 0);

    const auto AddTextureBarrier = [&](const hz::GPUTexture& TextureValue, ResourceState StateValue)
    {
        if (TextureValue.state != StateValue || StateValue == RESOURCE_STATE_UNORDERED_ACCESS)
        {
            if (TextureValue.pRenderTarget)
            {
                const RenderTargetBarrier RenderTargetBarrierValue = {
                    .pRenderTarget = TextureValue.pRenderTarget,
                    .currentState = TextureValue.state,
                    .newState = StateValue,
                };
                arrpush(renderTargetBarriers, RenderTargetBarrierValue);
            }
            else
            {
                const TextureBarrier TextureBarrierValue = {
                    .pTexture = TextureValue.pTexture,
                    .currentState = TextureValue.state,
                    .newState = StateValue,
                };
                arrpush(textureBarriers, TextureBarrierValue);
            }
            TextureValue.state = StateValue;
        }
    };

    for (const hz::GPUTexture* TextureValue : DependenciesValue.sampledTextures)
    {
        if (TextureValue)
        {
            AddTextureBarrier(*TextureValue, RESOURCE_STATE_SHADER_RESOURCE);
        }
    }
    for (const hz::GPUTexture* TextureValue : DependenciesValue.storageTextures)
    {
        if (TextureValue)
        {
            AddTextureBarrier(*TextureValue, RESOURCE_STATE_UNORDERED_ACCESS);
        }
    }
    AddBufferBarriers(DependenciesValue.buffers, false);
    AddBufferBarriers(DependenciesValue.storageBuffers, true);
    AddIndirectBarriers(DependenciesValue.indirectBuffers);

    BindRenderTargetsDesc Bind = { .renderTargetCount = Desc.colorAttachments.Count };
    for (uint32_t I = 0; I < Desc.colorAttachments.Count; ++I)
    {
        const hz::ColorAttachment& Input = Desc.colorAttachments.Data[I];
        AddTextureBarrier(*Input.pTexture, RESOURCE_STATE_RENDER_TARGET);
        Bind.renderTargets[I] = {
            .pRenderTarget = Input.pTexture->pRenderTarget,
            .loadAction = Input.loadAction,
            .storeAction = Input.storeAction,
            .ClearValue = Input.ClearValue,
            .overrideClearValue = Input.loadAction == LOAD_ACTION_CLEAR,
        };
    }

    if (Desc.depthAttachment.pTexture)
    {
        const hz::DepthAttachment& Input = Desc.depthAttachment;
        AddTextureBarrier(*Input.pTexture, RESOURCE_STATE_DEPTH_WRITE);
        Bind.depthStencil = {
            .pDepthStencil = Input.pTexture->pRenderTarget,
            .loadAction = Input.loadAction,
            .loadActionStencil = TinyImageFormat_HasStencil((TinyImageFormat)Input.pTexture->pRenderTarget->Format) ? Input.loadAction
                                                                                                                    : LOAD_ACTION_DONTCARE,
            .storeAction = Input.storeAction,
            .ClearValue = Input.ClearValue,
            .overrideClearValue = Input.loadAction == LOAD_ACTION_CLEAR,
        };
    }

    Barrier((uint32_t)arrlen(bufferBarriers), bufferBarriers, (uint32_t)arrlen(textureBarriers), textureBarriers,
            (uint32_t)arrlen(renderTargetBarriers), renderTargetBarriers);
    cmdBindRenderTargets(pCmd, &Bind);
}
void hz::CommandList::EndRendering() { cmdBindRenderTargets(pCmd, nullptr); }

void hz::CommandList::SetPipeline(const hz::GPUPipeline& PipelineObject)
{
    pCurrentRootSignature = PipelineObject.pRootSignature;
    pCurrentDrawIndirectSignature = PipelineObject.pDrawIndirectSignature;
    cmdBindPipeline(pCmd, PipelineObject.pPipeline);
}

void hz::CommandList::SetVertexBuffer(uint32_t BindingIndex, const hz::GPUBuffer& BufferValue, uint64_t Offset, uint32_t Stride)
{
    pVertexBuffers[BindingIndex] = BufferValue.pBuffer;
    vertexOffsets[BindingIndex] = Offset;
    vertexStrides[BindingIndex] = Stride;
    if (BindingIndex == vertexBufferCount)
    {
        ++vertexBufferCount;
    }
    cmdBindVertexBuffer(pCmd, vertexBufferCount, pVertexBuffers, vertexStrides, vertexOffsets);
}

void hz::CommandList::SetIndexBuffer(const hz::GPUBuffer& BufferValue, uint64_t Offset, IndexType Type)
{
    cmdBindIndexBuffer(pCmd, BufferValue.pBuffer, Type, Offset);
}

void hz::CommandList::SetViewport(float X, float Y, float W, float H, float MinD, float MaxD)
{
    cmdSetViewport(pCmd, X, Y, W, H, MinD, MaxD);
}
void hz::CommandList::SetScissor(uint32_t X, uint32_t Y, uint32_t W, uint32_t H) { cmdSetScissor(pCmd, X, Y, W, H); }
void hz::CommandList::SetRootConstant(Span<const uint32_t> Values, uint32_t OffsetIn32BitValues)
{
    ASSERT(pCurrentRootSignature && Values.Data && Values.Count);
    const uint32_t DescriptorIndex = getDescriptorIndexFromName(pCurrentRootSignature, "RootConstant0");
    ASSERT(DescriptorIndex != UINT32_MAX);
    // C++ mirrors of the shader block are maintained by hand; catch a struct that outgrew the reflected block.
    ASSERTMSG(OffsetIn32BitValues + Values.Count <= pCurrentRootSignature->pDescriptors[DescriptorIndex].Size,
              "Root constants [%u, %u) exceed the shader's RootConstant0 block of %u values", OffsetIn32BitValues,
              OffsetIn32BitValues + Values.Count, pCurrentRootSignature->pDescriptors[DescriptorIndex].Size);
    cmdBindPushConstants(pCmd, pCurrentRootSignature, DescriptorIndex, Values.Data, Values.Count, OffsetIn32BitValues);
}

void hz::CommandList::Draw(uint32_t Count, uint32_t First) { cmdDraw(pCmd, Count, First); }
void hz::CommandList::DrawIndexed(uint32_t Count, uint32_t First, uint32_t Vertex) { cmdDrawIndexed(pCmd, Count, First, Vertex); }

void hz::CommandList::DrawIndirect(const hz::GPUBuffer& BufferValue, uint64_t Offset, uint32_t DrawCount)
{
    if (!(BufferValue.state & RESOURCE_STATE_INDIRECT_ARGUMENT))
    {
        BufferBarrier IndirectBarrier = {
            .pBuffer = BufferValue.pBuffer,
            .currentState = BufferValue.state,
            .newState = RESOURCE_STATE_INDIRECT_ARGUMENT,
        };
        Barrier(1, &IndirectBarrier, 0, nullptr, 0, nullptr);
        BufferValue.state = RESOURCE_STATE_INDIRECT_ARGUMENT;
    }
    cmdExecuteIndirect(pCmd, pCurrentDrawIndirectSignature ? pCurrentDrawIndirectSignature : pContext->pDrawIndirectSignature, DrawCount,
                       BufferValue.pBuffer, Offset, nullptr, 0);
}

void hz::CommandList::DrawIndexedIndirect(const hz::GPUBuffer& BufferValue, uint64_t Offset, uint32_t DrawCount)
{
    if (!(BufferValue.state & RESOURCE_STATE_INDIRECT_ARGUMENT))
    {
        BufferBarrier IndirectBarrier = {
            .pBuffer = BufferValue.pBuffer,
            .currentState = BufferValue.state,
            .newState = RESOURCE_STATE_INDIRECT_ARGUMENT,
        };
        Barrier(1, &IndirectBarrier, 0, nullptr, 0, nullptr);
        BufferValue.state = RESOURCE_STATE_INDIRECT_ARGUMENT;
    }
    cmdExecuteIndirect(pCmd, pContext->pDrawIndexedIndirectSignature, DrawCount, BufferValue.pBuffer, Offset, nullptr, 0);
}

void hz::CommandList::Dispatch(uint32_t X, uint32_t Y, uint32_t Z, const hz::Dependencies& DependenciesValue)
{
    Barrier(DependenciesValue, nullptr);
    cmdDispatch(pCmd, X, Y, Z);
}

void hz::CommandList::DispatchIndirect(const hz::GPUBuffer& BufferValue, uint64_t Offset, const hz::Dependencies& DependenciesValue)
{
    Barrier(DependenciesValue, &BufferValue);
    cmdExecuteIndirect(pCmd, pContext->pDispatchIndirectSignature, 1, BufferValue.pBuffer, Offset, nullptr, 0);
}

Buffer* hz::CommandList::CreateUploadBuffer(uint64_t Size)
{
    const ::BufferDesc Desc = {
        .Size = Size,
        .Name = "CommandList Upload buffer",
        .MemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU,
        .Flags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT,
        .QueueType = QUEUE_TYPE_GRAPHICS,
        .StartState = RESOURCE_STATE_GENERIC_READ,
    };
    Buffer* BufferValue = nullptr;
    addBuffer(pContext->pRenderer, &Desc, &BufferValue);
    arrpush(pContext->commandResources[commandIndex].transientBuffers, BufferValue);
    return BufferValue;
}

void hz::CommandList::CopyBuffer(const hz::GPUBuffer& Dst, uint64_t DstOffset, const hz::GPUBuffer& Src, uint64_t SrcOffset, uint64_t Size)
{
    const BufferCopyRegion Region = { .srcOffset = SrcOffset, .dstOffset = DstOffset, .Size = Size };
    CopyBufferRegions(Dst, Src, { &Region, 1 });
}

void hz::CommandList::CopyBufferRegions(const hz::GPUBuffer& Dst, const hz::GPUBuffer& Src, Span<BufferCopyRegion> Regions)
{
    if (!Regions.Count)
    {
        return;
    }
    const ResourceState DstState = Dst.state;
    const ResourceState SrcState = Src.state;
    arrsetlen(bufferBarriers, 0);
    if (Dst.usage == RESOURCE_MEMORY_USAGE_GPU_ONLY && Dst.state != RESOURCE_STATE_COPY_DEST)
    {
        const BufferBarrier DstBarrier = {
            .pBuffer = Dst.pBuffer,
            .currentState = Dst.state,
            .newState = RESOURCE_STATE_COPY_DEST,
        };
        arrpush(bufferBarriers, DstBarrier);
        Dst.state = RESOURCE_STATE_COPY_DEST;
    }
    if (Src.usage == RESOURCE_MEMORY_USAGE_GPU_ONLY && Src.state != RESOURCE_STATE_COPY_SOURCE)
    {
        const BufferBarrier SrcBarrier = {
            .pBuffer = Src.pBuffer,
            .currentState = Src.state,
            .newState = RESOURCE_STATE_COPY_SOURCE,
        };
        arrpush(bufferBarriers, SrcBarrier);
        Src.state = RESOURCE_STATE_COPY_SOURCE;
    }
    Barrier((uint32_t)arrlen(bufferBarriers), bufferBarriers, 0, nullptr, 0, nullptr);
    for (const BufferCopyRegion& Region : Regions)
    {
        cmdUpdateBuffer(pCmd, Dst.pBuffer, Region.dstOffset, Src.pBuffer, Region.srcOffset, Region.Size);
    }

    arrsetlen(bufferBarriers, 0);
    if (Dst.state != DstState)
    {
        const BufferBarrier DstBarrier = {
            .pBuffer = Dst.pBuffer,
            .currentState = Dst.state,
            .newState = DstState,
        };
        arrpush(bufferBarriers, DstBarrier);
        Dst.state = DstState;
    }
    if (Src.state != SrcState)
    {
        const BufferBarrier SrcBarrier = {
            .pBuffer = Src.pBuffer,
            .currentState = Src.state,
            .newState = SrcState,
        };
        arrpush(bufferBarriers, SrcBarrier);
        Src.state = SrcState;
    }
    Barrier((uint32_t)arrlen(bufferBarriers), bufferBarriers, 0, nullptr, 0, nullptr);
}

void hz::CommandList::FillBuffer(const hz::GPUBuffer& Dst, uint64_t DstOffset, uint32_t Value, uint64_t Size)
{
    const auto Fill = [&](void* Data)
    {
        uint8_t* Bytes = (uint8_t*)Data;
        for (uint64_t Offset = 0; Offset < Size; Offset += sizeof(Value))
        {
            const size_t WriteSize = (size_t)TF_MIN((uint64_t)sizeof(Value), Size - Offset);
            memcpy(Bytes + Offset, &Value, WriteSize);
        }
    };

    if (Dst.usage == RESOURCE_MEMORY_USAGE_CPU_TO_GPU)
    {
        Fill((uint8_t*)Dst.pBuffer->pCpuMappedAddress + DstOffset);
    }
    else
    {
        Buffer* Upload = CreateUploadBuffer(Size);
        Fill(Upload->pCpuMappedAddress);

        const ResourceState DstState = Dst.state;
        BufferBarrier       DstBarrier = { .pBuffer = Dst.pBuffer };
        if (Dst.state != RESOURCE_STATE_COPY_DEST)
        {
            DstBarrier.currentState = Dst.state;
            DstBarrier.newState = RESOURCE_STATE_COPY_DEST;
            Barrier(1, &DstBarrier, 0, nullptr, 0, nullptr);
            Dst.state = RESOURCE_STATE_COPY_DEST;
        }
        cmdUpdateBuffer(pCmd, Dst.pBuffer, DstOffset, Upload, 0, Size);
        if (Dst.state != DstState)
        {
            DstBarrier.currentState = Dst.state;
            DstBarrier.newState = DstState;
            Barrier(1, &DstBarrier, 0, nullptr, 0, nullptr);
            Dst.state = DstState;
        }
    }
}

void hz::CommandList::UpdateBuffer(const hz::GPUBuffer& Dst, uint64_t DstOffset, const void* Data, uint64_t Size)
{
    if (Dst.usage == RESOURCE_MEMORY_USAGE_CPU_TO_GPU)
    {
        memcpy((uint8_t*)Dst.pBuffer->pCpuMappedAddress + DstOffset, Data, (size_t)Size);
    }
    else
    {
        Buffer* Upload = CreateUploadBuffer(Size);
        memcpy(Upload->pCpuMappedAddress, Data, (size_t)Size);

        const ResourceState DstState = Dst.state;
        BufferBarrier       DstBarrier = { .pBuffer = Dst.pBuffer };
        if (Dst.state != RESOURCE_STATE_COPY_DEST)
        {
            DstBarrier.currentState = Dst.state;
            DstBarrier.newState = RESOURCE_STATE_COPY_DEST;
            Barrier(1, &DstBarrier, 0, nullptr, 0, nullptr);
            Dst.state = RESOURCE_STATE_COPY_DEST;
        }
        cmdUpdateBuffer(pCmd, Dst.pBuffer, DstOffset, Upload, 0, Size);
        if (Dst.state != DstState)
        {
            DstBarrier.currentState = Dst.state;
            DstBarrier.newState = DstState;
            Barrier(1, &DstBarrier, 0, nullptr, 0, nullptr);
            Dst.state = DstState;
        }
    }
}

void hz::CommandList::CopyTexture(const hz::GPUTexture& Dst, const hz::GPUTexture& Src)
{
    const ResourceState DstState = Dst.state;
    const ResourceState SrcState = Src.state;
    arrsetlen(textureBarriers, 0);
    arrsetlen(renderTargetBarriers, 0);
    const auto Transition = [&](const hz::GPUTexture& TextureValue, ResourceState StateValue)
    {
        if (TextureValue.state != StateValue && TextureValue.pRenderTarget)
        {
            const RenderTargetBarrier TextureBarrierValue = {
                .pRenderTarget = TextureValue.pRenderTarget,
                .currentState = TextureValue.state,
                .newState = StateValue,
            };
            arrpush(renderTargetBarriers, TextureBarrierValue);
        }
        else if (TextureValue.state != StateValue)
        {
            const TextureBarrier TextureBarrierValue = {
                .pTexture = TextureValue.pTexture,
                .currentState = TextureValue.state,
                .newState = StateValue,
            };
            arrpush(textureBarriers, TextureBarrierValue);
        }
        TextureValue.state = StateValue;
    };

    Transition(Dst, RESOURCE_STATE_COPY_DEST);
    Transition(Src, RESOURCE_STATE_COPY_SOURCE);
    Barrier(0, nullptr, (uint32_t)arrlen(textureBarriers), textureBarriers, (uint32_t)arrlen(renderTargetBarriers), renderTargetBarriers);
    cmdCopyTexture(pCmd, Dst.pTexture, Src.pTexture);

    arrsetlen(textureBarriers, 0);
    arrsetlen(renderTargetBarriers, 0);
    Transition(Dst, DstState);
    Transition(Src, SrcState);
    Barrier(0, nullptr, (uint32_t)arrlen(textureBarriers), textureBarriers, (uint32_t)arrlen(renderTargetBarriers), renderTargetBarriers);
}

void hz::CommandList::PushDebugGroupLabel(const char* Label, uint32_t ColorRgba) const
{
    const float R = (float)((ColorRgba >> 0) & 0xff) / 255.0f;
    const float G = (float)((ColorRgba >> 8) & 0xff) / 255.0f;
    const float B = (float)((ColorRgba >> 16) & 0xff) / 255.0f;
    cmdBeginDebugMarker(pCmd, R, G, B, Label);
}

void hz::CommandList::InsertDebugEventLabel(const char* Label, uint32_t ColorRgba) const
{
    const float R = (float)((ColorRgba >> 0) & 0xff) / 255.0f;
    const float G = (float)((ColorRgba >> 8) & 0xff) / 255.0f;
    const float B = (float)((ColorRgba >> 16) & 0xff) / 255.0f;
    cmdAddDebugMarker(pCmd, R, G, B, Label);
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
void hz::CommandList::BeginGpuTimestamp(const char* NameValue)
{
    if (gpuProfilerToken != PROFILE_INVALID_TOKEN)
    {
        cmdBeginGpuTimestampQuery(pCmd, gpuProfilerToken, NameValue);
    }
    else
    {
        cmdBeginDebugMarker(pCmd, 1, 1, 0, NameValue);
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
