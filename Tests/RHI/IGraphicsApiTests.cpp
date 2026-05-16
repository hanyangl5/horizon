#include <gtest/gtest.h>

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <functional>
#include <string>
#include <vector>

#define IMEMORY_FROM_HEADER
#include "Core/IFileSystem.h"
#include "Core/IMemory.h"
#include "Core/ILog.h"
#include "Platform/IOperatingSystem.h"
#include "RHI/IGraphics.h"

#if defined(_WINDOWS)
#include <d3dcommon.h>
#endif

extern bool        gRendererUnsupported;
extern const char* pRendererUnsupportedReason;

extern void initWindowClass();
extern void exitWindowClass();

struct SubresourceDataDesc
{
    uint64_t mSrcOffset;
    uint32_t mMipLevel;
    uint32_t mArrayLayer;
};

void FORGE_CALLCONV getBufferSizeAlign(Renderer* pRenderer, const BufferDesc* pDesc, ResourceSizeAlign* pOut);
void FORGE_CALLCONV addBuffer(Renderer* pRenderer, const BufferDesc* pDesc, Buffer** ppBuffer);
void FORGE_CALLCONV removeBuffer(Renderer* pRenderer, Buffer* pBuffer);
void FORGE_CALLCONV mapBuffer(Renderer* pRenderer, Buffer* pBuffer, ReadRange* pRange);
void FORGE_CALLCONV unmapBuffer(Renderer* pRenderer, Buffer* pBuffer);
void FORGE_CALLCONV cmdUpdateBuffer(Cmd* pCmd, Buffer* pBuffer, uint64_t dstOffset, Buffer* pSrcBuffer, uint64_t srcOffset, uint64_t size);
void FORGE_CALLCONV cmdCopySubresource(Cmd* pCmd, Buffer* pDstBuffer, Texture* pTexture, const SubresourceDataDesc* pSubresourceDesc);

namespace
{
constexpr uint32_t kSwapChainWidth = 64;
constexpr uint32_t kSwapChainHeight = 64;
constexpr uint32_t kRenderTargetWidth = 16;
constexpr uint32_t kRenderTargetHeight = 4;

const char* kLifecycleComputeShader = R"(
RWStructuredBuffer<uint> OutputBuffer : register(u0);

[numthreads(1, 1, 1)]
void CSMain(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    OutputBuffer[0] = 0x1234u + dispatchThreadId.x;
}
)";

const char* kGraphicsShader = R"(
cbuffer RootConstantColor : register(b1)
{
    float4 DrawColor;
};

struct VSInput
{
    float2 Position : POSITION;
};

struct VSOutput
{
    float4 Position : SV_Position;
};

VSOutput VSMain(VSInput input)
{
    VSOutput output;
    output.Position = float4(input.Position, 0.0, 1.0);
    return output;
}

float4 PSMain(VSOutput input) : SV_Target0
{
    return DrawColor;
}
)";

const char* kCommandComputeShader = R"(
RWStructuredBuffer<uint> OutputBuffer : register(u0);

cbuffer RootConstantDispatch : register(b0)
{
    uint WriteValue;
    uint OutputIndex;
    uint2 DispatchPadding;
};

cbuffer ComputeRootCbv : register(b1)
{
    uint Multiplier;
    uint3 RootCbvPadding;
};

[numthreads(1, 1, 1)]
void CSMain(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    OutputBuffer[OutputIndex] = (WriteValue + dispatchThreadId.x) * Multiplier;
}
)";

struct RendererInitializationErrorScope
{
    bool        previousUnsupported = false;
    const char* previousReason = nullptr;

    RendererInitializationErrorScope()
    {
        previousUnsupported = gRendererUnsupported;
        previousReason = pRendererUnsupportedReason;
    }

    ~RendererInitializationErrorScope()
    {
        gRendererUnsupported = previousUnsupported;
        pRendererUnsupportedReason = previousReason;
    }
};

class DeferredCleanup
{
public:
    void add(std::function<void()> fn) { mActions.emplace_back(std::move(fn)); }

    ~DeferredCleanup()
    {
        for (auto it = mActions.rbegin(); it != mActions.rend(); ++it)
        {
            (*it)();
        }
    }

private:
    std::vector<std::function<void()>> mActions;
};

const char* getRendererInitReason()
{
    const char* reason = "";
    hasRendererInitializationError(&reason);
    return reason ? reason : "";
}

#define ASSERT_RHI_CALL_OR_SKIP_ON_UNSUPPORTED(expr)                                                                                  \
    do                                                                                                                                \
    {                                                                                                                                 \
        if (!(expr))                                                                                                                  \
        {                                                                                                                             \
            const char* reason = "";                                                                                                  \
            if (hasRendererInitializationError(&reason))                                                                              \
            {                                                                                                                         \
                GTEST_SKIP() << "Skipping because renderer initialization is unsupported: " << (reason ? reason : "unknown reason"); \
            }                                                                                                                         \
            FAIL() << #expr << " failed: " << getRendererInitReason();                                                                \
        }                                                                                                                             \
    } while (false)

ShaderSrcDesc makeShaderSourceDesc(ShaderStage stages, const char* source, const char* entry0, const char* entry1 = nullptr)
{
    ShaderSrcDesc desc = {
        .mStages = stages,
    };

    if (stages & SHADER_STAGE_VERT)
    {
        desc.mVert.pName = "InlineVert";
        desc.mVert.pByteCode = const_cast<char*>(source);
        desc.mVert.mByteCodeSize = (uint32_t)strlen(source);
        desc.mVert.pEntryPoint = entry0;
    }

    if (stages & SHADER_STAGE_FRAG)
    {
        desc.mFrag.pName = "InlineFrag";
        desc.mFrag.pByteCode = const_cast<char*>(source);
        desc.mFrag.mByteCodeSize = (uint32_t)strlen(source);
        desc.mFrag.pEntryPoint = entry1 ? entry1 : entry0;
    }

    if (stages & SHADER_STAGE_COMP)
    {
        desc.mComp.pName = "InlineComp";
        desc.mComp.pByteCode = const_cast<char*>(source);
        desc.mComp.mByteCodeSize = (uint32_t)strlen(source);
        desc.mComp.pEntryPoint = entry0;
    }

    return desc;
}

BinaryShaderDesc makeBinaryShaderDescFromSourceShader(const Shader* pShader, ShaderStage stage, const char* entryPoint)
{
    BinaryShaderDesc desc = {
        .mStages = stage,
    };

    if (stage == SHADER_STAGE_COMP)
    {
        desc.mComp.pName = "BinaryComp";
        desc.mComp.pByteCode = pShader->mDx.pShaderBlobs[0]->GetBufferPointer();
        desc.mComp.mByteCodeSize = (uint32_t)pShader->mDx.pShaderBlobs[0]->GetBufferSize();
        desc.mComp.pEntryPoint = entryPoint;
    }

    return desc;
}

struct LiveRendererHarness
{
    RendererContext* pContext = nullptr;
    Renderer*        pRenderer = nullptr;
    Queue*           pQueue = nullptr;
    Fence*           pFence = nullptr;
    Semaphore*       pSemaphore = nullptr;
    CmdPool*         pCmdPool = nullptr;
    Cmd*             pCmd = nullptr;
    Cmd**            ppExtraCmds = nullptr;
    uint32_t         mExtraCmdCount = 0;
    WindowDesc       mWindow = {};
    bool             mWindowClassInitialized = false;
    bool             mWindowOpened = false;
    bool             mWindowCreationUnavailable = false;
    SwapChain*       pSwapChain = nullptr;
    bool             mOwnsRenderer = true;
    bool             mOwnsContext = true;

    bool initRendererOnly(const char* appName)
    {
        RendererContextDesc contextDesc = {};
        initRendererContext(appName, &contextDesc, &pContext);
        if (!pContext)
        {
            return false;
        }

        RendererDesc rendererDesc = {
            .pContext = pContext,
        };
        initRenderer(appName, &rendererDesc, &pRenderer);
        if (!pRenderer)
        {
            return false;
        }

        return true;
    }

    bool attachRendererOnly(RendererContext* pSharedContext, Renderer* pSharedRenderer)
    {
        pContext = pSharedContext;
        pRenderer = pSharedRenderer;
        mOwnsContext = false;
        mOwnsRenderer = false;
        return pContext && pRenderer;
    }

    bool addGraphicsQueueOnly()
    {
        if (!pRenderer)
        {
            ADD_FAILURE() << "Renderer must be initialized before creating a queue";
            return false;
        }

        QueueDesc queueDesc = {
            .mType = QUEUE_TYPE_GRAPHICS,
            .mFlag = QUEUE_FLAG_NONE,
            .mPriority = QUEUE_PRIORITY_NORMAL,
            .pName = "IGraphicsApiTest.Queue",
        };
        addQueue(pRenderer, &queueDesc, &pQueue);
        if (!pQueue)
        {
            ADD_FAILURE() << "addQueue returned null";
            return false;
        }

        return true;
    }

    bool addSynchronizationPrimitivesOnly()
    {
        if (!pRenderer)
        {
            ADD_FAILURE() << "Renderer must be initialized before creating synchronization primitives";
            return false;
        }

        addFence(pRenderer, &pFence);
        addSemaphore(pRenderer, &pSemaphore);
        if (!pFence || !pSemaphore)
        {
            ADD_FAILURE() << "Failed to create synchronization primitives";
            return false;
        }

        return true;
    }

    bool addCommandPoolOnly()
    {
        if (!pRenderer)
        {
            ADD_FAILURE() << "Renderer must be initialized before creating a command pool";
            return false;
        }
        if (!pQueue)
        {
            ADD_FAILURE() << "Queue must be initialized before creating a command pool";
            return false;
        }

        CmdPoolDesc poolDesc = {
            .pQueue = pQueue,
            .mTransient = false,
        };
        addCmdPool(pRenderer, &poolDesc, &pCmdPool);
        if (!pCmdPool)
        {
            ADD_FAILURE() << "addCmdPool returned null";
            return false;
        }

        return true;
    }

    bool addPrimaryCmdOnly()
    {
        if (!pRenderer)
        {
            ADD_FAILURE() << "Renderer must be initialized before creating a command buffer";
            return false;
        }
        if (!pCmdPool)
        {
            ADD_FAILURE() << "Command pool must be initialized before creating a command buffer";
            return false;
        }

        CmdDesc cmdDesc = {
            .pPool = pCmdPool,
#ifdef ENABLE_GRAPHICS_DEBUG
            .pName = "IGraphicsApiTest.PrimaryCmd",
#endif
        };
        addCmd(pRenderer, &cmdDesc, &pCmd);
        if (!pCmd)
        {
            ADD_FAILURE() << "addCmd returned null";
            return false;
        }

        return true;
    }

    bool addExtraCmdsOnly(uint32_t extraCmdCount)
    {
        if (!pRenderer)
        {
            ADD_FAILURE() << "Renderer must be initialized before creating extra command buffers";
            return false;
        }
        if (!pCmdPool)
        {
            ADD_FAILURE() << "Command pool must be initialized before creating extra command buffers";
            return false;
        }

        if (extraCmdCount == 0)
        {
            return true;
        }

        CmdDesc cmdDesc = {
            .pPool = pCmdPool,
        };

        if (extraCmdCount > 0)
        {
            addCmd_n(pRenderer, &cmdDesc, extraCmdCount, &ppExtraCmds);
            mExtraCmdCount = extraCmdCount;
            if (!ppExtraCmds)
            {
                ADD_FAILURE() << "addCmd_n returned null";
                return false;
            }
        }

        return true;
    }

    bool init(const char* appName, uint32_t extraCmdCount)
    {
        return initRendererOnly(appName) && addGraphicsQueueOnly() && addSynchronizationPrimitivesOnly() && addCommandPoolOnly() &&
               addPrimaryCmdOnly() && addExtraCmdsOnly(extraCmdCount);
    }

    bool createWindowAndSwapChain(const char* appName, uint32_t width, uint32_t height)
    {
        mWindowCreationUnavailable = false;
        initWindowClass();
        mWindowClassInitialized = true;

        mWindow = {
            .windowedRect = { 0, 0, (int)width, (int)height },
            .fullscreenRect = { 0, 0, (int)width, (int)height },
            .clientRect = { 0, 0, (int)width, (int)height },
            .hide = true,
            .noresizeFrame = true,
            .overrideDefaultPosition = true,
        };

        openWindow(appName, &mWindow);
        mWindowOpened = mWindow.handle.window != nullptr;
        if (!mWindowOpened)
        {
            mWindowCreationUnavailable = true;
            return false;
        }

        Queue* presentQueues[] = { pQueue };
        SwapChainDesc swapChainDesc = {
            .mWindowHandle = mWindow.handle,
            .ppPresentQueues = presentQueues,
            .mPresentQueueCount = 1,
            .mImageCount = getRecommendedSwapchainImageCount(pRenderer, &mWindow.handle),
            .mWidth = width,
            .mHeight = height,
            .mEnableVsync = false,
            .mColorSpace = COLOR_SPACE_SDR_SRGB,
        };
        swapChainDesc.mColorFormat = getSupportedSwapchainFormat(pRenderer, &swapChainDesc, swapChainDesc.mColorSpace);

        addSwapChain(pRenderer, &swapChainDesc, &pSwapChain);
        if (!pSwapChain)
        {
            ADD_FAILURE() << "addSwapChain returned null";
            return false;
        }

        return true;
    }

    void submitAndWait(Cmd* pCmdToSubmit, Fence* pSubmitFence, uint32_t waitSemaphoreCount = 0, Semaphore** ppWaitSemaphores = nullptr,
                       uint32_t signalSemaphoreCount = 0, Semaphore** ppSignalSemaphores = nullptr)
    {
        QueueSubmitDesc submitDesc = {
            .ppCmds = &pCmdToSubmit,
            .pSignalFence = pSubmitFence,
            .ppWaitSemaphores = ppWaitSemaphores,
            .ppSignalSemaphores = ppSignalSemaphores,
            .mCmdCount = 1,
            .mWaitSemaphoreCount = waitSemaphoreCount,
            .mSignalSemaphoreCount = signalSemaphoreCount,
        };
        queueSubmit(pQueue, &submitDesc);
        waitForFences(pRenderer, 1, &pSubmitFence);
    }

    ~LiveRendererHarness()
    {
        if (pQueue)
        {
            waitQueueIdle(pQueue);
        }

        if (pSwapChain && pRenderer)
        {
            removeSwapChain(pRenderer, pSwapChain);
            pSwapChain = nullptr;
        }

        if (mWindowOpened)
        {
            closeWindow(&mWindow);
            mWindowOpened = false;
        }

        if (mWindowClassInitialized)
        {
            exitWindowClass();
            mWindowClassInitialized = false;
        }

        if (ppExtraCmds && pRenderer)
        {
            removeCmd_n(pRenderer, mExtraCmdCount, ppExtraCmds);
            ppExtraCmds = nullptr;
            mExtraCmdCount = 0;
        }

        if (pCmd && pRenderer)
        {
            removeCmd(pRenderer, pCmd);
            pCmd = nullptr;
        }

        if (pCmdPool && pRenderer)
        {
            removeCmdPool(pRenderer, pCmdPool);
            pCmdPool = nullptr;
        }

        if (pSemaphore && pRenderer)
        {
            removeSemaphore(pRenderer, pSemaphore);
            pSemaphore = nullptr;
        }

        if (pFence && pRenderer)
        {
            removeFence(pRenderer, pFence);
            pFence = nullptr;
        }

        if (pQueue && pRenderer)
        {
            removeQueue(pRenderer, pQueue);
            pQueue = nullptr;
        }

        if (pRenderer && mOwnsRenderer)
        {
            exitRenderer(pRenderer);
        }
        pRenderer = nullptr;

        if (pContext && mOwnsContext)
        {
            exitRendererContext(pContext);
        }
        pContext = nullptr;
    }
};

struct alignas(16) RootCbvConstants
{
    uint32_t multiplier;
    uint32_t padding[3];
};

struct DispatchCommandData
{
    IndirectDispatchArguments args;
    uint32_t                  padding;
};

#if defined(_WINDOWS)
std::wstring getD3DObjectName(ID3D12Object* pObject)
{
#if defined(ENABLE_GRAPHICS_DEBUG)
    wchar_t buffer[128] = {};
    UINT    size = sizeof(buffer);
    if (!pObject || FAILED(pObject->GetPrivateData(WKPDID_D3DDebugObjectNameW, &size, buffer)) || size == 0)
    {
        return {};
    }

    size_t length = size / sizeof(wchar_t);
    if (length > 0 && buffer[length - 1] == L'\0')
    {
        --length;
    }

    return std::wstring(buffer, length);
#else
    UNREF_PARAM(pObject);
    return {};
#endif
}

void expectD3DObjectName(ID3D12Object* pObject, const wchar_t* expectedName)
{
#if defined(ENABLE_GRAPHICS_DEBUG)
    ASSERT_NE(pObject, nullptr);
    EXPECT_EQ(getD3DObjectName(pObject), std::wstring(expectedName));
#else
    UNREF_PARAM(pObject);
    UNREF_PARAM(expectedName);
#endif
}
#endif

const uint8_t* getTexturePixel(const uint8_t* data, uint32_t rowPitch, uint32_t x, uint32_t y)
{
    return data + rowPitch * y + x * 4;
}

void expectPixelEq(const uint8_t* pixel, uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    EXPECT_EQ(pixel[0], r);
    EXPECT_EQ(pixel[1], g);
    EXPECT_EQ(pixel[2], b);
    EXPECT_EQ(pixel[3], a);
}

} // namespace

class RHIIGraphicsApiTest : public ::testing::Test
{
protected:
    static void SetUpTestSuite()
    {
        ASSERT_TRUE(initMemAlloc(nullptr));
        _EnableInteractiveMode(false);

        FileSystemInitDesc fsDesc = {
            .pAppName = "RHITests",
        };
        ASSERT_TRUE(initFileSystem(&fsDesc));

        fsSetPathForResourceDir(pSystemFileIO, RM_DEBUG, RD_LOG, "");
        fsSetPathForResourceDir(pSystemFileIO, RM_DEBUG, RD_SHADER_BINARIES, "CompiledShaders");

        initLog(nullptr, eERROR);

        sSharedRenderer = new LiveRendererHarness();
        if (!sSharedRenderer->initRendererOnly("RHIIGraphicsApiTests"))
        {
            delete sSharedRenderer;
            sSharedRenderer = nullptr;
        }
    }

    static void TearDownTestSuite()
    {
        delete sSharedRenderer;
        sSharedRenderer = nullptr;

        exitLog();
        exitFileSystem();
        _EnableInteractiveMode(true);
        exitMemAlloc();
    }

    static bool attachSharedRenderer(LiveRendererHarness& harness)
    {
        return sSharedRenderer && harness.attachRendererOnly(sSharedRenderer->pContext, sSharedRenderer->pRenderer);
    }

    static bool initSharedRendererHarness(LiveRendererHarness& harness, uint32_t extraCmdCount)
    {
        return attachSharedRenderer(harness) && harness.addGraphicsQueueOnly() && harness.addSynchronizationPrimitivesOnly() &&
               harness.addCommandPoolOnly() && harness.addPrimaryCmdOnly() && harness.addExtraCmdsOnly(extraCmdCount);
    }

    static LiveRendererHarness* sSharedRenderer;
};

LiveRendererHarness* RHIIGraphicsApiTest::sSharedRenderer = nullptr;

// Verifies that the renderer initialization error helpers expose the stored state and reason string.
TEST_F(RHIIGraphicsApiTest, RendererInitializationErrorHelpersRoundTripState)
{
    RendererInitializationErrorScope restoreState;

    gRendererUnsupported = false;
    pRendererUnsupportedReason = "";

    const char* reason = nullptr;
    EXPECT_FALSE(hasRendererInitializationError(&reason));
    EXPECT_STREQ(reason, "");

    setRendererInitializationError("synthetic init failure");

    EXPECT_TRUE(hasRendererInitializationError(&reason));
    EXPECT_STREQ(reason, "synthetic init failure");
}

// Verifies that descriptor lookup, flag operators, and indirect argument index helpers behave as expected for the public API surface.
TEST_F(RHIIGraphicsApiTest, HeaderHelpersPreserveExpectedLookupAndFlagBehavior)
{
    DescriptorInfo descriptors[3] = {
        { .pName = "FrameData" },
        { .pName = "SceneTexture" },
        { .pName = "OutputBuffer" },
    };

    RootSignature rootSignature = {
        .mDescriptorCount = TF_ARRAY_COUNT(descriptors),
        .pDescriptors = descriptors,
    };

    EXPECT_EQ(getDescriptorIndexFromName(&rootSignature, "FrameData"), 0u);
    EXPECT_EQ(getDescriptorIndexFromName(&rootSignature, "SceneTexture"), 1u);
    EXPECT_EQ(getDescriptorIndexFromName(&rootSignature, "Missing"), UINT32_MAX);

    ResourceState combinedState = RESOURCE_STATE_COPY_DEST | RESOURCE_STATE_SHADER_RESOURCE;
    EXPECT_EQ(combinedState & RESOURCE_STATE_COPY_DEST, RESOURCE_STATE_COPY_DEST);
    EXPECT_EQ(combinedState & RESOURCE_STATE_PIXEL_SHADER_RESOURCE, RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    EXPECT_EQ(combinedState & RESOURCE_STATE_PRESENT, RESOURCE_STATE_UNDEFINED);

    DescriptorType descriptorMask = DESCRIPTOR_TYPE_TEXTURE | DESCRIPTOR_TYPE_RW_TEXTURE;
    EXPECT_EQ(descriptorMask & DESCRIPTOR_TYPE_TEXTURE, DESCRIPTOR_TYPE_TEXTURE);
    EXPECT_EQ(descriptorMask & DESCRIPTOR_TYPE_RW_TEXTURE, DESCRIPTOR_TYPE_RW_TEXTURE);
    EXPECT_EQ(descriptorMask & DESCRIPTOR_TYPE_BUFFER, DESCRIPTOR_TYPE_UNDEFINED);

    QueueFlag queueFlags = QUEUE_FLAG_DISABLE_GPU_TIMEOUT | QUEUE_FLAG_INIT_MICROPROFILE;
    EXPECT_EQ(queueFlags & QUEUE_FLAG_DISABLE_GPU_TIMEOUT, QUEUE_FLAG_DISABLE_GPU_TIMEOUT);
    EXPECT_EQ(queueFlags & QUEUE_FLAG_INIT_MICROPROFILE, QUEUE_FLAG_INIT_MICROPROFILE);
    EXPECT_EQ(queueFlags & QUEUE_FLAG_NONE, QUEUE_FLAG_NONE);

    EXPECT_EQ(INDIRECT_DRAW_ELEM_INDEX(mVertexCount), 0u);
    EXPECT_EQ(INDIRECT_DRAW_ELEM_INDEX(mInstanceCount), 1u);
    EXPECT_EQ(INDIRECT_DRAW_ELEM_INDEX(mStartVertex), 2u);
    EXPECT_EQ(INDIRECT_DRAW_ELEM_INDEX(mStartInstance), 3u);

    EXPECT_EQ(INDIRECT_DRAW_INDEX_ELEM_INDEX(mIndexCount), 0u);
    EXPECT_EQ(INDIRECT_DRAW_INDEX_ELEM_INDEX(mInstanceCount), 1u);
    EXPECT_EQ(INDIRECT_DRAW_INDEX_ELEM_INDEX(mStartIndex), 2u);
    EXPECT_EQ(INDIRECT_DRAW_INDEX_ELEM_INDEX(mVertexOffset), 3u);
    EXPECT_EQ(INDIRECT_DRAW_INDEX_ELEM_INDEX(mStartInstance), 4u);

    EXPECT_EQ(INDIRECT_DISPATCH_ELEM_INDEX(mGroupCountX), 0u);
    EXPECT_EQ(INDIRECT_DISPATCH_ELEM_INDEX(mGroupCountY), 1u);
    EXPECT_EQ(INDIRECT_DISPATCH_ELEM_INDEX(mGroupCountZ), 2u);
}

struct LifecycleShaderBundle
{
    Shader*        pSourceShader = nullptr;
    Shader*        pBinaryShader = nullptr;
    RootSignature* pRootSignature = nullptr;
    uint32_t       outputIndex = UINT32_MAX;
};

struct LifecyclePlacedBufferBundle
{
    ResourceHeap*      pHeap = nullptr;
    Buffer*            pBuffer = nullptr;
    ResourceSizeAlign  sizeAlign = {};
};

struct LifecyclePipelineBundle
{
    PipelineCache* pPipelineCache = nullptr;
    Pipeline*      pPipeline = nullptr;
};

struct PixelValue
{
    uint8_t r = 0;
    uint8_t g = 0;
    uint8_t b = 0;
    uint8_t a = 0;
};

enum class DrawCallKind
{
    Draw,
    DrawInstanced,
    DrawIndexed,
    DrawIndexedInstanced,
};

struct GraphicsDrawSetup
{
    RenderTarget*                   pRenderTarget = nullptr;
    Buffer*                         pReadbackBuffer = nullptr;
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint = {};
    UINT64                          totalTextureBytes = 0;
    Shader*                         pShader = nullptr;
    RootSignature*                  pRootSignature = nullptr;
    uint32_t                        rootConstantIndex = UINT32_MAX;
    Buffer*                         pVertexBuffer = nullptr;
    Buffer*                         pIndexBuffer = nullptr;
    Pipeline*                       pPipeline = nullptr;
};

struct ComputeCommandSetup
{
    Shader*             pShader = nullptr;
    RootSignature*      pRootSignature = nullptr;
    DescriptorSet*      pDescriptorSet = nullptr;
    uint32_t            outputIndex = UINT32_MAX;
    uint32_t            rootCbvIndex = UINT32_MAX;
    uint32_t            rootConstantIndex = UINT32_MAX;
    Buffer*             pOutputBuffer = nullptr;
    Buffer*             pRootCbvBuffer = nullptr;
    RootCbvConstants    rootCbvData = {};
    Buffer*             pReadbackBuffer = nullptr;
    Buffer*             pIndirectBuffer = nullptr;
    Pipeline*           pPipeline = nullptr;
    CommandSignature*   pDispatchSignature = nullptr;
};

bool createLifecycleShaderBundle(LiveRendererHarness& harness, DeferredCleanup& cleanup, LifecycleShaderBundle* pOut)
{
    ShaderSrcDesc shaderSourceDesc = makeShaderSourceDesc(SHADER_STAGE_COMP, kLifecycleComputeShader, "CSMain");
    addShaderSource(harness.pRenderer, &shaderSourceDesc, &pOut->pSourceShader);
    if (!pOut->pSourceShader)
    {
        ADD_FAILURE() << "addShaderSource returned null";
        return false;
    }
    Shader* pSourceShader = pOut->pSourceShader;
    cleanup.add([renderer = harness.pRenderer, pSourceShader]
    {
        if (pSourceShader)
        {
            removeShader(renderer, pSourceShader);
        }
    });

    BinaryShaderDesc binaryShaderDesc = makeBinaryShaderDescFromSourceShader(pOut->pSourceShader, SHADER_STAGE_COMP, "CSMain");
    addShaderBinary(harness.pRenderer, &binaryShaderDesc, &pOut->pBinaryShader);
    if (!pOut->pBinaryShader)
    {
        ADD_FAILURE() << "addShaderBinary returned null";
        return false;
    }
    Shader* pBinaryShader = pOut->pBinaryShader;
    cleanup.add([renderer = harness.pRenderer, pBinaryShader]
    {
        if (pBinaryShader)
        {
            removeShader(renderer, pBinaryShader);
        }
    });

    Shader* rootShaders[] = { pOut->pBinaryShader };
    RootSignatureDesc rootSignatureDesc = {
        .ppShaders = rootShaders,
        .mShaderCount = 1,
    };
    addRootSignature(harness.pRenderer, &rootSignatureDesc, &pOut->pRootSignature);
    if (!pOut->pRootSignature)
    {
        ADD_FAILURE() << "addRootSignature returned null";
        return false;
    }
    RootSignature* pRootSignature = pOut->pRootSignature;
    cleanup.add([renderer = harness.pRenderer, pRootSignature]
    {
        if (pRootSignature)
        {
            removeRootSignature(renderer, pRootSignature);
        }
    });

    pOut->outputIndex = getDescriptorIndexFromName(pOut->pRootSignature, "OutputBuffer");
    if (pOut->outputIndex == UINT32_MAX)
    {
        ADD_FAILURE() << "OutputBuffer binding was not reflected";
        return false;
    }

    return true;
}

bool createLifecyclePlacedBuffer(LiveRendererHarness& harness, DeferredCleanup& cleanup, LifecyclePlacedBufferBundle* pOut)
{
    BufferDesc placedBufferDesc = {
        .mSize = sizeof(uint32_t) * 4,
        .mElementCount = 4,
        .mStructStride = sizeof(uint32_t),
        .pName = "LifecyclePlacedBuffer",
        .mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY,
        .mStartState = RESOURCE_STATE_UNORDERED_ACCESS,
        .mDescriptors = DESCRIPTOR_TYPE_RW_BUFFER,
    };
    getBufferSizeAlign(harness.pRenderer, &placedBufferDesc, &pOut->sizeAlign);

    ResourceHeapDesc heapDesc = {
        .mSize = pOut->sizeAlign.mSize,
        .mAlignment = pOut->sizeAlign.mAlignment,
        .mMemoryUsage = placedBufferDesc.mMemoryUsage,
        .mDescriptors = placedBufferDesc.mDescriptors,
        .mFlags = RESOURCE_HEAP_FLAG_ALLOW_ONLY_BUFFERS,
        .pName = "LifecycleBufferHeap",
    };
    addResourceHeap(harness.pRenderer, &heapDesc, &pOut->pHeap);
    if (!pOut->pHeap)
    {
        ADD_FAILURE() << "addResourceHeap returned null";
        return false;
    }
    ResourceHeap* pHeap = pOut->pHeap;
    cleanup.add([renderer = harness.pRenderer, pHeap]
    {
        if (pHeap)
        {
            removeResourceHeap(renderer, pHeap);
        }
    });

    ResourcePlacement placement = {
        .pHeap = pOut->pHeap,
        .mOffset = 0,
    };
    placedBufferDesc.pPlacement = &placement;
    addBuffer(harness.pRenderer, &placedBufferDesc, &pOut->pBuffer);
    if (!pOut->pBuffer)
    {
        ADD_FAILURE() << "addBuffer returned null for placed buffer";
        return false;
    }
    Buffer* pBuffer = pOut->pBuffer;
    cleanup.add([renderer = harness.pRenderer, pBuffer]
    {
        if (pBuffer)
        {
            removeBuffer(renderer, pBuffer);
        }
    });

    return true;
}

bool createDescriptorSetForOutput(LiveRendererHarness& harness, DeferredCleanup& cleanup, RootSignature* pRootSignature, Buffer* pBuffer,
                                  DescriptorSet** ppDescriptorSet)
{
    DescriptorSetDesc descriptorSetDesc = {
        .pRootSignature = pRootSignature,
        .mUpdateFrequency = DESCRIPTOR_UPDATE_FREQ_NONE,
        .mMaxSets = 1,
    };
    addDescriptorSet(harness.pRenderer, &descriptorSetDesc, ppDescriptorSet);
    if (!*ppDescriptorSet)
    {
        ADD_FAILURE() << "addDescriptorSet returned null";
        return false;
    }
    DescriptorSet* pDescriptorSet = *ppDescriptorSet;
    cleanup.add([renderer = harness.pRenderer, pDescriptorSet]
    {
        if (pDescriptorSet)
        {
            removeDescriptorSet(renderer, pDescriptorSet);
        }
    });

    DescriptorData bufferUpdate = {
        .pName = "OutputBuffer",
        .mCount = 1,
        .ppBuffers = &pBuffer,
    };
    updateDescriptorSet(harness.pRenderer, 0, *ppDescriptorSet, 1, &bufferUpdate);
    return true;
}

bool createLifecyclePipelineBundle(LiveRendererHarness& harness, DeferredCleanup& cleanup, Shader* pShader, RootSignature* pRootSignature,
                                   LifecyclePipelineBundle* pOut)
{
    PipelineCacheDesc pipelineCacheDesc = {};
    addPipelineCache(harness.pRenderer, &pipelineCacheDesc, &pOut->pPipelineCache);
    if (!pOut->pPipelineCache)
    {
        ADD_FAILURE() << "addPipelineCache returned null";
        return false;
    }
    PipelineCache* pPipelineCache = pOut->pPipelineCache;
    cleanup.add([renderer = harness.pRenderer, pPipelineCache]
    {
        if (pPipelineCache)
        {
            removePipelineCache(renderer, pPipelineCache);
        }
    });

    PipelineDesc pipelineDesc = {
        .mComputeDesc = {
            .pShaderProgram = pShader,
            .pRootSignature = pRootSignature,
        },
        .pCache = pOut->pPipelineCache,
        .pName = "LifecycleComputePipeline",
        .mType = PIPELINE_TYPE_COMPUTE,
    };
    addPipeline(harness.pRenderer, &pipelineDesc, &pOut->pPipeline);
    if (!pOut->pPipeline)
    {
        ADD_FAILURE() << "addPipeline returned null";
        return false;
    }
    Pipeline* pPipeline = pOut->pPipeline;
    cleanup.add([renderer = harness.pRenderer, pPipeline]
    {
        if (pPipeline)
        {
            removePipeline(renderer, pPipeline);
        }
    });

    return true;
}

bool createRenderTarget(LiveRendererHarness& harness, DeferredCleanup& cleanup, const char* name, uint32_t width, uint32_t height,
                        RenderTarget** ppRenderTarget)
{
    RenderTargetDesc renderTargetDesc = {
        .mWidth = width,
        .mHeight = height,
        .mDepth = 1,
        .mArraySize = 1,
        .mMipLevels = 1,
        .mSampleCount = SAMPLE_COUNT_1,
        .mFormat = TinyImageFormat_R8G8B8A8_UNORM,
        .mStartState = RESOURCE_STATE_RENDER_TARGET,
        .pName = name,
    };
    addRenderTarget(harness.pRenderer, &renderTargetDesc, ppRenderTarget);
    if (!*ppRenderTarget)
    {
        ADD_FAILURE() << "addRenderTarget returned null";
        return false;
    }
    RenderTarget* pRenderTarget = *ppRenderTarget;
    cleanup.add([renderer = harness.pRenderer, pRenderTarget]
    {
        if (pRenderTarget)
        {
            removeRenderTarget(renderer, pRenderTarget);
        }
    });
    return true;
}

bool createGraphicsDrawSetup(LiveRendererHarness& harness, DeferredCleanup& cleanup, GraphicsDrawSetup* pOut)
{
    if (!createRenderTarget(harness, cleanup, "CommandRenderTarget", kRenderTargetWidth, kRenderTargetHeight, &pOut->pRenderTarget))
    {
        return false;
    }

    D3D12_RESOURCE_DESC textureResourceDesc = pOut->pRenderTarget->pTexture->mDx.pResource->GetDesc();
    if (textureResourceDesc.Flags & D3D12_RESOURCE_FLAG_USE_TIGHT_ALIGNMENT)
    {
        textureResourceDesc.Alignment = 0;
    }
    UINT                numRows = 0;
    UINT64              rowSizeInBytes = 0;
    harness.pRenderer->mDx.pDevice->GetCopyableFootprints(&textureResourceDesc, 0, 1, 0, &pOut->footprint, &numRows, &rowSizeInBytes,
                                                          &pOut->totalTextureBytes);

    BufferDesc textureReadbackDesc = {
        .mSize = pOut->totalTextureBytes,
        .pName = "CommandTextureReadback",
        .mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_TO_CPU,
        .mStartState = RESOURCE_STATE_COPY_DEST,
    };
    addBuffer(harness.pRenderer, &textureReadbackDesc, &pOut->pReadbackBuffer);
    if (!pOut->pReadbackBuffer)
    {
        ADD_FAILURE() << "addBuffer returned null for texture readback";
        return false;
    }
    Buffer* pReadbackBuffer = pOut->pReadbackBuffer;
    cleanup.add([renderer = harness.pRenderer, pReadbackBuffer]
    {
        if (pReadbackBuffer)
        {
            removeBuffer(renderer, pReadbackBuffer);
        }
    });

    ShaderSrcDesc graphicsShaderDesc = makeShaderSourceDesc(SHADER_STAGE_VERT | SHADER_STAGE_FRAG, kGraphicsShader, "VSMain", "PSMain");
    addShaderSource(harness.pRenderer, &graphicsShaderDesc, &pOut->pShader);
    if (!pOut->pShader)
    {
        ADD_FAILURE() << "addShaderSource returned null for graphics shader";
        return false;
    }
    Shader* pGraphicsShader = pOut->pShader;
    cleanup.add([renderer = harness.pRenderer, pGraphicsShader]
    {
        if (pGraphicsShader)
        {
            removeShader(renderer, pGraphicsShader);
        }
    });

    Shader* graphicsShaders[] = { pOut->pShader };
    RootSignatureDesc graphicsRootSignatureDesc = {
        .ppShaders = graphicsShaders,
        .mShaderCount = 1,
    };
    addRootSignature(harness.pRenderer, &graphicsRootSignatureDesc, &pOut->pRootSignature);
    if (!pOut->pRootSignature)
    {
        ADD_FAILURE() << "addRootSignature returned null for graphics pipeline";
        return false;
    }
    RootSignature* pGraphicsRootSignature = pOut->pRootSignature;
    cleanup.add([renderer = harness.pRenderer, pGraphicsRootSignature]
    {
        if (pGraphicsRootSignature)
        {
            removeRootSignature(renderer, pGraphicsRootSignature);
        }
    });

    pOut->rootConstantIndex = getDescriptorIndexFromName(pOut->pRootSignature, "RootConstantColor");
    if (pOut->rootConstantIndex == UINT32_MAX)
    {
        ADD_FAILURE() << "RootConstantColor binding was not reflected";
        return false;
    }

    BufferDesc vertexBufferDesc = {
        .mSize = sizeof(float) * 2 * 3,
        .pName = "GraphicsVertexBuffer",
        .mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU,
        .mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT,
        .mStartState = RESOURCE_STATE_GENERIC_READ,
        .mDescriptors = DESCRIPTOR_TYPE_VERTEX_BUFFER,
    };
    addBuffer(harness.pRenderer, &vertexBufferDesc, &pOut->pVertexBuffer);
    if (!pOut->pVertexBuffer)
    {
        ADD_FAILURE() << "addBuffer returned null for vertex buffer";
        return false;
    }
    Buffer* pVertexBuffer = pOut->pVertexBuffer;
    cleanup.add([renderer = harness.pRenderer, pVertexBuffer]
    {
        if (pVertexBuffer)
        {
            removeBuffer(renderer, pVertexBuffer);
        }
    });

    const float fullscreenTriangle[3][2] = { { -1.0f, -1.0f }, { -1.0f, 3.0f }, { 3.0f, -1.0f } };
    memcpy(pOut->pVertexBuffer->pCpuMappedAddress, fullscreenTriangle, sizeof(fullscreenTriangle));

    BufferDesc indexBufferDesc = {
        .mSize = sizeof(uint16_t) * 3,
        .pName = "GraphicsIndexBuffer",
        .mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU,
        .mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT,
        .mStartState = RESOURCE_STATE_GENERIC_READ,
        .mDescriptors = DESCRIPTOR_TYPE_INDEX_BUFFER,
    };
    addBuffer(harness.pRenderer, &indexBufferDesc, &pOut->pIndexBuffer);
    if (!pOut->pIndexBuffer)
    {
        ADD_FAILURE() << "addBuffer returned null for index buffer";
        return false;
    }
    Buffer* pIndexBuffer = pOut->pIndexBuffer;
    cleanup.add([renderer = harness.pRenderer, pIndexBuffer]
    {
        if (pIndexBuffer)
        {
            removeBuffer(renderer, pIndexBuffer);
        }
    });

    const uint16_t triangleIndices[3] = { 0u, 1u, 2u };
    memcpy(pOut->pIndexBuffer->pCpuMappedAddress, triangleIndices, sizeof(triangleIndices));

    VertexLayout vertexLayout = {
        .mBindings = {
            {
                .mStride = sizeof(float) * 2,
                .mRate = VERTEX_BINDING_RATE_VERTEX,
            },
        },
        .mAttribs = {
            {
                .mSemantic = SEMANTIC_POSITION,
                .mFormat = TinyImageFormat_R32G32_SFLOAT,
                .mBinding = 0,
                .mLocation = 0,
            },
        },
        .mBindingCount = 1,
        .mAttribCount = 1,
    };

    RasterizerStateDesc graphicsRasterizerDesc = {
        .mCullMode = CULL_MODE_NONE,
    };

    PipelineDesc graphicsPipelineDesc = {
        .mGraphicsDesc = {
            .pShaderProgram = pOut->pShader,
            .pRootSignature = pOut->pRootSignature,
            .pVertexLayout = &vertexLayout,
            .pRasterizerState = &graphicsRasterizerDesc,
            .pColorFormats = &pOut->pRenderTarget->mFormat,
            .mRenderTargetCount = 1,
            .mSampleCount = SAMPLE_COUNT_1,
            .mSampleQuality = 0,
            .mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST,
        },
        .pName = "GraphicsPipeline",
        .mType = PIPELINE_TYPE_GRAPHICS,
    };
    addPipeline(harness.pRenderer, &graphicsPipelineDesc, &pOut->pPipeline);
    if (!pOut->pPipeline)
    {
        ADD_FAILURE() << "addPipeline returned null for graphics pipeline";
        return false;
    }
    Pipeline* pGraphicsPipeline = pOut->pPipeline;
    cleanup.add([renderer = harness.pRenderer, pGraphicsPipeline]
    {
        if (pGraphicsPipeline)
        {
            removePipeline(renderer, pGraphicsPipeline);
        }
    });

    return true;
}

PixelValue executeDrawAndReadPixel(LiveRendererHarness& harness, const GraphicsDrawSetup& setup, DrawCallKind kind, const float* color)
{
    resetCmdPool(harness.pRenderer, harness.pCmdPool);
    beginCmd(harness.pCmd);

    BindRenderTargetsDesc bindDesc = {
        .mRenderTargetCount = 1,
        .mRenderTargets = {
            {
                .pRenderTarget = setup.pRenderTarget,
                .mLoadAction = LOAD_ACTION_CLEAR,
                .mStoreAction = STORE_ACTION_STORE,
                .mClearValue = { .a = 1.0f },
                .mOverrideClearValue = true,
            },
        },
    };
    cmdBindRenderTargets(harness.pCmd, &bindDesc);
    cmdSetViewport(harness.pCmd, 0.0f, 0.0f, (float)kRenderTargetWidth, (float)kRenderTargetHeight, 0.0f, 1.0f);
    cmdSetScissor(harness.pCmd, 0, 0, kRenderTargetWidth, kRenderTargetHeight);
    cmdSetStencilReferenceValue(harness.pCmd, 7);
    cmdBindPipeline(harness.pCmd, setup.pPipeline);

    Buffer*  vertexBuffers[] = { setup.pVertexBuffer };
    uint32_t vertexStrides[] = { sizeof(float) * 2 };
    uint64_t vertexOffsets[] = { 0 };
    cmdBindVertexBuffer(harness.pCmd, 1, vertexBuffers, vertexStrides, vertexOffsets);
    cmdBindPushConstants(harness.pCmd, setup.pRootSignature, setup.rootConstantIndex, color);

    switch (kind)
    {
    case DrawCallKind::Draw:
        cmdDraw(harness.pCmd, 3, 0);
        break;
    case DrawCallKind::DrawInstanced:
        cmdDrawInstanced(harness.pCmd, 3, 0, 1, 0);
        break;
    case DrawCallKind::DrawIndexed:
        cmdBindIndexBuffer(harness.pCmd, setup.pIndexBuffer, INDEX_TYPE_UINT16, 0);
        cmdDrawIndexed(harness.pCmd, 3, 0, 0);
        break;
    case DrawCallKind::DrawIndexedInstanced:
        cmdBindIndexBuffer(harness.pCmd, setup.pIndexBuffer, INDEX_TYPE_UINT16, 0);
        cmdDrawIndexedInstanced(harness.pCmd, 3, 0, 1, 0, 0);
        break;
    }

    RenderTargetBarrier renderTargetBarrier = {
        .pRenderTarget = setup.pRenderTarget,
        .mCurrentState = RESOURCE_STATE_RENDER_TARGET,
        .mNewState = RESOURCE_STATE_COPY_SOURCE,
    };
    cmdResourceBarrier(harness.pCmd, 0, nullptr, 0, nullptr, 1, &renderTargetBarrier);

    SubresourceDataDesc subresourceCopy = {};
    cmdCopySubresource(harness.pCmd, setup.pReadbackBuffer, setup.pRenderTarget->pTexture, &subresourceCopy);
    endCmd(harness.pCmd);

    harness.submitAndWait(harness.pCmd, harness.pFence);
    waitQueueIdle(harness.pQueue);

    ReadRange textureReadRange = {
        .mOffset = 0,
        .mSize = setup.totalTextureBytes,
    };
    mapBuffer(harness.pRenderer, setup.pReadbackBuffer, &textureReadRange);
    const uint8_t* textureBytes = static_cast<const uint8_t*>(setup.pReadbackBuffer->pCpuMappedAddress);
    PixelValue result = {};
    if (textureBytes)
    {
        const uint8_t* pixel = getTexturePixel(textureBytes, setup.footprint.Footprint.RowPitch, 1, 1);
        result.r = pixel[0];
        result.g = pixel[1];
        result.b = pixel[2];
        result.a = pixel[3];
    }
    unmapBuffer(harness.pRenderer, setup.pReadbackBuffer);
    return result;
}

bool createComputeCommandSetup(LiveRendererHarness& harness, DeferredCleanup& cleanup, ComputeCommandSetup* pOut)
{
    ShaderSrcDesc computeShaderDesc = makeShaderSourceDesc(SHADER_STAGE_COMP, kCommandComputeShader, "CSMain");
    addShaderSource(harness.pRenderer, &computeShaderDesc, &pOut->pShader);
    if (!pOut->pShader)
    {
        ADD_FAILURE() << "addShaderSource returned null for compute shader";
        return false;
    }
    Shader* pComputeShader = pOut->pShader;
    cleanup.add([renderer = harness.pRenderer, pComputeShader]
    {
        if (pComputeShader)
        {
            removeShader(renderer, pComputeShader);
        }
    });

    Shader* computeShaders[] = { pOut->pShader };
    RootSignatureDesc computeRootSignatureDesc = {
        .ppShaders = computeShaders,
        .mShaderCount = 1,
    };
    addRootSignature(harness.pRenderer, &computeRootSignatureDesc, &pOut->pRootSignature);
    if (!pOut->pRootSignature)
    {
        ADD_FAILURE() << "addRootSignature returned null for compute pipeline";
        return false;
    }
    RootSignature* pComputeRootSignature = pOut->pRootSignature;
    cleanup.add([renderer = harness.pRenderer, pComputeRootSignature]
    {
        if (pComputeRootSignature)
        {
            removeRootSignature(renderer, pComputeRootSignature);
        }
    });

    pOut->outputIndex = getDescriptorIndexFromName(pOut->pRootSignature, "OutputBuffer");
    pOut->rootCbvIndex = getDescriptorIndexFromName(pOut->pRootSignature, "ComputeRootCbv");
    pOut->rootConstantIndex = getDescriptorIndexFromName(pOut->pRootSignature, "RootConstantDispatch");
    if (pOut->outputIndex == UINT32_MAX || pOut->rootCbvIndex == UINT32_MAX || pOut->rootConstantIndex == UINT32_MAX)
    {
        ADD_FAILURE() << "Compute bindings were not fully reflected";
        return false;
    }

    DescriptorSetDesc computeDescriptorSetDesc = {
        .pRootSignature = pOut->pRootSignature,
        .mUpdateFrequency = DESCRIPTOR_UPDATE_FREQ_NONE,
        .mMaxSets = 1,
    };
    addDescriptorSet(harness.pRenderer, &computeDescriptorSetDesc, &pOut->pDescriptorSet);
    if (!pOut->pDescriptorSet)
    {
        ADD_FAILURE() << "addDescriptorSet returned null for compute";
        return false;
    }
    DescriptorSet* pComputeDescriptorSet = pOut->pDescriptorSet;
    cleanup.add([renderer = harness.pRenderer, pComputeDescriptorSet]
    {
        if (pComputeDescriptorSet)
        {
            removeDescriptorSet(renderer, pComputeDescriptorSet);
        }
    });

    BufferDesc computeOutputDesc = {
        .mSize = sizeof(uint32_t) * 2,
        .mElementCount = 2,
        .mStructStride = sizeof(uint32_t),
        .pName = "ComputeOutputBuffer",
        .mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY,
        .mStartState = RESOURCE_STATE_UNORDERED_ACCESS,
        .mDescriptors = DESCRIPTOR_TYPE_RW_BUFFER,
    };
    addBuffer(harness.pRenderer, &computeOutputDesc, &pOut->pOutputBuffer);
    if (!pOut->pOutputBuffer)
    {
        ADD_FAILURE() << "addBuffer returned null for compute output";
        return false;
    }
    Buffer* pOutputBuffer = pOut->pOutputBuffer;
    cleanup.add([renderer = harness.pRenderer, pOutputBuffer]
    {
        if (pOutputBuffer)
        {
            removeBuffer(renderer, pOutputBuffer);
        }
    });

    DescriptorData computeOutputUpdate = {
        .pName = "OutputBuffer",
        .mCount = 1,
        .ppBuffers = &pOut->pOutputBuffer,
    };
    updateDescriptorSet(harness.pRenderer, 0, pOut->pDescriptorSet, 1, &computeOutputUpdate);

    BufferDesc computeRootCbvDesc = {
        .mSize = 256,
        .pName = "ComputeRootCbvBuffer",
        .mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU,
        .mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT,
        .mStartState = RESOURCE_STATE_GENERIC_READ,
        .mDescriptors = DESCRIPTOR_TYPE_UNIFORM_BUFFER,
    };
    addBuffer(harness.pRenderer, &computeRootCbvDesc, &pOut->pRootCbvBuffer);
    if (!pOut->pRootCbvBuffer)
    {
        ADD_FAILURE() << "addBuffer returned null for compute root CBV";
        return false;
    }
    Buffer* pRootCbvBuffer = pOut->pRootCbvBuffer;
    cleanup.add([renderer = harness.pRenderer, pRootCbvBuffer]
    {
        if (pRootCbvBuffer)
        {
            removeBuffer(renderer, pRootCbvBuffer);
        }
    });

    pOut->rootCbvData.multiplier = 3u;
    memcpy(pOut->pRootCbvBuffer->pCpuMappedAddress, &pOut->rootCbvData, sizeof(pOut->rootCbvData));

    BufferDesc computeReadbackDesc = {
        .mSize = sizeof(uint32_t) * 2,
        .pName = "ComputeReadbackBuffer",
        .mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_TO_CPU,
        .mStartState = RESOURCE_STATE_COPY_DEST,
    };
    addBuffer(harness.pRenderer, &computeReadbackDesc, &pOut->pReadbackBuffer);
    if (!pOut->pReadbackBuffer)
    {
        ADD_FAILURE() << "addBuffer returned null for compute readback";
        return false;
    }
    Buffer* pComputeReadbackBuffer = pOut->pReadbackBuffer;
    cleanup.add([renderer = harness.pRenderer, pComputeReadbackBuffer]
    {
        if (pComputeReadbackBuffer)
        {
            removeBuffer(renderer, pComputeReadbackBuffer);
        }
    });

    BufferDesc indirectBufferDesc = {
        .mSize = sizeof(DispatchCommandData),
        .pName = "DispatchIndirectBuffer",
        .mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU,
        .mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT,
        .mStartState = RESOURCE_STATE_GENERIC_READ,
        .mDescriptors = DESCRIPTOR_TYPE_INDIRECT_BUFFER,
    };
    addBuffer(harness.pRenderer, &indirectBufferDesc, &pOut->pIndirectBuffer);
    if (!pOut->pIndirectBuffer)
    {
        ADD_FAILURE() << "addBuffer returned null for indirect dispatch";
        return false;
    }
    Buffer* pIndirectBuffer = pOut->pIndirectBuffer;
    cleanup.add([renderer = harness.pRenderer, pIndirectBuffer]
    {
        if (pIndirectBuffer)
        {
            removeBuffer(renderer, pIndirectBuffer);
        }
    });

    DispatchCommandData dispatchData = {
        .args = {
            .mGroupCountX = 1,
            .mGroupCountY = 1,
            .mGroupCountZ = 1,
        },
    };
    memcpy(pOut->pIndirectBuffer->pCpuMappedAddress, &dispatchData, sizeof(dispatchData));

    PipelineDesc computePipelineDesc = {
        .mComputeDesc = {
            .pShaderProgram = pOut->pShader,
            .pRootSignature = pOut->pRootSignature,
        },
        .pName = "ComputePipeline",
        .mType = PIPELINE_TYPE_COMPUTE,
    };
    addPipeline(harness.pRenderer, &computePipelineDesc, &pOut->pPipeline);
    if (!pOut->pPipeline)
    {
        ADD_FAILURE() << "addPipeline returned null for compute";
        return false;
    }
    Pipeline* pComputePipeline = pOut->pPipeline;
    cleanup.add([renderer = harness.pRenderer, pComputePipeline]
    {
        if (pComputePipeline)
        {
            removePipeline(renderer, pComputePipeline);
        }
    });

    IndirectArgumentDescriptor dispatchArg = {
        .mType = INDIRECT_DISPATCH,
    };
    CommandSignatureDesc dispatchSignatureDesc = {
        .pArgDescs = &dispatchArg,
        .mIndirectArgCount = 1,
    };
    addIndirectCommandSignature(harness.pRenderer, &dispatchSignatureDesc, &pOut->pDispatchSignature);
    if (!pOut->pDispatchSignature)
    {
        ADD_FAILURE() << "addIndirectCommandSignature returned null";
        return false;
    }
    CommandSignature* pDispatchSignature = pOut->pDispatchSignature;
    cleanup.add([renderer = harness.pRenderer, pDispatchSignature]
    {
        if (pDispatchSignature)
        {
            removeIndirectCommandSignature(renderer, pDispatchSignature);
        }
    });

    return true;
}

void setComputeMultiplier(ComputeCommandSetup& setup, uint32_t multiplier)
{
    setup.rootCbvData.multiplier = multiplier;
    memcpy(setup.pRootCbvBuffer->pCpuMappedAddress, &setup.rootCbvData, sizeof(setup.rootCbvData));
}

void executeComputeAndReadBack(LiveRendererHarness& harness, const ComputeCommandSetup& setup, bool bindRootCbv, bool useIndirectDispatch,
                               uint32_t writeValue, uint32_t outputIndex, uint32_t* pOutValues)
{
    resetCmdPool(harness.pRenderer, harness.pCmdPool);
    beginCmd(harness.pCmd);
    cmdBindPipeline(harness.pCmd, setup.pPipeline);
    cmdBindDescriptorSet(harness.pCmd, 0, setup.pDescriptorSet);

    if (bindRootCbv)
    {
        DescriptorDataRange computeRootCbvRange = {
            .mOffset = 0,
            .mSize = (uint32_t)sizeof(setup.rootCbvData),
        };
        DescriptorData computeRootCbvBinding = {
            .pName = "ComputeRootCbv",
            .mCount = 1,
            .pRanges = &computeRootCbvRange,
            .ppBuffers = const_cast<Buffer**>(&setup.pRootCbvBuffer),
        };
        cmdBindDescriptorSetWithRootCbvs(harness.pCmd, 0, setup.pDescriptorSet, 1, &computeRootCbvBinding);
    }

    uint32_t dispatchConstants[4] = { writeValue, outputIndex, 0u, 0u };
    cmdBindPushConstants(harness.pCmd, setup.pRootSignature, setup.rootConstantIndex, dispatchConstants);
    if (useIndirectDispatch)
    {
        cmdExecuteIndirect(harness.pCmd, setup.pDispatchSignature, 1, setup.pIndirectBuffer, 0, nullptr, 0);
    }
    else
    {
        cmdDispatch(harness.pCmd, 1, 1, 1);
    }

    BufferBarrier computeBarrier = {
        .pBuffer = setup.pOutputBuffer,
        .mCurrentState = RESOURCE_STATE_UNORDERED_ACCESS,
        .mNewState = RESOURCE_STATE_COPY_SOURCE,
    };
    cmdResourceBarrier(harness.pCmd, 1, &computeBarrier, 0, nullptr, 0, nullptr);
    cmdUpdateBuffer(harness.pCmd, setup.pReadbackBuffer, 0, setup.pOutputBuffer, 0, sizeof(uint32_t) * 2);
    endCmd(harness.pCmd);

    harness.submitAndWait(harness.pCmd, harness.pFence);
    waitQueueIdle(harness.pQueue);

    ReadRange computeReadRange = {
        .mOffset = 0,
        .mSize = sizeof(uint32_t) * 2,
    };
    mapBuffer(harness.pRenderer, setup.pReadbackBuffer, &computeReadRange);
    const uint32_t* computeValues = static_cast<const uint32_t*>(setup.pReadbackBuffer->pCpuMappedAddress);
    pOutValues[0] = computeValues ? computeValues[0] : 0u;
    pOutValues[1] = computeValues ? computeValues[1] : 0u;
    unmapBuffer(harness.pRenderer, setup.pReadbackBuffer);
}

bool createQueryPool(LiveRendererHarness& harness, DeferredCleanup& cleanup, QueryPool** ppQueryPool)
{
    QueryPoolDesc queryPoolDesc = {
        .pName = "CommandTimestampQuery",
        .mType = QUERY_TYPE_TIMESTAMP,
        .mQueryCount = 1,
    };
    addQueryPool(harness.pRenderer, &queryPoolDesc, ppQueryPool);
    if (!*ppQueryPool)
    {
        ADD_FAILURE() << "addQueryPool returned null";
        return false;
    }
    QueryPool* pQueryPool = *ppQueryPool;
    cleanup.add([renderer = harness.pRenderer, pQueryPool]
    {
        if (pQueryPool)
        {
            removeQueryPool(renderer, pQueryPool);
        }
    });
    return true;
}

bool createMarkerBuffer(LiveRendererHarness& harness, DeferredCleanup& cleanup, Buffer** ppMarkerBuffer)
{
    BufferDesc markerBufferDesc = {
        .mSize = sizeof(uint32_t),
        .pName = "GpuMarkerBuffer",
        .mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_TO_CPU,
        .mFlags = BUFFER_CREATION_FLAG_MARKER,
        .mStartState = RESOURCE_STATE_COPY_DEST,
    };
    addBuffer(harness.pRenderer, &markerBufferDesc, ppMarkerBuffer);
    if (!*ppMarkerBuffer)
    {
        ADD_FAILURE() << "addBuffer returned null for marker buffer";
        return false;
    }
    Buffer* pMarkerBuffer = *ppMarkerBuffer;
    cleanup.add([renderer = harness.pRenderer, pMarkerBuffer]
    {
        if (pMarkerBuffer)
        {
            removeBuffer(renderer, pMarkerBuffer);
        }
    });
    return true;
}

// Verifies that initRendererContext/initRenderer create a live D3D12 renderer pair.
TEST_F(RHIIGraphicsApiTest, RendererApisCreateLiveContextAndRenderer)
{
    LiveRendererHarness harness;
    ASSERT_RHI_CALL_OR_SKIP_ON_UNSUPPORTED(attachSharedRenderer(harness));

    ASSERT_NE(harness.pContext, nullptr);
    ASSERT_NE(harness.pRenderer, nullptr);
    EXPECT_GT(harness.pContext->mGpuCount, 0u);
    EXPECT_EQ(harness.pRenderer->mRendererApi, RENDERER_API_D3D12);
}

// Verifies that queue, fence, semaphore, command-pool, and command-buffer APIs submit work and transition fence state as expected.
TEST_F(RHIIGraphicsApiTest, QueueAndSyncApisSubmitAndSignalCorrectly)
{
    LiveRendererHarness harness;
    ASSERT_RHI_CALL_OR_SKIP_ON_UNSUPPORTED(attachSharedRenderer(harness));
    ASSERT_TRUE(harness.addGraphicsQueueOnly());
    ASSERT_TRUE(harness.addSynchronizationPrimitivesOnly());
    ASSERT_TRUE(harness.addCommandPoolOnly());
    ASSERT_TRUE(harness.addPrimaryCmdOnly());
    ASSERT_TRUE(harness.addExtraCmdsOnly(2));

    FenceStatus fenceStatus = FENCE_STATUS_COMPLETE;
    getFenceStatus(harness.pRenderer, harness.pFence, &fenceStatus);
    EXPECT_EQ(fenceStatus, FENCE_STATUS_NOTSUBMITTED);

    resetCmdPool(harness.pRenderer, harness.pCmdPool);
    beginCmd(harness.ppExtraCmds[0]);
    endCmd(harness.ppExtraCmds[0]);

    Semaphore* signalSemaphores[] = { harness.pSemaphore };
    harness.submitAndWait(harness.ppExtraCmds[0], harness.pFence, 0, nullptr, 1, signalSemaphores);
    getFenceStatus(harness.pRenderer, harness.pFence, &fenceStatus);
    EXPECT_EQ(fenceStatus, FENCE_STATUS_COMPLETE);

    Fence* pWaitFence = nullptr;
    addFence(harness.pRenderer, &pWaitFence);
    ASSERT_NE(pWaitFence, nullptr);

    DeferredCleanup cleanup;
    cleanup.add([&]
    {
        if (pWaitFence)
        {
            removeFence(harness.pRenderer, pWaitFence);
            pWaitFence = nullptr;
        }
    });

    getFenceStatus(harness.pRenderer, pWaitFence, &fenceStatus);
    EXPECT_EQ(fenceStatus, FENCE_STATUS_NOTSUBMITTED);

    resetCmdPool(harness.pRenderer, harness.pCmdPool);
    beginCmd(harness.ppExtraCmds[1]);
    endCmd(harness.ppExtraCmds[1]);
    Semaphore* waitSemaphores[] = { harness.pSemaphore };
    harness.submitAndWait(harness.ppExtraCmds[1], pWaitFence, 1, waitSemaphores);
    getFenceStatus(harness.pRenderer, pWaitFence, &fenceStatus);
    EXPECT_EQ(fenceStatus, FENCE_STATUS_COMPLETE);
}

// Verifies that swapchain APIs acquire images, present them, and toggle vsync on a live window.
TEST_F(RHIIGraphicsApiTest, SwapChainApisAcquirePresentAndToggleVsync)
{
    LiveRendererHarness harness;
    ASSERT_RHI_CALL_OR_SKIP_ON_UNSUPPORTED(initSharedRendererHarness(harness, 0));
    if (!harness.createWindowAndSwapChain("RHIIGraphicsSwapchainWindow", kSwapChainWidth, kSwapChainHeight))
    {
        if (harness.mWindowCreationUnavailable)
        {
            GTEST_SKIP() << "Skipping because window creation is unavailable in this environment";
        }
        FAIL() << "createWindowAndSwapChain failed";
    }

    EXPECT_EQ(harness.pSwapChain->mImageCount, getRecommendedSwapchainImageCount(harness.pRenderer, &harness.mWindow.handle));
    EXPECT_EQ(harness.pSwapChain->mFormat, getSupportedSwapchainFormat(harness.pRenderer, nullptr, harness.pSwapChain->mColorSpace));

    uint32_t imageIndex = UINT32_MAX;
    acquireNextImage(harness.pRenderer, harness.pSwapChain, harness.pSemaphore, harness.pFence, &imageIndex);
    EXPECT_LT(imageIndex, harness.pSwapChain->mImageCount);

    QueuePresentDesc presentDesc = {
        .pSwapChain = harness.pSwapChain,
        .mIndex = (uint8_t)imageIndex,
    };
    queuePresent(harness.pQueue, &presentDesc);
    waitQueueIdle(harness.pQueue);

    const bool initialVsync = harness.pSwapChain->mEnableVsync != 0;
    SwapChain* pSwapChain = harness.pSwapChain;
    toggleVSync(harness.pRenderer, &pSwapChain);
    EXPECT_EQ(pSwapChain, harness.pSwapChain);
    EXPECT_NE(harness.pSwapChain->mEnableVsync != 0, initialVsync);
}

// Verifies that addSampler/removeSampler preserve the requested D3D12 address modes.
TEST_F(RHIIGraphicsApiTest, SamplerApiCreatesExpectedDescriptor)
{
    LiveRendererHarness harness;
    ASSERT_RHI_CALL_OR_SKIP_ON_UNSUPPORTED(attachSharedRenderer(harness));

    Sampler* pSampler = nullptr;
    SamplerDesc samplerDesc = {
        .mMinFilter = FILTER_LINEAR,
        .mMagFilter = FILTER_LINEAR,
        .mMipMapMode = MIPMAP_MODE_LINEAR,
        .mAddressU = ADDRESS_MODE_CLAMP_TO_EDGE,
        .mAddressV = ADDRESS_MODE_CLAMP_TO_EDGE,
        .mAddressW = ADDRESS_MODE_REPEAT,
        .mMaxAnisotropy = 1.0f,
        .mCompareFunc = CMP_NEVER,
    };
    addSampler(harness.pRenderer, &samplerDesc, &pSampler);
    ASSERT_NE(pSampler, nullptr);

    DeferredCleanup cleanup;
    cleanup.add([&]
    {
        if (pSampler)
        {
            removeSampler(harness.pRenderer, pSampler);
            pSampler = nullptr;
        }
    });

    EXPECT_EQ(pSampler->mDx.mDesc.AddressU, D3D12_TEXTURE_ADDRESS_MODE_CLAMP);
    EXPECT_EQ(pSampler->mDx.mDesc.AddressV, D3D12_TEXTURE_ADDRESS_MODE_CLAMP);
    EXPECT_EQ(pSampler->mDx.mDesc.AddressW, D3D12_TEXTURE_ADDRESS_MODE_WRAP);
}

// Verifies that shader APIs create valid source and binary compute shaders from the same compiled code.
TEST_F(RHIIGraphicsApiTest, ShaderApisCreateSourceAndBinaryShaders)
{
    LiveRendererHarness harness;
    ASSERT_RHI_CALL_OR_SKIP_ON_UNSUPPORTED(attachSharedRenderer(harness));

    DeferredCleanup cleanup;
    LifecycleShaderBundle shaderBundle = {};
    ASSERT_TRUE(createLifecycleShaderBundle(harness, cleanup, &shaderBundle));

    ASSERT_NE(shaderBundle.pSourceShader->pReflection, nullptr);
    ASSERT_NE(shaderBundle.pBinaryShader->pReflection, nullptr);
    EXPECT_EQ(shaderBundle.pSourceShader->mStages, SHADER_STAGE_COMP);
    EXPECT_EQ(shaderBundle.pBinaryShader->mStages, SHADER_STAGE_COMP);
}

// Verifies that root-signature and descriptor-set APIs expose reflected bindings and accept descriptor updates for them.
TEST_F(RHIIGraphicsApiTest, RootSignatureAndDescriptorSetApisExposeAndUpdateBindings)
{
    LiveRendererHarness harness;
    ASSERT_RHI_CALL_OR_SKIP_ON_UNSUPPORTED(attachSharedRenderer(harness));

    DeferredCleanup cleanup;
    LifecycleShaderBundle shaderBundle = {};
    ASSERT_TRUE(createLifecycleShaderBundle(harness, cleanup, &shaderBundle));

    LifecyclePlacedBufferBundle placedBufferBundle = {};
    ASSERT_TRUE(createLifecyclePlacedBuffer(harness, cleanup, &placedBufferBundle));

    DescriptorSet* pDescriptorSet = nullptr;
    ASSERT_TRUE(createDescriptorSetForOutput(harness, cleanup, shaderBundle.pRootSignature, placedBufferBundle.pBuffer, &pDescriptorSet));

    EXPECT_EQ((DescriptorType)shaderBundle.pRootSignature->pDescriptors[shaderBundle.outputIndex].mType, DESCRIPTOR_TYPE_RW_BUFFER);
    EXPECT_EQ(shaderBundle.pRootSignature->mDescriptorCount, 1u);
    ASSERT_NE(pDescriptorSet, nullptr);
}

// Verifies that getBufferSizeAlign/addResourceHeap/addBuffer create a valid placed buffer allocation.
TEST_F(RHIIGraphicsApiTest, ResourceHeapAndPlacedBufferApisCreateExpectedAllocation)
{
    LiveRendererHarness harness;
    ASSERT_RHI_CALL_OR_SKIP_ON_UNSUPPORTED(attachSharedRenderer(harness));

    DeferredCleanup cleanup;
    LifecyclePlacedBufferBundle placedBufferBundle = {};
    ASSERT_TRUE(createLifecyclePlacedBuffer(harness, cleanup, &placedBufferBundle));

    ASSERT_NE(placedBufferBundle.pHeap, nullptr);
    ASSERT_NE(placedBufferBundle.pBuffer, nullptr);
    EXPECT_GT(placedBufferBundle.sizeAlign.mSize, 0u);
    EXPECT_GT(placedBufferBundle.sizeAlign.mAlignment, 0u);
    EXPECT_NE(placedBufferBundle.pHeap->mDx.pHeap, nullptr);
    EXPECT_EQ(placedBufferBundle.pBuffer->mSize, sizeof(uint32_t) * 4);
}

// Verifies that pipeline-cache and compute-pipeline APIs create a usable pipeline and serialize cache data when available.
TEST_F(RHIIGraphicsApiTest, PipelineCacheAndPipelineApisCreateUsableComputePipeline)
{
    LiveRendererHarness harness;
    ASSERT_RHI_CALL_OR_SKIP_ON_UNSUPPORTED(attachSharedRenderer(harness));

    DeferredCleanup cleanup;
    LifecycleShaderBundle shaderBundle = {};
    ASSERT_TRUE(createLifecycleShaderBundle(harness, cleanup, &shaderBundle));

    LifecyclePipelineBundle pipelineBundle = {};
    ASSERT_TRUE(createLifecyclePipelineBundle(harness, cleanup, shaderBundle.pBinaryShader, shaderBundle.pRootSignature, &pipelineBundle));

    size_t serializedPipelineCacheSize = 0;
    getPipelineCacheData(harness.pRenderer, pipelineBundle.pPipelineCache, &serializedPipelineCacheSize, nullptr);
    if (serializedPipelineCacheSize > 0)
    {
        std::vector<uint8_t> pipelineCacheBytes(serializedPipelineCacheSize);
        getPipelineCacheData(harness.pRenderer, pipelineBundle.pPipelineCache, &serializedPipelineCacheSize, pipelineCacheBytes.data());
        EXPECT_FALSE(pipelineCacheBytes.empty());
    }

    ASSERT_NE(pipelineBundle.pPipeline->mDx.pPipelineState, nullptr);
}

// Verifies that render-target and memory-stat APIs create textures, report memory, and accept debug names.
TEST_F(RHIIGraphicsApiTest, RenderTargetAndMemoryStatsApisReportUsage)
{
    LiveRendererHarness harness;
    ASSERT_RHI_CALL_OR_SKIP_ON_UNSUPPORTED(attachSharedRenderer(harness));

    DeferredCleanup cleanup;
    RenderTarget* pRenderTarget = nullptr;
    ASSERT_TRUE(createRenderTarget(harness, cleanup, "LifecycleRenderTarget", 4, 4, &pRenderTarget));

    uint64_t usedBytes = 0;
    uint64_t totalAllocatedBytes = 0;
    calculateMemoryUse(harness.pRenderer, &usedBytes, &totalAllocatedBytes);
    EXPECT_GT(usedBytes, 0u);
    EXPECT_GT(totalAllocatedBytes, 0u);

    char* pStats = nullptr;
    calculateMemoryStats(harness.pRenderer, &pStats);
    EXPECT_NE(pStats, nullptr);
    if (pStats)
    {
        freeMemoryStats(harness.pRenderer, pStats);
    }

    EXPECT_EQ(pRenderTarget->mWidth, 4u);
    EXPECT_EQ(pRenderTarget->mHeight, 4u);
    ASSERT_NE(pRenderTarget->pTexture, nullptr);

#if defined(_WINDOWS)
    setTextureName(harness.pRenderer, pRenderTarget->pTexture, "LifecycleTexture");
    expectD3DObjectName(pRenderTarget->pTexture->mDx.pResource, L"LifecycleTexture");

    setRenderTargetName(harness.pRenderer, pRenderTarget, "LifecycleRenderTarget");
    expectD3DObjectName(pRenderTarget->pTexture->mDx.pResource, L"LifecycleRenderTarget");
#endif
}

// Verifies that cmdDraw renders the expected color into an offscreen target.
TEST_F(RHIIGraphicsApiTest, CmdDrawWritesExpectedPixels)
{
    LiveRendererHarness harness;
    ASSERT_RHI_CALL_OR_SKIP_ON_UNSUPPORTED(initSharedRendererHarness(harness, 0));

    DeferredCleanup cleanup;
    GraphicsDrawSetup drawSetup = {};
    ASSERT_TRUE(createGraphicsDrawSetup(harness, cleanup, &drawSetup));

    const float red[4] = { 1.0f, 0.0f, 0.0f, 1.0f };
    PixelValue pixel = executeDrawAndReadPixel(harness, drawSetup, DrawCallKind::Draw, red);
    expectPixelEq(&pixel.r, 255, 0, 0, 255);
}

// Verifies that cmdDrawInstanced renders the expected color into an offscreen target.
TEST_F(RHIIGraphicsApiTest, CmdDrawInstancedWritesExpectedPixels)
{
    LiveRendererHarness harness;
    ASSERT_RHI_CALL_OR_SKIP_ON_UNSUPPORTED(initSharedRendererHarness(harness, 0));

    DeferredCleanup cleanup;
    GraphicsDrawSetup drawSetup = {};
    ASSERT_TRUE(createGraphicsDrawSetup(harness, cleanup, &drawSetup));

    const float green[4] = { 0.0f, 1.0f, 0.0f, 1.0f };
    PixelValue pixel = executeDrawAndReadPixel(harness, drawSetup, DrawCallKind::DrawInstanced, green);
    expectPixelEq(&pixel.r, 0, 255, 0, 255);
}

// Verifies that cmdDrawIndexed renders the expected color into an offscreen target.
TEST_F(RHIIGraphicsApiTest, CmdDrawIndexedWritesExpectedPixels)
{
    LiveRendererHarness harness;
    ASSERT_RHI_CALL_OR_SKIP_ON_UNSUPPORTED(initSharedRendererHarness(harness, 0));

    DeferredCleanup cleanup;
    GraphicsDrawSetup drawSetup = {};
    ASSERT_TRUE(createGraphicsDrawSetup(harness, cleanup, &drawSetup));

    const float blue[4] = { 0.0f, 0.0f, 1.0f, 1.0f };
    PixelValue pixel = executeDrawAndReadPixel(harness, drawSetup, DrawCallKind::DrawIndexed, blue);
    expectPixelEq(&pixel.r, 0, 0, 255, 255);
}

// Verifies that cmdDrawIndexedInstanced renders the expected color into an offscreen target.
TEST_F(RHIIGraphicsApiTest, CmdDrawIndexedInstancedWritesExpectedPixels)
{
    LiveRendererHarness harness;
    ASSERT_RHI_CALL_OR_SKIP_ON_UNSUPPORTED(initSharedRendererHarness(harness, 0));

    DeferredCleanup cleanup;
    GraphicsDrawSetup drawSetup = {};
    ASSERT_TRUE(createGraphicsDrawSetup(harness, cleanup, &drawSetup));

    const float yellow[4] = { 1.0f, 1.0f, 0.0f, 1.0f };
    PixelValue pixel = executeDrawAndReadPixel(harness, drawSetup, DrawCallKind::DrawIndexedInstanced, yellow);
    expectPixelEq(&pixel.r, 255, 255, 0, 255);
}

// Verifies that cmdBindDescriptorSetWithRootCbvs binds the compute root CBV and affects shader output.
TEST_F(RHIIGraphicsApiTest, CmdBindDescriptorSetWithRootCbvsBindsComputeRootCbv)
{
    LiveRendererHarness harness;
    ASSERT_RHI_CALL_OR_SKIP_ON_UNSUPPORTED(initSharedRendererHarness(harness, 0));

    DeferredCleanup cleanup;
    ComputeCommandSetup computeSetup = {};
    ASSERT_TRUE(createComputeCommandSetup(harness, cleanup, &computeSetup));

    setComputeMultiplier(computeSetup, 7u);
    uint32_t values[2] = {};
    executeComputeAndReadBack(harness, computeSetup, true, false, 5u, 0u, values);
    EXPECT_EQ(values[0], 35u);
}

// Verifies that cmdDispatch writes the expected value into the targeted UAV slot.
TEST_F(RHIIGraphicsApiTest, CmdDispatchWritesExpectedBufferValues)
{
    LiveRendererHarness harness;
    ASSERT_RHI_CALL_OR_SKIP_ON_UNSUPPORTED(initSharedRendererHarness(harness, 0));

    DeferredCleanup cleanup;
    ComputeCommandSetup computeSetup = {};
    ASSERT_TRUE(createComputeCommandSetup(harness, cleanup, &computeSetup));

    uint32_t values[2] = {};
    executeComputeAndReadBack(harness, computeSetup, true, false, 5u, 0u, values);
    EXPECT_EQ(values[0], 15u);
}

// Verifies that cmdExecuteIndirect dispatches the expected work and writes through the targeted UAV slot.
TEST_F(RHIIGraphicsApiTest, CmdExecuteIndirectDispatchWritesExpectedBufferValues)
{
    LiveRendererHarness harness;
    ASSERT_RHI_CALL_OR_SKIP_ON_UNSUPPORTED(initSharedRendererHarness(harness, 0));

    DeferredCleanup cleanup;
    ComputeCommandSetup computeSetup = {};
    ASSERT_TRUE(createComputeCommandSetup(harness, cleanup, &computeSetup));

    uint32_t values[2] = {};
    executeComputeAndReadBack(harness, computeSetup, true, true, 9u, 1u, values);
    EXPECT_EQ(values[1], 27u);
    EXPECT_EQ(computeSetup.pDispatchSignature->mDrawType, INDIRECT_DISPATCH);
    EXPECT_EQ(computeSetup.pDispatchSignature->mStride, 16u);
}

// Verifies that query APIs report a valid timestamp range for submitted GPU work.
TEST_F(RHIIGraphicsApiTest, QueryApisProduceValidTimestamps)
{
    LiveRendererHarness harness;
    ASSERT_RHI_CALL_OR_SKIP_ON_UNSUPPORTED(initSharedRendererHarness(harness, 0));

    DeferredCleanup cleanup;
    ComputeCommandSetup computeSetup = {};
    ASSERT_TRUE(createComputeCommandSetup(harness, cleanup, &computeSetup));

    QueryPool* pQueryPool = nullptr;
    ASSERT_TRUE(createQueryPool(harness, cleanup, &pQueryPool));

    double timestampFrequency = 0.0;
    getTimestampFrequency(harness.pQueue, &timestampFrequency);
    EXPECT_GT(timestampFrequency, 0.0);

    resetCmdPool(harness.pRenderer, harness.pCmdPool);
    beginCmd(harness.pCmd);

    QueryDesc timestampQuery = {};
    cmdResetQuery(harness.pCmd, pQueryPool, 0, 1);
    cmdBeginQuery(harness.pCmd, pQueryPool, &timestampQuery);
    cmdBeginDebugMarker(harness.pCmd, 0.2f, 0.6f, 1.0f, "ComputeQuery");
    cmdBindPipeline(harness.pCmd, computeSetup.pPipeline);
    cmdBindDescriptorSet(harness.pCmd, 0, computeSetup.pDescriptorSet);

    DescriptorDataRange computeRootCbvRange = {
        .mOffset = 0,
        .mSize = (uint32_t)sizeof(computeSetup.rootCbvData),
    };
    DescriptorData computeRootCbvBinding = {
        .pName = "ComputeRootCbv",
        .mCount = 1,
        .pRanges = &computeRootCbvRange,
        .ppBuffers = &computeSetup.pRootCbvBuffer,
    };
    cmdBindDescriptorSetWithRootCbvs(harness.pCmd, 0, computeSetup.pDescriptorSet, 1, &computeRootCbvBinding);

    uint32_t dispatchConstants[4] = { 5u, 0u, 0u, 0u };
    cmdBindPushConstants(harness.pCmd, computeSetup.pRootSignature, computeSetup.rootConstantIndex, dispatchConstants);
    cmdDispatch(harness.pCmd, 1, 1, 1);
    cmdAddDebugMarker(harness.pCmd, 0.8f, 0.3f, 0.1f, "ResolveQuery");
    cmdEndDebugMarker(harness.pCmd);
    cmdEndQuery(harness.pCmd, pQueryPool, &timestampQuery);
    cmdResolveQuery(harness.pCmd, pQueryPool, 0, 1);
    endCmd(harness.pCmd);

    harness.submitAndWait(harness.pCmd, harness.pFence);
    waitQueueIdle(harness.pQueue);

    QueryData queryData = {};
    getQueryData(harness.pRenderer, pQueryPool, 0, &queryData);
    EXPECT_TRUE(queryData.mValid);
    EXPECT_GE(queryData.mEndTimestamp, queryData.mBeginTimestamp);
}

// Verifies that cmdUpdateBuffer and cmdWriteMarker make GPU results visible in CPU-readable buffers.
TEST_F(RHIIGraphicsApiTest, MarkerAndBufferCopyApisWriteExpectedResults)
{
    LiveRendererHarness harness;
    ASSERT_RHI_CALL_OR_SKIP_ON_UNSUPPORTED(initSharedRendererHarness(harness, 0));

    DeferredCleanup cleanup;
    ComputeCommandSetup computeSetup = {};
    ASSERT_TRUE(createComputeCommandSetup(harness, cleanup, &computeSetup));

    Buffer* pMarkerBuffer = nullptr;
    ASSERT_TRUE(createMarkerBuffer(harness, cleanup, &pMarkerBuffer));

#if defined(_WINDOWS)
    setBufferName(harness.pRenderer, pMarkerBuffer, "GpuMarkerBuffer");
    expectD3DObjectName(pMarkerBuffer->mDx.pResource, L"GpuMarkerBuffer");
#endif

    resetCmdPool(harness.pRenderer, harness.pCmdPool);
    beginCmd(harness.pCmd);
    cmdBindPipeline(harness.pCmd, computeSetup.pPipeline);
    cmdBindDescriptorSet(harness.pCmd, 0, computeSetup.pDescriptorSet);

    DescriptorDataRange computeRootCbvRange = {
        .mOffset = 0,
        .mSize = (uint32_t)sizeof(computeSetup.rootCbvData),
    };
    DescriptorData computeRootCbvBinding = {
        .pName = "ComputeRootCbv",
        .mCount = 1,
        .pRanges = &computeRootCbvRange,
        .ppBuffers = &computeSetup.pRootCbvBuffer,
    };
    cmdBindDescriptorSetWithRootCbvs(harness.pCmd, 0, computeSetup.pDescriptorSet, 1, &computeRootCbvBinding);

    uint32_t dispatchConstants[4] = { 5u, 0u, 0u, 0u };
    cmdBindPushConstants(harness.pCmd, computeSetup.pRootSignature, computeSetup.rootConstantIndex, dispatchConstants);
    cmdDispatch(harness.pCmd, 1, 1, 1);

    BufferBarrier computeBarrier = {
        .pBuffer = computeSetup.pOutputBuffer,
        .mCurrentState = RESOURCE_STATE_UNORDERED_ACCESS,
        .mNewState = RESOURCE_STATE_COPY_SOURCE,
    };
    cmdResourceBarrier(harness.pCmd, 1, &computeBarrier, 0, nullptr, 0, nullptr);
    cmdUpdateBuffer(harness.pCmd, computeSetup.pReadbackBuffer, 0, computeSetup.pOutputBuffer, 0, sizeof(uint32_t) * 2);

    MarkerDesc markerDesc = {
        .pBuffer = pMarkerBuffer,
        .mOffset = 0,
        .mValue = 0xCAFEBABEu,
        .mFlags = MARKER_FLAG_WAIT_FOR_WRITE,
    };
    cmdWriteMarker(harness.pCmd, &markerDesc);
    endCmd(harness.pCmd);

    harness.submitAndWait(harness.pCmd, harness.pFence);
    waitQueueIdle(harness.pQueue);

    ReadRange computeReadRange = {
        .mOffset = 0,
        .mSize = sizeof(uint32_t) * 2,
    };
    mapBuffer(harness.pRenderer, computeSetup.pReadbackBuffer, &computeReadRange);
    const uint32_t* computeValues = static_cast<const uint32_t*>(computeSetup.pReadbackBuffer->pCpuMappedAddress);
    ASSERT_NE(computeValues, nullptr);
    EXPECT_EQ(computeValues[0], 15u);
    unmapBuffer(harness.pRenderer, computeSetup.pReadbackBuffer);

    ReadRange markerReadRange = {
        .mOffset = 0,
        .mSize = sizeof(uint32_t),
    };
    mapBuffer(harness.pRenderer, pMarkerBuffer, &markerReadRange);
    const uint32_t* markerValue = static_cast<const uint32_t*>(pMarkerBuffer->pCpuMappedAddress);
    ASSERT_NE(markerValue, nullptr);
    EXPECT_EQ(markerValue[0], 0xCAFEBABEu);
    unmapBuffer(harness.pRenderer, pMarkerBuffer);
}
