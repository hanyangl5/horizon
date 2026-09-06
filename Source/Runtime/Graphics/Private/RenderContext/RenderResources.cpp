/* Copyright (c) 2026 Horizon */

#include "Graphics/RenderContext.h"
#include "Resources/IResourceLoader.h"

#include <string.h>

#include "Core/ILog.h"

namespace hz
{
GPUBuffer::GPUBuffer(RenderContext* context, Buffer* buffer, uint64_t bufferSize, ResourceMemoryUsage memoryUsage,
                     DescriptorType bufferDescriptors, ResourceState initialState):
    pContext(context),
    pBuffer(buffer), size(bufferSize), usage(memoryUsage), descriptors(bufferDescriptors), state(initialState)
{
    ASSERT(pContext && pBuffer);
    ++pContext->resourceCount;
}

GPUBuffer::~GPUBuffer() { destroy(); }

GPUBuffer::GPUBuffer(GPUBuffer&& other) noexcept:
    pContext(other.pContext), pBuffer(other.pBuffer), size(other.size), usage(other.usage), descriptors(other.descriptors),
    state(other.state)
{
    other.pContext = nullptr;
    other.pBuffer = nullptr;
    other.size = 0;
    other.usage = RESOURCE_MEMORY_USAGE_UNKNOWN;
    other.descriptors = DESCRIPTOR_TYPE_UNDEFINED;
    other.state = RESOURCE_STATE_UNDEFINED;
}

GPUBuffer& GPUBuffer::operator=(GPUBuffer&& other) noexcept
{
    if (this != &other)
    {
        destroy();
        pContext = other.pContext;
        pBuffer = other.pBuffer;
        size = other.size;
        usage = other.usage;
        descriptors = other.descriptors;
        state = other.state;
        other.pContext = nullptr;
        other.pBuffer = nullptr;
        other.size = 0;
        other.usage = RESOURCE_MEMORY_USAGE_UNKNOWN;
        other.descriptors = DESCRIPTOR_TYPE_UNDEFINED;
        other.state = RESOURCE_STATE_UNDEFINED;
    }
    return *this;
}

void GPUBuffer::destroy()
{
    ASSERT((pContext != nullptr) == (pBuffer != nullptr));
    if (pBuffer)
    {
        ASSERT(pContext->resourceCount);
        --pContext->resourceCount;
        removeResource(pBuffer);
    }
}

GPUTexture::GPUTexture(RenderContext* context, Texture* texture, RenderTarget* renderTarget, ResourceState initialState, bool ownsResource):
    pContext(context), pTexture(texture), pRenderTarget(renderTarget), state(initialState), owned(ownsResource)
{
    ASSERT(pTexture);
    if (owned)
    {
        ASSERT(pContext);
        ++pContext->resourceCount;
    }
}

GPUTexture::~GPUTexture() { destroy(); }

GPUTexture::GPUTexture(GPUTexture&& other) noexcept:
    pContext(other.pContext), pTexture(other.pTexture), pRenderTarget(other.pRenderTarget), state(other.state), owned(other.owned)
{
    other.pContext = nullptr;
    other.pTexture = nullptr;
    other.pRenderTarget = nullptr;
    other.state = RESOURCE_STATE_UNDEFINED;
    other.owned = false;
}

GPUTexture& GPUTexture::operator=(GPUTexture&& other) noexcept
{
    if (this != &other)
    {
        destroy();
        pContext = other.pContext;
        pTexture = other.pTexture;
        pRenderTarget = other.pRenderTarget;
        state = other.state;
        owned = other.owned;
        other.pContext = nullptr;
        other.pTexture = nullptr;
        other.pRenderTarget = nullptr;
        other.state = RESOURCE_STATE_UNDEFINED;
        other.owned = false;
    }
    return *this;
}

void GPUTexture::destroy()
{
    if (pTexture && owned)
    {
        ASSERT(pContext && pContext->resourceCount);
        --pContext->resourceCount;
        if (pRenderTarget)
            removeRenderTarget(pContext->pRenderer, pRenderTarget);
        else
            removeResource(pTexture);
    }
}

GPUSampler::GPUSampler(RenderContext* context, Sampler* sampler): pContext(context), pSampler(sampler)
{
    ASSERT(pContext && pSampler);
    ++pContext->resourceCount;
}

GPUSampler::~GPUSampler() { destroy(); }

GPUSampler::GPUSampler(GPUSampler&& other) noexcept: pContext(other.pContext), pSampler(other.pSampler)
{
    other.pContext = nullptr;
    other.pSampler = nullptr;
}

GPUSampler& GPUSampler::operator=(GPUSampler&& other) noexcept
{
    if (this != &other)
    {
        destroy();
        pContext = other.pContext;
        pSampler = other.pSampler;
        other.pContext = nullptr;
        other.pSampler = nullptr;
    }
    return *this;
}

void GPUSampler::destroy()
{
    ASSERT((pContext != nullptr) == (pSampler != nullptr));
    if (pSampler)
    {
        ASSERT(pContext->resourceCount);
        --pContext->resourceCount;
        removeSampler(pContext->pRenderer, pSampler);
    }
}

GPUShader::GPUShader(RenderContext* context, Shader* shader): pContext(context), pShader(shader)
{
    ASSERT(pContext && pShader);
    ++pContext->resourceCount;
}

GPUShader::~GPUShader() { destroy(); }

GPUShader::GPUShader(GPUShader&& other) noexcept: pContext(other.pContext), pShader(other.pShader)
{
    other.pContext = nullptr;
    other.pShader = nullptr;
}

GPUShader& GPUShader::operator=(GPUShader&& other) noexcept
{
    if (this != &other)
    {
        destroy();
        pContext = other.pContext;
        pShader = other.pShader;
        other.pContext = nullptr;
        other.pShader = nullptr;
    }
    return *this;
}

void GPUShader::destroy()
{
    ASSERT((pContext != nullptr) == (pShader != nullptr));
    if (pShader)
    {
        ASSERT(pContext->resourceCount);
        --pContext->resourceCount;
        removeShader(pContext->pRenderer, pShader);
    }
}

GPUPipeline::GPUPipeline(RenderContext* context, Pipeline* pipeline, RootSignature* rootSignature):
    pContext(context), pPipeline(pipeline), pRootSignature(rootSignature)
{
    ASSERT(pContext && pPipeline && pRootSignature);
    ++pContext->resourceCount;
}

GPUPipeline::~GPUPipeline() { destroy(); }

GPUPipeline::GPUPipeline(GPUPipeline&& other) noexcept:
    pContext(other.pContext), pPipeline(other.pPipeline), pRootSignature(other.pRootSignature)
{
    other.pContext = nullptr;
    other.pPipeline = nullptr;
    other.pRootSignature = nullptr;
}

GPUPipeline& GPUPipeline::operator=(GPUPipeline&& other) noexcept
{
    if (this != &other)
    {
        destroy();
        pContext = other.pContext;
        pPipeline = other.pPipeline;
        pRootSignature = other.pRootSignature;
        other.pContext = nullptr;
        other.pPipeline = nullptr;
        other.pRootSignature = nullptr;
    }
    return *this;
}

void GPUPipeline::destroy()
{
    ASSERT((pContext != nullptr) == (pPipeline != nullptr));
    if (pPipeline)
    {
        ASSERT(pContext->resourceCount && pRootSignature);
        --pContext->resourceCount;
        removePipeline(pContext->pRenderer, pPipeline);
        removeRootSignature(pContext->pRenderer, pRootSignature);
    }
}

GPUBuffer RenderContext::createBuffer(const BufferDesc& input)
{
    ASSERT(ready && input.size && input.initialDataSize <= input.size && ((input.pInitialData != nullptr) == (input.initialDataSize != 0)));

    ::BufferDesc desc = {};
    desc.mSize = input.size;
    desc.mElementCount = input.elementCount;
    desc.mStructStride = input.structStride;
    desc.pName = input.pName;
    desc.mQueueType = QUEUE_TYPE_GRAPHICS;
    desc.mMemoryUsage = input.usage;
    desc.mStartState = input.startState;
    desc.mDescriptors = input.descriptors;
    desc.mFlags = input.flags;

    Buffer*        buffer = nullptr;
    SyncToken      token = 0;
    BufferLoadDesc load = { .ppBuffer = &buffer, .pData = input.pInitialData, .mDesc = desc };
    addResource(&load, &token);
    ASSERT(buffer);
    waitForToken(&token);
    return GPUBuffer(this, buffer, input.size, input.usage, input.descriptors, input.startState);
}

GPUTexture RenderContext::createTexture(const TextureDesc& input)
{
    ASSERT(ready && input.width && input.height && input.depth && input.arraySize && input.mipLevels);

    Texture*      texture = nullptr;
    RenderTarget* renderTarget = nullptr;

    if (input.renderTarget)
    {
        const RenderTargetDesc desc = {
            .mFlags = input.flags,
            .mWidth = input.width,
            .mHeight = input.height,
            .mDepth = input.depth,
            .mArraySize = input.arraySize,
            .mMipLevels = input.mipLevels,
            .mSampleCount = input.sampleCount,
            .mFormat = input.format,
            .mStartState = input.startState,
            .mDescriptors = input.descriptors,
            .pName = input.pName,
        };
        addRenderTarget(pRenderer, &desc, &renderTarget);
        ASSERT(renderTarget);
        texture = renderTarget->pTexture;
    }
    else
    {
        ::TextureDesc desc = {};
        desc.mWidth = input.width;
        desc.mHeight = input.height;
        desc.mDepth = input.depth;
        desc.mArraySize = input.arraySize;
        desc.mMipLevels = input.mipLevels;
        desc.mSampleCount = input.sampleCount;
        desc.mFormat = input.format;
        desc.mStartState = input.startState;
        desc.mDescriptors = input.descriptors;
        desc.mFlags = input.flags;
        desc.pName = input.pName;
        SyncToken       token = 0;
        TextureLoadDesc load = { .ppTexture = &texture, .pDesc = &desc };
        addResource(&load, &token);
        ASSERT(texture);
        waitForToken(&token);
    }
    return GPUTexture(this, texture, renderTarget, input.startState);
}

GPUSampler RenderContext::createSampler(const SamplerDesc& input)
{
    ASSERT(ready);
    Sampler* sampler = nullptr;
    addSampler(pRenderer, &input, &sampler);
    ASSERT(sampler);
    return GPUSampler(this, sampler);
}

GPUShader RenderContext::createShader(const ShaderDesc& input)
{
    ASSERT(ready && input.stageCount && input.stageCount <= MAX_SHADER_STAGES);
    for (uint32_t i = 0; i < input.stageCount; ++i)
        ASSERT(input.stages[i].pSource && input.stages[i].sourceSize && input.stages[i].pEntryPoint && input.stages[i].pEntryPoint[0]);

    ShaderSrcDesc source = {};
    for (uint32_t i = 0; i < input.stageCount; ++i)
    {
        ShaderSrcStageDesc* stage = nullptr;
        switch (input.stages[i].stage)
        {
        case SHADER_STAGE_VERT:
            stage = &source.mVert;
            break;
        case SHADER_STAGE_FRAG:
            stage = &source.mFrag;
            break;
        case SHADER_STAGE_COMP:
            stage = &source.mComp;
            break;
        default:
            ASSERT(false);
            break;
        }
        source.mStages |= input.stages[i].stage;
        stage->pName = input.stages[i].pName;
        stage->pByteCode = (void*)input.stages[i].pSource;
        stage->mByteCodeSize = input.stages[i].sourceSize;
        stage->pEntryPoint = input.stages[i].pEntryPoint;
    }

    Shader* shader = nullptr;
    addShaderSource(pRenderer, &source, &shader);
    ASSERT(shader);
    return GPUShader(this, shader);
}

GPUPipeline RenderContext::createGraphicsPipeline(const GraphicsPipelineDesc& input)
{
    ASSERT(ready && input.pShader && input.pShader->pShader && input.renderTargetCount <= MAX_RENDER_TARGETS);
    Shader*           shader = input.pShader->pShader;
    Shader*           shaders[] = { shader };
    RootSignatureDesc rootDesc = { .ppShaders = shaders, .mShaderCount = 1 };
    RootSignature*    rootSignature = nullptr;
    addRootSignature(pRenderer, &rootDesc, &rootSignature);
    ASSERT(rootSignature);

    VertexLayout        vertex = input.vertexLayout;
    RasterizerStateDesc raster = input.rasterizer;
    DepthStateDesc      depth = input.depth;
    BlendStateDesc      blend = input.blend;
    TinyImageFormat     formats[MAX_RENDER_TARGETS] = {};
    memcpy(formats, input.colorFormats, sizeof(formats));
    const PipelineDesc desc = {
        .mGraphicsDesc = {
            .pShaderProgram = shader,
            .pRootSignature = rootSignature,
            .pVertexLayout = vertex.mBindingCount && vertex.mAttribCount ? &vertex : nullptr,
            .pBlendState = &blend,
            .pDepthState = &depth,
            .pRasterizerState = &raster,
            .pColorFormats = formats,
            .mRenderTargetCount = input.renderTargetCount,
            .mSampleCount = input.sampleCount,
            .mDepthStencilFormat = input.depthStencilFormat,
            .mPrimitiveTopo = input.topology,
        },
        .pName = input.pName,
        .mType = PIPELINE_TYPE_GRAPHICS,
    };
    Pipeline* pipeline = nullptr;
    addPipeline(pRenderer, &desc, &pipeline);
    ASSERT(pipeline);
    return GPUPipeline(this, pipeline, rootSignature);
}

GPUPipeline RenderContext::createComputePipeline(const ComputePipelineDesc& input)
{
    ASSERT(ready && input.pShader && input.pShader->pShader);
    Shader*           shader = input.pShader->pShader;
    Shader*           shaders[] = { shader };
    RootSignatureDesc rootDesc = { .ppShaders = shaders, .mShaderCount = 1 };
    RootSignature*    rootSignature = nullptr;
    addRootSignature(pRenderer, &rootDesc, &rootSignature);
    ASSERT(rootSignature);

    const PipelineDesc desc = {
        .mComputeDesc = { shader, rootSignature },
        .pName = input.pName,
        .mType = PIPELINE_TYPE_COMPUTE,
    };
    Pipeline* pipeline = nullptr;
    addPipeline(pRenderer, &desc, &pipeline);
    ASSERT(pipeline);
    return GPUPipeline(this, pipeline, rootSignature);
}

bool RenderContext::getGpuAddress(const GPUBuffer& buffer, uint64_t* pAddress) const
{
    ASSERT(ready && buffer.pBuffer && pAddress);
    *pAddress = buffer.pBuffer->mDx.mGpuAddress;
    return true;
}

bool RenderContext::updateBuffer(const GPUBuffer& buffer, uint64_t offset, const void* pData, uint64_t size)
{
    ASSERT(ready && buffer.pBuffer && pData && size);
    ASSERT(buffer.usage == RESOURCE_MEMORY_USAGE_CPU_TO_GPU && offset <= buffer.size && size <= buffer.size - offset &&
           buffer.pBuffer->pCpuMappedAddress);
    if (buffer.usage != RESOURCE_MEMORY_USAGE_CPU_TO_GPU || offset > buffer.size || size > buffer.size - offset ||
        !buffer.pBuffer->pCpuMappedAddress)
        return false;
    memcpy((uint8_t*)buffer.pBuffer->pCpuMappedAddress + offset, pData, (size_t)size);
    return true;
}
} // namespace hz
