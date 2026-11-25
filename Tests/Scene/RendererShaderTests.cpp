// GPU tests of the Examples/Renderer shaders and passes, built on the shared asset fixtures.
#include "AssetTestFixtures.h"
#include "../../Examples/Renderer/Source/RendererConstants.h"
#include "../../Examples/Renderer/Source/DeferredLighting.h"
#include "../../Examples/Renderer/Source/ClusteredLighting.h"
#include "Scene/GpuDrawList.h"
#include "../../Examples/Renderer/Source/FrustumCulling.h"
#include "../../Examples/Renderer/Source/RayTracingScene.h"
#include <bit>
#include <cmath>
#include <d3d12sdklayers.h>

#define IMEMORY_FROM_HEADER
#include "Core/IMemory.h"

class SlangCacheTest: public SceneAssetLoaderTest
{
protected:
    void TearDown() override
    {
        SetResourceDirectoryPath(SystemFileIO, MountDebug, DirectoryShaderBinaries, "CompiledShaders");
        SceneAssetLoaderTest::TearDown();
    }
};

TEST_F(SlangCacheTest, SlangCacheTracksNestedIncludes)
{
    SetResourceDirectoryPath(&fileSystem, MountProject, DirectoryShaderBinaries, directory.string().c_str());
    const auto writeSource = [](const char* name, const char* source)
    {
        FileStream stream = {};
        ASSERT_TRUE(OpenStreamFromPath(DirectoryOtherFiles, name, FileModeWrite, &stream));
        EXPECT_EQ(WriteToStream(&stream, source, strlen(source)), strlen(source));
        CloseStream(&stream);
    };
    writeSource("Root.slang", R"(#include "Middle.slang"
        cbuffer RootConstant0 { uint outputIndex; };
        [numthreads(1,1,1)] void CSMain() {
            RWStructuredBuffer<uint> result = ResourceDescriptorHeap[outputIndex];
            result[0] = VALUE;
        })");
    writeSource("Middle.slang", "#include \"Leaf.slang\"\n");
    writeSource("Leaf.slang", "#define VALUE 17\n");
    hz::RenderContext context({ .pAppName = "SlangCacheTest", .Width = 0, .Height = 0 });
    hz::GPUBuffer     output = context.CreateBuffer({ .Size = 4,
                                                      .ElementCount = 1,
                                                      .StructStride = 4,
                                                      .usage = RESOURCE_MEMORY_USAGE_GPU_ONLY,
                                                      .StartState = RESOURCE_STATE_UNORDERED_ACCESS,
                                                      .Descriptors = DESCRIPTOR_TYPE_RW_BUFFER });
    hz::GPUBuffer     readback = context.CreateBuffer({ .Size = 4,
                                                        .usage = RESOURCE_MEMORY_USAGE_GPU_TO_CPU,
                                                        .StartState = RESOURCE_STATE_COPY_DEST,
                                                        .Flags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT });
    const auto        checkResult = [&](uint32_t expected)
    {
        hz::GPUPipeline pipeline =
            context.CreateComputePipeline({ .shaderDesc = { .stages = { { .stage = SHADER_STAGE_COMP, .pEntryPoint = "CSMain" } },
                                                            .sourceDirectory = DirectoryOtherFiles,
                                                            .pFileName = "Root.slang" } });
        ASSERT_TRUE(pipeline.IsValid());
        hz::CommandList& commands = context.AcquireCommandList();
        commands.SetPipeline(pipeline);
        commands.SetRootConstant({ output.GetUavIndex() });
        commands.Dispatch(1, 1, 1, { .storageBuffers = { &output } });
        commands.CopyBuffer(readback, 0, output, 0, 4);
        context.Wait(context.Submit(commands));
        EXPECT_EQ(*static_cast<const uint32_t*>(readback.Get()->pCpuMappedAddress), expected);
    };
    checkResult(17);
    std::filesystem::path cache;
    for (const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator(directory))
    {
        if (entry.path().extension() == ".shadercache")
        {
            cache = entry.path();
        }
    }
    ASSERT_FALSE(cache.empty());
    const std::filesystem::file_time_type timestamp = std::filesystem::last_write_time(cache);
    checkResult(17);
    EXPECT_EQ(std::filesystem::last_write_time(cache), timestamp);
    writeSource("Leaf.slang", "#define VALUE 42\n");
    checkResult(42);
}

TEST_F(SceneAssetLoaderTest, RendererMotionUsesPerspectiveCorrectSubmittedTransforms)
{
    const std::filesystem::path shaders =
        std::filesystem::absolute(std::filesystem::path(__FILE__).parent_path().parent_path().parent_path() / "Examples/Renderer/Shaders");
    SetResourceDirectoryPath(&fileSystem, MountProject, DirectoryOtherFiles, shaders.string().c_str());
    hz::RenderContext context({ .pAppName = "RendererMotionTest", .enableGpuValidation = true });
    hz::GPUPipeline   pipeline = context.CreateGraphicsPipeline({
          .shaderDesc = { .stages = { { .stage = SHADER_STAGE_VERT, .pEntryPoint = "VSMain" },
                                      { .stage = SHADER_STAGE_FRAG, .pEntryPoint = "PSMain" } },
                          .sourceDirectory = DirectoryOtherFiles,
                          .pFileName = "Geometry.slang" },
          .colorTargets = { { .Format = hz::Format::R8G8B8A8_UNORM },
                            { .Format = hz::Format::R8G8B8A8_UNORM },
                            { .Format = hz::Format::R8G8B8A8_UNORM },
                            { .Format = hz::Format::R8G8B8A8_UNORM } },
    });
    SetResourceDirectoryPath(&fileSystem, MountProject, DirectoryOtherFiles, directory.string().c_str());
    ASSERT_TRUE(pipeline.IsValid());
    // Project x/z, y/z at constant Depth, with vertex w = 1, 1, 2.
    const Matrix4      projection(Vector4(1, 0, 0, 0), Vector4(0, 1, 0, 0), Vector4(0, 0, 0.5f, 1), Vector4(0));
    const FrameData    frameData = { .ViewProjection = projection,
                                     .PreviousViewProjection = projection,
                                     .InverseViewProjection = Matrix4::identity(),
                                     .Eye = Vector4(0, 0, 0, 1) };
    const float        positions[] = { -0.8f, -0.8f, 1, 0.6f, -0.8f, 1, -0.2f, 1, 2 };
    const uint32_t     normals[] = { 0x80008000, 0x80008000, 0x80008000 }, texcoords[] = { 0, 0, 0 };
    hz::RenderInstance instance;
    instance.current = Matrix4::translation(Vector3(0.2f, 0, 0));
    const hz::GpuMaterialData   material;
    const auto                  input = [&](const void* data, uint32_t Size, uint32_t stride = 0)
    {
        return context.CreateBuffer({ .Size = Size,
                                      .ElementCount = stride ? Size / stride : Size / 4,
                                      .StructStride = stride,
                                      .pInitialData = data,
                                      .initialDataSize = Size,
                                      .usage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU,
                                      .StartState = RESOURCE_STATE_SHADER_RESOURCE,
                                      .Descriptors = stride ? DESCRIPTOR_TYPE_BUFFER : DESCRIPTOR_TYPE_BUFFER_RAW });
    };
    hz::GPUBuffer  frame = input(&frameData, sizeof(frameData), sizeof(frameData));
    hz::GPUBuffer  instances = input(&instance, sizeof(instance)), materials = input(&material, sizeof(material));
    hz::GPUBuffer  vertices = input(positions, sizeof(positions)), normal = input(normals, sizeof(normals));
    hz::GPUBuffer  uv = input(texcoords, sizeof(texcoords));
    hz::GPUSampler sampler = context.CreateSampler();
    hz::GPUTexture targets[4];
    for (hz::GPUTexture& target : targets)
        target = context.CreateTexture({ .Width = 64,
                                         .Height = 64,
                                         .Format = hz::Format::R8G8B8A8_UNORM,
                                         .StartState = RESOURCE_STATE_RENDER_TARGET,
                                         .Descriptors = DESCRIPTOR_TYPE_TEXTURE,
                                         .GetRenderTarget = true });
    const char      readPixel[] = R"hlsl(
        cbuffer RootConstant0 { uint textureIndex; uint outputIndex; };
        [numthreads(1,1,1)] void CSMain() {
            Texture2D<float4> motion = ResourceDescriptorHeap[textureIndex];
            RWStructuredBuffer<uint4> output = ResourceDescriptorHeap[outputIndex];
            output[0] = uint4(round(motion.Load(int3(32,32,0)) * 255.0));
        }
    )hlsl";
    hz::GPUPipeline reader = context.CreateComputePipeline(
        { .shaderDesc = {
              .stages = {
                  { .stage = SHADER_STAGE_COMP, .pSource = readPixel, .sourceSize = sizeof(readPixel) - 1, .pEntryPoint = "CSMain" } } } });
    hz::GPUBuffer output = context.CreateBuffer({ .Size = 16,
                                                  .ElementCount = 1,
                                                  .StructStride = 16,
                                                  .usage = RESOURCE_MEMORY_USAGE_GPU_ONLY,
                                                  .StartState = RESOURCE_STATE_UNORDERED_ACCESS,
                                                  .Descriptors = DESCRIPTOR_TYPE_RW_BUFFER });
    hz::GPUBuffer readback = context.CreateBuffer({ .Size = 16,
                                                    .usage = RESOURCE_MEMORY_USAGE_GPU_TO_CPU,
                                                    .StartState = RESOURCE_STATE_COPY_DEST,
                                                    .Flags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT });
    ASSERT_TRUE(reader.IsValid() && sampler.IsValid());
    hz::CommandList& commands = context.AcquireCommandList();
    commands.BeginRendering({ .colorAttachments = {
        { .pTexture = &targets[0], .loadAction = LOAD_ACTION_CLEAR, .storeAction = STORE_ACTION_STORE },
        { .pTexture = &targets[1], .loadAction = LOAD_ACTION_CLEAR, .storeAction = STORE_ACTION_STORE },
        { .pTexture = &targets[2], .loadAction = LOAD_ACTION_CLEAR, .storeAction = STORE_ACTION_STORE },
        { .pTexture = &targets[3], .loadAction = LOAD_ACTION_CLEAR, .storeAction = STORE_ACTION_STORE },
    } }, { .buffers = { &frame, &instances, &materials, &vertices, &normal, &uv } });
    commands.SetViewport(0, 0, 64, 64);
    commands.SetScissor(0, 0, 64, 64);
    commands.SetPipeline(pipeline);
    commands.SetRootConstant({ 0, 0, 7, frame.GetSrvIndex(), instances.GetSrvIndex(), materials.GetSrvIndex(), UINT32_MAX,
                               sampler.GetIndex(), vertices.GetSrvIndex(), normal.GetSrvIndex(), uv.GetSrvIndex() });
    commands.Draw(3);
    commands.EndRendering();
    commands.SetPipeline(reader);
    commands.SetRootConstant({ targets[3].GetSrvIndex(), output.GetUavIndex() });
    commands.Dispatch(1, 1, 1, { .sampledTextures = { &targets[3] }, .storageBuffers = { &output } });
    commands.CopyBuffer(readback, 0, output, 0, 16);
    context.Wait(context.Submit(commands));
    const uint32_t* pixel = (const uint32_t*)readback.Get()->pCpuMappedAddress;
    ASSERT_EQ(pixel[3], 7u); // The sampled pixel was covered by the triangle.
    const uint32_t packed = pixel[0] | (pixel[1] << 8) | (pixel[2] << 16);
    const float    motion = (float)(packed & 0xfff) / 4095.0f * 2 - 1;
    const float    topWeight = ((1 - 2 * 32.5f / 64) + 0.8f) / 1.3f;
    const float    expected = 0.2f * (1 - topWeight * 0.5f) * 0.5f;
    EXPECT_NEAR(motion, expected, 1.0f / 4095.0f + 1e-5f);
    EXPECT_NEAR((float)(packed >> 12) / 4095.0f * 2 - 1, 0, 1.0f / 4095.0f + 1e-5f);
}

TEST_F(SceneAssetLoaderTest, RendererGpuCullingFillsFilteredDrawLists)
{
    const std::filesystem::path shaders =
        std::filesystem::absolute(std::filesystem::path(__FILE__).parent_path().parent_path().parent_path() / "Examples/Renderer/Shaders");
    SetResourceDirectoryPath(&fileSystem, MountProject, DirectoryOtherFiles, shaders.string().c_str());
    hz::RenderContext context({ .pAppName = "RendererCullingTest", .enableGpuValidation = true });
    hz::GPUPipeline   pipeline =
        context.CreateComputePipeline({ .shaderDesc = { .stages = { { .stage = SHADER_STAGE_COMP, .pEntryPoint = "CSMain" } },
                                                        .sourceDirectory = DirectoryOtherFiles,
                                                        .pFileName = "FrustumCulling.slang" } });
    hz::GPUPipeline Clear =
        context.CreateComputePipeline({ .shaderDesc = { .stages = { { .stage = SHADER_STAGE_COMP, .pEntryPoint = "ClearStatistics" } },
                                                        .sourceDirectory = DirectoryOtherFiles,
                                                        .pFileName = "FrustumCulling.slang" } });
    SetResourceDirectoryPath(&fileSystem, MountProject, DirectoryOtherFiles, directory.string().c_str());
    ASSERT_TRUE(pipeline.IsValid() && Clear.IsValid());
    FrameData          camera = { .Eye = Vector4(0, 0, 0, 1) };
    hz::RenderInstance instances[5];
    const float        bounds[5][6] = { { -.2f, -.2f, .2f, .2f, .2f, .8f },
                                        { 1.1f, 0, .2f, 1.5f, .2f, .8f },
                                        { 0, 0, -.8f, .2f, .2f, -.2f },
                                        { -.2f, -.2f, .2f, .2f, .2f, .8f },
                                        { -2, -2, -1, 2, 2, 2 } };
    uint32_t           drawData[5][12] = {};
    // Draw i uses material i; materials 1 and 4 carry tag bit 2.
    hz::GpuMaterialData materials[5] = {};
    for (uint32_t i = 0; i < 5; ++i)
    {
        memcpy(instances[i].bounds.minimum, bounds[i], 12);
        memcpy(instances[i].bounds.maximum, bounds[i] + 3, 12);
        drawData[i][0] = i;
        drawData[i][1] = i;
        drawData[i][3] = i == 3 ? 2u : 0u;
        drawData[i][11] = 3 * (i + 1);
        materials[i].Tags = i == 1 || i == 4 ? 4u : 0u;
    }
    const auto input = [&](const void* data, uint32_t Size, uint32_t stride = 0)
    {
        return context.CreateBuffer({ .Size = Size,
                                      .ElementCount = stride ? Size / stride : Size / 4,
                                      .StructStride = stride,
                                      .pInitialData = data,
                                      .initialDataSize = Size,
                                      .usage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU,
                                      .StartState = RESOURCE_STATE_SHADER_RESOURCE,
                                      .Descriptors = stride ? DESCRIPTOR_TYPE_BUFFER : DESCRIPTOR_TYPE_BUFFER_RAW,
                                      .Flags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT });
    };
    hz::GPUBuffer frame = input(&camera, sizeof(camera), sizeof(camera));
    hz::GPUBuffer instanceBuffer = input(instances, sizeof(instances));
    hz::GPUBuffer drawBuffer = input(drawData, sizeof(drawData));
    hz::GPUBuffer materialBuffer = input(materials, sizeof(materials));
    constexpr uint32_t ListCount = 4;
    hz::GPUBuffer outputs[ListCount];
    for (hz::GPUBuffer& buffer : outputs)
    {
        buffer = context.CreateBuffer({ .Size = 100,
                                        .ElementCount = 25,
                                        .usage = RESOURCE_MEMORY_USAGE_GPU_ONLY,
                                        .StartState = RESOURCE_STATE_UNORDERED_ACCESS,
                                        .Descriptors = DESCRIPTOR_TYPE_RW_BUFFER_RAW });
    }
    hz::GPUBuffer      statistics = context.CreateBuffer({ .Size = 16,
                                                           .ElementCount = 4,
                                                           .usage = RESOURCE_MEMORY_USAGE_GPU_ONLY,
                                                           .StartState = RESOURCE_STATE_UNORDERED_ACCESS,
                                                           .Descriptors = DESCRIPTOR_TYPE_RW_BUFFER_RAW });
    // Opaque and masked, blended, uncullable casters, and opaque draws whose material has tag bit 2.
    const uint32_t opaqueModes = hz::DrawListAlphaMode(hz::MaterialAlphaMode::Opaque) | hz::DrawListAlphaMode(hz::MaterialAlphaMode::Mask);
    hz::DrawListFilter filters[ListCount] = { { .AlphaModes = opaqueModes },
                                          { .AlphaModes = hz::DrawListAlphaMode(hz::MaterialAlphaMode::Blend) },
                                          { .AlphaModes = opaqueModes, .FrustumCull = 0 },
                                          { .AlphaModes = opaqueModes, .RequiredTags = 4 } };
    for (uint32_t i = 0; i < ListCount; ++i)
        filters[i].CommandsIndex = outputs[i].GetUavIndex();
    hz::GPUBuffer  filterBuffer = input(filters, sizeof(filters), sizeof(hz::DrawListFilter));
    hz::GPUBuffer  readback = context.CreateBuffer({ .Size = ListCount * 100 + 16,
                                                     .usage = RESOURCE_MEMORY_USAGE_GPU_TO_CPU,
                                                     .StartState = RESOURCE_STATE_COPY_DEST,
                                                     .Flags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT });
    // With a 90-degree frustum, the first camera sees centers (3,0,-2) and (4,0,-2), while (12,0,-2) is outside right.
    // Moving to (9,0,2) and looking diagonally toward +X/-Z makes that outside box visible and leaves both old boxes behind.
    const float    perspectiveBounds[5][6] = { { 2.75f, -.25f, -2.25f, 3.25f, .25f, -1.75f },
                                               { 11.75f, -.25f, -2.25f, 12.25f, .25f, -1.75f },
                                               { 2.75f, -.25f, 3.75f, 3.25f, .25f, 4.25f },
                                               { 3.75f, -.25f, -2.25f, 4.25f, .25f, -1.75f },
                                               { -20, -20, -20, 30, 20, 20 } };
    const uint32_t visibleMasks[] = { 0x19, 0x19, 0x12 }, triangleCounts[] = { 10, 10, 7 };
    for (uint32_t cameraCase = 0; cameraCase < 3; ++cameraCase)
    {
        SCOPED_TRACE(cameraCase);
        if (cameraCase)
        {
            for (uint32_t i = 0; i < 5; ++i)
            {
                memcpy(instances[i].bounds.minimum, perspectiveBounds[i], 12);
                memcpy(instances[i].bounds.maximum, perspectiveBounds[i] + 3, 12);
            }
            const Matrix4 pose = cameraCase == 1 ? Matrix4::translation(Vector3(3, 0, 2))
                                                 : Matrix4::translation(Vector3(9, 0, 2)) * Matrix4::rotationY(-PI / 4.0f);
            camera.ViewProjection = Matrix4::perspectiveRH(PI / 2.0f, 1.0f, 0.1f, 10.0f) * inverse(pose);
            camera.PreviousViewProjection = camera.ViewProjection;
            camera.InverseViewProjection = inverse(camera.ViewProjection);
            camera.Eye = pose.getCol3();
            ASSERT_TRUE(context.UpdateBuffer(frame, 0, &camera, sizeof(camera)));
            ASSERT_TRUE(context.UpdateBuffer(instanceBuffer, 0, instances, sizeof(instances)));
        }
        for (bool culling : { true, false })
        {
            SCOPED_TRACE(culling);
            hz::CommandList& commands = context.AcquireCommandList();
            commands.SetPipeline(Clear);
            commands.SetRootConstant({ statistics.GetUavIndex() }, 6);
            commands.Dispatch(1, 1, 1, { .storageBuffers = { &statistics } });
            commands.SetPipeline(pipeline);
            commands.SetRootConstant({ frame.GetSrvIndex(), drawBuffer.GetSrvIndex(), instanceBuffer.GetSrvIndex(),
                                       materialBuffer.GetSrvIndex(), filterBuffer.GetSrvIndex(), ListCount, statistics.GetUavIndex(), 5,
                                       culling ? 1u : 0u });
            commands.Dispatch(1, 1, 1,
                              { .buffers = { &frame, &drawBuffer, &instanceBuffer, &materialBuffer, &filterBuffer },
                                .storageBuffers = { &outputs[0], &outputs[1], &outputs[2], &outputs[3], &statistics } });
            for (uint32_t i = 0; i < ListCount; ++i)
            {
                commands.CopyBuffer(readback, i * 100, outputs[i], 0, 100);
            }
            commands.CopyBuffer(readback, ListCount * 100, statistics, 0, 16);
            context.Wait(context.Submit(commands));
            const uint32_t* data = (const uint32_t*)readback.Get()->pCpuMappedAddress;
            ASSERT_NE(data, nullptr);
            for (uint32_t i = 0; i < 5; ++i)
            {
                const bool visible = !culling || (visibleMasks[cameraCase] & (1u << i));
                const bool opaque = i != 3, tagged = i == 1 || i == 4;
                const bool expected[ListCount] = { visible && opaque, visible && !opaque, opaque, visible && opaque && tagged };
                for (uint32_t stream = 0; stream < ListCount; ++stream)
                {
                    const uint32_t* args = data + stream * 25 + i * 5;
                    EXPECT_EQ(args[0], i);
                    EXPECT_EQ(args[1], 3 * (i + 1));
                    EXPECT_EQ(args[2], uint32_t(expected[stream])) << "list " << stream;
                    EXPECT_EQ(args[3], 0u);
                    EXPECT_EQ(args[4], 0u);
                }
            }
            EXPECT_EQ(data[ListCount * 25], culling ? (cameraCase == 2 ? 2u : 3u) : 5u);
            EXPECT_EQ(data[ListCount * 25 + 1], culling ? triangleCounts[cameraCase] : 15u);
        }
    }
}

TEST_F(TypedAssetLiveTest, RayTracingSceneUsesUnculledDrawIdsAndSharesGeometry)
{
    std::string       text = kTypedScene;
    const size_t      materialStart = text.find("[", text.find("\"materials\"")) + 1;
    const size_t      materialEnd = text.rfind("]", text.find("\"meshes\""));
    std::string       blend = text.substr(materialStart, materialEnd - materialStart);
    const std::string originalID = "00000000-0000-4000-8000-000000000003";
    blend.replace(blend.find(originalID), originalID.size(), "00000000-0000-4000-8000-000000000006");
    blend.replace(blend.find("\"alphaMode\":1"), 13, "\"alphaMode\":2");
    text.insert(materialEnd, "," + blend);
    writeTexture();
    writeScene(text.c_str());
    writeGeometry(INDEX_TYPE_UINT16, true);
    hz::SceneAsset asset;
    ASSERT_TRUE(asset.Parse({ text.data(), (uint32_t)text.size() }));
    hz::AssetCatalog catalog;
    ASSERT_TRUE(catalog.AddScene(asset, "scene.sceneasset.json"));
    hz::RenderContext context({ .pAppName = "RayTracingSceneTest", .enableGpuValidation = true });
    if (!context.SupportsRayTracing())
    {
        GTEST_SKIP() << "Selected device does not support ray tracing";
    }
    const VertexLayout layout = {
        .bindings = { { .stride = 12 }, { .stride = 4 }, { .stride = 4 } },
        .attribs = {
            { .semantic = SEMANTIC_POSITION, .Format = hz::Format::R32G32B32_SFLOAT, .binding = 0 },
            { .semantic = SEMANTIC_NORMAL, .Format = hz::Format::R32_UINT, .binding = 1 },
            { .semantic = SEMANTIC_TEXCOORD0, .Format = hz::Format::R32_UINT, .binding = 2 },
        },
        .bindingCount = 3, .attribCount = 3,
    };

    hz::AssetLoaders loaders(context, { .vertexLayout = layout, .cookedDirectory = DirectoryOtherFiles });
    hz::AssetManager assetManager(catalog, &context);
    loaders.RegisterLoaders(assetManager);
    const hz::AssetHandle root = assetManager.Request(asset.GetID());
    Wait(assetManager, root);
    ASSERT_EQ(assetManager.GetState(root), hz::AssetState::Ready);

    hz::TypeRegistry types;
    ASSERT_TRUE(types.RegisterBuiltins());
    hz::World       world(types);
    hz::RenderScene scene(world, assetManager);
    hz::GpuScene    gpu(scene, assetManager, context);
    const float     offsets[] = { 0, 10, 20, 40 };
    hz::Entity      entities[4];
    for (uint32_t i = 0; i < 4; ++i)
    {
        entities[i] = world.CreateEntity();
        hz::MeshRenderer renderer = { .mesh = asset.GetMeshes().Data[i == 3 ? 1 : 0].id };
        if (i == 1)
        {
            renderer.overrides.PushBack({ .submesh = UINT64_C(0x8000000000000001), .material = asset.GetMaterials().Data[1].id });
        }
        const Matrix4 transform = Matrix4::translation(Vector3(offsets[i], 0, 0));
        ASSERT_TRUE(world.SetComponent(entities[i], hz::SceneTypes::meshRenderer, &renderer));
        ASSERT_TRUE(world.SetComponent(entities[i], hz::SceneTypes::localMatrix, &transform));
    }

    scene.Update();
    const hz::AssetHandle blendMaterial = assetManager.Find(asset.GetMaterials().Data[1].id);
    ASSERT_TRUE(blendMaterial.IsValid());
    Wait(assetManager, blendMaterial);
    ASSERT_EQ(assetManager.GetState(blendMaterial), hz::AssetState::Ready);
    const std::filesystem::path shaders =
        std::filesystem::absolute(std::filesystem::path(__FILE__).parent_path().parent_path().parent_path() / "Examples/Renderer/Shaders");
    SetResourceDirectoryPath(&fileSystem, MountProject, DirectoryShaderSources, shaders.string().c_str());

    hz::GpuDrawList     draws(context);
    FrustumCulling  culling(context);
    RayTracingScene rayScene(context);

    const char      source[] = R"hlsl(
        cbuffer RootConstant0 { uint topIndex; uint outputIndex; };
        [numthreads(4,1,1)] void CSMain(uint3 id : SV_DispatchThreadID) {
            RaytracingAccelerationStructure scene = ResourceDescriptorHeap[topIndex];
            RWStructuredBuffer<uint> output = ResourceDescriptorHeap[outputIndex];
            float offsets[4] = { 0, 10, 20, 40 };
            RayDesc ray;
            ray.Origin = float3(1 + offsets[id.x], -2.0 / 3.0, -10);
            ray.Direction = float3(0, 0, 1); ray.TMin = 0.001; ray.TMax = 30;
            RayQuery<RAY_FLAG_FORCE_OPAQUE> query;
            query.TraceRayInline(scene, RAY_FLAG_NONE, 0xff, ray);
            while (query.Proceed()) {}
            output[id.x] = query.CommittedStatus() == COMMITTED_TRIANGLE_HIT ? query.CommittedInstanceID() : 0xffffffff;
        }
    )hlsl";
    hz::GPUPipeline trace = context.CreateComputePipeline(
        { .shaderDesc = {
              .stages = {
                  { .stage = SHADER_STAGE_COMP, .pSource = source, .sourceSize = sizeof(source) - 1, .pEntryPoint = "CSMain" } } } });
    ASSERT_TRUE(trace.IsValid());

    const FrameData camera = { .Eye = Vector4(0, 0, -10, 1) };
    hz::GPUBuffer frame = context.CreateBuffer({ .Size = sizeof(camera),
                                                 .ElementCount = 1,
                                                 .StructStride = sizeof(camera),
                                                 .pInitialData = &camera,
                                                 .initialDataSize = sizeof(camera),
                                                 .usage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU,
                                                 .StartState = RESOURCE_STATE_SHADER_RESOURCE,
                                                 .Descriptors = DESCRIPTOR_TYPE_BUFFER });
    hz::GPUBuffer output = context.CreateBuffer({ .Size = 16,
                                                  .ElementCount = 4,
                                                  .StructStride = 4,
                                                  .usage = RESOURCE_MEMORY_USAGE_GPU_ONLY,
                                                  .StartState = RESOURCE_STATE_UNORDERED_ACCESS,
                                                  .Descriptors = DESCRIPTOR_TYPE_RW_BUFFER });
    hz::GPUBuffer readback = context.CreateBuffer({ .Size = 96,
                                                    .usage = RESOURCE_MEMORY_USAGE_GPU_TO_CPU,
                                                    .StartState = RESOURCE_STATE_COPY_DEST,
                                                    .Flags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT });

    for (uint32_t pass = 0; pass < 4; ++pass)
    {
        if (pass == 1)
        {
            ASSERT_TRUE(world.DestroyEntity(entities[0]));
        }
        if (pass == 2)
        {
            ASSERT_TRUE(world.DestroyEntity(entities[2]));
        }
        if (pass == 3)
        {
            ASSERT_TRUE(world.DestroyEntity(entities[1]));
            ASSERT_TRUE(world.DestroyEntity(entities[3]));
        }

        ASSERT_TRUE(gpu.BeginFrame());
        const hz::SceneView  view = { .Scene = scene, .GpuScene = gpu, .Assets = assetManager, .FrameIndex = pass % 3 };
        hz::CommandList& commands = context.AcquireCommandList();

        gpu.Upload(commands);
        draws.Prepare(view);
        culling.Execute(commands, { .Buffer = &frame, .Index = frame.GetSrvIndex() }, view, draws);
        rayScene.Update(commands, view, draws);
        EXPECT_EQ(rayScene.GetSceneTLAS().GetInstanceCount(), pass < 3 ? 3 - pass : 0);
        EXPECT_EQ(draws.GetGeometries().Count, pass < 2 ? 2u : (pass == 2 ? 1u : 0u));
        uint32_t expected[] = { UINT32_MAX, UINT32_MAX, UINT32_MAX, UINT32_MAX };
        if (pass < 3)
        {
            const hz::Span<hz::GpuDrawData> prepared = draws.GetDraws();
            EXPECT_EQ(prepared.Count, 4 - pass);
            for (uint32_t i = 0; i < prepared.Count; ++i)
            {
                const hz::GpuDrawData& Draw = prepared.Data[i];
                const float        x = (float)scene.GetInstances().Data[Draw.Instance].current.getTranslation().getX();
                for (uint32_t j = 0; j < 4; ++j)
                {
                    if (x == offsets[j] && Draw.AlphaMode != uint32_t(hz::MaterialAlphaMode::Blend))
                    {
                        expected[j] = i;
                    }
                }
            }
            const hz::Dependencies sceneDependencies = draws.GetDependencies(frame, view);
            commands.SetPipeline(trace);
            commands.SetRootConstant({ rayScene.GetSceneTLAS().GetSrvIndex(), output.GetUavIndex() });
            commands.Dispatch(1, 1, 1, { .storageBuffers = { &output }, .pShared = &sceneDependencies });
            commands.CopyBuffer(readback, 0, output, 0, 16);
            commands.CopyBuffer(readback, 16, draws.GetCommands(view.FrameIndex, draws.GetOpaqueList()), 0, prepared.Count * 20);
        }

        const hz::SubmitHandle submission = context.Submit(commands);
        gpu.EndFrame(submission);
        context.Wait(submission);
        if (pass < 3)
        {
            const uint32_t* values = (const uint32_t*)readback.Get()->pCpuMappedAddress;
            for (uint32_t i = 0; i < 4; ++i)
            {
                EXPECT_EQ(values[i], expected[i]) << "pass " << pass << ", ray " << i;
            }
            ASSERT_NE(expected[3], UINT32_MAX);
            EXPECT_EQ(values[4 + expected[3] * 5 + 2], 0u); // Offscreen raster Draw still received a ray hit.
        }
        else
        {
            EXPECT_FALSE(rayScene.GetSceneTLAS().IsValid());
        }
    }
}

TEST_F(TypedAssetLiveTest, IndirectDrawIdsSelectDistinctGeometryAndReadThreeUint16Indices)
{
    writeTexture();
    writeScene();
    writeGeometry(INDEX_TYPE_UINT16);
    hz::SceneAsset asset;
    ASSERT_TRUE(asset.Parse(kTypedScene));
    hz::AssetCatalog catalog;
    ASSERT_TRUE(catalog.AddScene(asset, "scene.sceneasset.json"));
    hz::RenderContext context({ .pAppName = "IndirectDrawIdTest", .enableGpuValidation = true });
    hz::AssetLoaders  loaders(context, { .vertexLayout = kAssetTestLayout, .cookedDirectory = DirectoryOtherFiles });
    hz::AssetManager  assetManager(catalog, &context);
    loaders.RegisterLoaders(assetManager);
    const hz::AssetHandle Handle = assetManager.Request(asset.GetMeshes().Data[0].id);
    Wait(assetManager, Handle);
    ASSERT_EQ(assetManager.GetState(Handle), hz::AssetState::Ready);
    const hz::MeshAsset* mesh = assetManager.GetMesh(Handle);
    ASSERT_NE(mesh, nullptr);
    ASSERT_EQ(mesh->GetIndexType(), INDEX_TYPE_UINT16);
    // Three indices consume six bytes; RAW loads require a second complete DWORD.
    ASSERT_EQ(mesh->GetIndexBuffer().GetSize(), 8u);
    const std::filesystem::path shaders =
        std::filesystem::absolute(std::filesystem::path(__FILE__).parent_path().parent_path().parent_path() / "Examples/Renderer/Shaders");
    SetResourceDirectoryPath(&fileSystem, MountProject, DirectoryOtherFiles, shaders.string().c_str());
    hz::GPUPipeline pipeline = context.CreateGraphicsPipeline({
        .shaderDesc = { .stages = { { .stage = SHADER_STAGE_VERT, .pEntryPoint = "VSIndirect" },
                                    { .stage = SHADER_STAGE_FRAG, .pEntryPoint = "PSMain" } },
                        .sourceDirectory = DirectoryOtherFiles,
                        .pFileName = "Geometry.slang" },
        .colorTargets = { { .Format = hz::Format::R8G8B8A8_UNORM },
                          { .Format = hz::Format::R8G8B8A8_UNORM },
                          { .Format = hz::Format::R8G8B8A8_UNORM },
                          { .Format = hz::Format::R8G8B8A8_UNORM } },
        .indirectDrawId = true,
    });
    SetResourceDirectoryPath(&fileSystem, MountProject, DirectoryOtherFiles, directory.string().c_str());
    ASSERT_TRUE(pipeline.IsValid());
    const FrameData frameData = { .Eye = Vector4(0, 0, -2, 1) };
    hz::RenderInstance instances[2];
    instances[0].primitive = { 2, 5 };
    instances[1].primitive = { 4, 8 };
    instances[0].id = { 0, 9 };
    instances[1].id = { 1, 15 };
    instances[0].current = instances[0].previous = Matrix4::translation(Vector3(-0.5f, 0, 0));
    instances[1].current = instances[1].previous = Matrix4::translation(Vector3(0.5f, 0, 0));
    hz::GpuMaterialData materials[2];
    materials[0].BaseColorFactor[1] = materials[0].BaseColorFactor[2] = 0;
    materials[1].BaseColorFactor[0] = materials[1].BaseColorFactor[2] = 0;
    materials[0].MetallicFactor = materials[1].MetallicFactor = 0;
    const float    positions[] = { -0.3f, -0.5f, 0.5f, 0.3f, -0.5f, 0.5f, 0, 0.5f, 0.5f };
    const uint32_t normals[] = { 0x80008000, 0x80008000, 0x80008000 }, uvs[] = { 0, 0, 0 };
    const auto     input = [&](const void* data, uint32_t Size, uint32_t stride = 0)
    {
        return context.CreateBuffer({ .Size = Size,
                                      .ElementCount = stride ? Size / stride : Size / 4,
                                      .StructStride = stride,
                                      .pInitialData = data,
                                      .initialDataSize = Size,
                                      .usage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU,
                                      .StartState = RESOURCE_STATE_SHADER_RESOURCE,
                                      .Descriptors = stride ? DESCRIPTOR_TYPE_BUFFER : DESCRIPTOR_TYPE_BUFFER_RAW });
    };
    hz::GPUBuffer frame = input(&frameData, sizeof(frameData), sizeof(frameData));
    hz::GPUBuffer instanceBuffer = input(instances, sizeof(instances)), materialBuffer = input(materials, sizeof(materials));
    hz::GPUBuffer vertices = input(positions, sizeof(positions)), normal = input(normals, sizeof(normals)), uv = input(uvs, sizeof(uvs));
    struct DrawData
    {
        uint32_t Instance, Material, MaterialId, AlphaMode, Positions, Normals, Texcoords, Indices;
        uint32_t IndexStride, FirstIndex;
        int32_t  VertexOffset;
        uint32_t IndexCount;
    };
    DrawData drawData[2] = { { .Instance = 0,
                               .Material = 0,
                               .MaterialId = 17,
                               .AlphaMode = 0,
                               .Positions = vertices.GetSrvIndex(),
                               .Normals = normal.GetSrvIndex(),
                               .Texcoords = uv.GetSrvIndex(),
                               .Indices = mesh->GetIndexBuffer().GetSrvIndex(),
                               .IndexStride = 2,
                               .FirstIndex = 0,
                               .VertexOffset = 0,
                               .IndexCount = 3 } };
    drawData[1] = drawData[0];
    drawData[1].Instance = drawData[1].Material = 1;
    drawData[1].MaterialId = 39;
    hz::GPUBuffer draws = input(drawData, sizeof(drawData));
    struct DrawCommand
    {
        uint32_t drawId, vertexCount, instanceCount, firstVertex, firstInstance;
    };
    static_assert(sizeof(DrawCommand) == 20);
    const DrawCommand arguments[] = { { 0, 3, 1, 0, 0 }, { 1, 3, 1, 0, 0 } };
    hz::GPUBuffer     indirect = context.CreateBuffer({ .Size = sizeof(arguments),
                                                        .pInitialData = arguments,
                                                        .initialDataSize = sizeof(arguments),
                                                        .usage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU,
                                                        .StartState = RESOURCE_STATE_INDIRECT_ARGUMENT,
                                                        .Descriptors = DESCRIPTOR_TYPE_INDIRECT_BUFFER });
    hz::GPUSampler    sampler = context.CreateSampler();
    hz::GPUTexture    targets[4];
    for (hz::GPUTexture& target : targets)
    {
        target = context.CreateTexture({ .Width = 64,
                                         .Height = 64,
                                         .Format = hz::Format::R8G8B8A8_UNORM,
                                         .StartState = RESOURCE_STATE_RENDER_TARGET,
                                         .Descriptors = DESCRIPTOR_TYPE_TEXTURE,
                                         .GetRenderTarget = true });
    }
    const char      readPixel[] = R"hlsl(
        cbuffer RootConstant0 { uint textureIndex; uint outputIndex; };
        [numthreads(1,1,1)] void CSMain() {
            Texture2D<float4> source = ResourceDescriptorHeap[textureIndex];
            RWStructuredBuffer<uint4> output = ResourceDescriptorHeap[outputIndex];
            output[0] = uint4(round(source.Load(int3(16,32,0)) * 255));
            output[1] = uint4(round(source.Load(int3(48,32,0)) * 255));
        }
    )hlsl";
    hz::GPUPipeline reader = context.CreateComputePipeline(
        { .shaderDesc = {
              .stages = {
                  { .stage = SHADER_STAGE_COMP, .pSource = readPixel, .sourceSize = sizeof(readPixel) - 1, .pEntryPoint = "CSMain" } } } });
    hz::GPUBuffer    output = context.CreateBuffer({ .Size = 32,
                                                     .ElementCount = 2,
                                                     .StructStride = 16,
                                                     .usage = RESOURCE_MEMORY_USAGE_GPU_ONLY,
                                                     .StartState = RESOURCE_STATE_UNORDERED_ACCESS,
                                                     .Descriptors = DESCRIPTOR_TYPE_RW_BUFFER });
    hz::GPUBuffer    readback = context.CreateBuffer({ .Size = 32,
                                                       .usage = RESOURCE_MEMORY_USAGE_GPU_TO_CPU,
                                                       .StartState = RESOURCE_STATE_COPY_DEST,
                                                       .Flags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT });
    hz::CommandList& commands = context.AcquireCommandList();
    commands.BeginRendering({ .colorAttachments = {
        { .pTexture = &targets[0], .loadAction = LOAD_ACTION_CLEAR, .storeAction = STORE_ACTION_STORE },
        { .pTexture = &targets[1], .loadAction = LOAD_ACTION_CLEAR, .storeAction = STORE_ACTION_STORE },
        { .pTexture = &targets[2], .loadAction = LOAD_ACTION_CLEAR, .storeAction = STORE_ACTION_STORE },
        { .pTexture = &targets[3], .loadAction = LOAD_ACTION_CLEAR, .storeAction = STORE_ACTION_STORE },
    } }, { .buffers = { &frame, &instanceBuffer, &materialBuffer, &vertices, &normal, &uv, &draws, &mesh->GetIndexBuffer() } });
    commands.SetViewport(0, 0, 64, 64);
    commands.SetScissor(0, 0, 64, 64);
    commands.SetPipeline(pipeline);
    uint32_t constants[27] = {};
    constants[3] = frame.GetSrvIndex();
    constants[4] = instanceBuffer.GetSrvIndex();
    constants[5] = materialBuffer.GetSrvIndex();
    constants[6] = UINT32_MAX;
    constants[7] = sampler.GetIndex();
    constants[11] = draws.GetSrvIndex();
    commands.SetRootConstant({ constants, 27 });
    commands.DrawIndirect(indirect, 0, 2);
    commands.EndRendering();
    commands.SetPipeline(reader);
    commands.SetRootConstant({ targets[2].GetSrvIndex(), output.GetUavIndex() });
    commands.Dispatch(1, 1, 1, { .sampledTextures = { &targets[2] }, .storageBuffers = { &output } });
    commands.CopyBuffer(readback, 0, output, 0, 32);
    context.Wait(context.Submit(commands));
    const uint32_t* pixels = (const uint32_t*)readback.Get()->pCpuMappedAddress;
    ASSERT_NE(pixels, nullptr);
    EXPECT_EQ(pixels[0], 255u);
    EXPECT_EQ(pixels[1], 0u);
    EXPECT_EQ(pixels[2], 0u);
    EXPECT_EQ(pixels[4], 0u);
    EXPECT_EQ(pixels[5], 255u);
    EXPECT_EQ(pixels[6], 0u);

    SetResourceDirectoryPath(&fileSystem, MountProject, DirectoryOtherFiles, shaders.string().c_str());
    hz::GPUPipeline pick =
        context.CreateGraphicsPipeline({ .shaderDesc = { .stages = { { .stage = SHADER_STAGE_VERT, .pEntryPoint = "VSIndirect" },
                                                                     { .stage = SHADER_STAGE_FRAG, .pEntryPoint = "PickPS" } },
                                                         .sourceDirectory = DirectoryOtherFiles,
                                                         .pFileName = "InspectorGeometry.slang" },
                                         .Depth = { .depthTest = true, .depthWrite = true, .depthFunc = CMP_LEQUAL },
                                         .colorTargets = { { .Format = hz::Format::R32G32_UINT } },
                                         .depthStencilFormat = hz::Format::D24_UNORM_S8_UINT,
                                         .indirectDrawId = true });
    hz::GPUPipeline maskPipeline = context.CreateGraphicsPipeline({
        .shaderDesc = { .stages = { { .stage = SHADER_STAGE_VERT, .pEntryPoint = "MaskVS" },
                                    { .stage = SHADER_STAGE_FRAG, .pEntryPoint = "MaskPS" } },
                        .sourceDirectory = DirectoryOtherFiles, .pFileName = "InspectorGeometry.slang" },
        .Depth = { .depthTest = true, .depthWrite = true, .depthFunc = CMP_LEQUAL }, .colorTargets = { { .Format = hz::Format::R8_UNORM } },
        .depthStencilFormat = hz::Format::D24_UNORM_S8_UINT, .indirectDrawId = true });
    ASSERT_TRUE(maskPipeline.IsValid());
    const std::filesystem::path editorShaders = shaders.parent_path().parent_path().parent_path() / "Source/Editor/Shaders";
    SetResourceDirectoryPath(&fileSystem, MountProject, DirectoryOtherFiles, editorShaders.string().c_str());
    hz::GPUPipeline readPick =
        context.CreateComputePipeline({ .shaderDesc = { .stages = { { .stage = SHADER_STAGE_COMP, .pEntryPoint = "CSMain" } },
                                                        .sourceDirectory = DirectoryOtherFiles,
                                                        .pFileName = "ViewportPickReadback.slang" } });
    ASSERT_TRUE(pick.IsValid() && readPick.IsValid());
    hz::GPUTexture ids = context.CreateTexture({ .Width = 8,
                                                 .Height = 8,
                                                 .Format = hz::Format::R32G32_UINT,
                                                 .StartState = RESOURCE_STATE_RENDER_TARGET,
                                                 .Descriptors = DESCRIPTOR_TYPE_TEXTURE,
                                                 .GetRenderTarget = true });
    hz::GPUBuffer  result = context.CreateBuffer({ .Size = 8,
                                                   .ElementCount = 1,
                                                   .StructStride = 8,
                                                   .usage = RESOURCE_MEMORY_USAGE_GPU_ONLY,
                                                   .StartState = RESOURCE_STATE_UNORDERED_ACCESS,
                                                   .Descriptors = DESCRIPTOR_TYPE_RW_BUFFER });
    hz::GPUTexture pickDepth = context.CreateTexture({ .Width = 8,
                                                       .Height = 8,
                                                       .Format = hz::Format::D24_UNORM_S8_UINT,
                                                       .StartState = RESOURCE_STATE_DEPTH_WRITE,
                                                       .GetRenderTarget = true });
    // A transparent triangle behind a fully cut-out foreground triangle shares the clicked pixel.
    instances[1].current = Matrix4::translation(Vector3(-0.5f, 0, 0.25f));
    materials[0].AlphaMode = 1;
    materials[0].AlphaCutoff = 0.5f;
    materials[1].AlphaMode = 2;
    materials[0].BaseColorFactor[3] = materials[1].BaseColorFactor[3] = 0;
    hz::GPUBuffer    overlap = input(instances, sizeof(instances)), cutout = input(materials, sizeof(materials));
    hz::CommandList& picks = context.AcquireCommandList();
    const uint32_t   x[] = { 16, 48, 63, 16 };
    for (uint32_t i = 0; i < 4; ++i)
    {
        const hz::GPUBuffer& objects = i == 3 ? overlap : instanceBuffer;
        const hz::GPUBuffer& surfaces = i == 3 ? cutout : materialBuffer;
        constants[4] = objects.GetSrvIndex();
        constants[5] = surfaces.GetSrvIndex();
        picks.BeginRendering(
            { .colorAttachments = { { .pTexture = &ids, .loadAction = LOAD_ACTION_CLEAR, .storeAction = STORE_ACTION_STORE } },
              .depthAttachment = { .pTexture = &pickDepth, .loadAction = LOAD_ACTION_CLEAR, .ClearValue = { .Depth = 1.0f } } },
            { .buffers = { &frame, &objects, &surfaces, &vertices, &normal, &uv, &draws, &mesh->GetIndexBuffer() },
              .indirectBuffers = { &indirect } });
        picks.SetViewport(-float(x[i] & ~7u), -32, 64, 64);
        picks.SetScissor(0, 0, 8, 8);
        picks.SetPipeline(pick);
        picks.SetRootConstant({ constants, 12 });
        picks.DrawIndirect(indirect, 0, 2);
        picks.EndRendering();
        picks.SetPipeline(readPick);
        picks.SetRootConstant({ ids.GetSrvIndex(), result.GetUavIndex(), x[i] & 7u, 0 });
        picks.Dispatch(1, 1, 1, { .sampledTextures = { &ids }, .storageBuffers = { &result } });
        picks.CopyBuffer(readback, i * 8, result, 0, 8);
    }
    context.Wait(context.Submit(picks));
    EXPECT_EQ(pixels[0], 0u);
    EXPECT_EQ(pixels[1], 9u);
    EXPECT_EQ(pixels[2], 1u);
    EXPECT_EQ(pixels[3], 15u);
    EXPECT_EQ(pixels[5], 0u); // Background has no generation.
    EXPECT_EQ(pixels[6], 1u);
    EXPECT_EQ(pixels[7], 15u);

    hz::GPUTexture mask = context.CreateTexture({ .Width = 64, .Height = 64, .Format = hz::Format::R8_UNORM,
                                                  .StartState = RESOURCE_STATE_RENDER_TARGET, .Descriptors = DESCRIPTOR_TYPE_TEXTURE,
                                                  .GetRenderTarget = true });
    hz::GPUTexture depth = context.CreateTexture({ .Width = 64, .Height = 64, .Format = hz::Format::D24_UNORM_S8_UINT,
                                                   .StartState = RESOURCE_STATE_DEPTH_WRITE, .GetRenderTarget = true });
    constants[0] = 1;
    constants[1] = instances[1].primitive.index;
    constants[2] = instances[1].primitive.generation;
    constants[4] = instanceBuffer.GetSrvIndex();
    constants[5] = materialBuffer.GetSrvIndex();
    for (float depthValue : { 1.0f, 0.0f })
    {
        hz::CommandList& maskCommands = context.AcquireCommandList();
        maskCommands.BeginRendering(
            { .colorAttachments = { { .pTexture = &mask, .loadAction = LOAD_ACTION_CLEAR, .storeAction = STORE_ACTION_STORE } },
              .depthAttachment = { .pTexture = &depth, .loadAction = LOAD_ACTION_CLEAR, .ClearValue = { .Depth = depthValue } } },
            { .buffers = { &frame, &instanceBuffer, &materialBuffer, &vertices, &normal, &uv, &draws, &mesh->GetIndexBuffer() }, .indirectBuffers = { &indirect } });
        maskCommands.SetViewport(0, 0, 64, 64);
        maskCommands.SetScissor(0, 0, 64, 64);
        maskCommands.SetPipeline(maskPipeline);
        maskCommands.SetRootConstant({ constants, 12 });
        maskCommands.DrawIndirect(indirect, 0, 2);
        maskCommands.EndRendering();
        maskCommands.SetPipeline(reader);
        maskCommands.SetRootConstant({ mask.GetSrvIndex(), output.GetUavIndex() });
        maskCommands.Dispatch(1, 1, 1, { .sampledTextures = { &mask }, .storageBuffers = { &output } });
        maskCommands.CopyBuffer(readback, 0, output, 0, 32);
        context.Wait(context.Submit(maskCommands));
        EXPECT_EQ(pixels[0], 0u);
        EXPECT_EQ(pixels[4], depthValue > 0.5f ? 255u : 0u);
    }
}

TEST_F(SceneAssetLoaderTest, TracesDxrHitsAndMissesAndUpdatesTopLevelInstances)
{
    hz::RenderContext context({ .pAppName = "DxrPipelineTest", .enableGpuValidation = true });
    if (!context.SupportsRayTracing())
    {
        GTEST_SKIP() << "Selected device does not support DXR pipelines and inline ray queries";
    }
    const char source[] = R"slang(
        cbuffer RootConstant0 { uint topIndex; uint outputIndex; };
        struct Payload { uint value; };
        [shader("raygeneration")] void RayGeneration() {
            RaytracingAccelerationStructure scene = ResourceDescriptorHeap[topIndex];
            RWStructuredBuffer<uint> output = ResourceDescriptorHeap[outputIndex];
            RayDesc ray;
            ray.Origin = float3(DispatchRaysIndex().x * 4.0, 0, -1);
            ray.Direction = float3(0, 0, 1); ray.TMin = 0.001; ray.TMax = 10;
            Payload payload = { 0 };
            TraceRay(scene, RAY_FLAG_NONE, 0xff, 0, 0, 0, ray, payload);
            output[DispatchRaysIndex().x] = payload.value;
        }
        [shader("miss")] void Miss(inout Payload payload) { payload.value = 7; }
        [shader("closesthit")] void ClosestHit(inout Payload payload, BuiltInTriangleIntersectionAttributes attributes) {
            payload.value = 100 + InstanceID() + PrimitiveIndex();
        }
    )slang";
    FileStream stream = {};
    ASSERT_TRUE(OpenStreamFromPath(DirectoryOtherFiles, "dxr-regression.slang", FileModeWrite, &stream));
    const size_t written = WriteToStream(&stream, source, sizeof(source) - 1);
    CloseStream(&stream);
    ASSERT_EQ(written, sizeof(source) - 1);
    hz::GPURayTracingPipeline pipeline =
        context.CreateRayTracingPipeline({ .sourceDirectory = DirectoryOtherFiles,
                                           .pFileName = "dxr-regression.slang",
                                           .pRayGeneration = "RayGeneration",
                                           .missShaders = { "Miss" },
                                           .hitGroups = { { .Name = "TriangleHit", .pClosestHit = "ClosestHit" } },
                                           .payloadSize = 4,
                                           .maxRecursionDepth = 1,
                                           .rootConstantSize = 8 });
    ASSERT_TRUE(pipeline.IsValid());
    const char      incrementSource[] = R"hlsl(
        cbuffer RootConstant0 { uint outputIndex; };
        [numthreads(2,1,1)] void CSMain(uint3 id : SV_DispatchThreadID) {
            RWStructuredBuffer<uint> output = ResourceDescriptorHeap[outputIndex];
            output[id.x] += 1000;
        }
    )hlsl";
    hz::GPUPipeline increment = context.CreateComputePipeline({ .shaderDesc = { .stages = { { .stage = SHADER_STAGE_COMP,
                                                                                              .pSource = incrementSource,
                                                                                              .sourceSize = sizeof(incrementSource) - 1,
                                                                                              .pEntryPoint = "CSMain" } } } });
    ASSERT_TRUE(increment.IsValid());
    const float                             positions[] = { -1, -1, 0, 1, -1, 0, 0, 1, 0 };
    const uint32_t                          indices[] = { 0, 1, 2 };
    hz::GPUBuffer                           vertices = context.CreateBuffer({ .Size = sizeof(positions),
                                                                              .ElementCount = 9,
                                                                              .pInitialData = positions,
                                                                              .initialDataSize = sizeof(positions),
                                                                              .usage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU,
                                                                              .StartState = RESOURCE_STATE_SHADER_RESOURCE,
                                                                              .Descriptors = DESCRIPTOR_TYPE_BUFFER_RAW });
    hz::GPUBuffer                           indexBuffer = context.CreateBuffer({ .Size = sizeof(indices),
                                                                                 .ElementCount = 3,
                                                                                 .pInitialData = indices,
                                                                                 .initialDataSize = sizeof(indices),
                                                                                 .usage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU,
                                                                                 .StartState = RESOURCE_STATE_SHADER_RESOURCE,
                                                                                 .Descriptors = DESCRIPTOR_TYPE_BUFFER_RAW });
    const hz::AccelerationStructureGeometry geometry = { .pVertexBuffer = &vertices,
                                                         .pIndexBuffer = &indexBuffer,
                                                         .vertexCount = 3,
                                                         .indexCount = 3 };
    hz::GPUAccelerationStructure            bottom = context.CreateAccelerationStructure({ .geometries = { &geometry, 1 } });
    ASSERT_TRUE(bottom.IsValid());
    hz::AccelerationStructureInstance instance = { .pBottom = &bottom,
                                                   .instanceId = 23,
                                                   .Flags = ACCELERATION_STRUCTURE_INSTANCE_FLAG_TRIANGLE_CULL_DISABLE };
    hz::GPUAccelerationStructure      top =
        context.CreateAccelerationStructure({ .Type = ACCELERATION_STRUCTURE_TYPE_TOP,
                                              .Flags = AccelerationStructureBuildFlags(ACCELERATION_STRUCTURE_BUILD_FLAG_ALLOW_UPDATE |
                                                                                       ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE),
                                              .instanceCount = 1 });
    ASSERT_TRUE(top.IsValid());
    ASSERT_EQ(top.GetInstanceCount(), 1u);
    hz::GPUBuffer output = context.CreateBuffer({ .Size = 8,
                                                  .ElementCount = 2,
                                                  .StructStride = 4,
                                                  .usage = RESOURCE_MEMORY_USAGE_GPU_ONLY,
                                                  .StartState = RESOURCE_STATE_UNORDERED_ACCESS,
                                                  .Descriptors = DESCRIPTOR_TYPE_RW_BUFFER });
    hz::GPUBuffer readback = context.CreateBuffer({ .Size = 40,
                                                    .usage = RESOURCE_MEMORY_USAGE_GPU_TO_CPU,
                                                    .StartState = RESOURCE_STATE_COPY_DEST,
                                                    .Flags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT });
    const auto    validateDevice = [&](const char* phase)
    {
        ID3D12Device* device = nullptr;
        readback.Get()->dx.pResource->GetDevice(IID_PPV_ARGS(&device));
        if (!device)
        {
            return E_FAIL;
        }
        const HRESULT    removed = device->GetDeviceRemovedReason();
        ID3D12InfoQueue* info = nullptr;
        device->QueryInterface(IID_PPV_ARGS(&info));
        if (info)
        {
            for (UINT64 i = 0; i < info->GetNumStoredMessagesAllowedByRetrievalFilter(); ++i)
            {
                SIZE_T Size = 0;
                info->GetMessage(i, nullptr, &Size);
                std::vector<uint8_t> bytes(Size);
                D3D12_MESSAGE*       message = (D3D12_MESSAGE*)bytes.data();
                info->GetMessage(i, message, &Size);
                if (message->Severity <= D3D12_MESSAGE_SEVERITY_ERROR)
                {
                    ADD_FAILURE() << phase << ": " << message->pDescription;
                }
                else if (message->Severity == D3D12_MESSAGE_SEVERITY_WARNING)
                {
                    fprintf(stderr, "%s: %s\n", phase, message->pDescription);
                }
            }
            info->ClearStoredMessages();
            info->Release();
        }
        if (FAILED(removed))
        {
            ID3D12DeviceRemovedExtendedData* dred = nullptr;
            if (SUCCEEDED(device->QueryInterface(IID_PPV_ARGS(&dred))))
            {
                D3D12_DRED_AUTO_BREADCRUMBS_OUTPUT breadcrumbs = {};
                if (SUCCEEDED(dred->GetAutoBreadcrumbsOutput(&breadcrumbs)))
                {
                    for (const D3D12_AUTO_BREADCRUMB_NODE* node = breadcrumbs.pHeadAutoBreadcrumbNode; node; node = node->pNext)
                    {
                        const UINT completed = node->pLastBreadcrumbValue ? *node->pLastBreadcrumbValue : 0;
                        ADD_FAILURE() << phase << " DRED completed " << completed << '/' << node->BreadcrumbCount;
                        const UINT first = completed > 4 ? completed - 4 : 0;
                        for (UINT i = first; node->pCommandHistory && i < node->BreadcrumbCount && i < completed + 4; ++i)
                        {
                            ADD_FAILURE() << "DRED operation[" << i << "] = " << unsigned(node->pCommandHistory[i]);
                        }
                    }
                }
                D3D12_DRED_PAGE_FAULT_OUTPUT pageFault = {};
                if (SUCCEEDED(dred->GetPageFaultAllocationOutput(&pageFault)))
                {
                    ADD_FAILURE() << phase << " DRED page fault VA: " << std::hex << pageFault.PageFaultVA;
                }
                dred->Release();
            }
        }
        device->Release();
        return removed;
    };
    hz::SubmitHandle submission;
    for (uint32_t pass = 0; pass < 5; ++pass)
    {
        hz::CommandList& commands = context.AcquireCommandList();
        if (!pass)
        {
            commands.BuildAccelerationStructure(bottom);
        }
        if (pass < 3)
        {
            instance.transform[3] = pass ? 4.0f : 0.0f;
            top.SetInstance(0, instance);
            // The third pass repeats the same instance and must preserve the existing TLAS.
            commands.UpdateAccelerationStructure(top);
        }
        else if (pass == 3)
        {
            // Saved TLAS inputs and BLAS references remain valid after their public owner is gone.
            commands.BuildAccelerationStructure(top, {}, true);
        }
        if (pass == 2)
        {
            bottom = {};
        }
        commands.SetPipeline(pipeline);
        commands.SetRootConstant({ top.GetSrvIndex(), output.GetUavIndex() });
        commands.TraceRays(pipeline, 2, 1, 1, { .storageBuffers = { &output } });
        commands.SetPipeline(increment);
        commands.SetRootConstant({ output.GetUavIndex() });
        commands.Dispatch(1, 1, 1, { .storageBuffers = { &output } });
        commands.CopyBuffer(readback, pass * 8, output, 0, 8);
        if (pass == 4)
        {
            // Trace-only submission: destruction is safe without an AS dependency declaration or an AS build.
            top = {};
            pipeline = {};
            increment = {};
            output = {};
        }
        submission = context.Submit(commands);
    }
    // Queue several frames through one TLAS, including refit after an earlier trace, before waiting.
    context.Wait(submission);
    ASSERT_EQ(validateDevice("Queued TLAS updates and traces"), S_OK);
    const uint32_t* values = (const uint32_t*)readback.Get()->pCpuMappedAddress;
    ASSERT_NE(values, nullptr);
    for (uint32_t pass = 0; pass < 5; ++pass)
    {
        EXPECT_EQ(values[pass * 2], pass ? 1007u : 1123u);
        EXPECT_EQ(values[pass * 2 + 1], pass ? 1123u : 1007u);
    }
}

static const char kCookbookTextureReadback[] = R"hlsl(
    cbuffer RootConstant0 { uint sourceIndex; uint outputIndex; uint offset; };
    [numthreads(8,8,1)] void CSMain(uint3 id : SV_DispatchThreadID) {
        Texture2D<float4> source = ResourceDescriptorHeap[sourceIndex];
        uint Width,Height; source.GetDimensions(Width,Height);
        if(id.x >= Width || id.y >= Height) return;
        RWStructuredBuffer<float4> output = ResourceDescriptorHeap[outputIndex];
        output[offset + id.y * Width + id.x] = source.Load(int3(id.xy,0));
    }
)hlsl";

TEST_F(SceneAssetLoaderTest, RayTracingReflectionRejectsMaskedAlphaAndKeepsOpaqueHits)
{
    hz::RenderContext context({ .pAppName = "RayTracingReflectionAlphaTest", .enableGpuValidation = true });
    if (!context.SupportsRayTracing())
    {
        GTEST_SKIP() << "Selected device does not support ray tracing pipelines";
    }
    const std::filesystem::path shaders =
        std::filesystem::absolute(std::filesystem::path(__FILE__).parent_path().parent_path().parent_path() / "Examples/Renderer/Shaders");
    SetResourceDirectoryPath(&fileSystem, MountProject, DirectoryOtherFiles, shaders.string().c_str());
    hz::GPURayTracingPipeline reflection = context.CreateRayTracingPipeline({
        .sourceDirectory = DirectoryOtherFiles,
        .pFileName = "RTReflection.slang",
        .pRayGeneration = "rayGenMain",
        .missShaders = { "missMain" },
        .hitGroups = { { .Name = "ReflectionHit", .pClosestHit = "closestHitMain", .pAnyHit = "anyHitFoliage" } },
        .payloadSize = 16,
        .maxRecursionDepth = 2,
    });
    SetResourceDirectoryPath(&fileSystem, MountProject, DirectoryOtherFiles, directory.string().c_str());
    hz::GPUPipeline reader =
        context.CreateComputePipeline({ .shaderDesc = { .stages = { { .stage = SHADER_STAGE_COMP,
                                                                      .pSource = kCookbookTextureReadback,
                                                                      .sourceSize = sizeof(kCookbookTextureReadback) - 1,
                                                                      .pEntryPoint = "CSMain" } } } });
    ASSERT_TRUE(reflection.IsValid() && reader.IsValid());
    const auto input = [&](const void* data, uint32_t Size, uint32_t stride = 0)
    {
        return context.CreateBuffer({ .Size = Size,
                                      .ElementCount = stride ? Size / stride : Size / 4,
                                      .StructStride = stride,
                                      .pInitialData = data,
                                      .initialDataSize = Size,
                                      .usage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU,
                                      .StartState = RESOURCE_STATE_SHADER_RESOURCE,
                                      .Descriptors = stride ? DESCRIPTOR_TYPE_BUFFER : DESCRIPTOR_TYPE_BUFFER_RAW,
                                      .Flags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT });
    };
    const auto image = [&](hz::Format Format, bool target)
    {
        return context.CreateTexture({ .Width = 1,
                                       .Height = 1,
                                       .Format = Format,
                                       .StartState = target ? RESOURCE_STATE_RENDER_TARGET : RESOURCE_STATE_UNORDERED_ACCESS,
                                       .Descriptors = DescriptorType(DESCRIPTOR_TYPE_TEXTURE | DESCRIPTOR_TYPE_RW_TEXTURE),
                                       .Flags = TEXTURE_CREATION_FLAG_FORCE_2D,
                                       .GetRenderTarget = target });
    };
    hz::GPUTexture normal = image(hz::Format::R32G32B32A32_SFLOAT, true), Depth = image(hz::Format::R32_SFLOAT, true);
    hz::GPUTexture alpha = image(hz::Format::R32G32B32A32_SFLOAT, true);
    hz::GPUTexture result = image(hz::Format::R32G32B32A32_SFLOAT, false);
    hz::GPUSampler sampler = context.CreateSampler();
    const FrameData         camera = { .Eye = Vector4(0, 0, 1, 1) };
    const LightingFrameData lighting = { .LightDirection = Vector4(0, 0, -1, 0) };
    hz::GPUBuffer  frame = input(&camera, sizeof(camera), sizeof(camera)), light = input(&lighting, sizeof(lighting), sizeof(lighting));
    const float    positions[] = { -1, -1, 0.5f, 3, -1, 0.5f, -1, 3, 0.5f };
    // A leading index exercises a uint16 triangle spanning two aligned words.
    const uint16_t indices[] = { UINT16_MAX, 0, 1, 2 };
    const uint32_t indices32[] = { UINT32_MAX, 0, 1, 2 };
    const uint32_t texcoords[] = { 0, 0, 0 };
    hz::GPUBuffer  vertices = input(positions, sizeof(positions)), indexBuffer = input(indices, sizeof(indices));
    hz::GPUBuffer indexBuffer32 = input(indices32, sizeof(indices32));
    hz::GPUBuffer  uv = input(texcoords, sizeof(texcoords));
    hz::GpuMaterialData materials[3];
    for (uint32_t i = 0; i < 3; ++i)
    {
        materials[i].Id = { i, 1 };
        materials[i].BaseColorTexture = i < 2 ? alpha.GetSrvIndex() : UINT32_MAX;
        materials[i].AlphaMode = i == 1 ? 1u : 0u;
        materials[i].EmissiveFactor[i == 2 ? 1 : 0] = 1;
        materials[i].AlphaCutoff = 0.5f;
    }
    hz::GpuTextureData textures[4];
    textures[3] = { .id = { 3, 9 }, .srv = alpha.GetSrvIndex() };
    hz::GpuDrawData drawData[8] = {};
    drawData[5] = { .Material = 0,
                    .AlphaMode = 0,
                    .Positions = vertices.GetSrvIndex(),
                    .Normals = uv.GetSrvIndex(),
                    .Texcoords = uv.GetSrvIndex(),
                    .Indices = indexBuffer.GetSrvIndex(),
                    .IndexStride = 2,
                    .FirstIndex = 1,
                    .IndexCount = 3 };
    drawData[6] = drawData[5];
    drawData[6].Material = drawData[6].AlphaMode = 1;
    drawData[7] = drawData[5];
    drawData[7].Material = 2;
    hz::GPUBuffer materialBuffer = input(materials, sizeof(materials)), textureBuffer = input(textures, sizeof(textures));
    hz::GPUBuffer drawBuffer = input(drawData, sizeof(drawData));
    hz::AccelerationStructureGeometry geometry = { .pVertexBuffer = &vertices,
                                                   .pIndexBuffer = &indexBuffer,
                                                   .indexOffset = sizeof(uint16_t),
                                                   .vertexCount = 3,
                                                   .indexCount = 3,
                                                   .indexType = INDEX_TYPE_UINT16 };
    hz::GPUAccelerationStructure      opaque = context.CreateAccelerationStructure({ .geometries = { &geometry, 1 } });
    geometry.opaque = false;
    hz::GPUAccelerationStructure      masked = context.CreateAccelerationStructure({ .geometries = { &geometry, 1 } });
    hz::AccelerationStructureInstance background = {
        .pBottom = &opaque,
        .instanceId = 7,
        .Flags = ACCELERATION_STRUCTURE_INSTANCE_FLAG_TRIANGLE_CULL_DISABLE,
    };
    background.transform[11] = 0.5f;
    const hz::AccelerationStructureInstance opaqueInstances[] = {
        { .pBottom = &opaque, .instanceId = 5, .Flags = ACCELERATION_STRUCTURE_INSTANCE_FLAG_TRIANGLE_CULL_DISABLE },
        background,
    };
    const hz::AccelerationStructureInstance maskedInstances[] = {
        { .pBottom = &masked, .instanceId = 6, .Flags = ACCELERATION_STRUCTURE_INSTANCE_FLAG_TRIANGLE_CULL_DISABLE },
        background,
    };
    hz::GPUAccelerationStructure opaqueTop =
        context.CreateAccelerationStructure({ .Type = ACCELERATION_STRUCTURE_TYPE_TOP, .instances = { opaqueInstances, 2 } });
    hz::GPUAccelerationStructure maskedTop =
        context.CreateAccelerationStructure({ .Type = ACCELERATION_STRUCTURE_TYPE_TOP, .instances = { maskedInstances, 2 } });
    hz::GPUAccelerationStructure maskedOnly =
        context.CreateAccelerationStructure({ .Type = ACCELERATION_STRUCTURE_TYPE_TOP, .instances = { maskedInstances, 1 } });
    ASSERT_TRUE(opaque.IsValid() && masked.IsValid() && opaqueTop.IsValid() && maskedTop.IsValid() && maskedOnly.IsValid());
    hz::GPUBuffer output = context.CreateBuffer({ .Size = 16,
                                                  .ElementCount = 1,
                                                  .StructStride = 16,
                                                  .usage = RESOURCE_MEMORY_USAGE_GPU_ONLY,
                                                  .StartState = RESOURCE_STATE_UNORDERED_ACCESS,
                                                  .Descriptors = DESCRIPTOR_TYPE_RW_BUFFER });
    hz::GPUBuffer readback = context.CreateBuffer({ .Size = 16,
                                                    .usage = RESOURCE_MEMORY_USAGE_GPU_TO_CPU,
                                                    .StartState = RESOURCE_STATE_COPY_DEST,
                                                    .Flags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT });
    struct ReflectionConstants
    {
        uint32_t frame, lighting, draws, materials, textures, sampler, tlas, normal, Depth, output, noise;
        uint32_t environment = UINT32_MAX, irradiance = UINT32_MAX, brdf = UINT32_MAX, environmentSampler = UINT32_MAX;
        uint32_t shadows = 1, ao = 0, samples = 1, phase = 0, alphaMethod = 1, retraceDepth = 3;
        float    aoRadius = 2, aoPower = 1, shadowRadius = 0;
    } constants = { frame.GetSrvIndex(),
                    light.GetSrvIndex(),
                    drawBuffer.GetSrvIndex(),
                    materialBuffer.GetSrvIndex(),
                    textureBuffer.GetSrvIndex(),
                    sampler.GetIndex(),
                    0,
                    normal.GetSrvIndex(),
                    Depth.GetSrvIndex(),
                    result.GetUavIndex(),
                    UINT32_MAX };
    struct AlphaCase
    {
        float textureAlpha;
        float factor;
        bool  hasTexture;
        bool  opaque;
        bool  background;
        float red;
        float green;
    };
    const AlphaCase cases[] = {
        { 0.0f, 1.0f, true, true, true, 1, 0 },    // Opaque geometry accepts even zero texture alpha.
        { 0.25f, 1.0f, true, false, true, 0, 1 },  // A mask hole reveals the green geometry behind it.
        { 0.75f, 1.0f, true, false, true, 1, 0 },  // The solid part of the mask stays red.
        { 0.5f, 1.0f, true, false, true, 1, 0 },   // Alpha equal to the cutoff is accepted.
        { 0.75f, 0.5f, true, false, true, 0, 1 },  // Texture alpha is multiplied by the material factor.
        { 1.0f, 0.25f, false, false, true, 0, 1 }, // Material alpha alone can reject a hit.
        { 0.0f, 0.75f, false, false, true, 1, 0 }, // An absent texture does not affect material alpha.
        { 0.25f, 1.0f, true, false, false, 0, 0 }, // A hole with no geometry behind it reaches the miss shader.
    };
    const uint32_t caseCount = sizeof(cases) / sizeof(cases[0]);
    for (uint32_t caseIndex = 0; caseIndex < 2 * caseCount; ++caseIndex)
    {
        SCOPED_TRACE(caseIndex);
        const AlphaCase& test = cases[caseIndex % caseCount];
        for (hz::GpuDrawData& Draw : drawData)
        {
            Draw.Indices = caseIndex < caseCount ? indexBuffer.GetSrvIndex() : indexBuffer32.GetSrvIndex();
            Draw.IndexStride = caseIndex < caseCount ? 2 : 4;
        }
        ASSERT_TRUE(context.UpdateBuffer(drawBuffer, 0, drawData, sizeof(drawData)));
        materials[1].BaseColorFactor[3] = test.factor;
        materials[1].BaseColorTexture = test.hasTexture ? alpha.GetSrvIndex() : UINT32_MAX;
        ASSERT_TRUE(context.UpdateBuffer(materialBuffer, 0, materials, sizeof(materials)));
        hz::CommandList& commands = context.AcquireCommandList();
        if (!caseIndex)
        {
            commands.BuildAccelerationStructure(opaque);
            commands.BuildAccelerationStructure(masked);
            commands.BuildAccelerationStructure(opaqueTop);
            commands.BuildAccelerationStructure(maskedTop);
            commands.BuildAccelerationStructure(maskedOnly);
        }
        const auto Clear = [&](const hz::GPUTexture& texture, ClearValue value)
        {
            commands.BeginRendering({ .colorAttachments = { { .pTexture = &texture,
                                                              .loadAction = LOAD_ACTION_CLEAR,
                                                              .storeAction = STORE_ACTION_STORE,
                                                              .ClearValue = value } } });
            commands.EndRendering();
        };
        Clear(normal, { .r = 0.5f, .g = 0.5f, .a = 1 });
        Clear(Depth, { .r = 0.2f });
        Clear(alpha, { .r = 1, .g = 1, .b = 1, .a = test.textureAlpha });
        constants.tlas = test.opaque ? opaqueTop.GetSrvIndex() : test.background ? maskedTop.GetSrvIndex() : maskedOnly.GetSrvIndex();
        commands.SetPipeline(reflection);
        commands.SetRootConstant({ reinterpret_cast<const uint32_t*>(&constants), sizeof(constants) / 4 });
        commands.TraceRays(reflection, 1, 1, 1,
                           { .sampledTextures = { &normal, &Depth, &alpha },
                             .storageTextures = { &result },
                             .buffers = { &frame, &light, &drawBuffer, &materialBuffer, &textureBuffer, &vertices, &indexBuffer, &indexBuffer32, &uv } });
        commands.SetPipeline(reader);
        commands.SetRootConstant({ result.GetSrvIndex(), output.GetUavIndex(), 0 });
        commands.Dispatch(1, 1, 1, { .sampledTextures = { &result }, .storageBuffers = { &output } });
        commands.CopyBuffer(readback, 0, output, 0, 16);
        context.Wait(context.Submit(commands));
        const float* values = (const float*)readback.Get()->pCpuMappedAddress;
        ASSERT_NE(values, nullptr);
        EXPECT_NEAR(values[0], test.red, 1e-5f);
        EXPECT_NEAR(values[1], test.green, 1e-5f);
        EXPECT_NEAR(values[2], 0, 1e-5f);
        EXPECT_FLOAT_EQ(values[3], 1);
    }
    // Verify reflected lighting color as well as emissive hit IDs: a white surface and white light stay neutral.
    LightingFrameData whiteLight = { .LightDirection = Vector4(0, 0, 1, 1), .LightColor = Vector4(1, 1, 1, 1) };
    ASSERT_TRUE(context.UpdateBuffer(light, 0, &whiteLight, sizeof(whiteLight)));
    materials[0].BaseColorTexture = UINT32_MAX;
    materials[0].BaseColorFactor[0] = materials[0].BaseColorFactor[1] = materials[0].BaseColorFactor[2] = 1;
    materials[0].EmissiveFactor[0] = materials[0].EmissiveFactor[1] = materials[0].EmissiveFactor[2] = 0;
    materials[0].MetallicFactor = 0;
    materials[0].RoughnessFactor = 1;
    ASSERT_TRUE(context.UpdateBuffer(materialBuffer, 0, materials, sizeof(materials)));
    constants.tlas = opaqueTop.GetSrvIndex();
    constexpr uint32_t roughnessCodes[] = { 0, 98 };
    // Unit roughness and normal incidence, including Default Lit's directional albedo and multiscattering.
    const float albedo = 1.0f - 1.0266916f / 1.466495f;
    const float specularWeight = 1.0f + 0.04f * (1.0f - albedo) / albedo;
    const float expectedLighting = (1.0f - specularWeight * albedo * 0.04f + specularWeight * 0.04f / 4.0f) / 3.14159265359f;
    for (uint32_t roughness : roughnessCodes)
    {
        hz::CommandList& lit = context.AcquireCommandList();
        lit.BeginRendering({ .colorAttachments = { { .pTexture = &normal,
                                                      .loadAction = LOAD_ACTION_CLEAR,
                                                      .storeAction = STORE_ACTION_STORE,
                                                      .ClearValue = { .r = 0.5f, .g = 0.5f, .b = float(roughness) / 1023.0f,
                                                                      .a = 2.0f / 3.0f } } } });
        lit.EndRendering();
        lit.SetPipeline(reflection);
        lit.SetRootConstant({ reinterpret_cast<const uint32_t*>(&constants), sizeof(constants) / 4 });
        lit.TraceRays(reflection, 1, 1, 1,
                      { .sampledTextures = { &normal, &Depth }, .storageTextures = { &result },
                        .buffers = { &frame, &light, &drawBuffer, &materialBuffer, &vertices, &indexBuffer32, &uv } });
        lit.SetPipeline(reader);
        lit.SetRootConstant({ result.GetSrvIndex(), output.GetUavIndex(), 0 });
        lit.Dispatch(1, 1, 1, { .sampledTextures = { &result }, .storageBuffers = { &output } });
        lit.CopyBuffer(readback, 0, output, 0, 16);
        context.Wait(context.Submit(lit));
        const float* litColor = (const float*)readback.Get()->pCpuMappedAddress;
        // At pixel (0, 0), frame 0, GGX sampling at roughness 98/255 gives a normalized weight of about 1.024.
        const float expected = expectedLighting * (roughness == 0 ? 1.0f : 1.0238f);
        for (uint32_t channel = 0; channel < 3; ++channel)
        {
            EXPECT_NEAR(litColor[channel], expected, 2e-4f) << roughness;
        }
    }
    // A red directional light and a blue point light illuminate the offscreen hit at x=1.25.
    const float             slope = std::tan(0.5f * std::atan(1.25f / 0.3f));
    const hz::GpuLightData  offscreen = { .PositionAndInvRadius = Vector4(1.25f, 0.0f, 0.4f, 5.0f),
                                          .DirectionAndType = Vector4(0, 0, -1, 1),
                                          .Color = Vector4(0, 0, 1, 0) };
    const uint32_t          id = 0;
    const hz::GPUBuffer     offscreenBuffer = input(&offscreen, sizeof(offscreen)), active = input(&id, sizeof(id));
    const LightingFrameData offscreenFrame = { .LightDirection = Vector4(0, 0, 1, 1),
                                               .LightColor = Vector4(1, 0, 0, 1),
                                               .LocalLightsIndex = offscreenBuffer.GetSrvIndex(),
                                               .LocalLightCount = 1,
                                               .ActiveLocalLightsIndex = active.GetSrvIndex() };
    ASSERT_TRUE(context.UpdateBuffer(light, 0, &offscreenFrame, sizeof(offscreenFrame)));
    hz::CommandList& bounce = context.AcquireCommandList();
    bounce.BeginRendering({ .colorAttachments = { { .pTexture = &normal,
                                                    .loadAction = LOAD_ACTION_CLEAR,
                                                    .storeAction = STORE_ACTION_STORE,
                                                    .ClearValue = { .r = 0.5f + 0.5f * slope / (1 + slope), .g = 0.5f } } } });
    bounce.EndRendering();
    bounce.SetPipeline(reflection);
    bounce.SetRootConstants(constants);
    bounce.TraceRays(
        reflection, 1, 1, 1,
        { .sampledTextures = { &normal, &Depth },
          .storageTextures = { &result },
          .buffers = { &frame, &light, &drawBuffer, &materialBuffer, &vertices, &indexBuffer32, &uv, &offscreenBuffer, &active } });
    bounce.SetPipeline(reader);
    bounce.SetRootConstant({ result.GetSrvIndex(), output.GetUavIndex(), 0 });
    bounce.Dispatch(1, 1, 1, { .sampledTextures = { &result }, .storageBuffers = { &output } });
    bounce.CopyBuffer(readback, 0, output, 0, 16);
    context.Wait(context.Submit(bounce));
    const float* reflected = (const float*)readback.Get()->pCpuMappedAddress;
    EXPECT_GT(reflected[0], 0.0f);
    EXPECT_FLOAT_EQ(reflected[2], 0.0f);

    ID3D12Device* device = nullptr;
    ASSERT_EQ(output.Get()->dx.pResource->GetDevice(IID_PPV_ARGS(&device)), S_OK);
    EXPECT_EQ(device->GetDeviceRemovedReason(), S_OK);
    ID3D12InfoQueue* info = nullptr;
    if (SUCCEEDED(device->QueryInterface(IID_PPV_ARGS(&info))))
    {
        for (UINT64 i = 0; i < info->GetNumStoredMessagesAllowedByRetrievalFilter(); ++i)
        {
            SIZE_T Size = 0;
            info->GetMessage(i, nullptr, &Size);
            std::vector<uint8_t> bytes(Size);
            D3D12_MESSAGE*       message = (D3D12_MESSAGE*)bytes.data();
            info->GetMessage(i, message, &Size);
            EXPECT_GT(message->Severity, D3D12_MESSAGE_SEVERITY_ERROR) << message->pDescription;
        }
        info->Release();
    }
    device->Release();
}

TEST_F(SceneAssetLoaderTest, CookbookRayShadowRejectsMaskedAlphaAndKeepsOpaqueOccluders)
{
    hz::RenderContext context({ .pAppName = "CookbookRayShadowTest", .enableGpuValidation = true });
    if (!context.SupportsRayTracing())
    {
        GTEST_SKIP() << "Selected device does not support inline ray queries";
    }
    const std::filesystem::path shaders =
        std::filesystem::absolute(std::filesystem::path(__FILE__).parent_path().parent_path().parent_path() / "Examples/Renderer/Shaders");
    SetResourceDirectoryPath(&fileSystem, MountProject, DirectoryOtherFiles, shaders.string().c_str());
    hz::GPUPipeline shadow =
        context.CreateComputePipeline({ .shaderDesc = { .stages = { { .stage = SHADER_STAGE_COMP, .pEntryPoint = "main" } },
                                                        .sourceDirectory = DirectoryOtherFiles,
                                                        .pFileName = "RTShadowAO.slang" } });
    SetResourceDirectoryPath(&fileSystem, MountProject, DirectoryOtherFiles, directory.string().c_str());
    hz::GPUPipeline reader =
        context.CreateComputePipeline({ .shaderDesc = { .stages = { { .stage = SHADER_STAGE_COMP,
                                                                      .pSource = kCookbookTextureReadback,
                                                                      .sourceSize = sizeof(kCookbookTextureReadback) - 1,
                                                                      .pEntryPoint = "CSMain" } } } });
    ASSERT_TRUE(shadow.IsValid() && reader.IsValid());
    const auto input = [&](const void* data, uint32_t Size, uint32_t stride = 0)
    {
        return context.CreateBuffer({ .Size = Size,
                                      .ElementCount = stride ? Size / stride : Size / 4,
                                      .StructStride = stride,
                                      .pInitialData = data,
                                      .initialDataSize = Size,
                                      .usage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU,
                                      .StartState = RESOURCE_STATE_SHADER_RESOURCE,
                                      .Descriptors = stride ? DESCRIPTOR_TYPE_BUFFER : DESCRIPTOR_TYPE_BUFFER_RAW });
    };
    const auto image = [&](hz::Format Format, bool target)
    {
        return context.CreateTexture({ .Width = 1,
                                       .Height = 1,
                                       .Format = Format,
                                       .StartState = target ? RESOURCE_STATE_RENDER_TARGET : RESOURCE_STATE_UNORDERED_ACCESS,
                                       .Descriptors = DescriptorType(DESCRIPTOR_TYPE_TEXTURE | DESCRIPTOR_TYPE_RW_TEXTURE),
                                       .Flags = TEXTURE_CREATION_FLAG_FORCE_2D,
                                       .GetRenderTarget = target });
    };
    hz::GPUTexture normal = image(hz::Format::R32G32B32A32_SFLOAT, true), Depth = image(hz::Format::R32_SFLOAT, true);
    hz::GPUTexture alpha = image(hz::Format::R32G32B32A32_SFLOAT, true), noise = image(hz::Format::R32G32B32A32_SFLOAT, true);
    hz::GPUTexture result = image(hz::Format::R8G8_UNORM, false);
    hz::GPUSampler sampler = context.CreateSampler();
    const FrameData         camera = { .Eye = Vector4(0, 0, -1, 1) };
    const LightingFrameData lighting = { .LightDirection = Vector4(0, 0, -1, 1), .RTShadowEnabled = 1 };
    hz::GPUBuffer  frame = input(&camera, sizeof(camera), sizeof(camera)), light = input(&lighting, sizeof(lighting), sizeof(lighting));
    const float    positions[] = { -1, -1, 0.5f, 3, -1, 0.5f, -1, 3, 0.5f };
    // One uint16 triangle, padded so the production RAW decoder can read its final word.
    const uint16_t indices[] = { 0, 1, 2, 0 };
    const uint32_t texcoords[] = { 0, 0, 0 };
    hz::GPUBuffer  vertices = input(positions, sizeof(positions)), indexBuffer = input(indices, sizeof(indices));
    hz::GPUBuffer  uv = input(texcoords, sizeof(texcoords));
    hz::GpuMaterialData     materials[2];
    for (uint32_t i = 0; i < 2; ++i)
    {
        materials[i].Id = { i, 1 };
        materials[i].BaseColorTexture = alpha.GetSrvIndex();
        materials[i].AlphaMode = i;
        materials[i].AlphaCutoff = 0.5f;
    }
    hz::GpuTextureData textures[4];
    textures[3] = { .id = { 3, 9 }, .srv = alpha.GetSrvIndex() };
    hz::GpuDrawData drawData[7] = {};
    drawData[5] = { .Material = 0,
                    .AlphaMode = 0,
                    .Positions = vertices.GetSrvIndex(),
                    .Normals = uv.GetSrvIndex(),
                    .Texcoords = uv.GetSrvIndex(),
                    .Indices = indexBuffer.GetSrvIndex(),
                    .IndexStride = 2,
                    .IndexCount = 3 };
    drawData[6] = drawData[5];
    drawData[6].Material = drawData[6].AlphaMode = 1;
    hz::GPUBuffer materialBuffer = input(materials, sizeof(materials)), textureBuffer = input(textures, sizeof(textures));
    hz::GPUBuffer                     drawBuffer = input(drawData, sizeof(drawData));
    hz::AccelerationStructureGeometry geometry = { .pVertexBuffer = &vertices,
                                                   .pIndexBuffer = &indexBuffer,
                                                   .vertexCount = 3,
                                                   .indexCount = 3,
                                                   .indexType = INDEX_TYPE_UINT16 };
    hz::GPUAccelerationStructure      opaque = context.CreateAccelerationStructure({ .geometries = { &geometry, 1 } });
    geometry.opaque = false;
    hz::GPUAccelerationStructure            masked = context.CreateAccelerationStructure({ .geometries = { &geometry, 1 } });
    const hz::AccelerationStructureInstance instances[] = {
        { .pBottom = &opaque, .instanceId = 5, .Flags = ACCELERATION_STRUCTURE_INSTANCE_FLAG_TRIANGLE_CULL_DISABLE },
        { .pBottom = &masked, .instanceId = 6, .Flags = ACCELERATION_STRUCTURE_INSTANCE_FLAG_TRIANGLE_CULL_DISABLE },
    };
    hz::GPUAccelerationStructure opaqueTop =
        context.CreateAccelerationStructure({ .Type = ACCELERATION_STRUCTURE_TYPE_TOP, .instances = { instances, 1 } });
    hz::GPUAccelerationStructure maskedTop =
        context.CreateAccelerationStructure({ .Type = ACCELERATION_STRUCTURE_TYPE_TOP, .instances = { instances + 1, 1 } });
    ASSERT_TRUE(opaque.IsValid() && masked.IsValid() && opaqueTop.IsValid() && maskedTop.IsValid());
    hz::GPUBuffer output = context.CreateBuffer({ .Size = 16,
                                                  .ElementCount = 1,
                                                  .StructStride = 16,
                                                  .usage = RESOURCE_MEMORY_USAGE_GPU_ONLY,
                                                  .StartState = RESOURCE_STATE_UNORDERED_ACCESS,
                                                  .Descriptors = DESCRIPTOR_TYPE_RW_BUFFER });
    hz::GPUBuffer readback = context.CreateBuffer({ .Size = 16,
                                                    .usage = RESOURCE_MEMORY_USAGE_GPU_TO_CPU,
                                                    .StartState = RESOURCE_STATE_COPY_DEST,
                                                    .Flags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT });
    struct ShadowConstants
    {
        uint32_t FrameIndexBuffer, LightingIndex, DrawsIndex, MaterialsIndex, TexturesIndex, SamplerIndex;
        uint32_t TlasIndex, NormalIndex, DepthIndex, OutputIndex, BlueNoiseIndex;
        uint32_t EnvironmentIndex = UINT32_MAX, IrradianceIndex = UINT32_MAX, BrdfIndex = UINT32_MAX, EnvironmentSamplerIndex = UINT32_MAX;
        uint32_t ShadowsEnabled = 1, AoEnabled = 0, AoSamples = 1, FrameNumber = 0, AlphaTestMethod = 1, RetraceMaxDepth = 3;
        float    AoRadius = 2, ShadowRadius = 0;
    } constants = { frame.GetSrvIndex(),
                    light.GetSrvIndex(),
                    drawBuffer.GetSrvIndex(),
                    materialBuffer.GetSrvIndex(),
                    textureBuffer.GetSrvIndex(),
                    sampler.GetIndex(),
                    0,
                    normal.GetSrvIndex(),
                    Depth.GetSrvIndex(),
                    result.GetUavIndex(),
                    noise.GetSrvIndex() };
    // Three alpha-test modes, then the reference AO path which deliberately forces opaque hits.
    for (uint32_t caseIndex = 0; caseIndex < 12; ++caseIndex)
    {
        SCOPED_TRACE(caseIndex);
        const uint32_t scenario = caseIndex % 3;
        constants.AlphaTestMethod = (caseIndex / 3) % 3;
        constants.ShadowsEnabled = caseIndex < 9;
        constants.AoEnabled = caseIndex >= 9;
        hz::CommandList& commands = context.AcquireCommandList();
        if (!caseIndex)
        {
            commands.BuildAccelerationStructure(opaque);
            commands.BuildAccelerationStructure(masked);
            commands.BuildAccelerationStructure(opaqueTop);
            commands.BuildAccelerationStructure(maskedTop);
        }
        const auto Clear = [&](const hz::GPUTexture& texture, ClearValue value)
        {
            commands.BeginRendering({ .colorAttachments = { { .pTexture = &texture,
                                                              .loadAction = LOAD_ACTION_CLEAR,
                                                              .storeAction = STORE_ACTION_STORE,
                                                              .ClearValue = value } } });
            commands.EndRendering();
        };
        Clear(normal, { .r = 0.5f, .g = 0.5f, .a = 1 }); // Octahedral +Z.
        Clear(Depth, { .r = 0.2f });
        Clear(noise, { .r = 0.5f, .g = 0.5f });
        Clear(alpha, { .a = scenario == 2 ? 0.75f : 0.25f });
        constants.TlasIndex = scenario ? maskedTop.GetSrvIndex() : opaqueTop.GetSrvIndex();
        commands.SetPipeline(shadow);
        commands.SetRootConstant({ reinterpret_cast<const uint32_t*>(&constants), sizeof(constants) / 4 });
        commands.Dispatch(1, 1, 1,
                          { .sampledTextures = { &normal, &Depth, &noise, &alpha },
                            .storageTextures = { &result },
                            .buffers = { &frame, &light, &drawBuffer, &materialBuffer, &textureBuffer, &vertices, &indexBuffer, &uv } });
        commands.SetPipeline(reader);
        commands.SetRootConstant({ result.GetSrvIndex(), output.GetUavIndex(), 0 });
        commands.Dispatch(1, 1, 1, { .sampledTextures = { &result }, .storageBuffers = { &output } });
        commands.CopyBuffer(readback, 0, output, 0, 16);
        context.Wait(context.Submit(commands));
        const float* values = (const float*)readback.Get()->pCpuMappedAddress;
        ASSERT_NE(values, nullptr);
        EXPECT_FLOAT_EQ(values[0], constants.AoEnabled || (constants.AlphaTestMethod != 0 && scenario == 1) ? 1.0f : 0.0f);
        EXPECT_FLOAT_EQ(values[1], constants.AoEnabled ? 0.0f : 1.0f);
    }

    // Reuse the same scene to exercise the production raster shadow alpha test as well.
    SetResourceDirectoryPath(&fileSystem, MountProject, DirectoryOtherFiles, shaders.string().c_str());
    hz::GPUPipeline raster =
        context.CreateGraphicsPipeline({ .shaderDesc = { .stages = { { .stage = SHADER_STAGE_VERT, .pEntryPoint = "VSIndirect" },
                                                                     { .stage = SHADER_STAGE_FRAG, .pEntryPoint = "PSShadow" } },
                                                         .sourceDirectory = DirectoryOtherFiles,
                                                         .pFileName = "ShadowMap.slang" },
                                         .Depth = { .depthTest = true, .depthWrite = true, .depthFunc = CMP_LEQUAL },
                                         .depthStencilFormat = hz::Format::D16_UNORM,
                                         .indirectDrawId = true });
    SetResourceDirectoryPath(&fileSystem, MountProject, DirectoryOtherFiles, directory.string().c_str());
    ASSERT_TRUE(raster.IsValid());
    const hz::RenderInstance identity;
    hz::GPUBuffer            instanceBuffer = input(&identity, sizeof(identity));
    const uint32_t           arguments[2][5] = { { 5, 3, 1, 0, 0 }, { 6, 3, 1, 0, 0 } };
    hz::GPUBuffer            indirect = context.CreateBuffer({ .Size = sizeof(arguments),
                                                               .pInitialData = arguments,
                                                               .initialDataSize = sizeof(arguments),
                                                               .usage = RESOURCE_MEMORY_USAGE_GPU_ONLY,
                                                               .StartState = RESOURCE_STATE_INDIRECT_ARGUMENT,
                                                               .Descriptors = DESCRIPTOR_TYPE_INDIRECT_BUFFER });
    hz::GPUTexture           shadowDepth = context.CreateTexture({ .Width = 1,
                                                                   .Height = 1,
                                                                   .Format = hz::Format::D16_UNORM,
                                                                   .StartState = RESOURCE_STATE_DEPTH_WRITE,
                                                                   .Descriptors = DESCRIPTOR_TYPE_TEXTURE,
                                                                   .Flags = TEXTURE_CREATION_FLAG_FORCE_2D,
                                                                   .GetRenderTarget = true });
    ASSERT_TRUE(instanceBuffer.IsValid() && indirect.IsValid() && shadowDepth.IsValid());
    for (uint32_t scenario = 0; scenario < 3; ++scenario)
    {
        SCOPED_TRACE(scenario);
        hz::CommandList& commands = context.AcquireCommandList();
        commands.BeginRendering({ .colorAttachments = { { .pTexture = &alpha,
                                                          .loadAction = LOAD_ACTION_CLEAR,
                                                          .storeAction = STORE_ACTION_STORE,
                                                          .ClearValue = { .a = scenario == 2 ? 0.75f : 0.25f } } } });
        commands.EndRendering();
        commands.SetPipeline(raster);
        commands.BeginRendering(
            { .depthAttachment = { .pTexture = &shadowDepth,
                                   .loadAction = LOAD_ACTION_CLEAR,
                                   .storeAction = STORE_ACTION_STORE,
                                   .ClearValue = { .Depth = 1 } } },
            { .sampledTextures = { &alpha },
              .buffers = { &frame, &instanceBuffer, &materialBuffer, &textureBuffer, &drawBuffer, &vertices, &indexBuffer, &uv },
              .indirectBuffers = { &indirect } });
        commands.SetViewport(0, 0, 1, 1);
        commands.SetScissor(0, 0, 1, 1);
        commands.SetRootConstant({ 0, 0, 0, frame.GetSrvIndex(), instanceBuffer.GetSrvIndex(), materialBuffer.GetSrvIndex(),
                                   textureBuffer.GetSrvIndex(), sampler.GetIndex() });
        commands.SetRootConstant({ drawBuffer.GetSrvIndex() }, 11);
        commands.DrawIndirect(indirect, scenario ? 20 : 0, 1);
        commands.EndRendering();
        commands.SetPipeline(reader);
        commands.SetRootConstant({ shadowDepth.GetSrvIndex(), output.GetUavIndex(), 0 });
        commands.Dispatch(1, 1, 1, { .sampledTextures = { &shadowDepth }, .storageBuffers = { &output } });
        commands.CopyBuffer(readback, 0, output, 0, 16);
        context.Wait(context.Submit(commands));
        const float* values = (const float*)readback.Get()->pCpuMappedAddress;
        ASSERT_NE(values, nullptr);
        EXPECT_NEAR(values[0], scenario == 1 ? 1.0f : 0.5f, 2e-5f);
    }
}

TEST_F(SceneAssetLoaderTest, CookbookPpllSortsDepthRejectsOccludedFragmentsAndBoundsOverflow)
{
    const std::filesystem::path shaders =
        std::filesystem::absolute(std::filesystem::path(__FILE__).parent_path().parent_path().parent_path() / "Examples/Renderer/Shaders");
    SetResourceDirectoryPath(&fileSystem, MountProject, DirectoryOtherFiles, shaders.string().c_str());
    hz::RenderContext context({ .pAppName = "CookbookPpllTest", .enableGpuValidation = true });
    hz::GPUPipeline   gather =
        context.CreateGraphicsPipeline({ .shaderDesc = { .stages = { { .stage = SHADER_STAGE_VERT, .pEntryPoint = "VSMain" },
                                                                     { .stage = SHADER_STAGE_FRAG, .pEntryPoint = "PSTransparent" } },
                                                         .sourceDirectory = DirectoryOtherFiles,
                                                         .pFileName = "Transparency.slang" },
                                         .Depth = { .depthTest = true, .depthWrite = false, .depthFunc = CMP_LEQUAL },
                                         .depthStencilFormat = hz::Format::D32_SFLOAT });
    const auto compute = [&](const char* entry)
    {
        return context.CreateComputePipeline({ .shaderDesc = { .stages = { { .stage = SHADER_STAGE_COMP, .pEntryPoint = entry } },
                                                               .sourceDirectory = DirectoryOtherFiles,
                                                               .pFileName = "Transparency.slang" } });
    };
    hz::GPUPipeline Clear = compute("ClearMain"), resolve = compute("ResolveMain");
    SetResourceDirectoryPath(&fileSystem, MountProject, DirectoryOtherFiles, directory.string().c_str());
    hz::GPUPipeline reader =
        context.CreateComputePipeline({ .shaderDesc = { .stages = { { .stage = SHADER_STAGE_COMP,
                                                                      .pSource = kCookbookTextureReadback,
                                                                      .sourceSize = sizeof(kCookbookTextureReadback) - 1,
                                                                      .pEntryPoint = "CSMain" } } } });
    ASSERT_TRUE(gather.IsValid() && Clear.IsValid() && resolve.IsValid() && reader.IsValid());
    const auto input = [&](const void* data, uint32_t Size, uint32_t stride = 0)
    {
        return context.CreateBuffer({ .Size = Size,
                                      .ElementCount = stride ? Size / stride : Size / 4,
                                      .StructStride = stride,
                                      .pInitialData = data,
                                      .initialDataSize = Size,
                                      .usage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU,
                                      .StartState = RESOURCE_STATE_SHADER_RESOURCE,
                                      .Descriptors = stride ? DESCRIPTOR_TYPE_BUFFER : DESCRIPTOR_TYPE_BUFFER_RAW });
    };
    const FrameData         camera = { .Eye = Vector4(0, 0, -1, 1), .Near = 0.01f, .Far = 1, .Width = 8, .Height = 8 };
    const LightingFrameData lighting;
    hz::RenderInstance      instances[2];
    instances[0].current = Matrix4::translation(Vector3(0, 0, 0.2f));
    instances[1].current = Matrix4::translation(Vector3(0, 0, 0.7f));
    hz::GpuMaterialData materials[2];
    for (hz::GpuMaterialData& material : materials)
    {
        material.AlphaMode = 2;
        material.BaseColorFactor[3] = 0.5f;
    }
    materials[0].EmissiveFactor[0] = materials[1].EmissiveFactor[1] = 1;
    const float    positions[] = { -1, -1, 0, 3, -1, 0, -1, 3, 0 };
    const uint32_t normals[] = { 0x80008000, 0x80008000, 0x80008000 }, texcoords[] = { 0, 0, 0 };
    hz::GPUBuffer  frame = input(&camera, sizeof(camera), sizeof(camera)), light = input(&lighting, sizeof(lighting), sizeof(lighting));
    hz::GPUBuffer  instanceBuffer = input(instances, sizeof(instances)), materialBuffer = input(materials, sizeof(materials));
    hz::GPUBuffer  vertices = input(positions, sizeof(positions)), normal = input(normals, sizeof(normals)),
                  uv = input(texcoords, sizeof(texcoords));
    hz::GPUSampler sampler = context.CreateSampler();
    const auto     image = [&](hz::Format Format, bool target)
    {
        return context.CreateTexture({ .Width = 8,
                                       .Height = 8,
                                       .Format = Format,
                                       .StartState = RESOURCE_STATE_UNORDERED_ACCESS,
                                       .Descriptors = (DescriptorType)(DESCRIPTOR_TYPE_TEXTURE | DESCRIPTOR_TYPE_RW_TEXTURE),
                                       .GetRenderTarget = target });
    };
    hz::GPUTexture heads = image(hz::Format::R32_UINT, false), result = image(hz::Format::R32G32B32A32_SFLOAT, false);
    hz::GPUTexture opaque = image(hz::Format::R32G32B32A32_SFLOAT, true);
    hz::GPUTexture Depth = context.CreateTexture(
        { .Width = 8, .Height = 8, .Format = hz::Format::D32_SFLOAT, .StartState = RESOURCE_STATE_DEPTH_WRITE, .GetRenderTarget = true });
    hz::GPUBuffer counters =
        context.CreateBuffer({ .Size = 16,
                               .ElementCount = 4,
                               .usage = RESOURCE_MEMORY_USAGE_GPU_ONLY,
                               .StartState = RESOURCE_STATE_UNORDERED_ACCESS,
                               .Descriptors = (DescriptorType)(DESCRIPTOR_TYPE_BUFFER_RAW | DESCRIPTOR_TYPE_RW_BUFFER_RAW) });
    hz::GPUBuffer pixels = context.CreateBuffer({ .Size = 64 * 16,
                                                  .ElementCount = 64,
                                                  .StructStride = 16,
                                                  .usage = RESOURCE_MEMORY_USAGE_GPU_ONLY,
                                                  .StartState = RESOURCE_STATE_UNORDERED_ACCESS,
                                                  .Descriptors = DESCRIPTOR_TYPE_RW_BUFFER });
    hz::GPUBuffer readback = context.CreateBuffer({ .Size = 64 * 16 + 16,
                                                    .usage = RESOURCE_MEMORY_USAGE_GPU_TO_CPU,
                                                    .StartState = RESOURCE_STATE_COPY_DEST,
                                                    .Flags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT });
    SetResourceDirectoryPath(&fileSystem, MountProject, DirectoryShaderSources, shaders.string().c_str());
    ClusteredLighting clusters(context);
    hz::FrameUpload uploadArena(context, 3);
    uploadArena.BeginFrame(1);
    LightingFrameData glassFrame;
    // Both lights cover the front glass, on opposite sides of it. Neither reaches the back pane.
    hz::RenderLight   glassLights[2];
    hz::GpuLightData  gpuLights[2];
    const uint32_t    ids[] = { 0, 1 };
    for (uint32_t i = 0; i < 2; ++i)
    {
        const Vector3 pos(0.125f, 0.125f, i ? 0.28f : 0.12f);
        glassLights[i].transform = Matrix4::translation(pos);
        glassLights[i].ready = 1;
        glassLights[i].InfluenceRadius = 0.20f;
        gpuLights[i] = { .PositionAndInvRadius = Vector4(pos, 5), .DirectionAndType = Vector4(0, 0, -1, 1), .Color = Vector4(1, 1, 1, 0) };
    }
    FrameData clusterCamera = camera;
    clusterCamera.ViewProjection.setRow(3, Vector4(0, 0, 1, 0));
    hz::GPUBuffer masks = context.CreateBuffer(ClusteredLighting::GetTileMaskDesc().Resolve(clusterCamera.Width, clusterCamera.Height));
    ASSERT_TRUE(clusters.Prepare(glassFrame, uploadArena, masks, glassLights, ids, clusterCamera));
    std::vector<uint32_t>    glassIndices(std::begin(ids), std::end(ids));
    const hz::Span<uint32_t> glassOrder = clusters.GetLightIndices();
    glassIndices.insert(glassIndices.end(), glassOrder.begin(), glassOrder.end());
    const hz::GPUBuffer     glassBuffer = input(gpuLights, sizeof(gpuLights)),
                            glassIds = input(glassIndices.data(), uint32_t(glassIndices.size() * sizeof(uint32_t)));
    glassFrame.LocalLightsIndex = glassBuffer.GetSrvIndex();
    glassFrame.LocalLightCount = 2;
    glassFrame.ActiveLocalLightsIndex = glassIds.GetSrvIndex();
    const hz::GPUBuffer glassLighting = input(&glassFrame, sizeof(glassFrame), sizeof(glassFrame));
    for (uint32_t scenario = 0; scenario < 5; ++scenario)
    {
        SCOPED_TRACE(scenario);
        const uint32_t Capacity = scenario == 3 ? 1 : 128;
        hz::GPUBuffer  nodes =
            context.CreateBuffer({ .Size = Capacity * 16,
                                   .ElementCount = Capacity * 4,
                                   .usage = RESOURCE_MEMORY_USAGE_GPU_ONLY,
                                   .StartState = RESOURCE_STATE_UNORDERED_ACCESS,
                                   .Descriptors = (DescriptorType)(DESCRIPTOR_TYPE_BUFFER_RAW | DESCRIPTOR_TYPE_RW_BUFFER_RAW) });
        hz::CommandList& commands = context.AcquireCommandList();
        commands.BeginRendering({ .colorAttachments = { { .pTexture = &opaque,
                                                          .loadAction = LOAD_ACTION_CLEAR,
                                                          .storeAction = STORE_ACTION_STORE,
                                                          .ClearValue = { .b = 1, .a = 1 } } } });
        commands.EndRendering();
        uint32_t constants[28];
        for (uint32_t& value : constants)
        {
            value = UINT32_MAX;
        }
        constants[3] = frame.GetSrvIndex();
        constants[4] = instanceBuffer.GetSrvIndex();
        constants[5] = materialBuffer.GetSrvIndex();
        constants[7] = sampler.GetIndex();
        constants[8] = vertices.GetSrvIndex();
        constants[9] = normal.GetSrvIndex();
        constants[10] = uv.GetSrvIndex();
        constants[12] = heads.GetUavIndex();
        constants[13] = counters.GetUavIndex();
        constants[14] = nodes.GetUavIndex();
        constants[15] = Capacity;
        const hz::GPUBuffer& surfaceLighting = scenario == 4 ? glassLighting : light;
        constants[16] = surfaceLighting.GetSrvIndex();
        if (scenario == 4)
        {
            clusters.BuildLightTileMasks(commands, clusterCamera, masks, glassBuffer, glassIds, glassFrame.LocalLightCount);
        }
        commands.SetPipeline(Clear);
        commands.SetRootConstant(constants);
        commands.Dispatch(1, 1, 1, { .storageTextures = { &heads }, .storageBuffers = { &counters } });
        commands.BeginRendering(
            { .depthAttachment = { .pTexture = &Depth,
                                   .loadAction = LOAD_ACTION_CLEAR,
                                   .storeAction = STORE_ACTION_STORE,
                                   .ClearValue = { .Depth = scenario == 2 ? 0.5f : 1.0f } } },
            { .storageTextures = { &heads },
              .buffers = { &frame, &surfaceLighting, &instanceBuffer, &materialBuffer, &vertices, &normal, &uv,
                           &glassBuffer, &glassIds, &uploadArena.GetBuffer(), &masks },
              .storageBuffers = { &counters, &nodes } });
        commands.SetViewport(0, 0, 8, 8);
        commands.SetScissor(0, 0, 8, 8);
        commands.SetPipeline(gather);
        for (uint32_t Draw = 0; Draw < 2; ++Draw)
        {
            constants[0] = constants[1] = scenario == 1 ? 1 - Draw : Draw;
            commands.SetRootConstant(constants);
            commands.Draw(3);
        }
        commands.EndRendering();
        constants[12] = heads.GetSrvIndex();
        constants[14] = nodes.GetSrvIndex();
        constants[23] = opaque.GetSrvIndex();
        constants[24] = constants[25] = constants[27] = 0;
        constants[26] = result.GetUavIndex();
        commands.SetPipeline(resolve);
        commands.SetRootConstant(constants);
        commands.Dispatch(1, 1, 1,
                          { .sampledTextures = { &heads, &opaque },
                            .storageTextures = { &result },
                            .buffers = { &nodes },
                            .storageBuffers = { &counters } });
        commands.SetPipeline(reader);
        commands.SetRootConstant({ result.GetSrvIndex(), pixels.GetUavIndex(), 0 });
        commands.Dispatch(1, 1, 1, { .sampledTextures = { &result }, .storageBuffers = { &pixels } });
        commands.CopyBuffer(readback, 0, pixels, 0, 64 * 16);
        commands.CopyBuffer(readback, 64 * 16, counters, 0, 16);
        context.Wait(context.Submit(commands));
        const float*    color = (const float*)readback.Get()->pCpuMappedAddress;
        const uint32_t* stats = (const uint32_t*)((const uint8_t*)color + 64 * 16);
        EXPECT_EQ(stats[0], scenario == 2 ? 64u : 128u);
        EXPECT_EQ(stats[1], scenario == 3 ? 127u : 0u);
        EXPECT_EQ(stats[2], 0u);
        double sum[3] = {};
        for (uint32_t pixel = 0; pixel < 64; ++pixel)
        {
            for (uint32_t channel = 0; channel < 3; ++channel)
            {
                sum[channel] += color[pixel * 4 + channel];
            }
            if (scenario < 3)
            {
                EXPECT_NEAR(color[pixel * 4], 0.5, 1e-5);
                EXPECT_NEAR(color[pixel * 4 + 1], scenario == 2 ? 0.0 : 0.25, 1e-5);
                EXPECT_NEAR(color[pixel * 4 + 2], scenario == 2 ? 0.5 : 0.25, 1e-5);
            }
        }
        if (scenario == 4)
        {
            EXPECT_GT(sum[0], 32.0);
        }
        if (scenario == 3)
        {
            EXPECT_NEAR(sum[0] + sum[1], 0.5, 1e-5);
            EXPECT_NEAR(sum[2], 63.5, 1e-5);
        }
    }
}

TEST_F(SceneAssetLoaderTest, CookbookPostExposureToneMapsAndBloomChainValues)
{
    const std::filesystem::path shaders =
        std::filesystem::absolute(std::filesystem::path(__FILE__).parent_path().parent_path().parent_path() / "Examples/Renderer/Shaders");
    SetResourceDirectoryPath(&fileSystem, MountProject, DirectoryOtherFiles, shaders.string().c_str());
    hz::RenderContext context({ .pAppName = "CookbookPostTest", .enableGpuValidation = true });
    const auto        compute = [&](const char* file, const char* entry)
    {
        return context.CreateComputePipeline({ .shaderDesc = { .stages = { { .stage = SHADER_STAGE_COMP, .pEntryPoint = entry } },
                                                               .sourceDirectory = DirectoryOtherFiles,
                                                               .pFileName = file } });
    };
    hz::GPUPipeline adapt = compute("PostLuminance.slang", "CSAdapt");
    hz::GPUPipeline metering = compute("PostMetering.slang", "CSMain");
    hz::GPUPipeline downsample = compute("PostBloomDownsample.slang", "CSMain");
    hz::GPUPipeline upsample = compute("PostBloomUpsample.slang", "CSMain");
    hz::GPUPipeline tone =
        context.CreateGraphicsPipeline({ .shaderDesc = { .stages = { { .stage = SHADER_STAGE_VERT, .pEntryPoint = "VSMain" },
                                                                     { .stage = SHADER_STAGE_FRAG, .pEntryPoint = "PSMain" } },
                                                         .sourceDirectory = DirectoryOtherFiles,
                                                         .pFileName = "PostToneMap.slang" },
                                         .colorTargets = { { .Format = hz::Format::R32G32B32A32_SFLOAT } } });
    hz::GPUPipeline deferred =
        context.CreateGraphicsPipeline({ .shaderDesc = { .stages = { { .stage = SHADER_STAGE_VERT, .pEntryPoint = "VSMain" },
                                                                     { .stage = SHADER_STAGE_FRAG, .pEntryPoint = "PSMain" } },
                                                         .sourceDirectory = DirectoryOtherFiles,
                                                         .pFileName = "DeferredLighting.slang" },
                                         .colorTargets = { { .Format = hz::Format::R32G32B32A32_SFLOAT } } });
    const std::filesystem::path editorShaders = shaders.parent_path().parent_path().parent_path() / "Source/Editor/Shaders";
    SetResourceDirectoryPath(&fileSystem, MountProject, DirectoryOtherFiles, editorShaders.string().c_str());
    hz::GPUPipeline outline = context.CreateGraphicsPipeline({
        .shaderDesc = { .stages = { { .stage = SHADER_STAGE_VERT, .pEntryPoint = "VSMain" }, { .stage = SHADER_STAGE_FRAG, .pEntryPoint = "PSMain" } },
                        .sourceDirectory = DirectoryOtherFiles, .pFileName = "SelectionOutline.slang" },
        .colorTargets = { { .Format = hz::Format::R32G32B32A32_SFLOAT, .srcFactor = BC_SRC_ALPHA, .dstFactor = BC_ONE_MINUS_SRC_ALPHA } } });
    ASSERT_TRUE(outline.IsValid());
    SetResourceDirectoryPath(&fileSystem, MountProject, DirectoryOtherFiles, directory.string().c_str());

    const char seedSource[] = R"hlsl(
        cbuffer RootConstant0 { uint outputIndex; uint peakX; };
        [numthreads(8,8,1)] void CSMeteringScene(uint3 id : SV_DispatchThreadID) {
            RWTexture2D<float4> output = ResourceDescriptorHeap[outputIndex];
            uint w,h; output.GetDimensions(w,h);
            if(id.x < w && id.y < h) {
                float luminance = id.x < 32 ? 0.0 : id.x == 32 ? 0.125 : id.x == 64 ? 1048576.0 : 1024.0;
                output[id.xy] = float4(luminance, luminance, luminance, 1.0);
            }
        }
        [numthreads(6,1,1)] void CSSelectionMask(uint3 id : SV_DispatchThreadID) {
            RWTexture2D<float> output = ResourceDescriptorHeap[outputIndex];
            output[id.xy] = id.x < 3 ? 1.0 : 0.0;
        }
        [numthreads(8,8,1)] void CSMovingPeak(uint3 id : SV_DispatchThreadID) {
            RWTexture2D<float4> output = ResourceDescriptorHeap[outputIndex];
            uint w,h; output.GetDimensions(w,h);
            float3 peak = peakX == 0 ? float3(asfloat(0x7fc00000), 0, 0) : float3(270,270,270);
            if(id.x < w && id.y < h) output[id.xy] = float4(all(id.xy == uint2(peakX, 0)) ? peak : float3(0,0,0), 1);
        }
    )hlsl";
    const auto helper = [&](const char* source, uint32_t Size, const char* entry)
    {
        return context.CreateComputePipeline(
            { .shaderDesc = {
                  .stages = { { .stage = SHADER_STAGE_COMP, .pSource = source, .sourceSize = Size, .pEntryPoint = entry } } } });
    };

    hz::GPUPipeline seedMetering = helper(seedSource, sizeof(seedSource) - 1, "CSMeteringScene");
    hz::GPUPipeline seedSelection = helper(seedSource, sizeof(seedSource) - 1, "CSSelectionMask");
    hz::GPUPipeline movingPeak = helper(seedSource, sizeof(seedSource) - 1, "CSMovingPeak");
    hz::GPUPipeline reader = helper(kCookbookTextureReadback, sizeof(kCookbookTextureReadback) - 1, "CSMain");
    ASSERT_TRUE(adapt.IsValid() && metering.IsValid() && downsample.IsValid() &&
                upsample.IsValid() && tone.IsValid() && seedMetering.IsValid() && movingPeak.IsValid() && reader.IsValid() &&
                deferred.IsValid());
    const auto image = [&](uint32_t Width, uint32_t Height, hz::Format Format)
    {
        return context.CreateTexture({ .Width = Width,
                                       .Height = Height,
                                       .Format = Format,
                                       .StartState = RESOURCE_STATE_RENDER_TARGET,
                                       .Descriptors = (DescriptorType)(DESCRIPTOR_TYPE_TEXTURE | DESCRIPTOR_TYPE_RW_TEXTURE),
                                       .Flags = TEXTURE_CREATION_FLAG_FORCE_2D,
                                       .GetRenderTarget = true });
    };
    constexpr hz::Format rgba = hz::Format::R32G32B32A32_SFLOAT, scalar = hz::Format::R32_SFLOAT;
    hz::GPUTexture       source = image(1, 1, rgba), black = image(1, 1, rgba), average = image(1, 1, scalar), mapped = image(1, 1, rgba);
    hz::GPUTexture       meteringScene = image(65, 3, rgba), histogram = image(64, 4, hz::Format::R32_UINT);
    hz::GPUTexture       previous = image(1, 1, scalar), adapted = image(1, 1, scalar);
    hz::GPUTexture       impulse = image(9, 9, rgba), blurred = image(9, 9, rgba);
    hz::GPUSampler       sampler = context.CreateSampler(
        { .AddressU = ADDRESS_MODE_CLAMP_TO_EDGE, .AddressV = ADDRESS_MODE_CLAMP_TO_EDGE, .AddressW = ADDRESS_MODE_CLAMP_TO_EDGE });
    hz::GPUBuffer output = context.CreateBuffer({ .Size = 256 * 16,
                                                  .ElementCount = 256,
                                                  .StructStride = 16,
                                                  .usage = RESOURCE_MEMORY_USAGE_GPU_ONLY,
                                                  .StartState = RESOURCE_STATE_UNORDERED_ACCESS,
                                                  .Descriptors = DESCRIPTOR_TYPE_RW_BUFFER });
    hz::GPUBuffer readback = context.CreateBuffer({ .Size = 256 * 16,
                                                    .usage = RESOURCE_MEMORY_USAGE_GPU_TO_CPU,
                                                    .StartState = RESOURCE_STATE_COPY_DEST,
                                                    .Flags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT });

    hz::GPUTexture peakSource = image(2, 2, rgba), peakBright = image(1, 1, rgba);
    hz::GPUTexture peakNormal = image(1, 1, rgba), peakBase = image(1, 1, rgba);

    const FrameData   peakCamera = { .Eye = Vector4(0, 0, 2, 1), .Near = 0.01f, .Far = 2, .Width = 1, .Height = 1 };
    LightingFrameData peakLight;
    peakLight.LightDirection = Vector4(0, 0, -1, 100000);
    const auto Upload = [&](const void* data, uint32_t Size)
    {
        return context.CreateBuffer({ .Size = Size,
                                      .ElementCount = 1,
                                      .StructStride = Size,
                                      .pInitialData = data,
                                      .initialDataSize = Size,
                                      .usage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU,
                                      .StartState = RESOURCE_STATE_SHADER_RESOURCE,
                                      .Descriptors = DESCRIPTOR_TYPE_BUFFER });
    };
    hz::GPUBuffer    peakFrame = Upload(&peakCamera, sizeof(peakCamera)), peakLighting = Upload(&peakLight, sizeof(peakLight));
    hz::GpuLightData lights[] = {
        { .PositionAndInvRadius = Vector4(0.0f), .DirectionAndType = Vector4(0, 0, -1, 0), .Color = Vector4(1, 0, 0, 0) },
        { .PositionAndInvRadius = Vector4(0.0f, 0.0f, 1.18f, 0.01f),
          .DirectionAndType = Vector4(0, 0, -1, 1),
          .Color = Vector4(0, 1, 0, 0) },
        { .PositionAndInvRadius = Vector4(0.0f, 0.0f, 1.18f, 0.01f),
          .DirectionAndType = Vector4(0, 0, -1, 2),
          .Color = Vector4(0, 0, 1, 0),
          .SpotAngles = float2(0.5f, 4.0f) },
        { .PositionAndInvRadius = Vector4(0.0f, 0.0f, 1.18f, 4.0f),
          .DirectionAndType = Vector4(0, 0, -1, 1),
          .Color = Vector4(10, 10, 10, 0) },
        { .PositionAndInvRadius = Vector4(0.0f, 0.0f, 1.18f, 0.01f),
          .DirectionAndType = Vector4(1, 0, 0, 2),
          .Color = Vector4(10, 10, 10, 0),
          .SpotAngles = float2(0.5f, 4.0f) },
        { .DirectionAndType = Vector4(0, 0, -1, 0), .Color = Vector4(10, 10, 10, 0) },
        { .DirectionAndType = Vector4(0, 0, -1, 0), .Color = Vector4(10, 10, 10, 0) },
    };
    const hz::GPUBuffer lightList = context.CreateBuffer({ .Size = sizeof(lights),
                                                           .ElementCount = TF_ARRAY_COUNT(lights) * (sizeof(hz::GpuLightData) / sizeof(uint32_t)),
                                                           .pInitialData = lights,
                                                           .initialDataSize = sizeof(lights),
                                                           .usage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU,
                                                           .StartState = RESOURCE_STATE_SHADER_RESOURCE,
                                                           .Descriptors = DESCRIPTOR_TYPE_BUFFER_RAW });
    const uint32_t activeLights[] = { 4, 0, 2, 1, 3, 5 };
    SetResourceDirectoryPath(&fileSystem, MountProject, DirectoryShaderSources, shaders.string().c_str());
    ClusteredLighting clusters(context);
    hz::FrameUpload uploadArena(context, 3);
    uploadArena.BeginFrame(1);
    LightingFrameData localFrame = { .LightDirection = Vector4(0, 0, -1, 1), .LightColor = Vector4(1, 0, 0, 1) };
    hz::RenderLight   sourceLights[TF_ARRAY_COUNT(lights)];
    for (uint32_t i = 0; i < TF_ARRAY_COUNT(lights); ++i)
    {
        sourceLights[i].Type = hz::LightType(int(float(lights[i].DirectionAndType.getW())));
        sourceLights[i].transform = Matrix4::translation(lights[i].PositionAndInvRadius.getXYZ());
        const float invRadius = lights[i].PositionAndInvRadius.getW();
        sourceLights[i].InfluenceRadius = invRadius > 0 ? 1.0f / invRadius : 0;
        sourceLights[i].ready = 1;
    }
    FrameData clusterCamera = peakCamera;
    clusterCamera.ViewProjection.setRow(3, Vector4(0, 0, 1, 0));
    hz::GPUBuffer masks = context.CreateBuffer(ClusteredLighting::GetTileMaskDesc().Resolve(clusterCamera.Width, clusterCamera.Height));
    ASSERT_TRUE(clusters.Prepare(localFrame, uploadArena, masks, sourceLights, activeLights, clusterCamera));
    std::vector<uint32_t>    localIndices = { 4, 2, 1, 3 };
    const uint32_t           localCount = uint32_t(localIndices.size());
    const hz::Span<uint32_t> localOrder = clusters.GetLightIndices();
    localIndices.insert(localIndices.end(), localOrder.begin(), localOrder.end());
    const uint32_t      localIndexBytes = uint32_t(localIndices.size() * sizeof(uint32_t));
    const hz::GPUBuffer activeLightList = context.CreateBuffer({ .Size = localIndexBytes,
                                                                 .ElementCount = uint32_t(localIndices.size()),
                                                                 .pInitialData = localIndices.data(),
                                                                 .initialDataSize = localIndexBytes,
                                                                 .usage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU,
                                                                 .StartState = RESOURCE_STATE_SHADER_RESOURCE,
                                                                 .Descriptors = DESCRIPTOR_TYPE_BUFFER_RAW });
    ASSERT_TRUE(lightList.IsValid() && activeLightList.IsValid());
    localFrame.LocalLightsIndex = lightList.GetSrvIndex();
    localFrame.LocalLightCount = localCount;
    localFrame.ActiveLocalLightsIndex = activeLightList.GetSrvIndex();
    const hz::GPUBuffer localLighting = Upload(&localFrame, sizeof(localFrame));
    hz::CommandList& commands = context.AcquireCommandList();
    const auto       Clear = [&](const hz::GPUTexture& texture, float value)
    {
        commands.BeginRendering({ .colorAttachments = { { .pTexture = &texture,
                                                          .loadAction = LOAD_ACTION_CLEAR,
                                                          .storeAction = STORE_ACTION_STORE,
                                                          .ClearValue = { .r = value, .g = value, .b = value, .a = 1 } } } });
        commands.EndRendering();
    };
    Clear(source, 1.2f);
    Clear(black, 0);
    Clear(average, 0.18f);
    const auto read = [&](const hz::GPUTexture& texture, uint32_t offset)
    {
        commands.SetPipeline(reader);
        commands.SetRootConstant({ texture.GetSrvIndex(), output.GetUavIndex(), offset });
        commands.Dispatch((texture.GetWidth() + 7) / 8, (texture.GetHeight() + 7) / 8, 1,
                          { .sampledTextures = { &texture }, .storageBuffers = { &output } });
    };
    struct ToneConstants
    {
        uint32_t source, average, bloom, sampler, mode;
        float    exposure = 1, bloomStrength = 0, maxWhite = 2, P = 1, a = 1.05f, m = 0.1f, l = 0.8f, c = 3, b = 0, compression = 0.8f,
              desaturation = 0.15f;
        uint32_t outputMode = 0, encodeSRGB = 0;
        float    paperWhite = 200, peak = 1000;
    } toneConstants = { source.GetSrvIndex(), average.GetSrvIndex(), black.GetSrvIndex(), sampler.GetIndex(), 0 };
    for (uint32_t mode = 0; mode < 4; ++mode)
    {
        commands.BeginRendering(
            { .colorAttachments = { { .pTexture = &mapped, .loadAction = LOAD_ACTION_DONTCARE, .storeAction = STORE_ACTION_STORE } } },
            { .sampledTextures = { &source, &average, &black } });
        commands.SetViewport(0, 0, 1, 1);
        commands.SetScissor(0, 0, 1, 1);
        commands.SetPipeline(tone);
        toneConstants.mode = mode;
        commands.SetRootConstant({ (const uint32_t*)&toneConstants, sizeof(toneConstants) / 4 });
        commands.Draw(3);
        commands.EndRendering();
        read(mapped, mode);
    }
    hz::GPUTexture selection = image(6, 1, scalar), outlined = image(6, 1, rgba);
    commands.SetPipeline(seedSelection);
    commands.SetRootConstant({ selection.GetUavIndex() });
    commands.Dispatch(1, 1, 1, { .storageTextures = { &selection } });
    Clear(outlined, 1.2f);
    commands.BeginRendering(
        { .colorAttachments = { { .pTexture = &outlined, .loadAction = LOAD_ACTION_LOAD, .storeAction = STORE_ACTION_STORE } } },
        { .sampledTextures = { &selection } });
    commands.SetViewport(0, 0, 6, 1);
    commands.SetScissor(0, 0, 6, 1);
    commands.SetPipeline(outline);
    commands.SetRootConstant({ selection.GetSrvIndex(), sampler.GetIndex(), 0, 0, 0, 0 });
    commands.Draw(3);
    commands.EndRendering();
    read(outlined, 190);



    // Normal incidence, white dielectric, specular 8/16. Test the smooth limit and a rough lobe as well as the HDR peak.
    clusters.BuildLightTileMasks(commands, clusterCamera, masks, lightList, activeLightList, localCount);
    constexpr uint32_t roughnessCodes[] = { 0, 51, 255, 255 };
    constexpr uint32_t lightingOffsets[] = { 182, 15, 183, 184 };
    uint32_t           peakConstants[15];
    for (uint32_t& value : peakConstants)
    {
        value = UINT32_MAX;
    }
    peakConstants[0] = peakFrame.GetSrvIndex();
    peakConstants[1] = peakLighting.GetSrvIndex();
    peakConstants[2] = black.GetSrvIndex();
    peakConstants[3] = peakNormal.GetSrvIndex();
    peakConstants[4] = peakBase.GetSrvIndex();
    peakConstants[5] = average.GetSrvIndex();
    for (uint32_t probe = 0; probe < TF_ARRAY_COUNT(roughnessCodes); ++probe)
    {
        commands.BeginRendering({ .colorAttachments = { { .pTexture = &peakNormal,
                                                          .loadAction = LOAD_ACTION_CLEAR,
                                                          .storeAction = STORE_ACTION_STORE,
                                                          .ClearValue = { .r = .5f,
                                                                          .g = .5f,
                                                                          .b = float(roughnessCodes[probe]) / 1023.0f,
                                                                          .a = 2.0f / 3.0f } },
                                                        { .pTexture = &peakBase,
                                                          .loadAction = LOAD_ACTION_CLEAR,
                                                          .storeAction = STORE_ACTION_STORE,
                                                          .ClearValue = { .r = 1, .g = 1, .b = 1, .a = 0 } } } });
        commands.EndRendering();
        const hz::GPUBuffer& lighting = probe == 3 ? localLighting : peakLighting;
        peakConstants[1] = lighting.GetSrvIndex();
        commands.SetPipeline(deferred);
        commands.BeginRendering(
            { .colorAttachments = { { .pTexture = &mapped, .loadAction = LOAD_ACTION_DONTCARE, .storeAction = STORE_ACTION_STORE } } },
            { .sampledTextures = { &black, &average, &peakNormal, &peakBase },
              .buffers = { &peakFrame, &lighting, &lightList, &activeLightList, &uploadArena.GetBuffer(),
                           &masks } });
        commands.SetViewport(0, 0, 1, 1);
        commands.SetScissor(0, 0, 1, 1);
        commands.SetRootConstant(peakConstants);
        commands.Draw(3);
        commands.EndRendering();
        read(mapped, lightingOffsets[probe]);
    }
    const auto meter = [&](const hz::GPUTexture& input)
    {
        commands.SetPipeline(metering);
        commands.SetRootConstant({ input.GetSrvIndex(), histogram.GetUavIndex() });
        commands.Dispatch(histogram.GetHeight(), 1, 1, { .sampledTextures = { &input }, .storageTextures = { &histogram } });
    };
    const auto adaptExposure = [&](bool historyValid, float seconds, uint32_t offset)
    {
        hz::DependencyList dependencies;
        const uint32_t bindings[] = { dependencies.Sample(&histogram), dependencies.Store(&adapted),
                                      dependencies.Sample(historyValid ? &previous : nullptr) };
        commands.SetPipeline(adapt);
        commands.SetRootConstant(bindings);
        commands.SetRootConstants(seconds, 3);
        commands.Dispatch(1, 1, 1, dependencies.Get());
        read(adapted, offset);
    };
    meter(black);
    adaptExposure(false, 0.0f, 8);
    commands.SetPipeline(seedMetering);
    commands.SetRootConstant({ meteringScene.GetUavIndex() });
    commands.Dispatch(9, 1, 1, { .storageTextures = { &meteringScene } });
    meter(meteringScene);
    adaptExposure(false, 0.0f, 12);
    Clear(previous, 128.0f);
    adaptExposure(true, 1.0f / 3.0f, 13);
    Clear(previous, 8192.0f);
    adaptExposure(true, 1.0f, 14);
    Clear(previous, 512.0f);
    adaptExposure(true, 1.0f / 60.0f, 9);
    struct DownsampleConstants
    {
        uint32_t input, output, sampler, adapted, threshold, autoExposure;
        float    exposure;
    };
    struct UpsampleConstants
    {
        uint32_t down, input, output, sampler;
    };
    // Bloom chain: the 2x2 box keeps a constant, and the tent upsample adds it back on top of its own level.
    hz::GPUTexture bloomSmall = image(4, 4, rgba), bloomWide = image(9, 9, rgba);
    Clear(impulse, 2.0f);
    Clear(bloomWide, 1.0f);
    const DownsampleConstants boxConstants = { impulse.GetSrvIndex(), bloomSmall.GetUavIndex(), sampler.GetIndex(),
                                               average.GetSrvIndex(), 0, 0, 1 };
    commands.SetPipeline(downsample);
    commands.SetRootConstant({ (const uint32_t*)&boxConstants, sizeof(boxConstants) / 4 });
    commands.Dispatch(1, 1, 1, { .sampledTextures = { &impulse, &average }, .storageTextures = { &bloomSmall } });
    const UpsampleConstants tentConstants = { bloomWide.GetSrvIndex(), bloomSmall.GetSrvIndex(), blurred.GetUavIndex(), sampler.GetIndex() };
    commands.SetPipeline(upsample);
    commands.SetRootConstant({ (const uint32_t*)&tentConstants, sizeof(tentConstants) / 4 });
    commands.Dispatch(1, 1, 1, { .sampledTextures = { &bloomWide, &bloomSmall }, .storageTextures = { &blurred } });
    read(blurred, 16);
    // Real output shader: retain the HDR shoulder for gray 1.2 above paper white.
    toneConstants.mode = 0;
    toneConstants.paperWhite = 203;
    toneConstants.peak = 1000;
    for (uint32_t mode = 1; mode <= 2; ++mode)
    {
        toneConstants.outputMode = mode;
        commands.SetPipeline(tone);
        commands.BeginRendering(
            { .colorAttachments = { { .pTexture = &mapped, .loadAction = LOAD_ACTION_DONTCARE, .storeAction = STORE_ACTION_STORE } } },
            { .sampledTextures = { &source, &average, &black } });
        commands.SetViewport(0, 0, 1, 1);
        commands.SetScissor(0, 0, 1, 1);
        commands.SetRootConstant({ reinterpret_cast<const uint32_t*>(&toneConstants), sizeof(toneConstants) / 4 });
        commands.Draw(3);
        commands.EndRendering();
        read(mapped, 177 + mode);
    }
    // A 2x2 footprint is one box step: a 270 texel keeps a quarter and the soft knee passes 66.5; a NaN texel is dropped.
    for (uint32_t x = 0; x < 2; ++x)
    {
        commands.SetPipeline(movingPeak);
        commands.SetRootConstant({ peakSource.GetUavIndex(), x });
        commands.Dispatch(1, 1, 1, { .storageTextures = { &peakSource } });
        const DownsampleConstants movingConstants = { peakSource.GetSrvIndex(), peakBright.GetUavIndex(), sampler.GetIndex(),
                                                      average.GetSrvIndex(), 1, 1, 1 };
        commands.SetPipeline(downsample);
        commands.SetRootConstant({ (const uint32_t*)&movingConstants, sizeof(movingConstants) / 4 });
        commands.Dispatch(1, 1, 1, { .sampledTextures = { &peakSource, &average }, .storageTextures = { &peakBright } });
        read(peakBright, 180 + x);
    }

    commands.CopyBuffer(readback, 0, output, 0, 256 * 16);

    context.Wait(context.Submit(commands));
    const float* values = (const float*)readback.Get()->pCpuMappedAddress;
    ASSERT_NE(values, nullptr);
    for (uint32_t x = 0; x < 2; ++x)
    {
        for (uint32_t channel = 0; channel < 3; ++channel)
        {
            EXPECT_NEAR(values[(180 + x) * 4 + channel], x == 0 ? 0.0f : 66.5f, 2e-5) << x;
        }
    }
    // Cookbook shared/Tonemap.h reference goldens: gray 1.2, white 2 and default Uchimura/PBR-neutral controls, linear SDR.
    const double golden[] = { 1.2, 0.7090909090909091, 0.9839406668663332, 0.91 };
    for (uint32_t mode = 0; mode < 4; ++mode)
    {
        for (uint32_t channel = 0; channel < 3; ++channel)
        {
            EXPECT_NEAR(values[mode * 4 + channel], golden[mode], 2e-5) << mode;
        }
    }
    EXPECT_FLOAT_EQ(values[8 * 4], std::exp2(-10.0f));
    // Offline double goldens: shoulder -> 242.58323517119788 nits; ST2084 PQ and nits/80 respectively.
    const double hdrGolden[] = { 0.5993841391362755, 3.0322904396399735 };
    for (uint32_t mode = 0; mode < 2; ++mode)
    {
        for (uint32_t channel = 0; channel < 3; ++channel)
        {
            EXPECT_NEAR(values[(178 + mode) * 4 + channel], hdrGolden[mode], 2e-5) << mode << ":" << channel;
        }
    }
    // At normal incidence the grazing term is zero. Evaluate the analytic directional albedo in double precision.
    const double f0 = 0.08 * 8.0 / 16.0, pi = 3.141592653589793;
    for (uint32_t probe = 0; probe < TF_ARRAY_COUNT(roughnessCodes); ++probe)
    {
        const double roughness = roughnessCodes[probe] / 255.0;
        const double albedo = roughness > 0.0 ? 1.0 - std::pow(roughness, 1.0 / roughness) * ((roughness + 0.0266916) / 1.466495)
                                             : 1.0;
        const double specularWeight = 1.0 + f0 * (1.0 - albedo) / albedo;
        const double diffuseWeight = 1.0 - specularWeight * albedo * f0;
        const double alphaSquared = std::fmax(std::pow(roughness, 4), 2e-6);
        // The last probe has 1 lux of each RGB primary from the directional, point and spot lights.
        const double illuminance = probe >= 3 ? 1.0 : 100000.0;
        const double expected = illuminance * (diffuseWeight / pi + specularWeight * f0 / (4.0 * pi * alphaSquared));
        for (uint32_t channel = 0; channel < 3; ++channel)
        {
            const float value = values[lightingOffsets[probe] * 4 + channel];
            EXPECT_TRUE(std::isfinite(value));
            EXPECT_NEAR(value, expected, expected * 2e-5) << roughness;
        }
    }
    for (uint32_t x = 0; x < 6; ++x)
    {
        const float* color = values + (190 + x) * 4;
        const bool edge = x == 1 || x == 2;
        EXPECT_NEAR(color[0], edge ? 1.0f : 1.2f, 1e-5f);
        EXPECT_NEAR(color[1], edge ? 0.32f : 1.2f, 1e-5f);
        EXPECT_NEAR(color[2], edge ? 0.02f : 1.2f, 1e-5f);
    }
    EXPECT_GT(values[15 * 4], 65504.0f);
    EXPECT_NEAR(values[12 * 4], 1024.0f, 0.01f); // Black background and both outliers are excluded.
    EXPECT_NEAR(values[13 * 4], 256.0f, 0.01f);
    EXPECT_NEAR(values[14 * 4], 4096.0f, 0.05f);
    EXPECT_NEAR(values[9 * 4], 512.0f * std::exp2(3.0f / (60.0f * 1.5f)), 0.01f);
    // 2x2 box of 2.0 plus a 3x3 tent of 2.0 on top of a level cleared to 1.0 is 3.0 at every texel.
    for (uint32_t y = 0; y < 9; ++y)
    {
        for (uint32_t x = 0; x < 9; ++x)
        {
            for (uint32_t channel = 0; channel < 3; ++channel)
            {
                EXPECT_NEAR(values[(16 + y * 9 + x) * 4 + channel], 3.0f, 2e-5) << x << "," << y;
            }
        }
    }
}

TEST_F(SceneAssetLoaderTest, RendererMaskUsesReferenceDitherAndDistanceCutoffWhileOpaqueIgnoresAlpha)
{
    hz::RenderContext           context({ .pAppName = "RendererMaskAlphaTest", .enableGpuValidation = true });
    const std::filesystem::path shaders =
        std::filesystem::absolute(std::filesystem::path(__FILE__).parent_path().parent_path().parent_path() / "Examples/Renderer/Shaders");
    SetResourceDirectoryPath(&fileSystem, MountProject, DirectoryOtherFiles, shaders.string().c_str());
    uint32_t normalDDS[36] = {};
    normalDDS[0] = 0x20534444; // DDS magic.
    normalDDS[1] = 124;
    normalDDS[2] = 0x81007;
    normalDDS[3] = normalDDS[4] = 4;
    normalDDS[5] = 16;
    normalDDS[19] = 32;
    normalDDS[20] = 4;
    normalDDS[21] = 0x32495441; // ATI2 / BC5_UNORM.
    normalDDS[27] = 0x1000;
    normalDDS[32] = normalDDS[34] = 0x8080; // Both BC4 blocks decode to 128/255; the texture has no Z channel.
    SetResourceDirectoryPath(&fileSystem, MountProject, DirectoryOtherFiles, directory.string().c_str());
    FileStream normalStream = {};
    ASSERT_TRUE(OpenStreamFromPath(DirectoryOtherFiles, "normal-bc5.dds", FileModeWrite, &normalStream));
    const size_t normalBytes = WriteToStream(&normalStream, normalDDS, sizeof(normalDDS));
    CloseStream(&normalStream);
    ASSERT_EQ(normalBytes, sizeof(normalDDS));
    hz::GPUTexture normalMap = context.LoadTexture(DirectoryOtherFiles, "normal-bc5.dds");
    ASSERT_TRUE(normalMap.IsValid());
    ASSERT_EQ(normalMap.GetFormat(), hz::Format::DXBC5_UNORM);
    SetResourceDirectoryPath(&fileSystem, MountProject, DirectoryOtherFiles, shaders.string().c_str());
    hz::GPUPipeline geometry =
        context.CreateGraphicsPipeline({ .shaderDesc = { .stages = { { .stage = SHADER_STAGE_VERT, .pEntryPoint = "VSMain" },
                                                                     { .stage = SHADER_STAGE_FRAG, .pEntryPoint = "PSMain" } },
                                                         .sourceDirectory = DirectoryOtherFiles,
                                                         .pFileName = "Geometry.slang" },
                                         .colorTargets = { { .Format = hz::Format::R8G8B8A8_UNORM },
                                                           { .Format = hz::Format::R8G8B8A8_UNORM },
                                                           { .Format = hz::Format::R8G8B8A8_UNORM },
                                                           { .Format = hz::Format::R8G8B8A8_UNORM } } });
    SetResourceDirectoryPath(&fileSystem, MountProject, DirectoryOtherFiles, directory.string().c_str());
    hz::GPUPipeline reader =
        context.CreateComputePipeline({ .shaderDesc = { .stages = { { .stage = SHADER_STAGE_COMP,
                                                                      .pSource = kCookbookTextureReadback,
                                                                      .sourceSize = sizeof(kCookbookTextureReadback) - 1,
                                                                      .pEntryPoint = "CSMain" } } } });
    ASSERT_TRUE(geometry.IsValid() && reader.IsValid());
    const auto input = [&](const void* data, uint32_t Size, uint32_t stride = 0)
    {
        return context.CreateBuffer({ .Size = Size,
                                      .ElementCount = stride ? Size / stride : Size / 4,
                                      .StructStride = stride,
                                      .pInitialData = data,
                                      .initialDataSize = Size,
                                      .usage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU,
                                      .StartState = RESOURCE_STATE_SHADER_RESOURCE,
                                      .Descriptors = stride ? DESCRIPTOR_TYPE_BUFFER : DESCRIPTOR_TYPE_BUFFER_RAW,
                                      .Flags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT });
    };
    const auto image = [&](hz::Format Format)
    {
        return context.CreateTexture({ .Width = 4,
                                       .Height = 4,
                                       .Format = Format,
                                       .StartState = RESOURCE_STATE_RENDER_TARGET,
                                       .Descriptors = DESCRIPTOR_TYPE_TEXTURE,
                                       .Flags = TEXTURE_CREATION_FLAG_FORCE_2D,
                                       .GetRenderTarget = true });
    };
    hz::GPUTexture alpha = image(hz::Format::R32G32B32A32_SFLOAT), targets[4];
    for (hz::GPUTexture& target : targets)
    {
        target = image(hz::Format::R8G8B8A8_UNORM);
    }
    const FrameData          camera = { .Eye = Vector4(0, 0, 2, 1) };
    const hz::RenderInstance instance;
    hz::GpuMaterialData      material;
    material.BaseColorTexture = alpha.GetSrvIndex();
    material.NormalTexture = normalMap.GetSrvIndex();
    material.AlphaCutoff = .5f;
    const hz::GpuTextureData   texture[] = { { .id = { 0, 9 }, .srv = alpha.GetSrvIndex() },
                                             { .id = { 1, 9 }, .srv = normalMap.GetSrvIndex() } };
    // Production vertex storage: float3 positions, oct-normal uint, and packed half2 UV.
    const float                positions[] = { -1, -1, .5f, 3, -1, .5f, -1, 3, .5f };
    const uint32_t             normals[] = { 0x80008000, 0x80008000, 0x80008000 };
    const uint32_t             nearUV[] = { 0, 0x00003000, 0x30000000 }; // half(.125): fwidth(uv.x)=1/64 in the 4x4 viewport.
    const uint32_t             farUV[] = { 0, 0x00004800, 0x48000000 };  // half(8): fwidth(uv.x)=1, cutoff becomes .5/32.
    hz::GPUBuffer              frame = input(&camera, sizeof(camera), sizeof(camera)), instances = input(&instance, sizeof(instance));
    hz::GPUBuffer              materials = input(&material, sizeof(material)), textures = input(&texture, sizeof(texture));
    hz::GPUBuffer              vertices = input(positions, sizeof(positions)), normal = input(normals, sizeof(normals)),
                  uv = input(nearUV, sizeof(nearUV));
    hz::GPUSampler sampler = context.CreateSampler();
    hz::GPUBuffer  output = context.CreateBuffer({ .Size = 16 * 16,
                                                   .ElementCount = 16,
                                                   .StructStride = 16,
                                                   .usage = RESOURCE_MEMORY_USAGE_GPU_ONLY,
                                                   .StartState = RESOURCE_STATE_UNORDERED_ACCESS,
                                                   .Descriptors = DESCRIPTOR_TYPE_RW_BUFFER });
    hz::GPUBuffer  readback = context.CreateBuffer({ .Size = 2 * 16 * 16,
                                                     .usage = RESOURCE_MEMORY_USAGE_GPU_TO_CPU,
                                                     .StartState = RESOURCE_STATE_COPY_DEST,
                                                     .Flags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT });
    struct Case
    {
        uint32_t mode;
        float    alpha;
        bool     distant;
        uint32_t coverage;
    };
    const Case cases[] = { { 0, 0, false, 16 }, { 1, 0, false, 0 }, { 1, 1, false, 16 }, { 1, .75f, false, 8 }, { 1, .25f, true, 7 } };
    for (const Case& scenario : cases)
    {
        SCOPED_TRACE(::testing::Message() << scenario.mode << ":" << scenario.alpha << ":" << scenario.distant);
        material.AlphaMode = scenario.mode;
        ASSERT_TRUE(context.UpdateBuffer(materials, 0, &material, sizeof(material)));
        ASSERT_TRUE(context.UpdateBuffer(uv, 0, scenario.distant ? farUV : nearUV, sizeof(nearUV)));
        hz::CommandList& commands = context.AcquireCommandList();
        commands.BeginRendering({ .colorAttachments = { { .pTexture = &alpha,
                                                          .loadAction = LOAD_ACTION_CLEAR,
                                                          .storeAction = STORE_ACTION_STORE,
                                                          .ClearValue = { .r = 1, .g = 1, .b = 1, .a = scenario.alpha } } } });
        commands.EndRendering();
        commands.SetPipeline(geometry);
        commands.BeginRendering(
            { .colorAttachments = { { .pTexture = &targets[0], .loadAction = LOAD_ACTION_CLEAR, .storeAction = STORE_ACTION_STORE },
                                    { .pTexture = &targets[1], .loadAction = LOAD_ACTION_CLEAR, .storeAction = STORE_ACTION_STORE },
                                    { .pTexture = &targets[2], .loadAction = LOAD_ACTION_CLEAR, .storeAction = STORE_ACTION_STORE },
                                    { .pTexture = &targets[3], .loadAction = LOAD_ACTION_CLEAR, .storeAction = STORE_ACTION_STORE } } },
            { .sampledTextures = { &alpha, &normalMap },
              .buffers = { &frame, &instances, &materials, &textures, &vertices, &normal, &uv } });
        commands.SetViewport(0, 0, 4, 4);
        commands.SetScissor(0, 0, 4, 4);
        uint32_t constants[27] = {};
        constants[3] = frame.GetSrvIndex();
        constants[4] = instances.GetSrvIndex();
        constants[5] = materials.GetSrvIndex();
        constants[6] = textures.GetSrvIndex();
        constants[7] = sampler.GetIndex();
        constants[8] = vertices.GetSrvIndex();
        constants[9] = normal.GetSrvIndex();
        constants[10] = uv.GetSrvIndex();
        commands.SetRootConstant(constants);
        commands.Draw(3);
        commands.EndRendering();
        commands.SetPipeline(reader);
        commands.SetRootConstant({ targets[2].GetSrvIndex(), output.GetUavIndex(), 0 });
        commands.Dispatch(1, 1, 1, { .sampledTextures = { &targets[2] }, .storageBuffers = { &output } });
        commands.CopyBuffer(readback, 0, output, 0, 16 * 16);
        commands.SetRootConstant({ targets[1].GetSrvIndex(), output.GetUavIndex(), 0 });
        commands.Dispatch(1, 1, 1, { .sampledTextures = { &targets[1] }, .storageBuffers = { &output } });
        commands.CopyBuffer(readback, 16 * 16, output, 0, 16 * 16);
        context.Wait(context.Submit(commands));
        const float* pixels = (const float*)readback.Get()->pCpuMappedAddress;
        ASSERT_NE(pixels, nullptr);
        uint32_t coverage = 0;
        for (uint32_t i = 0; i < 16; ++i)
        {
            EXPECT_TRUE(pixels[i * 4] == 0.0f || pixels[i * 4] == 1.0f);
            coverage += pixels[i * 4] > .5f ? 1u : 0u;
            if (pixels[i * 4] > .5f)
            {
                // A flat BC5 map must remain +Z (oct-encoded near .5/.5), not flip to -Z because sampled B is zero.
                EXPECT_NEAR(pixels[64 + i * 4], .5f, 1.0f / 255.0f);
                EXPECT_NEAR(pixels[64 + i * 4 + 1], .5f, 1.0f / 255.0f);
            }
        }
        EXPECT_EQ(coverage, scenario.coverage);
    }
}

TEST_F(SceneAssetLoaderTest, RendererTemporalResolveGTAOAndDenoiseRespectSceneDepth)
{
    const std::filesystem::path shaders =
        std::filesystem::absolute(std::filesystem::path(__FILE__).parent_path().parent_path().parent_path() / "Examples/Renderer/Shaders");
    SetResourceDirectoryPath(&fileSystem, MountProject, DirectoryOtherFiles, shaders.string().c_str());
    hz::RenderContext context({ .pAppName = "RendererTemporalAndGTAOTest", .enableGpuValidation = true });
    const auto pipeline = [&](const char* file, const char* entry)
    {
        return context.CreateComputePipeline({ .shaderDesc = { .stages = { { .stage = SHADER_STAGE_COMP, .pEntryPoint = entry } },
                                                                .sourceDirectory = DirectoryOtherFiles,
                                                                .pFileName = file } });
    };
    const hz::GPUPipeline temporal = pipeline("TemporalAA.slang", "CSMain");
    const hz::GPUPipeline horizon = pipeline("GTAO.slang", "CSMain");
    const hz::GPUPipeline upsample = pipeline("GTAO.slang", "BlurMain");
    const hz::GPUPipeline denoise = pipeline("ScreenSpaceDenoise/SSDReflectionSpatialAccumulation.slang", "main");
    ASSERT_TRUE(temporal.IsValid() && horizon.IsValid() && upsample.IsValid() && denoise.IsValid());
    // Resolve each nested shader through the same runtime source path as Renderer.
    const char* accumulationShaders[] = {
        "ScreenSpaceDenoise/SSDShadowAOTemporalAccumulation.slang",
        "ScreenSpaceDenoise/SSDShadowAOSpatialAccumulation.slang",
        "ScreenSpaceDenoise/SSDReflectionTemporalAccumulation.slang",
    };
    for (const char* file : accumulationShaders)
    {
        ASSERT_TRUE(pipeline(file, "main").IsValid()) << file;
    }
    ASSERT_TRUE(pipeline("ScreenSpaceDenoise/SSDDepthHistory.slang", "CSCopyDepth").IsValid());

    static constexpr char supportSource[] = R"(
        cbuffer RootConstant0 { uint A; uint B; uint C; uint D; uint E; };
        [numthreads(8,8,1)] void Seed(uint3 id : SV_DispatchThreadID) {
            RWTexture2D<float4> color = ResourceDescriptorHeap[A];
            RWTexture2D<float> depth = ResourceDescriptorHeap[B];
            RWTexture2D<float4> motion = ResourceDescriptorHeap[C];
            RWTexture2D<float4> history = ResourceDescriptorHeap[D];
            RWTexture2D<float4> normals = ResourceDescriptorHeap[E];
            color[id.xy] = float4((id.x / 8.0).xxx, 1);
            depth[id.xy] = id.y == 7 ? 1.0 : id.x >= 4 ? 0.35 : 0.65;
            motion[id.xy] = float4(0, 8.0 / 255.0, 128.0 / 255.0, 0);
            history[id.xy] = float4(0.5, 0.5, 0.5, id.x == 1 ? 5.0 : 1.0);
            normals[id.xy] = float4(1, 1, 51.0 / 1023.0, 0);
        }
        [numthreads(1,1,1)] void Read() {
            Texture2D<float4> temporal = ResourceDescriptorHeap[A];
            Texture2D<float4> ao = ResourceDescriptorHeap[B];
            RWStructuredBuffer<float4> output = ResourceDescriptorHeap[C];
            Texture2D<float4> filtered = ResourceDescriptorHeap[D];
            output[0] = float4(temporal.Load(int3(3, 3, 0)).r, temporal.Load(int3(1, 3, 0)).r,
                               temporal.Load(int3(3, 7, 0)).a, ao.Load(int3(3, 7, 0)).r);
            output[1] = float4(ao.Load(int3(3, 3, 0)).r, ao.Load(int3(7, 3, 0)).r,
                               filtered.Load(int3(3, 3, 0)).r, filtered.Load(int3(3, 7, 0)).a);
        }
    )";
    const auto supportPipeline = [&](const char* entry)
    {
        return context.CreateComputePipeline({ .shaderDesc = { .stages = { { .stage = SHADER_STAGE_COMP,
                                                                              .pSource = supportSource,
                                                                              .sourceSize = sizeof(supportSource) - 1,
                                                                              .pEntryPoint = entry } } } });
    };
    const hz::GPUPipeline seed = supportPipeline("Seed");
    const hz::GPUPipeline read = supportPipeline("Read");
    ASSERT_TRUE(seed.IsValid() && read.IsValid());

    const auto image = [&](uint32_t size, hz::Format format = hz::Format::R32G32B32A32_SFLOAT)
    {
        return context.CreateTexture({ .Width = size,
                                       .Height = size,
                                       .Format = format,
                                       .StartState = RESOURCE_STATE_UNORDERED_ACCESS,
                                       .Descriptors = (DescriptorType)(DESCRIPTOR_TYPE_TEXTURE | DESCRIPTOR_TYPE_RW_TEXTURE),
                                       .Flags = TEXTURE_CREATION_FLAG_FORCE_2D });
    };
    const hz::GPUTexture color = image(8), depth = image(8, hz::Format::R32_SFLOAT), motion = image(8), history = image(8),
                         normals = image(8), resolved = image(8), halfAO = image(4), ao = image(8);
    const hz::GPUSampler sampler =
        context.CreateSampler({ .AddressU = ADDRESS_MODE_CLAMP_TO_EDGE, .AddressV = ADDRESS_MODE_CLAMP_TO_EDGE });
    const FrameData frame = { .Eye = Vector4(0, 0, -2, 1) };
    const hz::GPUBuffer frameBuffer = context.CreateBuffer({ .Size = sizeof(frame),
                                                             .ElementCount = 1,
                                                             .StructStride = sizeof(frame),
                                                             .pInitialData = &frame,
                                                             .initialDataSize = sizeof(frame),
                                                             .usage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU,
                                                             .StartState = RESOURCE_STATE_SHADER_RESOURCE,
                                                             .Descriptors = DESCRIPTOR_TYPE_BUFFER });
    const hz::GPUBuffer output = context.CreateBuffer({ .Size = 32,
                                                         .ElementCount = 2,
                                                         .StructStride = 16,
                                                         .usage = RESOURCE_MEMORY_USAGE_GPU_ONLY,
                                                         .StartState = RESOURCE_STATE_UNORDERED_ACCESS,
                                                         .Descriptors = DESCRIPTOR_TYPE_RW_BUFFER });
    const hz::GPUBuffer readback = context.CreateBuffer({ .Size = 32,
                                                           .usage = RESOURCE_MEMORY_USAGE_GPU_TO_CPU,
                                                           .StartState = RESOURCE_STATE_COPY_DEST,
                                                           .Flags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT });
    hz::CommandList& commands = context.AcquireCommandList();
    commands.SetPipeline(seed);
    commands.SetRootConstant({ color.GetUavIndex(), depth.GetUavIndex(), motion.GetUavIndex(), history.GetUavIndex(), normals.GetUavIndex() });
    commands.Dispatch(1, 1, 1, { .storageTextures = { &color, &depth, &motion, &history, &normals } });
    commands.SetPipeline(temporal);
    commands.SetRootConstant({ color.GetSrvIndex(), UINT32_MAX, depth.GetSrvIndex(), motion.GetSrvIndex(), frameBuffer.GetSrvIndex(),
                               history.GetSrvIndex(), resolved.GetUavIndex(), sampler.GetIndex(), 0, 0, 0, 0 });
    commands.Dispatch(1, 1, 1, { .sampledTextures = { &color, &depth, &motion, &history },
                                 .storageTextures = { &resolved },
                                 .buffers = { &frameBuffer } });
    commands.SetPipeline(horizon);
    commands.SetRootConstant({ frameBuffer.GetSrvIndex(), depth.GetSrvIndex(), normals.GetSrvIndex(), UINT32_MAX, halfAO.GetUavIndex(),
                               std::bit_cast<uint32_t>(2.0f) });
    commands.Dispatch(1, 1, 1, { .sampledTextures = { &depth, &normals }, .storageTextures = { &halfAO }, .buffers = { &frameBuffer } });
    commands.SetPipeline(upsample);
    commands.SetRootConstant({ UINT32_MAX, depth.GetSrvIndex(), normals.GetSrvIndex(), halfAO.GetSrvIndex(), ao.GetUavIndex(), 0 });
    commands.Dispatch(1, 1, 1, { .sampledTextures = { &depth, &normals, &halfAO }, .storageTextures = { &ao } });
    commands.SetPipeline(denoise);
    commands.SetRootConstant({ frameBuffer.GetSrvIndex(), resolved.GetSrvIndex(), UINT32_MAX, depth.GetSrvIndex(), UINT32_MAX,
                               normals.GetSrvIndex(), history.GetUavIndex(), 0, 0, 0, std::bit_cast<uint32_t>(0.01f), 0, UINT32_MAX });
    commands.Dispatch(1, 1, 1, { .sampledTextures = { &resolved, &depth, &normals },
                                 .storageTextures = { &history },
                                 .buffers = { &frameBuffer } });
    commands.SetPipeline(read);
    commands.SetRootConstant({ resolved.GetSrvIndex(), ao.GetSrvIndex(), output.GetUavIndex(), history.GetSrvIndex() });
    commands.Dispatch(1, 1, 1, { .sampledTextures = { &resolved, &ao, &history }, .storageBuffers = { &output } });
    commands.CopyBuffer(readback, 0, output, 0, 32);
    context.Wait(context.Submit(commands));
    const float* values = (const float*)readback.Get()->pCpuMappedAddress;
    EXPECT_NEAR(values[0], 0.4875f, 1e-4f); // Accepted neighboring history.
    EXPECT_NEAR(values[1], 0.125f, 1e-4f);  // Depth mismatch.
    EXPECT_FLOAT_EQ(values[2], 0.0f);       // Sky has no history depth.
    EXPECT_FLOAT_EQ(values[3], 1.0f);       // Sky has no occlusion.
    EXPECT_GE(values[4], 0.0f);
    EXPECT_LT(values[4], values[5]);      // The nearer ledge occludes the back plane.
    EXPECT_NEAR(values[6], 0.445833f, 1e-4f); // Filters the neighbor on the same plane and rejects the brighter ledge.
    EXPECT_FLOAT_EQ(values[7], 0.0f);
}
