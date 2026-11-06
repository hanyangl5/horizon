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
RenderContext::CommandResources::~CommandResources() { arrfree(transientBuffers); }

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

void RenderContext::waitIdle()
{
    waitQueueIdle(pGraphicsQueue);
    collectDeferredBuffers();
}

void RenderContext::deferRelease(GPUBuffer&& buffer)
{
    if (!buffer.pBuffer)
    {
        return;
    }
    ASSERT(ready && buffer.pContext == this);
    const DeferredBuffer deferred = { .pBuffer = buffer.pBuffer, .useId = getResourceUseId() };
    arrpush(deferredBuffers, deferred);
    buffer.pBuffer = nullptr;
    buffer.pContext = nullptr;
}

uint64_t RenderContext::getResourceUseId() const
{
    for (const CommandResources& resources : commandResources)
    {
        if (resources.commands.pCmd)
        {
            return nextSubmitId;
        }
    }
    return nextSubmitId - 1;
}

uint64_t RenderContext::getPendingUseId() const
{
    // Submissions complete in order on the single graphics queue.
    uint64_t pending = UINT64_MAX;
    for (const CommandResources& resources : commandResources)
    {
        if (resources.commands.pCmd)
        {
            pending = TF_MIN(pending, nextSubmitId);
        }
        if (resources.submitId && resources.submitId < pending)
        {
            FenceStatus status = FENCE_STATUS_COMPLETE;
            getFenceStatus(pRenderer, resources.pFence, &status);
            if (status == FENCE_STATUS_INCOMPLETE)
            {
                pending = resources.submitId;
            }
        }
    }
    return pending;
}

bool RenderContext::isResourceUseComplete(uint64_t useId) const { return useId < getPendingUseId(); }

void RenderContext::collectDeferredBuffers()
{
    const uint64_t pending = getPendingUseId();
    uint32_t       kept = 0;
    for (uint32_t i = 0; i < (uint32_t)arrlen(deferredBuffers); ++i)
    {
        if (deferredBuffers[i].useId < pending)
        {
            ASSERT(resourceCount);
            --resourceCount;
            removeResource(deferredBuffers[i].pBuffer);
        }
        else
        {
            deferredBuffers[kept++] = deferredBuffers[i];
        }
    }
    arrsetlen(deferredBuffers, kept);
}

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
        CommandResources& resources = commandResources[i];
        CmdPoolDesc  poolDesc = {
            .pQueue = pGraphicsQueue,
            .transient = false,
        };
        addCmdPool(pRenderer, &poolDesc, &resources.pCmdPool);
        ASSERT(resources.pCmdPool);

        CmdDesc cmdDesc = { .pPool = resources.pCmdPool };
        addCmd(pRenderer, &cmdDesc, &resources.pCmd);
        addFence(pRenderer, &resources.pFence);
        addSemaphore(pRenderer, &resources.pSemaphore);
        ASSERT(resources.pCmd && resources.pFence && resources.pSemaphore);
        resources.commands.pContext = this;
        resources.commands.commandIndex = i;
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
    for (const CommandResources& resources : commandResources)
    {
        ASSERT(!resources.commands.pCmd);
    }
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
    for (const CommandResources& resources : commandResources)
    {
        ASSERT(!resources.commands.pCmd);
    }
    destroySwapChain();
    destroyDevice(true);
}

void RenderContext::destroyDevice(bool waitForGpu)
{
    ready = false;
    if (waitForGpu && pGraphicsQueue)
        waitQueueIdle(pGraphicsQueue);
    for (uint32_t i = 0; i < (uint32_t)arrlen(deferredBuffers); ++i)
    {
        ASSERT(resourceCount);
        --resourceCount;
        removeResource(deferredBuffers[i].pBuffer);
    }
    arrfree(deferredBuffers);
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
        for (CommandResources& resources : commandResources)
        {
            for (uint32_t i = 0; i < (uint32_t)arrlen(resources.transientBuffers); ++i)
            {
                removeBuffer(pRenderer, resources.transientBuffers[i]);
            }
            arrsetlen(resources.transientBuffers, 0);
            if (resources.pSemaphore)
            {
                removeSemaphore(pRenderer, resources.pSemaphore);
            }
            if (resources.pFence)
            {
                removeFence(pRenderer, resources.pFence);
            }
            if (resources.pCmd)
            {
                removeCmd(pRenderer, resources.pCmd);
            }
            if (resources.pCmdPool)
            {
                removeCmdPool(pRenderer, resources.pCmdPool);
            }
            resources.pSemaphore = nullptr;
            resources.pFence = nullptr;
            resources.pCmd = nullptr;
            resources.pCmdPool = nullptr;
            resources.submitId = 0;
            resources.commands.clear();
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
    if (pRaytracing)
    {
        removeRaytracing(pRenderer, pRaytracing);
        pRaytracing = nullptr;
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
    for (const CommandResources& resources : commandResources)
    {
        ASSERT(!resources.commands.pCmd);
    }
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
    for (const CommandResources& resources : commandResources)
    {
        ASSERT(!resources.commands.pCmd);
    }
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
    PROFILER_SET_CPU_SCOPE_AUTO_NAMED("RenderContext::acquireCommandList");
    ASSERT(ready);
    for (const CommandResources& resources : commandResources)
    {
        ASSERT(!resources.commands.pCmd);
    }

    CommandResources* available = nullptr;
    for (uint32_t offset = 0; offset < maxCommandLists; ++offset)
    {
        CommandResources& resources = commandResources[(nextCommandIndex + offset) % maxCommandLists];
        if (resources.submitId)
        {
            FenceStatus status = FENCE_STATUS_NOTSUBMITTED;
            getFenceStatus(pRenderer, resources.pFence, &status);
            if (status == FENCE_STATUS_COMPLETE)
            {
                resources.submitId = 0;
                // Reclaim completed submissions even when a different command resources is selected.
                for (uint32_t i = 0; i < (uint32_t)arrlen(resources.transientBuffers); ++i)
                {
                    removeBuffer(pRenderer, resources.transientBuffers[i]);
                }
                arrsetlen(resources.transientBuffers, 0);
            }
        }
        if (!resources.submitId && !available)
        {
            available = &resources;
        }
    }

    if (!available)
    {
        available = &commandResources[nextCommandIndex];
        waitForFences(pRenderer, 1, &available->pFence);
        available->submitId = 0;
    }

    for (uint32_t i = 0; i < (uint32_t)arrlen(available->transientBuffers); ++i)
        removeBuffer(pRenderer, available->transientBuffers[i]);
    arrsetlen(available->transientBuffers, 0);
    collectDeferredBuffers();
    resetCmdPool(pRenderer, available->pCmdPool);
    beginCmd(available->pCmd);
    available->commands.reset(available->pCmd, gpuProfilerToken);
    available->commands.beginGpuFrameProfile();
    nextCommandIndex = (available->commands.commandIndex + 1) % maxCommandLists;
    return available->commands;
}

SubmitHandle RenderContext::submit(CommandList& commands, const GPUTexture* pPresent)
{
    PROFILER_SET_CPU_SCOPE_AUTO_NAMED("RenderContext::submit");
    ASSERT(commands.commandIndex < maxCommandLists);
    CommandResources& resources = commandResources[commands.commandIndex];
    ASSERT(&commands == &resources.commands && commands.pCmd == resources.pCmd && !resources.submitId);
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
    endCmd(resources.pCmd);

    Semaphore*      waitSemaphores[] = { pImageAcquiredSemaphore };
    QueueSubmitDesc submitDesc = {
        .ppCmds = &resources.pCmd,
        .pSignalFence = resources.pFence,
        .ppWaitSemaphores = imageAcquireWaitPending ? waitSemaphores : nullptr,
        .ppSignalSemaphores = pPresent ? &resources.pSemaphore : nullptr,
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
            .ppWaitSemaphores = &resources.pSemaphore,
            .waitSemaphoreCount = 1,
            .index = (uint8_t)swapchainImageIndex,
            .submitDone = true,
        };
        {
            PROFILER_SET_CPU_SCOPE_AUTO_NAMED("RenderContext::present");
            queuePresent(pGraphicsQueue, &presentDesc);
        }
        imageAcquired = false;
        flipProfiler();
    }

    const SubmitHandle handle = { .id = nextSubmitId++, .commandIndex = commands.commandIndex };
    resources.submitId = handle.id;
    commands.clear();
    return handle;
}

void RenderContext::wait(SubmitHandle handle)
{
    PROFILER_SET_CPU_SCOPE_AUTO_NAMED("RenderContext::wait");
    ASSERT(handle.isValid() && handle.commandIndex < maxCommandLists);
    CommandResources& resources = commandResources[handle.commandIndex];
    if (resources.submitId == handle.id)
    {
        waitForFences(pRenderer, 1, &resources.pFence);
    }
}

bool RenderContext::isComplete(SubmitHandle handle) const
{
    ASSERT(handle.isValid() && handle.commandIndex < maxCommandLists);
    const CommandResources& resources = commandResources[handle.commandIndex];
    if (resources.submitId != handle.id)
    {
        return true;
    }
    FenceStatus status = FENCE_STATUS_INCOMPLETE;
    getFenceStatus(pRenderer, resources.pFence, &status);
    return status != FENCE_STATUS_INCOMPLETE;
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
        PROFILER_SET_CPU_SCOPE_AUTO_NAMED("RenderContext::acquireNextImage");
        acquireNextImage(pRenderer, pSwapChain, pImageAcquiredSemaphore, nullptr, &swapchainImageIndex);
        ASSERT(swapchainImageIndex < pSwapChain->imageCount);
        imageAcquired = true;
        imageAcquireWaitPending = true;
    }
    return backbuffers[swapchainImageIndex];
}
bool RenderContext::isSuspended() const { return suspended; }

} // namespace hz
