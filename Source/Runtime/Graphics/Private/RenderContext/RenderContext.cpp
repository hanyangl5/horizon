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

RenderContext::RenderContext(const ContextDesc& Input)
{
    const bool Valid = Input.pAppName && Input.pAppName[0] && strlen(Input.pAppName) < sizeof(appName) &&
                       (!Input.imageCount || Input.imageCount <= MAX_SWAPCHAIN_IMAGES);
    ASSERT(Valid);
    if (!Valid)
    {
        return;
    }

    desc = Input;
    desc.imageCount = Input.imageCount ? Input.imageCount : 3;
    memcpy(appName, Input.pAppName, strlen(Input.pAppName) + 1);

    const bool DeviceInitialized = InitDevice();
    ASSERT(DeviceInitialized);
    if (!DeviceInitialized)
    {
        DestroyDevice(false);
        return;
    }

    suspended = !Input.Width || !Input.Height;
    if (desc.enablePipelineCache)
    {
        InitPipelineCache();
    }
    const bool Initialized = suspended || CreateSwapChain();
    ASSERT(Initialized);
    if (!Initialized)
    {
        Cleanup();
    }
}

RenderContext::~RenderContext() { Cleanup(); }

void RenderContext::WaitIdle()
{
    waitQueueIdle(pGraphicsQueue);
    CollectDeferredResources();
}

void RenderContext::DeferRelease(GPUBuffer&& BufferValue)
{
    if (!BufferValue.pBuffer)
    {
        return;
    }
    ASSERT(ready && BufferValue.pContext == this);
    BufferValue.Destroy();
}

uint64_t RenderContext::GetResourceUseId() const
{
    for (const CommandResources& Resources : commandResources)
    {
        if (Resources.commands.pCmd)
        {
            return nextSubmitId;
        }
    }
    return nextSubmitId - 1;
}

uint64_t RenderContext::GetPendingUseId() const
{
    // Submissions complete in order on the single graphics queue.
    uint64_t Pending = UINT64_MAX;
    for (const CommandResources& Resources : commandResources)
    {
        if (Resources.commands.pCmd)
        {
            Pending = TF_MIN(Pending, nextSubmitId);
        }
        if (Resources.submitId && Resources.submitId < Pending)
        {
            FenceStatus Status = FENCE_STATUS_COMPLETE;
            getFenceStatus(pRenderer, Resources.pFence, &Status);
            if (Status == FENCE_STATUS_INCOMPLETE)
            {
                Pending = Resources.submitId;
            }
        }
    }
    return Pending;
}

bool RenderContext::IsResourceUseComplete(uint64_t UseId) const { return UseId < GetPendingUseId(); }

void RenderContext::DeferResource(ResourceType Type, void* Resource, void* Secondary, void* Tertiary)
{
    if (Resource)
    {
        const DeferredResource Deferred = { .resource = Resource,
                                            .secondary = Secondary,
                                            .tertiary = Tertiary,
                                            .useId = GetResourceUseId(),
                                            .Type = Type };
        arrpush(deferredResources, Deferred);
    }
}

void RenderContext::CollectDeferredResources()
{
    const uint64_t Pending = GetPendingUseId();
    uint32_t       Kept = 0;
    // Destruction may enqueue dependent resources, which are processed in the same pass.
    for (uint32_t I = 0; I < (uint32_t)arrlen(deferredResources); ++I)
    {
        const DeferredResource Resource = deferredResources[I];
        if (Resource.useId >= Pending)
        {
            deferredResources[Kept++] = Resource;
            continue;
        }
        ASSERT(resourceCount);
        --resourceCount;
        switch (Resource.Type)
        {
        case ResourceType::Buffer:
            removeResource((Buffer*)Resource.resource);
            break;
        case ResourceType::Texture:
            removeResource((Texture*)Resource.resource);
            break;
        case ResourceType::RenderTarget:
            removeRenderTarget(pRenderer, (RenderTarget*)Resource.resource);
            break;
        case ResourceType::Sampler:
            removeSampler(pRenderer, (Sampler*)Resource.resource);
            break;
        case ResourceType::Shader:
            removeShader(pRenderer, (Shader*)Resource.resource);
            break;
        case ResourceType::Pipeline:
            if (Resource.tertiary)
            {
                removeIndirectCommandSignature(pRenderer, (CommandSignature*)Resource.tertiary);
            }
            removePipeline(pRenderer, (Pipeline*)Resource.resource);
            removeRootSignature(pRenderer, (RootSignature*)Resource.secondary);
            break;
        case ResourceType::AccelerationStructure:
            DestroyAccelerationStructure((AccelerationStructureData*)Resource.resource);
            break;
        case ResourceType::RayTracingPipeline:
            removeRaytracingPipeline(pRaytracing, (RaytracingPipeline*)Resource.resource);
            break;
        }
    }
    arrsetlen(deferredResources, Kept);
}

bool RenderContext::InitDevice()
{
    const RendererContextDesc ContextDescValue = {
        .enableGpuBasedValidation = desc.enableGpuValidation,
    };
    initRendererContext(appName, &ContextDescValue, &pRendererContext);
    if (!pRendererContext)
        return false;

    const RendererDesc RendererDescValue = {
        .shaderTarget = SHADER_TARGET_6_6,
        .pContext = pRendererContext,
        .enableGpuBasedValidation = desc.enableGpuValidation,
    };
    initRenderer(appName, &RendererDescValue, &pRenderer);
    if (!pRenderer)
        return false;

    QueueDesc QueueDescValue = {
        .Type = QUEUE_TYPE_GRAPHICS,
        .flag = QUEUE_FLAG_NONE,
        .priority = QUEUE_PRIORITY_NORMAL,
        .Name = "RenderContext Graphics Queue",
    };
    addQueue(pRenderer, &QueueDescValue, &pGraphicsQueue);
    if (!pGraphicsQueue)
        return false;

    IndirectArgumentDescriptor DrawArgument = { .Type = INDIRECT_DRAW };
    CommandSignatureDesc       DrawSignatureDesc = {
              .pArgDescs = &DrawArgument,
              .indirectArgCount = 1,
              .packed = true,
    };
    addIndirectCommandSignature(pRenderer, &DrawSignatureDesc, &pDrawIndirectSignature);

    IndirectArgumentDescriptor DrawIndexedArgument = { .Type = INDIRECT_DRAW_INDEX };
    CommandSignatureDesc       DrawIndexedSignatureDesc = {
              .pArgDescs = &DrawIndexedArgument,
              .indirectArgCount = 1,
              .packed = true,
    };
    addIndirectCommandSignature(pRenderer, &DrawIndexedSignatureDesc, &pDrawIndexedIndirectSignature);

    IndirectArgumentDescriptor DispatchArgument = { .Type = INDIRECT_DISPATCH };
    CommandSignatureDesc       DispatchSignatureDesc = {
              .pArgDescs = &DispatchArgument,
              .indirectArgCount = 1,
              .packed = true,
    };
    addIndirectCommandSignature(pRenderer, &DispatchSignatureDesc, &pDispatchIndirectSignature);
    ASSERT(pDrawIndirectSignature && pDrawIndexedIndirectSignature && pDispatchIndirectSignature);

    gpuProfilerToken = PROFILE_INVALID_TOKEN;
#if defined(ENABLE_TRACY_MEMORY)
    const bool initializeProfiler = true;
#else
    const bool InitializeProfiler = desc.enableGpuProfiler;
#endif
    if (InitializeProfiler)
    {
        Queue*       Queues[] = { pGraphicsQueue };
        const char*  Names[] = { appName };
        ProfilerDesc ProfilerDescValue = {
            .pRenderer = pRenderer,
            .ppQueues = Queues,
            .ppProfilerNames = Names,
            .pProfileTokens = &gpuProfilerToken,
            .gpuProfilerCount = desc.enableGpuProfiler ? 1u : 0u,
        };
        initProfiler(&ProfilerDescValue);
        profilerInitialized = true;
    }

    ResourceLoaderDesc LoaderDesc = gDefaultResourceLoaderDesc;
    LoaderDesc.bufferSize = 16u * 1024u * 1024u;
    initResourceLoaderInterface(pRenderer, &LoaderDesc);
    resourceLoaderInitialized = true;

    addSemaphore(pRenderer, &pImageAcquiredSemaphore);
    ASSERT(pImageAcquiredSemaphore);
    for (uint32_t I = 0; I < maxCommandLists; ++I)
    {
        CommandResources& Resources = commandResources[I];
        CmdPoolDesc       PoolDesc = {
                  .pQueue = pGraphicsQueue,
                  .transient = false,
        };
        addCmdPool(pRenderer, &PoolDesc, &Resources.pCmdPool);
        ASSERT(Resources.pCmdPool);

        CmdDesc CmdDescValue = { .pPool = Resources.pCmdPool };
        addCmd(pRenderer, &CmdDescValue, &Resources.pCmd);
        addFence(pRenderer, &Resources.pFence);
        addSemaphore(pRenderer, &Resources.pSemaphore);
        ASSERT(Resources.pCmd && Resources.pFence && Resources.pSemaphore);
        Resources.commands.pContext = this;
        Resources.commands.commandIndex = I;
    }

    ready = true;
    return true;
}

bool RenderContext::CreateSwapChain()
{
    ASSERT(ready && !pSwapChain);
    Queue*        Queues[] = { pGraphicsQueue };
    SwapChainDesc SwapDesc = {
        .windowHandle = desc.windowHandle,
        .ppPresentQueues = Queues,
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
        SwapDesc.colorFormat = hz::Format::UNDEFINED;
        // TODO: Prefer scRGB for windowed presentation and HDR10 for fullscreen presentation.
        SwapDesc.colorSpace = COLOR_SPACE_P2020;
        if (SwapDesc.hdrMetadata.maxMasteringLuminance <= 0.0f)
        {
            SwapDesc.hdrMetadata.maxMasteringLuminance = 1000.0f;
        }
        if (SwapDesc.hdrMetadata.minMasteringLuminance <= 0.0f)
        {
            SwapDesc.hdrMetadata.minMasteringLuminance = 0.001f;
        }
        if (SwapDesc.hdrMetadata.maxContentLightLevel <= 0.0f)
        {
            SwapDesc.hdrMetadata.maxContentLightLevel = 1000.0f;
        }
        if (SwapDesc.hdrMetadata.maxFrameAverageLightLevel <= 0.0f)
        {
            SwapDesc.hdrMetadata.maxFrameAverageLightLevel = 400.0f;
        }
        SwapDesc.colorFormat = getSupportedSwapchainFormat(pRenderer, &SwapDesc, SwapDesc.colorSpace);
        if (SwapDesc.colorFormat == hz::Format::UNDEFINED)
        {
            SwapDesc.colorSpace = COLOR_SPACE_EXTENDED_SRGB;
            SwapDesc.colorFormat = getSupportedSwapchainFormat(pRenderer, &SwapDesc, SwapDesc.colorSpace);
        }
        if (SwapDesc.colorFormat == hz::Format::UNDEFINED)
        {
            LOGF(LogLevel::Warning, "HDR presentation is unavailable; falling back to SDR sRGB");
            SwapDesc.colorSpace = COLOR_SPACE_SDR_SRGB;
            SwapDesc.hdrMetadata = {};
        }
    }
    else if (SwapDesc.colorFormat == hz::Format::UNDEFINED)
    {
        SwapDesc.colorSpace = COLOR_SPACE_SDR_SRGB;
    }

    if (SwapDesc.colorFormat == hz::Format::UNDEFINED)
    {
        SwapDesc.colorFormat = getSupportedSwapchainFormat(pRenderer, &SwapDesc, SwapDesc.colorSpace);
    }
    if (SwapDesc.colorFormat == hz::Format::UNDEFINED)
    {
        LOGF(LogLevel::Error, "No supported swapchain format is available");
        return false;
    }

    addSwapChain(pRenderer, &SwapDesc, &pSwapChain);
    if (pSwapChain)
    {
        desc.colorFormat = pSwapChain->Format;
        desc.colorSpace = pSwapChain->colorSpace;
        hdrDisplayInfo = pSwapChain->hdrDisplayInfo;
        hdrMetadata = pSwapChain->hdrMetadata;
        if (pSwapChain->colorSpace == COLOR_SPACE_P2020)
        {
            outputMode = OUTPUT_MODE_HDR10;
        }
        else if (pSwapChain->colorSpace == COLOR_SPACE_EXTENDED_SRGB)
        {
            outputMode = OUTPUT_MODE_SCRGB;
        }
        else
            outputMode = OUTPUT_MODE_SDR;
        for (uint32_t I = 0; I < pSwapChain->imageCount; ++I)
        {
            RenderTarget* Target = pSwapChain->ppRenderTargets[I];
            backbuffers[I] = GPUTexture(nullptr, Target->pTexture, Target, RESOURCE_STATE_PRESENT, false);
        }
    }
    return pSwapChain != nullptr;
}

void RenderContext::DestroySwapChain()
{
    for (const CommandResources& Resources : commandResources)
    {
        ASSERT(!Resources.commands.pCmd);
    }
    ASSERT(!imageAcquired);
    if (pSwapChain)
    {
        waitQueueIdle(pGraphicsQueue);
        for (GPUTexture& Backbuffer : backbuffers)
        {
            Backbuffer = GPUTexture{};
        }
        removeSwapChain(pRenderer, pSwapChain);
        pSwapChain = nullptr;
    }
    swapchainImageIndex = 0;
    imageAcquireWaitPending = false;
}

void RenderContext::Cleanup()
{
    SavePipelineCache();
    for (const CommandResources& Resources : commandResources)
    {
        ASSERT(!Resources.commands.pCmd);
    }
    ReleaseTransientSlots();
    DestroySwapChain();
    DestroyDevice(true);
}

void RenderContext::DestroyDevice(bool WaitForGpu)
{
    ready = false;
    if (WaitForGpu && pGraphicsQueue)
    {
        waitQueueIdle(pGraphicsQueue);
    }
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
        for (CommandResources& Resources : commandResources)
        {
            for (uint32_t I = 0; I < (uint32_t)arrlen(Resources.transientBuffers); ++I)
            {
                removeBuffer(pRenderer, Resources.transientBuffers[I]);
            }
            arrsetlen(Resources.transientBuffers, 0);
            if (Resources.pSemaphore)
            {
                removeSemaphore(pRenderer, Resources.pSemaphore);
            }
            if (Resources.pFence)
            {
                removeFence(pRenderer, Resources.pFence);
            }
            if (Resources.pCmd)
            {
                removeCmd(pRenderer, Resources.pCmd);
            }
            if (Resources.pCmdPool)
            {
                removeCmdPool(pRenderer, Resources.pCmdPool);
            }
            Resources.pSemaphore = nullptr;
            Resources.pFence = nullptr;
            Resources.pCmd = nullptr;
            Resources.pCmdPool = nullptr;
            Resources.submitId = 0;
            Resources.commands.Clear();
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
    for (const CommandResources& Resources : commandResources)
    {
        ASSERT(!Resources.commands.pCmd);
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

    const bool Created = CreateSwapChain();
    ASSERT(Created);
    if (!Created)
    {
        DestroySwapChain();
        suspended = true;
    }
    return Created;
}

bool RenderContext::SetVSync(bool Enabled)
{
    for (const CommandResources& Resources : commandResources)
    {
        ASSERT(!Resources.commands.pCmd);
    }
    if (desc.enableVSync == Enabled)
    {
        return true;
    }
    desc.enableVSync = Enabled;
    if (suspended)
        return true;

    ASSERT(pSwapChain);
    toggleVSync(pRenderer, &pSwapChain);
    const bool Updated = (pSwapChain->enableVsync != 0) == Enabled;
    ASSERT(Updated);
    return Updated;
}

CommandList& RenderContext::AcquireCommandList()
{
    PROFILER_SET_CPU_SCOPE_AUTO_NAMED("RenderContext::AcquireCommandList");
    ASSERT(ready);
    for (const CommandResources& Resources : commandResources)
    {
        ASSERT(!Resources.commands.pCmd);
    }

    CommandResources* Available = nullptr;
    for (uint32_t Offset = 0; Offset < maxCommandLists; ++Offset)
    {
        CommandResources& Resources = commandResources[(nextCommandIndex + Offset) % maxCommandLists];
        if (Resources.submitId)
        {
            FenceStatus Status = FENCE_STATUS_NOTSUBMITTED;
            getFenceStatus(pRenderer, Resources.pFence, &Status);
            if (Status == FENCE_STATUS_COMPLETE)
            {
                Resources.submitId = 0;
                // Reclaim completed submissions even when a different command resources is selected.
                for (uint32_t I = 0; I < (uint32_t)arrlen(Resources.transientBuffers); ++I)
                {
                    removeBuffer(pRenderer, Resources.transientBuffers[I]);
                }
                arrsetlen(Resources.transientBuffers, 0);
            }
        }
        if (!Resources.submitId && !Available)
        {
            Available = &Resources;
        }
    }

    if (!Available)
    {
        Available = &commandResources[nextCommandIndex];
        waitForFences(pRenderer, 1, &Available->pFence);
        Available->submitId = 0;
    }

    for (uint32_t I = 0; I < (uint32_t)arrlen(Available->transientBuffers); ++I)
    {
        removeBuffer(pRenderer, Available->transientBuffers[I]);
    }
    arrsetlen(Available->transientBuffers, 0);
    CollectDeferredResources();
    resetCmdPool(pRenderer, Available->pCmdPool);
    beginCmd(Available->pCmd);
    Available->commands.Reset(Available->pCmd, gpuProfilerToken);
    Available->commands.BeginGpuFrameProfile();
    nextCommandIndex = (Available->commands.commandIndex + 1) % maxCommandLists;
    return Available->commands;
}

SubmitHandle RenderContext::Submit(CommandList& Commands, const GPUTexture* Present)
{
    PROFILER_SET_CPU_SCOPE_AUTO_NAMED("RenderContext::Submit");
    ASSERT(Commands.commandIndex < maxCommandLists);
    CommandResources& Resources = commandResources[Commands.commandIndex];
    ASSERT(&Commands == &Resources.commands && Commands.pCmd == Resources.pCmd && !Resources.submitId);
    if (Present)
    {
        ASSERT(imageAcquired && Present == &backbuffers[swapchainImageIndex]);
        RenderTargetBarrier PresentBarrier = {
            .pRenderTarget = Present->pRenderTarget,
            .currentState = Present->state,
            .newState = RESOURCE_STATE_PRESENT,
        };
        Commands.Barrier(0, nullptr, 0, nullptr, 1, &PresentBarrier);
        Present->state = RESOURCE_STATE_PRESENT;
    }
    Commands.EndGpuFrameProfile();
    endCmd(Resources.pCmd);

    Semaphore*      WaitSemaphores[] = { pImageAcquiredSemaphore };
    QueueSubmitDesc SubmitDesc = {
        .ppCmds = &Resources.pCmd,
        .pSignalFence = Resources.pFence,
        .ppWaitSemaphores = imageAcquireWaitPending ? WaitSemaphores : nullptr,
        .ppSignalSemaphores = Present ? &Resources.pSemaphore : nullptr,
        .cmdCount = 1,
        .waitSemaphoreCount = imageAcquireWaitPending ? 1u : 0u,
        .signalSemaphoreCount = Present ? 1u : 0u,
    };
    queueSubmit(pGraphicsQueue, &SubmitDesc);
    imageAcquireWaitPending = false;

    if (Present)
    {
        QueuePresentDesc PresentDesc = {
            .pSwapChain = pSwapChain,
            .ppWaitSemaphores = &Resources.pSemaphore,
            .waitSemaphoreCount = 1,
            .index = (uint8_t)swapchainImageIndex,
            .submitDone = true,
        };
        {
            PROFILER_SET_CPU_SCOPE_AUTO_NAMED("RenderContext::present");
            queuePresent(pGraphicsQueue, &PresentDesc);
        }
        imageAcquired = false;
        flipProfiler();
    }

    const SubmitHandle Handle = { .id = nextSubmitId++, .commandIndex = Commands.commandIndex };
    Resources.submitId = Handle.id;
    Commands.Clear();
    return Handle;
}

void RenderContext::Wait(SubmitHandle Handle)
{
    PROFILER_SET_CPU_SCOPE_AUTO_NAMED("RenderContext::Wait");
    ASSERT(Handle.IsValid() && Handle.commandIndex < maxCommandLists);
    CommandResources& Resources = commandResources[Handle.commandIndex];
    if (Resources.submitId == Handle.id)
    {
        waitForFences(pRenderer, 1, &Resources.pFence);
    }
}

bool RenderContext::IsComplete(SubmitHandle Handle) const
{
    ASSERT(Handle.IsValid() && Handle.commandIndex < maxCommandLists);
    const CommandResources& Resources = commandResources[Handle.commandIndex];
    if (Resources.submitId != Handle.id)
    {
        return true;
    }
    FenceStatus Status = FENCE_STATUS_INCOMPLETE;
    getFenceStatus(pRenderer, Resources.pFence, &Status);
    return Status != FENCE_STATUS_INCOMPLETE;
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
