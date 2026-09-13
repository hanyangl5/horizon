/* Copyright (c) 2026 Horizon */

#include "Graphics/RenderContext.h"
#include "Resources/IResourceLoader.h"

#include <stdio.h>
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

GPUPipeline::GPUPipeline(RenderContext* context, Pipeline* pipeline, RootSignature* rootSignature, GPUShader&& shader):
    pContext(context), pPipeline(pipeline), pRootSignature(rootSignature), shader(std::move(shader))
{
    ASSERT(pContext && pPipeline && pRootSignature);
    ++pContext->resourceCount;
}

GPUPipeline::~GPUPipeline() { destroy(); }

GPUPipeline::GPUPipeline(GPUPipeline&& other) noexcept:
    pContext(other.pContext), pPipeline(other.pPipeline), pRootSignature(other.pRootSignature), shader(std::move(other.shader))
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
        shader = std::move(other.shader);
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
    desc.size = input.size;
    desc.elementCount = input.elementCount;
    desc.structStride = input.structStride;
    desc.pName = input.pName;
    desc.queueType = QUEUE_TYPE_GRAPHICS;
    desc.memoryUsage = input.usage;
    desc.startState = input.startState;
    desc.descriptors = input.descriptors;
    desc.flags = input.flags;

    Buffer*        buffer = nullptr;
    SyncToken      token = 0;
    BufferLoadDesc load = { .ppBuffer = &buffer, .pData = input.pInitialData, .desc = desc };
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
            .flags = input.flags,
            .width = input.width,
            .height = input.height,
            .depth = input.depth,
            .arraySize = input.arraySize,
            .mipLevels = input.mipLevels,
            .sampleCount = input.sampleCount,
            .format = input.format,
            .startState = input.startState,
            .descriptors = input.descriptors,
            .pName = input.pName,
        };
        addRenderTarget(pRenderer, &desc, &renderTarget);
        ASSERT(renderTarget);
        texture = renderTarget->pTexture;
    }
    else
    {
        ::TextureDesc desc = {};
        desc.width = input.width;
        desc.height = input.height;
        desc.depth = input.depth;
        desc.arraySize = input.arraySize;
        desc.mipLevels = input.mipLevels;
        desc.sampleCount = input.sampleCount;
        desc.format = input.format;
        desc.startState = input.startState;
        desc.descriptors = input.descriptors;
        desc.flags = input.flags;
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
    const ::SamplerDesc samplerDesc = {
        .minFilter = input.minFilter,
        .magFilter = input.magFilter,
        .mipMapMode = input.mipMapMode,
        .addressU = input.addressU,
        .addressV = input.addressV,
        .addressW = input.addressW,
        .mipLodBias = input.mipLodBias,
        .setLodRange = input.setLodRange,
        .minLod = input.minLod,
        .maxLod = input.maxLod,
        .maxAnisotropy = input.maxAnisotropy,
        .compareFunc = input.compareFunc,
    };
    Sampler* sampler = nullptr;
    addSampler(pRenderer, &samplerDesc, &sampler);
    ASSERT(sampler);
    return GPUSampler(this, sampler);
}

Shader* RenderContext::createShader(const ShaderDesc& input)
{
    ASSERT(ready);
    ShaderSrcDesc source = {};
    FileStream    sourceFile = {};
    const void*   pFileSource = nullptr;
    size_t        fileSize = 0;
    char          sourcePath[FS_MAX_PATH] = {};
    if (input.pFileName && (!fsOpenStreamFromPath(input.sourceDirectory, input.pFileName, FM_READ, &sourceFile) ||
                            !fsStreamMemoryMap(&sourceFile, &fileSize, &pFileSource) || !fileSize || fileSize > UINT32_MAX))
    {
        LOGF(eERROR, "Could not map shader source '%s'", input.pFileName);
        if (sourceFile.pIO)
            fsCloseStream(&sourceFile);
        return {};
    }
    if (pFileSource)
        snprintf(sourcePath, sizeof(sourcePath), "%s/%s", fsGetResourceDirectory(input.sourceDirectory), input.pFileName);
    uint32_t stageCount = input.stages.count;
    for (uint32_t i = 0; i < stageCount; ++i)
    {
        const ShaderStageDesc& inputStage = *(input.stages.pData + i);
        ShaderSrcStageDesc*    stage = nullptr;
        switch (inputStage.stage)
        {
        case SHADER_STAGE_VERT:
            stage = &source.vert;
            break;
        case SHADER_STAGE_FRAG:
            stage = &source.frag;
            break;
        case SHADER_STAGE_COMP:
            stage = &source.comp;
            break;
        default:
            ASSERT(false);
            break;
        }
        source.stages |= inputStage.stage;
        stage->pName = pFileSource ? sourcePath : inputStage.pName;
        stage->pByteCode = (void*)(pFileSource ? pFileSource : inputStage.pSource);
        stage->byteCodeSize = pFileSource ? (uint32_t)fileSize : inputStage.sourceSize;
        stage->pEntryPoint = inputStage.pEntryPoint;
        ASSERT(stage->pByteCode && stage->byteCodeSize && stage->pEntryPoint && stage->pEntryPoint[0]);
    }

    Shader*     shader = nullptr;
    const char* extension = input.pFileName ? strrchr(input.pFileName, '.') : nullptr;
    if (input.language == ShaderLanguage::Slang ||
        (input.language == ShaderLanguage::Auto && extension && strcmp(extension, ".slang") == 0))
        shader = createSlangShader(source);
    else
        addShaderSource(pRenderer, &source, &shader);
    if (sourceFile.pIO)
        fsCloseStream(&sourceFile);
    ASSERT(shader);
    return shader;
}

GPUPipeline RenderContext::createGraphicsPipeline(const GraphicsPipelineDesc& input)
{
    ASSERT(ready && input.colorTargets.count <= MAX_RENDER_TARGETS && (input.colorTargets.pData || !input.colorTargets.count));
    Shader* shader = this->createShader(input.shaderDesc);
    ASSERT(shader);
    Shader*           shaders[] = { shader };
    RootSignatureDesc rootDesc = { .ppShaders = shaders, .shaderCount = 1 };
    RootSignature*    rootSignature = nullptr;
    addRootSignature(pRenderer, &rootDesc, &rootSignature);
    ASSERT(rootSignature);

    VertexLayout        vertex = input.vertexLayout;
    RasterizerStateDesc raster = input.rasterizer;
    DepthStateDesc      depth = input.depth;
    BlendStateDesc      blend = {
        .renderTargetMask = (BlendStateTargets)((1u << input.colorTargets.count) - 1),
        .alphaToCoverage = input.alphaToCoverage,
        .independentBlend = true,
    };
    hz::Format formats[MAX_RENDER_TARGETS] = {};
    for (uint32_t i = 0; i < input.colorTargets.count; ++i)
    {
        const ColorTargetDesc& target = input.colorTargets.pData[i];
        formats[i] = target.format;
        blend.srcFactors[i] = target.srcFactor;
        blend.dstFactors[i] = target.dstFactor;
        blend.srcAlphaFactors[i] = target.srcAlphaFactor;
        blend.dstAlphaFactors[i] = target.dstAlphaFactor;
        blend.blendModes[i] = target.blendMode;
        blend.blendAlphaModes[i] = target.blendAlphaMode;
        blend.colorWriteMasks[i] = target.colorWriteMask;
    }
    const PipelineDesc desc = {
        .graphicsDesc = {
            .pShaderProgram = shader,
            .pRootSignature = rootSignature,
            .pVertexLayout = vertex.bindingCount && vertex.attribCount ? &vertex : nullptr,
            .pBlendState = &blend,
            .pDepthState = &depth,
            .pRasterizerState = &raster,
            .pColorFormats = formats,
            .renderTargetCount = input.colorTargets.count,
            .sampleCount = input.sampleCount,
            .depthStencilFormat = input.depthStencilFormat,
            .primitiveTopo = input.topology,
        },
        .pName = input.pName,
        .type = PIPELINE_TYPE_GRAPHICS,
    };
    Pipeline* pipeline = nullptr;
    addPipeline(pRenderer, &desc, &pipeline);
    ASSERT(pipeline);
    return GPUPipeline(this, pipeline, rootSignature, GPUShader(this, shader));
}

GPUPipeline RenderContext::createComputePipeline(const ComputePipelineDesc& input)
{
    ASSERT(ready);
    Shader* shader = this->createShader(input.shaderDesc);
    ASSERT(shader);
    Shader*           shaders[] = { shader };
    RootSignatureDesc rootDesc = { .ppShaders = shaders, .shaderCount = 1 };
    RootSignature*    rootSignature = nullptr;
    addRootSignature(pRenderer, &rootDesc, &rootSignature);
    ASSERT(rootSignature);

    const PipelineDesc desc = {
        .computeDesc = { shader, rootSignature },
        .pName = input.pName,
        .type = PIPELINE_TYPE_COMPUTE,
    };
    Pipeline* pipeline = nullptr;
    addPipeline(pRenderer, &desc, &pipeline);
    ASSERT(pipeline);
    return GPUPipeline(this, pipeline, rootSignature, GPUShader(this, shader));
}

bool RenderContext::getGpuAddress(const GPUBuffer& buffer, uint64_t* pAddress) const
{
    ASSERT(ready && buffer.pBuffer && pAddress);
    *pAddress = buffer.pBuffer->dx.gpuAddress;
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
