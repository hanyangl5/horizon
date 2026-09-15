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
RenderContext::CommandSlot::~CommandSlot() { arrfree(transientBuffers); }

RenderContext::RenderContext(const ContextDesc& input)
{
    const bool valid = input.pAppName && input.pAppName[0] && strlen(input.pAppName) < sizeof(appName) &&
                       (!input.imageCount || input.imageCount <= MAX_SWAPCHAIN_IMAGES);
    ASSERT(valid);
    if (!valid)
        return;

    desc = input;
    desc.imageCount = input.imageCount ? input.imageCount : 3;
    memcpy(appName, input.pAppName, strlen(input.pAppName) + 1);

    const bool deviceInitialized = initDevice();
    ASSERT(deviceInitialized);
    if (!deviceInitialized)
    {
        destroyDevice(false);
        return;
    }

    suspended = !input.width || !input.height;
    const bool initialized = suspended || createSwapChain();
    ASSERT(initialized);
    if (!initialized)
        cleanup();
}

RenderContext::~RenderContext() { cleanup(); }

void RenderContext::waitIdle() { waitQueueIdle(pGraphicsQueue); }

bool RenderContext::initDevice()
{
    const RendererContextDesc contextDesc = {
        .enableGpuBasedValidation = desc.enableGpuValidation,
    };
    initRendererContext(appName, &contextDesc, &pRendererContext);
    if (!pRendererContext)
        return false;

    const RendererDesc rendererDesc = {
        .shaderTarget = SHADER_TARGET_6_6,
        .pContext = pRendererContext,
        .enableGpuBasedValidation = desc.enableGpuValidation,
    };
    initRenderer(appName, &rendererDesc, &pRenderer);
    if (!pRenderer)
        return false;

    QueueDesc queueDesc = {
        .type = QUEUE_TYPE_GRAPHICS,
        .flag = QUEUE_FLAG_NONE,
        .priority = QUEUE_PRIORITY_NORMAL,
        .pName = "RenderContext Graphics Queue",
    };
    addQueue(pRenderer, &queueDesc, &pGraphicsQueue);
    if (!pGraphicsQueue)
        return false;

    IndirectArgumentDescriptor drawArgument = { .type = INDIRECT_DRAW };
    CommandSignatureDesc       drawSignatureDesc = {
        .pArgDescs = &drawArgument,
        .indirectArgCount = 1,
        .packed = true,
    };
    addIndirectCommandSignature(pRenderer, &drawSignatureDesc, &pDrawIndirectSignature);

    IndirectArgumentDescriptor drawIndexedArgument = { .type = INDIRECT_DRAW_INDEX };
    CommandSignatureDesc       drawIndexedSignatureDesc = {
        .pArgDescs = &drawIndexedArgument,
        .indirectArgCount = 1,
        .packed = true,
    };
    addIndirectCommandSignature(pRenderer, &drawIndexedSignatureDesc, &pDrawIndexedIndirectSignature);

    IndirectArgumentDescriptor dispatchArgument = { .type = INDIRECT_DISPATCH };
    CommandSignatureDesc       dispatchSignatureDesc = {
        .pArgDescs = &dispatchArgument,
        .indirectArgCount = 1,
        .packed = true,
    };
    addIndirectCommandSignature(pRenderer, &dispatchSignatureDesc, &pDispatchIndirectSignature);
    ASSERT(pDrawIndirectSignature && pDrawIndexedIndirectSignature && pDispatchIndirectSignature);

    gpuProfilerToken = PROFILE_INVALID_TOKEN;
#if defined(ENABLE_TRACY_MEMORY)
    const bool initializeProfiler = true;
#else
    const bool initializeProfiler = desc.enableGpuProfiler;
#endif
    if (initializeProfiler)
    {
        Queue*       queues[] = { pGraphicsQueue };
        const char*  names[] = { appName };
        ProfilerDesc profilerDesc = {
            .pRenderer = pRenderer,
            .ppQueues = queues,
            .ppProfilerNames = names,
            .pProfileTokens = &gpuProfilerToken,
            .gpuProfilerCount = desc.enableGpuProfiler ? 1u : 0u,
        };
        initProfiler(&profilerDesc);
        profilerInitialized = true;
    }

    ResourceLoaderDesc loaderDesc = gDefaultResourceLoaderDesc;
    loaderDesc.bufferSize = 16u * 1024u * 1024u;
    initResourceLoaderInterface(pRenderer, &loaderDesc);
    resourceLoaderInitialized = true;

    addSemaphore(pRenderer, &pImageAcquiredSemaphore);
    ASSERT(pImageAcquiredSemaphore);
    for (uint32_t i = 0; i < maxCommandLists; ++i)
    {
        CommandSlot& slot = commandSlots[i];
        CmdPoolDesc  poolDesc = {
            .pQueue = pGraphicsQueue,
            .transient = false,
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
        .windowHandle = desc.windowHandle,
        .ppPresentQueues = queues,
        .presentQueueCount = 1,
        .imageCount = desc.imageCount,
        .width = desc.width,
        .height = desc.height,
        .colorFormat = desc.colorFormat,
        .enableVsync = desc.enableVSync,
        .colorSpace = desc.colorSpace,
        .hdrMetadata = desc.hdrMetadata,
    };
    if (desc.enableHDR)
    {
        swapDesc.colorFormat = hz::Format::UNDEFINED;
        // TODO: Prefer scRGB for windowed presentation and HDR10 for fullscreen presentation.
        swapDesc.colorSpace = COLOR_SPACE_P2020;
        if (swapDesc.hdrMetadata.maxMasteringLuminance <= 0.0f)
            swapDesc.hdrMetadata.maxMasteringLuminance = 1000.0f;
        if (swapDesc.hdrMetadata.minMasteringLuminance <= 0.0f)
            swapDesc.hdrMetadata.minMasteringLuminance = 0.001f;
        if (swapDesc.hdrMetadata.maxContentLightLevel <= 0.0f)
            swapDesc.hdrMetadata.maxContentLightLevel = 1000.0f;
        if (swapDesc.hdrMetadata.maxFrameAverageLightLevel <= 0.0f)
            swapDesc.hdrMetadata.maxFrameAverageLightLevel = 400.0f;
        swapDesc.colorFormat = getSupportedSwapchainFormat(pRenderer, &swapDesc, swapDesc.colorSpace);
        if (swapDesc.colorFormat == hz::Format::UNDEFINED)
        {
            swapDesc.colorSpace = COLOR_SPACE_EXTENDED_SRGB;
            swapDesc.colorFormat = getSupportedSwapchainFormat(pRenderer, &swapDesc, swapDesc.colorSpace);
        }
        if (swapDesc.colorFormat == hz::Format::UNDEFINED)
        {
            LOGF(LogLevel::eWARNING, "HDR presentation is unavailable; falling back to SDR sRGB");
            swapDesc.colorSpace = COLOR_SPACE_SDR_SRGB;
            swapDesc.hdrMetadata = {};
        }
    }
    else if (swapDesc.colorFormat == hz::Format::UNDEFINED)
    {
        swapDesc.colorSpace = COLOR_SPACE_SDR_SRGB;
    }

    if (swapDesc.colorFormat == hz::Format::UNDEFINED)
        swapDesc.colorFormat = getSupportedSwapchainFormat(pRenderer, &swapDesc, swapDesc.colorSpace);
    if (swapDesc.colorFormat == hz::Format::UNDEFINED)
    {
        LOGF(LogLevel::eERROR, "No supported swapchain format is available");
        return false;
    }

    addSwapChain(pRenderer, &swapDesc, &pSwapChain);
    if (pSwapChain)
    {
        desc.colorFormat = pSwapChain->format;
        desc.colorSpace = pSwapChain->colorSpace;
        hdrDisplayInfo = pSwapChain->hdrDisplayInfo;
        hdrMetadata = pSwapChain->hdrMetadata;
        if (pSwapChain->colorSpace == COLOR_SPACE_P2020)
            outputMode = OUTPUT_MODE_HDR10;
        else if (pSwapChain->colorSpace == COLOR_SPACE_EXTENDED_SRGB)
            outputMode = OUTPUT_MODE_SCRGB;
        else
            outputMode = OUTPUT_MODE_SDR;
        for (uint32_t i = 0; i < pSwapChain->imageCount; ++i)
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
            backbuffer = GPUTexture{};
        removeSwapChain(pRenderer, pSwapChain);
        pSwapChain = nullptr;
    }
    swapchainImageIndex = 0;
    imageAcquireWaitPending = false;
}

void RenderContext::cleanup()
{
    destroyShaderCompiler();
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
    const bool updated = (pSwapChain->enableVsync != 0) == enabled;
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
            {
                slot.submitId = 0;
                // Reclaim completed submissions even when a different command slot is selected.
                for (uint32_t i = 0; i < (uint32_t)arrlen(slot.transientBuffers); ++i)
                    removeBuffer(pRenderer, slot.transientBuffers[i]);
                arrsetlen(slot.transientBuffers, 0);
            }
        }
        if (!slot.submitId && !available)
            available = &slot;
    }

    if (!available)
    {
        available = &commandSlots[nextCommandSlot];
        waitForFences(pRenderer, 1, &available->pFence);
        available->submitId = 0;
    }

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
            .currentState = pPresent->state,
            .newState = RESOURCE_STATE_PRESENT,
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
        .cmdCount = 1,
        .waitSemaphoreCount = imageAcquireWaitPending ? 1u : 0u,
        .signalSemaphoreCount = pPresent ? 1u : 0u,
    };
    queueSubmit(pGraphicsQueue, &submitDesc);
    imageAcquireWaitPending = false;

    if (pPresent)
    {
        QueuePresentDesc presentDesc = {
            .pSwapChain = pSwapChain,
            .ppWaitSemaphores = &slot.pSemaphore,
            .waitSemaphoreCount = 1,
            .index = (uint8_t)swapchainImageIndex,
            .submitDone = true,
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
    ASSERT(handle.isValid() && handle.slot < maxCommandLists);
    CommandSlot& slot = commandSlots[handle.slot];
    if (slot.submitId == handle.id)
        waitForFences(pRenderer, 1, &slot.pFence);
}

uint32_t              RenderContext::getWidth() const { return desc.width; }
uint32_t              RenderContext::getHeight() const { return desc.height; }
hz::Format            RenderContext::getColorFormat() const { return desc.colorFormat; }
bool                  RenderContext::isHDREnabled() const { return outputMode != OUTPUT_MODE_SDR; }
OutputMode            RenderContext::getOutputMode() const { return outputMode; }
const HDRDisplayInfo& RenderContext::getHDRDisplayInfo() const { return hdrDisplayInfo; }
const HDRMetadata&    RenderContext::getHDRMetadata() const { return hdrMetadata; }
const GPUTexture&     RenderContext::getCurrentBackbuffer()
{
    ASSERT(!suspended && pSwapChain);
    if (!imageAcquired)
    {
        acquireNextImage(pRenderer, pSwapChain, pImageAcquiredSemaphore, nullptr, &swapchainImageIndex);
        ASSERT(swapchainImageIndex < pSwapChain->imageCount);
        imageAcquired = true;
        imageAcquireWaitPending = true;
    }
    return backbuffers[swapchainImageIndex];
}
bool RenderContext::isSuspended() const { return suspended; }

} // namespace hz
