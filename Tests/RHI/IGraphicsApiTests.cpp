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
void FORGE_CALLCONV cmdUpdateBuffer(Cmd* pCmd, Buffer* pBuffer, uint64_t dstOffset, Buffer* pSrcBuffer, uint64_t srcOffset, uint64_t Size);
void FORGE_CALLCONV cmdCopySubresource(Cmd* pCmd, Buffer* pDstBuffer, Texture* pTexture, const SubresourceDataDesc* pSubresourceDesc);

namespace
{
constexpr uint32_t kSwapChainWidth = 64;
constexpr uint32_t kSwapChainHeight = 64;
constexpr uint32_t kRenderTargetWidth = 16;
constexpr uint32_t kRenderTargetHeight = 4;

const char* kLifecycleComputeShader = R"(
cbuffer RootConstantOutput { uint outputHeapIndex; };

[numthreads(1, 1, 1)]
void CSMain(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    RWStructuredBuffer<uint> OutputBuffer = ResourceDescriptorHeap[outputHeapIndex];
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
cbuffer RootConstantDispatch : register(b0)
{
    uint WriteValue;
    uint OutputIndex;
    uint OutputHeapIndex;
    uint UniformHeapIndex;
};
struct ComputeUniforms { uint Multiplier; uint3 padding; };
[numthreads(1, 1, 1)]
void CSMain(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    RWStructuredBuffer<uint> OutputBuffer = ResourceDescriptorHeap[OutputHeapIndex];
    ConstantBuffer<ComputeUniforms> uniforms = ResourceDescriptorHeap[UniformHeapIndex];
    OutputBuffer[OutputIndex] = (WriteValue + dispatchThreadId.x) * uniforms.Multiplier;
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

#define ASSERT_RHI_CALL_OR_SKIP_ON_UNSUPPORTED(expr)                                                                                 \
    do                                                                                                                               \
    {                                                                                                                                \
        if (!(expr))                                                                                                                 \
        {                                                                                                                            \
            const char* reason = "";                                                                                                 \
            if (hasRendererInitializationError(&reason))                                                                             \
            {                                                                                                                        \
                GTEST_SKIP() << "Skipping because renderer initialization is unsupported: " << (reason ? reason : "unknown reason"); \
            }                                                                                                                        \
            FAIL() << #expr << " failed: " << getRendererInitReason();                                                               \
        }                                                                                                                            \
    } while (false)

ShaderSrcDesc makeShaderSourceDesc(ShaderStage stages, const char* source, const char* entry0, const char* entry1 = nullptr)
{
    ShaderSrcDesc desc = {
        .stages = stages,
    };

    if (stages & ShaderStageVert)
    {
        desc.vert.Name = "InlineVert";
        desc.vert.pByteCode = (char*)source;
        desc.vert.byteCodeSize = (uint32_t)strlen(source);
        desc.vert.pEntryPoint = entry0;
    }

    if (stages & ShaderStageFrag)
    {
        desc.frag.Name = "InlineFrag";
        desc.frag.pByteCode = (char*)source;
        desc.frag.byteCodeSize = (uint32_t)strlen(source);
        desc.frag.pEntryPoint = entry1 ? entry1 : entry0;
    }

    if (stages & ShaderStageComp)
    {
        desc.comp.Name = "InlineComp";
        desc.comp.pByteCode = (char*)source;
        desc.comp.byteCodeSize = (uint32_t)strlen(source);
        desc.comp.pEntryPoint = entry0;
    }

    return desc;
}

BinaryShaderDesc makeBinaryShaderDescFromSourceShader(const Shader* pShader, ShaderStage stage, const char* entryPoint)
{
    BinaryShaderDesc desc = {
        .stages = stage,
    };

    if (stage == ShaderStageComp)
    {
        desc.comp.Name = "BinaryComp";
        desc.comp.pByteCode = pShader->dx.pShaderBlobs[0]->GetBufferPointer();
        desc.comp.byteCodeSize = (uint32_t)pShader->dx.pShaderBlobs[0]->GetBufferSize();
        desc.comp.pEntryPoint = entryPoint;
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
            .Type = QueueTypeGraphics,
            .flag = QUEUE_FLAG_NONE,
            .priority = QUEUE_PRIORITY_NORMAL,
            .Name = "IGraphicsApiTest.Queue",
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
            .transient = false,
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
            .Name = "IGraphicsApiTest.PrimaryCmd",
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

    bool createWindowAndSwapChain(const char* appName, uint32_t Width, uint32_t Height)
    {
        mWindowCreationUnavailable = false;
        initWindowClass();
        mWindowClassInitialized = true;

        mWindow = {
            .WindowedRect = { 0, 0, (int)Width, (int)Height },
            .FullscreenRect = { 0, 0, (int)Width, (int)Height },
            .ClientRect = { 0, 0, (int)Width, (int)Height },
            .Hidden = true,
            .NoResizeFrame = true,
            .OverrideDefaultPosition = true,
        };

        OpenWindow(appName, &mWindow);
        mWindowOpened = mWindow.Handle.window != nullptr;
        if (!mWindowOpened)
        {
            mWindowCreationUnavailable = true;
            return false;
        }

        Queue*        presentQueues[] = { pQueue };
        SwapChainDesc swapChainDesc = {
            .windowHandle = mWindow.Handle,
            .ppPresentQueues = presentQueues,
            .presentQueueCount = 1,
            .imageCount = getRecommendedSwapchainImageCount(pRenderer, &mWindow.Handle),
            .Width = Width,
            .Height = Height,
            .enableVsync = false,
            .colorSpace = ColorSpaceSdrSrgb,
        };
        swapChainDesc.colorFormat = getSupportedSwapchainFormat(pRenderer, &swapChainDesc, swapChainDesc.colorSpace);

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
            .cmdCount = 1,
            .waitSemaphoreCount = waitSemaphoreCount,
            .signalSemaphoreCount = signalSemaphoreCount,
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
            ClosePlatformWindow(&mWindow);
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
    UINT    Size = sizeof(buffer);
    if (!pObject || FAILED(pObject->GetPrivateData(WKPDID_D3DDebugObjectNameW, &Size, buffer)) || Size == 0)
    {
        return {};
    }

    size_t length = Size / sizeof(wchar_t);
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

const uint8_t* getTexturePixel(const uint8_t* data, uint32_t rowPitch, uint32_t x, uint32_t y) { return data + rowPitch * y + x * 4; }

void expectPixelEq(const uint8_t* pixel, uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    EXPECT_EQ(pixel[0], r);
    EXPECT_EQ(pixel[1], g);
    EXPECT_EQ(pixel[2], b);
    EXPECT_EQ(pixel[3], a);
}

} // namespace

class RHIIGraphicsApiTest: public ::testing::Test
{
protected:
    static void SetUpTestSuite()
    {
        ASSERT_TRUE(InitMemoryAlloc(nullptr));
        _EnableInteractiveMode(false);

        FileSystemInitDesc FileSystemDesc = {
            .pAppName = "RHITests",
        };
        ASSERT_TRUE(InitFileSystem(&FileSystemDesc));

        SetResourceDirectoryPath(SystemFileIO, MountDebug, DirectoryLog, "");
        SetResourceDirectoryPath(SystemFileIO, MountDebug, DirectoryShaderBinaries, "CompiledShaders");

        InitLog(nullptr, Error);

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

        ExitLog();
        ExitFileSystem();
        _EnableInteractiveMode(true);
        ExitMemoryAlloc();
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

// Verifies that flag operators and indirect argument index helpers behave as expected for the public API surface.
TEST_F(RHIIGraphicsApiTest, HeaderHelpersPreserveExpectedFlagBehavior)
{
    ResourceState combinedState = ResourceStateCopyDest | ResourceStateShaderResource;
    EXPECT_EQ(combinedState & ResourceStateCopyDest, ResourceStateCopyDest);
    EXPECT_EQ(combinedState & ResourceStatePixelShaderResource, ResourceStatePixelShaderResource);
    EXPECT_EQ(combinedState & ResourceStatePresent, ResourceStateUndefined);

    DescriptorType descriptorMask = DescriptorTypeTexture | DescriptorTypeRwTexture;
    EXPECT_EQ(descriptorMask & DescriptorTypeTexture, DescriptorTypeTexture);
    EXPECT_EQ(descriptorMask & DescriptorTypeRwTexture, DescriptorTypeRwTexture);
    EXPECT_EQ(descriptorMask & DescriptorTypeBuffer, DescriptorTypeUndefined);

    QueueFlag queueFlags = QUEUE_FLAG_DISABLE_GPU_TIMEOUT | QUEUE_FLAG_INIT_MICROPROFILE;
    EXPECT_EQ(queueFlags & QUEUE_FLAG_DISABLE_GPU_TIMEOUT, QUEUE_FLAG_DISABLE_GPU_TIMEOUT);
    EXPECT_EQ(queueFlags & QUEUE_FLAG_INIT_MICROPROFILE, QUEUE_FLAG_INIT_MICROPROFILE);
    EXPECT_EQ(queueFlags & QUEUE_FLAG_NONE, QUEUE_FLAG_NONE);

    EXPECT_EQ(INDIRECT_DRAW_ELEM_INDEX(vertexCount), 0u);
    EXPECT_EQ(INDIRECT_DRAW_ELEM_INDEX(instanceCount), 1u);
    EXPECT_EQ(INDIRECT_DRAW_ELEM_INDEX(startVertex), 2u);
    EXPECT_EQ(INDIRECT_DRAW_ELEM_INDEX(startInstance), 3u);

    EXPECT_EQ(INDIRECT_DRAW_INDEX_ELEM_INDEX(indexCount), 0u);
    EXPECT_EQ(INDIRECT_DRAW_INDEX_ELEM_INDEX(instanceCount), 1u);
    EXPECT_EQ(INDIRECT_DRAW_INDEX_ELEM_INDEX(startIndex), 2u);
    EXPECT_EQ(INDIRECT_DRAW_INDEX_ELEM_INDEX(vertexOffset), 3u);
    EXPECT_EQ(INDIRECT_DRAW_INDEX_ELEM_INDEX(startInstance), 4u);

    EXPECT_EQ(INDIRECT_DISPATCH_ELEM_INDEX(groupCountX), 0u);
    EXPECT_EQ(INDIRECT_DISPATCH_ELEM_INDEX(groupCountY), 1u);
    EXPECT_EQ(INDIRECT_DISPATCH_ELEM_INDEX(groupCountZ), 2u);
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
    ResourceHeap*     pHeap = nullptr;
    Buffer*           pBuffer = nullptr;
    ResourceSizeAlign sizeAlign = {};
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
    RenderTarget*                      pRenderTarget = nullptr;
    Buffer*                            pReadbackBuffer = nullptr;
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint = {};
    UINT64                             totalTextureBytes = 0;
    Shader*                            pShader = nullptr;
    RootSignature*                     pRootSignature = nullptr;
    uint32_t                           rootConstantIndex = UINT32_MAX;
    Buffer*                            pVertexBuffer = nullptr;
    Buffer*                            pIndexBuffer = nullptr;
    Pipeline*                          pPipeline = nullptr;
};

struct ComputeCommandSetup
{
    Shader*           pShader = nullptr;
    RootSignature*    pRootSignature = nullptr;
    uint32_t          rootConstantIndex = UINT32_MAX;
    Buffer*           pOutputBuffer = nullptr;
    Buffer*           pRootCbvBuffer = nullptr;
    RootCbvConstants  rootCbvData = {};
    Buffer*           pReadbackBuffer = nullptr;
    Buffer*           pIndirectBuffer = nullptr;
    Pipeline*         pPipeline = nullptr;
    CommandSignature* pDispatchSignature = nullptr;
};

bool createLifecycleShaderBundle(LiveRendererHarness& harness, DeferredCleanup& Cleanup, LifecycleShaderBundle* pOut)
{
    ShaderSrcDesc shaderSourceDesc = makeShaderSourceDesc(ShaderStageComp, kLifecycleComputeShader, "CSMain");
    addShaderSource(harness.pRenderer, &shaderSourceDesc, &pOut->pSourceShader);
    if (!pOut->pSourceShader)
    {
        ADD_FAILURE() << "addShaderSource returned null";
        return false;
    }
    Shader* pSourceShader = pOut->pSourceShader;
    Cleanup.add(
        [renderer = harness.pRenderer, pSourceShader]
        {
            if (pSourceShader)
            {
                removeShader(renderer, pSourceShader);
            }
        });

    BinaryShaderDesc binaryShaderDesc = makeBinaryShaderDescFromSourceShader(pOut->pSourceShader, ShaderStageComp, "CSMain");
    addShaderBinary(harness.pRenderer, &binaryShaderDesc, &pOut->pBinaryShader);
    if (!pOut->pBinaryShader)
    {
        ADD_FAILURE() << "addShaderBinary returned null";
        return false;
    }
    Shader* pBinaryShader = pOut->pBinaryShader;
    Cleanup.add(
        [renderer = harness.pRenderer, pBinaryShader]
        {
            if (pBinaryShader)
            {
                removeShader(renderer, pBinaryShader);
            }
        });

    Shader*           rootShaders[] = { pOut->pBinaryShader };
    RootSignatureDesc rootSignatureDesc = {
        .ppShaders = rootShaders,
        .shaderCount = 1,
    };
    addRootSignature(harness.pRenderer, &rootSignatureDesc, &pOut->pRootSignature);
    if (!pOut->pRootSignature)
    {
        ADD_FAILURE() << "addRootSignature returned null";
        return false;
    }
    RootSignature* pRootSignature = pOut->pRootSignature;
    Cleanup.add(
        [renderer = harness.pRenderer, pRootSignature]
        {
            if (pRootSignature)
            {
                removeRootSignature(renderer, pRootSignature);
            }
        });

    pOut->outputIndex = getDescriptorIndexFromName(pOut->pRootSignature, "RootConstantOutput");
    if (pOut->outputIndex == UINT32_MAX)
    {
        ADD_FAILURE() << "RootConstantOutput was not reflected";
        return false;
    }

    return true;
}

bool createLifecyclePlacedBuffer(LiveRendererHarness& harness, DeferredCleanup& Cleanup, LifecyclePlacedBufferBundle* pOut)
{
    BufferDesc placedBufferDesc = {
        .Size = sizeof(uint32_t) * 4,
        .ElementCount = 4,
        .StructStride = sizeof(uint32_t),
        .Name = "LifecyclePlacedBuffer",
        .MemoryUsage = MemoryUsageGpuOnly,
        .StartState = ResourceStateUnorderedAccess,
        .Descriptors = DescriptorTypeRwBuffer,
    };
    getBufferSizeAlign(harness.pRenderer, &placedBufferDesc, &pOut->sizeAlign);

    ResourceHeapDesc heapDesc = {
        .Size = pOut->sizeAlign.Size,
        .alignment = pOut->sizeAlign.alignment,
        .MemoryUsage = placedBufferDesc.MemoryUsage,
        .Descriptors = placedBufferDesc.Descriptors,
        .Flags = RESOURCE_HEAP_FLAG_ALLOW_ONLY_BUFFERS,
        .Name = "LifecycleBufferHeap",
    };
    addResourceHeap(harness.pRenderer, &heapDesc, &pOut->pHeap);
    if (!pOut->pHeap)
    {
        ADD_FAILURE() << "addResourceHeap returned null";
        return false;
    }
    ResourceHeap* pHeap = pOut->pHeap;
    Cleanup.add(
        [renderer = harness.pRenderer, pHeap]
        {
            if (pHeap)
            {
                removeResourceHeap(renderer, pHeap);
            }
        });

    ResourcePlacement placement = {
        .pHeap = pOut->pHeap,
        .offset = 0,
    };
    placedBufferDesc.Placement = &placement;
    addBuffer(harness.pRenderer, &placedBufferDesc, &pOut->pBuffer);
    if (!pOut->pBuffer)
    {
        ADD_FAILURE() << "addBuffer returned null for placed buffer";
        return false;
    }
    Buffer* pBuffer = pOut->pBuffer;
    Cleanup.add(
        [renderer = harness.pRenderer, pBuffer]
        {
            if (pBuffer)
            {
                removeBuffer(renderer, pBuffer);
            }
        });

    return true;
}

bool createLifecyclePipelineBundle(LiveRendererHarness& harness, DeferredCleanup& Cleanup, Shader* pShader, RootSignature* pRootSignature,
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
    Cleanup.add(
        [renderer = harness.pRenderer, pPipelineCache]
        {
            if (pPipelineCache)
            {
                removePipelineCache(renderer, pPipelineCache);
            }
        });

    PipelineDesc pipelineDesc = {
        .computeDesc = {
            .pShaderProgram = pShader,
            .pRootSignature = pRootSignature,
        },
        .pCache = pOut->pPipelineCache,
        .Name = "LifecycleComputePipeline",
        .Type = PIPELINE_TYPE_COMPUTE,
    };
    addPipeline(harness.pRenderer, &pipelineDesc, &pOut->pPipeline);
    if (!pOut->pPipeline)
    {
        ADD_FAILURE() << "addPipeline returned null";
        return false;
    }
    Pipeline* pPipeline = pOut->pPipeline;
    Cleanup.add(
        [renderer = harness.pRenderer, pPipeline]
        {
            if (pPipeline)
            {
                removePipeline(renderer, pPipeline);
            }
        });

    return true;
}

bool createRenderTarget(LiveRendererHarness& harness, DeferredCleanup& Cleanup, const char* name, uint32_t Width, uint32_t Height,
                        RenderTarget** ppRenderTarget)
{
    RenderTargetDesc renderTargetDesc = {
        .Width = Width,
        .Height = Height,
        .Depth = 1,
        .ArraySize = 1,
        .MipLevels = 1,
        .SampleCount = SAMPLE_COUNT_1,
        .Format = hz::Format::R8G8B8A8_UNORM,
        .StartState = ResourceStateRenderTarget,
        .Name = name,
    };
    addRenderTarget(harness.pRenderer, &renderTargetDesc, ppRenderTarget);
    if (!*ppRenderTarget)
    {
        ADD_FAILURE() << "addRenderTarget returned null";
        return false;
    }
    RenderTarget* pRenderTarget = *ppRenderTarget;
    Cleanup.add(
        [renderer = harness.pRenderer, pRenderTarget]
        {
            if (pRenderTarget)
            {
                removeRenderTarget(renderer, pRenderTarget);
            }
        });
    return true;
}

bool createGraphicsDrawSetup(LiveRendererHarness& harness, DeferredCleanup& Cleanup, GraphicsDrawSetup* pOut)
{
    if (!createRenderTarget(harness, Cleanup, "CommandRenderTarget", kRenderTargetWidth, kRenderTargetHeight, &pOut->pRenderTarget))
    {
        return false;
    }

    D3D12_RESOURCE_DESC textureResourceDesc = pOut->pRenderTarget->pTexture->dx.pResource->GetDesc();
    if (textureResourceDesc.Flags & D3D12_RESOURCE_FLAG_USE_TIGHT_ALIGNMENT)
    {
        textureResourceDesc.Alignment = 0;
    }
    UINT   numRows = 0;
    UINT64 rowSizeInBytes = 0;
    harness.pRenderer->dx.pDevice->GetCopyableFootprints(&textureResourceDesc, 0, 1, 0, &pOut->footprint, &numRows, &rowSizeInBytes,
                                                          &pOut->totalTextureBytes);

    BufferDesc textureReadbackDesc = {
        .Size = pOut->totalTextureBytes,
        .Name = "CommandTextureReadback",
        .MemoryUsage = MemoryUsageGpuToCpu,
        .StartState = ResourceStateCopyDest,
    };
    addBuffer(harness.pRenderer, &textureReadbackDesc, &pOut->pReadbackBuffer);
    if (!pOut->pReadbackBuffer)
    {
        ADD_FAILURE() << "addBuffer returned null for texture readback";
        return false;
    }
    Buffer* pReadbackBuffer = pOut->pReadbackBuffer;
    Cleanup.add(
        [renderer = harness.pRenderer, pReadbackBuffer]
        {
            if (pReadbackBuffer)
            {
                removeBuffer(renderer, pReadbackBuffer);
            }
        });

    ShaderSrcDesc graphicsShaderDesc = makeShaderSourceDesc(ShaderStageVert | ShaderStageFrag, kGraphicsShader, "VSMain", "PSMain");
    addShaderSource(harness.pRenderer, &graphicsShaderDesc, &pOut->pShader);
    if (!pOut->pShader)
    {
        ADD_FAILURE() << "addShaderSource returned null for graphics shader";
        return false;
    }
    Shader* pGraphicsShader = pOut->pShader;
    Cleanup.add(
        [renderer = harness.pRenderer, pGraphicsShader]
        {
            if (pGraphicsShader)
            {
                removeShader(renderer, pGraphicsShader);
            }
        });

    Shader*           graphicsShaders[] = { pOut->pShader };
    RootSignatureDesc graphicsRootSignatureDesc = {
        .ppShaders = graphicsShaders,
        .shaderCount = 1,
    };
    addRootSignature(harness.pRenderer, &graphicsRootSignatureDesc, &pOut->pRootSignature);
    if (!pOut->pRootSignature)
    {
        ADD_FAILURE() << "addRootSignature returned null for graphics pipeline";
        return false;
    }
    RootSignature* pGraphicsRootSignature = pOut->pRootSignature;
    Cleanup.add(
        [renderer = harness.pRenderer, pGraphicsRootSignature]
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
        .Size = sizeof(float) * 2 * 3,
        .Name = "GraphicsVertexBuffer",
        .MemoryUsage = MemoryUsageCpuToGpu,
        .Flags = BufferCreationFlagPersistentMapBit,
        .StartState = ResourceStateGenericRead,
        .Descriptors = DescriptorTypeVertexBuffer,
    };
    addBuffer(harness.pRenderer, &vertexBufferDesc, &pOut->pVertexBuffer);
    if (!pOut->pVertexBuffer)
    {
        ADD_FAILURE() << "addBuffer returned null for vertex buffer";
        return false;
    }
    Buffer* pVertexBuffer = pOut->pVertexBuffer;
    Cleanup.add(
        [renderer = harness.pRenderer, pVertexBuffer]
        {
            if (pVertexBuffer)
            {
                removeBuffer(renderer, pVertexBuffer);
            }
        });

    const float fullscreenTriangle[3][2] = { { -1.0f, -1.0f }, { -1.0f, 3.0f }, { 3.0f, -1.0f } };
    memcpy(pOut->pVertexBuffer->pCpuMappedAddress, fullscreenTriangle, sizeof(fullscreenTriangle));

    BufferDesc indexBufferDesc = {
        .Size = sizeof(uint16_t) * 3,
        .Name = "GraphicsIndexBuffer",
        .MemoryUsage = MemoryUsageCpuToGpu,
        .Flags = BufferCreationFlagPersistentMapBit,
        .StartState = ResourceStateGenericRead,
        .Descriptors = DescriptorTypeIndexBuffer,
    };
    addBuffer(harness.pRenderer, &indexBufferDesc, &pOut->pIndexBuffer);
    if (!pOut->pIndexBuffer)
    {
        ADD_FAILURE() << "addBuffer returned null for index buffer";
        return false;
    }
    Buffer* pIndexBuffer = pOut->pIndexBuffer;
    Cleanup.add(
        [renderer = harness.pRenderer, pIndexBuffer]
        {
            if (pIndexBuffer)
            {
                removeBuffer(renderer, pIndexBuffer);
            }
        });

    const uint16_t triangleIndices[3] = { 0u, 1u, 2u };
    memcpy(pOut->pIndexBuffer->pCpuMappedAddress, triangleIndices, sizeof(triangleIndices));

    VertexLayout vertexLayout = {
        .bindings = {
            {
                .stride = sizeof(float) * 2,
                .rate = VERTEX_BINDING_RATE_VERTEX,
            },
        },
        .attribs = {
            {
                .semantic = SEMANTIC_POSITION,
                .Format = hz::Format::R32G32_SFLOAT,
                .binding = 0,
                .location = 0,
            },
        },
        .bindingCount = 1,
        .attribCount = 1,
    };

    RasterizerStateDesc graphicsRasterizerDesc = {
        .cullMode = CULL_MODE_NONE,
    };

    PipelineDesc graphicsPipelineDesc = {
        .graphicsDesc = {
            .pShaderProgram = pOut->pShader,
            .pRootSignature = pOut->pRootSignature,
            .pVertexLayout = &vertexLayout,
            .pRasterizerState = &graphicsRasterizerDesc,
            .pColorFormats = &pOut->pRenderTarget->Format,
            .renderTargetCount = 1,
            .SampleCount = SAMPLE_COUNT_1,
            .SampleQuality = 0,
            .primitiveTopo = PRIMITIVE_TOPO_TRI_LIST,
        },
        .Name = "GraphicsPipeline",
        .Type = PIPELINE_TYPE_GRAPHICS,
    };
    addPipeline(harness.pRenderer, &graphicsPipelineDesc, &pOut->pPipeline);
    if (!pOut->pPipeline)
    {
        ADD_FAILURE() << "addPipeline returned null for graphics pipeline";
        return false;
    }
    Pipeline* pGraphicsPipeline = pOut->pPipeline;
    Cleanup.add(
        [renderer = harness.pRenderer, pGraphicsPipeline]
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
        .renderTargetCount = 1,
        .renderTargets = {
            {
                .pRenderTarget = setup.pRenderTarget,
                .loadAction = LoadActionClear,
                .storeAction = StoreActionStore,
                .ClearValue = { .a = 1.0f },
                .overrideClearValue = true,
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
        cmdBindIndexBuffer(harness.pCmd, setup.pIndexBuffer, IndexTypeUInt16, 0);
        cmdDrawIndexed(harness.pCmd, 3, 0, 0);
        break;
    case DrawCallKind::DrawIndexedInstanced:
        cmdBindIndexBuffer(harness.pCmd, setup.pIndexBuffer, IndexTypeUInt16, 0);
        cmdDrawIndexedInstanced(harness.pCmd, 3, 0, 1, 0, 0);
        break;
    }

    RenderTargetBarrier renderTargetBarrier = {
        .pRenderTarget = setup.pRenderTarget,
        .currentState = ResourceStateRenderTarget,
        .newState = ResourceStateCopySource,
    };
    cmdResourceBarrier(harness.pCmd, 0, nullptr, 0, nullptr, 1, &renderTargetBarrier);

    SubresourceDataDesc subresourceCopy = {};
    cmdCopySubresource(harness.pCmd, setup.pReadbackBuffer, setup.pRenderTarget->pTexture, &subresourceCopy);
    endCmd(harness.pCmd);

    harness.submitAndWait(harness.pCmd, harness.pFence);
    waitQueueIdle(harness.pQueue);

    ReadRange textureReadRange = {
        .offset = 0,
        .Size = setup.totalTextureBytes,
    };
    mapBuffer(harness.pRenderer, setup.pReadbackBuffer, &textureReadRange);
    const uint8_t* textureBytes = (const uint8_t*)setup.pReadbackBuffer->pCpuMappedAddress;
    PixelValue     result = {};
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

bool createComputeCommandSetup(LiveRendererHarness& harness, DeferredCleanup& Cleanup, ComputeCommandSetup* pOut)
{
    ShaderSrcDesc computeShaderDesc = makeShaderSourceDesc(ShaderStageComp, kCommandComputeShader, "CSMain");
    addShaderSource(harness.pRenderer, &computeShaderDesc, &pOut->pShader);
    if (!pOut->pShader)
    {
        ADD_FAILURE() << "addShaderSource returned null for compute shader";
        return false;
    }
    Shader* pComputeShader = pOut->pShader;
    Cleanup.add(
        [renderer = harness.pRenderer, pComputeShader]
        {
            if (pComputeShader)
            {
                removeShader(renderer, pComputeShader);
            }
        });

    Shader*           computeShaders[] = { pOut->pShader };
    RootSignatureDesc computeRootSignatureDesc = {
        .ppShaders = computeShaders,
        .shaderCount = 1,
    };
    addRootSignature(harness.pRenderer, &computeRootSignatureDesc, &pOut->pRootSignature);
    if (!pOut->pRootSignature)
    {
        ADD_FAILURE() << "addRootSignature returned null for compute pipeline";
        return false;
    }
    RootSignature* pComputeRootSignature = pOut->pRootSignature;
    Cleanup.add(
        [renderer = harness.pRenderer, pComputeRootSignature]
        {
            if (pComputeRootSignature)
            {
                removeRootSignature(renderer, pComputeRootSignature);
            }
        });

    pOut->rootConstantIndex = getDescriptorIndexFromName(pOut->pRootSignature, "RootConstantDispatch");
    if (pOut->rootConstantIndex == UINT32_MAX)
    {
        ADD_FAILURE() << "Compute root constants were not reflected";
        return false;
    }

    BufferDesc computeOutputDesc = {
        .Size = sizeof(uint32_t) * 2,
        .ElementCount = 2,
        .StructStride = sizeof(uint32_t),
        .Name = "ComputeOutputBuffer",
        .MemoryUsage = MemoryUsageGpuOnly,
        .StartState = ResourceStateUnorderedAccess,
        .Descriptors = DescriptorTypeRwBuffer,
    };
    addBuffer(harness.pRenderer, &computeOutputDesc, &pOut->pOutputBuffer);
    if (!pOut->pOutputBuffer)
    {
        ADD_FAILURE() << "addBuffer returned null for compute output";
        return false;
    }
    Buffer* pOutputBuffer = pOut->pOutputBuffer;
    Cleanup.add(
        [renderer = harness.pRenderer, pOutputBuffer]
        {
            if (pOutputBuffer)
            {
                removeBuffer(renderer, pOutputBuffer);
            }
        });

    BufferDesc computeRootCbvDesc = {
        .Size = 256,
        .Name = "ComputeRootCbvBuffer",
        .MemoryUsage = MemoryUsageCpuToGpu,
        .Flags = BufferCreationFlagPersistentMapBit,
        .StartState = ResourceStateGenericRead,
        .Descriptors = DescriptorTypeUniformBuffer,
    };
    addBuffer(harness.pRenderer, &computeRootCbvDesc, &pOut->pRootCbvBuffer);
    if (!pOut->pRootCbvBuffer)
    {
        ADD_FAILURE() << "addBuffer returned null for compute root CBV";
        return false;
    }
    Buffer* pRootCbvBuffer = pOut->pRootCbvBuffer;
    Cleanup.add(
        [renderer = harness.pRenderer, pRootCbvBuffer]
        {
            if (pRootCbvBuffer)
            {
                removeBuffer(renderer, pRootCbvBuffer);
            }
        });

    pOut->rootCbvData.multiplier = 3u;
    memcpy(pOut->pRootCbvBuffer->pCpuMappedAddress, &pOut->rootCbvData, sizeof(pOut->rootCbvData));

    BufferDesc computeReadbackDesc = {
        .Size = sizeof(uint32_t) * 2,
        .Name = "ComputeReadbackBuffer",
        .MemoryUsage = MemoryUsageGpuToCpu,
        .StartState = ResourceStateCopyDest,
    };
    addBuffer(harness.pRenderer, &computeReadbackDesc, &pOut->pReadbackBuffer);
    if (!pOut->pReadbackBuffer)
    {
        ADD_FAILURE() << "addBuffer returned null for compute readback";
        return false;
    }
    Buffer* pComputeReadbackBuffer = pOut->pReadbackBuffer;
    Cleanup.add(
        [renderer = harness.pRenderer, pComputeReadbackBuffer]
        {
            if (pComputeReadbackBuffer)
            {
                removeBuffer(renderer, pComputeReadbackBuffer);
            }
        });

    BufferDesc indirectBufferDesc = {
        .Size = sizeof(DispatchCommandData),
        .Name = "DispatchIndirectBuffer",
        .MemoryUsage = MemoryUsageCpuToGpu,
        .Flags = BufferCreationFlagPersistentMapBit,
        .StartState = ResourceStateGenericRead,
        .Descriptors = DescriptorTypeIndirectBuffer,
    };
    addBuffer(harness.pRenderer, &indirectBufferDesc, &pOut->pIndirectBuffer);
    if (!pOut->pIndirectBuffer)
    {
        ADD_FAILURE() << "addBuffer returned null for indirect Dispatch";
        return false;
    }
    Buffer* pIndirectBuffer = pOut->pIndirectBuffer;
    Cleanup.add(
        [renderer = harness.pRenderer, pIndirectBuffer]
        {
            if (pIndirectBuffer)
            {
                removeBuffer(renderer, pIndirectBuffer);
            }
        });

    DispatchCommandData dispatchData = {
        .args = {
            .groupCountX = 1,
            .groupCountY = 1,
            .groupCountZ = 1,
        },
    };
    memcpy(pOut->pIndirectBuffer->pCpuMappedAddress, &dispatchData, sizeof(dispatchData));

    PipelineDesc computePipelineDesc = {
        .computeDesc = {
            .pShaderProgram = pOut->pShader,
            .pRootSignature = pOut->pRootSignature,
        },
        .Name = "ComputePipeline",
        .Type = PIPELINE_TYPE_COMPUTE,
    };
    addPipeline(harness.pRenderer, &computePipelineDesc, &pOut->pPipeline);
    if (!pOut->pPipeline)
    {
        ADD_FAILURE() << "addPipeline returned null for compute";
        return false;
    }
    Pipeline* pComputePipeline = pOut->pPipeline;
    Cleanup.add(
        [renderer = harness.pRenderer, pComputePipeline]
        {
            if (pComputePipeline)
            {
                removePipeline(renderer, pComputePipeline);
            }
        });

    IndirectArgumentDescriptor dispatchArg = {
        .Type = INDIRECT_DISPATCH,
    };
    CommandSignatureDesc dispatchSignatureDesc = {
        .pArgDescs = &dispatchArg,
        .indirectArgCount = 1,
    };
    addIndirectCommandSignature(harness.pRenderer, &dispatchSignatureDesc, &pOut->pDispatchSignature);
    if (!pOut->pDispatchSignature)
    {
        ADD_FAILURE() << "addIndirectCommandSignature returned null";
        return false;
    }
    CommandSignature* pDispatchSignature = pOut->pDispatchSignature;
    Cleanup.add(
        [renderer = harness.pRenderer, pDispatchSignature]
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

void executeComputeAndReadBack(LiveRendererHarness& harness, const ComputeCommandSetup& setup, bool useIndirectDispatch,
                               uint32_t writeValue, uint32_t outputIndex, uint32_t* pOutValues)
{
    resetCmdPool(harness.pRenderer, harness.pCmdPool);
    beginCmd(harness.pCmd);
    cmdBindPipeline(harness.pCmd, setup.pPipeline);
    uint32_t dispatchConstants[4] = { writeValue, outputIndex, getBufferUavIndex(setup.pOutputBuffer),
                                      getBufferCbvIndex(setup.pRootCbvBuffer) };
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
        .currentState = ResourceStateUnorderedAccess,
        .newState = ResourceStateCopySource,
    };
    cmdResourceBarrier(harness.pCmd, 1, &computeBarrier, 0, nullptr, 0, nullptr);
    cmdUpdateBuffer(harness.pCmd, setup.pReadbackBuffer, 0, setup.pOutputBuffer, 0, sizeof(uint32_t) * 2);
    endCmd(harness.pCmd);

    harness.submitAndWait(harness.pCmd, harness.pFence);
    waitQueueIdle(harness.pQueue);

    ReadRange computeReadRange = {
        .offset = 0,
        .Size = sizeof(uint32_t) * 2,
    };
    mapBuffer(harness.pRenderer, setup.pReadbackBuffer, &computeReadRange);
    const uint32_t* computeValues = (const uint32_t*)setup.pReadbackBuffer->pCpuMappedAddress;
    pOutValues[0] = computeValues ? computeValues[0] : 0u;
    pOutValues[1] = computeValues ? computeValues[1] : 0u;
    unmapBuffer(harness.pRenderer, setup.pReadbackBuffer);
}

bool createQueryPool(LiveRendererHarness& harness, DeferredCleanup& Cleanup, QueryPool** ppQueryPool)
{
    QueryPoolDesc queryPoolDesc = {
        .Name = "CommandTimestampQuery",
        .Type = QUERY_TYPE_TIMESTAMP,
        .queryCount = 1,
    };
    addQueryPool(harness.pRenderer, &queryPoolDesc, ppQueryPool);
    if (!*ppQueryPool)
    {
        ADD_FAILURE() << "addQueryPool returned null";
        return false;
    }
    QueryPool* pQueryPool = *ppQueryPool;
    Cleanup.add(
        [renderer = harness.pRenderer, pQueryPool]
        {
            if (pQueryPool)
            {
                removeQueryPool(renderer, pQueryPool);
            }
        });
    return true;
}

bool createMarkerBuffer(LiveRendererHarness& harness, DeferredCleanup& Cleanup, Buffer** ppMarkerBuffer)
{
    BufferDesc markerBufferDesc = {
        .Size = sizeof(uint32_t),
        .Name = "GpuMarkerBuffer",
        .MemoryUsage = MemoryUsageGpuToCpu,
        .Flags = BufferCreationFlagMarker,
        .StartState = ResourceStateCopyDest,
    };
    addBuffer(harness.pRenderer, &markerBufferDesc, ppMarkerBuffer);
    if (!*ppMarkerBuffer)
    {
        ADD_FAILURE() << "addBuffer returned null for marker buffer";
        return false;
    }
    Buffer* pMarkerBuffer = *ppMarkerBuffer;
    Cleanup.add(
        [renderer = harness.pRenderer, pMarkerBuffer]
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
    EXPECT_GT(harness.pContext->gpuCount, 0u);
    EXPECT_EQ(harness.pRenderer->rendererApi, RENDERER_API_D3D12);
}

// Verifies that queue, fence, semaphore, command-pool, and command-buffer APIs Submit work and transition fence state as expected.
TEST_F(RHIIGraphicsApiTest, QueueAndSyncApisSubmitAndSignalCorrectly)
{
    LiveRendererHarness harness;
    ASSERT_RHI_CALL_OR_SKIP_ON_UNSUPPORTED(attachSharedRenderer(harness));
    ASSERT_TRUE(harness.addGraphicsQueueOnly());
    ASSERT_TRUE(harness.addSynchronizationPrimitivesOnly());
    ASSERT_TRUE(harness.addCommandPoolOnly());
    ASSERT_TRUE(harness.addPrimaryCmdOnly());
    ASSERT_TRUE(harness.addExtraCmdsOnly(2));

    FenceStatus fenceStatus = FenceStatusComplete;
    getFenceStatus(harness.pRenderer, harness.pFence, &fenceStatus);
    EXPECT_EQ(fenceStatus, FenceStatusNotSubmitted);

    resetCmdPool(harness.pRenderer, harness.pCmdPool);
    beginCmd(harness.ppExtraCmds[0]);
    endCmd(harness.ppExtraCmds[0]);

    Semaphore* signalSemaphores[] = { harness.pSemaphore };
    harness.submitAndWait(harness.ppExtraCmds[0], harness.pFence, 0, nullptr, 1, signalSemaphores);
    getFenceStatus(harness.pRenderer, harness.pFence, &fenceStatus);
    EXPECT_EQ(fenceStatus, FenceStatusComplete);

    Fence* pWaitFence = nullptr;
    addFence(harness.pRenderer, &pWaitFence);
    ASSERT_NE(pWaitFence, nullptr);

    DeferredCleanup Cleanup;
    Cleanup.add(
        [&]
        {
            if (pWaitFence)
            {
                removeFence(harness.pRenderer, pWaitFence);
                pWaitFence = nullptr;
            }
        });

    getFenceStatus(harness.pRenderer, pWaitFence, &fenceStatus);
    EXPECT_EQ(fenceStatus, FenceStatusNotSubmitted);

    resetCmdPool(harness.pRenderer, harness.pCmdPool);
    beginCmd(harness.ppExtraCmds[1]);
    endCmd(harness.ppExtraCmds[1]);
    Semaphore* waitSemaphores[] = { harness.pSemaphore };
    harness.submitAndWait(harness.ppExtraCmds[1], pWaitFence, 1, waitSemaphores);
    getFenceStatus(harness.pRenderer, pWaitFence, &fenceStatus);
    EXPECT_EQ(fenceStatus, FenceStatusComplete);
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

    EXPECT_EQ(harness.pSwapChain->imageCount, getRecommendedSwapchainImageCount(harness.pRenderer, &harness.mWindow.Handle));
    EXPECT_EQ(harness.pSwapChain->Format, getSupportedSwapchainFormat(harness.pRenderer, nullptr, harness.pSwapChain->colorSpace));

    uint32_t imageIndex = UINT32_MAX;
    acquireNextImage(harness.pRenderer, harness.pSwapChain, harness.pSemaphore, harness.pFence, &imageIndex);
    EXPECT_LT(imageIndex, harness.pSwapChain->imageCount);

    QueuePresentDesc presentDesc = {
        .pSwapChain = harness.pSwapChain,
        .index = (uint8_t)imageIndex,
    };
    queuePresent(harness.pQueue, &presentDesc);
    waitQueueIdle(harness.pQueue);

    const bool initialVsync = harness.pSwapChain->enableVsync != 0;
    SwapChain* pSwapChain = harness.pSwapChain;
    toggleVSync(harness.pRenderer, &pSwapChain);
    EXPECT_EQ(pSwapChain, harness.pSwapChain);
    EXPECT_NE(harness.pSwapChain->enableVsync != 0, initialVsync);
}

// Verifies that addSampler/removeSampler preserve the requested D3D12 address modes.
TEST_F(RHIIGraphicsApiTest, SamplerApiCreatesExpectedDescriptor)
{
    LiveRendererHarness harness;
    ASSERT_RHI_CALL_OR_SKIP_ON_UNSUPPORTED(attachSharedRenderer(harness));

    Sampler*    pSampler = nullptr;
    SamplerDesc samplerDesc = {
        .MinFilter = FILTER_LINEAR,
        .MagFilter = FILTER_LINEAR,
        .MipMapMode = MIPMAP_MODE_LINEAR,
        .AddressU = AddressModeClampToEdge,
        .AddressV = AddressModeClampToEdge,
        .AddressW = AddressModeRepeat,
        .MaxAnisotropy = 1.0f,
        .CompareFunc = CMP_NEVER,
    };
    addSampler(harness.pRenderer, &samplerDesc, &pSampler);
    ASSERT_NE(pSampler, nullptr);

    DeferredCleanup Cleanup;
    Cleanup.add(
        [&]
        {
            if (pSampler)
            {
                removeSampler(harness.pRenderer, pSampler);
                pSampler = nullptr;
            }
        });

    EXPECT_EQ(pSampler->dx.desc.AddressU, D3D12_TEXTURE_ADDRESS_MODE_CLAMP);
    EXPECT_EQ(pSampler->dx.desc.AddressV, D3D12_TEXTURE_ADDRESS_MODE_CLAMP);
    EXPECT_EQ(pSampler->dx.desc.AddressW, D3D12_TEXTURE_ADDRESS_MODE_WRAP);
}

// Verifies that shader APIs create valid source and binary compute shaders from the same compiled code.
TEST_F(RHIIGraphicsApiTest, ShaderApisCreateSourceAndBinaryShaders)
{
    LiveRendererHarness harness;
    ASSERT_RHI_CALL_OR_SKIP_ON_UNSUPPORTED(attachSharedRenderer(harness));

    DeferredCleanup       Cleanup;
    LifecycleShaderBundle shaderBundle = {};
    ASSERT_TRUE(createLifecycleShaderBundle(harness, Cleanup, &shaderBundle));

    ASSERT_NE(shaderBundle.pSourceShader->pReflection, nullptr);
    ASSERT_NE(shaderBundle.pBinaryShader->pReflection, nullptr);
    EXPECT_EQ(shaderBundle.pSourceShader->stages, ShaderStageComp);
    EXPECT_EQ(shaderBundle.pBinaryShader->stages, ShaderStageComp);
}

// Bindless shaders retain root constants while resources use persistent heap indices.
TEST_F(RHIIGraphicsApiTest, RootSignatureEnablesBindlessResources)
{
    LiveRendererHarness harness;
    ASSERT_RHI_CALL_OR_SKIP_ON_UNSUPPORTED(attachSharedRenderer(harness));

    DeferredCleanup       Cleanup;
    LifecycleShaderBundle shaderBundle = {};
    ASSERT_TRUE(createLifecycleShaderBundle(harness, Cleanup, &shaderBundle));

    LifecyclePlacedBufferBundle placedBufferBundle = {};
    ASSERT_TRUE(createLifecyclePlacedBuffer(harness, Cleanup, &placedBufferBundle));

    EXPECT_NE(getBufferUavIndex(placedBufferBundle.pBuffer), UINT32_MAX);
    EXPECT_EQ(shaderBundle.pRootSignature->descriptorCount, 1u);
    EXPECT_TRUE(shaderBundle.pBinaryShader->pReflection->stageReflections[0].cbvHeapIndexing);
}

// Verifies that getBufferSizeAlign/addResourceHeap/addBuffer create a valid placed buffer allocation.
TEST_F(RHIIGraphicsApiTest, ResourceHeapAndPlacedBufferApisCreateExpectedAllocation)
{
    LiveRendererHarness harness;
    ASSERT_RHI_CALL_OR_SKIP_ON_UNSUPPORTED(attachSharedRenderer(harness));

    DeferredCleanup             Cleanup;
    LifecyclePlacedBufferBundle placedBufferBundle = {};
    ASSERT_TRUE(createLifecyclePlacedBuffer(harness, Cleanup, &placedBufferBundle));

    ASSERT_NE(placedBufferBundle.pHeap, nullptr);
    ASSERT_NE(placedBufferBundle.pBuffer, nullptr);
    EXPECT_GT(placedBufferBundle.sizeAlign.Size, 0u);
    EXPECT_GT(placedBufferBundle.sizeAlign.alignment, 0u);
    EXPECT_NE(placedBufferBundle.pHeap->dx.pHeap, nullptr);
    EXPECT_EQ(placedBufferBundle.pBuffer->Size, sizeof(uint32_t) * 4);
}

// Verifies that pipeline-cache and compute-pipeline APIs create a usable pipeline and serialize cache data when available.
TEST_F(RHIIGraphicsApiTest, PipelineCacheAndPipelineApisCreateUsableComputePipeline)
{
    LiveRendererHarness harness;
    ASSERT_RHI_CALL_OR_SKIP_ON_UNSUPPORTED(attachSharedRenderer(harness));

    DeferredCleanup       Cleanup;
    LifecycleShaderBundle shaderBundle = {};
    ASSERT_TRUE(createLifecycleShaderBundle(harness, Cleanup, &shaderBundle));

    LifecyclePipelineBundle pipelineBundle = {};
    ASSERT_TRUE(createLifecyclePipelineBundle(harness, Cleanup, shaderBundle.pBinaryShader, shaderBundle.pRootSignature, &pipelineBundle));

    size_t serializedPipelineCacheSize = 0;
    getPipelineCacheData(harness.pRenderer, pipelineBundle.pPipelineCache, &serializedPipelineCacheSize, nullptr);
    if (serializedPipelineCacheSize > 0)
    {
        std::vector<uint8_t> pipelineCacheBytes(serializedPipelineCacheSize);
        getPipelineCacheData(harness.pRenderer, pipelineBundle.pPipelineCache, &serializedPipelineCacheSize, pipelineCacheBytes.GetData());
        EXPECT_FALSE(pipelineCacheBytes.Empty());
    }

    ASSERT_NE(pipelineBundle.pPipeline->dx.pPipelineState, nullptr);
}

// Verifies that render-target and memory-stat APIs create textures, report memory, and accept debug names.
TEST_F(RHIIGraphicsApiTest, RenderTargetAndMemoryStatsApisReportUsage)
{
    LiveRendererHarness harness;
    ASSERT_RHI_CALL_OR_SKIP_ON_UNSUPPORTED(attachSharedRenderer(harness));

    DeferredCleanup Cleanup;
    RenderTarget*   pRenderTarget = nullptr;
    ASSERT_TRUE(createRenderTarget(harness, Cleanup, "LifecycleRenderTarget", 4, 4, &pRenderTarget));

    uint64_t usedBytes = 0;
    uint64_t totalAllocatedBytes = 0;
    calculateMemoryUse(harness.pRenderer, &usedBytes, &totalAllocatedBytes);
    EXPECT_GT(usedBytes, 0u);
    EXPECT_GT(totalAllocatedBytes, 0u);
    EXPECT_LE(usedBytes, totalAllocatedBytes);

    char* pStats = nullptr;
    calculateMemoryStats(harness.pRenderer, &pStats);
    EXPECT_NE(pStats, nullptr);
    if (pStats)
    {
        EXPECT_NE(pStats[0], '\0');
        freeMemoryStats(harness.pRenderer, pStats);
    }

    EXPECT_EQ(pRenderTarget->Width, 4u);
    EXPECT_EQ(pRenderTarget->Height, 4u);
    ASSERT_NE(pRenderTarget->pTexture, nullptr);

#if defined(_WINDOWS)
    setTextureName(harness.pRenderer, pRenderTarget->pTexture, "LifecycleTexture");
    expectD3DObjectName(pRenderTarget->pTexture->dx.pResource, L"LifecycleTexture");

    setRenderTargetName(harness.pRenderer, pRenderTarget, "LifecycleRenderTarget");
    expectD3DObjectName(pRenderTarget->pTexture->dx.pResource, L"LifecycleRenderTarget");
#endif
}

// Verifies that cmdDraw renders the expected color into an offscreen target.
TEST_F(RHIIGraphicsApiTest, CmdDrawWritesExpectedPixels)
{
    LiveRendererHarness harness;
    ASSERT_RHI_CALL_OR_SKIP_ON_UNSUPPORTED(initSharedRendererHarness(harness, 0));

    DeferredCleanup   Cleanup;
    GraphicsDrawSetup drawSetup = {};
    ASSERT_TRUE(createGraphicsDrawSetup(harness, Cleanup, &drawSetup));

    const float red[4] = { 1.0f, 0.0f, 0.0f, 1.0f };
    PixelValue  pixel = executeDrawAndReadPixel(harness, drawSetup, DrawCallKind::Draw, red);
    expectPixelEq(&pixel.r, 255, 0, 0, 255);
}

// Verifies that cmdDrawInstanced renders the expected color into an offscreen target.
TEST_F(RHIIGraphicsApiTest, CmdDrawInstancedWritesExpectedPixels)
{
    LiveRendererHarness harness;
    ASSERT_RHI_CALL_OR_SKIP_ON_UNSUPPORTED(initSharedRendererHarness(harness, 0));

    DeferredCleanup   Cleanup;
    GraphicsDrawSetup drawSetup = {};
    ASSERT_TRUE(createGraphicsDrawSetup(harness, Cleanup, &drawSetup));

    const float green[4] = { 0.0f, 1.0f, 0.0f, 1.0f };
    PixelValue  pixel = executeDrawAndReadPixel(harness, drawSetup, DrawCallKind::DrawInstanced, green);
    expectPixelEq(&pixel.r, 0, 255, 0, 255);
}

// Verifies that cmdDrawIndexed renders the expected color into an offscreen target.
TEST_F(RHIIGraphicsApiTest, CmdDrawIndexedWritesExpectedPixels)
{
    LiveRendererHarness harness;
    ASSERT_RHI_CALL_OR_SKIP_ON_UNSUPPORTED(initSharedRendererHarness(harness, 0));

    DeferredCleanup   Cleanup;
    GraphicsDrawSetup drawSetup = {};
    ASSERT_TRUE(createGraphicsDrawSetup(harness, Cleanup, &drawSetup));

    const float blue[4] = { 0.0f, 0.0f, 1.0f, 1.0f };
    PixelValue  pixel = executeDrawAndReadPixel(harness, drawSetup, DrawCallKind::DrawIndexed, blue);
    expectPixelEq(&pixel.r, 0, 0, 255, 255);
}

// Verifies that cmdDrawIndexedInstanced renders the expected color into an offscreen target.
TEST_F(RHIIGraphicsApiTest, CmdDrawIndexedInstancedWritesExpectedPixels)
{
    LiveRendererHarness harness;
    ASSERT_RHI_CALL_OR_SKIP_ON_UNSUPPORTED(initSharedRendererHarness(harness, 0));

    DeferredCleanup   Cleanup;
    GraphicsDrawSetup drawSetup = {};
    ASSERT_TRUE(createGraphicsDrawSetup(harness, Cleanup, &drawSetup));

    const float yellow[4] = { 1.0f, 1.0f, 0.0f, 1.0f };
    PixelValue  pixel = executeDrawAndReadPixel(harness, drawSetup, DrawCallKind::DrawIndexedInstanced, yellow);
    expectPixelEq(&pixel.r, 255, 255, 0, 255);
}

// Constant-buffer heap indexing reads the current uploaded data.
TEST_F(RHIIGraphicsApiTest, BindlessConstantBufferAffectsComputeOutput)
{
    LiveRendererHarness harness;
    ASSERT_RHI_CALL_OR_SKIP_ON_UNSUPPORTED(initSharedRendererHarness(harness, 0));

    DeferredCleanup     Cleanup;
    ComputeCommandSetup computeSetup = {};
    ASSERT_TRUE(createComputeCommandSetup(harness, Cleanup, &computeSetup));

    setComputeMultiplier(computeSetup, 7u);
    uint32_t values[2] = {};
    executeComputeAndReadBack(harness, computeSetup, false, 5u, 0u, values);
    EXPECT_EQ(values[0], 35u);
}

// Verifies that cmdDispatch writes the expected value into the targeted UAV slot.
TEST_F(RHIIGraphicsApiTest, CmdDispatchWritesExpectedBufferValues)
{
    LiveRendererHarness harness;
    ASSERT_RHI_CALL_OR_SKIP_ON_UNSUPPORTED(initSharedRendererHarness(harness, 0));

    DeferredCleanup     Cleanup;
    ComputeCommandSetup computeSetup = {};
    ASSERT_TRUE(createComputeCommandSetup(harness, Cleanup, &computeSetup));

    uint32_t values[2] = {};
    executeComputeAndReadBack(harness, computeSetup, false, 5u, 0u, values);
    EXPECT_EQ(values[0], 15u);
}

// Verifies that cmdExecuteIndirect dispatches the expected work and writes through the targeted UAV slot.
TEST_F(RHIIGraphicsApiTest, CmdExecuteIndirectDispatchWritesExpectedBufferValues)
{
    LiveRendererHarness harness;
    ASSERT_RHI_CALL_OR_SKIP_ON_UNSUPPORTED(initSharedRendererHarness(harness, 0));

    DeferredCleanup     Cleanup;
    ComputeCommandSetup computeSetup = {};
    ASSERT_TRUE(createComputeCommandSetup(harness, Cleanup, &computeSetup));

    uint32_t values[2] = {};
    executeComputeAndReadBack(harness, computeSetup, true, 9u, 1u, values);
    EXPECT_EQ(values[1], 27u);
    EXPECT_EQ(computeSetup.pDispatchSignature->drawType, INDIRECT_DISPATCH);
    EXPECT_EQ(computeSetup.pDispatchSignature->stride, 16u);
}

// Verifies that query APIs report a valid timestamp range for submitted GPU work.
TEST_F(RHIIGraphicsApiTest, QueryApisProduceValidTimestamps)
{
    LiveRendererHarness harness;
    ASSERT_RHI_CALL_OR_SKIP_ON_UNSUPPORTED(initSharedRendererHarness(harness, 0));

    DeferredCleanup     Cleanup;
    ComputeCommandSetup computeSetup = {};
    ASSERT_TRUE(createComputeCommandSetup(harness, Cleanup, &computeSetup));

    QueryPool* pQueryPool = nullptr;
    ASSERT_TRUE(createQueryPool(harness, Cleanup, &pQueryPool));

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
    uint32_t dispatchConstants[4] = { 5u, 0u, getBufferUavIndex(computeSetup.pOutputBuffer),
                                      getBufferCbvIndex(computeSetup.pRootCbvBuffer) };
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
    EXPECT_TRUE(queryData.valid);
    EXPECT_GE(queryData.endTimestamp, queryData.beginTimestamp);
}

// Verifies that cmdUpdateBuffer and cmdWriteMarker make GPU results visible in CPU-readable buffers.
TEST_F(RHIIGraphicsApiTest, MarkerAndBufferCopyApisWriteExpectedResults)
{
    LiveRendererHarness harness;
    ASSERT_RHI_CALL_OR_SKIP_ON_UNSUPPORTED(initSharedRendererHarness(harness, 0));

    DeferredCleanup     Cleanup;
    ComputeCommandSetup computeSetup = {};
    ASSERT_TRUE(createComputeCommandSetup(harness, Cleanup, &computeSetup));

    Buffer* pMarkerBuffer = nullptr;
    ASSERT_TRUE(createMarkerBuffer(harness, Cleanup, &pMarkerBuffer));

#if defined(_WINDOWS)
    setBufferName(harness.pRenderer, pMarkerBuffer, "GpuMarkerBuffer");
    expectD3DObjectName(pMarkerBuffer->dx.pResource, L"GpuMarkerBuffer");
#endif

    resetCmdPool(harness.pRenderer, harness.pCmdPool);
    beginCmd(harness.pCmd);
    cmdBindPipeline(harness.pCmd, computeSetup.pPipeline);
    uint32_t dispatchConstants[4] = { 5u, 0u, getBufferUavIndex(computeSetup.pOutputBuffer),
                                      getBufferCbvIndex(computeSetup.pRootCbvBuffer) };
    cmdBindPushConstants(harness.pCmd, computeSetup.pRootSignature, computeSetup.rootConstantIndex, dispatchConstants);
    cmdDispatch(harness.pCmd, 1, 1, 1);

    BufferBarrier computeBarrier = {
        .pBuffer = computeSetup.pOutputBuffer,
        .currentState = ResourceStateUnorderedAccess,
        .newState = ResourceStateCopySource,
    };
    cmdResourceBarrier(harness.pCmd, 1, &computeBarrier, 0, nullptr, 0, nullptr);
    cmdUpdateBuffer(harness.pCmd, computeSetup.pReadbackBuffer, 0, computeSetup.pOutputBuffer, 0, sizeof(uint32_t) * 2);

    MarkerDesc markerDesc = {
        .pBuffer = pMarkerBuffer,
        .offset = 0,
        .value = 0xCAFEBABEu,
        .Flags = MARKER_FLAG_WAIT_FOR_WRITE,
    };
    cmdWriteMarker(harness.pCmd, &markerDesc);
    endCmd(harness.pCmd);

    harness.submitAndWait(harness.pCmd, harness.pFence);
    waitQueueIdle(harness.pQueue);

    ReadRange computeReadRange = {
        .offset = 0,
        .Size = sizeof(uint32_t) * 2,
    };
    mapBuffer(harness.pRenderer, computeSetup.pReadbackBuffer, &computeReadRange);
    const uint32_t* computeValues = (const uint32_t*)computeSetup.pReadbackBuffer->pCpuMappedAddress;
    ASSERT_NE(computeValues, nullptr);
    EXPECT_EQ(computeValues[0], 15u);
    unmapBuffer(harness.pRenderer, computeSetup.pReadbackBuffer);

    ReadRange markerReadRange = {
        .offset = 0,
        .Size = sizeof(uint32_t),
    };
    mapBuffer(harness.pRenderer, pMarkerBuffer, &markerReadRange);
    const uint32_t* markerValue = (const uint32_t*)pMarkerBuffer->pCpuMappedAddress;
    ASSERT_NE(markerValue, nullptr);
    EXPECT_EQ(markerValue[0], 0xCAFEBABEu);
    unmapBuffer(harness.pRenderer, pMarkerBuffer);
}
