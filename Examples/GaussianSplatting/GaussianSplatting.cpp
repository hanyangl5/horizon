#include <stddef.h>
#include <stdint.h>

#include <ThirdParty/stb/stb_ds.h>
#include "Application/IApp.h"
#include "Core/IFileSystem.h"
#include "Core/ILog.h"
#include "RHI/IGraphics.h"
#include "RHI/RingBuffer.h"
#include "Runtime/RHI/Private/RendererResourceAPI.h"

#include "GaussianSplattingCore.h"
#include "GaussianSplattingHelpers.h"
#include "GaussianSplattingPly.h"

namespace
{
using namespace GaussianSplattingCore;
using namespace GaussianSplattingHelpers;
using namespace GaussianSplattingPly;

// This sample intentionally breaks Gaussian Splatting into the basic steps:
//
// 1. Each splat is a small "cloud point" with position, scale, color, opacity,
//    and elliptical orientation.
// 2. The CPU projects each 3D splat into a 2D screen-space ellipse every frame.
// 3. Each ellipse is drawn as a billboard quad made from two triangles.
// 4. The pixel shader evaluates a Gaussian footprint so the center is dense and
//    the edge fades softly.
// 5. Transparent splats should be drawn back-to-front for reasonable alpha blending.
//
// A production 3D Gaussian Splatting renderer reads millions of points from
// trained .ply data and uses full 3D covariance matrices, spherical harmonic
// colors, GPU sorting/culling, and other acceleration paths. This sample can
// read standard 3DGS .ply files and projects 3D covariance on the CPU. To keep
// the introductory code readable, it still uses only DC color and keeps
// sorting/projection on the CPU.

struct CommandLineOptions
{
    const char* inputPath = nullptr;
    size_t      maxLoadedSplats = kDefaultLoadedSplatLimit;
    bool        quitAfterFrame = false;
    bool        orbitCamera = false;
};

constexpr char kGaussianSplattingShader[] = R"(
struct VSInput
{
    float4 Position : POSITION;
    float2 LocalUv : TEXCOORD0;
    float4 ColorOpacity : COLOR;
};

struct VSOutput
{
    float4 Position : SV_Position;
    float2 LocalUv : TEXCOORD0;
    float4 ColorOpacity : COLOR;
};

VSOutput VSMain(VSInput input)
{
    VSOutput output;
    output.Position = input.Position;
    output.LocalUv = input.LocalUv;
    output.ColorOpacity = input.ColorOpacity;
    return output;
}

float4 PSMain(VSOutput input) : SV_Target0
{
    float2 local = input.LocalUv;
    float radiusSq = dot(local, local);

    float2 supportPosition = local * 2.0f;
    float supportValue = -dot(supportPosition, supportPosition);
    clip(supportValue + 4.0f);

    const float sigma = 0.25f;
    const float invSigmaSq = 1.0f / (sigma * sigma);
    const float gaussianNorm = 1.0f / (sigma * sqrt(2.0f * 3.14159265358979323846f));
    float alpha = saturate(input.ColorOpacity.a * gaussianNorm * exp(-0.5f * radiusSq * invSigmaSq));

    return float4(input.ColorOpacity.rgb, alpha);
}
)";

ShaderSrcDesc makeShaderSourceDesc()
{
    return {
        .mStages = SHADER_STAGE_VERT | SHADER_STAGE_FRAG,
        .mVert = {
            .pName = "GaussianSplattingVS",
            .pByteCode = const_cast<char*>(kGaussianSplattingShader),
            .mByteCodeSize = static_cast<uint32_t>(sizeof(kGaussianSplattingShader) - 1),
            .pEntryPoint = "VSMain",
        },
        .mFrag = {
            .pName = "GaussianSplattingPS",
            .pByteCode = const_cast<char*>(kGaussianSplattingShader),
            .mByteCodeSize = static_cast<uint32_t>(sizeof(kGaussianSplattingShader) - 1),
            .pEntryPoint = "PSMain",
        },
    };
}

CommandLineOptions parseCommandLineOptions()
{
    CommandLineOptions options = {};

    for (int argIndex = 1; argIndex < IApp::argc; ++argIndex)
    {
        const char* arg = IApp::argv[argIndex] ? IApp::argv[argIndex] : "";

        if ((stringEquals(arg, "--input") || stringEquals(arg, "--ply")) && argIndex + 1 < IApp::argc)
        {
            options.inputPath = IApp::argv[++argIndex];
            continue;
        }

        if (stringEquals(arg, "--max-splats") && argIndex + 1 < IApp::argc)
        {
            size_t requestedLimit = 0;
            if (parseSize(IApp::argv[++argIndex], &requestedLimit))
            {
                options.maxLoadedSplats = requestedLimit < kHardLoadedSplatLimit ? requestedLimit : kHardLoadedSplatLimit;
            }
            continue;
        }

        if (stringEquals(arg, "--quit-after-frame"))
        {
            options.quitAfterFrame = true;
            continue;
        }

        if (stringEquals(arg, "--orbit"))
        {
            options.orbitCamera = true;
            continue;
        }

        if (hasSupportedPlyExtension(arg))
        {
            options.inputPath = arg;
        }
    }

    return options;
}

} // namespace

class GaussianSplattingApp final: public IApp
{
public:
    GaussianSplattingApp()
    {
        mSettings.mWidth = 1280;
        mSettings.mHeight = 720;
        mSettings.mVSyncEnabled = true;
        mSettings.mShowPlatformUI = false;
    }

    bool Init() override
    {
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
            .pName = "GaussianSplatting.GraphicsQueue",
        };
        addQueue(pRenderer, &queueDesc, &pGraphicsQueue);
        if (!pGraphicsQueue)
        {
            LOGF(eERROR, "Failed to create graphics queue");
            return false;
        }

        const uint32_t recommendedFrameCount = getRecommendedSwapchainImageCount(pRenderer, &pWindow->handle);
        mFrameCount = recommendedFrameCount > 1u ? recommendedFrameCount : 1u;

        GpuCmdRingDesc cmdRingDesc = {
            .pQueue = pGraphicsQueue,
            .mPoolCount = mFrameCount,
            .mCmdPerPoolCount = 1,
            .mAddSyncPrimitives = true,
        };
        addGpuCmdRing(pRenderer, &cmdRingDesc, &mGraphicsCmdRing);

        addSemaphore(pRenderer, &pImageAcquiredSemaphore);
        if (!pImageAcquiredSemaphore)
        {
            LOGF(eERROR, "Failed to create image-acquired semaphore");
            return false;
        }

        fsSetPathForResourceDir(pSystemFileIO, RM_PROJECT, RD_OTHER_FILES, "");

        mOptions = parseCommandLineOptions();
        if (mOptions.inputPath && mOptions.inputPath[0])
        {
            const char* error = nullptr;
            if (!loadSplatFile(mOptions.inputPath, mOptions.maxLoadedSplats, &mSplats, &error))
            {
                LOGF(eERROR, "Failed to load splat file '%s': %s", mOptions.inputPath, error ? error : "unknown error");
                return false;
            }

            LOGF(eINFO, "Loaded %zu splats from '%s' (limit: %zu)", arrlenu(mSplats), mOptions.inputPath, mOptions.maxLoadedSplats);
        }
        else
        {
            // Use built-in data when no input path is provided, which keeps the
            // sample easy to launch the first time.
            // Example invocations:
            //   GaussianSplatting.exe model.ply
            //   GaussianSplatting.exe --input model.ply --max-splats 500000
            //   GaussianSplatting.exe model.ply --quit-after-frame
            //   GaussianSplatting.exe model.ply --orbit
            makeIntroSplatCloud(&mSplats);
            LOGF(eINFO, "No splat file passed; using the built-in intro splat cloud");
        }

        if (arrlenu(mSplats) == 0)
        {
            LOGF(eERROR, "GaussianSplatting has no splats to render");
            return false;
        }

        const uint64_t vertexCount = (uint64_t)arrlenu(mSplats) * kVerticesPerSplat;
        if (vertexCount > UINT32_MAX)
        {
            LOGF(eERROR, "Too many splats for this introductory renderer: %zu", arrlenu(mSplats));
            return false;
        }

        mMaxSplatVertices = static_cast<uint32_t>(vertexCount);
        arrsetcap(mProjectedSplats, arrlenu(mSplats));

        if (!createShaderResources())
        {
            return false;
        }

        return createVertexBuffers();
    }

    void Exit() override
    {
        if (pGraphicsQueue)
        {
            waitQueueIdle(pGraphicsQueue);
        }

        removeBufferResources();
        removeSplatData();
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
        if ((pReloadDesc->mType & (RELOAD_TYPE_RESIZE | RELOAD_TYPE_RENDERTARGET)) && !addSwapChainResource())
        {
            return false;
        }

        return addPipelineResource();
    }

    void Unload(ReloadDesc* pReloadDesc) override
    {
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
        mElapsedTime += deltaTime;

        // Test/CI mode: quit after the app has initialized and rendered at least
        // one frame, so the sample window does not stay open forever.
        if (mOptions.quitAfterFrame && mElapsedTime > 0.0f)
        {
            mSettings.mQuit = true;
        }
    }

    void Draw() override
    {
        if (!pSwapChain || !pPipeline || arrlenu(mSplatVertexBuffers) == 0)
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

        // Get the command buffer and synchronization objects for this frame.
        // Waiting on the fence below guarantees that if the GPU is still using
        // the previous submission for this frame slot, the CPU waits before
        // overwriting the corresponding vertex buffer.
        GpuCmdRingElement cmdRingElement = getNextGpuCmdRingElement(&mGraphicsCmdRing, true, 1);
        FenceStatus       fenceStatus = FENCE_STATUS_NOTSUBMITTED;
        getFenceStatus(pRenderer, cmdRingElement.pFence, &fenceStatus);
        if (fenceStatus == FENCE_STATUS_INCOMPLETE)
        {
            waitForFences(pRenderer, 1, &cmdRingElement.pFence);
        }

        const uint32_t frameIndex = mGraphicsCmdRing.mPoolIndex;

        // Update splat vertices every frame: rotate the cloud, project to screen,
        // sort by depth, and write quad vertices.
        // This is the main entry point for the CPU-based introductory GS path.
        const uint32_t vertexCount = updateSplatVertexBuffer(frameIndex);

        resetCmdPool(pRenderer, cmdRingElement.pCmdPool);

        Cmd*          pCmd = cmdRingElement.pCmds[0];
        RenderTarget* pRenderTarget = pSwapChain->ppRenderTargets[swapchainImageIndex];

        beginCmd(pCmd);

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
                    .mClearValue = { .r = 0.0f, .g = 0.0f, .b = 0.0f, .a = 1.0f },
                    .mOverrideClearValue = true,
                },
            },
        };
        cmdBindRenderTargets(pCmd, &bindDesc);

        cmdSetViewport(pCmd, 0.0f, 0.0f, static_cast<float>(mSettings.mWidth), static_cast<float>(mSettings.mHeight), 0.0f, 1.0f);
        cmdSetScissor(pCmd, 0, 0, static_cast<uint32_t>(mSettings.mWidth), static_cast<uint32_t>(mSettings.mHeight));

        if (vertexCount > 0)
        {
            // This submits a regular triangle list. Every six vertices represent
            // one splat billboard quad. The soft cloud look comes from Gaussian
            // alpha in the pixel shader, not from the geometry itself.
            Buffer*  vertexBuffers[] = { mSplatVertexBuffers[frameIndex] };
            uint32_t vertexStrides[] = { static_cast<uint32_t>(sizeof(SplatVertex)) };
            uint64_t vertexOffsets[] = { 0 };
            cmdBindVertexBuffer(pCmd, 1, vertexBuffers, vertexStrides, vertexOffsets);

            cmdBindPipeline(pCmd, pPipeline);
            cmdDraw(pCmd, vertexCount, 0);
        }

        cmdBindRenderTargets(pCmd, nullptr);

        RenderTargetBarrier toPresent = {
            .pRenderTarget = pRenderTarget,
            .mCurrentState = RESOURCE_STATE_RENDER_TARGET,
            .mNewState = RESOURCE_STATE_PRESENT,
        };
        cmdResourceBarrier(pCmd, 0, nullptr, 0, nullptr, 1, &toPresent);

        endCmd(pCmd);

        Semaphore*      waitSemaphores[] = { pImageAcquiredSemaphore };
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
    }

    const char* GetName() override { return "GaussianSplatting"; }

private:
    bool createShaderResources()
    {
        ShaderSrcDesc shaderDesc = makeShaderSourceDesc();
        addShaderSource(pRenderer, &shaderDesc, &pShader);
        if (!pShader)
        {
            LOGF(eERROR, "Failed to create gaussian-splatting shader");
            return false;
        }

        Shader*           shaders[] = { pShader };
        RootSignatureDesc rootSignatureDesc = {
            .ppShaders = shaders,
            .mShaderCount = 1,
        };
        addRootSignature(pRenderer, &rootSignatureDesc, &pRootSignature);
        if (!pRootSignature)
        {
            LOGF(eERROR, "Failed to create gaussian-splatting root signature");
            return false;
        }

        return true;
    }

    bool createVertexBuffers()
    {
        arrsetlen(mSplatVertexBuffers, mFrameCount);

        // Each in-flight frame owns a CPU-writable vertex buffer. The CPU
        // reprojects, sorts, and writes splat vertices every frame; reusing one
        // buffer could overwrite data the GPU is still reading.
        for (uint32_t frameIndex = 0; frameIndex < mFrameCount; ++frameIndex)
        {
            mSplatVertexBuffers[frameIndex] = nullptr;
            BufferDesc vertexBufferDesc = {
                .mSize = sizeof(SplatVertex) * static_cast<uint64_t>(mMaxSplatVertices),
                .pName = "GaussianSplatting.SplatVertexBuffer",
                .mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU,
                .mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT,
                .mStartState = RESOURCE_STATE_GENERIC_READ,
                .mDescriptors = DESCRIPTOR_TYPE_VERTEX_BUFFER,
                .mNodeIndex = pRenderer->mUnlinkedRendererIndex,
            };
            addBuffer(pRenderer, &vertexBufferDesc, &mSplatVertexBuffers[frameIndex]);
            if (!mSplatVertexBuffers[frameIndex] || !mSplatVertexBuffers[frameIndex]->pCpuMappedAddress)
            {
                LOGF(eERROR, "Failed to create gaussian-splatting vertex buffer");
                return false;
            }
        }

        return true;
    }

    bool addPipelineResource()
    {
        if (!pSwapChain)
        {
            return false;
        }

        removePipelineResource();

        // This intro sample expands each splat into six ordinary vertices every
        // frame instead of using an instance buffer. It is easier to inspect:
        // every six vertices in GPU memory describe one ellipse.
        VertexLayout vertexLayout = {
            .mBindings = {
                {
                    .mStride = sizeof(SplatVertex),
                    .mRate = VERTEX_BINDING_RATE_VERTEX,
                },
            },
            .mAttribs = {
                {
                    .mSemantic = SEMANTIC_POSITION,
                    .mFormat = TinyImageFormat_R32G32B32A32_SFLOAT,
                    .mBinding = 0,
                    .mLocation = 0,
                    .mOffset = offsetof(SplatVertex, position),
                },
                {
                    .mSemantic = SEMANTIC_TEXCOORD0,
                    .mFormat = TinyImageFormat_R32G32_SFLOAT,
                    .mBinding = 0,
                    .mLocation = 1,
                    .mOffset = offsetof(SplatVertex, uv),
                },
                {
                    .mSemantic = SEMANTIC_COLOR,
                    .mFormat = TinyImageFormat_R32G32B32A32_SFLOAT,
                    .mBinding = 0,
                    .mLocation = 2,
                    .mOffset = offsetof(SplatVertex, color),
                },
            },
            .mBindingCount = 1,
            .mAttribCount = 3,
        };

        // Gaussian splats are transparent, so this uses straight-alpha blending:
        //
        // output = src.rgb * src.a + dst.rgb * (1 - src.a)
        //
        // The splats are sorted back-to-front because transparent blending is
        // not commutative, and drawing near splats first produces visibly
        // incorrect color.
        BlendStateDesc blendStateDesc = {};
        blendStateDesc.mSrcFactors[0] = BC_SRC_ALPHA;
        blendStateDesc.mDstFactors[0] = BC_ONE_MINUS_SRC_ALPHA;
        blendStateDesc.mSrcAlphaFactors[0] = BC_ONE;
        blendStateDesc.mDstAlphaFactors[0] = BC_ONE_MINUS_SRC_ALPHA;
        blendStateDesc.mColorWriteMasks[0] = COLOR_MASK_ALL;
        blendStateDesc.mRenderTargetMask = BLEND_STATE_TARGET_ALL;
        blendStateDesc.mIndependentBlend = false;

        // The intro renderer does not write a depth buffer and relies on sorting
        // for transparency. A production renderer may combine this with
        // tile/binning, OIT, or more complex visibility strategies.
        DepthStateDesc depthStateDesc = {};
        depthStateDesc.mDepthTest = false;
        depthStateDesc.mDepthWrite = false;

        RasterizerStateDesc rasterizerDesc = {};
        rasterizerDesc.mCullMode = CULL_MODE_NONE;
        rasterizerDesc.mScissor = true;

        TinyImageFormat colorFormat = pSwapChain->mFormat;

        PipelineDesc pipelineDesc = {
            .mGraphicsDesc = {
                .pShaderProgram = pShader,
                .pRootSignature = pRootSignature,
                .pVertexLayout = &vertexLayout,
                .pBlendState = &blendStateDesc,
                .pDepthState = &depthStateDesc,
                .pRasterizerState = &rasterizerDesc,
                .pColorFormats = &colorFormat,
                .mRenderTargetCount = 1,
                .mSampleCount = SAMPLE_COUNT_1,
                .mSampleQuality = 0,
                .mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST,
            },
            .pName = "GaussianSplatting.Pipeline",
            .mType = PIPELINE_TYPE_GRAPHICS,
        };
        addPipeline(pRenderer, &pipelineDesc, &pPipeline);

        if (!pPipeline)
        {
            LOGF(eERROR, "Failed to create gaussian-splatting graphics pipeline");
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
            .mImageCount = mFrameCount,
            .mWidth = static_cast<uint32_t>(mSettings.mWidth),
            .mHeight = static_cast<uint32_t>(mSettings.mHeight),
            .mEnableVsync = mSettings.mVSyncEnabled,
            .mColorSpace = COLOR_SPACE_SDR_SRGB,
        };
        swapChainDesc.mColorFormat = getSupportedSwapchainFormat(pRenderer, &swapChainDesc, swapChainDesc.mColorSpace);
        addSwapChain(pRenderer, &swapChainDesc, &pSwapChain);

        if (!pSwapChain)
        {
            LOGF(eERROR, "Failed to create gaussian-splatting swapchain");
            return false;
        }

        return true;
    }

    uint32_t updateSplatVertexBuffer(uint32_t frameIndex)
    {
        if (frameIndex >= arrlenu(mSplatVertexBuffers) || !mSplatVertexBuffers[frameIndex] ||
            !mSplatVertexBuffers[frameIndex]->pCpuMappedAddress)
        {
            return 0;
        }

        SplatVertex* vertices = static_cast<SplatVertex*>(mSplatVertexBuffers[frameIndex]->pCpuMappedAddress);
        return buildSplatVertexBuffer(vertices, mMaxSplatVertices, mSplats, &mProjectedSplats, (float)mSettings.mWidth,
                                      (float)mSettings.mHeight, mOptions.inputPath, mOptions.orbitCamera, mElapsedTime);
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

    void removeBufferResources()
    {
        for (size_t bufferIndex = 0; bufferIndex < arrlenu(mSplatVertexBuffers); ++bufferIndex)
        {
            Buffer*& pBuffer = mSplatVertexBuffers[bufferIndex];
            if (pBuffer)
            {
                removeBuffer(pRenderer, pBuffer);
                pBuffer = nullptr;
            }
        }
        arrfree(mSplatVertexBuffers);
    }

    void removeSplatData()
    {
        arrfree(mSplats);
        arrfree(mProjectedSplats);
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
    Renderer*          pRenderer = nullptr;
    Queue*             pGraphicsQueue = nullptr;
    SwapChain*         pSwapChain = nullptr;
    Semaphore*         pImageAcquiredSemaphore = nullptr;
    Shader*            pShader = nullptr;
    RootSignature*     pRootSignature = nullptr;
    Pipeline*          pPipeline = nullptr;
    GpuCmdRing         mGraphicsCmdRing = {};
    CommandLineOptions mOptions = {};
    Splat*             mSplats = nullptr;
    ProjectedSplat*    mProjectedSplats = nullptr;
    Buffer**           mSplatVertexBuffers = nullptr;
    uint32_t           mMaxSplatVertices = 0;
    uint32_t           mFrameCount = 0;
    float              mElapsedTime = 0.0f;
};

DEFINE_APPLICATION_MAIN(GaussianSplattingApp)
