/* Copyright (c) 2026 Horizon */
#pragma once

#include "Core/IFileSystem.h"
#include "Core/ILog.h"
#include "Core/ISpan.h"
#include "RHI/IGraphics.h"
#include "Graphics/RayTracing.h"

#include <stdint.h>

namespace slang
{
struct IGlobalSession;
}

namespace hz
{
constexpr uint32_t MAX_SHADER_STAGES = 6;
constexpr uint32_t MAX_RENDER_TARGETS = MAX_RENDER_TARGET_ATTACHMENTS;

class CommandList;
class RenderContext;
class GPUBuffer;
class GPUTexture;
class GPUSampler;
class GPUShader;
class GPUPipeline;
struct SceneResourceAccess;

struct BufferDesc
{
    uint64_t            Size = 0;
    uint32_t            ElementCount = 0;
    uint32_t            StructStride = 0;
    const char*         Name = nullptr;
    const void*         pInitialData = nullptr;
    uint64_t            initialDataSize = 0;
    ResourceMemoryUsage usage = MemoryUsageUnknown;
    ResourceState       StartState = ResourceStateUndefined;
    DescriptorType      Descriptors = DescriptorTypeUndefined;
    BufferCreationFlags Flags = BufferCreationFlagNone;
};

struct TextureDesc
{
    uint32_t             Width = 0;
    uint32_t             Height = 0;
    uint32_t             Depth = 1;
    uint32_t             ArraySize = 1;
    uint32_t             MipLevels = 1;
    SampleCount          SampleCount = SAMPLE_COUNT_1;
    hz::Format           Format = hz::Format::UNDEFINED;
    ResourceState        StartState = ResourceStateUndefined;
    DescriptorType       Descriptors = DescriptorTypeUndefined;
    TextureCreationFlags Flags = TextureCreationFlagNone;
    bool                 GetRenderTarget = false;
    const char*          Name = nullptr;
};

struct SamplerDesc
{
    FilterType  MinFilter = FILTER_LINEAR;
    FilterType  MagFilter = FILTER_LINEAR;
    MipMapMode  MipMapMode = MIPMAP_MODE_LINEAR;
    AddressMode AddressU = AddressModeRepeat;
    AddressMode AddressV = AddressModeRepeat;
    AddressMode AddressW = AddressModeRepeat;
    float       MipLodBias = 0.0f;
    bool        SetLodRange = false;
    float       MinLod = 0.0f;
    float       MaxLod = 0.0f;
    float       MaxAnisotropy = 0.0f;
    CompareMode CompareFunc = CMP_NEVER;
};

struct ShaderStageDesc
{
    ShaderStage stage = ShaderStageNone;
    const void* pSource = nullptr;
    uint32_t    sourceSize = 0;
    const char* pEntryPoint = "main";
    const char* Name = nullptr;
};

enum class ShaderLanguage
{
    Auto,
    HLSL,
    Slang,
};

struct ShaderDesc
{
    hz::Span<ShaderStageDesc> stages = {};
    ResourceDirectory         sourceDirectory = DirectoryShaderSources;
    const char*               pFileName = nullptr;
    ShaderLanguage            language = ShaderLanguage::Auto;
};

struct ColorTargetDesc
{
    Format        Format = Format::UNDEFINED;
    BlendConstant srcFactor = BC_ONE;
    BlendConstant dstFactor = BC_ZERO;
    BlendConstant srcAlphaFactor = BC_ONE;
    BlendConstant dstAlphaFactor = BC_ZERO;
    BlendMode     blendMode = BM_ADD;
    BlendMode     blendAlphaMode = BM_ADD;
    ColorMask     colorWriteMask = COLOR_MASK_ALL;
};

struct GraphicsPipelineDesc
{
    ShaderDesc            shaderDesc = {};
    VertexLayout          vertexLayout = {};
    RasterizerStateDesc   rasterizer = { .cullMode = CULL_MODE_NONE, .fillMode = FILL_MODE_SOLID, .frontFace = FRONT_FACE_CCW };
    DepthStateDesc        Depth = { .depthTest = false, .depthWrite = false, .depthFunc = CMP_ALWAYS };
    Span<ColorTargetDesc> colorTargets = {};
    bool                  alphaToCoverage = false;
    hz::Format            depthStencilFormat = hz::Format::UNDEFINED;
    PrimitiveTopology     topology = PRIMITIVE_TOPO_TRI_LIST;
    SampleCount           SampleCount = SAMPLE_COUNT_1;
    bool                  indirectDrawId = false;
    const char*           Name = nullptr;
};

struct ComputePipelineDesc
{
    ShaderDesc  shaderDesc = {};
    const char* Name = nullptr;
};

struct ColorAttachment
{
    const GPUTexture* pTexture;
    LoadActionType    loadAction;
    StoreActionType   storeAction;
    ClearValue        ClearValue;
};

struct DepthAttachment
{
    const GPUTexture* pTexture;
    LoadActionType    loadAction;
    StoreActionType   storeAction;
    ClearValue        ClearValue;
};

struct RenderPassDesc
{
    Span<ColorAttachment> colorAttachments = {};
    DepthAttachment       depthAttachment = {};
};

enum OutputMode : uint32_t
{
    OUTPUT_MODE_SDR = 0,
    OUTPUT_MODE_HDR10,
    OUTPUT_MODE_SCRGB,
};

struct ContextDesc
{
    const char*  pAppName;
    WindowHandle windowHandle;
    uint32_t     Width;
    uint32_t     Height;
    uint32_t     imageCount;
    hz::Format   colorFormat;
    ColorSpace   colorSpace;
    HDRMetadata  hdrMetadata;
    bool         enableHDR = false;
    bool         enableVSync;
    bool         enableGpuValidation;
    bool         enableGpuProfiler;
    // Persists the driver pipeline cache between runs; only apps that map DirectoryShaderBinaries should enable it.
    bool         enablePipelineCache = false;
};

class GPUBuffer
{
public:
    GPUBuffer() = default;
    ~GPUBuffer();
    GPUBuffer(GPUBuffer&&) noexcept;
    GPUBuffer& operator=(GPUBuffer&&) noexcept;
    GPUBuffer(const GPUBuffer&) = delete;
    GPUBuffer& operator=(const GPUBuffer&) = delete;
    bool       IsValid() const { return pBuffer != nullptr; }
    Buffer*    Get() const { return pBuffer; }
    uint64_t   GetSize() const { return Size; }
    uint32_t   GetSrvIndex() const { return getBufferSrvIndex(pBuffer); }
    uint32_t   GetUavIndex() const { return getBufferUavIndex(pBuffer); }
    uint32_t   GetCbvIndex() const { return getBufferCbvIndex(pBuffer); }

private:
    GPUBuffer(RenderContext*, Buffer*, uint64_t, ResourceMemoryUsage, DescriptorType, ResourceState);
    void                  Destroy();
    RenderContext*        pContext = nullptr;
    Buffer*               pBuffer = nullptr;
    uint64_t              Size = 0;
    ResourceMemoryUsage   usage = MemoryUsageUnknown;
    DescriptorType        Descriptors = DescriptorTypeUndefined;
    mutable ResourceState state = ResourceStateUndefined;
    friend struct SceneResourceAccess;
    friend class CommandList;
    friend class RenderContext;
};

class GPUTexture
{
public:
    GPUTexture() = default;
    ~GPUTexture();
    GPUTexture(GPUTexture&&) noexcept;
    GPUTexture& operator=(GPUTexture&&) noexcept;
    GPUTexture(const GPUTexture&) = delete;
    GPUTexture&   operator=(const GPUTexture&) = delete;
    bool          IsValid() const { return pTexture != nullptr; }
    Texture*      Get() const { return pTexture; }
    uint32_t      GetSrvIndex() const { return getTextureSrvIndex(pTexture); }
    uint32_t      GetUavIndex(uint32_t mip = 0) const { return getTextureUavIndex(pTexture, mip); }
    RenderTarget* GetRenderTarget() const { return pRenderTarget; }
    uint32_t      GetWidth() const;
    uint32_t      GetHeight() const;
    uint32_t      GetArraySize() const;
    uint32_t      GetMipLevels() const;
    Format        GetFormat() const;

private:
    GPUTexture(RenderContext*, Texture*, RenderTarget*, ResourceState, bool owned = true);
    void                  Destroy();
    RenderContext*        pContext = nullptr;
    Texture*              pTexture = nullptr;
    RenderTarget*         pRenderTarget = nullptr;
    mutable ResourceState state = ResourceStateUndefined;
    bool                  owned = false;
    friend struct SceneResourceAccess;
    friend class CommandList;
    friend class RenderContext;
};

class GPUSampler
{
public:
    GPUSampler() = default;
    ~GPUSampler();
    GPUSampler(GPUSampler&&) noexcept;
    GPUSampler& operator=(GPUSampler&&) noexcept;
    GPUSampler(const GPUSampler&) = delete;
    GPUSampler& operator=(const GPUSampler&) = delete;
    bool        IsValid() const { return pSampler != nullptr; }
    Sampler*    Get() const { return pSampler; }
    uint32_t    GetIndex() const { return getSamplerIndex(pSampler); }

private:
    GPUSampler(RenderContext*, Sampler*);
    void           Destroy();
    RenderContext* pContext = nullptr;
    Sampler*       pSampler = nullptr;
    friend class CommandList;
    friend class RenderContext;
};

class GPUShader
{
public:
    GPUShader() = default;
    ~GPUShader();
    GPUShader(GPUShader&&) noexcept;
    GPUShader& operator=(GPUShader&&) noexcept;
    GPUShader(const GPUShader&) = delete;
    GPUShader& operator=(const GPUShader&) = delete;
    bool       IsValid() const { return pShader != nullptr; }
    Shader*    Get() const { return pShader; }

private:
    GPUShader(RenderContext*, Shader*);
    void           Destroy();
    RenderContext* pContext = nullptr;
    Shader*        pShader = nullptr;
    friend class CommandList;
    friend class RenderContext;
};

class GPUPipeline
{
public:
    GPUPipeline() = default;
    ~GPUPipeline();
    GPUPipeline(GPUPipeline&&) noexcept;
    GPUPipeline& operator=(GPUPipeline&&) noexcept;
    GPUPipeline(const GPUPipeline&) = delete;
    GPUPipeline& operator=(const GPUPipeline&) = delete;
    bool         IsValid() const { return pPipeline != nullptr; }
    Pipeline*    Get() const { return pPipeline; }

private:
    GPUPipeline(RenderContext*, Pipeline*, RootSignature*, GPUShader&&, CommandSignature* = nullptr);
    void           Destroy();
    RenderContext* pContext = nullptr;
    Pipeline*      pPipeline = nullptr;
    RootSignature* pRootSignature = nullptr;
    CommandSignature* pDrawIndirectSignature = nullptr;
    GPUShader      shader;
    friend class CommandList;
    friend class RenderContext;
};

struct Dependencies
{
    Span<const GPUTexture*> sampledTextures = {};
    Span<const GPUTexture*> storageTextures = {};
    Span<const GPUBuffer*>  buffers = {};
    Span<const GPUBuffer*>  storageBuffers = {};
    Span<const GPUBuffer*>  indirectBuffers = {};

    const Dependencies* pShared = nullptr;
};

// Collects the resources a command touches while handing out their descriptor indices, so binding a
// resource and declaring its Barrier cannot drift apart. Fixed Capacity, no allocation.
class DependencyList
{
public:
    // Optional inputs return UINT32_MAX and are not recorded, so call sites need no branch.
    uint32_t sample(const GPUTexture* pTexture)
    {
        if (!pTexture || !pTexture->IsValid())
            return UINT32_MAX;
        if (sampledCount < textureCapacity)
            sampledTextures[sampledCount++] = pTexture;
        else
            ASSERT(false);
        return pTexture->GetSrvIndex();
    }
    uint32_t store(const GPUTexture* pTexture)
    {
        if (!pTexture || !pTexture->IsValid())
            return UINT32_MAX;
        if (storageCount < textureCapacity)
            storageTextures[storageCount++] = pTexture;
        else
            ASSERT(false);
        return pTexture->GetUavIndex();
    }
    uint32_t read(const GPUBuffer& buffer)
    {
        if (!buffer.IsValid())
            return UINT32_MAX;
        if (bufferCount < bufferCapacity)
            buffers[bufferCount++] = &buffer;
        else
            ASSERT(false);
        return buffer.GetSrvIndex();
    }
    uint32_t write(const GPUBuffer& buffer)
    {
        if (!buffer.IsValid())
            return UINT32_MAX;
        if (storageBufferCount < bufferCapacity)
            storageBuffers[storageBufferCount++] = &buffer;
        else
            ASSERT(false);
        return buffer.GetUavIndex();
    }
    void indirect(const GPUBuffer& buffer)
    {
        if (!buffer.IsValid())
            return;
        if (indirectCount < bufferCapacity)
            indirectBuffers[indirectCount++] = &buffer;
        else
            ASSERT(false);
    }
    Dependencies Get() const
    {
        return { .sampledTextures = { sampledTextures, sampledCount },
                 .storageTextures = { storageTextures, storageCount },
                 .buffers = { buffers, bufferCount },
                 .storageBuffers = { storageBuffers, storageBufferCount },
                 .indirectBuffers = { indirectBuffers, indirectCount } };
    }

private:
    static constexpr uint32_t textureCapacity = 16;
    static constexpr uint32_t bufferCapacity = 8;
    const GPUTexture* sampledTextures[textureCapacity] = {};
    const GPUTexture* storageTextures[textureCapacity] = {};
    const GPUBuffer*  buffers[bufferCapacity] = {};
    const GPUBuffer*  storageBuffers[bufferCapacity] = {};
    const GPUBuffer*  indirectBuffers[bufferCapacity] = {};
    uint32_t          sampledCount = 0;
    uint32_t          storageCount = 0;
    uint32_t          bufferCount = 0;
    uint32_t          storageBufferCount = 0;
    uint32_t          indirectCount = 0;
};

struct BufferCopyRegion
{
    uint64_t srcOffset = 0;
    uint64_t dstOffset = 0;
    uint64_t Size = 0;
};

class CommandList
{
public:
    ~CommandList();

    void BeginRendering(const RenderPassDesc&, const Dependencies& = {});
    void EndRendering();
    void SetPipeline(const GPUPipeline&);
    void SetPipeline(const GPURayTracingPipeline&);
    void BuildAccelerationStructure(GPUAccelerationStructure&, Span<AccelerationStructureInstance> instances = {}, bool update = false);
    void UpdateAccelerationStructure(GPUAccelerationStructure&);
    void TraceRays(const GPURayTracingPipeline&, uint32_t Width, uint32_t Height, uint32_t Depth = 1, const Dependencies& = {});
    void SetVertexBuffer(uint32_t bindingIndex, const GPUBuffer&, uint64_t offset, uint32_t stride);
    void SetIndexBuffer(const GPUBuffer&, uint64_t offset, IndexType);
    void SetViewport(float x, float y, float Width, float Height, float minDepth = 0.0f, float maxDepth = 1.0f);
    void SetScissor(uint32_t x, uint32_t y, uint32_t Width, uint32_t Height);
    void SetRootConstant(Span<const uint32_t> values, uint32_t offsetIn32BitValues = 0);
    // Typed block form. T must mirror the shader constant block field for field, in declaration order.
    template<typename T>
    void SetRootConstants(const T& values, uint32_t offsetIn32BitValues = 0)
    {
        static_assert(sizeof(T) % sizeof(uint32_t) == 0, "Root constants are uploaded as 32-bit words");
        SetRootConstant({ (const uint32_t*)&values, (uint32_t)(sizeof(T) / sizeof(uint32_t)) }, offsetIn32BitValues);
    }

    void Draw(uint32_t vertexCount, uint32_t firstVertex = 0);
    void DrawIndexed(uint32_t indexCount, uint32_t firstIndex = 0, uint32_t firstVertex = 0);
    void DrawIndirect(const GPUBuffer& buffer, uint64_t offset, uint32_t drawCount);
    void DrawIndexedIndirect(const GPUBuffer& buffer, uint64_t offset, uint32_t drawCount);
    void Dispatch(uint32_t groupCountX, uint32_t groupCountY = 1, uint32_t groupCountZ = 1, const Dependencies& = {});
    void DispatchIndirect(const GPUBuffer& buffer, uint64_t offset, const Dependencies& = {});

    void BeginGpuTimestamp(const char* name);
    void EndGpuTimestamp();

    void CopyBuffer(const GPUBuffer& dst, uint64_t dstOffset, const GPUBuffer& src, uint64_t srcOffset, uint64_t Size);
    // Transitions both buffers once for all regions.
    void CopyBufferRegions(const GPUBuffer& dst, const GPUBuffer& src, Span<BufferCopyRegion> regions);
    void FillBuffer(const GPUBuffer& dst, uint64_t dstOffset, uint32_t value, uint64_t Size);
    void UpdateBuffer(const GPUBuffer& dst, uint64_t dstOffset, const void* Data, uint64_t Size);

    void CopyTexture(const GPUTexture& dst, const GPUTexture& src);

    void PushDebugGroupLabel(const char* label, uint32_t colorRGBA) const;
    void InsertDebugEventLabel(const char* label, uint32_t colorRGBA) const;
    void PopDebugGroupLabel() const;

private:
    CommandList() = default;
    CommandList(const CommandList&) = delete;
    CommandList& operator=(const CommandList&) = delete;
    void         Reset(Cmd*, uint64_t gpuProfilerToken);
    void         Clear();
    void         BeginGpuFrameProfile();
    void         EndGpuFrameProfile();
    void         AddBufferBarriers(Span<const GPUBuffer*> buffers, bool storage);
    void         AddIndirectBarriers(Span<const GPUBuffer*> buffers);
    Buffer*      CreateUploadBuffer(uint64_t Size);
    void         Barrier(uint32_t bufferCount, BufferBarrier*, uint32_t textureCount, TextureBarrier*, uint32_t renderTargetCount,
                         RenderTargetBarrier*);
    void         Barrier(const Dependencies&, const GPUBuffer* pIndirectBuffer);

    Cmd*                 pCmd = nullptr;
    RenderContext*       pContext = nullptr;
    RootSignature*       pCurrentRootSignature = nullptr;
    CommandSignature*    pCurrentDrawIndirectSignature = nullptr;
    uint64_t             gpuProfilerToken = UINT64_MAX;
    Buffer*              pVertexBuffers[MAX_VERTEX_BINDINGS] = {};
    uint32_t             vertexStrides[MAX_VERTEX_BINDINGS] = {};
    uint64_t             vertexOffsets[MAX_VERTEX_BINDINGS] = {};
    uint32_t             vertexBufferCount = 0;
    uint32_t             commandIndex = UINT32_MAX;
    BufferBarrier*       bufferBarriers = nullptr;
    TextureBarrier*      textureBarriers = nullptr;
    RenderTargetBarrier* renderTargetBarriers = nullptr;
    friend class RenderContext;
};

struct SubmitHandle
{
    uint64_t id = 0;
    uint32_t commandIndex = 0;
    bool     IsValid() const { return id != 0; }
};

class RenderContext
{
public:
    RenderContext(const ContextDesc&);
    ~RenderContext();
    RenderContext(const RenderContext&) = delete;
    RenderContext& operator=(const RenderContext&) = delete;
    bool           IsValid() const { return ready; }

    void WaitIdle();
    // Persists the driver-owned pipeline cache; call once the startup pipelines exist.
    void SavePipelineCache();

    bool                  Resize(uint32_t Width, uint32_t Height);
    bool                  SetVSync(bool enabled);
    CommandList&          AcquireCommandList();
    SubmitHandle          Submit(CommandList&, const GPUTexture* pPresent = nullptr);
    void                  Wait(SubmitHandle);
    bool                  IsComplete(SubmitHandle) const;
    uint32_t              GetWidth() const;
    uint32_t              GetHeight() const;
    hz::Format            GetColorFormat() const;
    bool                  IsHDREnabled() const;
    OutputMode            GetOutputMode() const;
    const HDRDisplayInfo& GetHDRDisplayInfo() const;
    const HDRMetadata&    GetHDRMetadata() const;
    const GPUTexture&     GetCurrentBackbuffer();
    bool                  IsSuspended() const;

    bool                     SupportsRayTracing() const;
    GPUAccelerationStructure CreateAccelerationStructure(const AccelerationStructureDesc&);
    GPURayTracingPipeline    CreateRayTracingPipeline(const RayTracingPipelineDesc&);
    GPUBuffer   CreateBuffer(const BufferDesc&);
    GPUTexture  CreateTexture(const TextureDesc&);
    // Pooled per-frame target, or null when every matching slot is still in use. The pool keeps the
    // Handle alive between frames, so its resource state survives; ReleaseTransient parks it again.
    GPUTexture* AcquireTransient(const TextureDesc&);
    void        ReleaseTransient(GPUTexture* pTexture);
    // Destroys parked targets, for example when the render Size changes and their Descriptors no longer
    // match. Call after waiting for the GPU; targets still in use are kept.
    void        PurgeTransients();
    GPUTexture               LoadTexture(ResourceDirectory, const char* pFileName, TextureCreationFlags = TextureCreationFlagNone);
    GPUSampler  CreateSampler(const SamplerDesc& = {});
    GPUPipeline CreateGraphicsPipeline(const GraphicsPipelineDesc&);
    GPUPipeline CreateComputePipeline(const ComputePipelineDesc&);
    bool        GetGpuAddress(const GPUBuffer&, uint64_t* pAddress) const;
    bool        UpdateBuffer(const GPUBuffer&, uint64_t offset, const void* Data, uint64_t Size);
    // Destroys the buffer once the open command list and all earlier submissions complete.
    void                     DeferRelease(GPUBuffer&& buffer);
    // Identifies the open command list, or the latest submission when none is open.
    uint64_t                 GetResourceUseId() const;
    bool                     IsResourceUseComplete(uint64_t useId) const;

private:
    enum class ResourceType
    {
        Buffer,
        Texture,
        RenderTarget,
        Sampler,
        Shader,
        Pipeline,
        AccelerationStructure,
        RayTracingPipeline
    };
    struct DeferredResource
    {
        void*        resource = nullptr;
        void*        secondary = nullptr;
        void*        tertiary = nullptr;
        uint64_t useId = 0;
        ResourceType Type = ResourceType::Buffer;
    };
    void DeferResource(ResourceType Type, void* resource, void* secondary = nullptr, void* tertiary = nullptr);
    void ReleaseAccelerationStructure(AccelerationStructureData*);
    void DestroyAccelerationStructure(AccelerationStructureData*);
    void ReleaseTransientSlots();

    struct CommandResources
    {
        ~CommandResources();

        CmdPool*    pCmdPool = nullptr;
        Cmd*        pCmd = nullptr;
        Fence*      pFence = nullptr;
        Semaphore*  pSemaphore = nullptr;
        CommandList commands = {};
        Buffer**    transientBuffers = nullptr;
        uint64_t    submitId = 0;
    };

    // A parked target keeps its Handle so resource state and descriptor indices stay valid between frames.
    struct TransientSlot
    {
        GPUTexture  texture;
        TextureDesc desc = {};
        uint64_t    useId = 0;
        bool        inUse = false;
        bool        used = false;
    };

    bool     InitDevice();
    bool     CreateSwapChain();
    void     DestroySwapChain();
    void     DestroyDevice(bool waitForGpu);
    void     Cleanup();
    void     CollectDeferredResources();
    uint64_t GetPendingUseId() const;
    Shader*  CreateShader(const ShaderDesc&);
    Shader*  CreateSlangShader(const ShaderSrcDesc&);
    bool     InitPipelineCache();

    Raytracing*    pRaytracing = nullptr;
    PipelineCache* pPipelineCache = nullptr;

    ContextDesc               desc = {};
    HDRDisplayInfo            hdrDisplayInfo = {};
    HDRMetadata               hdrMetadata = {};
    OutputMode                outputMode = OUTPUT_MODE_SDR;
    RendererContext*          pRendererContext = nullptr;
    Renderer*                 pRenderer = nullptr;
    Queue*                    pGraphicsQueue = nullptr;
    SwapChain*                pSwapChain = nullptr;
    CommandSignature*         pDrawIndirectSignature = nullptr;
    CommandSignature*         pDrawIndexedIndirectSignature = nullptr;
    CommandSignature*         pDispatchIndirectSignature = nullptr;
    // TODO: Use a dynamic array for command resources to support more than 32 command lists if needed
    static constexpr uint32_t maxCommandLists = 16;
    CommandResources          commandResources[maxCommandLists] = {};
    Semaphore*                pImageAcquiredSemaphore = nullptr;
    GPUTexture                backbuffers[MAX_SWAPCHAIN_IMAGES] = {};
    DeferredResource*         deferredResources = nullptr;
    uint64_t                  gpuProfilerToken = UINT64_MAX;
    uint64_t                  nextSubmitId = 1;
    uint32_t                  nextCommandIndex = 0;
    static constexpr uint32_t transientCapacity = 64;
    TransientSlot             transientSlots[transientCapacity] = {};
    uint32_t                  swapchainImageIndex = 0;
    char                      appName[128] = {};
    bool                      resourceLoaderInitialized = false;
    bool                      profilerInitialized = false;
    bool                      imageAcquired = false;
    bool                      imageAcquireWaitPending = false;
    bool                      suspended = false;
    bool                      ready = false;
    uint32_t                  resourceCount = 0;
    friend class GPUBuffer;
    friend class GPUTexture;
    friend class GPUSampler;
    friend class GPUShader;
    friend class GPUPipeline;
    friend class GPUAccelerationStructure;
    friend class GPURayTracingPipeline;
    friend class CommandList;
    friend class AccelerationStructureData;
};

} // namespace hz
