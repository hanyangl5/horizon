#include <gtest/gtest.h>

#include "Graphics/RenderContext.h"
#include "Platform/IOperatingSystem.h"

#include <type_traits>

#if defined(_WINDOWS)
extern void initWindowClass();
extern void exitWindowClass();
#endif

template <typename T>
constexpr bool IsMoveOnlyGpuResource = !std::is_copy_constructible_v<T> && !std::is_copy_assignable_v<T> &&
                                     std::is_nothrow_move_constructible_v<T> && std::is_nothrow_move_assignable_v<T>;

static_assert(IsMoveOnlyGpuResource<hz::GPUBuffer>);
static_assert(IsMoveOnlyGpuResource<hz::GPUTexture>);
static_assert(IsMoveOnlyGpuResource<hz::GPUSampler>);
static_assert(IsMoveOnlyGpuResource<hz::GPUShader>);
static_assert(IsMoveOnlyGpuResource<hz::GPUPipeline>);
static_assert(!std::is_polymorphic_v<hz::CommandList>);
static_assert(!std::is_polymorphic_v<hz::RenderContext>);
static_assert(std::is_same_v<decltype(hz::BufferDesc{}.usage), ResourceMemoryUsage>);
static_assert(std::is_same_v<decltype(hz::BufferDesc{}.startState), ResourceState>);
static_assert(std::is_same_v<decltype(hz::BufferDesc{}.descriptors), DescriptorType>);
static_assert(std::is_same_v<decltype(hz::BufferDesc{}.flags), BufferCreationFlags>);
static_assert(std::is_same_v<decltype(hz::TextureDesc{}.flags), TextureCreationFlags>);
static_assert(std::is_same_v<decltype(hz::ShaderStageDesc{}.stage), ShaderStage>);
static_assert(std::is_same_v<decltype(hz::GraphicsPipelineDesc{}.vertexLayout), VertexLayout>);

TEST(RenderContextLiveTest, OwnsDeviceAndResourceLifetime)
{
#if defined(_WINDOWS)
    const hz::ContextDesc desc = {
        .pAppName = "RenderContextTest",
        .width = 0,
        .height = 0,
        .imageCount = 2,
        .colorFormat = TinyImageFormat_B8G8R8A8_SRGB,
        .colorSpace = COLOR_SPACE_SDR_SRGB,
    };
    hz::RenderContext context;
    ASSERT_TRUE(context.init(desc));
    EXPECT_TRUE(context.isSuspended());
    EXPECT_EQ(context.getWidth(), 0u);
    EXPECT_EQ(context.getHeight(), 0u);

    const uint32_t       initial = 17;
    const hz::BufferDesc bufferDesc = {
        .size = sizeof(initial),
        .elementCount = 1,
        .structStride = sizeof(initial),
        .pName = "RenderContextTest.Buffer",
        .pInitialData = &initial,
        .initialDataSize = sizeof(initial),
        .usage = RESOURCE_MEMORY_USAGE_GPU_ONLY,
        .startState = RESOURCE_STATE_SHADER_RESOURCE,
        .descriptors = DESCRIPTOR_TYPE_BUFFER,
        .flags = BUFFER_CREATION_FLAG_NONE,
    };
    {
        hz::GPUBuffer first = context.createBuffer(bufferDesc);
        ASSERT_TRUE(first);
        hz::GPUBuffer second((hz::GPUBuffer&&)first);
        EXPECT_FALSE(first);
        EXPECT_TRUE(second);
    }
#else
    GTEST_SKIP() << "RenderContext production backend is Windows/D3D12 only";
#endif
}

TEST(RenderContextLiveTest, RecreatesSwapchainAcrossResize)
{
#if defined(_WINDOWS)
    initWindowClass();
    WindowDesc window = {
        .windowedRect = { 0, 0, 640, 480 },
        .fullscreenRect = { 0, 0, 640, 480 },
        .clientRect = { 0, 0, 640, 480 },
        .hide = true,
        .noresizeFrame = true,
        .overrideDefaultPosition = true,
    };
    openWindow("RenderContextResizeTest", &window);
    ASSERT_NE(window.handle.window, nullptr);

    const hz::ContextDesc desc = {
        .pAppName = "RenderContextResizeTest",
        .windowHandle = window.handle,
        .width = 640,
        .height = 480,
        .imageCount = 2,
        .colorFormat = TinyImageFormat_B8G8R8A8_SRGB,
        .colorSpace = COLOR_SPACE_SDR_SRGB,
    };
    hz::RenderContext context;
    ASSERT_TRUE(context.init(desc));

    const uint32_t sizes[][2] = { { 900, 600 }, { 1400, 800 }, { 640, 480 }, { 1280, 720 } };
    for (const auto& size : sizes)
    {
        ASSERT_TRUE(context.resize(size[0], size[1]));
        hz::CommandList&       prepare = context.acquireCommandList();
        const hz::SubmitHandle prepareSubmit = context.submit(prepare);

        hz::CommandList&      commands = context.acquireCommandList();
        const hz::GPUTexture& backbuffer = context.getCurrentBackbuffer();
        hz::RenderPassDesc    pass = {
               .colorAttachments = { {
                   .pTexture = &backbuffer,
                   .loadAction = LOAD_ACTION_CLEAR,
                   .storeAction = STORE_ACTION_STORE,
                   .clearValue = { .r = 0.0f, .g = 0.0f, .b = 0.0f, .a = 1.0f },
            } },
               .colorAttachmentCount = 1,
        };
        commands.beginRendering(pass);
        commands.endRendering();
        const hz::SubmitHandle presentSubmit = context.submit(commands, &backbuffer);
        EXPECT_TRUE(prepareSubmit);
        EXPECT_TRUE(presentSubmit);
        context.wait(presentSubmit);
    }

    closeWindow(&window);
    exitWindowClass();
#else
    GTEST_SKIP() << "RenderContext production backend is Windows/D3D12 only";
#endif
}
