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

    const bool deviceInitialized = InitDevice();
    ASSERT(deviceInitialized);
    if (!deviceInitialized)
    {
        DestroyDevice(false);
        return;
    }

    suspended = !input.Width || !input.Height;
    if (desc.enablePipelineCache)
    {
        InitPipelineCache();
    }
    const bool initialized = suspended || CreateSwapChain();
    ASSERT(initialized);
    if (!initialized)
        Cleanup();
}

RenderContext::~RenderContext() { Cleanup(); }

void RenderContext::WaitIdle()
{
    waitQueueIdle(pGraphicsQueue);
    CollectDeferredResources();
}

void RenderContext::DeferRelease(GPUBuffer&& buffer)
{
    if (!buffer.pBuffer)
    {
        return;
    }
    ASSERT(ready && buffer.pContext == this);
    buffer.Destroy();
}

uint64_t RenderContext::GetResourceUseId() const
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

uint64_t RenderContext::GetPendingUseId() const
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
            FenceStatus status = FenceStatusComplete;
            getFenceStatus(pRenderer, resources.pFence, &status);
            if (status == FenceStatusIncomplete)
            {
                pending = resources.submitId;
            }
        }
    }
    return pending;
}

bool RenderContext::IsResourceUseComplete(uint64_t useId) const { return useId < GetPendingUseId(); }

void RenderContext::DeferResource(ResourceType Type, void* resource, void* secondary, void* tertiary)
{
    if (resource)
    {
        const DeferredResource deferred = { .resource = resource,
                                            .secondary = secondary,
                                            .tertiary = tertiary,
                                            .useId = GetResourceUseId(),
                                            .Type = Type };
        arrpush(deferredResources, deferred);
    }
}

void RenderContext::CollectDeferredResources()
{
    const uint64_t pending = GetPendingUseId();
    uint32_t       kept = 0;
    // Destruction may enqueue dependent resources, which are processed in the same pass.
    for (uint32_t i = 0; i < (uint32_t)arrlen(deferredResources); ++i)
    {
        const DeferredResource resource = deferredResources[i];
        if (resource.useId >= pending)
        {
            deferredResources[kept++] = resource;
            continue;
        }
        ASSERT(resourceCount);
        --resourceCount;
        switch (resource.Type)
        {
        case ResourceType::Buffer:
            removeResource((Buffer*)resource.resource);
            break;
        case ResourceType::Texture:
            removeResource((Texture*)resource.resource);
            break;
        case ResourceType::RenderTarget:
            removeRenderTarget(pRenderer, (RenderTarget*)resource.resource);
            break;
        case ResourceType::Sampler:
            removeSampler(pRenderer, (Sampler*)resource.resource);
            break;
        case ResourceType::Shader:
            removeShader(pRenderer, (Shader*)resource.resource);
            break;
        case ResourceType::Pipeline:
            if (resource.tertiary)
            {
                removeIndirectCommandSignature(pRenderer, (CommandSignature*)resource.tertiary);
            }
            removePipeline(pRenderer, (Pipeline*)resource.resource);
            removeRootSignature(pRenderer, (RootSignature*)resource.secondary);
            break;
        case ResourceType::AccelerationStructure:
            DestroyAccelerationStructure((AccelerationStructureData*)resource.resource);
            break;
        case ResourceType::RayTracingPipeline:
            removeRaytracingPipeline(pRaytracing, (RaytracingPipeline*)resource.resource);
            break;
        }
    }
    arrsetlen(deferredResources, kept);
}

bool RenderContext::InitDevice()
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
        .Type = QueueTypeGraphics,
        .flag = QUEUE_FLAG_NONE,
        .priority = QUEUE_PRIORITY_NORMAL,
        .Name = "RenderContext Graphics Queue",
    };
    addQueue(pRenderer, &queueDesc, &pGraphicsQueue);
    if (!pGraphicsQueue)
        return false;

    IndirectArgumentDescriptor drawArgument = { .Type = INDIRECT_DRAW };
    CommandSignatureDesc       drawSignatureDesc = {
        .pArgDescs = &drawArgument,
        .indirectArgCount = 1,
        .packed = true,
    };
    addIndirectCommandSignature(pRenderer, &drawSignatureDesc, &pDrawIndirectSignature);

    IndirectArgumentDescriptor drawIndexedArgument = { .Type = INDIRECT_DRAW_INDEX };
    CommandSignatureDesc       drawIndexedSignatureDesc = {
        .pArgDescs = &drawIndexedArgument,
        .indirectArgCount = 1,
        .packed = true,
    };
    addIndirectCommandSignature(pRenderer, &drawIndexedSignatureDesc, &pDrawIndexedIndirectSignature);

    IndirectArgumentDescriptor dispatchArgument = { .Type = INDIRECT_DISPATCH };
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

bool RenderContext::CreateSwapChain()
{
    ASSERT(ready && !pSwapChain);
    Queue*        queues[] = { pGraphicsQueue };
    SwapChainDesc swapDesc = {
        .windowHandle = desc.windowHandle,
        .ppPresentQueues = queues,
        .presentQueueCount = 1,
        .imageCount = desc.imageCount,
        .Width = desc.Width,
        .Height = desc.Height,
        .colorFormat = desc.colorFormat,
        .enableVsync = desc.enableVSync,
        .colorSpace = desc.colorSpace,
        .hdrMetadata = desc.hdrMetadata,
    };
    if (desc.enableHDR)
    {
        swapDesc.colorFormat = hz::Format::UNDEFINED;
        // TODO: Prefer scRGB for windowed presentation and HDR10 for fullscreen presentation.
        swapDesc.colorSpace = ColorSpaceP2020;
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
            swapDesc.colorSpace = ColorSpaceExtendedSrgb;
            swapDesc.colorFormat = getSupportedSwapchainFormat(pRenderer, &swapDesc, swapDesc.colorSpace);
        }
        if (swapDesc.colorFormat == hz::Format::UNDEFINED)
        {
            LOGF(LogLevel::Warning, "HDR presentation is unavailable; falling back to SDR sRGB");
            swapDesc.colorSpace = ColorSpaceSdrSrgb;
            swapDesc.hdrMetadata = {};
        }
    }
    else if (swapDesc.colorFormat == hz::Format::UNDEFINED)
    {
        swapDesc.colorSpace = ColorSpaceSdrSrgb;
    }

    if (swapDesc.colorFormat == hz::Format::UNDEFINED)
        swapDesc.colorFormat = getSupportedSwapchainFormat(pRenderer, &swapDesc, swapDesc.colorSpace);
    if (swapDesc.colorFormat == hz::Format::UNDEFINED)
    {
        LOGF(LogLevel::Error, "No supported swapchain format is available");
        return false;
    }

    addSwapChain(pRenderer, &swapDesc, &pSwapChain);
    if (pSwapChain)
    {
        desc.colorFormat = pSwapChain->Format;
        desc.colorSpace = pSwapChain->colorSpace;
        hdrDisplayInfo = pSwapChain->hdrDisplayInfo;
        hdrMetadata = pSwapChain->hdrMetadata;
        if (pSwapChain->colorSpace == ColorSpaceP2020)
        {
            outputMode = OUTPUT_MODE_HDR10;
        }
        else if (pSwapChain->colorSpace == ColorSpaceExtendedSrgb)
        {
            outputMode = OUTPUT_MODE_SCRGB;
        }
        else
            outputMode = OUTPUT_MODE_SDR;
        for (uint32_t i = 0; i < pSwapChain->imageCount; ++i)
        {
            RenderTarget* target = pSwapChain->ppRenderTargets[i];
            backbuffers[i] = GPUTexture(nullptr, target->pTexture, target, ResourceStatePresent, false);
        }
    }
    return pSwapChain != nullptr;
}

void RenderContext::DestroySwapChain()
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

void RenderContext::Cleanup()
{
    SavePipelineCache();
    for (const CommandResources& resources : commandResources)
    {
        ASSERT(!resources.commands.pCmd);
    }
    ReleaseTransientSlots();
    DestroySwapChain();
    DestroyDevice(true);
}

void RenderContext::DestroyDevice(bool waitForGpu)
{
    ready = false;
    if (waitForGpu && pGraphicsQueue)
        waitQueueIdle(pGraphicsQueue);
    CollectDeferredResources();
    arrfree(deferredResources);
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
        if (pPipelineCache)
        {
            removePipelineCache(pRenderer, pPipelineCache);
            pPipelineCache = nullptr;
        }
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
            resources.commands.Clear();
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

bool RenderContext::Resize(uint32_t Width, uint32_t Height)
{
    for (const CommandResources& resources : commandResources)
    {
        ASSERT(!resources.commands.pCmd);
    }
    ASSERT(!imageAcquired);
    if (!suspended && desc.Width == Width && desc.Height == Height)
    {
        return true;
    }

    DestroySwapChain();
    desc.Width = Width;
    desc.Height = Height;
    suspended = !Width || !Height;
    if (suspended)
        return true;

    const bool created = CreateSwapChain();
    ASSERT(created);
    if (!created)
    {
        DestroySwapChain();
        suspended = true;
    }
    return created;
}

bool RenderContext::SetVSync(bool enabled)
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

CommandList& RenderContext::AcquireCommandList()
{
    PROFILER_SET_CPU_SCOPE_AUTO_NAMED("RenderContext::AcquireCommandList");
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
            FenceStatus status = FenceStatusNotSubmitted;
            getFenceStatus(pRenderer, resources.pFence, &status);
            if (status == FenceStatusComplete)
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
    CollectDeferredResources();
    resetCmdPool(pRenderer, available->pCmdPool);
    beginCmd(available->pCmd);
    available->commands.Reset(available->pCmd, gpuProfilerToken);
    available->commands.BeginGpuFrameProfile();
    nextCommandIndex = (available->commands.commandIndex + 1) % maxCommandLists;
    return available->commands;
}

SubmitHandle RenderContext::Submit(CommandList& commands, const GPUTexture* pPresent)
{
    PROFILER_SET_CPU_SCOPE_AUTO_NAMED("RenderContext::Submit");
    ASSERT(commands.commandIndex < maxCommandLists);
    CommandResources& resources = commandResources[commands.commandIndex];
    ASSERT(&commands == &resources.commands && commands.pCmd == resources.pCmd && !resources.submitId);
    if (pPresent)
    {
        ASSERT(imageAcquired && pPresent == &backbuffers[swapchainImageIndex]);
        RenderTargetBarrier presentBarrier = {
            .pRenderTarget = pPresent->pRenderTarget,
            .currentState = pPresent->state,
            .newState = ResourceStatePresent,
        };
        commands.Barrier(0, nullptr, 0, nullptr, 1, &presentBarrier);
        pPresent->state = ResourceStatePresent;
    }
    commands.EndGpuFrameProfile();
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

    const SubmitHandle Handle = { .id = nextSubmitId++, .commandIndex = commands.commandIndex };
    resources.submitId = Handle.id;
    commands.Clear();
    return Handle;
}

void RenderContext::Wait(SubmitHandle Handle)
{
    PROFILER_SET_CPU_SCOPE_AUTO_NAMED("RenderContext::Wait");
    ASSERT(Handle.IsValid() && Handle.commandIndex < maxCommandLists);
    CommandResources& resources = commandResources[Handle.commandIndex];
    if (resources.submitId == Handle.id)
    {
        waitForFences(pRenderer, 1, &resources.pFence);
    }
}

bool RenderContext::IsComplete(SubmitHandle Handle) const
{
    ASSERT(Handle.IsValid() && Handle.commandIndex < maxCommandLists);
    const CommandResources& resources = commandResources[Handle.commandIndex];
    if (resources.submitId != Handle.id)
    {
        return true;
    }
    FenceStatus status = FenceStatusIncomplete;
    getFenceStatus(pRenderer, resources.pFence, &status);
    return status != FenceStatusIncomplete;
}

uint32_t              RenderContext::GetWidth() const { return desc.Width; }
uint32_t              RenderContext::GetHeight() const { return desc.Height; }
hz::Format            RenderContext::GetColorFormat() const { return desc.colorFormat; }
bool                  RenderContext::IsHDREnabled() const { return outputMode != OUTPUT_MODE_SDR; }
OutputMode            RenderContext::GetOutputMode() const { return outputMode; }
const HDRDisplayInfo& RenderContext::GetHDRDisplayInfo() const { return hdrDisplayInfo; }
const HDRMetadata&    RenderContext::GetHDRMetadata() const { return hdrMetadata; }
const GPUTexture&     RenderContext::GetCurrentBackbuffer()
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
bool RenderContext::IsSuspended() const { return suspended; }

} // namespace hz
