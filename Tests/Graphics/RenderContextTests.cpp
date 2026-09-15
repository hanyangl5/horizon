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

    const char                source[] = "cbuffer RootConstant0 { uint outputIndex; };"
                                         "[numthreads(1, 1, 1)] void CSMain(uint3 id : SV_DispatchThreadID) {"
                                         "RWStructuredBuffer<uint> output = ResourceDescriptorHeap[outputIndex]; output[id.x] = id.x; }";
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
        const char fragmentSource[] = "struct Output { float4 first : SV_Target0; float4 second : SV_Target1; };"
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

TEST(RenderContextLiveTest, CompilesSlangWithBindlessResources)
{
#if defined(_WINDOWS)
    hz::RenderContext context({ .pAppName = "SlangComputeTest", .width = 0, .height = 0 });
    const char        source[] = R"(
        cbuffer RootConstant0 { uint outputIndex; };
        T identity<T>(T value) { return value; }
        [numthreads(4, 1, 1)]
        void CSMain(uint3 id : SV_DispatchThreadID) { RWStructuredBuffer<uint> Results = ResourceDescriptorHeap[outputIndex]; Results[id.x] = identity<uint>(id.x + 23); }
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
    commands.setRootConstant({ output.getUavIndex() });
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
              .stages = { { .stage = SHADER_STAGE_COMP, .pSource = source, .sourceSize = sizeof(source) - 1, .pEntryPoint = "Write" } },
              .language = hz::ShaderLanguage::Slang,
          } });
    hz::GPUPipeline read = context.createComputePipeline(
        { .shaderDesc = {
              .stages = { { .stage = SHADER_STAGE_COMP, .pSource = source, .sourceSize = sizeof(source) - 1, .pEntryPoint = "Read" } },
              .language = hz::ShaderLanguage::Slang,
          } });
    ASSERT_TRUE(write.isValid() && read.isValid());
    const hz::TextureDesc textureDesc = {
        .width = 2,
        .height = 2,
        .mipLevels = 2,
        .format = hz::Format::R8G8B8A8_UNORM,
        .startState = RESOURCE_STATE_UNORDERED_ACCESS,
        .descriptors = DESCRIPTOR_TYPE_TEXTURE | DESCRIPTOR_TYPE_RW_TEXTURE,
    };
    hz::GPUTexture textures[] = { context.createTexture(textureDesc), context.createTexture(textureDesc) };
    hz::GPUSampler sampler = context.createSampler({ .minFilter = FILTER_NEAREST, .magFilter = FILTER_NEAREST });
    const uint32_t configData[64] = { 2 };
    const uint32_t bias = 3;
    hz::GPUBuffer  config = context.createBuffer({
        .size = 256,
        .elementCount = 64,
        .structStride = 4,
        .pInitialData = configData,
        .initialDataSize = sizeof(configData),
        .usage = RESOURCE_MEMORY_USAGE_GPU_ONLY,
        .startState = RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER,
        .descriptors = DESCRIPTOR_TYPE_UNIFORM_BUFFER | DESCRIPTOR_TYPE_RW_BUFFER,
    });
    hz::GPUBuffer  raw = context.createBuffer({
        .size = 4,
        .elementCount = 1,
        .pInitialData = &bias,
        .initialDataSize = sizeof(bias),
        .usage = RESOURCE_MEMORY_USAGE_GPU_ONLY,
        .startState = RESOURCE_STATE_SHADER_RESOURCE,
        .descriptors = DESCRIPTOR_TYPE_BUFFER_RAW,
    });
    hz::GPUBuffer  output = context.createBuffer({
        .size = 32,
        .elementCount = 2,
        .structStride = 16,
        .usage = RESOURCE_MEMORY_USAGE_GPU_ONLY,
        .startState = RESOURCE_STATE_UNORDERED_ACCESS,
        .descriptors = DESCRIPTOR_TYPE_RW_BUFFER,
    });
    hz::GPUBuffer  readback = context.createBuffer({
        .size = 32,
        .usage = RESOURCE_MEMORY_USAGE_GPU_TO_CPU,
        .startState = RESOURCE_STATE_COPY_DEST,
        .flags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT,
    });
    EXPECT_NE(config.getCbvIndex(), config.getUavIndex());
    EXPECT_EQ(textures[0].getUavIndex(1), textures[0].getUavIndex(0) + 1);
    const uint32_t   indices[] = { textures[0].getUavIndex(1), textures[1].getUavIndex(1), textures[0].getSrvIndex(),
                                   textures[1].getSrvIndex(),  sampler.getIndex(),         output.getUavIndex(),
                                   config.getCbvIndex(),       raw.getSrvIndex() };
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
    EXPECT_FALSE(textures[0].isValid());
    EXPECT_EQ(moved.getSrvIndex(), indices[2]);
    moved = {};
    textures[0] = context.createTexture(textureDesc);
    EXPECT_EQ(textures[0].getSrvIndex(), indices[2]);
    EXPECT_EQ(textures[1].getSrvIndex(), indices[3]);
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
