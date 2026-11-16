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
    bool                 renderTarget = false;
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
    bool       isValid() const { return pBuffer != nullptr; }
    Buffer*    get() const { return pBuffer; }
    uint64_t   getSize() const { return Size; }
    uint32_t   getSrvIndex() const { return getBufferSrvIndex(pBuffer); }
    uint32_t   getUavIndex() const { return getBufferUavIndex(pBuffer); }
    uint32_t   getCbvIndex() const { return getBufferCbvIndex(pBuffer); }

private:
    GPUBuffer(RenderContext*, Buffer*, uint64_t, ResourceMemoryUsage, DescriptorType, ResourceState);
    void                  destroy();
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
    bool          isValid() const { return pTexture != nullptr; }
    Texture*      get() const { return pTexture; }
    uint32_t      getSrvIndex() const { return getTextureSrvIndex(pTexture); }
    uint32_t      getUavIndex(uint32_t mip = 0) const { return getTextureUavIndex(pTexture, mip); }
    RenderTarget* renderTarget() const { return pRenderTarget; }
    uint32_t      getWidth() const;
    uint32_t      getHeight() const;
    uint32_t      getArraySize() const;
    uint32_t      getMipLevels() const;
    Format        getFormat() const;

private:
    GPUTexture(RenderContext*, Texture*, RenderTarget*, ResourceState, bool owned = true);
    void                  destroy();
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
    bool        isValid() const { return pSampler != nullptr; }
    Sampler*    get() const { return pSampler; }
    uint32_t    getIndex() const { return getSamplerIndex(pSampler); }

private:
    GPUSampler(RenderContext*, Sampler*);
    void           destroy();
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
    bool       isValid() const { return pShader != nullptr; }
    Shader*    get() const { return pShader; }

private:
    GPUShader(RenderContext*, Shader*);
    void           destroy();
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
    bool         isValid() const { return pPipeline != nullptr; }
    Pipeline*    get() const { return pPipeline; }

private:
    GPUPipeline(RenderContext*, Pipeline*, RootSignature*, GPUShader&&, CommandSignature* = nullptr);
    void           destroy();
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
// resource and declaring its barrier cannot drift apart. Fixed Capacity, no allocation.
class DependencyList
{
public:
    // Optional inputs return UINT32_MAX and are not recorded, so call sites need no branch.
    uint32_t sample(const GPUTexture* pTexture)
    {
        if (!pTexture || !pTexture->isValid())
            return UINT32_MAX;
        if (sampledCount < textureCapacity)
            sampledTextures[sampledCount++] = pTexture;
        else
            ASSERT(false);
        return pTexture->getSrvIndex();
    }
    uint32_t store(const GPUTexture* pTexture)
    {
        if (!pTexture || !pTexture->isValid())
            return UINT32_MAX;
        if (storageCount < textureCapacity)
            storageTextures[storageCount++] = pTexture;
        else
            ASSERT(false);
        return pTexture->getUavIndex();
    }
    uint32_t read(const GPUBuffer& buffer)
    {
        if (!buffer.isValid())
            return UINT32_MAX;
        if (bufferCount < bufferCapacity)
            buffers[bufferCount++] = &buffer;
        else
            ASSERT(false);
        return buffer.getSrvIndex();
    }
    uint32_t write(const GPUBuffer& buffer)
    {
        if (!buffer.isValid())
            return UINT32_MAX;
        if (storageBufferCount < bufferCapacity)
            storageBuffers[storageBufferCount++] = &buffer;
        else
            ASSERT(false);
        return buffer.getUavIndex();
    }
    void indirect(const GPUBuffer& buffer)
    {
        if (!buffer.isValid())
            return;
        if (indirectCount < bufferCapacity)
            indirectBuffers[indirectCount++] = &buffer;
        else
            ASSERT(false);
    }
    Dependencies get() const
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

    void beginRendering(const RenderPassDesc&, const Dependencies& = {});
    void endRendering();
    void setPipeline(const GPUPipeline&);
    void setPipeline(const GPURayTracingPipeline&);
    void buildAccelerationStructure(GPUAccelerationStructure&, Span<AccelerationStructureInstance> instances = {}, bool update = false);
    void updateAccelerationStructure(GPUAccelerationStructure&);
    void traceRays(const GPURayTracingPipeline&, uint32_t Width, uint32_t Height, uint32_t Depth = 1, const Dependencies& = {});
    void setVertexBuffer(uint32_t bindingIndex, const GPUBuffer&, uint64_t offset, uint32_t stride);
    void setIndexBuffer(const GPUBuffer&, uint64_t offset, IndexType);
    void setViewport(float x, float y, float Width, float Height, float minDepth = 0.0f, float maxDepth = 1.0f);
    void setScissor(uint32_t x, uint32_t y, uint32_t Width, uint32_t Height);
    void setRootConstant(Span<const uint32_t> values, uint32_t offsetIn32BitValues = 0);
    // Typed block form. T must mirror the shader constant block field for field, in declaration order.
    template<typename T>
    void setRootConstants(const T& values, uint32_t offsetIn32BitValues = 0)
    {
        static_assert(sizeof(T) % sizeof(uint32_t) == 0, "Root constants are uploaded as 32-bit words");
        setRootConstant({ (const uint32_t*)&values, (uint32_t)(sizeof(T) / sizeof(uint32_t)) }, offsetIn32BitValues);
    }

    void draw(uint32_t vertexCount, uint32_t firstVertex = 0);
    void drawIndexed(uint32_t indexCount, uint32_t firstIndex = 0, uint32_t firstVertex = 0);
    void drawIndirect(const GPUBuffer& buffer, uint64_t offset, uint32_t drawCount);
    void drawIndexedIndirect(const GPUBuffer& buffer, uint64_t offset, uint32_t drawCount);
    void dispatch(uint32_t groupCountX, uint32_t groupCountY = 1, uint32_t groupCountZ = 1, const Dependencies& = {});
    void dispatchIndirect(const GPUBuffer& buffer, uint64_t offset, const Dependencies& = {});

    void beginGpuTimestamp(const char* name);
    void endGpuTimestamp();

    void copyBuffer(const GPUBuffer& dst, uint64_t dstOffset, const GPUBuffer& src, uint64_t srcOffset, uint64_t Size);
    // Transitions both buffers once for all regions.
    void copyBufferRegions(const GPUBuffer& dst, const GPUBuffer& src, Span<BufferCopyRegion> regions);
    void fillBuffer(const GPUBuffer& dst, uint64_t dstOffset, uint32_t value, uint64_t Size);
    void updateBuffer(const GPUBuffer& dst, uint64_t dstOffset, const void* Data, uint64_t Size);

    void copyTexture(const GPUTexture& dst, const GPUTexture& src);

    void pushDebugGroupLabel(const char* label, uint32_t colorRGBA) const;
    void insertDebugEventLabel(const char* label, uint32_t colorRGBA) const;
    void popDebugGroupLabel() const;

private:
    CommandList() = default;
    CommandList(const CommandList&) = delete;
    CommandList& operator=(const CommandList&) = delete;
    void         reset(Cmd*, uint64_t gpuProfilerToken);
    void         Clear();
    void         beginGpuFrameProfile();
    void         endGpuFrameProfile();
    void         addBufferBarriers(Span<const GPUBuffer*> buffers, bool storage);
    void         addIndirectBarriers(Span<const GPUBuffer*> buffers);
    Buffer*      createUploadBuffer(uint64_t Size);
    void         barrier(uint32_t bufferCount, BufferBarrier*, uint32_t textureCount, TextureBarrier*, uint32_t renderTargetCount,
                         RenderTargetBarrier*);
    void         barrier(const Dependencies&, const GPUBuffer* pIndirectBuffer);

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
    bool     isValid() const { return id != 0; }
};

class RenderContext
{
public:
    RenderContext(const ContextDesc&);
    ~RenderContext();
    RenderContext(const RenderContext&) = delete;
    RenderContext& operator=(const RenderContext&) = delete;
    bool           isValid() const { return ready; }

    void waitIdle();
    // Persists the driver-owned pipeline cache; call once the startup pipelines exist.
    void savePipelineCache();

    bool                  resize(uint32_t Width, uint32_t Height);
    bool                  setVSync(bool enabled);
    CommandList&          acquireCommandList();
    SubmitHandle          submit(CommandList&, const GPUTexture* pPresent = nullptr);
    void                  wait(SubmitHandle);
    bool                  isComplete(SubmitHandle) const;
    uint32_t              getWidth() const;
    uint32_t              getHeight() const;
    hz::Format            getColorFormat() const;
    bool                  isHDREnabled() const;
    OutputMode            getOutputMode() const;
    const HDRDisplayInfo& getHDRDisplayInfo() const;
    const HDRMetadata&    getHDRMetadata() const;
    const GPUTexture&     getCurrentBackbuffer();
    bool                  isSuspended() const;

    bool                     supportsRayTracing() const;
    GPUAccelerationStructure createAccelerationStructure(const AccelerationStructureDesc&);
    GPURayTracingPipeline    createRayTracingPipeline(const RayTracingPipelineDesc&);
    GPUBuffer   createBuffer(const BufferDesc&);
    GPUTexture  createTexture(const TextureDesc&);
    // Pooled per-frame target, or null when every matching slot is still in use. The pool keeps the
    // Handle alive between frames, so its resource state survives; releaseTransient parks it again.
    GPUTexture* acquireTransient(const TextureDesc&);
    void        releaseTransient(GPUTexture* pTexture);
    // Destroys parked targets, for example when the render Size changes and their Descriptors no longer
    // match. Call after waiting for the GPU; targets still in use are kept.
    void        purgeTransients();
    GPUTexture               loadTexture(ResourceDirectory, const char* pFileName, TextureCreationFlags = TextureCreationFlagNone);
    GPUSampler  createSampler(const SamplerDesc& = {});
    GPUPipeline createGraphicsPipeline(const GraphicsPipelineDesc&);
    GPUPipeline createComputePipeline(const ComputePipelineDesc&);
    bool        getGpuAddress(const GPUBuffer&, uint64_t* pAddress) const;
    bool        updateBuffer(const GPUBuffer&, uint64_t offset, const void* Data, uint64_t Size);
    // Destroys the buffer once the open command list and all earlier submissions complete.
    void                     deferRelease(GPUBuffer&& buffer);
    // Identifies the open command list, or the latest submission when none is open.
    uint64_t                 getResourceUseId() const;
    bool                     isResourceUseComplete(uint64_t useId) const;

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
    void deferResource(ResourceType Type, void* resource, void* secondary = nullptr, void* tertiary = nullptr);
    void releaseAccelerationStructure(AccelerationStructureData*);
    void destroyAccelerationStructure(AccelerationStructureData*);
    void releaseTransientSlots();

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

    bool     initDevice();
    bool     createSwapChain();
    void     destroySwapChain();
    void     destroyDevice(bool waitForGpu);
    void     cleanup();
    void     collectDeferredResources();
    uint64_t getPendingUseId() const;
    Shader*  createShader(const ShaderDesc&);
    Shader*  createSlangShader(const ShaderSrcDesc&);
    bool     initPipelineCache();

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
