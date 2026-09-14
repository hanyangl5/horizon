#include <gtest/gtest.h>

#include "Graphics/RenderContext.h"
#include "Platform/IOperatingSystem.h"

#include <type_traits>

#define IMEMORY_FROM_HEADER
#include "Core/IMemory.h"

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
        .colorFormat = hz::Format::B8G8R8A8_SRGB,
        .colorSpace = COLOR_SPACE_SDR_SRGB,
    };
    hz::RenderContext context(desc);
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
        ASSERT_TRUE(first.isValid());
        hz::GPUBuffer second((hz::GPUBuffer&&)first);
        EXPECT_FALSE(first.isValid());
        EXPECT_TRUE(second.isValid());
    }

    const char source[] =
        "RWStructuredBuffer<uint> output : register(u0);"
        "[numthreads(1, 1, 1)] void CSMain(uint3 id : SV_DispatchThreadID) { output[id.x] = id.x; }";
    const hz::ShaderStageDesc stages[] = {
        { .stage = SHADER_STAGE_COMP,
          .pSource = source,
          .sourceSize = sizeof(source) - 1,
          .pEntryPoint = "CSMain",
          .pName = "RenderContextTest.Compute" },
    };
    const hz::ComputePipelineDesc pipelineDesc = {
        .shaderDesc = { .stages = stages },
        .pName = "RenderContextTest.Pipeline",
    };
    const MemoryTrackingStats before = memGetTrackingStats();
    {
        hz::GPUPipeline first = context.createComputePipeline(pipelineDesc);
        ASSERT_TRUE(first.isValid());
        Pipeline* const pPipeline = first.get();
        hz::GPUPipeline second(std::move(first));
        EXPECT_FALSE(first.isValid());
        EXPECT_EQ(second.get(), pPipeline);

        first = context.createComputePipeline(pipelineDesc);
        first = std::move(second);
        EXPECT_FALSE(second.isValid());
        EXPECT_EQ(first.get(), pPipeline);
        first = hz::GPUPipeline{};
        EXPECT_FALSE(first.isValid());
        const char vertexSource[] = "float4 main(uint id : SV_VertexID) : SV_Position { return float4(0, 0, 0, 1); }";
        const char fragmentSource[] =
            "struct Output { float4 first : SV_Target0; float4 second : SV_Target1; };"
            "Output main() { Output result; result.first = 1; result.second = 0.5; return result; }";
        second = context.createGraphicsPipeline({
            .shaderDesc = {
                .stages = {
                    { .stage = SHADER_STAGE_VERT, .pSource = vertexSource, .sourceSize = sizeof(vertexSource) - 1 },
                    { .stage = SHADER_STAGE_FRAG, .pSource = fragmentSource, .sourceSize = sizeof(fragmentSource) - 1 },
                },
            },
            .colorTargets = {
                { .format = hz::Format::R8G8B8A8_UNORM },
                { .format = hz::Format::R16G16B16A16_SFLOAT, .srcFactor = BC_SRC_ALPHA, .dstFactor = BC_ONE_MINUS_SRC_ALPHA },
            },
            .pName = "RenderContextTest.MrtPipeline",
        });
        EXPECT_TRUE(second.isValid());
    }
    const MemoryTrackingStats after = memGetTrackingStats();
    if (before.trackingEnabled)
    {
        EXPECT_EQ(after.liveAllocationCount, before.liveAllocationCount);
        EXPECT_EQ(after.liveRequestedBytes, before.liveRequestedBytes);
    }
#else
    GTEST_SKIP() << "RenderContext production backend is Windows/D3D12 only";
#endif
}

TEST(RenderContextLiveTest, CompilesSlangWithAutomaticBindings)
{
#if defined(_WINDOWS)
    hz::RenderContext context({ .pAppName = "SlangComputeTest", .width = 0, .height = 0 });
    const char        source[] = R"(
        RWStructuredBuffer<uint> Results;
        T identity<T>(T value) { return value; }
        [numthreads(4, 1, 1)]
        void CSMain(uint3 id : SV_DispatchThreadID) { Results[id.x] = identity<uint>(id.x + 23); }
    )";
    hz::GPUPipeline pipeline = context.createComputePipeline({
        .shaderDesc = {
            .stages = { { .stage = SHADER_STAGE_COMP, .pSource = source, .sourceSize = sizeof(source) - 1,
                          .pEntryPoint = "CSMain" } },
            .language = hz::ShaderLanguage::Slang,
        },
    });
    ASSERT_TRUE(pipeline.isValid());
    hz::GPUBuffer    output = context.createBuffer({
           .size = 4 * sizeof(uint32_t),
           .elementCount = 4,
           .structStride = sizeof(uint32_t),
           .usage = RESOURCE_MEMORY_USAGE_GPU_ONLY,
           .startState = RESOURCE_STATE_UNORDERED_ACCESS,
           .descriptors = DESCRIPTOR_TYPE_RW_BUFFER,
    });
    hz::GPUBuffer    readback = context.createBuffer({
           .size = 4 * sizeof(uint32_t),
           .usage = RESOURCE_MEMORY_USAGE_GPU_TO_CPU,
           .startState = RESOURCE_STATE_COPY_DEST,
           .flags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT,
    });
    hz::CommandList& commands = context.acquireCommandList();
    commands.setPipeline(pipeline);
    commands.bindBuffer("Results", output);
    commands.dispatch(1, 1, 1, { .buffers = { &output } });
    commands.copyBuffer(readback, 0, output, 0, 4 * sizeof(uint32_t));
    context.wait(context.submit(commands));
    const uint32_t* values = (const uint32_t*)readback.get()->pCpuMappedAddress;
    ASSERT_NE(values, nullptr);
    for (uint32_t i = 0; i < 4; ++i)
        EXPECT_EQ(values[i], i + 23);

    const char graphicsSource[] = R"(
        StructuredBuffer<float4> VertexOnly;
        StructuredBuffer<float4> Shared;
        StructuredBuffer<float4> FragmentOnly;
        float4 VSMain(uint id : SV_VertexID) : SV_Position { return VertexOnly[id] + Shared[0]; }
        float4 PSMain() : SV_Target0 { return FragmentOnly[0] + Shared[0]; }
    )";
    hz::GPUPipeline graphics = context.createGraphicsPipeline({
        .shaderDesc = {
            .stages = {
                { .stage = SHADER_STAGE_VERT, .pSource = graphicsSource, .sourceSize = sizeof(graphicsSource) - 1,
                  .pEntryPoint = "VSMain" },
                { .stage = SHADER_STAGE_FRAG, .pSource = graphicsSource, .sourceSize = sizeof(graphicsSource) - 1,
                  .pEntryPoint = "PSMain" },
            },
            .language = hz::ShaderLanguage::Slang,
        },
        .colorTargets = { { .format = hz::Format::R8G8B8A8_UNORM } },
    });
    EXPECT_TRUE(graphics.isValid());
#else
    GTEST_SKIP() << "RenderContext production backend is Windows/D3D12 only";
#endif
}

TEST(RenderContextLiveTest, BindsSlangParameterBlocksAndSparseSpaces)
{
#if defined(_WINDOWS)
    hz::RenderContext context({ .pAppName = "SlangSpacesTest", .width = 0, .height = 0 });
    const char        source[] = R"(
        struct Output { RWStructuredBuffer<uint> values; };
        ParameterBlock<Output> group0;
        ParameterBlock<Output> group1;
        ParameterBlock<Output> group2;
        ParameterBlock<Output> group3;
        ParameterBlock<Output> group4;
        ParameterBlock<Output> group5;
        ParameterBlock<Output> group6;
        ParameterBlock<Output> group7;
        ParameterBlock<Output> group8;
        ParameterBlock<Output> group9;
        ParameterBlock<Output> group10;
        ParameterBlock<Output> group11;
        ParameterBlock<Output> group12;
        ParameterBlock<Output> group13;
        ParameterBlock<Output> group14;
        ParameterBlock<Output> group15;
        struct Nested { ParameterBlock<Output> inner; };
        ParameterBlock<Nested> nested;
        RWStructuredBuffer<uint> sparse : register(u0, space999);
        RWStructuredBuffer<uint> large : register(u0, space65536);
        RWStructuredBuffer<uint> highest : register(u0, space2147483647);
        [numthreads(1, 1, 1)]
        void CSMain()
        {
            group0.values[0] = 10;
            group1.values[1] = 11;
            group2.values[2] = 12;
            group3.values[3] = 13;
            group4.values[4] = 14;
            group5.values[5] = 15;
            group6.values[6] = 16;
            group7.values[7] = 17;
            group8.values[8] = 18;
            group9.values[9] = 19;
            group10.values[10] = 20;
            group11.values[11] = 21;
            group12.values[12] = 22;
            group13.values[13] = 23;
            group14.values[14] = 24;
            group15.values[15] = 25;
            nested.inner.values[16] = 26;
            sparse[17] = 27;
            large[18] = 28;
            highest[19] = 29;
        }
    )";
    hz::GPUPipeline pipeline = context.createComputePipeline({
        .shaderDesc = {
            .stages = { { .stage = SHADER_STAGE_COMP, .pSource = source, .sourceSize = sizeof(source) - 1,
                          .pEntryPoint = "CSMain" } },
            .language = hz::ShaderLanguage::Slang,
        },
    });
    ASSERT_TRUE(pipeline.isValid());
    const RootSignature* root = pipeline.get()->dx.pRootSignature;
    EXPECT_EQ(root->descriptorSetCount, 20u);
    for (uint32_t i = 0; i < root->descriptorCount; ++i)
    {
        EXPECT_LT(root->pDescriptors[i].groupIndex, root->descriptorSetCount);
        EXPECT_EQ(root->dx.pLayouts[root->pDescriptors[i].groupIndex].spaceIndex, root->pDescriptors[i].spaceIndex);
    }
    const uint32_t sparseSpaces[] = { 999, 65536, 0x7fffffff };
    const char*    sparseNames[] = { "sparse", "large", "highest" };
    for (uint32_t i = 0; i < 3; ++i)
    {
        const uint32_t index = getDescriptorIndexFromName(root, sparseNames[i]);
        ASSERT_NE(index, UINT32_MAX);
        EXPECT_EQ(root->pDescriptors[index].spaceIndex, sparseSpaces[i]);
    }

    const char      smallSource[] = "RWStructuredBuffer<uint> output : register(u0, space4294967279);"
                                    "[numthreads(1, 1, 1)] void CSMain() { output[0] = 99; }";
    hz::GPUPipeline smallPipeline = context.createComputePipeline({
        .shaderDesc = { .stages = { { .stage = SHADER_STAGE_COMP,
                                      .pSource = smallSource,
                                      .sourceSize = sizeof(smallSource) - 1,
                                      .pEntryPoint = "CSMain" } } },
    });
    ASSERT_TRUE(smallPipeline.isValid());
    ASSERT_EQ(smallPipeline.get()->dx.pRootSignature->descriptorSetCount, 1u);
    EXPECT_EQ(smallPipeline.get()->dx.pRootSignature->pDescriptors[0].spaceIndex, 0xffffffefu);
    hz::GPUBuffer outputs[2];
    for (hz::GPUBuffer& output : outputs)
    {
        output = context.createBuffer({
            .size = 20 * sizeof(uint32_t),
            .elementCount = 20,
            .structStride = sizeof(uint32_t),
            .usage = RESOURCE_MEMORY_USAGE_GPU_ONLY,
            .startState = RESOURCE_STATE_UNORDERED_ACCESS,
            .descriptors = DESCRIPTOR_TYPE_RW_BUFFER,
        });
        ASSERT_TRUE(output.isValid());
    }
    hz::GPUBuffer readback = context.createBuffer({
        .size = 40 * sizeof(uint32_t),
        .usage = RESOURCE_MEMORY_USAGE_GPU_TO_CPU,
        .startState = RESOURCE_STATE_COPY_DEST,
        .flags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT,
    });
    ASSERT_TRUE(readback.isValid());
    for (uint32_t iteration = 0; iteration < 2; ++iteration)
    {
        hz::CommandList& commands = context.acquireCommandList();
        for (uint32_t outputIndex = 0; outputIndex < 2; ++outputIndex)
        {
            commands.setPipeline(pipeline);
            for (uint32_t i = 0; i < root->descriptorCount; ++i)
                commands.bindBuffer(root->pDescriptors[i].pName, outputs[outputIndex]);
            commands.dispatch(1, 1, 1, { .buffers = { &outputs[outputIndex] } });
            if (outputIndex == 0)
            {
                commands.setPipeline(smallPipeline);
                commands.bindBuffer("output", outputs[0]);
                commands.dispatch(1, 1, 1, { .buffers = { &outputs[0] } });
            }
            commands.copyBuffer(readback, outputIndex * 20 * sizeof(uint32_t), outputs[outputIndex], 0, 20 * sizeof(uint32_t));
        }
        context.wait(context.submit(commands));
        const uint32_t* values = (const uint32_t*)readback.get()->pCpuMappedAddress;
        ASSERT_NE(values, nullptr);
        for (uint32_t i = 0; i < 40; ++i)
            EXPECT_EQ(values[i], i == 0 ? 99u : 10u + i % 20);
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
        .colorFormat = hz::Format::B8G8R8A8_SRGB,
        .colorSpace = COLOR_SPACE_SDR_SRGB,
    };
    hz::RenderContext context(desc);

    const uint32_t sizes[][2] = { { 900, 600 }, { 1400, 800 }, { 640, 480 }, { 1280, 720 } };
    for (const auto& size : sizes)
    {
        ASSERT_TRUE(context.resize(size[0], size[1]));
        hz::CommandList&       prepare = context.acquireCommandList();
        const hz::SubmitHandle prepareSubmit = context.submit(prepare);

        hz::CommandList&      commands = context.acquireCommandList();
        const hz::GPUTexture& backbuffer = context.getCurrentBackbuffer();
        commands.beginRendering({
            .colorAttachments = { {
                .pTexture = &backbuffer,
                .loadAction = LOAD_ACTION_CLEAR,
                .storeAction = STORE_ACTION_STORE,
                .clearValue = { .r = 0.0f, .g = 0.0f, .b = 0.0f, .a = 1.0f },
            } },
        });
        commands.endRendering();
        const hz::SubmitHandle presentSubmit = context.submit(commands, &backbuffer);
        EXPECT_TRUE(prepareSubmit.isValid());
        EXPECT_TRUE(presentSubmit.isValid());
        context.wait(presentSubmit);
    }

    closeWindow(&window);
    exitWindowClass();
#else
    GTEST_SKIP() << "RenderContext production backend is Windows/D3D12 only";
#endif
}
