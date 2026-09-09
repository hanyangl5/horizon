/* Copyright (c) 2026 Horizon */
#pragma once

#include "Core/IFileSystem.h"
#include "Core/ISpan.h"
#include "RHI/IGraphics.h"

#include <stdint.h>

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
    uint64_t            size;
    uint32_t            elementCount;
    uint32_t            structStride;
    const char*         pName;
    const void*         pInitialData;
    uint64_t            initialDataSize;
    ResourceMemoryUsage usage;
    ResourceState       startState;
    DescriptorType      descriptors;
    BufferCreationFlags flags;
};

struct TextureDesc
{
    uint32_t             width;
    uint32_t             height;
    uint32_t             depth;
    uint32_t             arraySize;
    uint32_t             mipLevels;
    SampleCount          sampleCount;
    TinyImageFormat      format;
    ResourceState        startState;
    DescriptorType       descriptors;
    TextureCreationFlags flags;
    bool                 renderTarget;
    const char*          pName;
};

struct ShaderStageDesc
{
    ShaderStage stage;
    const void* pSource;
    uint32_t    sourceSize;
    const char* pEntryPoint;
    const char* pName;
};

struct ShaderDesc
{
    ShaderStageDesc   stages[MAX_SHADER_STAGES];
    uint32_t          stageCount;
    ResourceDirectory sourceDirectory;
    const char*       pFileName;
};

struct GraphicsPipelineDesc
{
    const GPUShader*    pShader;
    VertexLayout        vertexLayout;
    RasterizerStateDesc rasterizer;
    DepthStateDesc      depth;
    BlendStateDesc      blend;
    TinyImageFormat     colorFormats[MAX_RENDER_TARGETS];
    uint32_t            renderTargetCount;
    TinyImageFormat     depthStencilFormat;
    PrimitiveTopology   topology;
    SampleCount         sampleCount;
    const char*         pName;
};

struct ComputePipelineDesc
{
    const GPUShader* pShader;
    const char*      pName;
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
    ColorAttachment colorAttachments[MAX_RENDER_TARGETS];
    uint32_t        colorAttachmentCount;
    DepthAttachment depthAttachment;
};

struct ContextDesc
{
    const char*     pAppName;
    WindowHandle    windowHandle;
    uint32_t        width;
    uint32_t        height;
    uint32_t        imageCount;
    TinyImageFormat colorFormat;
    ColorSpace      colorSpace;
    bool            enableVSync;
    bool            enableGpuValidation;
    bool            enableGpuProfiler;
};

class GPUBuffer final
{
public:
    GPUBuffer() = default;
    ~GPUBuffer();
    GPUBuffer(GPUBuffer&&) noexcept;
    GPUBuffer& operator=(GPUBuffer&&) noexcept;
    GPUBuffer(const GPUBuffer&) = delete;
    GPUBuffer& operator=(const GPUBuffer&) = delete;
    explicit   operator bool() const { return pBuffer != nullptr; }
    Buffer*    native() const { return pBuffer; }

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

class GPUTexture final
{
public:
    GPUTexture() = default;
    ~GPUTexture();
    GPUTexture(GPUTexture&&) noexcept;
    GPUTexture& operator=(GPUTexture&&) noexcept;
    GPUTexture(const GPUTexture&) = delete;
    GPUTexture&   operator=(const GPUTexture&) = delete;
    explicit      operator bool() const { return pTexture != nullptr; }
    Texture*      native() const { return pTexture; }
    RenderTarget* renderTarget() const { return pRenderTarget; }

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

class GPUSampler final
{
public:
    GPUSampler() = default;
    ~GPUSampler();
    GPUSampler(GPUSampler&&) noexcept;
    GPUSampler& operator=(GPUSampler&&) noexcept;
    GPUSampler(const GPUSampler&) = delete;
    GPUSampler& operator=(const GPUSampler&) = delete;
    explicit    operator bool() const { return pSampler != nullptr; }
    Sampler*    native() const { return pSampler; }

private:
    GPUSampler(RenderContext*, Sampler*);
    void           destroy();
    RenderContext* pContext = nullptr;
    Sampler*       pSampler = nullptr;
    friend class CommandList;
    friend class RenderContext;
};

class GPUShader final
{
public:
    GPUShader() = default;
    ~GPUShader();
    GPUShader(GPUShader&&) noexcept;
    GPUShader& operator=(GPUShader&&) noexcept;
    GPUShader(const GPUShader&) = delete;
    GPUShader& operator=(const GPUShader&) = delete;
    explicit   operator bool() const { return pShader != nullptr; }
    Shader*    native() const { return pShader; }

private:
    GPUShader(RenderContext*, Shader*);
    void           destroy();
    RenderContext* pContext = nullptr;
    Shader*        pShader = nullptr;
    friend class CommandList;
    friend class RenderContext;
};

class GPUPipeline final
{
public:
    GPUPipeline() = default;
    ~GPUPipeline();
    GPUPipeline(GPUPipeline&&) noexcept;
    GPUPipeline& operator=(GPUPipeline&&) noexcept;
    GPUPipeline(const GPUPipeline&) = delete;
    GPUPipeline& operator=(const GPUPipeline&) = delete;
    explicit     operator bool() const { return pPipeline != nullptr; }
    Pipeline*    native() const { return pPipeline; }

private:
    GPUPipeline(RenderContext*, Pipeline*, RootSignature*);
    void           destroy();
    RenderContext* pContext = nullptr;
    Pipeline*      pPipeline = nullptr;
    RootSignature* pRootSignature = nullptr;
    friend class CommandList;
    friend class RenderContext;
};

struct Dependencies
{
    Span<const GPUTexture*> sampledTextures = {};
    Span<const GPUTexture*> storageTextures = {};
    Span<const GPUBuffer*>  buffers = {};
};

class CommandList
{
public:
    ~CommandList();

    void beginRendering(const RenderPassDesc&, const Dependencies& = {});
    void endRendering();
    void setPipeline(const GPUPipeline&);
    void bindBuffer(const char* name, const GPUBuffer&);
    void bindTexture(const char* name, const GPUTexture&);
    void bindSampler(const char* name, const GPUSampler&);
    void setVertexBuffer(uint32_t slot, const GPUBuffer&, uint64_t offset, uint32_t stride);
    void setIndexBuffer(const GPUBuffer&, uint64_t offset, IndexType);
    void setViewport(float x, float y, float width, float height, float minDepth = 0.0f, float maxDepth = 1.0f);
    void setScissor(uint32_t x, uint32_t y, uint32_t width, uint32_t height);
    void setPushConstants(uint32_t slot, const void* data, uint32_t size);

    void draw(uint32_t vertexCount, uint32_t firstVertex = 0);
    void drawIndexed(uint32_t indexCount, uint32_t firstIndex = 0, uint32_t firstVertex = 0);
    void drawIndirect(const GPUBuffer& buffer, uint64_t offset, uint32_t drawCount);
    void drawIndexedIndirect(const GPUBuffer& buffer, uint64_t offset, uint32_t drawCount);
    void dispatch(uint32_t groupCountX, uint32_t groupCountY = 1, uint32_t groupCountZ = 1, const Dependencies& = {});
    void dispatchIndirect(const GPUBuffer& buffer, uint64_t offset, const Dependencies& = {});

    void beginGpuTimestamp(const char* name);
    void endGpuTimestamp();

    void copyBuffer(const GPUBuffer& dst, uint64_t dstOffset, const GPUBuffer& src, uint64_t srcOffset, uint64_t size);
    void fillBuffer(const GPUBuffer& dst, uint64_t dstOffset, uint32_t value, uint64_t size);
    void updateBuffer(const GPUBuffer& dst, uint64_t dstOffset, const void* pData, uint64_t size);

    void copyTexture(const GPUTexture& dst, const GPUTexture& src);

    void pushDebugGroupLabel(const char* label, uint32_t colorRGBA) const;
    void insertDebugEventLabel(const char* label, uint32_t colorRGBA) const;
    void popDebugGroupLabel() const;

private:
    struct Binding
    {
        uint32_t       descriptorIndex = UINT32_MAX;
        DescriptorType type = DESCRIPTOR_TYPE_UNDEFINED;
        Buffer*        pBuffer = nullptr;
        Texture*       pTexture = nullptr;
        Sampler*       pSampler = nullptr;
    };

    CommandList() = default;
    CommandList(const CommandList&) = delete;
    CommandList& operator=(const CommandList&) = delete;
    void         reset(Cmd*, uint64_t gpuProfilerToken);
    void         clear();
    void         beginGpuFrameProfile();
    void         endGpuFrameProfile();
    void         bindDescriptors();
    Buffer*      createUploadBuffer(uint64_t size);
    void         barrier(uint32_t bufferCount, BufferBarrier*, uint32_t textureCount, TextureBarrier*, uint32_t renderTargetCount,
                         RenderTargetBarrier*);
    void         barrier(const Dependencies&, const GPUBuffer* pIndirectBuffer);

    Cmd*                 pCmd = nullptr;
    RenderContext*       pContext = nullptr;
    RootSignature*       pCurrentRootSignature = nullptr;
    uint64_t             gpuProfilerToken = UINT64_MAX;
    uint64_t             gpuTimestampToken = 0;
    Buffer*              pVertexBuffers[MAX_VERTEX_BINDINGS] = {};
    uint32_t             vertexStrides[MAX_VERTEX_BINDINGS] = {};
    uint64_t             vertexOffsets[MAX_VERTEX_BINDINGS] = {};
    uint32_t             vertexBufferCount = 0;
    uint32_t             slot = UINT32_MAX;
    Binding*             bindings = nullptr;
    BufferBarrier*       bufferBarriers = nullptr;
    TextureBarrier*      textureBarriers = nullptr;
    RenderTargetBarrier* renderTargetBarriers = nullptr;
    DescriptorData*      descriptorData[DESCRIPTOR_UPDATE_FREQ_COUNT] = {};
    bool                 bindingsDirty = false;
    friend class RenderContext;
};

struct SubmitHandle
{
    uint64_t id = 0;
    uint32_t slot = 0;
    explicit operator bool() const { return id != 0; }
};

class RenderContext
{
public:
    explicit RenderContext(const ContextDesc&);
    ~RenderContext();
    RenderContext(const RenderContext&) = delete;
    RenderContext& operator=(const RenderContext&) = delete;
    explicit       operator bool() const { return ready; }

    void waitIdle();

    bool              resize(uint32_t width, uint32_t height);
    bool              setVSync(bool enabled);
    CommandList&      acquireCommandList();
    SubmitHandle      submit(CommandList&, const GPUTexture* pPresent = nullptr);
    void              wait(SubmitHandle);
    uint32_t          getWidth() const;
    uint32_t          getHeight() const;
    TinyImageFormat   getColorFormat() const;
    const GPUTexture& getCurrentBackbuffer();
    bool              isSuspended() const;

    GPUBuffer   createBuffer(const BufferDesc&);
    GPUTexture  createTexture(const TextureDesc&);
    GPUSampler  createSampler(const SamplerDesc&);
    GPUShader   createShader(const ShaderDesc&);
    GPUPipeline createGraphicsPipeline(const GraphicsPipelineDesc&);
    GPUPipeline createComputePipeline(const ComputePipelineDesc&);
    bool        getGpuAddress(const GPUBuffer&, uint64_t* pAddress) const;
    bool        updateBuffer(const GPUBuffer&, uint64_t offset, const void* pData, uint64_t size);

private:
    struct CommandSlot
    {
        ~CommandSlot();

        CmdPool*        pCmdPool = nullptr;
        Cmd*            pCmd = nullptr;
        Fence*          pFence = nullptr;
        Semaphore*      pSemaphore = nullptr;
        CommandList     commands = {};
        DescriptorSet** descriptorSets = nullptr;
        Buffer**        transientBuffers = nullptr;
        uint64_t        submitId = 0;
    };

    bool initDevice();
    bool createSwapChain();
    void destroySwapChain();
    void destroyDevice(bool waitForGpu);
    void cleanup();

    ContextDesc               desc = {};
    RendererContext*          pRendererContext = nullptr;
    Renderer*                 pRenderer = nullptr;
    Queue*                    pGraphicsQueue = nullptr;
    SwapChain*                pSwapChain = nullptr;
    CommandSignature*         pDrawIndirectSignature = nullptr;
    CommandSignature*         pDrawIndexedIndirectSignature = nullptr;
    CommandSignature*         pDispatchIndirectSignature = nullptr;
    static constexpr uint32_t maxCommandLists = 16;
    CommandSlot               commandSlots[maxCommandLists] = {};
    Semaphore*                pImageAcquiredSemaphore = nullptr;
    GPUTexture                backbuffers[MAX_SWAPCHAIN_IMAGES] = {};
    uint64_t                  gpuProfilerToken = UINT64_MAX;
    uint64_t                  nextSubmitId = 1;
    uint32_t                  nextCommandSlot = 0;
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
    friend class CommandList;
};

} // namespace hz
