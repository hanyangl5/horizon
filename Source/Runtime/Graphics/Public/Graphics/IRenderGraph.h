/*
 * Copyright (c) 2026 Horizon
 */

#pragma once

#include <cstdint>

#include "RHI/IGraphics.h"

static constexpr uint32_t InvalidHandle = UINT32_MAX;

struct RGTexture
{
    uint32_t mId = InvalidHandle;
    bool     isValid() const { return mId != InvalidHandle; }
};

struct RGBuffer
{
    uint32_t mId = InvalidHandle;
    bool     isValid() const { return mId != InvalidHandle; }
};

// TODO(hyl5): hide impl in private header and only expose necessary APIs in this public header.
enum class DebugEventType : uint32_t
{
    Pass,
    Barrier,
    BindRenderTargets,
    UnbindRenderTargets,
    FinalBarrier,
};

enum class ResourceKind : uint32_t
{
    Unknown,
    Texture,
    Buffer,
};

struct DebugEvent
{
    DebugEventType mType = DebugEventType::Pass;
    ResourceKind   mResourceKind = ResourceKind::Unknown;
    const char*    pPassName = nullptr;
    const char*    pResourceName = nullptr;
    ResourceState  mStateBefore = RESOURCE_STATE_UNDEFINED;
    ResourceState  mStateAfter = RESOURCE_STATE_UNDEFINED;
};

class RenderGraph;

// get render target/texture/buffer from RGPassContext, which can be used in pass execute callback without capturing the RenderGraph pointer
class RGPassContext
{
public:
    Renderer* getRenderer() const;
    uint32_t  getWidth() const;
    uint32_t  getHeight() const;
    uint32_t  getNodeIndex() const;

    RenderTarget* getRenderTarget(RGTexture handle) const;
    Texture*      getTexture(RGTexture handle) const;
    Buffer*       getBuffer(RGBuffer handle) const;

private:
    friend class RenderGraph;
    explicit RGPassContext(RenderGraph& graph);

    RenderGraph* pGraph = nullptr;
};

using PassExecuteCallback = void (*)(Cmd*, const RGPassContext&, void*);

class RGPassBuilder
{
public:
    RGPassBuilder& writeRenderTarget(RGTexture handle, uint32_t colorIndex, LoadActionType loadAction = LOAD_ACTION_LOAD,
                                     StoreActionType storeAction = STORE_ACTION_STORE, ClearValue clearValue = {},
                                     bool overrideClearValue = false);
    RGPassBuilder& writeDepthStencil(RGTexture handle, LoadActionType loadAction = LOAD_ACTION_LOAD,
                                     StoreActionType storeAction = STORE_ACTION_STORE, ClearValue clearValue = {},
                                     bool overrideClearValue = false);

    RGPassBuilder& read(RGTexture handle, ResourceState state);
    RGPassBuilder& write(RGTexture handle, ResourceState state);
    RGPassBuilder& read(RGBuffer handle, ResourceState state);
    RGPassBuilder& write(RGBuffer handle, ResourceState state);

    RGPassBuilder& setExecute(PassExecuteCallback callback, void* pUserData = nullptr);

private:
    friend class RenderGraph;
    RGPassBuilder(RenderGraph& graph, uint32_t passIndex);

    RenderGraph* pGraph = nullptr;
    uint32_t     mPassIndex = InvalidHandle;
};

class RenderGraph
{
public:
    RenderGraph();
    ~RenderGraph();

    RenderGraph(const RenderGraph&) = delete;
    RenderGraph& operator=(const RenderGraph&) = delete;

    void beginFrame(Renderer* pRenderer, uint32_t width, uint32_t height, uint32_t nodeIndex, uint32_t frameResourceIndex = 0,
                    Fence* pFrameFence = nullptr);

    RGTexture importRenderTarget(const char* pName, RenderTarget* pRenderTarget, ResourceState currentState, ResourceState finalState);
    RGTexture importTexture(const char* pName, Texture* pTexture, ResourceState currentState, ResourceState finalState);
    RGBuffer  importBuffer(const char* pName, Buffer* pBuffer, ResourceState currentState, ResourceState finalState);
    RGTexture createRenderTarget(const char* pName, const RenderTargetDesc* desc);
    RGTexture createTexture(const char* pName, const TextureDesc* desc);
    RGBuffer  createBuffer(const char* pName, const BufferDesc* desc);

    RGPassBuilder addRasterPass(const char* pName);
    RGPassBuilder addComputePass(const char* pName);
    RGPassBuilder addCopyPass(const char* pName);
    RGPassBuilder addRayTracingPass(const char* pName);

    void execute(Cmd* pCmd);
    void endFrame(Cmd* pCmd);
    void reset();

    RenderTarget* getRenderTarget(RGTexture handle) const;
    Texture*      getTexture(RGTexture handle) const;
    Buffer*       getBuffer(RGBuffer handle) const;

    uint32_t          buildExecutionPlan();
    const DebugEvent* getLastDebugEvents() const;
    uint32_t          getLastDebugEventCount() const;
    bool              isResourceUsed(RGTexture handle) const;
    bool              isResourceUsed(RGBuffer handle) const;
    bool              isResourceAllocated(RGTexture handle) const;
    bool              isResourceAllocated(RGBuffer handle) const;
    bool              wasImportedResourceWritten(RGTexture handle) const;
    bool              wasImportedResourceWritten(RGBuffer handle) const;
    uint32_t          getResourceFirstUse(RGTexture handle) const;
    uint32_t          getResourceFirstUse(RGBuffer handle) const;
    uint32_t          getResourceLastUse(RGTexture handle) const;
    uint32_t          getResourceLastUse(RGBuffer handle) const;

private:
    friend class RGPassBuilder;
    friend class RGPassContext;

    struct Impl;

    void reclaimCompletedTransientResources(Fence* pFrameFence);
    void retireActiveTransientResources();
    void releaseTransientResources();
    void clearPassStorage();
    void prepareInternalResources();
    void recordPassEvents(uint32_t passIndex, DebugEvent** ppEvents, ResourceState** ppStates, bool** ppLastWrites,
                          bool includeBindEvents) const;
    void recordFinalEvents(DebugEvent** ppEvents, ResourceState** ppStates, bool** ppLastWrites) const;
    void resetResourceStates();

    struct ColorAttachment
    {
        RGTexture       mHandle;
        LoadActionType  mLoadAction = LOAD_ACTION_LOAD;
        StoreActionType mStoreAction = STORE_ACTION_STORE;
        ClearValue      mClearValue = {};
        bool            mOverrideClearValue = false;
        bool            mBound = false;
    };

    struct DepthAttachment
    {
        RGTexture       mHandle;
        LoadActionType  mLoadAction = LOAD_ACTION_LOAD;
        StoreActionType mStoreAction = STORE_ACTION_STORE;
        ClearValue      mClearValue = {};
        bool            mOverrideClearValue = false;
    };
    struct ResourceUse
    {
        uint32_t      mResourceIndex = InvalidHandle;
        ResourceKind  mKind = ResourceKind::Unknown;
        ResourceState mState = RESOURCE_STATE_UNDEFINED;
        bool          mWrite = false;
    };
    struct ResourceNode
    {
        const char*   pName = nullptr;
        ResourceKind  mKind = ResourceKind::Unknown;
        ResourceState mInitialState = RESOURCE_STATE_UNDEFINED;
        ResourceState mFinalState = RESOURCE_STATE_UNDEFINED;
        bool          mImported = false;
        bool          mTransient = false;
        bool          mWrittenImported = false;
        bool          mUsed = false;
        uint32_t      mFirstUse = InvalidHandle;
        uint32_t      mLastUse = InvalidHandle;

        RenderTargetDesc mRenderTargetDesc = {};
        TextureDesc      mTextureDesc = {};
        BufferDesc       mBufferDesc = {};
        bool             mHasRenderTargetDesc = false;
        bool             mHasTextureDesc = false;
        bool             mHasBufferDesc = false;

        Texture*      pTexture = nullptr;
        RenderTarget* pRenderTarget = nullptr;
        Buffer*       pBuffer = nullptr;
    };

    struct TransientResource
    {
        const char*   pName = nullptr;
        ResourceKind  mKind = ResourceKind::Unknown;
        ResourceState mCurrentState = RESOURCE_STATE_UNDEFINED;
        uint32_t      mFrameResourceIndex = 0;

        RenderTargetDesc mRenderTargetDesc = {};
        TextureDesc      mTextureDesc = {};
        BufferDesc       mBufferDesc = {};
        bool             mHasRenderTargetDesc = false;
        bool             mHasTextureDesc = false;
        bool             mHasBufferDesc = false;

        Texture*      pTexture = nullptr;
        RenderTarget* pRenderTarget = nullptr;
        Buffer*       pBuffer = nullptr;
    };

    struct PassNode
    {
        const char*         pName = nullptr;
        ColorAttachment*    pColorAttachments = nullptr;
        bool                mHasDepthAttachment = false;
        DepthAttachment     mDepthAttachment;
        ResourceUse*        pReads = nullptr;
        ResourceUse*        pWrites = nullptr;
        PassExecuteCallback pExecute = nullptr;
        void*               pUserData = nullptr;
    };

    Renderer*          pRenderer = nullptr;
    uint32_t           mWidth = 0;
    uint32_t           mHeight = 0;
    uint32_t           mNodeIndex = 0;
    uint32_t           mFrameResourceIndex = 0;
    ResourceNode*      pResources = nullptr;
    PassNode*          pPasses = nullptr;
    ResourceState*     pResourceStates = nullptr;
    bool*              pResourceLastWrites = nullptr;
    DebugEvent*        pLastDebugEvents = nullptr;
    TransientResource* pAvailableTransientResources = nullptr;
    TransientResource* pRetiredTransientResources = nullptr;
};

#define RDG_EXECUTE(UserType, userData, ...)                         \
    [](Cmd* pPassCmd, const RGPassContext& context, void* pUserData) \
    {                                                                \
        UserType* self = static_cast<UserType*>(pUserData);          \
        (void)self;                                                  \
        __VA_ARGS__                                                  \
    },                                                               \
        userData
