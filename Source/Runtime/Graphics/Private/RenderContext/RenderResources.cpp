/* Copyright (c) 2026 Horizon */

#include "Graphics/RenderContext.h"
#include "Resources/IResourceLoader.h"

#include <stdio.h>
#include <string.h>

#include "Core/ILog.h"

#include "Core/IMemory.h" //NOTE: this should be the last include in a .cpp

namespace hz
{
GPUBuffer::GPUBuffer(RenderContext* context, Buffer* buffer, uint64_t bufferSize, ResourceMemoryUsage MemoryUsage,
                     DescriptorType bufferDescriptors, ResourceState initialState):
    pContext(context),
    pBuffer(buffer), Size(bufferSize), usage(MemoryUsage), Descriptors(bufferDescriptors), state(initialState)
{
    ASSERT(pContext && pBuffer);
    ++pContext->resourceCount;
}

GPUBuffer::~GPUBuffer() { Destroy(); }

GPUBuffer::GPUBuffer(GPUBuffer&& other) noexcept:
    pContext(other.pContext), pBuffer(other.pBuffer), Size(other.Size), usage(other.usage), Descriptors(other.Descriptors),
    state(other.state)
{
    other.pContext = nullptr;
    other.pBuffer = nullptr;
    other.Size = 0;
    other.usage = MemoryUsageUnknown;
    other.Descriptors = DescriptorTypeUndefined;
    other.state = ResourceStateUndefined;
}

GPUBuffer& GPUBuffer::operator=(GPUBuffer&& other) noexcept
{
    if (this != &other)
    {
        Destroy();
        pContext = other.pContext;
        pBuffer = other.pBuffer;
        Size = other.Size;
        usage = other.usage;
        Descriptors = other.Descriptors;
        state = other.state;
        other.pContext = nullptr;
        other.pBuffer = nullptr;
        other.Size = 0;
        other.usage = MemoryUsageUnknown;
        other.Descriptors = DescriptorTypeUndefined;
        other.state = ResourceStateUndefined;
    }
    return *this;
}

void GPUBuffer::Destroy()
{
    ASSERT((pContext != nullptr) == (pBuffer != nullptr));
    if (pBuffer)
    {
        ASSERT(pContext->resourceCount);
        pContext->DeferResource(RenderContext::ResourceType::Buffer, pBuffer);
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

GPUTexture::~GPUTexture() { Destroy(); }

GPUTexture::GPUTexture(GPUTexture&& other) noexcept:
    pContext(other.pContext), pTexture(other.pTexture), pRenderTarget(other.pRenderTarget), state(other.state), owned(other.owned)
{
    other.pContext = nullptr;
    other.pTexture = nullptr;
    other.pRenderTarget = nullptr;
    other.state = ResourceStateUndefined;
    other.owned = false;
}

GPUTexture& GPUTexture::operator=(GPUTexture&& other) noexcept
{
    if (this != &other)
    {
        Destroy();
        pContext = other.pContext;
        pTexture = other.pTexture;
        pRenderTarget = other.pRenderTarget;
        state = other.state;
        owned = other.owned;
        other.pContext = nullptr;
        other.pTexture = nullptr;
        other.pRenderTarget = nullptr;
        other.state = ResourceStateUndefined;
        other.owned = false;
    }
    return *this;
}

void GPUTexture::Destroy()
{
    if (pTexture && owned)
    {
        ASSERT(pContext && pContext->resourceCount);
        pContext->DeferResource(pRenderTarget ? RenderContext::ResourceType::RenderTarget : RenderContext::ResourceType::Texture,
                                pRenderTarget ? (void*)pRenderTarget : (void*)pTexture);
    }
}

GPUSampler::GPUSampler(RenderContext* context, Sampler* sampler): pContext(context), pSampler(sampler)
{
    ASSERT(pContext && pSampler);
    ++pContext->resourceCount;
}

GPUSampler::~GPUSampler() { Destroy(); }

GPUSampler::GPUSampler(GPUSampler&& other) noexcept: pContext(other.pContext), pSampler(other.pSampler)
{
    other.pContext = nullptr;
    other.pSampler = nullptr;
}

GPUSampler& GPUSampler::operator=(GPUSampler&& other) noexcept
{
    if (this != &other)
    {
        Destroy();
        pContext = other.pContext;
        pSampler = other.pSampler;
        other.pContext = nullptr;
        other.pSampler = nullptr;
    }
    return *this;
}

void GPUSampler::Destroy()
{
    ASSERT((pContext != nullptr) == (pSampler != nullptr));
    if (pSampler)
    {
        ASSERT(pContext->resourceCount);
        pContext->DeferResource(RenderContext::ResourceType::Sampler, pSampler);
    }
}

GPUShader::GPUShader(RenderContext* context, Shader* shader): pContext(context), pShader(shader)
{
    ASSERT(pContext && pShader);
    ++pContext->resourceCount;
}

GPUShader::~GPUShader() { Destroy(); }

GPUShader::GPUShader(GPUShader&& other) noexcept: pContext(other.pContext), pShader(other.pShader)
{
    other.pContext = nullptr;
    other.pShader = nullptr;
}

GPUShader& GPUShader::operator=(GPUShader&& other) noexcept
{
    if (this != &other)
    {
        Destroy();
        pContext = other.pContext;
        pShader = other.pShader;
        other.pContext = nullptr;
        other.pShader = nullptr;
    }
    return *this;
}

void GPUShader::Destroy()
{
    ASSERT((pContext != nullptr) == (pShader != nullptr));
    if (pShader)
    {
        ASSERT(pContext->resourceCount);
        pContext->DeferResource(RenderContext::ResourceType::Shader, pShader);
    }
}

GPUPipeline::GPUPipeline(RenderContext* context, Pipeline* pipeline, RootSignature* rootSignature, GPUShader&& shader,
                         CommandSignature* drawSignature):
    pContext(context), pPipeline(pipeline), pRootSignature(rootSignature), pDrawIndirectSignature(drawSignature), shader(std::move(shader))
{
    ASSERT(pContext && pPipeline && pRootSignature);
    ++pContext->resourceCount;
}

GPUPipeline::~GPUPipeline() { Destroy(); }

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
        Destroy();
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

void GPUPipeline::Destroy()
{
    ASSERT((pContext != nullptr) == (pPipeline != nullptr));
    if (pPipeline)
    {
        ASSERT(pContext->resourceCount && pRootSignature);
        pContext->DeferResource(RenderContext::ResourceType::Pipeline, pPipeline, pRootSignature, pDrawIndirectSignature);
    }
}

GPUBuffer RenderContext::createBuffer(const BufferDesc& input)
{
    ASSERT(ready && input.Size && input.initialDataSize <= input.Size && ((input.pInitialData != nullptr) == (input.initialDataSize != 0)));

    ::BufferDesc desc = {};
    desc.Size = input.Size;
    desc.ElementCount = input.ElementCount;
    desc.StructStride = input.StructStride;
    desc.Name = input.Name;
    desc.QueueType = QueueTypeGraphics;
    desc.MemoryUsage = input.usage;
    desc.StartState = input.StartState;
    desc.Descriptors = input.Descriptors;
    desc.Flags = input.Flags;

    Buffer*        buffer = nullptr;
    SyncToken      token = 0;
    BufferLoadDesc load = { .ppBuffer = &buffer, .Data = input.pInitialData, .desc = desc };
    addResource(&load, &token);
    ASSERT(buffer);
    waitForToken(&token);
    return GPUBuffer(this, buffer, input.Size, input.usage, input.Descriptors, input.StartState);
}

GPUTexture RenderContext::createTexture(const TextureDesc& input)
{
    ASSERT(ready && input.Width && input.Height && input.Depth && input.ArraySize && input.MipLevels);

    Texture*      texture = nullptr;
    RenderTarget* renderTarget = nullptr;

    if (input.renderTarget)
    {
        const RenderTargetDesc desc = {
            .Flags = input.Flags,
            .Width = input.Width,
            .Height = input.Height,
            .Depth = input.Depth,
            .ArraySize = input.ArraySize,
            .MipLevels = input.MipLevels,
            .SampleCount = input.SampleCount,
            .Format = input.Format,
            .StartState = input.StartState,
            .Descriptors = input.Descriptors,
            .Name = input.Name,
        };
        addRenderTarget(pRenderer, &desc, &renderTarget);
        ASSERT(renderTarget);
        texture = renderTarget->pTexture;
    }
    else
    {
        ::TextureDesc desc = {};
        desc.Width = input.Width;
        desc.Height = input.Height;
        desc.Depth = input.Depth;
        desc.ArraySize = input.ArraySize;
        desc.MipLevels = input.MipLevels;
        desc.SampleCount = input.SampleCount;
        desc.Format = input.Format;
        desc.StartState = input.StartState;
        desc.Descriptors = input.Descriptors;
        desc.Flags = input.Flags;
        desc.Name = input.Name;
        SyncToken       token = 0;
        TextureLoadDesc load = { .ppTexture = &texture, .pDesc = &desc };
        addResource(&load, &token);
        ASSERT(texture);
        waitForToken(&token);
    }
    return GPUTexture(this, texture, renderTarget, input.StartState);
}

uint32_t GPUTexture::GetWidth() const { return pTexture ? pTexture->Width : 0; }
uint32_t GPUTexture::GetHeight() const { return pTexture ? pTexture->Height : 0; }
uint32_t GPUTexture::GetArraySize() const { return pTexture ? pTexture->arraySizeMinusOne + 1 : 0; }
uint32_t GPUTexture::GetMipLevels() const { return pTexture ? pTexture->MipLevels : 0; }
Format   GPUTexture::GetFormat() const { return pTexture ? pTexture->Format : Format::UNDEFINED; }

static bool sameTextureDesc(const TextureDesc& a, const TextureDesc& b)
{
    return a.Width == b.Width && a.Height == b.Height && a.Depth == b.Depth && a.ArraySize == b.ArraySize &&
           a.MipLevels == b.MipLevels && a.SampleCount == b.SampleCount && a.Format == b.Format && a.StartState == b.StartState &&
           a.Descriptors == b.Descriptors && a.Flags == b.Flags && a.renderTarget == b.renderTarget;
}

GPUTexture* RenderContext::acquireTransient(const TextureDesc& input)
{
    ASSERT(ready && input.Width && input.Height);
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

GPUTexture RenderContext::loadTexture(ResourceDirectory directory, const char* pFileName, TextureCreationFlags Flags)
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
                             .creationFlag = Flags,
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
    return GPUTexture(this, texture, nullptr, ResourceStateShaderResource);
}

GPUSampler RenderContext::createSampler(const SamplerDesc& input)
{
    ASSERT(ready);
    const ::SamplerDesc samplerDesc = {
        .MinFilter = input.MinFilter,
        .MagFilter = input.MagFilter,
        .MipMapMode = input.MipMapMode,
        .AddressU = input.AddressU,
        .AddressV = input.AddressV,
        .AddressW = input.AddressW,
        .MipLodBias = input.MipLodBias,
        .SetLodRange = input.SetLodRange,
        .MinLod = input.MinLod,
        .MaxLod = input.MaxLod,
        .MaxAnisotropy = input.MaxAnisotropy,
        .CompareFunc = input.CompareFunc,
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
    if (input.pFileName && (!OpenStreamFromPath(input.sourceDirectory, input.pFileName, FileModeRead, &sourceFile) ||
                            !StreamMemoryMap(&sourceFile, &fileSize, &pFileSource) || !fileSize || fileSize > UINT32_MAX))
    {
        LOGF(Error, "Could not map shader source '%s'", input.pFileName);
        if (sourceFile.FileSystem)
            CloseStream(&sourceFile);
        return {};
    }
    if (pFileSource)
        snprintf(sourcePath, sizeof(sourcePath), "%s/%s", GetResourceDirectory(input.sourceDirectory), input.pFileName);
    uint32_t stageCount = input.stages.Count;
    for (uint32_t i = 0; i < stageCount; ++i)
    {
        const ShaderStageDesc& inputStage = *(input.stages.Data + i);
        ShaderSrcStageDesc*    stage = nullptr;
        switch (inputStage.stage)
        {
        case ShaderStageVert:
            stage = &source.vert;
            break;
        case ShaderStageFrag:
            stage = &source.frag;
            break;
        case ShaderStageComp:
            stage = &source.comp;
            break;
        default:
            ASSERT(false);
            break;
        }
        source.stages |= inputStage.stage;
        stage->Name = pFileSource ? sourcePath : inputStage.Name;
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
    if (sourceFile.FileSystem)
        CloseStream(&sourceFile);
    ASSERT(shader);
    return shader;
}

GPUPipeline RenderContext::createGraphicsPipeline(const GraphicsPipelineDesc& input)
{
    ASSERT(ready && input.colorTargets.Count <= MAX_RENDER_TARGETS && (input.colorTargets.Data || !input.colorTargets.Count));
    Shader* shader = this->createShader(input.shaderDesc);
    ASSERT(shader);
    Shader*           shaders[] = { shader };
    RootSignatureDesc rootDesc = { .ppShaders = shaders, .shaderCount = 1 };
    RootSignature*    rootSignature = nullptr;
    addRootSignature(pRenderer, &rootDesc, &rootSignature);
    ASSERT(rootSignature);

    VertexLayout        vertex = input.vertexLayout;
    RasterizerStateDesc raster = input.rasterizer;
    DepthStateDesc      Depth = input.Depth;
    BlendStateDesc      blend = {
        .renderTargetMask = (BlendStateTargets)((1u << input.colorTargets.Count) - 1),
        .alphaToCoverage = input.alphaToCoverage,
        .independentBlend = true,
    };
    hz::Format formats[MAX_RENDER_TARGETS] = {};
    for (uint32_t i = 0; i < input.colorTargets.Count; ++i)
    {
        const ColorTargetDesc& target = input.colorTargets.Data[i];
        formats[i] = target.Format;
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
            .pDepthState = &Depth,
            .pRasterizerState = &raster,
            .pColorFormats = formats,
            .renderTargetCount = input.colorTargets.Count,
            .SampleCount = input.SampleCount,
            .depthStencilFormat = input.depthStencilFormat,
            .primitiveTopo = input.topology,
        },
        .pCache = pPipelineCache,
        .Name = input.Name,
        .Type = PIPELINE_TYPE_GRAPHICS,
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
            { .Type = INDIRECT_CONSTANT, .index = constant, .byteSize = sizeof(uint32_t) },
            { .Type = INDIRECT_DRAW },
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
        .Name = input.Name,
        .Type = PIPELINE_TYPE_COMPUTE,
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

bool RenderContext::updateBuffer(const GPUBuffer& buffer, uint64_t offset, const void* Data, uint64_t Size)
{
    ASSERT(ready && buffer.pBuffer && Data && Size);
    ASSERT(buffer.usage == MemoryUsageCpuToGpu && offset <= buffer.Size && Size <= buffer.Size - offset &&
           buffer.pBuffer->pCpuMappedAddress);
    if (buffer.usage != MemoryUsageCpuToGpu || offset > buffer.Size || Size > buffer.Size - offset ||
        !buffer.pBuffer->pCpuMappedAddress)
        return false;
    memcpy((uint8_t*)buffer.pBuffer->pCpuMappedAddress + offset, Data, (size_t)Size);
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
    if (OpenStreamFromPath(DirectoryShaderBinaries, kPipelineCacheFile, FileModeRead, &stream))
    {
        PipelineCacheHeader header = {};
        const ssize_t       fileSize = GetStreamFileSize(&stream);
        const bool          headerRead =
            fileSize > (ssize_t)sizeof(header) && ReadFromStream(&stream, &header, sizeof(header)) == sizeof(header);
        if (headerRead && memcmp(&header, &expected, sizeof(header)) == 0)
        {
            const size_t blobSize = (size_t)fileSize - sizeof(header);
            pBlob = MemAllocate(blobSize);
            if (pBlob && ReadFromStream(&stream, pBlob, blobSize) == blobSize)
            {
                cacheDesc.Data = pBlob;
                cacheDesc.Size = blobSize;
            }
        }
        CloseStream(&stream);
    }
    addPipelineCache(pRenderer, &cacheDesc, &pPipelineCache);
    MemDeallocate(pBlob);
    return pPipelineCache != nullptr;
}

void RenderContext::SavePipelineCache()
{
    if (!pRenderer || !pPipelineCache)
        return;
    size_t blobSize = 0;
    getPipelineCacheData(pRenderer, pPipelineCache, &blobSize, nullptr);
    if (!blobSize)
        return;
    uint8_t* pBlob = (uint8_t*)MemAllocate(blobSize);
    if (!pBlob)
        return;
    getPipelineCacheData(pRenderer, pPipelineCache, &blobSize, pBlob);

    const PipelineCacheHeader header = makeCacheHeader(pRenderer->pGpu->settings);
    FileStream                stream = {};
    if (OpenStreamFromPath(DirectoryShaderBinaries, kPipelineCacheFile, FileModeWrite, &stream))
    {
        WriteToStream(&stream, &header, sizeof(header));
        WriteToStream(&stream, pBlob, blobSize);
        CloseStream(&stream);
    }
    MemDeallocate(pBlob);
}
} // namespace hz
