#include "Graphics/IRenderGraph.h"

#include "Core/ILog.h"
#include "Runtime/RHI/Private/RendererResourceAPI.h"
#include <ThirdParty/stb/stb_ds.h>

//TODO(hyl5): remove lots of defensive code or move them to debug only

namespace
{

ResourceState getActualRenderTargetStartState(TinyImageFormat format, ResourceState requestedState)
{
    const bool isDepth = format == TinyImageFormat_D32_SFLOAT || format == TinyImageFormat_D24_UNORM_S8_UINT ||
                         format == TinyImageFormat_D32_SFLOAT_S8_UINT;
    const ResourceState attachmentState = isDepth ? RESOURCE_STATE_DEPTH_WRITE : RESOURCE_STATE_RENDER_TARGET;
    const ResourceState creationState = requestedState | attachmentState;
    return creationState > attachmentState ? (creationState & static_cast<ResourceState>(~attachmentState)) : attachmentState;
}

bool bufferDescMatches(const BufferDesc& a, const BufferDesc& b)
{
    return a.mSize == b.mSize && a.mFirstElement == b.mFirstElement && a.mElementCount == b.mElementCount &&
           a.mStructStride == b.mStructStride && a.mAlignment == b.mAlignment && a.mMemoryUsage == b.mMemoryUsage &&
           a.mFlags == b.mFlags && a.mQueueType == b.mQueueType && a.mFormat == b.mFormat &&
           a.mDescriptors == b.mDescriptors && a.mNodeIndex == b.mNodeIndex;
}

bool textureDescMatches(const TextureDesc& a, const TextureDesc& b)
{
    return a.mFlags == b.mFlags && a.mWidth == b.mWidth && a.mHeight == b.mHeight && a.mDepth == b.mDepth &&
           a.mArraySize == b.mArraySize && a.mMipLevels == b.mMipLevels && a.mSampleCount == b.mSampleCount &&
           a.mSampleQuality == b.mSampleQuality && a.mFormat == b.mFormat && a.mDescriptors == b.mDescriptors &&
           a.mNodeIndex == b.mNodeIndex;
}

bool renderTargetDescMatches(const RenderTargetDesc& a, const RenderTargetDesc& b)
{
    return a.mFlags == b.mFlags && a.mWidth == b.mWidth && a.mHeight == b.mHeight && a.mDepth == b.mDepth &&
           a.mArraySize == b.mArraySize && a.mMipLevels == b.mMipLevels && a.mSampleCount == b.mSampleCount &&
           a.mFormat == b.mFormat && a.mSampleQuality == b.mSampleQuality && a.mDescriptors == b.mDescriptors &&
           a.mNodeIndex == b.mNodeIndex;
}

} // namespace

RGPassContext::RGPassContext(RenderGraph& graph): pGraph(&graph) {}

Renderer* RGPassContext::getRenderer() const { return pGraph ? pGraph->pRenderer : nullptr; }
uint32_t RGPassContext::getWidth() const { return pGraph ? pGraph->mWidth : 0; }
uint32_t RGPassContext::getHeight() const { return pGraph ? pGraph->mHeight : 0; }
uint32_t RGPassContext::getNodeIndex() const { return pGraph ? pGraph->mNodeIndex : 0; }
RenderTarget* RGPassContext::getRenderTarget(RGTexture handle) const { return pGraph ? pGraph->getRenderTarget(handle) : nullptr; }
Texture* RGPassContext::getTexture(RGTexture handle) const { return pGraph ? pGraph->getTexture(handle) : nullptr; }
Buffer* RGPassContext::getBuffer(RGBuffer handle) const { return pGraph ? pGraph->getBuffer(handle) : nullptr; }

RGPassBuilder::RGPassBuilder(RenderGraph& graph, uint32_t passIndex): pGraph(&graph), mPassIndex(passIndex) {}

RGPassBuilder& RGPassBuilder::writeRenderTarget(RGTexture handle, uint32_t colorIndex, LoadActionType loadAction, StoreActionType storeAction,
                                      ClearValue clearValue, bool overrideClearValue)
{
    if (!pGraph || !handle.isValid())
        return *this;
    if (colorIndex >= MAX_RENDER_TARGET_ATTACHMENTS)
    {
        LOGF(eERROR, "RenderGraph color attachment index %u exceeds the maximum of %u", colorIndex, MAX_RENDER_TARGET_ATTACHMENTS);
        return *this;
    }

    RenderGraph::PassNode& pass = pGraph->pPasses[mPassIndex];
    const uint32_t oldCount = static_cast<uint32_t>(arrlenu(pass.pColorAttachments));
    if (oldCount <= colorIndex)
    {
        arrsetlen(pass.pColorAttachments, colorIndex + 1);
        for (uint32_t i = oldCount; i <= colorIndex; ++i)
            pass.pColorAttachments[i] = {};
    }

    pass.pColorAttachments[colorIndex] = { handle, loadAction, storeAction, clearValue, overrideClearValue, true };
    arrpush(pass.pWrites, (RenderGraph::ResourceUse{ handle.mId, ResourceKind::Texture, RESOURCE_STATE_RENDER_TARGET, true }));
    if (handle.mId < arrlenu(pGraph->pResources) && pGraph->pResources[handle.mId].mImported)
        pGraph->pResources[handle.mId].mWrittenImported = true;
    return *this;
}

RGPassBuilder& RGPassBuilder::writeDepthStencil(RGTexture handle, LoadActionType loadAction, StoreActionType storeAction,
                                     ClearValue clearValue, bool overrideClearValue)
{
    if (!pGraph || !handle.isValid())
        return *this;

    RenderGraph::PassNode& pass = pGraph->pPasses[mPassIndex];
    pass.mHasDepthAttachment = true;
    pass.mDepthAttachment = { handle, loadAction, storeAction, clearValue, overrideClearValue };
    arrpush(pass.pWrites, (RenderGraph::ResourceUse{ handle.mId, ResourceKind::Texture, RESOURCE_STATE_DEPTH_WRITE, true }));
    if (handle.mId < arrlenu(pGraph->pResources) && pGraph->pResources[handle.mId].mImported)
        pGraph->pResources[handle.mId].mWrittenImported = true;
    return *this;
}

RGPassBuilder& RGPassBuilder::read(RGBuffer handle, ResourceState state)
{
    if (pGraph && handle.isValid())
        arrpush(pGraph->pPasses[mPassIndex].pReads, (RenderGraph::ResourceUse{ handle.mId, ResourceKind::Buffer, state, false }));
    return *this;
}


RGPassBuilder& RGPassBuilder::read(RGTexture handle, ResourceState state)
{
    if (pGraph && handle.isValid())
        arrpush(pGraph->pPasses[mPassIndex].pReads, (RenderGraph::ResourceUse{ handle.mId, ResourceKind::Texture, state, false }));
    return *this;
}



RGPassBuilder& RGPassBuilder::write(RGBuffer handle, ResourceState state)
{
    if (pGraph && handle.isValid())
    {
        arrpush(pGraph->pPasses[mPassIndex].pWrites, (RenderGraph::ResourceUse{ handle.mId, ResourceKind::Buffer, state, true }));
        if (handle.mId < arrlenu(pGraph->pResources) && pGraph->pResources[handle.mId].mImported)
            pGraph->pResources[handle.mId].mWrittenImported = true;
    }
    return *this;
}


RGPassBuilder& RGPassBuilder::write(RGTexture handle, ResourceState state)
{
    if (pGraph && handle.isValid())
    {
        arrpush(pGraph->pPasses[mPassIndex].pWrites, (RenderGraph::ResourceUse{ handle.mId, ResourceKind::Texture, state, true }));
        if (handle.mId < arrlenu(pGraph->pResources) && pGraph->pResources[handle.mId].mImported)
            pGraph->pResources[handle.mId].mWrittenImported = true;
    }
    return *this;
}

RGPassBuilder& RGPassBuilder::setExecute(PassExecuteCallback callback, void* pUserData)
{
    if (pGraph)
    {
        RenderGraph::PassNode& pass = pGraph->pPasses[mPassIndex];
        pass.pExecute = callback;
        pass.pUserData = pUserData;
    }
    return *this;
}

RenderGraph::RenderGraph() = default;

RenderGraph::~RenderGraph()
{
    reset();
}

void RenderGraph::beginFrame(Renderer* pInRenderer, uint32_t width, uint32_t height, uint32_t nodeIndex, uint32_t frameResourceIndex,
                             Fence* pFrameFence)
{
    retireActiveTransientResources();
    pRenderer = pInRenderer;
    mFrameResourceIndex = frameResourceIndex;
    reclaimCompletedTransientResources(pFrameFence);
    clearPassStorage();
    arrsetlen(pResources, 0);
    arrsetlen(pResourceStates, 0);
    arrsetlen(pResourceLastWrites, 0);
    arrsetlen(pLastDebugEvents, 0);
    mWidth = width;
    mHeight = height;
    mNodeIndex = nodeIndex;
}

RGTexture RenderGraph::importRenderTarget(const char* pName, RenderTarget* pRenderTarget, ResourceState currentState,
                                             ResourceState finalState)
{
    if (!pRenderTarget)
        return {};

    ResourceNode node = {};
    node.pName = pName ? pName : "";
    node.mKind = ResourceKind::Texture;
    node.mInitialState = currentState;
    node.mFinalState = finalState;
    node.mImported = true;
    node.pRenderTarget = pRenderTarget;
    node.pTexture = pRenderTarget->pTexture;
    arrpush(pResources, node);
    arrpush(pResourceStates, currentState);
    arrpush(pResourceLastWrites, false);
    return { static_cast<uint32_t>(arrlenu(pResources) - 1) };
}

RGTexture RenderGraph::importTexture(const char* pName, Texture* pTexture, ResourceState currentState, ResourceState finalState)
{
    if (!pTexture)
        return {};

    ResourceNode node = {};
    node.pName = pName ? pName : "";
    node.mKind = ResourceKind::Texture;
    node.mInitialState = currentState;
    node.mFinalState = finalState;
    node.mImported = true;
    node.pTexture = pTexture;
    arrpush(pResources, node);
    arrpush(pResourceStates, currentState);
    arrpush(pResourceLastWrites, false);
    return { static_cast<uint32_t>(arrlenu(pResources) - 1) };
}

RGBuffer RenderGraph::importBuffer(const char* pName, Buffer* pBuffer, ResourceState currentState, ResourceState finalState)
{
    if (!pBuffer)
        return {};

    ResourceNode node = {};
    node.pName = pName ? pName : "";
    node.mKind = ResourceKind::Buffer;
    node.mInitialState = currentState;
    node.mFinalState = finalState;
    node.mImported = true;
    node.pBuffer = pBuffer;
    arrpush(pResources, node);
    arrpush(pResourceStates, currentState);
    arrpush(pResourceLastWrites, false);
    return { static_cast<uint32_t>(arrlenu(pResources) - 1) };
}

RGTexture RenderGraph::createRenderTarget(const char* pName, const RenderTargetDesc* desc)
{
    if (!desc)
    {
        LOGF(eERROR, "RenderGraph::createRenderTarget requires a descriptor");
        return {};
    }
    RenderTargetDesc localDesc = *desc;
    localDesc.pName = pName ? pName : localDesc.pName;
    localDesc.mStartState = localDesc.mStartState;
    localDesc.mNodeIndex = localDesc.mNodeIndex ? localDesc.mNodeIndex : mNodeIndex;

    const ResourceState actualInitialState = getActualRenderTargetStartState(localDesc.mFormat, localDesc.mStartState);
    ResourceNode node = {};
    node.pName = localDesc.pName ? localDesc.pName : "";
    node.mKind = ResourceKind::Texture;
    node.mInitialState = actualInitialState;
    node.mFinalState = actualInitialState;
    node.mTransient = true;
    node.mRenderTargetDesc = localDesc;
    node.mRenderTargetDesc.mStartState = actualInitialState;
    node.mHasRenderTargetDesc = true;
    arrpush(pResources, node);
    arrpush(pResourceStates, actualInitialState);
    arrpush(pResourceLastWrites, false);
    return { static_cast<uint32_t>(arrlenu(pResources) - 1) };
}

RGTexture RenderGraph::createTexture(const char* pName, const TextureDesc* desc)
{
    if (!desc)
    {
        LOGF(eERROR, "RenderGraph::createTexture requires a descriptor");
        return {};
    }
    TextureDesc localDesc = *desc;
    localDesc.pName = pName ? pName : localDesc.pName;
    localDesc.mStartState = localDesc.mStartState;
    localDesc.mNodeIndex = localDesc.mNodeIndex ? localDesc.mNodeIndex : mNodeIndex;

    ResourceNode node = {};
    node.pName = localDesc.pName ? localDesc.pName : "";
    node.mKind = ResourceKind::Texture;
    node.mInitialState = localDesc.mStartState;
    node.mFinalState = localDesc.mStartState;
    node.mTransient = true;
    node.mTextureDesc = localDesc;
    node.mHasTextureDesc = true;
    arrpush(pResources, node);
    arrpush(pResourceStates, localDesc.mStartState);
    arrpush(pResourceLastWrites, false);
    return { static_cast<uint32_t>(arrlenu(pResources) - 1) };
}

RGBuffer RenderGraph::createBuffer(const char* pName, const BufferDesc* desc)
{
    if (!desc)
    {
        LOGF(eERROR, "RenderGraph::createBuffer requires a descriptor");
        return {};
    }
    BufferDesc localDesc = *desc;
    localDesc.pName = pName ? pName : localDesc.pName;
    localDesc.mStartState = localDesc.mStartState;
    localDesc.mNodeIndex = localDesc.mNodeIndex ? localDesc.mNodeIndex : mNodeIndex;

    ResourceNode node = {};
    node.pName = localDesc.pName ? localDesc.pName : "";
    node.mKind = ResourceKind::Buffer;
    node.mInitialState = localDesc.mStartState;
    node.mFinalState = localDesc.mStartState;
    node.mTransient = true;
    node.mBufferDesc = localDesc;
    node.mHasBufferDesc = true;
    arrpush(pResources, node);
    arrpush(pResourceStates, localDesc.mStartState);
    arrpush(pResourceLastWrites, false);
    return { static_cast<uint32_t>(arrlenu(pResources) - 1) };
}

RGPassBuilder RenderGraph::addRasterPass(const char* pName)
{
    PassNode pass = {};
    pass.pName = pName ? pName : "";
    arrpush(pPasses, pass);
    return RGPassBuilder(*this, static_cast<uint32_t>(arrlenu(pPasses) - 1));
}

RGPassBuilder RenderGraph::addComputePass(const char* pName)
{
    PassNode pass = {};
    pass.pName = pName ? pName : "";
    arrpush(pPasses, pass);
    return RGPassBuilder(*this, static_cast<uint32_t>(arrlenu(pPasses) - 1));
}

RGPassBuilder RenderGraph::addCopyPass(const char* pName)
{
    PassNode pass = {};
    pass.pName = pName ? pName : "";
    arrpush(pPasses, pass);
    return RGPassBuilder(*this, static_cast<uint32_t>(arrlenu(pPasses) - 1));
}

RGPassBuilder RenderGraph::addRayTracingPass(const char* pName)
{
    PassNode pass = {};
    pass.pName = pName ? pName : "";
    arrpush(pPasses, pass);
    return RGPassBuilder(*this, static_cast<uint32_t>(arrlenu(pPasses) - 1));
}

void RenderGraph::execute(Cmd* pCmd)
{
    arrsetlen(pLastDebugEvents, 0);
    prepareInternalResources();
    resetResourceStates();

    RGPassContext context(*this);
    for (uint32_t passIndex = 0; passIndex < arrlenu(pPasses); ++passIndex)
    {
        const PassNode& pass = pPasses[passIndex];
        BufferBarrier*       pBufferBarriers = nullptr;
        TextureBarrier*      pTextureBarriers = nullptr;
        RenderTargetBarrier* pRtBarriers = nullptr;

        auto addBarrier = [&](const ResourceUse& use)
        {
            if (use.mResourceIndex >= arrlenu(pResourceStates))
                return;

            ResourceNode& resource = pResources[use.mResourceIndex];
            const ResourceState beforeState = pResourceStates[use.mResourceIndex];
            const bool lastUseWasWrite = pResourceLastWrites[use.mResourceIndex];
            const bool needsBarrier =
                beforeState != use.mState || (use.mState == RESOURCE_STATE_UNORDERED_ACCESS && (lastUseWasWrite || use.mWrite));

            if (needsBarrier)
            {
                if (resource.pBuffer)
                    arrpush(pBufferBarriers, (BufferBarrier{ resource.pBuffer, beforeState, use.mState }));
                else if (resource.pRenderTarget)
                    arrpush(pRtBarriers, (RenderTargetBarrier{ resource.pRenderTarget, beforeState, use.mState }));
                else if (resource.pTexture)
                    arrpush(pTextureBarriers, (TextureBarrier{ resource.pTexture, beforeState, use.mState }));

                arrpush(pLastDebugEvents,
                        (DebugEvent{ DebugEventType::Barrier, resource.mKind, pass.pName, resource.pName, beforeState, use.mState }));
            }

            pResourceStates[use.mResourceIndex] = use.mState;
            pResourceLastWrites[use.mResourceIndex] = use.mWrite;
        };

        for (uint32_t i = 0; i < arrlenu(pass.pReads); ++i)
            addBarrier(pass.pReads[i]);
        for (uint32_t i = 0; i < arrlenu(pass.pWrites); ++i)
            addBarrier(pass.pWrites[i]);

        if (pCmd && (arrlenu(pBufferBarriers) || arrlenu(pTextureBarriers) || arrlenu(pRtBarriers)))
        {
            cmdResourceBarrier(pCmd, static_cast<uint32_t>(arrlenu(pBufferBarriers)), pBufferBarriers,
                               static_cast<uint32_t>(arrlenu(pTextureBarriers)), pTextureBarriers,
                               static_cast<uint32_t>(arrlenu(pRtBarriers)), pRtBarriers);
        }

        arrpush(pLastDebugEvents, (DebugEvent{ DebugEventType::Pass, ResourceKind::Unknown, pass.pName }));

        if (pCmd && (arrlenu(pass.pColorAttachments) || pass.mHasDepthAttachment))
        {
            BindRenderTargetsDesc bindDesc = {};
            bindDesc.mRenderTargetCount = static_cast<uint32_t>(arrlenu(pass.pColorAttachments));
            bool bindDescValid = true;
            for (uint32_t i = 0; i < arrlenu(pass.pColorAttachments); ++i)
            {
                const ColorAttachment& attachment = pass.pColorAttachments[i];
                if (!attachment.mBound || !attachment.mHandle.isValid())
                {
                    LOGF(eERROR, "RenderGraph pass %s has a missing color attachment at slot %u", pass.pName ? pass.pName : "<unnamed>", i);
                    bindDescValid = false;
                    continue;
                }

                RenderTarget* pRenderTarget = getRenderTarget(attachment.mHandle);
                if (!pRenderTarget)
                {
                    LOGF(eERROR, "RenderGraph pass %s has an invalid color attachment at slot %u", pass.pName ? pass.pName : "<unnamed>", i);
                    bindDescValid = false;
                    continue;
                }

                bindDesc.mRenderTargets[i] = {
                    .pRenderTarget = pRenderTarget,
                    .mLoadAction = attachment.mLoadAction,
                    .mStoreAction = attachment.mStoreAction,
                    .mClearValue = attachment.mClearValue,
                    .mOverrideClearValue = attachment.mOverrideClearValue,
                };
            }

            if (pass.mHasDepthAttachment)
            {
                const DepthAttachment& attachment = pass.mDepthAttachment;
                RenderTarget* pDepthStencil = getRenderTarget(attachment.mHandle);
                if (!pDepthStencil)
                {
                    LOGF(eERROR, "RenderGraph pass %s has an invalid depth attachment", pass.pName ? pass.pName : "<unnamed>");
                    bindDescValid = false;
                }
                bindDesc.mDepthStencil = {
                    .pDepthStencil = pDepthStencil,
                    .mLoadAction = attachment.mLoadAction,
                    .mStoreAction = attachment.mStoreAction,
                    .mClearValue = attachment.mClearValue,
                    .mOverrideClearValue = attachment.mOverrideClearValue,
                };
            }

            arrpush(pLastDebugEvents, (DebugEvent{ DebugEventType::BindRenderTargets, ResourceKind::Unknown, pass.pName }));
            if (bindDescValid)
                cmdBindRenderTargets(pCmd, &bindDesc);
        }

        if (pCmd && pass.pExecute)
            pass.pExecute(pCmd, context, pass.pUserData);

        if (pCmd && (arrlenu(pass.pColorAttachments) || pass.mHasDepthAttachment))
        {
            cmdBindRenderTargets(pCmd, nullptr);
            arrpush(pLastDebugEvents, (DebugEvent{ DebugEventType::UnbindRenderTargets, ResourceKind::Unknown, pass.pName }));
        }

        arrfree(pBufferBarriers);
        arrfree(pTextureBarriers);
        arrfree(pRtBarriers);
    }
}

void RenderGraph::endFrame(Cmd* pCmd)
{
    if (arrlenu(pResourceStates) != arrlenu(pResources) || arrlenu(pResourceLastWrites) != arrlenu(pResources))
        resetResourceStates();

    RenderTargetBarrier* pRtBarriers = nullptr;
    BufferBarrier*       pBufferBarriers = nullptr;
    TextureBarrier*      pTextureBarriers = nullptr;
    for (uint32_t i = 0; i < arrlenu(pResources); ++i)
    {
        ResourceNode& resource = pResources[i];
        if (!resource.mImported)
            continue;

        const ResourceState beforeState = pResourceStates[i];
        const bool needsBarrier =
            beforeState != resource.mFinalState ||
            (resource.mFinalState == RESOURCE_STATE_UNORDERED_ACCESS && pResourceLastWrites[i]);
        if (!needsBarrier)
            continue;

        if (resource.pBuffer)
            arrpush(pBufferBarriers, (BufferBarrier{ resource.pBuffer, beforeState, resource.mFinalState }));
        else if (resource.pRenderTarget)
            arrpush(pRtBarriers, (RenderTargetBarrier{ resource.pRenderTarget, beforeState, resource.mFinalState }));
        else if (resource.pTexture)
            arrpush(pTextureBarriers, (TextureBarrier{ resource.pTexture, beforeState, resource.mFinalState }));

        arrpush(pLastDebugEvents,
                (DebugEvent{ DebugEventType::FinalBarrier, resource.mKind, nullptr, resource.pName, beforeState, resource.mFinalState }));
        pResourceStates[i] = resource.mFinalState;
        pResourceLastWrites[i] = false;
    }

    if (pCmd && (arrlenu(pRtBarriers) || arrlenu(pBufferBarriers) || arrlenu(pTextureBarriers)))
    {
        cmdResourceBarrier(pCmd, static_cast<uint32_t>(arrlenu(pBufferBarriers)), pBufferBarriers,
                           static_cast<uint32_t>(arrlenu(pTextureBarriers)), pTextureBarriers,
                           static_cast<uint32_t>(arrlenu(pRtBarriers)), pRtBarriers);
    }

    arrfree(pRtBarriers);
    arrfree(pBufferBarriers);
    arrfree(pTextureBarriers);

    retireActiveTransientResources();
}

void RenderGraph::reset()
{
    retireActiveTransientResources();
    releaseTransientResources();
    clearPassStorage();
    arrfree(pPasses);
    arrfree(pResources);
    arrfree(pResourceStates);
    arrfree(pResourceLastWrites);
    arrfree(pLastDebugEvents);
    pRenderer = nullptr;
    mWidth = 0;
    mHeight = 0;
    mNodeIndex = 0;
    mFrameResourceIndex = 0;
}

RenderTarget* RenderGraph::getRenderTarget(RGTexture handle) const
{
    if (!handle.isValid() || handle.mId >= arrlenu(pResources))
        return nullptr;
    return pResources[handle.mId].pRenderTarget;
}

Texture* RenderGraph::getTexture(RGTexture handle) const
{
    if (!handle.isValid() || handle.mId >= arrlenu(pResources))
        return nullptr;
    return pResources[handle.mId].pTexture;
}

Buffer* RenderGraph::getBuffer(RGBuffer handle) const
{
    if (!handle.isValid() || handle.mId >= arrlenu(pResources))
        return nullptr;
    return pResources[handle.mId].pBuffer;
}

uint32_t RenderGraph::buildExecutionPlan()
{
    arrsetlen(pLastDebugEvents, 0);

    ResourceState* pStates = nullptr;
    bool*          pLastWrites = nullptr;
    for (uint32_t i = 0; i < arrlenu(pResources); ++i)
    {
        arrpush(pStates, pResources[i].mInitialState);
        arrpush(pLastWrites, false);
    }

    for (uint32_t passIndex = 0; passIndex < arrlenu(pPasses); ++passIndex)
        recordPassEvents(passIndex, &pLastDebugEvents, &pStates, &pLastWrites, true);
    recordFinalEvents(&pLastDebugEvents, &pStates, &pLastWrites);

    arrfree(pStates);
    arrfree(pLastWrites);
    return static_cast<uint32_t>(arrlenu(pLastDebugEvents));
}

const DebugEvent* RenderGraph::getLastDebugEvents() const
{
    return pLastDebugEvents;
}

uint32_t RenderGraph::getLastDebugEventCount() const
{
    return static_cast<uint32_t>(arrlenu(pLastDebugEvents));
}

bool RenderGraph::isResourceUsed(RGTexture handle) const
{
    return handle.isValid() && handle.mId < arrlenu(pResources) && pResources[handle.mId].mUsed;
}

bool RenderGraph::isResourceUsed(RGBuffer handle) const
{
    return handle.isValid() && handle.mId < arrlenu(pResources) && pResources[handle.mId].mUsed;
}

bool RenderGraph::isResourceAllocated(RGTexture handle) const
{
    return handle.isValid() && handle.mId < arrlenu(pResources) &&
           (pResources[handle.mId].pRenderTarget || pResources[handle.mId].pTexture);
}

bool RenderGraph::isResourceAllocated(RGBuffer handle) const
{
    return handle.isValid() && handle.mId < arrlenu(pResources) && pResources[handle.mId].pBuffer;
}

bool RenderGraph::wasImportedResourceWritten(RGTexture handle) const
{
    return handle.isValid() && handle.mId < arrlenu(pResources) && pResources[handle.mId].mImported &&
           pResources[handle.mId].mWrittenImported;
}

bool RenderGraph::wasImportedResourceWritten(RGBuffer handle) const
{
    return handle.isValid() && handle.mId < arrlenu(pResources) && pResources[handle.mId].mImported &&
           pResources[handle.mId].mWrittenImported;
}

uint32_t RenderGraph::getResourceFirstUse(RGTexture handle) const
{
    return handle.isValid() && handle.mId < arrlenu(pResources) ? pResources[handle.mId].mFirstUse : InvalidHandle;
}

uint32_t RenderGraph::getResourceFirstUse(RGBuffer handle) const
{
    return handle.isValid() && handle.mId < arrlenu(pResources) ? pResources[handle.mId].mFirstUse : InvalidHandle;
}

uint32_t RenderGraph::getResourceLastUse(RGTexture handle) const
{
    return handle.isValid() && handle.mId < arrlenu(pResources) ? pResources[handle.mId].mLastUse : InvalidHandle;
}

uint32_t RenderGraph::getResourceLastUse(RGBuffer handle) const
{
    return handle.isValid() && handle.mId < arrlenu(pResources) ? pResources[handle.mId].mLastUse : InvalidHandle;
}

void RenderGraph::reclaimCompletedTransientResources(Fence* pFrameFence)
{
    if (!pRenderer)
        return;

    bool canReclaim = true;
    if (pFrameFence)
    {
        FenceStatus status = FENCE_STATUS_INCOMPLETE;
        getFenceStatus(pRenderer, pFrameFence, &status);
        canReclaim = status != FENCE_STATUS_INCOMPLETE;
    }
    if (!canReclaim)
        return;

    for (uint32_t i = 0; i < arrlenu(pRetiredTransientResources);)
    {
        if (pRetiredTransientResources[i].mFrameResourceIndex != mFrameResourceIndex)
        {
            ++i;
            continue;
        }

        arrpush(pAvailableTransientResources, pRetiredTransientResources[i]);
        arrdel(pRetiredTransientResources, i);
    }
}

void RenderGraph::retireActiveTransientResources()
{
    for (uint32_t i = 0; i < arrlenu(pResources); ++i)
    {
        ResourceNode& resource = pResources[i];
        if (!resource.mTransient || (!resource.pRenderTarget && !resource.pTexture && !resource.pBuffer))
            continue;

        TransientResource retired = {};
        retired.pName = resource.pName;
        retired.mKind = resource.mKind;
        retired.mCurrentState = i < arrlenu(pResourceStates) ? pResourceStates[i] : resource.mInitialState;
        retired.mFrameResourceIndex = mFrameResourceIndex;
        retired.mRenderTargetDesc = resource.mRenderTargetDesc;
        retired.mTextureDesc = resource.mTextureDesc;
        retired.mBufferDesc = resource.mBufferDesc;
        retired.mHasRenderTargetDesc = resource.mHasRenderTargetDesc;
        retired.mHasTextureDesc = resource.mHasTextureDesc;
        retired.mHasBufferDesc = resource.mHasBufferDesc;
        retired.pTexture = resource.pTexture;
        retired.pRenderTarget = resource.pRenderTarget;
        retired.pBuffer = resource.pBuffer;
        arrpush(pRetiredTransientResources, retired);

        resource.pTexture = nullptr;
        resource.pRenderTarget = nullptr;
        resource.pBuffer = nullptr;
    }
}

void RenderGraph::releaseTransientResources()
{
    if (!pRenderer)
    {
        arrfree(pAvailableTransientResources);
        arrfree(pRetiredTransientResources);
        return;
    }

    auto releaseResource = [&](TransientResource& resource)
    {
        if (resource.pRenderTarget)
        {
            removeRenderTarget(pRenderer, resource.pRenderTarget);
            resource.pRenderTarget = nullptr;
            resource.pTexture = nullptr;
        }
        else if (resource.pTexture)
        {
            removeTexture(pRenderer, resource.pTexture);
            resource.pTexture = nullptr;
        }
        else if (resource.pBuffer)
        {
            removeBuffer(pRenderer, resource.pBuffer);
            resource.pBuffer = nullptr;
        }
    };

    for (uint32_t i = 0; i < arrlenu(pResources); ++i)
    {
        ResourceNode& node = pResources[i];
        if (!node.mTransient)
            continue;

        TransientResource resource = {};
        resource.pRenderTarget = node.pRenderTarget;
        resource.pTexture = node.pTexture;
        resource.pBuffer = node.pBuffer;
        releaseResource(resource);
        node.pRenderTarget = nullptr;
        node.pTexture = nullptr;
        node.pBuffer = nullptr;
    }

    for (uint32_t i = 0; i < arrlenu(pAvailableTransientResources); ++i)
        releaseResource(pAvailableTransientResources[i]);
    for (uint32_t i = 0; i < arrlenu(pRetiredTransientResources); ++i)
        releaseResource(pRetiredTransientResources[i]);

    arrfree(pAvailableTransientResources);
    arrfree(pRetiredTransientResources);
}

void RenderGraph::prepareInternalResources()
{
    for (uint32_t i = 0; i < arrlenu(pResources); ++i)
    {
        pResources[i].mUsed = false;
        pResources[i].mFirstUse = InvalidHandle;
        pResources[i].mLastUse = InvalidHandle;
    }

    auto markUse = [&](uint32_t passIndex, const ResourceUse& use)
    {
        if (use.mResourceIndex >= arrlenu(pResources))
            return;

        ResourceNode& resource = pResources[use.mResourceIndex];
        resource.mUsed = true;
        resource.mFirstUse = resource.mFirstUse == InvalidHandle ? passIndex : resource.mFirstUse;
        resource.mLastUse = passIndex;
    };

    for (uint32_t passIndex = 0; passIndex < arrlenu(pPasses); ++passIndex)
    {
        const PassNode& pass = pPasses[passIndex];
        for (uint32_t i = 0; i < arrlenu(pass.pReads); ++i)
            markUse(passIndex, pass.pReads[i]);
        for (uint32_t i = 0; i < arrlenu(pass.pWrites); ++i)
            markUse(passIndex, pass.pWrites[i]);
    }

    if (!pRenderer)
        return;

    for (uint32_t resourceIndex = 0; resourceIndex < arrlenu(pResources); ++resourceIndex)
    {
        ResourceNode& resource = pResources[resourceIndex];
        if (!resource.mTransient || !resource.mUsed)
            continue;

        bool matched = false;
        for (uint32_t i = 0; i < arrlenu(pAvailableTransientResources); ++i)
        {
            TransientResource& candidate = pAvailableTransientResources[i];
            if (resource.mHasRenderTargetDesc && candidate.mHasRenderTargetDesc &&
                renderTargetDescMatches(resource.mRenderTargetDesc, candidate.mRenderTargetDesc))
            {
                resource.pRenderTarget = candidate.pRenderTarget;
                resource.pTexture = candidate.pTexture;
                resource.mInitialState = candidate.mCurrentState;
                arrdel(pAvailableTransientResources, i);
                matched = true;
                break;
            }

            if (resource.mHasTextureDesc && candidate.mHasTextureDesc &&
                textureDescMatches(resource.mTextureDesc, candidate.mTextureDesc))
            {
                resource.pTexture = candidate.pTexture;
                resource.mInitialState = candidate.mCurrentState;
                arrdel(pAvailableTransientResources, i);
                matched = true;
                break;
            }

            if (resource.mHasBufferDesc && candidate.mHasBufferDesc && bufferDescMatches(resource.mBufferDesc, candidate.mBufferDesc))
            {
                resource.pBuffer = candidate.pBuffer;
                resource.mInitialState = candidate.mCurrentState;
                arrdel(pAvailableTransientResources, i);
                matched = true;
                break;
            }
        }

        if (matched)
            continue;

        if (resource.mHasRenderTargetDesc)
        {
            RenderTargetDesc desc = resource.mRenderTargetDesc;
            desc.pName = resource.pName;
            desc.mNodeIndex = desc.mNodeIndex ? desc.mNodeIndex : mNodeIndex;
            addRenderTarget(pRenderer, &desc, &resource.pRenderTarget);
            if (!resource.pRenderTarget)
            {
                LOGF(eERROR, "RDG failed to create transient render target %s", resource.pName ? resource.pName : "<unnamed>");
                continue;
            }
            resource.pTexture = resource.pRenderTarget->pTexture;
            resource.mInitialState = getActualRenderTargetStartState(desc.mFormat, desc.mStartState);
        }
        else if (resource.mHasTextureDesc)
        {
            TextureDesc desc = resource.mTextureDesc;
            desc.pName = resource.pName;
            desc.mNodeIndex = desc.mNodeIndex ? desc.mNodeIndex : mNodeIndex;
            addTexture(pRenderer, &desc, &resource.pTexture);
            if (!resource.pTexture)
            {
                LOGF(eERROR, "RDG failed to create transient texture %s", resource.pName ? resource.pName : "<unnamed>");
                continue;
            }
            resource.mInitialState = desc.mStartState;
        }
        else if (resource.mHasBufferDesc)
        {
            BufferDesc desc = resource.mBufferDesc;
            desc.pName = resource.pName;
            desc.mNodeIndex = desc.mNodeIndex ? desc.mNodeIndex : mNodeIndex;
            addBuffer(pRenderer, &desc, &resource.pBuffer);
            if (!resource.pBuffer)
            {
                LOGF(eERROR, "RDG failed to create transient buffer %s", resource.pName ? resource.pName : "<unnamed>");
                continue;
            }
            resource.mInitialState = desc.mStartState;
        }
    }
}

void RenderGraph::clearPassStorage()
{
    for (uint32_t i = 0; i < arrlenu(pPasses); ++i)
    {
        arrfree(pPasses[i].pColorAttachments);
        arrfree(pPasses[i].pReads);
        arrfree(pPasses[i].pWrites);
    }
    arrsetlen(pPasses, 0);
}

void RenderGraph::recordPassEvents(uint32_t passIndex, DebugEvent** ppEvents, ResourceState** ppStates, bool** ppLastWrites,
                                   bool includeBindEvents) const
{
    const PassNode& pass = pPasses[passIndex];
    auto recordUse = [&](const ResourceUse& use)
    {
        if (use.mResourceIndex >= arrlenu(*ppStates))
            return;

        const ResourceNode& resource = pResources[use.mResourceIndex];
        const ResourceState beforeState = (*ppStates)[use.mResourceIndex];
        const bool lastUseWasWrite = (*ppLastWrites)[use.mResourceIndex];
        const bool needsBarrier =
            beforeState != use.mState || (use.mState == RESOURCE_STATE_UNORDERED_ACCESS && (lastUseWasWrite || use.mWrite));
        if (needsBarrier)
        {
            arrpush(*ppEvents,
                    (DebugEvent{ DebugEventType::Barrier, resource.mKind, pass.pName, resource.pName, beforeState, use.mState }));
        }

        (*ppStates)[use.mResourceIndex] = use.mState;
        (*ppLastWrites)[use.mResourceIndex] = use.mWrite;
    };

    for (uint32_t i = 0; i < arrlenu(pass.pReads); ++i)
        recordUse(pass.pReads[i]);
    for (uint32_t i = 0; i < arrlenu(pass.pWrites); ++i)
        recordUse(pass.pWrites[i]);

    arrpush(*ppEvents, (DebugEvent{ DebugEventType::Pass, ResourceKind::Unknown, pass.pName }));
    if (includeBindEvents && (arrlenu(pass.pColorAttachments) || pass.mHasDepthAttachment))
    {
        arrpush(*ppEvents, (DebugEvent{ DebugEventType::BindRenderTargets, ResourceKind::Unknown, pass.pName }));
        arrpush(*ppEvents, (DebugEvent{ DebugEventType::UnbindRenderTargets, ResourceKind::Unknown, pass.pName }));
    }
}

void RenderGraph::recordFinalEvents(DebugEvent** ppEvents, ResourceState** ppStates, bool** ppLastWrites) const
{
    for (uint32_t i = 0; i < arrlenu(pResources); ++i)
    {
        const ResourceNode& resource = pResources[i];
        if (!resource.mImported)
            continue;

        const ResourceState beforeState = (*ppStates)[i];
        const bool needsBarrier =
            beforeState != resource.mFinalState || (resource.mFinalState == RESOURCE_STATE_UNORDERED_ACCESS && (*ppLastWrites)[i]);
        if (!needsBarrier)
            continue;

        arrpush(*ppEvents,
                (DebugEvent{ DebugEventType::FinalBarrier, resource.mKind, nullptr, resource.pName, beforeState, resource.mFinalState }));
        (*ppStates)[i] = resource.mFinalState;
        (*ppLastWrites)[i] = false;
    }
}

void RenderGraph::resetResourceStates()
{
    arrsetlen(pResourceStates, 0);
    arrsetlen(pResourceLastWrites, 0);
    for (uint32_t i = 0; i < arrlenu(pResources); ++i)
    {
        arrpush(pResourceStates, pResources[i].mInitialState);
        arrpush(pResourceLastWrites, false);
    }
}
