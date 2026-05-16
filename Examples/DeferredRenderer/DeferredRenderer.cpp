#include <cmath>
#include <cstdint>
#include <cstring>

#include "DeferredRendererPasses.h"

#include "Application/IApp.h"
#include "Core/ILog.h"
#include "Profiler/IProfiler.h"
#include "RHI/RingBuffer.h"
#include "Runtime/RHI/Private/RendererResourceAPI.h"

constexpr uint32_t kCpuProfileColor = 0x88CC44;

class DeferredRendererApp final: public IApp
{
public:
    DeferredRendererApp()
    {
        mSettings.mWidth = 1280;
        mSettings.mHeight = 720;
        mSettings.mVSyncEnabled = true;
        mSettings.mShowPlatformUI = false;
    }

    bool Init() override
    {
        PROFILER_SET_CPU_SCOPE("DeferredRenderer", "Init", kCpuProfileColor);

        RendererDesc rendererDesc = {};
        rendererDesc.mEnableGpuBasedValidation = true;
        initRenderer(GetName(), &rendererDesc, &pRenderer);
        if (!pRenderer)
        {
            LOGF(eERROR, "Failed to initialize renderer for %s", GetName());
            return false;
        }

        QueueDesc queueDesc = {
            .mType = QUEUE_TYPE_GRAPHICS,
            .mFlag = QUEUE_FLAG_NONE,
            .mPriority = QUEUE_PRIORITY_NORMAL,
            .pName = "DeferredRenderer.GraphicsQueue",
        };
        addQueue(pRenderer, &queueDesc, &pGraphicsQueue);
        if (!pGraphicsQueue)
        {
            LOGF(eERROR, "Failed to create graphics queue");
            return false;
        }

        GpuCmdRingDesc cmdRingDesc = {
            .pQueue = pGraphicsQueue,
            .mPoolCount = kFrameResourceCount,
            .mCmdPerPoolCount = 1,
            .mAddSyncPrimitives = true,
        };
        addGpuCmdRing(pRenderer, &cmdRingDesc, &mGraphicsCmdRing);

        Queue*      profilerQueues[] = { pGraphicsQueue };
        const char* profilerNames[] = { "DeferredRenderer GPU" };
        ProfilerDesc profilerDesc = {
            .pRenderer = pRenderer,
            .ppQueues = profilerQueues,
            .ppProfilerNames = profilerNames,
            .pProfileTokens = &mGpuProfileToken,
            .mGpuProfilerCount = 1,
        };
        initProfiler(&profilerDesc);

        addSemaphore(pRenderer, &pImageAcquiredSemaphore);
        if (!pImageAcquiredSemaphore)
            return false;

        mFrameResourceCount = mGraphicsCmdRing.mPoolCount;
        return createShaderResources() && createUniformBuffers() && createDescriptorSets();
    }

    void Exit() override
    {
        PROFILER_SET_CPU_SCOPE("DeferredRenderer", "Exit", kCpuProfileColor);

        if (pGraphicsQueue)
            waitQueueIdle(pGraphicsQueue);

        mGraph.reset();
        exitProfiler();
        mGpuProfileToken = PROFILE_INVALID_TOKEN;

        removePipelineResource();
        removeDescriptorSets();
        removeBufferResources();
        removeShaderResources();

        if (pImageAcquiredSemaphore)
        {
            removeSemaphore(pRenderer, pImageAcquiredSemaphore);
            pImageAcquiredSemaphore = nullptr;
        }

        removeGpuCmdRing(pRenderer, &mGraphicsCmdRing);

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
    }

    bool Load(ReloadDesc* pReloadDesc) override
    {
        PROFILER_SET_CPU_SCOPE("DeferredRenderer", "Load", kCpuProfileColor);

        if (pReloadDesc->mType & (RELOAD_TYPE_RESIZE | RELOAD_TYPE_RENDERTARGET))
        {
            if (!addSwapChainResource())
                return false;
        }

        return addPipelineResource();
    }

    void Unload(ReloadDesc* pReloadDesc) override
    {
        PROFILER_SET_CPU_SCOPE("DeferredRenderer", "Unload", kCpuProfileColor);

        if (pGraphicsQueue)
            waitQueueIdle(pGraphicsQueue);

        mGraph.reset();
        removePipelineResource();

        if (pReloadDesc->mType & (RELOAD_TYPE_RESIZE | RELOAD_TYPE_RENDERTARGET))
            removeSwapChainResource();
    }

    void Update(float deltaTime) override
    {
        PROFILER_SET_CPU_SCOPE("DeferredRenderer", "Update", kCpuProfileColor);
        mElapsedTime += deltaTime;
    }

    void Draw() override
    {
        PROFILER_SET_CPU_SCOPE("DeferredRenderer", "Draw", kCpuProfileColor);

        if (!pSwapChain || !mGeometryBuildPass.isReady() || !mGBufferPass.isReady() || !mLightingPass.isReady() ||
            !mFrameResourceCount)
            return;

        if (pSwapChain->mEnableVsync != mSettings.mVSyncEnabled)
        {
            waitQueueIdle(pGraphicsQueue);
            toggleVSync(pRenderer, &pSwapChain);
        }

        uint32_t swapchainImageIndex = 0;
        acquireNextImage(pRenderer, pSwapChain, pImageAcquiredSemaphore, nullptr, &swapchainImageIndex);

        GpuCmdRingElement cmdRingElement = getNextGpuCmdRingElement(&mGraphicsCmdRing, true, 1);
        FenceStatus       fenceStatus = FENCE_STATUS_NOTSUBMITTED;
        getFenceStatus(pRenderer, cmdRingElement.pFence, &fenceStatus);
        if (fenceStatus == FENCE_STATUS_INCOMPLETE)
            waitForFences(pRenderer, 1, &cmdRingElement.pFence);

        const uint32_t frameResourceIndex = mGraphicsCmdRing.mPoolIndex;
        if (frameResourceIndex >= mFrameResourceCount)
            return;

        updateSceneUniforms(frameResourceIndex);
        resetCmdPool(pRenderer, cmdRingElement.pCmdPool);

        Cmd*          pCmd = cmdRingElement.pCmds[0];
        RenderTarget* pBackbuffer = pSwapChain->ppRenderTargets[swapchainImageIndex];

        beginCmd(pCmd);
        cmdBeginGpuFrameProfile(pCmd, mGpuProfileToken);

        recordFrameGraph(pCmd, pBackbuffer, frameResourceIndex, cmdRingElement.pFence);

        cmdEndGpuFrameProfile(pCmd, mGpuProfileToken);
        endCmd(pCmd);

        Semaphore* waitSemaphores[] = { pImageAcquiredSemaphore };
        QueueSubmitDesc submitDesc = {
            .ppCmds = &pCmd,
            .pSignalFence = cmdRingElement.pFence,
            .ppWaitSemaphores = waitSemaphores,
            .ppSignalSemaphores = &cmdRingElement.pSemaphore,
            .mCmdCount = 1,
            .mWaitSemaphoreCount = 1,
            .mSignalSemaphoreCount = 1,
        };
        queueSubmit(pGraphicsQueue, &submitDesc);

        QueuePresentDesc presentDesc = {
            .pSwapChain = pSwapChain,
            .ppWaitSemaphores = &cmdRingElement.pSemaphore,
            .mWaitSemaphoreCount = 1,
            .mIndex = static_cast<uint8_t>(swapchainImageIndex),
            .mSubmitDone = true,
        };
        queuePresent(pGraphicsQueue, &presentDesc);

        flipProfiler();
    }

    const char* GetName() override { return "DeferredRenderer"; }

private:
    bool createShaderResources()
    {
        return mGeometryBuildPass.init(pRenderer) && mGBufferPass.init(pRenderer) && mLightingPass.init(pRenderer);
    }

    bool createUniformBuffers()
    {
        if (!mFrameResourceCount)
            return false;

        for (uint32_t frame = 0; frame < mFrameResourceCount; ++frame)
        {
            for (uint32_t i = 0; i < kSceneObjectCount; ++i)
            {
                BufferDesc uniformDesc = {
                    .mSize = kConstantBufferSize,
                    .pName = i == 0 ? "DeferredRenderer.CubeUniforms" : "DeferredRenderer.FloorUniforms",
                    .mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_UPLOAD,
                    .mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT,
                    .mStartState = RESOURCE_STATE_GENERIC_READ,
                    .mDescriptors = DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                };
                addBuffer(pRenderer, &uniformDesc, &pSceneUniformBuffers[frame][i]);
                if (!pSceneUniformBuffers[frame][i] || !pSceneUniformBuffers[frame][i]->pCpuMappedAddress)
                {
                    removeBufferResources();
                    return false;
                }
            }

            updateSceneUniforms(frame);
        }
        return true;
    }

    bool createDescriptorSets()
    {
        if (!mFrameResourceCount)
            return false;

        return mGeometryBuildPass.createDescriptorSet(pRenderer, mFrameResourceCount) &&
               mGBufferPass.createDescriptorSet(pRenderer, pSceneUniformBuffers, mFrameResourceCount) &&
               mLightingPass.createDescriptorSet(pRenderer, mFrameResourceCount);
    }

    bool addPipelineResource()
    {
        if (!pSwapChain)
            return false;

        removePipelineResource();
        if (!mGeometryBuildPass.addPipeline(pRenderer))
            return false;
        if (!mGBufferPass.addPipeline(pRenderer))
            return false;
        return mLightingPass.addPipeline(pRenderer, pSwapChain->mFormat);
    }

    bool addSwapChainResource()
    {
        removeSwapChainResource();

        SwapChainDesc swapChainDesc = {
            .mWindowHandle = pWindow->handle,
            .ppPresentQueues = &pGraphicsQueue,
            .mPresentQueueCount = 1,
            .mImageCount = getRecommendedSwapchainImageCount(pRenderer, &pWindow->handle),
            .mWidth = static_cast<uint32_t>(mSettings.mWidth),
            .mHeight = static_cast<uint32_t>(mSettings.mHeight),
            .mEnableVsync = mSettings.mVSyncEnabled,
            .mColorSpace = COLOR_SPACE_SDR_SRGB,
        };
        swapChainDesc.mColorFormat = getSupportedSwapchainFormat(pRenderer, &swapChainDesc, swapChainDesc.mColorSpace);
        addSwapChain(pRenderer, &swapChainDesc, &pSwapChain);
        return pSwapChain != nullptr;
    }

    void recordFrameGraph(Cmd* pCmd, RenderTarget* pBackbuffer, uint32_t frameResourceIndex, Fence* pFrameFence)
    {
        const uint32_t width = static_cast<uint32_t>(mSettings.mWidth);
        const uint32_t height = static_cast<uint32_t>(mSettings.mHeight);

        mGraph.beginFrame(pRenderer, width, height, frameResourceIndex, pFrameFence);
        mFrameData = {};
        mFrameData.backbuffer =
            mGraph.importRenderTarget("Backbuffer", pBackbuffer, RESOURCE_STATE_PRESENT, RESOURCE_STATE_PRESENT);

        mGeometryBuildPass.createFrameResources(mGraph, mFrameData, pRenderer);
        mGBufferPass.createFrameResources(mGraph, mFrameData, pRenderer, width, height);

        mGeometryBuildPass.record(mGraph, mFrameData, mGpuProfileToken);
        mGBufferPass.record(mGraph, mFrameData, pSceneUniformBuffers[mGraph.getFrameResourceIndex()], mGpuProfileToken);
        mLightingPass.record(mGraph, mFrameData, mGpuProfileToken);

        mGraph.execute(pCmd);
        mGraph.endFrame(pCmd);
    }

    void updateSceneUniforms(uint32_t frameResourceIndex)
    {
        if (frameResourceIndex >= mFrameResourceCount || !pSceneUniformBuffers[frameResourceIndex][0] ||
            !pSceneUniformBuffers[frameResourceIndex][1])
            return;

        const float aspectInverse = static_cast<float>(mSettings.mHeight) / static_cast<float>(mSettings.mWidth);
        const Matrix4 view = Matrix4::lookAtLH(Point3(3.5f, 3.0f, -6.0f), Point3(0.0f, 0.7f, 0.0f), Vector3(0.0f, 1.0f, 0.0f));
        const float verticalFov = 60.0f * 3.1415926535f / 180.0f;
        const float horizontalFov = 2.0f * std::atan(std::tan(verticalFov * 0.5f) / aspectInverse);
        const Matrix4 projection = Matrix4::perspectiveLH(horizontalFov, aspectInverse, 0.1f, 100.0f);
        const Matrix4 viewProjection = projection * view;

        SceneUniforms cube = {};
        cube.world =
            Matrix4::translation(Vector3(0.0f, 1.15f, 0.0f)) * Matrix4::rotationY(mElapsedTime) * Matrix4::scale(Vector3(0.85f, 0.85f, 0.85f));
        cube.worldViewProjection = viewProjection * cube.world;
        memcpy(pSceneUniformBuffers[frameResourceIndex][0]->pCpuMappedAddress, &cube, sizeof(cube));

        SceneUniforms floor = {};
        floor.world = Matrix4::identity();
        floor.worldViewProjection = viewProjection * floor.world;
        memcpy(pSceneUniformBuffers[frameResourceIndex][1]->pCpuMappedAddress, &floor, sizeof(floor));
    }

    void removePipelineResource()
    {
        mLightingPass.removePipeline(pRenderer);
        mGBufferPass.removePipeline(pRenderer);
        mGeometryBuildPass.removePipeline(pRenderer);
    }

    void removeSwapChainResource()
    {
        if (pSwapChain)
        {
            removeSwapChain(pRenderer, pSwapChain);
            pSwapChain = nullptr;
        }
    }

    void removeDescriptorSets()
    {
        mLightingPass.removeDescriptorSet(pRenderer);
        mGBufferPass.removeDescriptorSet(pRenderer);
        mGeometryBuildPass.removeDescriptorSet(pRenderer);
    }

    void removeBufferResources()
    {
        for (uint32_t frame = 0; frame < kFrameResourceCount; ++frame)
        {
            for (uint32_t object = 0; object < kSceneObjectCount; ++object)
            {
                if (pSceneUniformBuffers[frame][object])
                {
                    removeBuffer(pRenderer, pSceneUniformBuffers[frame][object]);
                    pSceneUniformBuffers[frame][object] = nullptr;
                }
            }
        }
    }

    void removeShaderResources()
    {
        mLightingPass.exit(pRenderer);
        mGBufferPass.exit(pRenderer);
        mGeometryBuildPass.exit(pRenderer);
    }

private:
    Renderer*      pRenderer = nullptr;
    Queue*         pGraphicsQueue = nullptr;
    SwapChain*     pSwapChain = nullptr;
    Semaphore*     pImageAcquiredSemaphore = nullptr;
    Buffer*        pSceneUniformBuffers[kFrameResourceCount][kSceneObjectCount] = {};
    GpuCmdRing     mGraphicsCmdRing = {};
    ProfileToken   mGpuProfileToken = PROFILE_INVALID_TOKEN;
    RenderGraph     mGraph;
    RGFrameData    mFrameData = {};
    GeometryBuildPass mGeometryBuildPass;
    GBufferPass       mGBufferPass;
    LightingPass      mLightingPass;
    uint32_t       mFrameResourceCount = 0;
    float          mElapsedTime = 0.0f;
};

DEFINE_APPLICATION_MAIN(DeferredRendererApp)
