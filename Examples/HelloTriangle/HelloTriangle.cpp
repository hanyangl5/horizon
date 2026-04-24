#include <cstddef>
#include <cstdint>
#include <cstring>

#include "Application/IApp.h"
#include "Core/ILog.h"
#include "Profiler/IProfiler.h"
#include "RHI/IGraphics.h"
#include "RHI/RingBuffer.h"
#include "Runtime/RHI/Private/RendererResourceAPI.h"

namespace
{
constexpr uint32_t kCpuProfileColor = 0x3399FF;

struct Vertex
{
    float position[2];
    float color[3];
};

constexpr Vertex kTriangleVertices[] = {
    { { 0.0f, 0.65f }, { 1.0f, 0.2f, 0.2f } },
    { { 0.65f, -0.55f }, { 0.2f, 1.0f, 0.2f } },
    { { -0.65f, -0.55f }, { 0.2f, 0.4f, 1.0f } },
};

constexpr char kHelloTriangleShader[] = R"(
struct VSInput
{
    float2 Position : POSITION;
    float3 Color : COLOR;
};

struct VSOutput
{
    float4 Position : SV_Position;
    float3 Color : COLOR;
};

VSOutput VSMain(VSInput input)
{
    VSOutput output;
    output.Position = float4(input.Position, 0.0f, 1.0f);
    output.Color = input.Color;
    return output;
}

float4 PSMain(VSOutput input) : SV_Target0
{
    return float4(input.Color, 1.0f);
}
)";

ShaderSrcDesc makeShaderSourceDesc()
{
    ShaderSrcDesc desc = {};
    desc.mStages = SHADER_STAGE_VERT | SHADER_STAGE_FRAG;

    desc.mVert.pName = "HelloTriangleVS";
    desc.mVert.pByteCode = const_cast<char*>(kHelloTriangleShader);
    desc.mVert.mByteCodeSize = static_cast<uint32_t>(sizeof(kHelloTriangleShader) - 1);
    desc.mVert.pEntryPoint = "VSMain";

    desc.mFrag.pName = "HelloTrianglePS";
    desc.mFrag.pByteCode = const_cast<char*>(kHelloTriangleShader);
    desc.mFrag.mByteCodeSize = static_cast<uint32_t>(sizeof(kHelloTriangleShader) - 1);
    desc.mFrag.pEntryPoint = "PSMain";

    return desc;
}
} // namespace

class HelloTriangleApp final: public IApp
{
public:
    HelloTriangleApp()
    {
        mSettings.mWidth = 1280;
        mSettings.mHeight = 720;
        mSettings.mVSyncEnabled = true;
        mSettings.mShowPlatformUI = false;
    }

    bool Init() override
    {
        PROFILER_SET_CPU_SCOPE("HelloTriangle", "Init", kCpuProfileColor);

        RendererDesc rendererDesc = {};
        initRenderer(GetName(), &rendererDesc, &pRenderer);
        if (!pRenderer)
        {
            LOGF(eERROR, "Failed to initialize renderer for %s", GetName());
            return false;
        }

        QueueDesc queueDesc = {};
        queueDesc.mType = QUEUE_TYPE_GRAPHICS;
        queueDesc.mFlag = QUEUE_FLAG_NONE;
        queueDesc.mPriority = QUEUE_PRIORITY_NORMAL;
        queueDesc.mNodeIndex = pRenderer->mUnlinkedRendererIndex;
        queueDesc.pName = "HelloTriangle.GraphicsQueue";
        addQueue(pRenderer, &queueDesc, &pGraphicsQueue);
        if (!pGraphicsQueue)
        {
            LOGF(eERROR, "Failed to create graphics queue");
            return false;
        }

        GpuCmdRingDesc cmdRingDesc = {};
        cmdRingDesc.pQueue = pGraphicsQueue;
        cmdRingDesc.mPoolCount = getRecommendedSwapchainImageCount(pRenderer, &pWindow->handle);
        cmdRingDesc.mCmdPerPoolCount = 1;
        cmdRingDesc.mAddSyncPrimitives = true;
        addGpuCmdRing(pRenderer, &cmdRingDesc, &mGraphicsCmdRing);

        Queue*         profilerQueues[] = { pGraphicsQueue };
        const char*    profilerNames[] = { "HelloTriangle GPU" };
        ProfilerDesc   profilerDesc = {};
        profilerDesc.pRenderer = pRenderer;
        profilerDesc.ppQueues = profilerQueues;
        profilerDesc.ppProfilerNames = profilerNames;
        profilerDesc.pProfileTokens = &mGpuProfileToken;
        profilerDesc.mGpuProfilerCount = 1;
        initProfiler(&profilerDesc);

        addSemaphore(pRenderer, &pImageAcquiredSemaphore);
        if (!pImageAcquiredSemaphore)
        {
            LOGF(eERROR, "Failed to create image-acquired semaphore");
            return false;
        }

        if (!createShaderResources())
        {
            return false;
        }

        return createVertexBuffer();
    }

    void Exit() override
    {
        PROFILER_SET_CPU_SCOPE("HelloTriangle", "Exit", kCpuProfileColor);

        if (pGraphicsQueue)
        {
            waitQueueIdle(pGraphicsQueue);
        }

        exitProfiler();
        mGpuProfileToken = PROFILE_INVALID_TOKEN;

        removeBufferResource();
        removePipelineResource();
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
        PROFILER_SET_CPU_SCOPE("HelloTriangle", "Load", kCpuProfileColor);

        if ((pReloadDesc->mType & (RELOAD_TYPE_RESIZE | RELOAD_TYPE_RENDERTARGET)) && !addSwapChainResource())
        {
            return false;
        }

        return addPipelineResource();
    }

    void Unload(ReloadDesc* pReloadDesc) override
    {
        PROFILER_SET_CPU_SCOPE("HelloTriangle", "Unload", kCpuProfileColor);

        if (pGraphicsQueue)
        {
            waitQueueIdle(pGraphicsQueue);
        }

        removePipelineResource();

        if (pReloadDesc->mType & (RELOAD_TYPE_RESIZE | RELOAD_TYPE_RENDERTARGET))
        {
            removeSwapChainResource();
        }
    }

    void Update(float deltaTime) override
    {
        PROFILER_SET_CPU_SCOPE("HelloTriangle", "Update", kCpuProfileColor);
        mElapsedTime += deltaTime;
    }

    void Draw() override
    {
        PROFILER_SET_CPU_SCOPE("HelloTriangle", "Draw", kCpuProfileColor);

        if (!pSwapChain || !pPipeline || !pVertexBuffer)
        {
            return;
        }

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
        {
            waitForFences(pRenderer, 1, &cmdRingElement.pFence);
        }

        resetCmdPool(pRenderer, cmdRingElement.pCmdPool);

        Cmd*         pCmd = cmdRingElement.pCmds[0];
        RenderTarget* pRenderTarget = pSwapChain->ppRenderTargets[swapchainImageIndex];

        beginCmd(pCmd);
        cmdBeginGpuFrameProfile(pCmd, mGpuProfileToken);

        RenderTargetBarrier toRenderTarget = {};
        toRenderTarget.pRenderTarget = pRenderTarget;
        toRenderTarget.mCurrentState = RESOURCE_STATE_PRESENT;
        toRenderTarget.mNewState = RESOURCE_STATE_RENDER_TARGET;
        cmdResourceBarrier(pCmd, 0, nullptr, 0, nullptr, 1, &toRenderTarget);

        BindRenderTargetsDesc bindDesc = {};
        bindDesc.mRenderTargetCount = 1;
        bindDesc.mRenderTargets[0].pRenderTarget = pRenderTarget;
        bindDesc.mRenderTargets[0].mLoadAction = LOAD_ACTION_CLEAR;
        bindDesc.mRenderTargets[0].mStoreAction = STORE_ACTION_STORE;
        bindDesc.mRenderTargets[0].mOverrideClearValue = true;
        bindDesc.mRenderTargets[0].mClearValue.r = 0.05f;
        bindDesc.mRenderTargets[0].mClearValue.g = 0.06f;
        bindDesc.mRenderTargets[0].mClearValue.b = 0.08f;
        bindDesc.mRenderTargets[0].mClearValue.a = 1.0f;
        cmdBindRenderTargets(pCmd, &bindDesc);

        cmdBeginGpuTimestampQuery(pCmd, mGpuProfileToken, "Triangle Pass");

        cmdSetViewport(pCmd, 0.0f, 0.0f, static_cast<float>(mSettings.mWidth), static_cast<float>(mSettings.mHeight), 0.0f, 1.0f);
        cmdSetScissor(pCmd, 0, 0, static_cast<uint32_t>(mSettings.mWidth), static_cast<uint32_t>(mSettings.mHeight));
        cmdBindPipeline(pCmd, pPipeline);

        Buffer*  vertexBuffers[] = { pVertexBuffer };
        uint32_t vertexStrides[] = { static_cast<uint32_t>(sizeof(Vertex)) };
        uint64_t vertexOffsets[] = { 0 };
        cmdBindVertexBuffer(pCmd, 1, vertexBuffers, vertexStrides, vertexOffsets);
        cmdDraw(pCmd, 3, 0);

        cmdEndGpuTimestampQuery(pCmd, mGpuProfileToken);

        cmdBindRenderTargets(pCmd, nullptr);

        RenderTargetBarrier toPresent = {};
        toPresent.pRenderTarget = pRenderTarget;
        toPresent.mCurrentState = RESOURCE_STATE_RENDER_TARGET;
        toPresent.mNewState = RESOURCE_STATE_PRESENT;
        cmdResourceBarrier(pCmd, 0, nullptr, 0, nullptr, 1, &toPresent);

        cmdEndGpuFrameProfile(pCmd, mGpuProfileToken);
        endCmd(pCmd);

        Semaphore* waitSemaphores[] = { pImageAcquiredSemaphore };
        QueueSubmitDesc submitDesc = {};
        submitDesc.ppCmds = &pCmd;
        submitDesc.pSignalFence = cmdRingElement.pFence;
        submitDesc.ppWaitSemaphores = waitSemaphores;
        submitDesc.ppSignalSemaphores = &cmdRingElement.pSemaphore;
        submitDesc.mCmdCount = 1;
        submitDesc.mWaitSemaphoreCount = 1;
        submitDesc.mSignalSemaphoreCount = 1;
        queueSubmit(pGraphicsQueue, &submitDesc);

        QueuePresentDesc presentDesc = {};
        presentDesc.pSwapChain = pSwapChain;
        presentDesc.ppWaitSemaphores = &cmdRingElement.pSemaphore;
        presentDesc.mWaitSemaphoreCount = 1;
        presentDesc.mIndex = static_cast<uint8_t>(swapchainImageIndex);
        presentDesc.mSubmitDone = true;
        queuePresent(pGraphicsQueue, &presentDesc);

        flipProfiler();
    }

    const char* GetName() override { return "HelloTriangle"; }

private:
    bool createShaderResources()
    {
        ShaderSrcDesc shaderDesc = makeShaderSourceDesc();
        addShaderSource(pRenderer, &shaderDesc, &pShader);
        if (!pShader)
        {
            LOGF(eERROR, "Failed to create hello-triangle shader");
            return false;
        }

        Shader* shaders[] = { pShader };
        RootSignatureDesc rootSignatureDesc = {};
        rootSignatureDesc.ppShaders = shaders;
        rootSignatureDesc.mShaderCount = 1;
        addRootSignature(pRenderer, &rootSignatureDesc, &pRootSignature);
        if (!pRootSignature)
        {
            LOGF(eERROR, "Failed to create hello-triangle root signature");
            return false;
        }

        return true;
    }

    bool createVertexBuffer()
    {
        BufferDesc vertexBufferDesc = {};
        vertexBufferDesc.mSize = sizeof(kTriangleVertices);
        vertexBufferDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
        vertexBufferDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
        vertexBufferDesc.mDescriptors = DESCRIPTOR_TYPE_VERTEX_BUFFER;
        vertexBufferDesc.mStartState = RESOURCE_STATE_GENERIC_READ;
        vertexBufferDesc.pName = "HelloTriangle.VertexBuffer";
        vertexBufferDesc.mNodeIndex = pRenderer->mUnlinkedRendererIndex;
        addBuffer(pRenderer, &vertexBufferDesc, &pVertexBuffer);
        if (!pVertexBuffer || !pVertexBuffer->pCpuMappedAddress)
        {
            LOGF(eERROR, "Failed to create hello-triangle vertex buffer");
            return false;
        }

        memcpy(pVertexBuffer->pCpuMappedAddress, kTriangleVertices, sizeof(kTriangleVertices));
        return true;
    }

    bool addPipelineResource()
    {
        if (!pSwapChain)
        {
            return false;
        }

        removePipelineResource();

        VertexLayout vertexLayout = {};
        vertexLayout.mBindingCount = 1;
        vertexLayout.mBindings[0].mStride = sizeof(Vertex);
        vertexLayout.mBindings[0].mRate = VERTEX_BINDING_RATE_VERTEX;

        vertexLayout.mAttribCount = 2;
        vertexLayout.mAttribs[0].mBinding = 0;
        vertexLayout.mAttribs[0].mLocation = 0;
        vertexLayout.mAttribs[0].mSemantic = SEMANTIC_POSITION;
        vertexLayout.mAttribs[0].mFormat = TinyImageFormat_R32G32_SFLOAT;
        vertexLayout.mAttribs[0].mOffset = offsetof(Vertex, position);

        vertexLayout.mAttribs[1].mBinding = 0;
        vertexLayout.mAttribs[1].mLocation = 1;
        vertexLayout.mAttribs[1].mSemantic = SEMANTIC_COLOR;
        vertexLayout.mAttribs[1].mFormat = TinyImageFormat_R32G32B32_SFLOAT;
        vertexLayout.mAttribs[1].mOffset = offsetof(Vertex, color);

        RasterizerStateDesc rasterizerDesc = {};
        rasterizerDesc.mCullMode = CULL_MODE_NONE;

        TinyImageFormat colorFormat = pSwapChain->mFormat;

        PipelineDesc pipelineDesc = {};
        pipelineDesc.mType = PIPELINE_TYPE_GRAPHICS;
        pipelineDesc.pName = "HelloTriangle.Pipeline";
        pipelineDesc.mGraphicsDesc.pShaderProgram = pShader;
        pipelineDesc.mGraphicsDesc.pRootSignature = pRootSignature;
        pipelineDesc.mGraphicsDesc.pVertexLayout = &vertexLayout;
        pipelineDesc.mGraphicsDesc.pRasterizerState = &rasterizerDesc;
        pipelineDesc.mGraphicsDesc.mRenderTargetCount = 1;
        pipelineDesc.mGraphicsDesc.pColorFormats = &colorFormat;
        pipelineDesc.mGraphicsDesc.mSampleCount = SAMPLE_COUNT_1;
        pipelineDesc.mGraphicsDesc.mSampleQuality = 0;
        pipelineDesc.mGraphicsDesc.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
        addPipeline(pRenderer, &pipelineDesc, &pPipeline);

        if (!pPipeline)
        {
            LOGF(eERROR, "Failed to create hello-triangle graphics pipeline");
            return false;
        }

        return true;
    }

    bool addSwapChainResource()
    {
        removeSwapChainResource();

        SwapChainDesc swapChainDesc = {};
        swapChainDesc.mWindowHandle = pWindow->handle;
        swapChainDesc.ppPresentQueues = &pGraphicsQueue;
        swapChainDesc.mPresentQueueCount = 1;
        swapChainDesc.mImageCount = getRecommendedSwapchainImageCount(pRenderer, &pWindow->handle);
        swapChainDesc.mWidth = static_cast<uint32_t>(mSettings.mWidth);
        swapChainDesc.mHeight = static_cast<uint32_t>(mSettings.mHeight);
        swapChainDesc.mColorSpace = COLOR_SPACE_SDR_SRGB;
        swapChainDesc.mEnableVsync = mSettings.mVSyncEnabled;
        swapChainDesc.mColorFormat = getSupportedSwapchainFormat(pRenderer, &swapChainDesc, swapChainDesc.mColorSpace);
        addSwapChain(pRenderer, &swapChainDesc, &pSwapChain);

        if (!pSwapChain)
        {
            LOGF(eERROR, "Failed to create hello-triangle swapchain");
            return false;
        }

        return true;
    }

    void removePipelineResource()
    {
        if (pPipeline)
        {
            removePipeline(pRenderer, pPipeline);
            pPipeline = nullptr;
        }
    }

    void removeSwapChainResource()
    {
        if (pSwapChain)
        {
            removeSwapChain(pRenderer, pSwapChain);
            pSwapChain = nullptr;
        }
    }

    void removeBufferResource()
    {
        if (pVertexBuffer)
        {
            removeBuffer(pRenderer, pVertexBuffer);
            pVertexBuffer = nullptr;
        }
    }

    void removeShaderResources()
    {
        if (pRootSignature)
        {
            removeRootSignature(pRenderer, pRootSignature);
            pRootSignature = nullptr;
        }

        if (pShader)
        {
            removeShader(pRenderer, pShader);
            pShader = nullptr;
        }
    }

private:
    Renderer*      pRenderer = nullptr;
    Queue*         pGraphicsQueue = nullptr;
    SwapChain*     pSwapChain = nullptr;
    Semaphore*     pImageAcquiredSemaphore = nullptr;
    Shader*        pShader = nullptr;
    RootSignature* pRootSignature = nullptr;
    Pipeline*      pPipeline = nullptr;
    Buffer*        pVertexBuffer = nullptr;
    GpuCmdRing     mGraphicsCmdRing = {};
    ProfileToken   mGpuProfileToken = PROFILE_INVALID_TOKEN;
    float          mElapsedTime = 0.0f;
};

DEFINE_APPLICATION_MAIN(HelloTriangleApp)
