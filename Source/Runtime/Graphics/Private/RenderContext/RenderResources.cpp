/* Copyright (c) 2026 Horizon */

#include "Graphics/RenderContext.h"
#include "Resources/IResourceLoader.h"
#include "TextureDescs.h"

#include <stdio.h>
#include <string.h>

#include "Core/ILog.h"

#include "Core/IMemory.h" //NOTE: this should be the last include in a .cpp

namespace hz
{
GPUBuffer::GPUBuffer(RenderContext* Context, Buffer* BufferValue, uint64_t BufferSize, ResourceMemoryUsage MemoryUsage,
                     DescriptorType BufferDescriptors, ResourceState InitialState):
    pContext(Context), pBuffer(BufferValue), Size(BufferSize), usage(MemoryUsage), Descriptors(BufferDescriptors), state(InitialState)
{
    ASSERT(pContext && pBuffer);
    ++pContext->resourceCount;
}

GPUBuffer::~GPUBuffer() { Destroy(); }

GPUBuffer::GPUBuffer(GPUBuffer&& Other) noexcept:
    pContext(Other.pContext), pBuffer(Other.pBuffer), Size(Other.Size), usage(Other.usage), Descriptors(Other.Descriptors),
    state(Other.state)
{
    Other.pContext = nullptr;
    Other.pBuffer = nullptr;
    Other.Size = 0;
    Other.usage = RESOURCE_MEMORY_USAGE_UNKNOWN;
    Other.Descriptors = DESCRIPTOR_TYPE_UNDEFINED;
    Other.state = RESOURCE_STATE_UNDEFINED;
}

GPUBuffer& GPUBuffer::operator=(GPUBuffer&& Other) noexcept
{
    if (this != &Other)
    {
        Destroy();
        pContext = Other.pContext;
        pBuffer = Other.pBuffer;
        Size = Other.Size;
        usage = Other.usage;
        Descriptors = Other.Descriptors;
        state = Other.state;
        Other.pContext = nullptr;
        Other.pBuffer = nullptr;
        Other.Size = 0;
        Other.usage = RESOURCE_MEMORY_USAGE_UNKNOWN;
        Other.Descriptors = DESCRIPTOR_TYPE_UNDEFINED;
        Other.state = RESOURCE_STATE_UNDEFINED;
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

GPUTexture::GPUTexture(RenderContext* Context, Texture* TextureValue, RenderTarget* GetRenderTarget, ResourceState InitialState,
                       bool OwnsResource):
    pContext(Context), pTexture(TextureValue), pRenderTarget(GetRenderTarget), state(InitialState), owned(OwnsResource)
{
    ASSERT(pTexture);
    if (owned)
    {
        ASSERT(pContext);
        ++pContext->resourceCount;
    }
}

GPUTexture::~GPUTexture() { Destroy(); }

GPUTexture::GPUTexture(GPUTexture&& Other) noexcept:
    pContext(Other.pContext), pTexture(Other.pTexture), pRenderTarget(Other.pRenderTarget), state(Other.state), owned(Other.owned)
{
    Other.pContext = nullptr;
    Other.pTexture = nullptr;
    Other.pRenderTarget = nullptr;
    Other.state = RESOURCE_STATE_UNDEFINED;
    Other.owned = false;
}

GPUTexture& GPUTexture::operator=(GPUTexture&& Other) noexcept
{
    if (this != &Other)
    {
        Destroy();
        pContext = Other.pContext;
        pTexture = Other.pTexture;
        pRenderTarget = Other.pRenderTarget;
        state = Other.state;
        owned = Other.owned;
        Other.pContext = nullptr;
        Other.pTexture = nullptr;
        Other.pRenderTarget = nullptr;
        Other.state = RESOURCE_STATE_UNDEFINED;
        Other.owned = false;
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

GPUSampler::GPUSampler(RenderContext* Context, Sampler* SamplerValue): pContext(Context), pSampler(SamplerValue)
{
    ASSERT(pContext && pSampler);
    ++pContext->resourceCount;
}

GPUSampler::~GPUSampler() { Destroy(); }

GPUSampler::GPUSampler(GPUSampler&& Other) noexcept: pContext(Other.pContext), pSampler(Other.pSampler)
{
    Other.pContext = nullptr;
    Other.pSampler = nullptr;
}

GPUSampler& GPUSampler::operator=(GPUSampler&& Other) noexcept
{
    if (this != &Other)
    {
        Destroy();
        pContext = Other.pContext;
        pSampler = Other.pSampler;
        Other.pContext = nullptr;
        Other.pSampler = nullptr;
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

GPUShader::GPUShader(RenderContext* Context, Shader* ShaderObject): pContext(Context), pShader(ShaderObject)
{
    ASSERT(pContext && pShader);
    ++pContext->resourceCount;
}

GPUShader::~GPUShader() { Destroy(); }

GPUShader::GPUShader(GPUShader&& Other) noexcept: pContext(Other.pContext), pShader(Other.pShader)
{
    Other.pContext = nullptr;
    Other.pShader = nullptr;
}

GPUShader& GPUShader::operator=(GPUShader&& Other) noexcept
{
    if (this != &Other)
    {
        Destroy();
        pContext = Other.pContext;
        pShader = Other.pShader;
        Other.pContext = nullptr;
        Other.pShader = nullptr;
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

GPUPipeline::GPUPipeline(RenderContext* Context, Pipeline* PipelineObject, RootSignature* RootSignatureValue, GPUShader&& ShaderObject,
                         CommandSignature* DrawSignature):
    pContext(Context), pPipeline(PipelineObject), pRootSignature(RootSignatureValue), pDrawIndirectSignature(DrawSignature),
    shader(std::move(ShaderObject))
{
    ASSERT(pContext && pPipeline && pRootSignature);
    ++pContext->resourceCount;
}

GPUPipeline::~GPUPipeline() { Destroy(); }

GPUPipeline::GPUPipeline(GPUPipeline&& Other) noexcept:
    pContext(Other.pContext), pPipeline(Other.pPipeline), pRootSignature(Other.pRootSignature),
    pDrawIndirectSignature(Other.pDrawIndirectSignature), shader(std::move(Other.shader))
{
    Other.pContext = nullptr;
    Other.pPipeline = nullptr;
    Other.pRootSignature = nullptr;
    Other.pDrawIndirectSignature = nullptr;
}

GPUPipeline& GPUPipeline::operator=(GPUPipeline&& Other) noexcept
{
    if (this != &Other)
    {
        Destroy();
        pContext = Other.pContext;
        pPipeline = Other.pPipeline;
        pRootSignature = Other.pRootSignature;
        pDrawIndirectSignature = Other.pDrawIndirectSignature;
        shader = std::move(Other.shader);
        Other.pContext = nullptr;
        Other.pPipeline = nullptr;
        Other.pRootSignature = nullptr;
        Other.pDrawIndirectSignature = nullptr;
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

GPUBuffer RenderContext::CreateBuffer(const BufferDesc& Input)
{
    ASSERT(ready && Input.Size && Input.initialDataSize <= Input.Size && ((Input.pInitialData != nullptr) == (Input.initialDataSize != 0)));

    ::BufferDesc Desc = {};
    Desc.Size = Input.Size;
    Desc.ElementCount = Input.ElementCount;
    Desc.StructStride = Input.StructStride;
    Desc.Name = Input.Name;
    Desc.QueueType = QUEUE_TYPE_GRAPHICS;
    Desc.MemoryUsage = Input.usage;
    Desc.StartState = Input.StartState;
    Desc.Descriptors = Input.Descriptors;
    Desc.Flags = Input.Flags;

    Buffer*        BufferValue = nullptr;
    SyncToken      Token = 0;
    BufferLoadDesc Load = { .ppBuffer = &BufferValue, .Data = Input.pInitialData, .desc = Desc };
    addResource(&Load, &Token);
    ASSERT(BufferValue);
    waitForToken(&Token);
    return GPUBuffer(this, BufferValue, Input.Size, Input.usage, Input.Descriptors, Input.StartState);
}

GPUTexture RenderContext::CreateTexture(const TextureDesc& Input)
{
    ASSERT(ready && Input.Width && Input.Height && Input.Depth && Input.ArraySize && Input.MipLevels);

    Texture*      TextureValue = nullptr;
    RenderTarget* GetRenderTarget = nullptr;

    if (Input.GetRenderTarget)
    {
        const RenderTargetDesc Desc = {
            .Flags = Input.Flags,
            .Width = Input.Width,
            .Height = Input.Height,
            .Depth = Input.Depth,
            .ArraySize = Input.ArraySize,
            .MipLevels = Input.MipLevels,
            .SampleCount = Input.SampleCount,
            .Format = Input.Format,
            .StartState = Input.StartState,
            .Descriptors = Input.Descriptors,
            .Name = Input.Name,
        };
        addRenderTarget(pRenderer, &Desc, &GetRenderTarget);
        ASSERT(GetRenderTarget);
        TextureValue = GetRenderTarget->pTexture;
    }
    else
    {
        ::TextureDesc Desc = {};
        Desc.Width = Input.Width;
        Desc.Height = Input.Height;
        Desc.Depth = Input.Depth;
        Desc.ArraySize = Input.ArraySize;
        Desc.MipLevels = Input.MipLevels;
        Desc.SampleCount = Input.SampleCount;
        Desc.Format = Input.Format;
        Desc.StartState = Input.StartState;
        Desc.Descriptors = Input.Descriptors;
        Desc.Flags = Input.Flags;
        Desc.Name = Input.Name;
        SyncToken       Token = 0;
        TextureLoadDesc Load = { .ppTexture = &TextureValue, .pDesc = &Desc };
        addResource(&Load, &Token);
        ASSERT(TextureValue);
        waitForToken(&Token);
    }
    return GPUTexture(this, TextureValue, GetRenderTarget, Input.StartState);
}

uint32_t GPUTexture::GetWidth() const { return pTexture ? pTexture->Width : 0; }
uint32_t GPUTexture::GetHeight() const { return pTexture ? pTexture->Height : 0; }
bool     GPUTexture::IsCube() const { return pTexture && pTexture->cube; }
uint32_t GPUTexture::GetArraySize() const { return pTexture ? pTexture->arraySizeMinusOne + 1 : 0; }
uint32_t GPUTexture::GetMipLevels() const { return pTexture ? pTexture->MipLevels : 0; }
Format   GPUTexture::GetFormat() const { return pTexture ? pTexture->Format : Format::UNDEFINED; }

GPUTexture* RenderContext::AcquireTransient(const TextureDesc& Input)
{
    ASSERT(ready && Input.Width && Input.Height);
    uint32_t FreeSlot = transientCapacity;
    for (uint32_t I = 0; I < transientCapacity; ++I)
    {
        TransientSlot& Slot = transientSlots[I];
        if (!Slot.used)
        {
            if (FreeSlot == transientCapacity)
            {
                FreeSlot = I;
            }
            continue;
        }
        if (Slot.inUse || !SameTextureDesc(Slot.desc, Input))
        {
            continue;
        }
        if (Slot.useId && !IsResourceUseComplete(Slot.useId))
        {
            continue;
        }
        Slot.inUse = true;
        return &Slot.texture;
    }
    if (FreeSlot == transientCapacity)
    {
        ASSERT(false);
        return nullptr;
    }
    TransientSlot& Slot = transientSlots[FreeSlot];
    Slot.texture = CreateTexture(Input);
    Slot.desc = Input;
    Slot.useId = 0;
    Slot.inUse = true;
    Slot.used = true;
    return &Slot.texture;
}

void RenderContext::ReleaseTransient(GPUTexture* TexturePointer)
{
    for (uint32_t I = 0; I < transientCapacity; ++I)
    {
        if (&transientSlots[I].texture != TexturePointer)
        {
            continue;
        }
        ASSERT(transientSlots[I].inUse);
        transientSlots[I].inUse = false;
        transientSlots[I].useId = GetResourceUseId();
        return;
    }
    ASSERT(false);
}

void RenderContext::PurgeTransients()
{
    for (TransientSlot& Slot : transientSlots)
    {
        if (!Slot.used || Slot.inUse)
        {
            continue;
        }
        Slot.texture = GPUTexture{};
        Slot.used = false;
        Slot.useId = 0;
    }
}

void RenderContext::ReleaseTransientSlots()
{
    for (TransientSlot& Slot : transientSlots)
    {
        if (!Slot.used)
        {
            continue;
        }
        ASSERT(!Slot.inUse);
        Slot.texture = GPUTexture{};
        Slot.used = false;
        Slot.useId = 0;
    }
}

GPUTexture RenderContext::LoadTexture(ResourceDirectory Directory, const char* FileName, TextureCreationFlags Flags)
{
    ASSERT(ready && FileName);
    const char* Extension = strrchr(FileName, '.');
    if (!Extension || (strcmp(Extension, ".dds") && strcmp(Extension, ".ktx")))
    {
        LOGF(Error, "Texture '%s' requires a DDS or KTX container", FileName);
        return {};
    }
    Texture*        TextureValue = nullptr;
    bool            Success = false;
    TextureLoadDesc Desc = { .ppTexture = &TextureValue,
                             .pFileName = FileName,
                             .creationFlag = Flags,
                             .container = strcmp(Extension, ".ktx") == 0 ? TEXTURE_CONTAINER_KTX : TEXTURE_CONTAINER_DDS,
                             .resourceDirectory = Directory,
                             .pSuccess = &Success };
    SyncToken       Token = 0;
    addResource(&Desc, &Token);
    waitForToken(&Token);
    if (!Success || !TextureValue)
    {
        if (TextureValue)
        {
            removeResource(TextureValue);
        }
        LOGF(Error, "Failed to Load texture '%s'", FileName);
        return {};
    }
    return GPUTexture(this, TextureValue, nullptr, RESOURCE_STATE_SHADER_RESOURCE);
}

GPUSampler RenderContext::CreateSampler(const SamplerDesc& Input)
{
    ASSERT(ready);
    const ::SamplerDesc SamplerDescValue = {
        .MinFilter = Input.MinFilter,
        .MagFilter = Input.MagFilter,
        .MipMapMode = Input.MipMapMode,
        .AddressU = Input.AddressU,
        .AddressV = Input.AddressV,
        .AddressW = Input.AddressW,
        .MipLodBias = Input.MipLodBias,
        .SetLodRange = Input.SetLodRange,
        .MinLod = Input.MinLod,
        .MaxLod = Input.MaxLod,
        .MaxAnisotropy = Input.MaxAnisotropy,
        .CompareFunc = Input.CompareFunc,
    };
    Sampler* SamplerValue = nullptr;
    addSampler(pRenderer, &SamplerDescValue, &SamplerValue);
    ASSERT(SamplerValue);
    return GPUSampler(this, SamplerValue);
}

Shader* RenderContext::CreateShader(const ShaderDesc& Input)
{
    ASSERT(ready);
    ShaderSrcDesc Source = {};
    FileStream    SourceFile = {};
    const void*   FileSource = nullptr;
    size_t        FileSize = 0;
    char          SourcePath[FS_MAX_PATH] = {};
    if (Input.pFileName && (!OpenStreamFromPath(Input.sourceDirectory, Input.pFileName, FileModeRead, &SourceFile) ||
                            !StreamMemoryMap(&SourceFile, &FileSize, &FileSource) || !FileSize || FileSize > UINT32_MAX))
    {
        LOGF(Error, "Could not map shader source '%s'", Input.pFileName);
        if (SourceFile.FileSystem)
        {
            CloseStream(&SourceFile);
        }
        return {};
    }
    if (FileSource)
    {
        snprintf(SourcePath, sizeof(SourcePath), "%s/%s", GetResourceDirectory(Input.sourceDirectory), Input.pFileName);
    }
    uint32_t StageCount = Input.stages.Count;
    for (uint32_t I = 0; I < StageCount; ++I)
    {
        const ShaderStageDesc& InputStage = *(Input.stages.Data + I);
        ShaderSrcStageDesc*    Stage = nullptr;
        switch (InputStage.stage)
        {
        case SHADER_STAGE_VERT:
            Stage = &Source.vert;
            break;
        case SHADER_STAGE_FRAG:
            Stage = &Source.frag;
            break;
        case SHADER_STAGE_COMP:
            Stage = &Source.comp;
            break;
        default:
            ASSERT(false);
            break;
        }
        Source.stages |= InputStage.stage;
        Stage->Name = FileSource ? SourcePath : InputStage.Name;
        Stage->pByteCode = (void*)(FileSource ? FileSource : InputStage.pSource);
        Stage->byteCodeSize = FileSource ? (uint32_t)FileSize : InputStage.sourceSize;
        Stage->pEntryPoint = InputStage.pEntryPoint;
        ASSERT(Stage->pByteCode && Stage->byteCodeSize && Stage->pEntryPoint && Stage->pEntryPoint[0]);
    }

    Shader*     ShaderObject = nullptr;
    const char* Extension = Input.pFileName ? strrchr(Input.pFileName, '.') : nullptr;
    if (Input.language == ShaderLanguage::Slang ||
        (Input.language == ShaderLanguage::Auto && Extension && strcmp(Extension, ".slang") == 0))
    {
        ShaderObject = CreateSlangShader(Source);
    }
    else
        addShaderSource(pRenderer, &Source, &ShaderObject);
    if (SourceFile.FileSystem)
    {
        CloseStream(&SourceFile);
    }
    ASSERT(ShaderObject);
    return ShaderObject;
}

GPUPipeline RenderContext::CreateGraphicsPipeline(const GraphicsPipelineDesc& Input)
{
    ASSERT(ready && Input.colorTargets.Count <= MAX_RENDER_TARGETS && (Input.colorTargets.Data || !Input.colorTargets.Count));
    Shader* ShaderObject = this->CreateShader(Input.shaderDesc);
    ASSERT(ShaderObject);
    Shader*           Shaders[] = { ShaderObject };
    RootSignatureDesc RootDesc = { .ppShaders = Shaders, .shaderCount = 1 };
    RootSignature*    RootSignatureValue = nullptr;
    addRootSignature(pRenderer, &RootDesc, &RootSignatureValue);
    ASSERT(RootSignatureValue);

    VertexLayout        Vertex = Input.vertexLayout;
    RasterizerStateDesc Raster = Input.rasterizer;
    DepthStateDesc      Depth = Input.Depth;
    BlendStateDesc      Blend = {
             .renderTargetMask = (BlendStateTargets)((1u << Input.colorTargets.Count) - 1),
             .alphaToCoverage = Input.alphaToCoverage,
             .independentBlend = true,
    };
    hz::Format Formats[MAX_RENDER_TARGETS] = {};
    for (uint32_t I = 0; I < Input.colorTargets.Count; ++I)
    {
        const ColorTargetDesc& Target = Input.colorTargets.Data[I];
        Formats[I] = Target.Format;
        Blend.srcFactors[I] = Target.srcFactor;
        Blend.dstFactors[I] = Target.dstFactor;
        Blend.srcAlphaFactors[I] = Target.srcAlphaFactor;
        Blend.dstAlphaFactors[I] = Target.dstAlphaFactor;
        Blend.blendModes[I] = Target.blendMode;
        Blend.blendAlphaModes[I] = Target.blendAlphaMode;
        Blend.colorWriteMasks[I] = Target.colorWriteMask;
    }
    const PipelineDesc Desc = {
        .graphicsDesc = {
            .pShaderProgram = ShaderObject,
            .pRootSignature = RootSignatureValue,
            .pVertexLayout = Vertex.bindingCount && Vertex.attribCount ? &Vertex : nullptr,
            .pBlendState = &Blend,
            .pDepthState = &Depth,
            .pRasterizerState = &Raster,
            .pColorFormats = Formats,
            .renderTargetCount = Input.colorTargets.Count,
            .SampleCount = Input.SampleCount,
            .depthStencilFormat = Input.depthStencilFormat,
            .primitiveTopo = Input.topology,
        },
        .pCache = pPipelineCache,
        .Name = Input.Name,
        .Type = PIPELINE_TYPE_GRAPHICS,
    };
    Pipeline* PipelineObject = nullptr;
    addPipeline(pRenderer, &Desc, &PipelineObject);
    ASSERT(PipelineObject);
    CommandSignature* DrawSignature = nullptr;
    if (Input.indirectDrawId)
    {
        const uint32_t Constant = getDescriptorIndexFromName(RootSignatureValue, "RootConstant0");
        ASSERT(Constant != UINT32_MAX);
        IndirectArgumentDescriptor Arguments[] = {
            { .Type = INDIRECT_CONSTANT, .index = Constant, .byteSize = sizeof(uint32_t) },
            { .Type = INDIRECT_DRAW },
        };
        const CommandSignatureDesc Signature = { .pRootSignature = RootSignatureValue,
                                                 .pArgDescs = Arguments,
                                                 .indirectArgCount = 2,
                                                 .packed = true };
        addIndirectCommandSignature(pRenderer, &Signature, &DrawSignature);
        ASSERT(DrawSignature);
    }
    return GPUPipeline(this, PipelineObject, RootSignatureValue, GPUShader(this, ShaderObject), DrawSignature);
}

GPUPipeline RenderContext::CreateComputePipeline(const ComputePipelineDesc& Input)
{
    ASSERT(ready);
    Shader* ShaderObject = this->CreateShader(Input.shaderDesc);
    ASSERT(ShaderObject);
    Shader*           Shaders[] = { ShaderObject };
    RootSignatureDesc RootDesc = { .ppShaders = Shaders, .shaderCount = 1 };
    RootSignature*    RootSignatureValue = nullptr;
    addRootSignature(pRenderer, &RootDesc, &RootSignatureValue);
    ASSERT(RootSignatureValue);

    const PipelineDesc Desc = {
        .computeDesc = { ShaderObject, RootSignatureValue },
        .pCache = pPipelineCache,
        .Name = Input.Name,
        .Type = PIPELINE_TYPE_COMPUTE,
    };
    Pipeline* PipelineObject = nullptr;
    addPipeline(pRenderer, &Desc, &PipelineObject);
    ASSERT(PipelineObject);
    return GPUPipeline(this, PipelineObject, RootSignatureValue, GPUShader(this, ShaderObject));
}

bool RenderContext::GetGpuAddress(const GPUBuffer& BufferValue, uint64_t* Address) const
{
    ASSERT(ready && BufferValue.pBuffer && Address);
    *Address = BufferValue.pBuffer->dx.gpuAddress;
    return true;
}

bool RenderContext::UpdateBuffer(const GPUBuffer& BufferValue, uint64_t Offset, const void* Data, uint64_t Size)
{
    ASSERT(ready && BufferValue.pBuffer && Data && Size);
    ASSERT(BufferValue.usage == RESOURCE_MEMORY_USAGE_CPU_TO_GPU && Offset <= BufferValue.Size && Size <= BufferValue.Size - Offset &&
           BufferValue.pBuffer->pCpuMappedAddress);
    if (BufferValue.usage != RESOURCE_MEMORY_USAGE_CPU_TO_GPU || Offset > BufferValue.Size || Size > BufferValue.Size - Offset ||
        !BufferValue.pBuffer->pCpuMappedAddress)
    {
        return false;
    }
    memcpy((uint8_t*)BufferValue.pBuffer->pCpuMappedAddress + Offset, Data, (size_t)Size);
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
PipelineCacheHeader MakeCacheHeader(const GPUSettings& SettingsValue)
{
    uint32_t DriverHash = 2166136261u;
    for (const char* P = SettingsValue.gpuVendorPreset.gpuDriverVersion; *P; ++P)
    {
        DriverHash = (DriverHash ^ (uint8_t)*P) * 16777619u;
    }
    return { .magic = kPipelineCacheMagic,
             .version = kPipelineCacheVersion,
             .vendorId = SettingsValue.gpuVendorPreset.vendorId,
             .deviceId = SettingsValue.gpuVendorPreset.modelId,
             .driverHash = DriverHash };
}
} // namespace

bool RenderContext::InitPipelineCache()
{
    const GPUSettings&        SettingsValue = pRenderer->pGpu->settings;
    const PipelineCacheHeader Expected = MakeCacheHeader(SettingsValue);
    PipelineCacheDesc         CacheDesc = {};
    void*                     Blob = nullptr;
    FileStream                Stream = {};
    if (OpenStreamFromPath(DirectoryShaderBinaries, kPipelineCacheFile, FileModeRead, &Stream))
    {
        PipelineCacheHeader Header = {};
        const ssize_t       FileSize = GetStreamFileSize(&Stream);
        const bool HeaderRead = FileSize > (ssize_t)sizeof(Header) && ReadFromStream(&Stream, &Header, sizeof(Header)) == sizeof(Header);
        if (HeaderRead && memcmp(&Header, &Expected, sizeof(Header)) == 0)
        {
            const size_t BlobSize = (size_t)FileSize - sizeof(Header);
            Blob = MemAllocate(BlobSize);
            if (Blob && ReadFromStream(&Stream, Blob, BlobSize) == BlobSize)
            {
                CacheDesc.Data = Blob;
                CacheDesc.Size = BlobSize;
            }
        }
        CloseStream(&Stream);
    }
    addPipelineCache(pRenderer, &CacheDesc, &pPipelineCache);
    MemDeallocate(Blob);
    return pPipelineCache != nullptr;
}

void RenderContext::SavePipelineCache()
{
    if (!pRenderer || !pPipelineCache)
    {
        return;
    }
    size_t BlobSize = 0;
    getPipelineCacheData(pRenderer, pPipelineCache, &BlobSize, nullptr);
    if (!BlobSize)
    {
        return;
    }
    uint8_t* Blob = (uint8_t*)MemAllocate(BlobSize);
    if (!Blob)
    {
        return;
    }
    getPipelineCacheData(pRenderer, pPipelineCache, &BlobSize, Blob);

    const PipelineCacheHeader Header = MakeCacheHeader(pRenderer->pGpu->settings);
    FileStream                Stream = {};
    if (OpenStreamFromPath(DirectoryShaderBinaries, kPipelineCacheFile, FileModeWrite, &Stream))
    {
        WriteToStream(&Stream, &Header, sizeof(Header));
        WriteToStream(&Stream, Blob, BlobSize);
        CloseStream(&Stream);
    }
    MemDeallocate(Blob);
}
} // namespace hz
