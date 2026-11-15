/* Copyright (c) 2026 Horizon */

#include "Graphics/RenderContext.h"
#include "Resources/IResourceLoader.h"

#include <stdio.h>
#include <string.h>

#include "Core/ILog.h"

#include "Core/IMemory.h" //NOTE: this should be the last include in a .cpp

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
        pContext->deferResource(RenderContext::ResourceType::Buffer, pBuffer);
        pBuffer = nullptr;
        pContext = nullptr;
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
        pContext->deferResource(pRenderTarget ? RenderContext::ResourceType::RenderTarget : RenderContext::ResourceType::Texture,
                                pRenderTarget ? (void*)pRenderTarget : (void*)pTexture);
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
        pContext->deferResource(RenderContext::ResourceType::Sampler, pSampler);
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
        pContext->deferResource(RenderContext::ResourceType::Shader, pShader);
    }
}

GPUPipeline::GPUPipeline(RenderContext* context, Pipeline* pipeline, RootSignature* rootSignature, GPUShader&& shader,
                         CommandSignature* drawSignature):
    pContext(context), pPipeline(pipeline), pRootSignature(rootSignature), pDrawIndirectSignature(drawSignature), shader(std::move(shader))
{
    ASSERT(pContext && pPipeline && pRootSignature);
    ++pContext->resourceCount;
}

GPUPipeline::~GPUPipeline() { destroy(); }

GPUPipeline::GPUPipeline(GPUPipeline&& other) noexcept:
    pContext(other.pContext), pPipeline(other.pPipeline), pRootSignature(other.pRootSignature),
    pDrawIndirectSignature(other.pDrawIndirectSignature), shader(std::move(other.shader))
{
    other.pContext = nullptr;
    other.pPipeline = nullptr;
    other.pRootSignature = nullptr;
    other.pDrawIndirectSignature = nullptr;
}

GPUPipeline& GPUPipeline::operator=(GPUPipeline&& other) noexcept
{
    if (this != &other)
    {
        destroy();
        pContext = other.pContext;
        pPipeline = other.pPipeline;
        pRootSignature = other.pRootSignature;
        pDrawIndirectSignature = other.pDrawIndirectSignature;
        shader = std::move(other.shader);
        other.pContext = nullptr;
        other.pPipeline = nullptr;
        other.pRootSignature = nullptr;
        other.pDrawIndirectSignature = nullptr;
    }
    return *this;
}

void GPUPipeline::destroy()
{
    ASSERT((pContext != nullptr) == (pPipeline != nullptr));
    if (pPipeline)
    {
        ASSERT(pContext->resourceCount && pRootSignature);
        pContext->deferResource(RenderContext::ResourceType::Pipeline, pPipeline, pRootSignature, pDrawIndirectSignature);
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

uint32_t GPUTexture::getWidth() const { return pTexture ? pTexture->width : 0; }
uint32_t GPUTexture::getHeight() const { return pTexture ? pTexture->height : 0; }
uint32_t GPUTexture::getArraySize() const { return pTexture ? pTexture->arraySizeMinusOne + 1 : 0; }
uint32_t GPUTexture::getMipLevels() const { return pTexture ? pTexture->mipLevels : 0; }
Format   GPUTexture::getFormat() const { return pTexture ? pTexture->format : Format::UNDEFINED; }

static bool sameTextureDesc(const TextureDesc& a, const TextureDesc& b)
{
    return a.width == b.width && a.height == b.height && a.depth == b.depth && a.arraySize == b.arraySize &&
           a.mipLevels == b.mipLevels && a.sampleCount == b.sampleCount && a.format == b.format && a.startState == b.startState &&
           a.descriptors == b.descriptors && a.flags == b.flags && a.renderTarget == b.renderTarget;
}

GPUTexture* RenderContext::acquireTransient(const TextureDesc& input)
{
    ASSERT(ready && input.width && input.height);
    uint32_t freeSlot = transientCapacity;
    for (uint32_t i = 0; i < transientCapacity; ++i)
    {
        TransientSlot& slot = transientSlots[i];
        if (!slot.used)
        {
            if (freeSlot == transientCapacity)
                freeSlot = i;
            continue;
        }
        if (slot.inUse || !sameTextureDesc(slot.desc, input))
            continue;
        if (slot.useId && !isResourceUseComplete(slot.useId))
            continue;
        slot.inUse = true;
        return &slot.texture;
    }
    if (freeSlot == transientCapacity)
    {
        ASSERT(false);
        return nullptr;
    }
    TransientSlot& slot = transientSlots[freeSlot];
    slot.texture = createTexture(input);
    slot.desc = input;
    slot.useId = 0;
    slot.inUse = true;
    slot.used = true;
    return &slot.texture;
}

void RenderContext::releaseTransient(GPUTexture* pTexture)
{
    for (uint32_t i = 0; i < transientCapacity; ++i)
    {
        if (&transientSlots[i].texture != pTexture)
            continue;
        ASSERT(transientSlots[i].inUse);
        transientSlots[i].inUse = false;
        transientSlots[i].useId = getResourceUseId();
        return;
    }
    ASSERT(false);
}

void RenderContext::purgeTransients()
{
    for (TransientSlot& slot : transientSlots)
    {
        if (!slot.used || slot.inUse)
            continue;
        slot.texture = GPUTexture{};
        slot.used = false;
        slot.useId = 0;
    }
}

void RenderContext::releaseTransientSlots()
{
    for (TransientSlot& slot : transientSlots)
    {
        if (!slot.used)
            continue;
        ASSERT(!slot.inUse);
        slot.texture = GPUTexture{};
        slot.used = false;
        slot.useId = 0;
    }
}

GPUTexture RenderContext::loadTexture(ResourceDirectory directory, const char* pFileName, TextureCreationFlags flags)
{
    ASSERT(ready && pFileName);
    const char* extension = strrchr(pFileName, '.');
    if (!extension || (strcmp(extension, ".dds") && strcmp(extension, ".ktx")))
    {
        LOGF(Error, "Texture '%s' requires a DDS or KTX container", pFileName);
        return {};
    }
    Texture*        texture = nullptr;
    bool            success = false;
    TextureLoadDesc desc = { .ppTexture = &texture,
                             .pFileName = pFileName,
                             .creationFlag = flags,
                             .container = strcmp(extension, ".ktx") == 0 ? TEXTURE_CONTAINER_KTX : TEXTURE_CONTAINER_DDS,
                             .resourceDirectory = directory,
                             .pSuccess = &success };
    SyncToken       token = 0;
    addResource(&desc, &token);
    waitForToken(&token);
    if (!success || !texture)
    {
        if (texture)
        {
            removeResource(texture);
        }
        LOGF(Error, "Failed to load texture '%s'", pFileName);
        return {};
    }
    return GPUTexture(this, texture, nullptr, RESOURCE_STATE_SHADER_RESOURCE);
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
    if (input.pFileName && (!OpenStreamFromPath(input.sourceDirectory, input.pFileName, FM_READ, &sourceFile) ||
                            !StreamMemoryMap(&sourceFile, &fileSize, &pFileSource) || !fileSize || fileSize > UINT32_MAX))
    {
        LOGF(Error, "Could not map shader source '%s'", input.pFileName);
        if (sourceFile.pIO)
            CloseStream(&sourceFile);
        return {};
    }
    if (pFileSource)
        snprintf(sourcePath, sizeof(sourcePath), "%s/%s", GetResourceDirectory(input.sourceDirectory), input.pFileName);
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
        CloseStream(&sourceFile);
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
        .pCache = pPipelineCache,
        .pName = input.pName,
        .type = PIPELINE_TYPE_GRAPHICS,
    };
    Pipeline* pipeline = nullptr;
    addPipeline(pRenderer, &desc, &pipeline);
    ASSERT(pipeline);
    CommandSignature* drawSignature = nullptr;
    if (input.indirectDrawId)
    {
        const uint32_t constant = getDescriptorIndexFromName(rootSignature, "RootConstant0");
        ASSERT(constant != UINT32_MAX);
        IndirectArgumentDescriptor arguments[] = {
            { .type = INDIRECT_CONSTANT, .index = constant, .byteSize = sizeof(uint32_t) },
            { .type = INDIRECT_DRAW },
        };
        const CommandSignatureDesc signature = { .pRootSignature = rootSignature,
                                                 .pArgDescs = arguments,
                                                 .indirectArgCount = 2,
                                                 .packed = true };
        addIndirectCommandSignature(pRenderer, &signature, &drawSignature);
        ASSERT(drawSignature);
    }
    return GPUPipeline(this, pipeline, rootSignature, GPUShader(this, shader), drawSignature);
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
        .pCache = pPipelineCache,
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

namespace
{
constexpr const char* kPipelineCacheFile = "PipelineCache.bin";
constexpr uint32_t    kPipelineCacheMagic = 0x43505348; // 'HSPC'
constexpr uint32_t    kPipelineCacheVersion = 1;

struct PipelineCacheHeader
{
    uint32_t magic;
    uint32_t version;
    uint32_t vendorId;
    uint32_t deviceId;
    uint32_t driverHash;
    uint32_t reserved;
};

// PSO blobs are driver generated, so the driver and device are what validate a library; DXIL has its own cache.
PipelineCacheHeader makeCacheHeader(const GPUSettings& settings)
{
    uint32_t driverHash = 2166136261u;
    for (const char* p = settings.gpuVendorPreset.gpuDriverVersion; *p; ++p)
        driverHash = (driverHash ^ (uint8_t)*p) * 16777619u;
    return { .magic = kPipelineCacheMagic,
             .version = kPipelineCacheVersion,
             .vendorId = settings.gpuVendorPreset.vendorId,
             .deviceId = settings.gpuVendorPreset.modelId,
             .driverHash = driverHash };
}
} // namespace

bool RenderContext::initPipelineCache()
{
    const GPUSettings&       settings = pRenderer->pGpu->settings;
    const PipelineCacheHeader expected = makeCacheHeader(settings);
    PipelineCacheDesc        cacheDesc = {};
    void*                    pBlob = nullptr;
    FileStream               stream = {};
    if (OpenStreamFromPath(RD_SHADER_BINARIES, kPipelineCacheFile, FM_READ, &stream))
    {
        PipelineCacheHeader header = {};
        const ssize_t       fileSize = GetStreamFileSize(&stream);
        const bool          headerRead =
            fileSize > (ssize_t)sizeof(header) && ReadFromStream(&stream, &header, sizeof(header)) == sizeof(header);
        if (headerRead && memcmp(&header, &expected, sizeof(header)) == 0)
        {
            const size_t blobSize = (size_t)fileSize - sizeof(header);
            pBlob = tf_malloc(blobSize);
            if (pBlob && ReadFromStream(&stream, pBlob, blobSize) == blobSize)
            {
                cacheDesc.pData = pBlob;
                cacheDesc.size = blobSize;
            }
        }
        CloseStream(&stream);
    }
    addPipelineCache(pRenderer, &cacheDesc, &pPipelineCache);
    tf_free(pBlob);
    return pPipelineCache != nullptr;
}

void RenderContext::savePipelineCache()
{
    if (!pRenderer || !pPipelineCache)
        return;
    size_t blobSize = 0;
    getPipelineCacheData(pRenderer, pPipelineCache, &blobSize, nullptr);
    if (!blobSize)
        return;
    uint8_t* pBlob = (uint8_t*)tf_malloc(blobSize);
    if (!pBlob)
        return;
    getPipelineCacheData(pRenderer, pPipelineCache, &blobSize, pBlob);

    const PipelineCacheHeader header = makeCacheHeader(pRenderer->pGpu->settings);
    FileStream                stream = {};
    if (OpenStreamFromPath(RD_SHADER_BINARIES, kPipelineCacheFile, FM_WRITE, &stream))
    {
        WriteToStream(&stream, &header, sizeof(header));
        WriteToStream(&stream, pBlob, blobSize);
        CloseStream(&stream);
    }
    tf_free(pBlob);
}
} // namespace hz
