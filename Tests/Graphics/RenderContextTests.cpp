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

template<typename T>
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
static_assert(std::is_same_v<decltype(hz::BufferDesc{}.StartState), ResourceState>);
static_assert(std::is_same_v<decltype(hz::BufferDesc{}.Descriptors), DescriptorType>);
static_assert(std::is_same_v<decltype(hz::BufferDesc{}.Flags), BufferCreationFlags>);
static_assert(std::is_same_v<decltype(hz::TextureDesc{}.Flags), TextureCreationFlags>);
static_assert(std::is_same_v<decltype(hz::ShaderStageDesc{}.stage), ShaderStage>);
static_assert(std::is_same_v<decltype(hz::GraphicsPipelineDesc{}.vertexLayout), VertexLayout>);

TEST(RenderContextLiveTest, OwnsDeviceAndResourceLifetime)
{
#if defined(_WINDOWS)
    const hz::ContextDesc desc = {
        .pAppName = "RenderContextTest",
        .Width = 0,
        .Height = 0,
        .imageCount = 2,
        .colorFormat = hz::Format::B8G8R8A8_SRGB,
        .colorSpace = ColorSpaceSdrSrgb,
    };
    hz::RenderContext context(desc);
    EXPECT_TRUE(context.IsSuspended());
    EXPECT_EQ(context.GetWidth(), 0u);
    EXPECT_EQ(context.GetHeight(), 0u);

    const uint32_t       initial = 17;
    const hz::BufferDesc bufferDesc = {
        .Size = sizeof(initial),
        .ElementCount = 1,
        .StructStride = sizeof(initial),
        .Name = "RenderContextTest.Buffer",
        .pInitialData = &initial,
        .initialDataSize = sizeof(initial),
        .usage = MemoryUsageGpuOnly,
        .StartState = ResourceStateShaderResource,
        .Descriptors = DescriptorTypeBuffer,
        .Flags = BufferCreationFlagNone,
    };
    {
        hz::GPUBuffer first = context.createBuffer(bufferDesc);
        ASSERT_TRUE(first.IsValid());
        hz::GPUBuffer second((hz::GPUBuffer&&)first);
        EXPECT_FALSE(first.IsValid());
        EXPECT_TRUE(second.IsValid());
    }

    const char                source[] = "cbuffer RootConstant0 { uint outputIndex; };"
                                         "[numthreads(1, 1, 1)] void CSMain(uint3 id : SV_DispatchThreadID) {"
                                         "RWStructuredBuffer<uint> output = ResourceDescriptorHeap[outputIndex]; output[id.x] = id.x; }";
    const hz::ShaderStageDesc stages[] = {
        { .stage = ShaderStageComp,
          .pSource = source,
          .sourceSize = sizeof(source) - 1,
          .pEntryPoint = "CSMain",
          .Name = "RenderContextTest.Compute" },
    };
    const hz::ComputePipelineDesc pipelineDesc = {
        .shaderDesc = { .stages = stages },
        .Name = "RenderContextTest.Pipeline",
    };
    const MemoryTrackingStats before = GetMemoryTrackingStats();
    {
        hz::GPUPipeline first = context.createComputePipeline(pipelineDesc);
        ASSERT_TRUE(first.IsValid());
        Pipeline* const pPipeline = first.get();
        hz::GPUPipeline second(std::move(first));
        EXPECT_FALSE(first.IsValid());
        EXPECT_EQ(second.get(), pPipeline);

        first = context.createComputePipeline(pipelineDesc);
        first = std::move(second);
        EXPECT_FALSE(second.IsValid());
        EXPECT_EQ(first.get(), pPipeline);
        first = hz::GPUPipeline{};
        EXPECT_FALSE(first.IsValid());
        const char vertexSource[] = "float4 main(uint id : SV_VertexID) : SV_Position { return float4(0, 0, 0, 1); }";
        const char fragmentSource[] = "struct Output { float4 first : SV_Target0; float4 second : SV_Target1; };"
                                      "Output main() { Output result; result.first = 1; result.second = 0.5; return result; }";
        second = context.createGraphicsPipeline({
            .shaderDesc = {
                .stages = {
                    { .stage = ShaderStageVert, .pSource = vertexSource, .sourceSize = sizeof(vertexSource) - 1 },
                    { .stage = ShaderStageFrag, .pSource = fragmentSource, .sourceSize = sizeof(fragmentSource) - 1 },
                },
            },
            .colorTargets = {
                { .Format = hz::Format::R8G8B8A8_UNORM },
                { .Format = hz::Format::R16G16B16A16_SFLOAT, .srcFactor = BC_SRC_ALPHA, .dstFactor = BC_ONE_MINUS_SRC_ALPHA },
            },
            .Name = "RenderContextTest.MrtPipeline",
        });
        EXPECT_TRUE(second.IsValid());
    }
    const MemoryTrackingStats after = GetMemoryTrackingStats();
    if (before.TrackingEnabled)
    {
        EXPECT_EQ(after.LiveAllocationCount, before.LiveAllocationCount);
        EXPECT_EQ(after.LiveRequestedBytes, before.LiveRequestedBytes);
    }
#else
    GTEST_SKIP() << "RenderContext production backend is Windows/D3D12 only";
#endif
}

TEST(RenderContextLiveTest, CompilesSlangWithBindlessResources)
{
#if defined(_WINDOWS)
    hz::RenderContext context({ .pAppName = "SlangComputeTest", .Width = 0, .Height = 0 });
    const char        source[] = R"(
        cbuffer RootConstant0 { uint outputIndex; };
        T identity<T>(T value) { return value; }
        [numthreads(4, 1, 1)]
        void CSMain(uint3 id : SV_DispatchThreadID) { RWStructuredBuffer<uint> Results = ResourceDescriptorHeap[outputIndex]; Results[id.x] = identity<uint>(id.x + 23); }
    )";
    hz::GPUPipeline pipeline = context.createComputePipeline({
        .shaderDesc = {
            .stages = { { .stage = ShaderStageComp, .pSource = source, .sourceSize = sizeof(source) - 1,
                          .pEntryPoint = "CSMain" } },
            .language = hz::ShaderLanguage::Slang,
        },
    });
    ASSERT_TRUE(pipeline.IsValid());
    hz::GPUBuffer    output = context.createBuffer({
        .Size = 4 * sizeof(uint32_t),
        .ElementCount = 4,
        .StructStride = sizeof(uint32_t),
        .usage = MemoryUsageGpuOnly,
        .StartState = ResourceStateUnorderedAccess,
        .Descriptors = DescriptorTypeRwBuffer,
    });
    hz::GPUBuffer    readback = context.createBuffer({
        .Size = 4 * sizeof(uint32_t),
        .usage = MemoryUsageGpuToCpu,
        .StartState = ResourceStateCopyDest,
        .Flags = BufferCreationFlagPersistentMapBit,
    });
    hz::CommandList& commands = context.acquireCommandList();
    commands.setPipeline(pipeline);
    commands.setRootConstant({ output.GetUavIndex() });
    commands.dispatch(1, 1, 1, { .storageBuffers = { &output } });
    commands.copyBuffer(readback, 0, output, 0, 4 * sizeof(uint32_t));
    context.wait(context.submit(commands));
    const uint32_t* values = (const uint32_t*)readback.get()->pCpuMappedAddress;
    ASSERT_NE(values, nullptr);
    for (uint32_t i = 0; i < 4; ++i)
        EXPECT_EQ(values[i], i + 23);

    const char graphicsSource[] = R"(
        cbuffer RootConstant0 { uint vertexIndex; uint sharedIndex; uint fragmentIndex; };
        float4 VSMain(uint id : SV_VertexID) : SV_Position
        {
            StructuredBuffer<float4> VertexOnly = ResourceDescriptorHeap[vertexIndex];
            StructuredBuffer<float4> Shared = ResourceDescriptorHeap[sharedIndex];
            return VertexOnly[id] + Shared[0];
        }
        float4 PSMain() : SV_Target0
        {
            StructuredBuffer<float4> FragmentOnly = ResourceDescriptorHeap[fragmentIndex];
            StructuredBuffer<float4> Shared = ResourceDescriptorHeap[sharedIndex];
            return FragmentOnly[0] + Shared[0];
        }
    )";
    hz::GPUPipeline graphics = context.createGraphicsPipeline({
        .shaderDesc = {
            .stages = {
                { .stage = ShaderStageVert, .pSource = graphicsSource, .sourceSize = sizeof(graphicsSource) - 1,
                  .pEntryPoint = "VSMain" },
                { .stage = ShaderStageFrag, .pSource = graphicsSource, .sourceSize = sizeof(graphicsSource) - 1,
                  .pEntryPoint = "PSMain" },
            },
            .language = hz::ShaderLanguage::Slang,
        },
        .colorTargets = { { .Format = hz::Format::R8G8B8A8_UNORM } },
    });
    EXPECT_TRUE(graphics.IsValid());
#else
    GTEST_SKIP() << "RenderContext production backend is Windows/D3D12 only";
#endif
}

TEST(RenderContextLiveTest, CopiesBufferRegionsAndDefersBufferRelease)
{
#if defined(_WINDOWS)
    hz::RenderContext context({ .pAppName = "BufferRegionsTest", .Width = 0, .Height = 0, .enableGpuValidation = true });
    uint32_t          values[16];
    for (uint32_t i = 0; i < 16; ++i)
        values[i] = 100 + i;
    hz::GPUBuffer staging = context.createBuffer({
        .Size = sizeof(values),
        .usage = MemoryUsageCpuToGpu,
        .StartState = ResourceStateGenericRead,
        .Flags = BufferCreationFlagPersistentMapBit,
    });
    hz::GPUBuffer target = context.createBuffer({
        .Size = sizeof(values),
        .ElementCount = 16,
        .usage = MemoryUsageGpuOnly,
        .StartState = ResourceStateShaderResource,
        .Descriptors = DescriptorTypeBufferRaw,
    });
    hz::GPUBuffer readback = context.createBuffer({
        .Size = sizeof(values),
        .usage = MemoryUsageGpuToCpu,
        .StartState = ResourceStateCopyDest,
        .Flags = BufferCreationFlagPersistentMapBit,
    });
    ASSERT_TRUE(staging.IsValid() && target.IsValid() && readback.IsValid());
    ASSERT_TRUE(context.updateBuffer(staging, 0, values, sizeof(values)));

    hz::CommandList& commands = context.acquireCommandList();
    commands.fillBuffer(target, 0, 0, sizeof(values));
    const hz::BufferCopyRegion regions[] = {
        { .srcOffset = 0, .dstOffset = 8 * sizeof(uint32_t), .Size = 4 * sizeof(uint32_t) },
        { .srcOffset = 12 * sizeof(uint32_t), .dstOffset = 0, .Size = 2 * sizeof(uint32_t) },
    };
    commands.copyBufferRegions(target, staging, regions);
    commands.copyBuffer(readback, 0, target, 0, sizeof(values));
    context.wait(context.submit(commands));
    const uint32_t* result = (const uint32_t*)readback.get()->pCpuMappedAddress;
    ASSERT_NE(result, nullptr);
    for (uint32_t i = 0; i < 16; ++i)
    {
        const uint32_t expected = i < 2 ? 112 + i : (i >= 8 && i < 12 ? 100 + i - 8 : 0);
        EXPECT_EQ(result[i], expected) << i;
    }

    // Released buffers return their SRV index for immediate reuse, which makes the release point observable.
    const hz::BufferDesc rawDesc = {
        .Size = sizeof(values),
        .ElementCount = 16,
        .usage = MemoryUsageGpuOnly,
        .StartState = ResourceStateShaderResource,
        .Descriptors = DescriptorTypeBufferRaw,
    };
    hz::GPUBuffer released = context.createBuffer(rawDesc);
    ASSERT_TRUE(released.IsValid());
    const uint32_t   releasedIndex = released.GetSrvIndex();
    hz::CommandList& pending = context.acquireCommandList();
    pending.copyBufferRegions(released, staging, regions);
    context.DeferRelease(std::move(released));
    EXPECT_FALSE(released.IsValid());
    // The open command list still references the buffer, so idling the queue must not release it.
    context.WaitIdle();
    {
        const hz::GPUBuffer probe = context.createBuffer(rawDesc);
        EXPECT_NE(probe.GetSrvIndex(), releasedIndex);
    }
    context.wait(context.submit(pending));
    context.submit(context.acquireCommandList());
    const hz::GPUBuffer reused = context.createBuffer(rawDesc);
    EXPECT_EQ(reused.GetSrvIndex(), releasedIndex);
    context.WaitIdle();
#else
    GTEST_SKIP() << "RenderContext production backend is Windows/D3D12 only";
#endif
}

TEST(RenderContextLiveTest, SamplesBindlessTexturesAndRecyclesViews)
{
#if defined(_WINDOWS)
    hz::RenderContext context({ .pAppName = "BindlessViewsTest", .enableGpuValidation = true });
    const char        source[] = R"(
        cbuffer RootConstant0
        {
            uint firstUav; uint secondUav; uint firstSrv; uint secondSrv;
            uint samplerIndex; uint outputIndex; uint configIndex; uint rawIndex;
        };
        struct Config { uint multiplier; };
        [numthreads(2, 1, 1)]
        void Write(uint3 id : SV_DispatchThreadID)
        {
            RWTexture2D<float4> target = ResourceDescriptorHeap[NonUniformResourceIndex(id.x == 0 ? firstUav : secondUav)];
            target[uint2(0, 0)] = id.x == 0 ? float4(1, 0, 0, 1) : float4(0, 1, 0, 1);
        }
        [numthreads(2, 1, 1)]
        void Read(uint3 id : SV_DispatchThreadID)
        {
            Texture2D<float4> source = ResourceDescriptorHeap[NonUniformResourceIndex(id.x == 0 ? firstSrv : secondSrv)];
            SamplerState surface = SamplerDescriptorHeap[samplerIndex];
            RWStructuredBuffer<uint4> output = ResourceDescriptorHeap[outputIndex];
            ConstantBuffer<Config> config = ResourceDescriptorHeap[configIndex];
            ByteAddressBuffer raw = ResourceDescriptorHeap[rawIndex];
            output[id.x] = uint4(round(source.SampleLevel(surface, float2(0.5, 0.5), 1) * 255)) * config.multiplier + raw.Load(0);
        }
    )";
    hz::GPUPipeline   write = context.createComputePipeline(
        { .shaderDesc = {
              .stages = { { .stage = ShaderStageComp, .pSource = source, .sourceSize = sizeof(source) - 1, .pEntryPoint = "Write" } },
              .language = hz::ShaderLanguage::Slang,
          } });
    hz::GPUPipeline read = context.createComputePipeline(
        { .shaderDesc = {
              .stages = { { .stage = ShaderStageComp, .pSource = source, .sourceSize = sizeof(source) - 1, .pEntryPoint = "Read" } },
              .language = hz::ShaderLanguage::Slang,
          } });
    ASSERT_TRUE(write.IsValid() && read.IsValid());
    const hz::TextureDesc textureDesc = {
        .Width = 2,
        .Height = 2,
        .MipLevels = 2,
        .Format = hz::Format::R8G8B8A8_UNORM,
        .StartState = ResourceStateUnorderedAccess,
        .Descriptors = DescriptorTypeTexture | DescriptorTypeRwTexture,
    };
    hz::GPUTexture textures[] = { context.createTexture(textureDesc), context.createTexture(textureDesc) };
    hz::GPUSampler sampler = context.createSampler({ .MinFilter = FILTER_NEAREST, .MagFilter = FILTER_NEAREST });
    const uint32_t configData[64] = { 2 };
    const uint32_t bias = 3;
    hz::GPUBuffer  config = context.createBuffer({
        .Size = 256,
        .ElementCount = 64,
        .StructStride = 4,
        .pInitialData = configData,
        .initialDataSize = sizeof(configData),
        .usage = MemoryUsageGpuOnly,
        .StartState = ResourceStateVertexAndConstantBuffer,
        .Descriptors = DescriptorTypeUniformBuffer | DescriptorTypeRwBuffer,
    });
    hz::GPUBuffer  raw = context.createBuffer({
        .Size = 4,
        .ElementCount = 1,
        .pInitialData = &bias,
        .initialDataSize = sizeof(bias),
        .usage = MemoryUsageGpuOnly,
        .StartState = ResourceStateShaderResource,
        .Descriptors = DescriptorTypeBufferRaw,
    });
    hz::GPUBuffer  output = context.createBuffer({
        .Size = 32,
        .ElementCount = 2,
        .StructStride = 16,
        .usage = MemoryUsageGpuOnly,
        .StartState = ResourceStateUnorderedAccess,
        .Descriptors = DescriptorTypeRwBuffer,
    });
    hz::GPUBuffer  readback = context.createBuffer({
        .Size = 32,
        .usage = MemoryUsageGpuToCpu,
        .StartState = ResourceStateCopyDest,
        .Flags = BufferCreationFlagPersistentMapBit,
    });
    EXPECT_NE(config.GetCbvIndex(), config.GetUavIndex());
    EXPECT_EQ(textures[0].GetUavIndex(1), textures[0].GetUavIndex(0) + 1);
    const uint32_t   indices[] = { textures[0].GetUavIndex(1), textures[1].GetUavIndex(1), textures[0].GetSrvIndex(),
                                   textures[1].GetSrvIndex(),  sampler.GetIndex(),         output.GetUavIndex(),
                                   config.GetCbvIndex(),       raw.GetSrvIndex() };
    hz::CommandList& commands = context.acquireCommandList();
    commands.setPipeline(write);
    commands.setRootConstant({ indices, 4 });
    commands.setRootConstant({ indices + 4, 4 }, 4);
    commands.dispatch(1, 1, 1, { .storageTextures = { &textures[0], &textures[1] } });
    commands.setPipeline(read);
    commands.setRootConstant({ indices, 4 });
    commands.setRootConstant({ indices + 4, 4 }, 4);
    commands.dispatch(1, 1, 1,
                      { .sampledTextures = { &textures[0], &textures[1] }, .buffers = { &config, &raw }, .storageBuffers = { &output } });
    commands.copyBuffer(readback, 0, output, 0, 32);
    context.wait(context.submit(commands));
    const uint32_t  expected[] = { 513, 3, 3, 513, 3, 513, 3, 513 };
    const uint32_t* values = (const uint32_t*)readback.get()->pCpuMappedAddress;
    for (uint32_t i = 0; i < TF_ARRAY_COUNT(expected); ++i)
        EXPECT_EQ(values[i], expected[i]);

    hz::GPUTexture moved(std::move(textures[0]));
    EXPECT_FALSE(textures[0].IsValid());
    EXPECT_EQ(moved.GetSrvIndex(), indices[2]);
    moved = {};
    textures[0] = context.createTexture(textureDesc);
    EXPECT_EQ(textures[0].GetSrvIndex(), indices[2]);
    EXPECT_EQ(textures[1].GetSrvIndex(), indices[3]);
#else
    GTEST_SKIP() << "RenderContext production backend is Windows/D3D12 only";
#endif
}

TEST(RenderContextLiveTest, RecreatesSwapchainAcrossResize)
{
#if defined(_WINDOWS)
    initWindowClass();
    WindowDesc window = {
        .WindowedRect = { 0, 0, 640, 480 },
        .FullscreenRect = { 0, 0, 640, 480 },
        .ClientRect = { 0, 0, 640, 480 },
        .Hidden = true,
        .NoResizeFrame = true,
        .OverrideDefaultPosition = true,
    };
    OpenWindow("RenderContextResizeTest", &window);
    ASSERT_NE(window.Handle.window, nullptr);

    const hz::ContextDesc desc = {
        .pAppName = "RenderContextResizeTest",
        .windowHandle = window.Handle,
        .Width = 640,
        .Height = 480,
        .imageCount = 2,
        .colorFormat = hz::Format::B8G8R8A8_SRGB,
        .colorSpace = ColorSpaceSdrSrgb,
    };
    hz::RenderContext context(desc);

    const uint32_t sizes[][2] = { { 900, 600 }, { 1400, 800 }, { 640, 480 }, { 1280, 720 } };
    for (const auto& Size : sizes)
    {
        ASSERT_TRUE(context.resize(Size[0], Size[1]));
        hz::CommandList&       prepare = context.acquireCommandList();
        const hz::SubmitHandle prepareSubmit = context.submit(prepare);

        hz::CommandList&      commands = context.acquireCommandList();
        const hz::GPUTexture& backbuffer = context.getCurrentBackbuffer();
        commands.beginRendering({
            .colorAttachments = { {
                .pTexture = &backbuffer,
                .loadAction = LoadActionClear,
                .storeAction = StoreActionStore,
                .ClearValue = { .r = 0.0f, .g = 0.0f, .b = 0.0f, .a = 1.0f },
            } },
        });
        commands.endRendering();
        const hz::SubmitHandle presentSubmit = context.submit(commands, &backbuffer);
        EXPECT_TRUE(prepareSubmit.IsValid());
        EXPECT_TRUE(presentSubmit.IsValid());
        context.wait(presentSubmit);
    }

    ClosePlatformWindow(&window);
    exitWindowClass();
#else
    GTEST_SKIP() << "RenderContext production backend is Windows/D3D12 only";
#endif
}

TEST(DependencyListTest, SkipsOptionalResourcesAndReportsIndices)
{
    hz::GPUTexture     texture;
    hz::GPUBuffer      buffer;
    hz::DependencyList dependencies;
    // Null and uninitialized handles are optional: they report the invalid index and add no barrier.
    EXPECT_EQ(dependencies.sample(nullptr), UINT32_MAX);
    EXPECT_EQ(dependencies.sample(&texture), UINT32_MAX);
    EXPECT_EQ(dependencies.store(nullptr), UINT32_MAX);
    EXPECT_EQ(dependencies.store(&texture), UINT32_MAX);
    EXPECT_EQ(dependencies.read(buffer), UINT32_MAX);
    EXPECT_EQ(dependencies.write(buffer), UINT32_MAX);
    dependencies.indirect(buffer);
    const hz::Dependencies recorded = dependencies.get();
    EXPECT_EQ(recorded.sampledTextures.Count, 0u);
    EXPECT_EQ(recorded.storageTextures.Count, 0u);
    EXPECT_EQ(recorded.buffers.Count, 0u);
    EXPECT_EQ(recorded.storageBuffers.Count, 0u);
    EXPECT_EQ(recorded.indirectBuffers.Count, 0u);
}