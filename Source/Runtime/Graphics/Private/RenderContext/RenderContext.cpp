/* Copyright (c) 2026 Horizon */

#include "Graphics/RenderContext.h"

#include "Profiler/IProfiler.h"
#include "../../../RHI/Private/RendererResourceAPI.h"
#include "Resources/IResourceLoader.h"

#include <string.h>

#include "Core/ILog.h"
#include <ThirdParty/stb/stb_ds.h>

namespace hz
{
RenderContext::CommandSlot::~CommandSlot()
{
    arrfree(descriptorSets);
    arrfree(transientBuffers);
}

RenderContext::~RenderContext() { cleanup(); }

void RenderContext::waitIdle() { waitQueueIdle(pGraphicsQueue); }

bool RenderContext::initDevice()
{
    const RendererContextDesc contextDesc = {
        .mEnableGpuBasedValidation = desc.enableGpuValidation,
    };
    initRendererContext(appName, &contextDesc, &pRendererContext);
    if (!pRendererContext)
        return false;

    const RendererDesc rendererDesc = {
        .mShaderTarget = SHADER_TARGET_6_6,
        .pContext = pRendererContext,
        .mEnableGpuBasedValidation = desc.enableGpuValidation,
    };
    initRenderer(appName, &rendererDesc, &pRenderer);
    if (!pRenderer)
        return false;

    QueueDesc queueDesc = {
        .mType = QUEUE_TYPE_GRAPHICS,
        .mFlag = QUEUE_FLAG_NONE,
        .mPriority = QUEUE_PRIORITY_NORMAL,
        .pName = "RenderContext Graphics Queue",
    };
    addQueue(pRenderer, &queueDesc, &pGraphicsQueue);
    if (!pGraphicsQueue)
        return false;

    IndirectArgumentDescriptor drawArgument = { .mType = INDIRECT_DRAW };
    CommandSignatureDesc       drawSignatureDesc = {
              .pArgDescs = &drawArgument,
              .mIndirectArgCount = 1,
              .mPacked = true,
    };
    addIndirectCommandSignature(pRenderer, &drawSignatureDesc, &pDrawIndirectSignature);

    IndirectArgumentDescriptor drawIndexedArgument = { .mType = INDIRECT_DRAW_INDEX };
    CommandSignatureDesc       drawIndexedSignatureDesc = {
              .pArgDescs = &drawIndexedArgument,
              .mIndirectArgCount = 1,
              .mPacked = true,
    };
    addIndirectCommandSignature(pRenderer, &drawIndexedSignatureDesc, &pDrawIndexedIndirectSignature);

    IndirectArgumentDescriptor dispatchArgument = { .mType = INDIRECT_DISPATCH };
    CommandSignatureDesc       dispatchSignatureDesc = {
              .pArgDescs = &dispatchArgument,
              .mIndirectArgCount = 1,
              .mPacked = true,
    };
    addIndirectCommandSignature(pRenderer, &dispatchSignatureDesc, &pDispatchIndirectSignature);
    ASSERT(pDrawIndirectSignature && pDrawIndexedIndirectSignature && pDispatchIndirectSignature);

    gpuProfilerToken = PROFILE_INVALID_TOKEN;
    if (desc.enableGpuProfiler)
    {
        Queue*       queues[] = { pGraphicsQueue };
        const char*  names[] = { appName };
        ProfilerDesc profilerDesc = {
            .pRenderer = pRenderer,
            .ppQueues = queues,
            .ppProfilerNames = names,
            .pProfileTokens = &gpuProfilerToken,
            .mGpuProfilerCount = 1,
        };
        initProfiler(&profilerDesc);
        profilerInitialized = true;
    }

    ResourceLoaderDesc loaderDesc = gDefaultResourceLoaderDesc;
    loaderDesc.mBufferSize = 16u * 1024u * 1024u;
    initResourceLoaderInterface(pRenderer, &loaderDesc);
    resourceLoaderInitialized = true;

    addSemaphore(pRenderer, &pImageAcquiredSemaphore);
    ASSERT(pImageAcquiredSemaphore);
    for (uint32_t i = 0; i < maxCommandLists; ++i)
    {
        CommandSlot& slot = commandSlots[i];
        CmdPoolDesc  poolDesc = {
             .pQueue = pGraphicsQueue,
             .mTransient = false,
        };
        addCmdPool(pRenderer, &poolDesc, &slot.pCmdPool);
        ASSERT(slot.pCmdPool);

        CmdDesc cmdDesc = { .pPool = slot.pCmdPool };
        addCmd(pRenderer, &cmdDesc, &slot.pCmd);
        addFence(pRenderer, &slot.pFence);
        addSemaphore(pRenderer, &slot.pSemaphore);
        ASSERT(slot.pCmd && slot.pFence && slot.pSemaphore);
        slot.commands.pContext = this;
        slot.commands.slot = i;
    }

    ready = true;
    return true;
}

bool RenderContext::createSwapChain()
{
    ASSERT(ready && !pSwapChain);
    Queue*        queues[] = { pGraphicsQueue };
    SwapChainDesc swapDesc = {
        .mWindowHandle = desc.windowHandle,
        .ppPresentQueues = queues,
        .mPresentQueueCount = 1,
        .mImageCount = desc.imageCount,
        .mWidth = desc.width,
        .mHeight = desc.height,
        .mColorFormat = desc.colorFormat,
        .mEnableVsync = desc.enableVSync,
        .mColorSpace = desc.colorSpace,
    };
    if (swapDesc.mColorFormat == TinyImageFormat_UNDEFINED)
        swapDesc.mColorFormat = getSupportedSwapchainFormat(pRenderer, &swapDesc, swapDesc.mColorSpace);
    addSwapChain(pRenderer, &swapDesc, &pSwapChain);
    if (pSwapChain)
    {
        for (uint32_t i = 0; i < pSwapChain->mImageCount; ++i)
        {
            RenderTarget* target = pSwapChain->ppRenderTargets[i];
            backbuffers[i] = GPUTexture(nullptr, target->pTexture, target, RESOURCE_STATE_PRESENT, false);
        }
    }
    return pSwapChain != nullptr;
}

void RenderContext::destroySwapChain()
{
    for (const CommandSlot& slot : commandSlots)
        ASSERT(!slot.commands.pCmd);
    ASSERT(!imageAcquired);
    if (pSwapChain)
    {
        waitQueueIdle(pGraphicsQueue);
        for (GPUTexture& backbuffer : backbuffers)
            backbuffer.destroy();
        removeSwapChain(pRenderer, pSwapChain);
        pSwapChain = nullptr;
    }
    swapchainImageIndex = 0;
    imageAcquireWaitPending = false;
}

void RenderContext::cleanup()
{
    for (const CommandSlot& slot : commandSlots)
        ASSERT(!slot.commands.pCmd);
    destroySwapChain();
    destroyDevice(true);
}

void RenderContext::destroyDevice(bool waitForGpu)
{
    ready = false;
    if (waitForGpu && pGraphicsQueue)
        waitQueueIdle(pGraphicsQueue);
    if (profilerInitialized)
    {
        exitProfiler();
        profilerInitialized = false;
        gpuProfilerToken = PROFILE_INVALID_TOKEN;
    }
    if (resourceLoaderInitialized)
    {
        exitResourceLoaderInterface(pRenderer);
        resourceLoaderInitialized = false;
    }
    if (pRenderer)
    {
        for (CommandSlot& slot : commandSlots)
        {
            for (uint32_t i = 0; i < (uint32_t)arrlen(slot.descriptorSets); ++i)
                removeDescriptorSet(pRenderer, slot.descriptorSets[i]);
            arrsetlen(slot.descriptorSets, 0);
            for (uint32_t i = 0; i < (uint32_t)arrlen(slot.transientBuffers); ++i)
                removeBuffer(pRenderer, slot.transientBuffers[i]);
            arrsetlen(slot.transientBuffers, 0);
            if (slot.pSemaphore)
                removeSemaphore(pRenderer, slot.pSemaphore);
            if (slot.pFence)
                removeFence(pRenderer, slot.pFence);
            if (slot.pCmd)
                removeCmd(pRenderer, slot.pCmd);
            if (slot.pCmdPool)
                removeCmdPool(pRenderer, slot.pCmdPool);
            slot.pSemaphore = nullptr;
            slot.pFence = nullptr;
            slot.pCmd = nullptr;
            slot.pCmdPool = nullptr;
            slot.submitId = 0;
            slot.commands.clear();
        }
        if (pImageAcquiredSemaphore)
        {
            removeSemaphore(pRenderer, pImageAcquiredSemaphore);
            pImageAcquiredSemaphore = nullptr;
        }
        if (pDrawIndirectSignature)
        {
            removeIndirectCommandSignature(pRenderer, pDrawIndirectSignature);
            pDrawIndirectSignature = nullptr;
        }
        if (pDrawIndexedIndirectSignature)
        {
            removeIndirectCommandSignature(pRenderer, pDrawIndexedIndirectSignature);
            pDrawIndexedIndirectSignature = nullptr;
        }
        if (pDispatchIndirectSignature)
        {
            removeIndirectCommandSignature(pRenderer, pDispatchIndirectSignature);
            pDispatchIndirectSignature = nullptr;
        }
    }
    if (pGraphicsQueue)
    {
        removeQueue(pRenderer, pGraphicsQueue);
        pGraphicsQueue = nullptr;
    }
    if (pRenderer)
    {
        exitRenderer(pRenderer);
        pRenderer = nullptr;
    }
    if (pRendererContext)
    {
        exitRendererContext(pRendererContext);
        pRendererContext = nullptr;
    }
}

bool RenderContext::init(const ContextDesc& input)
{
    ASSERT(!ready && !pRendererContext && !pRenderer && !pGraphicsQueue && !pSwapChain && !pImageAcquiredSemaphore && !resourceCount);
    for (const CommandSlot& slot : commandSlots)
        ASSERT(!slot.commands.pCmd);
    ASSERT(input.pAppName && input.pAppName[0] && strlen(input.pAppName) < sizeof(appName));
    ASSERT(!input.imageCount || input.imageCount <= MAX_SWAPCHAIN_IMAGES);
    if (!input.pAppName || !input.pAppName[0] || strlen(input.pAppName) >= sizeof(appName) || input.imageCount > MAX_SWAPCHAIN_IMAGES)
        return false;

    desc = input;
    desc.imageCount = input.imageCount ? input.imageCount : 3;
    memcpy(appName, input.pAppName, strlen(input.pAppName) + 1);
    if (!initDevice())
    {
        destroyDevice(false);
        return false;
    }

    suspended = !input.width || !input.height;
    const bool initialized = suspended || createSwapChain();
    ASSERT(initialized);
    if (!initialized)
        cleanup();
    return initialized;
}

bool RenderContext::resize(uint32_t width, uint32_t height)
{
    for (const CommandSlot& slot : commandSlots)
        ASSERT(!slot.commands.pCmd);
    ASSERT(!imageAcquired);
    if (!suspended && desc.width == width && desc.height == height)
        return true;

    destroySwapChain();
    desc.width = width;
    desc.height = height;
    suspended = !width || !height;
    if (suspended)
        return true;

    const bool created = createSwapChain();
    ASSERT(created);
    if (!created)
    {
        destroySwapChain();
        suspended = true;
    }
    return created;
}

bool RenderContext::setVSync(bool enabled)
{
    for (const CommandSlot& slot : commandSlots)
        ASSERT(!slot.commands.pCmd);
    if (desc.enableVSync == enabled)
        return true;
    desc.enableVSync = enabled;
    if (suspended)
        return true;

    ASSERT(pSwapChain);
    toggleVSync(pRenderer, &pSwapChain);
    const bool updated = (pSwapChain->mEnableVsync != 0) == enabled;
    ASSERT(updated);
    return updated;
}

CommandList& RenderContext::acquireCommandList()
{
    ASSERT(ready);
    for (const CommandSlot& slot : commandSlots)
        ASSERT(!slot.commands.pCmd);

    CommandSlot* available = nullptr;
    for (uint32_t offset = 0; offset < maxCommandLists; ++offset)
    {
        CommandSlot& slot = commandSlots[(nextCommandSlot + offset) % maxCommandLists];
        if (slot.submitId)
        {
            FenceStatus status = FENCE_STATUS_NOTSUBMITTED;
            getFenceStatus(pRenderer, slot.pFence, &status);
            if (status == FENCE_STATUS_COMPLETE)
                slot.submitId = 0;
        }
        if (!slot.submitId)
        {
            available = &slot;
            break;
        }
    }

    if (!available)
    {
        available = &commandSlots[nextCommandSlot];
        waitForFences(pRenderer, 1, &available->pFence);
        available->submitId = 0;
    }

    for (uint32_t i = 0; i < (uint32_t)arrlen(available->descriptorSets); ++i)
        removeDescriptorSet(pRenderer, available->descriptorSets[i]);
    arrsetlen(available->descriptorSets, 0);
    for (uint32_t i = 0; i < (uint32_t)arrlen(available->transientBuffers); ++i)
        removeBuffer(pRenderer, available->transientBuffers[i]);
    arrsetlen(available->transientBuffers, 0);
    resetCmdPool(pRenderer, available->pCmdPool);
    beginCmd(available->pCmd);
    available->commands.reset(available->pCmd, gpuProfilerToken);
    available->commands.beginGpuFrameProfile();
    nextCommandSlot = (available->commands.slot + 1) % maxCommandLists;
    return available->commands;
}

SubmitHandle RenderContext::submit(CommandList& commands, const GPUTexture* pPresent)
{
    ASSERT(commands.slot < maxCommandLists);
    CommandSlot& slot = commandSlots[commands.slot];
    ASSERT(&commands == &slot.commands && commands.pCmd == slot.pCmd && !slot.submitId);
    if (pPresent)
    {
        ASSERT(imageAcquired && pPresent == &backbuffers[swapchainImageIndex]);
        RenderTargetBarrier presentBarrier = {
            .pRenderTarget = pPresent->pRenderTarget,
            .mCurrentState = pPresent->state,
            .mNewState = RESOURCE_STATE_PRESENT,
        };
        commands.barrier(0, nullptr, 0, nullptr, 1, &presentBarrier);
        pPresent->state = RESOURCE_STATE_PRESENT;
    }
    commands.endGpuFrameProfile();
    endCmd(slot.pCmd);

    Semaphore*      waitSemaphores[] = { pImageAcquiredSemaphore };
    QueueSubmitDesc submitDesc = {
        .ppCmds = &slot.pCmd,
        .pSignalFence = slot.pFence,
        .ppWaitSemaphores = imageAcquireWaitPending ? waitSemaphores : nullptr,
        .ppSignalSemaphores = pPresent ? &slot.pSemaphore : nullptr,
        .mCmdCount = 1,
        .mWaitSemaphoreCount = imageAcquireWaitPending ? 1u : 0u,
        .mSignalSemaphoreCount = pPresent ? 1u : 0u,
    };
    queueSubmit(pGraphicsQueue, &submitDesc);
    imageAcquireWaitPending = false;

    if (pPresent)
    {
        QueuePresentDesc presentDesc = {
            .pSwapChain = pSwapChain,
            .ppWaitSemaphores = &slot.pSemaphore,
            .mWaitSemaphoreCount = 1,
            .mIndex = (uint8_t)swapchainImageIndex,
            .mSubmitDone = true,
        };
        queuePresent(pGraphicsQueue, &presentDesc);
        imageAcquired = false;
        flipProfiler();
    }

    const SubmitHandle handle = { .id = nextSubmitId++, .slot = commands.slot };
    slot.submitId = handle.id;
    commands.clear();
    return handle;
}

void RenderContext::wait(SubmitHandle handle)
{
    ASSERT(handle && handle.slot < maxCommandLists);
    CommandSlot& slot = commandSlots[handle.slot];
    if (slot.submitId == handle.id)
        waitForFences(pRenderer, 1, &slot.pFence);
}

uint32_t          RenderContext::getWidth() const { return desc.width; }
uint32_t          RenderContext::getHeight() const { return desc.height; }
TinyImageFormat   RenderContext::getColorFormat() const { return desc.colorFormat; }
const GPUTexture& RenderContext::getCurrentBackbuffer()
{
    ASSERT(!suspended && pSwapChain);
    if (!imageAcquired)
    {
        acquireNextImage(pRenderer, pSwapChain, pImageAcquiredSemaphore, nullptr, &swapchainImageIndex);
        ASSERT(swapchainImageIndex < pSwapChain->mImageCount);
        imageAcquired = true;
        imageAcquireWaitPending = true;
    }
    return backbuffers[swapchainImageIndex];
}
bool RenderContext::isSuspended() const { return suspended; }

} // namespace hz
