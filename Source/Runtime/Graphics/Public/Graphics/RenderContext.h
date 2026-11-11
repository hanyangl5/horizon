/* Copyright (c) 2026 Horizon */
#pragma once

#include "Core/IFileSystem.h"
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
    uint64_t            size = 0;
    uint32_t            elementCount = 0;
    uint32_t            structStride = 0;
    const char*         pName = nullptr;
    const void*         pInitialData = nullptr;
    uint64_t            initialDataSize = 0;
    ResourceMemoryUsage usage = RESOURCE_MEMORY_USAGE_UNKNOWN;
    ResourceState       startState = RESOURCE_STATE_UNDEFINED;
    DescriptorType      descriptors = DESCRIPTOR_TYPE_UNDEFINED;
    BufferCreationFlags flags = BUFFER_CREATION_FLAG_NONE;
};

struct TextureDesc
{
    uint32_t             width = 0;
    uint32_t             height = 0;
    uint32_t             depth = 1;
    uint32_t             arraySize = 1;
    uint32_t             mipLevels = 1;
    SampleCount          sampleCount = SAMPLE_COUNT_1;
    hz::Format           format = hz::Format::UNDEFINED;
    ResourceState        startState = RESOURCE_STATE_UNDEFINED;
    DescriptorType       descriptors = DESCRIPTOR_TYPE_UNDEFINED;
    TextureCreationFlags flags = TEXTURE_CREATION_FLAG_NONE;
    bool                 renderTarget = false;
    const char*          pName = nullptr;
};

struct SamplerDesc
{
    FilterType  minFilter = FILTER_LINEAR;
    FilterType  magFilter = FILTER_LINEAR;
    MipMapMode  mipMapMode = MIPMAP_MODE_LINEAR;
    AddressMode addressU = ADDRESS_MODE_REPEAT;
    AddressMode addressV = ADDRESS_MODE_REPEAT;
    AddressMode addressW = ADDRESS_MODE_REPEAT;
    float       mipLodBias = 0.0f;
    bool        setLodRange = false;
    float       minLod = 0.0f;
    float       maxLod = 0.0f;
    float       maxAnisotropy = 0.0f;
    CompareMode compareFunc = CMP_NEVER;
};

struct ShaderStageDesc
{
    ShaderStage stage = SHADER_STAGE_NONE;
    const void* pSource = nullptr;
    uint32_t    sourceSize = 0;
    const char* pEntryPoint = "main";
    const char* pName = nullptr;
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
    ResourceDirectory         sourceDirectory = RD_SHADER_SOURCES;
    const char*               pFileName = nullptr;
    ShaderLanguage            language = ShaderLanguage::Auto;
};

struct ColorTargetDesc
{
    Format        format = Format::UNDEFINED;
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
    DepthStateDesc        depth = { .depthTest = false, .depthWrite = false, .depthFunc = CMP_ALWAYS };
    Span<ColorTargetDesc> colorTargets = {};
    bool                  alphaToCoverage = false;
    hz::Format            depthStencilFormat = hz::Format::UNDEFINED;
    PrimitiveTopology     topology = PRIMITIVE_TOPO_TRI_LIST;
    SampleCount           sampleCount = SAMPLE_COUNT_1;
    bool                  indirectDrawId = false;
    const char*           pName = nullptr;
};

struct ComputePipelineDesc
{
    ShaderDesc  shaderDesc = {};
    const char* pName = nullptr;
};

struct ColorAttachment
{
    const GPUTexture* pTexture;
    LoadActionType    loadAction;
    StoreActionType   storeAction;
    ClearValue        clearValue;
};

struct DepthAttachment
{
    const GPUTexture* pTexture;
    LoadActionType    loadAction;
    StoreActionType   storeAction;
    ClearValue        clearValue;
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
    uint32_t     width;
    uint32_t     height;
    uint32_t     imageCount;
    hz::Format   colorFormat;
    ColorSpace   colorSpace;
    HDRMetadata  hdrMetadata;
    bool         enableHDR = false;
    bool         enableVSync;
    bool         enableGpuValidation;
    bool         enableGpuProfiler;
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
    uint64_t   getSize() const { return size; }
    uint32_t   getSrvIndex() const { return getBufferSrvIndex(pBuffer); }
    uint32_t   getUavIndex() const { return getBufferUavIndex(pBuffer); }
    uint32_t   getCbvIndex() const { return getBufferCbvIndex(pBuffer); }

private:
    GPUBuffer(RenderContext*, Buffer*, uint64_t, ResourceMemoryUsage, DescriptorType, ResourceState);
    void                  destroy();
    RenderContext*        pContext = nullptr;
    Buffer*               pBuffer = nullptr;
    uint64_t              size = 0;
    ResourceMemoryUsage   usage = RESOURCE_MEMORY_USAGE_UNKNOWN;
    DescriptorType        descriptors = DESCRIPTOR_TYPE_UNDEFINED;
    mutable ResourceState state = RESOURCE_STATE_UNDEFINED;
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
    mutable ResourceState state = RESOURCE_STATE_UNDEFINED;
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

struct BufferCopyRegion
{
    uint64_t srcOffset = 0;
    uint64_t dstOffset = 0;
    uint64_t size = 0;
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
    void traceRays(const GPURayTracingPipeline&, uint32_t width, uint32_t height, uint32_t depth = 1, const Dependencies& = {});
    void setVertexBuffer(uint32_t bindingIndex, const GPUBuffer&, uint64_t offset, uint32_t stride);
    void setIndexBuffer(const GPUBuffer&, uint64_t offset, IndexType);
    void setViewport(float x, float y, float width, float height, float minDepth = 0.0f, float maxDepth = 1.0f);
    void setScissor(uint32_t x, uint32_t y, uint32_t width, uint32_t height);
    void setRootConstant(Span<const uint32_t> values, uint32_t offsetIn32BitValues = 0);

    void draw(uint32_t vertexCount, uint32_t firstVertex = 0);
    void drawIndexed(uint32_t indexCount, uint32_t firstIndex = 0, uint32_t firstVertex = 0);
    void drawIndirect(const GPUBuffer& buffer, uint64_t offset, uint32_t drawCount);
    void drawIndexedIndirect(const GPUBuffer& buffer, uint64_t offset, uint32_t drawCount);
    void dispatch(uint32_t groupCountX, uint32_t groupCountY = 1, uint32_t groupCountZ = 1, const Dependencies& = {});
    void dispatchIndirect(const GPUBuffer& buffer, uint64_t offset, const Dependencies& = {});

    void beginGpuTimestamp(const char* name);
    void endGpuTimestamp();

    void copyBuffer(const GPUBuffer& dst, uint64_t dstOffset, const GPUBuffer& src, uint64_t srcOffset, uint64_t size);
    // Transitions both buffers once for all regions.
    void copyBufferRegions(const GPUBuffer& dst, const GPUBuffer& src, Span<BufferCopyRegion> regions);
    void fillBuffer(const GPUBuffer& dst, uint64_t dstOffset, uint32_t value, uint64_t size);
    void updateBuffer(const GPUBuffer& dst, uint64_t dstOffset, const void* pData, uint64_t size);

    void copyTexture(const GPUTexture& dst, const GPUTexture& src);

    void pushDebugGroupLabel(const char* label, uint32_t colorRGBA) const;
    void insertDebugEventLabel(const char* label, uint32_t colorRGBA) const;
    void popDebugGroupLabel() const;

private:
    CommandList() = default;
    CommandList(const CommandList&) = delete;
    CommandList& operator=(const CommandList&) = delete;
    void         reset(Cmd*, uint64_t gpuProfilerToken);
    void         clear();
    void         beginGpuFrameProfile();
    void         endGpuFrameProfile();
    void         addBufferBarriers(Span<const GPUBuffer*> buffers, bool storage);
    void         addIndirectBarriers(Span<const GPUBuffer*> buffers);
    Buffer*      createUploadBuffer(uint64_t size);
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

    bool                  resize(uint32_t width, uint32_t height);
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
    GPUTexture               loadTexture(ResourceDirectory, const char* pFileName, TextureCreationFlags = TEXTURE_CREATION_FLAG_NONE);
    GPUSampler  createSampler(const SamplerDesc& = {});
    GPUPipeline createGraphicsPipeline(const GraphicsPipelineDesc&);
    GPUPipeline createComputePipeline(const ComputePipelineDesc&);
    bool        getGpuAddress(const GPUBuffer&, uint64_t* pAddress) const;
    bool        updateBuffer(const GPUBuffer&, uint64_t offset, const void* pData, uint64_t size);
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
        ResourceType type = ResourceType::Buffer;
    };
    void deferResource(ResourceType type, void* resource, void* secondary = nullptr, void* tertiary = nullptr);
    void releaseAccelerationStructure(AccelerationStructureData*);
    void destroyAccelerationStructure(AccelerationStructureData*);

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

    bool     initDevice();
    bool     createSwapChain();
    void     destroySwapChain();
    void     destroyDevice(bool waitForGpu);
    void     cleanup();
    void     collectDeferredResources();
    uint64_t getPendingUseId() const;
    Shader*  createShader(const ShaderDesc&);
    Shader*  createSlangShader(const ShaderSrcDesc&);
    void     destroyShaderCompiler();

    slang::IGlobalSession* pSlangSession = nullptr;
    Raytracing*            pRaytracing = nullptr;

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
