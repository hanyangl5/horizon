// GPU tests of the Examples/Renderer shaders and passes, built on the shared asset fixtures.
#include "AssetTestFixtures.h"
#include "../../Examples/Renderer/Source/DeferredLighting.h"
#include "../../Examples/Renderer/Source/GpuDrawList.h"
#include "../../Examples/Renderer/Source/FrustumCulling.h"
#include "../../Examples/Renderer/Source/RayTracingScene.h"
#include <cmath>
#include <d3d12sdklayers.h>

#define IMEMORY_FROM_HEADER
#include "Core/IMemory.h"

TEST_F(SceneAssetLoaderTest, RendererMotionUsesPerspectiveCorrectSubmittedTransforms)
{
    const std::filesystem::path shaders =
        std::filesystem::absolute(std::filesystem::path(__FILE__).parent_path().parent_path().parent_path() / "Examples/Renderer/Shaders");
    SetResourceDirectoryPath(&fileSystem, MountProject, DirectoryOtherFiles, shaders.string().c_str());
    hz::RenderContext context({ .pAppName = "RendererMotionTest", .enableGpuValidation = true });
    hz::GPUPipeline   pipeline = context.CreateGraphicsPipeline({
        .shaderDesc = { .stages = { { .stage = ShaderStageVert, .pEntryPoint = "VSMain" },
                                    { .stage = ShaderStageFrag, .pEntryPoint = "PSMain" } },
                        .sourceDirectory = DirectoryOtherFiles,
                        .pFileName = "Geometry.slang" },
        .colorTargets = { { .Format = hz::Format::R8G8B8A8_UNORM },
                          { .Format = hz::Format::R8G8B8A8_UNORM },
                          { .Format = hz::Format::R8G8B8A8_UNORM },
                          { .Format = hz::Format::R8G8B8A8_UNORM } },
    });
    SetResourceDirectoryPath(&fileSystem, MountProject, DirectoryOtherFiles, directory.string().c_str());
    ASSERT_TRUE(pipeline.IsValid());
    struct FrameData
    {
        Matrix4 viewProjection;
        Matrix4 previousViewProjection;
        Matrix4 inverseViewProjection;
        Vector4 eye;
    };
    // Project x/z, y/z at constant Depth, with vertex w = 1, 1, 2.
    const Matrix4      projection(Vector4(1, 0, 0, 0), Vector4(0, 1, 0, 0), Vector4(0, 0, 0.5f, 1), Vector4(0));
    const FrameData    frameData = { .viewProjection = projection,
                                     .previousViewProjection = projection,
                                     .inverseViewProjection = Matrix4::identity(),
                                     .eye = Vector4(0, 0, 0, 1) };
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
                                      .usage = MemoryUsageCpuToGpu,
                                      .StartState = ResourceStateShaderResource,
                                      .Descriptors = stride ? DescriptorTypeBuffer : DescriptorTypeBufferRaw });
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
                                         .StartState = ResourceStateRenderTarget,
                                         .Descriptors = DescriptorTypeTexture,
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
                  { .stage = ShaderStageComp, .pSource = readPixel, .sourceSize = sizeof(readPixel) - 1, .pEntryPoint = "CSMain" } } } });
    hz::GPUBuffer output = context.CreateBuffer({ .Size = 16,
                                                  .ElementCount = 1,
                                                  .StructStride = 16,
                                                  .usage = MemoryUsageGpuOnly,
                                                  .StartState = ResourceStateUnorderedAccess,
                                                  .Descriptors = DescriptorTypeRwBuffer });
    hz::GPUBuffer readback = context.CreateBuffer({ .Size = 16,
                                                    .usage = MemoryUsageGpuToCpu,
                                                    .StartState = ResourceStateCopyDest,
                                                    .Flags = BufferCreationFlagPersistentMapBit });
    ASSERT_TRUE(reader.IsValid() && sampler.IsValid());
    hz::CommandList& commands = context.AcquireCommandList();
    commands.BeginRendering({ .colorAttachments = {
        { .pTexture = &targets[0], .loadAction = LoadActionClear, .storeAction = StoreActionStore },
        { .pTexture = &targets[1], .loadAction = LoadActionClear, .storeAction = StoreActionStore },
        { .pTexture = &targets[2], .loadAction = LoadActionClear, .storeAction = StoreActionStore },
        { .pTexture = &targets[3], .loadAction = LoadActionClear, .storeAction = StoreActionStore },
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
        context.CreateComputePipeline({ .shaderDesc = { .stages = { { .stage = ShaderStageComp, .pEntryPoint = "CSMain" } },
                                                        .sourceDirectory = DirectoryOtherFiles,
                                                        .pFileName = "FrustumCulling.slang" } });
    hz::GPUPipeline Clear =
        context.CreateComputePipeline({ .shaderDesc = { .stages = { { .stage = ShaderStageComp, .pEntryPoint = "ClearStatistics" } },
                                                        .sourceDirectory = DirectoryOtherFiles,
                                                        .pFileName = "FrustumCulling.slang" } });
    SetResourceDirectoryPath(&fileSystem, MountProject, DirectoryOtherFiles, directory.string().c_str());
    ASSERT_TRUE(pipeline.IsValid() && Clear.IsValid());
    struct FrameData
    {
        Matrix4 current;
        Matrix4 previous;
        Matrix4 inverse;
        Vector4 eye;
    };
    FrameData          camera = { Matrix4::identity(), Matrix4::identity(), Matrix4::identity(), Vector4(0, 0, 0, 1) };
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
        materials[i].tags = i == 1 || i == 4 ? 4u : 0u;
    }
    const auto input = [&](const void* data, uint32_t Size, uint32_t stride = 0)
    {
        return context.CreateBuffer({ .Size = Size,
                                      .ElementCount = stride ? Size / stride : Size / 4,
                                      .StructStride = stride,
                                      .pInitialData = data,
                                      .initialDataSize = Size,
                                      .usage = MemoryUsageCpuToGpu,
                                      .StartState = ResourceStateShaderResource,
                                      .Descriptors = stride ? DescriptorTypeBuffer : DescriptorTypeBufferRaw,
                                      .Flags = BufferCreationFlagPersistentMapBit });
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
                                        .usage = MemoryUsageGpuOnly,
                                        .StartState = ResourceStateUnorderedAccess,
                                        .Descriptors = DescriptorTypeRwBufferRaw });
    }
    hz::GPUBuffer  statistics = context.CreateBuffer({ .Size = 16,
                                                       .ElementCount = 4,
                                                       .usage = MemoryUsageGpuOnly,
                                                       .StartState = ResourceStateUnorderedAccess,
                                                       .Descriptors = DescriptorTypeRwBufferRaw });
    // Opaque and masked, blended, uncullable casters, and opaque draws whose material has tag bit 2.
    const uint32_t opaqueModes = DrawListAlphaMode(hz::MaterialAlphaMode::Opaque) | DrawListAlphaMode(hz::MaterialAlphaMode::Mask);
    DrawListFilter filters[ListCount] = { { .alphaModes = opaqueModes },
                                          { .alphaModes = DrawListAlphaMode(hz::MaterialAlphaMode::Blend) },
                                          { .alphaModes = opaqueModes, .frustumCull = 0 },
                                          { .alphaModes = opaqueModes, .requiredTags = 4 } };
    for (uint32_t i = 0; i < ListCount; ++i)
        filters[i].commandsIndex = outputs[i].GetUavIndex();
    hz::GPUBuffer  filterBuffer = input(filters, sizeof(filters), sizeof(DrawListFilter));
    hz::GPUBuffer  readback = context.CreateBuffer({ .Size = ListCount * 100 + 16,
                                                     .usage = MemoryUsageGpuToCpu,
                                                     .StartState = ResourceStateCopyDest,
                                                     .Flags = BufferCreationFlagPersistentMapBit });
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
            camera.current = Matrix4::perspectiveRH(PI / 2.0f, 1.0f, 0.1f, 10.0f) * inverse(pose);
            camera.previous = camera.current;
            camera.inverse = inverse(camera.current);
            camera.eye = pose.getCol3();
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
    writeGeometry(IndexTypeUInt16, true);
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

    GpuDrawList     draws(context);
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
                  { .stage = ShaderStageComp, .pSource = source, .sourceSize = sizeof(source) - 1, .pEntryPoint = "CSMain" } } } });
    ASSERT_TRUE(trace.IsValid());

    struct FrameData
    {
        Matrix4 current = Matrix4::identity(), previous = Matrix4::identity(), inverse = Matrix4::identity();
        Vector4 eye = Vector4(0, 0, -10, 1);
    } camera;
    hz::GPUBuffer frame = context.CreateBuffer({ .Size = sizeof(camera),
                                                 .ElementCount = sizeof(camera) / 4,
                                                 .pInitialData = &camera,
                                                 .initialDataSize = sizeof(camera),
                                                 .usage = MemoryUsageCpuToGpu,
                                                 .StartState = ResourceStateShaderResource,
                                                 .Descriptors = DescriptorTypeBufferRaw });
    hz::GPUBuffer output = context.CreateBuffer({ .Size = 16,
                                                  .ElementCount = 4,
                                                  .StructStride = 4,
                                                  .usage = MemoryUsageGpuOnly,
                                                  .StartState = ResourceStateUnorderedAccess,
                                                  .Descriptors = DescriptorTypeRwBuffer });
    hz::GPUBuffer readback = context.CreateBuffer({ .Size = 96,
                                                    .usage = MemoryUsageGpuToCpu,
                                                    .StartState = ResourceStateCopyDest,
                                                    .Flags = BufferCreationFlagPersistentMapBit });

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
        const SceneView  view = { .Scene = scene, .GpuScene = gpu, .Assets = assetManager, .FrameIndex = pass % 3 };
        hz::CommandList& commands = context.AcquireCommandList();

        gpu.Upload(commands);
        draws.Prepare(view);
        culling.Execute(commands, frame, view, draws);
        rayScene.Update(commands, view, draws);
        EXPECT_EQ(rayScene.GetSceneTLAS().GetInstanceCount(), pass < 3 ? 3 - pass : 0);
        EXPECT_EQ(draws.GetGeometries().Count, pass < 2 ? 2u : (pass == 2 ? 1u : 0u));
        uint32_t expected[] = { UINT32_MAX, UINT32_MAX, UINT32_MAX, UINT32_MAX };
        if (pass < 3)
        {
            const hz::Span<GpuDrawData> prepared = draws.GetDraws();
            EXPECT_EQ(prepared.Count, 4 - pass);
            for (uint32_t i = 0; i < prepared.Count; ++i)
            {
                const GpuDrawData& Draw = prepared.Data[i];
                const float        x = (float)scene.GetInstances().Data[Draw.instance].current.getTranslation().getX();
                for (uint32_t j = 0; j < 4; ++j)
                {
                    if (x == offsets[j] && Draw.alphaMode != uint32_t(hz::MaterialAlphaMode::Blend))
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
    writeGeometry(IndexTypeUInt16);
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
    ASSERT_EQ(mesh->GetIndexType(), IndexTypeUInt16);
    // Three indices consume six bytes; RAW loads require a second complete DWORD.
    ASSERT_EQ(mesh->GetIndexBuffer().GetSize(), 8u);
    const std::filesystem::path shaders =
        std::filesystem::absolute(std::filesystem::path(__FILE__).parent_path().parent_path().parent_path() / "Examples/Renderer/Shaders");
    SetResourceDirectoryPath(&fileSystem, MountProject, DirectoryOtherFiles, shaders.string().c_str());
    hz::GPUPipeline pipeline = context.CreateGraphicsPipeline({
        .shaderDesc = { .stages = { { .stage = ShaderStageVert, .pEntryPoint = "VSIndirect" },
                                    { .stage = ShaderStageFrag, .pEntryPoint = "PSMain" } },
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
    struct FrameData
    {
        Matrix4 viewProjection = Matrix4::identity();
        Matrix4 previousViewProjection = Matrix4::identity();
        Matrix4 inverseViewProjection = Matrix4::identity();
        Vector4 eye = Vector4(0, 0, -2, 1);
    } frameData;
    hz::RenderInstance instances[2];
    instances[0].current = instances[0].previous = Matrix4::translation(Vector3(-0.5f, 0, 0));
    instances[1].current = instances[1].previous = Matrix4::translation(Vector3(0.5f, 0, 0));
    hz::GpuMaterialData materials[2];
    materials[0].baseColorFactor[1] = materials[0].baseColorFactor[2] = 0;
    materials[1].baseColorFactor[0] = materials[1].baseColorFactor[2] = 0;
    materials[0].metallicFactor = materials[1].metallicFactor = 0;
    const float    positions[] = { -0.3f, -0.5f, 0.5f, 0.3f, -0.5f, 0.5f, 0, 0.5f, 0.5f };
    const uint32_t normals[] = { 0x80008000, 0x80008000, 0x80008000 }, uvs[] = { 0, 0, 0 };
    const auto     input = [&](const void* data, uint32_t Size, uint32_t stride = 0)
    {
        return context.CreateBuffer({ .Size = Size,
                                      .ElementCount = stride ? Size / stride : Size / 4,
                                      .StructStride = stride,
                                      .pInitialData = data,
                                      .initialDataSize = Size,
                                      .usage = MemoryUsageCpuToGpu,
                                      .StartState = ResourceStateShaderResource,
                                      .Descriptors = stride ? DescriptorTypeBuffer : DescriptorTypeBufferRaw });
    };
    hz::GPUBuffer frame = input(&frameData, sizeof(frameData), sizeof(frameData));
    hz::GPUBuffer instanceBuffer = input(instances, sizeof(instances)), materialBuffer = input(materials, sizeof(materials));
    hz::GPUBuffer vertices = input(positions, sizeof(positions)), normal = input(normals, sizeof(normals)), uv = input(uvs, sizeof(uvs));
    struct DrawData
    {
        uint32_t instance, material, materialId, alphaMode, positions, normals, texcoords, indices;
        uint32_t indexStride, firstIndex;
        int32_t  vertexOffset;
        uint32_t indexCount;
    };
    DrawData drawData[2] = { { .instance = 0,
                               .material = 0,
                               .materialId = 17,
                               .alphaMode = 0,
                               .positions = vertices.GetSrvIndex(),
                               .normals = normal.GetSrvIndex(),
                               .texcoords = uv.GetSrvIndex(),
                               .indices = mesh->GetIndexBuffer().GetSrvIndex(),
                               .indexStride = 2,
                               .firstIndex = 0,
                               .vertexOffset = 0,
                               .indexCount = 3 } };
    drawData[1] = drawData[0];
    drawData[1].instance = drawData[1].material = 1;
    drawData[1].materialId = 39;
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
                                                        .usage = MemoryUsageCpuToGpu,
                                                        .StartState = ResourceStateIndirectArgument,
                                                        .Descriptors = DescriptorTypeIndirectBuffer });
    hz::GPUSampler    sampler = context.CreateSampler();
    hz::GPUTexture    targets[4];
    for (hz::GPUTexture& target : targets)
    {
        target = context.CreateTexture({ .Width = 64,
                                         .Height = 64,
                                         .Format = hz::Format::R8G8B8A8_UNORM,
                                         .StartState = ResourceStateRenderTarget,
                                         .Descriptors = DescriptorTypeTexture,
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
                  { .stage = ShaderStageComp, .pSource = readPixel, .sourceSize = sizeof(readPixel) - 1, .pEntryPoint = "CSMain" } } } });
    hz::GPUBuffer    output = context.CreateBuffer({ .Size = 32,
                                                     .ElementCount = 2,
                                                     .StructStride = 16,
                                                     .usage = MemoryUsageGpuOnly,
                                                     .StartState = ResourceStateUnorderedAccess,
                                                     .Descriptors = DescriptorTypeRwBuffer });
    hz::GPUBuffer    readback = context.CreateBuffer({ .Size = 32,
                                                       .usage = MemoryUsageGpuToCpu,
                                                       .StartState = ResourceStateCopyDest,
                                                       .Flags = BufferCreationFlagPersistentMapBit });
    hz::CommandList& commands = context.AcquireCommandList();
    commands.BeginRendering({ .colorAttachments = {
        { .pTexture = &targets[0], .loadAction = LoadActionClear, .storeAction = StoreActionStore },
        { .pTexture = &targets[1], .loadAction = LoadActionClear, .storeAction = StoreActionStore },
        { .pTexture = &targets[2], .loadAction = LoadActionClear, .storeAction = StoreActionStore },
        { .pTexture = &targets[3], .loadAction = LoadActionClear, .storeAction = StoreActionStore },
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
    hz::GPUPipeline increment = context.CreateComputePipeline({ .shaderDesc = { .stages = { { .stage = ShaderStageComp,
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
                                                                              .usage = MemoryUsageCpuToGpu,
                                                                              .StartState = ResourceStateShaderResource,
                                                                              .Descriptors = DescriptorTypeBufferRaw });
    hz::GPUBuffer                           indexBuffer = context.CreateBuffer({ .Size = sizeof(indices),
                                                                                 .ElementCount = 3,
                                                                                 .pInitialData = indices,
                                                                                 .initialDataSize = sizeof(indices),
                                                                                 .usage = MemoryUsageCpuToGpu,
                                                                                 .StartState = ResourceStateShaderResource,
                                                                                 .Descriptors = DescriptorTypeBufferRaw });
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
                                                  .usage = MemoryUsageGpuOnly,
                                                  .StartState = ResourceStateUnorderedAccess,
                                                  .Descriptors = DescriptorTypeRwBuffer });
    hz::GPUBuffer readback = context.CreateBuffer({ .Size = 40,
                                                    .usage = MemoryUsageGpuToCpu,
                                                    .StartState = ResourceStateCopyDest,
                                                    .Flags = BufferCreationFlagPersistentMapBit });
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
        context.CreateComputePipeline({ .shaderDesc = { .stages = { { .stage = ShaderStageComp,
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
                                      .usage = MemoryUsageCpuToGpu,
                                      .StartState = ResourceStateShaderResource,
                                      .Descriptors = stride ? DescriptorTypeBuffer : DescriptorTypeBufferRaw,
                                      .Flags = BufferCreationFlagPersistentMapBit });
    };
    const auto image = [&](hz::Format Format, bool target)
    {
        return context.CreateTexture({ .Width = 1,
                                       .Height = 1,
                                       .Format = Format,
                                       .StartState = target ? ResourceStateRenderTarget : ResourceStateUnorderedAccess,
                                       .Descriptors = DescriptorType(DescriptorTypeTexture | DescriptorTypeRwTexture),
                                       .Flags = TextureCreationFlagForce2D,
                                       .GetRenderTarget = target });
    };
    hz::GPUTexture normal = image(hz::Format::R32G32B32A32_SFLOAT, true), Depth = image(hz::Format::R32_SFLOAT, true);
    hz::GPUTexture alpha = image(hz::Format::R32G32B32A32_SFLOAT, true);
    hz::GPUTexture result = image(hz::Format::R32G32B32A32_SFLOAT, false);
    hz::GPUSampler sampler = context.CreateSampler();
    struct FrameData
    {
        Matrix4 current, previous, inverse;
        Vector4 eye;
    };
    const FrameData         camera = { Matrix4::identity(), Matrix4::identity(), Matrix4::identity(), Vector4(0, 0, 1, 1) };
    const LightingFrameData lighting = { .lightDirection = Vector4(0, 0, -1, 0) };
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
        materials[i].id = { i, 1 };
        materials[i].baseColorTexture = i < 2 ? alpha.GetSrvIndex() : UINT32_MAX;
        materials[i].alphaMode = i == 1 ? 1u : 0u;
        materials[i].emissiveFactor[i == 2 ? 1 : 0] = 1;
        materials[i].alphaCutoff = 0.5f;
    }
    hz::GpuTextureData textures[4];
    textures[3] = { .id = { 3, 9 }, .srv = alpha.GetSrvIndex() };
    GpuDrawData drawData[8] = {};
    drawData[5] = { .material = 0,
                    .alphaMode = 0,
                    .positions = vertices.GetSrvIndex(),
                    .normals = uv.GetSrvIndex(),
                    .texcoords = uv.GetSrvIndex(),
                    .indices = indexBuffer.GetSrvIndex(),
                    .indexStride = 2,
                    .firstIndex = 1,
                    .indexCount = 3 };
    drawData[6] = drawData[5];
    drawData[6].material = drawData[6].alphaMode = 1;
    drawData[7] = drawData[5];
    drawData[7].material = 2;
    hz::GPUBuffer materialBuffer = input(materials, sizeof(materials)), textureBuffer = input(textures, sizeof(textures));
    hz::GPUBuffer drawBuffer = input(drawData, sizeof(drawData));
    hz::AccelerationStructureGeometry geometry = { .pVertexBuffer = &vertices,
                                                   .pIndexBuffer = &indexBuffer,
                                                   .indexOffset = sizeof(uint16_t),
                                                   .vertexCount = 3,
                                                   .indexCount = 3,
                                                   .indexType = IndexTypeUInt16 };
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
                                                  .usage = MemoryUsageGpuOnly,
                                                  .StartState = ResourceStateUnorderedAccess,
                                                  .Descriptors = DescriptorTypeRwBuffer });
    hz::GPUBuffer readback = context.CreateBuffer({ .Size = 16,
                                                    .usage = MemoryUsageGpuToCpu,
                                                    .StartState = ResourceStateCopyDest,
                                                    .Flags = BufferCreationFlagPersistentMapBit });
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
        for (GpuDrawData& Draw : drawData)
        {
            Draw.indices = caseIndex < caseCount ? indexBuffer.GetSrvIndex() : indexBuffer32.GetSrvIndex();
            Draw.indexStride = caseIndex < caseCount ? 2 : 4;
        }
        ASSERT_TRUE(context.UpdateBuffer(drawBuffer, 0, drawData, sizeof(drawData)));
        materials[1].baseColorFactor[3] = test.factor;
        materials[1].baseColorTexture = test.hasTexture ? alpha.GetSrvIndex() : UINT32_MAX;
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
                                                              .loadAction = LoadActionClear,
                                                              .storeAction = StoreActionStore,
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
    LightingFrameData whiteLight = { .lightDirection = Vector4(0, 0, 1, 1), .lightColor = Vector4(1, 1, 1, 1) };
    ASSERT_TRUE(context.UpdateBuffer(light, 0, &whiteLight, sizeof(whiteLight)));
    materials[0].baseColorTexture = UINT32_MAX;
    materials[0].baseColorFactor[0] = materials[0].baseColorFactor[1] = materials[0].baseColorFactor[2] = 1;
    materials[0].emissiveFactor[0] = materials[0].emissiveFactor[1] = materials[0].emissiveFactor[2] = 0;
    materials[0].metallicFactor = 0;
    materials[0].roughnessFactor = 1;
    ASSERT_TRUE(context.UpdateBuffer(materialBuffer, 0, materials, sizeof(materials)));
    constants.tlas = opaqueTop.GetSrvIndex();
    hz::CommandList& lit = context.AcquireCommandList();
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
    const float expectedLighting = (0.96f + 0.04f / 4.0f) / 3.14159265359f;
    for (uint32_t channel = 0; channel < 3; ++channel)
    {
        EXPECT_NEAR(litColor[channel], expectedLighting, 1e-4f);
    }
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
        context.CreateComputePipeline({ .shaderDesc = { .stages = { { .stage = ShaderStageComp, .pEntryPoint = "main" } },
                                                        .sourceDirectory = DirectoryOtherFiles,
                                                        .pFileName = "RTShadowAO.slang" } });
    SetResourceDirectoryPath(&fileSystem, MountProject, DirectoryOtherFiles, directory.string().c_str());
    hz::GPUPipeline reader =
        context.CreateComputePipeline({ .shaderDesc = { .stages = { { .stage = ShaderStageComp,
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
                                      .usage = MemoryUsageCpuToGpu,
                                      .StartState = ResourceStateShaderResource,
                                      .Descriptors = stride ? DescriptorTypeBuffer : DescriptorTypeBufferRaw });
    };
    const auto image = [&](hz::Format Format, bool target)
    {
        return context.CreateTexture({ .Width = 1,
                                       .Height = 1,
                                       .Format = Format,
                                       .StartState = target ? ResourceStateRenderTarget : ResourceStateUnorderedAccess,
                                       .Descriptors = DescriptorType(DescriptorTypeTexture | DescriptorTypeRwTexture),
                                       .Flags = TextureCreationFlagForce2D,
                                       .GetRenderTarget = target });
    };
    hz::GPUTexture normal = image(hz::Format::R32G32B32A32_SFLOAT, true), Depth = image(hz::Format::R32_SFLOAT, true);
    hz::GPUTexture alpha = image(hz::Format::R32G32B32A32_SFLOAT, true), noise = image(hz::Format::R32G32B32A32_SFLOAT, true);
    hz::GPUTexture result = image(hz::Format::R8G8_UNORM, false);
    hz::GPUSampler sampler = context.CreateSampler();
    struct FrameData
    {
        Matrix4 current, previous, inverse;
        Vector4 eye;
    };
    const FrameData         camera = { Matrix4::identity(), Matrix4::identity(), Matrix4::identity(), Vector4(0, 0, -1, 1) };
    const LightingFrameData lighting = { .lightDirection = Vector4(0, 0, -1, 1) };
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
        materials[i].id = { i, 1 };
        materials[i].baseColorTexture = alpha.GetSrvIndex();
        materials[i].alphaMode = i;
        materials[i].alphaCutoff = 0.5f;
    }
    hz::GpuTextureData textures[4];
    textures[3] = { .id = { 3, 9 }, .srv = alpha.GetSrvIndex() };
    GpuDrawData drawData[7] = {};
    drawData[5] = { .material = 0,
                    .alphaMode = 0,
                    .positions = vertices.GetSrvIndex(),
                    .normals = uv.GetSrvIndex(),
                    .texcoords = uv.GetSrvIndex(),
                    .indices = indexBuffer.GetSrvIndex(),
                    .indexStride = 2,
                    .indexCount = 3 };
    drawData[6] = drawData[5];
    drawData[6].material = drawData[6].alphaMode = 1;
    hz::GPUBuffer materialBuffer = input(materials, sizeof(materials)), textureBuffer = input(textures, sizeof(textures));
    hz::GPUBuffer                     drawBuffer = input(drawData, sizeof(drawData));
    hz::AccelerationStructureGeometry geometry = { .pVertexBuffer = &vertices,
                                                   .pIndexBuffer = &indexBuffer,
                                                   .vertexCount = 3,
                                                   .indexCount = 3,
                                                   .indexType = IndexTypeUInt16 };
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
                                                  .usage = MemoryUsageGpuOnly,
                                                  .StartState = ResourceStateUnorderedAccess,
                                                  .Descriptors = DescriptorTypeRwBuffer });
    hz::GPUBuffer readback = context.CreateBuffer({ .Size = 16,
                                                    .usage = MemoryUsageGpuToCpu,
                                                    .StartState = ResourceStateCopyDest,
                                                    .Flags = BufferCreationFlagPersistentMapBit });
    struct ShadowConstants
    {
        uint32_t frame, lighting, draws, materials, textures, sampler, top, normal, Depth, output, noise;
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
                    noise.GetSrvIndex() };
    // Three alpha-test modes, then the reference AO path which deliberately forces opaque hits.
    for (uint32_t caseIndex = 0; caseIndex < 12; ++caseIndex)
    {
        SCOPED_TRACE(caseIndex);
        const uint32_t scenario = caseIndex % 3;
        constants.alphaMethod = (caseIndex / 3) % 3;
        constants.shadows = caseIndex < 9;
        constants.ao = caseIndex >= 9;
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
                                                              .loadAction = LoadActionClear,
                                                              .storeAction = StoreActionStore,
                                                              .ClearValue = value } } });
            commands.EndRendering();
        };
        Clear(normal, { .r = 0.5f, .g = 0.5f, .a = 1 }); // Octahedral +Z.
        Clear(Depth, { .r = 0.2f });
        Clear(noise, { .r = 0.5f, .g = 0.5f });
        Clear(alpha, { .a = scenario == 2 ? 0.75f : 0.25f });
        constants.top = scenario ? maskedTop.GetSrvIndex() : opaqueTop.GetSrvIndex();
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
        EXPECT_FLOAT_EQ(values[0], constants.ao || (constants.alphaMethod != 0 && scenario == 1) ? 1.0f : 0.0f);
        EXPECT_FLOAT_EQ(values[1], constants.ao ? 0.0f : 1.0f);
    }

    // Reuse the same scene to exercise the production raster shadow alpha test as well.
    SetResourceDirectoryPath(&fileSystem, MountProject, DirectoryOtherFiles, shaders.string().c_str());
    hz::GPUPipeline raster =
        context.CreateGraphicsPipeline({ .shaderDesc = { .stages = { { .stage = ShaderStageVert, .pEntryPoint = "VSIndirect" },
                                                                     { .stage = ShaderStageFrag, .pEntryPoint = "PSShadow" } },
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
                                                               .usage = MemoryUsageGpuOnly,
                                                               .StartState = ResourceStateIndirectArgument,
                                                               .Descriptors = DescriptorTypeIndirectBuffer });
    hz::GPUTexture           shadowDepth = context.CreateTexture({ .Width = 1,
                                                                   .Height = 1,
                                                                   .Format = hz::Format::D16_UNORM,
                                                                   .StartState = ResourceStateDepthWrite,
                                                                   .Descriptors = DescriptorTypeTexture,
                                                                   .Flags = TextureCreationFlagForce2D,
                                                                   .GetRenderTarget = true });
    ASSERT_TRUE(instanceBuffer.IsValid() && indirect.IsValid() && shadowDepth.IsValid());
    for (uint32_t scenario = 0; scenario < 3; ++scenario)
    {
        SCOPED_TRACE(scenario);
        hz::CommandList& commands = context.AcquireCommandList();
        commands.BeginRendering({ .colorAttachments = { { .pTexture = &alpha,
                                                          .loadAction = LoadActionClear,
                                                          .storeAction = StoreActionStore,
                                                          .ClearValue = { .a = scenario == 2 ? 0.75f : 0.25f } } } });
        commands.EndRendering();
        commands.SetPipeline(raster);
        commands.BeginRendering(
            { .depthAttachment = { .pTexture = &shadowDepth,
                                   .loadAction = LoadActionClear,
                                   .storeAction = StoreActionStore,
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
        context.CreateGraphicsPipeline({ .shaderDesc = { .stages = { { .stage = ShaderStageVert, .pEntryPoint = "VSMain" },
                                                                     { .stage = ShaderStageFrag, .pEntryPoint = "PSTransparent" } },
                                                         .sourceDirectory = DirectoryOtherFiles,
                                                         .pFileName = "Transparency.slang" },
                                         .Depth = { .depthTest = true, .depthWrite = false, .depthFunc = CMP_LEQUAL },
                                         .depthStencilFormat = hz::Format::D32_SFLOAT });
    const auto compute = [&](const char* entry)
    {
        return context.CreateComputePipeline({ .shaderDesc = { .stages = { { .stage = ShaderStageComp, .pEntryPoint = entry } },
                                                               .sourceDirectory = DirectoryOtherFiles,
                                                               .pFileName = "Transparency.slang" } });
    };
    hz::GPUPipeline Clear = compute("ClearMain"), resolve = compute("ResolveMain");
    SetResourceDirectoryPath(&fileSystem, MountProject, DirectoryOtherFiles, directory.string().c_str());
    hz::GPUPipeline reader =
        context.CreateComputePipeline({ .shaderDesc = { .stages = { { .stage = ShaderStageComp,
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
                                      .usage = MemoryUsageCpuToGpu,
                                      .StartState = ResourceStateShaderResource,
                                      .Descriptors = stride ? DescriptorTypeBuffer : DescriptorTypeBufferRaw });
    };
    struct FrameData
    {
        Matrix4 vp, previousVP, inverseVP;
        Vector4 eye;
    };
    const FrameData         camera = { Matrix4::identity(), Matrix4::identity(), Matrix4::identity(), Vector4(0, 0, -1, 1) };
    const LightingFrameData lighting;
    hz::RenderInstance      instances[2];
    instances[0].current = Matrix4::translation(Vector3(0, 0, 0.2f));
    instances[1].current = Matrix4::translation(Vector3(0, 0, 0.7f));
    hz::GpuMaterialData materials[2];
    for (hz::GpuMaterialData& material : materials)
    {
        material.alphaMode = 2;
        material.baseColorFactor[3] = 0.5f;
    }
    materials[0].emissiveFactor[0] = materials[1].emissiveFactor[1] = 1;
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
                                       .StartState = ResourceStateUnorderedAccess,
                                       .Descriptors = (DescriptorType)(DescriptorTypeTexture | DescriptorTypeRwTexture),
                                       .GetRenderTarget = target });
    };
    hz::GPUTexture heads = image(hz::Format::R32_UINT, false), result = image(hz::Format::R32G32B32A32_SFLOAT, false);
    hz::GPUTexture opaque = image(hz::Format::R32G32B32A32_SFLOAT, true);
    hz::GPUTexture Depth = context.CreateTexture(
        { .Width = 8, .Height = 8, .Format = hz::Format::D32_SFLOAT, .StartState = ResourceStateDepthWrite, .GetRenderTarget = true });
    hz::GPUBuffer counters =
        context.CreateBuffer({ .Size = 16,
                               .ElementCount = 4,
                               .usage = MemoryUsageGpuOnly,
                               .StartState = ResourceStateUnorderedAccess,
                               .Descriptors = (DescriptorType)(DescriptorTypeBufferRaw | DescriptorTypeRwBufferRaw) });
    hz::GPUBuffer pixels = context.CreateBuffer({ .Size = 64 * 16,
                                                  .ElementCount = 64,
                                                  .StructStride = 16,
                                                  .usage = MemoryUsageGpuOnly,
                                                  .StartState = ResourceStateUnorderedAccess,
                                                  .Descriptors = DescriptorTypeRwBuffer });
    hz::GPUBuffer readback = context.CreateBuffer({ .Size = 64 * 16 + 16,
                                                    .usage = MemoryUsageGpuToCpu,
                                                    .StartState = ResourceStateCopyDest,
                                                    .Flags = BufferCreationFlagPersistentMapBit });
    for (uint32_t scenario = 0; scenario < 4; ++scenario)
    {
        SCOPED_TRACE(scenario);
        const uint32_t Capacity = scenario == 3 ? 1 : 128;
        hz::GPUBuffer  nodes =
            context.CreateBuffer({ .Size = Capacity * 16,
                                   .ElementCount = Capacity * 4,
                                   .usage = MemoryUsageGpuOnly,
                                   .StartState = ResourceStateUnorderedAccess,
                                   .Descriptors = (DescriptorType)(DescriptorTypeBufferRaw | DescriptorTypeRwBufferRaw) });
        hz::CommandList& commands = context.AcquireCommandList();
        commands.BeginRendering({ .colorAttachments = { { .pTexture = &opaque,
                                                          .loadAction = LoadActionClear,
                                                          .storeAction = StoreActionStore,
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
        constants[16] = light.GetSrvIndex();
        commands.SetPipeline(Clear);
        commands.SetRootConstant(constants);
        commands.Dispatch(1, 1, 1, { .storageTextures = { &heads }, .storageBuffers = { &counters } });
        commands.BeginRendering({ .depthAttachment = { .pTexture = &Depth,
                                                       .loadAction = LoadActionClear,
                                                       .storeAction = StoreActionStore,
                                                       .ClearValue = { .Depth = scenario == 2 ? 0.5f : 1.0f } } },
                                { .storageTextures = { &heads },
                                  .buffers = { &frame, &light, &instanceBuffer, &materialBuffer, &vertices, &normal, &uv },
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
            if (scenario != 3)
            {
                EXPECT_NEAR(color[pixel * 4], 0.5, 1e-5);
                EXPECT_NEAR(color[pixel * 4 + 1], scenario == 2 ? 0.0 : 0.25, 1e-5);
                EXPECT_NEAR(color[pixel * 4 + 2], scenario == 2 ? 0.5 : 0.25, 1e-5);
            }
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
        return context.CreateComputePipeline({ .shaderDesc = { .stages = { { .stage = ShaderStageComp, .pEntryPoint = entry } },
                                                               .sourceDirectory = DirectoryOtherFiles,
                                                               .pFileName = file } });
    };
    hz::GPUPipeline initialize = compute("PostLuminance.slang", "CSInitialize");
    hz::GPUPipeline reduce = compute("PostLuminance.slang", "CSReduce"), adapt = compute("PostLuminance.slang", "CSAdapt");
    hz::GPUPipeline metering = compute("PostMetering.slang", "CSMain");
    hz::GPUPipeline downsample = compute("PostBloomDownsample.slang", "CSMain");
    hz::GPUPipeline upsample = compute("PostBloomUpsample.slang", "CSMain");
    hz::GPUPipeline tone =
        context.CreateGraphicsPipeline({ .shaderDesc = { .stages = { { .stage = ShaderStageVert, .pEntryPoint = "VSMain" },
                                                                     { .stage = ShaderStageFrag, .pEntryPoint = "PSMain" } },
                                                         .sourceDirectory = DirectoryOtherFiles,
                                                         .pFileName = "PostToneMap.slang" },
                                         .colorTargets = { { .Format = hz::Format::R32G32B32A32_SFLOAT } } });
    hz::GPUPipeline deferred =
        context.CreateGraphicsPipeline({ .shaderDesc = { .stages = { { .stage = ShaderStageVert, .pEntryPoint = "VSMain" },
                                                                     { .stage = ShaderStageFrag, .pEntryPoint = "PSMain" } },
                                                         .sourceDirectory = DirectoryOtherFiles,
                                                         .pFileName = "DeferredLighting.slang" },
                                         .colorTargets = { { .Format = hz::Format::R32G32B32A32_SFLOAT } } });
    SetResourceDirectoryPath(&fileSystem, MountProject, DirectoryOtherFiles, directory.string().c_str());

    const char seedSource[] = R"hlsl(
        cbuffer RootConstant0 { uint outputIndex; uint peakX; };
        [numthreads(8,8,1)] void CSLogGrid(uint3 id : SV_DispatchThreadID) {
            RWTexture2D<float> output = ResourceDescriptorHeap[outputIndex];
            uint w,h; output.GetDimensions(w,h);
            if(id.x < w && id.y < h) output[id.xy] = 2.0 * (id.x + 2 * id.y);
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
                  .stages = { { .stage = ShaderStageComp, .pSource = source, .sourceSize = Size, .pEntryPoint = entry } } } });
    };

    hz::GPUPipeline seedLogs = helper(seedSource, sizeof(seedSource) - 1, "CSLogGrid");
    hz::GPUPipeline movingPeak = helper(seedSource, sizeof(seedSource) - 1, "CSMovingPeak");
    hz::GPUPipeline reader = helper(kCookbookTextureReadback, sizeof(kCookbookTextureReadback) - 1, "CSMain");
    ASSERT_TRUE(initialize.IsValid() && reduce.IsValid() && adapt.IsValid() && metering.IsValid() && downsample.IsValid() &&
                upsample.IsValid() && tone.IsValid() && seedLogs.IsValid() && movingPeak.IsValid() && reader.IsValid() &&
                deferred.IsValid());
    const auto image = [&](uint32_t Width, uint32_t Height, hz::Format Format)
    {
        return context.CreateTexture({ .Width = Width,
                                       .Height = Height,
                                       .Format = Format,
                                       .StartState = ResourceStateRenderTarget,
                                       .Descriptors = (DescriptorType)(DescriptorTypeTexture | DescriptorTypeRwTexture),
                                       .Flags = TextureCreationFlagForce2D,
                                       .GetRenderTarget = true });
    };
    constexpr hz::Format rgba = hz::Format::R32G32B32A32_SFLOAT, scalar = hz::Format::R32_SFLOAT;
    hz::GPUTexture       source = image(1, 1, rgba), black = image(1, 1, rgba), average = image(1, 1, scalar), mapped = image(1, 1, rgba);
    hz::GPUTexture       logOutput = image(2, 2, scalar), logGrid = image(2, 2, scalar);
    hz::GPUTexture       reduced = image(1, 1, scalar), previous = image(1, 1, scalar), adapted = image(1, 1, scalar);
    hz::GPUTexture       impulse = image(9, 9, rgba), blurred = image(9, 9, rgba);
    hz::GPUSampler       sampler = context.CreateSampler(
        { .AddressU = AddressModeClampToEdge, .AddressV = AddressModeClampToEdge, .AddressW = AddressModeClampToEdge });
    hz::GPUBuffer  output = context.CreateBuffer({ .Size = 256 * 16,
                                                   .ElementCount = 256,
                                                   .StructStride = 16,
                                                   .usage = MemoryUsageGpuOnly,
                                                   .StartState = ResourceStateUnorderedAccess,
                                                   .Descriptors = DescriptorTypeRwBuffer });
    hz::GPUBuffer  readback = context.CreateBuffer({ .Size = 256 * 16,
                                                     .usage = MemoryUsageGpuToCpu,
                                                     .StartState = ResourceStateCopyDest,
                                                     .Flags = BufferCreationFlagPersistentMapBit });

    hz::GPUTexture peakSource = image(2, 2, rgba), peakBright = image(1, 1, rgba);
    hz::GPUTexture peakNormal = image(1, 1, rgba), peakBase = image(1, 1, rgba);

    struct PeakFrame
    {
        Matrix4 vp, previous, inverse;
        Vector4 eye;
    };
    const PeakFrame   peakCamera = { Matrix4::identity(), Matrix4::identity(), Matrix4::identity(), Vector4(0, 0, 2, 1) };
    LightingFrameData peakLight;
    peakLight.lightDirection = Vector4(0, 0, -1, 100000);
    const auto Upload = [&](const void* data, uint32_t Size)
    {
        return context.CreateBuffer({ .Size = Size,
                                      .ElementCount = 1,
                                      .StructStride = Size,
                                      .pInitialData = data,
                                      .initialDataSize = Size,
                                      .usage = MemoryUsageCpuToGpu,
                                      .StartState = ResourceStateShaderResource,
                                      .Descriptors = DescriptorTypeBuffer });
    };
    hz::GPUBuffer    peakFrame = Upload(&peakCamera, sizeof(peakCamera)), peakLighting = Upload(&peakLight, sizeof(peakLight));
    hz::CommandList& commands = context.AcquireCommandList();
    const auto       Clear = [&](const hz::GPUTexture& texture, float value)
    {
        commands.BeginRendering({ .colorAttachments = { { .pTexture = &texture,
                                                          .loadAction = LoadActionClear,
                                                          .storeAction = StoreActionStore,
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
            { .colorAttachments = { { .pTexture = &mapped, .loadAction = LoadActionDontCare, .storeAction = StoreActionStore } } },
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
    struct MeteringConstants
    {
        uint32_t source, log;
    };
    const MeteringConstants meteringConstants = { source.GetSrvIndex(), logOutput.GetUavIndex() };
    commands.SetPipeline(metering);
    commands.SetRootConstant({ (const uint32_t*)&meteringConstants, sizeof(meteringConstants) / 4 });
    commands.Dispatch(1, 1, 1, { .sampledTextures = { &source }, .storageTextures = { &logOutput } });
    read(logOutput, 8);
    // Real deferred shader: N=V=L, white dielectric, roughness=.2, quantized Specular=8/16. No environment, shadow or AO.
    commands.BeginRendering({ .colorAttachments = { { .pTexture = &peakNormal,
                                                      .loadAction = LoadActionClear,
                                                      .storeAction = StoreActionStore,
                                                      .ClearValue = { .r = .5f, .g = .5f, .b = 51.0f / 1023.0f, .a = 2.0f / 3.0f } },
                                                    { .pTexture = &peakBase,
                                                      .loadAction = LoadActionClear,
                                                      .storeAction = StoreActionStore,
                                                      .ClearValue = { .r = 1, .g = 1, .b = 1, .a = 0 } } } });
    commands.EndRendering();
    commands.SetPipeline(deferred);
    commands.BeginRendering(
        { .colorAttachments = { { .pTexture = &mapped, .loadAction = LoadActionDontCare, .storeAction = StoreActionStore } } },
        { .sampledTextures = { &black, &average, &peakNormal, &peakBase }, .buffers = { &peakFrame, &peakLighting } });
    commands.SetViewport(0, 0, 1, 1);
    commands.SetScissor(0, 0, 1, 1);
    uint32_t peakConstants[15];
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
    commands.SetRootConstant(peakConstants);
    commands.Draw(3);
    commands.EndRendering();
    read(mapped, 15);
    struct LuminanceConstants
    {
        uint32_t input, output, previous;
        float    time;
    };
    LuminanceConstants luminanceConstants = { previous.GetUavIndex(), adapted.GetUavIndex(), 0, 0 };
    commands.SetPipeline(initialize);
    commands.SetRootConstant({ (const uint32_t*)&luminanceConstants, sizeof(luminanceConstants) / 4 });
    commands.Dispatch(1, 1, 1, { .storageTextures = { &previous, &adapted } });
    read(previous, 12);
    commands.SetPipeline(seedLogs);
    commands.SetRootConstant({ logGrid.GetUavIndex() });
    commands.Dispatch(1, 1, 1, { .storageTextures = { &logGrid } });
    commands.SetPipeline(reduce);
    luminanceConstants = { logGrid.GetSrvIndex(), reduced.GetUavIndex(), 0, 0 };
    commands.SetRootConstant({ (const uint32_t*)&luminanceConstants, sizeof(luminanceConstants) / 4 });
    commands.Dispatch(1, 1, 1, { .sampledTextures = { &logGrid }, .storageTextures = { &reduced } });
    read(reduced, 13);
    commands.SetPipeline(adapt);
    luminanceConstants = { reduced.GetSrvIndex(), adapted.GetUavIndex(), previous.GetSrvIndex(), 0.3f };
    commands.SetRootConstant({ (const uint32_t*)&luminanceConstants, sizeof(luminanceConstants) / 4 });
    commands.Dispatch(1, 1, 1, { .sampledTextures = { &reduced, &previous }, .storageTextures = { &adapted } });
    read(adapted, 14);
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
            { .colorAttachments = { { .pTexture = &mapped, .loadAction = LoadActionDontCare, .storeAction = StoreActionStore } } },
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
    for (uint32_t pixel = 0; pixel < 4; ++pixel)
    {
        EXPECT_NEAR(values[(8 + pixel) * 4], std::log2(1.2), 2e-5);
    }
    // Offline double goldens: shoulder -> 242.58323517119788 nits; ST2084 PQ and nits/80 respectively.
    const double hdrGolden[] = { 0.5993841391362755, 3.0322904396399735 };
    for (uint32_t mode = 0; mode < 2; ++mode)
    {
        for (uint32_t channel = 0; channel < 3; ++channel)
        {
            EXPECT_NEAR(values[(178 + mode) * 4 + channel], hdrGolden[mode], 2e-5) << mode << ":" << channel;
        }
    }
    // At normal incidence the reference GGX reduces analytically to (1-F0)/pi + F0/(4*pi*roughness^4).
    const double f0 = 0.08 * 8.0 / 16.0, pi = 3.141592653589793;
    const double peakExpected = 100000.0 * ((1.0 - f0) / pi + f0 / (4.0 * pi * std::pow(0.2, 4)));
    for (uint32_t channel = 0; channel < 3; ++channel)
    {
        EXPECT_TRUE(std::isfinite(values[15 * 4 + channel]));
        EXPECT_GT(values[15 * 4 + channel], 65504.0f);
        EXPECT_NEAR(values[15 * 4 + channel], peakExpected, peakExpected * 2e-5);
    }
    EXPECT_FLOAT_EQ(values[12 * 4], 50.0f);
    EXPECT_FLOAT_EQ(values[13 * 4], 3.0f); // log2(1),log2(4),log2(16),log2(64): geometric mean is 8, not the arithmetic mean.
    EXPECT_NEAR(values[14 * 4], 50.0 + (8.0 - 50.0) * (1.0 - std::exp(-0.3)), 1e-4);
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
        context.CreateGraphicsPipeline({ .shaderDesc = { .stages = { { .stage = ShaderStageVert, .pEntryPoint = "VSMain" },
                                                                     { .stage = ShaderStageFrag, .pEntryPoint = "PSMain" } },
                                                         .sourceDirectory = DirectoryOtherFiles,
                                                         .pFileName = "Geometry.slang" },
                                         .colorTargets = { { .Format = hz::Format::R8G8B8A8_UNORM },
                                                           { .Format = hz::Format::R8G8B8A8_UNORM },
                                                           { .Format = hz::Format::R8G8B8A8_UNORM },
                                                           { .Format = hz::Format::R8G8B8A8_UNORM } } });
    SetResourceDirectoryPath(&fileSystem, MountProject, DirectoryOtherFiles, directory.string().c_str());
    hz::GPUPipeline reader =
        context.CreateComputePipeline({ .shaderDesc = { .stages = { { .stage = ShaderStageComp,
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
                                      .usage = MemoryUsageCpuToGpu,
                                      .StartState = ResourceStateShaderResource,
                                      .Descriptors = stride ? DescriptorTypeBuffer : DescriptorTypeBufferRaw,
                                      .Flags = BufferCreationFlagPersistentMapBit });
    };
    const auto image = [&](hz::Format Format)
    {
        return context.CreateTexture({ .Width = 4,
                                       .Height = 4,
                                       .Format = Format,
                                       .StartState = ResourceStateRenderTarget,
                                       .Descriptors = DescriptorTypeTexture,
                                       .Flags = TextureCreationFlagForce2D,
                                       .GetRenderTarget = true });
    };
    hz::GPUTexture alpha = image(hz::Format::R32G32B32A32_SFLOAT), targets[4];
    for (hz::GPUTexture& target : targets)
    {
        target = image(hz::Format::R8G8B8A8_UNORM);
    }
    struct FrameData
    {
        Matrix4 vp, previous, inverse;
        Vector4 eye;
    };
    const FrameData          camera = { Matrix4::identity(), Matrix4::identity(), Matrix4::identity(), Vector4(0, 0, 2, 1) };
    const hz::RenderInstance instance;
    hz::GpuMaterialData      material;
    material.baseColorTexture = alpha.GetSrvIndex();
    material.normalTexture = normalMap.GetSrvIndex();
    material.alphaCutoff = .5f;
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
                                                   .usage = MemoryUsageGpuOnly,
                                                   .StartState = ResourceStateUnorderedAccess,
                                                   .Descriptors = DescriptorTypeRwBuffer });
    hz::GPUBuffer  readback = context.CreateBuffer({ .Size = 2 * 16 * 16,
                                                     .usage = MemoryUsageGpuToCpu,
                                                     .StartState = ResourceStateCopyDest,
                                                     .Flags = BufferCreationFlagPersistentMapBit });
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
        material.alphaMode = scenario.mode;
        ASSERT_TRUE(context.UpdateBuffer(materials, 0, &material, sizeof(material)));
        ASSERT_TRUE(context.UpdateBuffer(uv, 0, scenario.distant ? farUV : nearUV, sizeof(nearUV)));
        hz::CommandList& commands = context.AcquireCommandList();
        commands.BeginRendering({ .colorAttachments = { { .pTexture = &alpha,
                                                          .loadAction = LoadActionClear,
                                                          .storeAction = StoreActionStore,
                                                          .ClearValue = { .r = 1, .g = 1, .b = 1, .a = scenario.alpha } } } });
        commands.EndRendering();
        commands.SetPipeline(geometry);
        commands.BeginRendering(
            { .colorAttachments = { { .pTexture = &targets[0], .loadAction = LoadActionClear, .storeAction = StoreActionStore },
                                    { .pTexture = &targets[1], .loadAction = LoadActionClear, .storeAction = StoreActionStore },
                                    { .pTexture = &targets[2], .loadAction = LoadActionClear, .storeAction = StoreActionStore },
                                    { .pTexture = &targets[3], .loadAction = LoadActionClear, .storeAction = StoreActionStore } } },
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

TEST_F(SceneAssetLoaderTest, RendererScreenSpaceAOShadersCompile)
{
    const std::filesystem::path shaders =
        std::filesystem::absolute(std::filesystem::path(__FILE__).parent_path().parent_path().parent_path() / "Examples/Renderer/Shaders");
    SetResourceDirectoryPath(&fileSystem, MountProject, DirectoryOtherFiles, shaders.string().c_str());
    hz::RenderContext context({ .pAppName = "RendererShaderCompileTest", .enableGpuValidation = true });
    // The AO pass produces the indirect-light occlusion the deferred shader consumes, so both entry points must link.
    for (const char* entry : { "CSMain", "BlurMain" })
    {
        const hz::GPUPipeline pipeline = context.CreateComputePipeline(
            { .shaderDesc = { .stages = { { .stage = ShaderStageComp, .pEntryPoint = entry } },
                              .sourceDirectory = DirectoryOtherFiles,
                              .pFileName = "ScreenSpaceAO.slang" } });
        EXPECT_TRUE(pipeline.IsValid()) << entry;
    }
}
