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
    return {
        .mStages = SHADER_STAGE_VERT | SHADER_STAGE_FRAG,
        .mVert = {
            .pName = "HelloTriangleVS",
            .pByteCode = const_cast<char*>(kHelloTriangleShader),
            .mByteCodeSize = static_cast<uint32_t>(sizeof(kHelloTriangleShader) - 1),
            .pEntryPoint = "VSMain",
        },
        .mFrag = {
            .pName = "HelloTrianglePS",
            .pByteCode = const_cast<char*>(kHelloTriangleShader),
            .mByteCodeSize = static_cast<uint32_t>(sizeof(kHelloTriangleShader) - 1),
            .pEntryPoint = "PSMain",
        },
    };
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

        QueueDesc queueDesc = {
            .mType = QUEUE_TYPE_GRAPHICS,
            .mFlag = QUEUE_FLAG_NONE,
            .mPriority = QUEUE_PRIORITY_NORMAL,
            .mNodeIndex = pRenderer->mUnlinkedRendererIndex,
            .pName = "HelloTriangle.GraphicsQueue",
        };
        addQueue(pRenderer, &queueDesc, &pGraphicsQueue);
        if (!pGraphicsQueue)
        {
            LOGF(eERROR, "Failed to create graphics queue");
            return false;
        }

        GpuCmdRingDesc cmdRingDesc = {
            .pQueue = pGraphicsQueue,
            .mPoolCount = getRecommendedSwapchainImageCount(pRenderer, &pWindow->handle),
            .mCmdPerPoolCount = 1,
            .mAddSyncPrimitives = true,
        };
        addGpuCmdRing(pRenderer, &cmdRingDesc, &mGraphicsCmdRing);

        Queue*         profilerQueues[] = { pGraphicsQueue };
        const char*    profilerNames[] = { "HelloTriangle GPU" };
        ProfilerDesc   profilerDesc = {
            .pRenderer = pRenderer,
            .ppQueues = profilerQueues,
            .ppProfilerNames = profilerNames,
            .pProfileTokens = &mGpuProfileToken,
            .mGpuProfilerCount = 1,
        };
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

        RenderTargetBarrier toRenderTarget = {
            .pRenderTarget = pRenderTarget,
            .mCurrentState = RESOURCE_STATE_PRESENT,
            .mNewState = RESOURCE_STATE_RENDER_TARGET,
        };
        cmdResourceBarrier(pCmd, 0, nullptr, 0, nullptr, 1, &toRenderTarget);

        BindRenderTargetsDesc bindDesc = {
            .mRenderTargetCount = 1,
            .mRenderTargets = {
                {
                    .pRenderTarget = pRenderTarget,
                    .mLoadAction = LOAD_ACTION_CLEAR,
                    .mStoreAction = STORE_ACTION_STORE,
                    .mClearValue = { .r = 0.05f, .g = 0.06f, .b = 0.08f, .a = 1.0f },
                    .mOverrideClearValue = true,
                },
            },
        };
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

        RenderTargetBarrier toPresent = {
            .pRenderTarget = pRenderTarget,
            .mCurrentState = RESOURCE_STATE_RENDER_TARGET,
            .mNewState = RESOURCE_STATE_PRESENT,
        };
        cmdResourceBarrier(pCmd, 0, nullptr, 0, nullptr, 1, &toPresent);

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
        RootSignatureDesc rootSignatureDesc = {
            .ppShaders = shaders,
            .mShaderCount = 1,
        };
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
        BufferDesc vertexBufferDesc = {
            .mSize = sizeof(kTriangleVertices),
            .pName = "HelloTriangle.VertexBuffer",
            .mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU,
            .mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT,
            .mStartState = RESOURCE_STATE_GENERIC_READ,
            .mDescriptors = DESCRIPTOR_TYPE_VERTEX_BUFFER,
            .mNodeIndex = pRenderer->mUnlinkedRendererIndex,
        };
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

        VertexLayout vertexLayout = {
            .mBindings = {
                {
                    .mStride = sizeof(Vertex),
                    .mRate = VERTEX_BINDING_RATE_VERTEX,
                },
            },
            .mAttribs = {
                {
                    .mSemantic = SEMANTIC_POSITION,
                    .mFormat = TinyImageFormat_R32G32_SFLOAT,
                    .mBinding = 0,
                    .mLocation = 0,
                    .mOffset = offsetof(Vertex, position),
                },
                {
                    .mSemantic = SEMANTIC_COLOR,
                    .mFormat = TinyImageFormat_R32G32B32_SFLOAT,
                    .mBinding = 0,
                    .mLocation = 1,
                    .mOffset = offsetof(Vertex, color),
                },
            },
            .mBindingCount = 1,
            .mAttribCount = 2,
        };

        RasterizerStateDesc rasterizerDesc = {
            .mCullMode = CULL_MODE_NONE,
        };

        TinyImageFormat colorFormat = pSwapChain->mFormat;

        PipelineDesc pipelineDesc = {
            .mGraphicsDesc = {
                .pShaderProgram = pShader,
                .pRootSignature = pRootSignature,
                .pVertexLayout = &vertexLayout,
                .pRasterizerState = &rasterizerDesc,
                .pColorFormats = &colorFormat,
                .mRenderTargetCount = 1,
                .mSampleCount = SAMPLE_COUNT_1,
                .mSampleQuality = 0,
                .mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST,
            },
            .pName = "HelloTriangle.Pipeline",
            .mType = PIPELINE_TYPE_GRAPHICS,
        };
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
