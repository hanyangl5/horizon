#include "AssetTestFixtures.h"
#include <fstream>

#define IMEMORY_FROM_HEADER
#include "Core/IMemory.h"

TEST_F(AssetManagerTest, DeduplicatesRequestsAndRejectsStaleHandlesAfterReuse)
{
    hz::AssetCatalog catalog;
    Add(catalog, 1);
    TestAssetLoader  loader;
    hz::AssetManager assetManager(catalog);
    assetManager.SetLoader(hz::AssetType::Scene, loader.GetLoader());
    const hz::AssetHandle first = assetManager.Request(assetID(1));
    ASSERT_TRUE(first.IsValid());
    EXPECT_EQ(assetManager.Request(assetID(1)), first);
    EXPECT_EQ(assetManager.GetState(first), hz::AssetState::Queued);
    assetManager.Update();
    EXPECT_EQ(assetManager.GetState(first), hz::AssetState::Ready);
    EXPECT_EQ(loader.begun, 1u);
    EXPECT_TRUE(assetManager.Release(first));
    assetManager.Update();
    EXPECT_EQ(assetManager.GetState(first), hz::AssetState::Ready);
    EXPECT_TRUE(assetManager.Release(first));
    assetManager.Update();
    EXPECT_EQ(assetManager.GetCount(), 0u);
    const hz::AssetHandle second = assetManager.Request(assetID(1));
    EXPECT_EQ(second.index, first.index);
    EXPECT_NE(second.generation, first.generation);
    EXPECT_FALSE(assetManager.Release(first));
    EXPECT_EQ(assetManager.GetState(first), hz::AssetState::Invalid);
    assetManager.Update();
    assetManager.Shutdown();
    EXPECT_EQ(loader.destroyed.size(), 2u);
}

TEST_F(AssetManagerTest, SharesDependenciesAndDestroysParentsBeforeChildren)
{
    hz::AssetCatalog catalog;
    Add(catalog, 1);
    Add(catalog, 2, { assetID(1), assetID(1) });
    Add(catalog, 3, { assetID(1) });
    EXPECT_EQ(catalog.Find(assetID(2))->dependencies.Count, 1u);
    TestAssetLoader  loader;
    hz::AssetManager assetManager(catalog);
    assetManager.SetLoader(hz::AssetType::Scene, loader.GetLoader());
    const hz::AssetHandle a = assetManager.Request(assetID(2));
    const hz::AssetHandle b = assetManager.Request(assetID(3));
    assetManager.Update();
    EXPECT_EQ(loader.begun, 3u);
    EXPECT_TRUE(assetManager.Release(a));
    assetManager.Update();
    EXPECT_EQ(assetManager.GetCount(), 2u);
    EXPECT_EQ(assetManager.GetState(b), hz::AssetState::Ready);
    EXPECT_EQ(loader.destroyed, (std::vector<uint32_t>{ 2 }));
    assetManager.Shutdown();
    EXPECT_EQ(loader.destroyed, (std::vector<uint32_t>{ 2, 3, 1 }));
}

TEST_F(AssetManagerTest, RejectsInvalidDependencyGraphsWithoutChangingExistingReferences)
{
    hz::AssetCatalog catalog;
    Add(catalog, 1);
    Add(catalog, 2, { assetID(1), assetID(99) });
    Add(catalog, 3, { assetID(4) });
    Add(catalog, 4, { assetID(3) });
    ASSERT_TRUE(
        catalog.Add({ .id = assetID(5), .Type = hz::AssetType::Texture, .pSourcePath = "test.png", .location = { .pPath = "test.dds" } }));
    TestAssetLoader  loader;
    hz::AssetManager assetManager(catalog);
    assetManager.SetLoader(hz::AssetType::Scene, loader.GetLoader());
    const hz::AssetHandle existing = assetManager.Request(assetID(1));
    hz::AssetError        error = hz::AssetError::None;
    EXPECT_FALSE(assetManager.Request(assetID(2), &error).IsValid());
    EXPECT_EQ(error, hz::AssetError::UnknownAsset);
    EXPECT_FALSE(assetManager.Request(assetID(3), &error).IsValid());
    EXPECT_EQ(error, hz::AssetError::DependencyCycle);
    EXPECT_FALSE(assetManager.Request(assetID(5), &error).IsValid());
    EXPECT_EQ(error, hz::AssetError::MissingLoader);
    EXPECT_EQ(assetManager.GetCount(), 1u);
    EXPECT_TRUE(assetManager.Release(existing));
    assetManager.Update();
    EXPECT_EQ(assetManager.GetCount(), 0u);
    EXPECT_EQ(loader.begun, 0u);
}

TEST_F(AssetManagerTest, CancellationDrainsLateResultsWithoutPublishingOrReusingTheIndex)
{
    hz::AssetCatalog catalog;
    Add(catalog, 1);
    TestAssetLoader loader;
    loader.ready = false;
    hz::AssetManager assetManager(catalog);
    assetManager.SetLoader(hz::AssetType::Scene, loader.GetLoader());
    const hz::AssetHandle Handle = assetManager.Request(assetID(1));
    assetManager.Update();
    ASSERT_EQ(assetManager.GetState(Handle), hz::AssetState::LoadingCPU);
    EXPECT_TRUE(assetManager.Release(Handle));
    assetManager.Update();
    EXPECT_EQ(assetManager.GetState(Handle), hz::AssetState::Retiring);
    EXPECT_EQ(assetManager.GetScene(Handle), nullptr);
    hz::AssetError error;
    EXPECT_FALSE(assetManager.Request(assetID(1), &error).IsValid());
    EXPECT_EQ(error, hz::AssetError::Retiring);
    EXPECT_EQ(loader.requestsDestroyed, 0u);
    loader.jobs[1]->ready = true;
    assetManager.Update();
    EXPECT_EQ(assetManager.GetState(Handle), hz::AssetState::Invalid);
    EXPECT_EQ(loader.requestsDestroyed, 1u);
    EXPECT_EQ(loader.destroyed, (std::vector<uint32_t>{ 1 }));
}

TEST_F(AssetManagerTest, WaitsForDependenciesBeforePublishingDecodedPayload)
{
    hz::AssetCatalog catalog;
    Add(catalog, 1);
    Add(catalog, 2, { assetID(1) });
    TestAssetLoader loader;
    loader.ready = false;
    hz::AssetManager assetManager(catalog);
    assetManager.SetLoader(hz::AssetType::Scene, loader.GetLoader());
    const hz::AssetHandle Handle = assetManager.Request(assetID(2));
    assetManager.Update();
    loader.jobs[2]->ready = true;
    assetManager.Update();
    EXPECT_EQ(assetManager.GetState(Handle), hz::AssetState::WaitingDependencies);
    EXPECT_EQ(assetManager.GetScene(Handle), nullptr);
    loader.jobs[1]->ready = true;
    assetManager.Update();
    EXPECT_EQ(assetManager.GetState(Handle), hz::AssetState::Ready);
    assetManager.Shutdown();
    EXPECT_EQ(loader.destroyed, (std::vector<uint32_t>{ 2, 1 }));
}

TEST_F(AssetManagerTest, DependencyFailureDrainsPendingParentAndReleasesPartialPayloads)
{
    hz::AssetCatalog catalog;
    Add(catalog, 1);
    Add(catalog, 2, { assetID(1) });
    TestAssetLoader loader;
    loader.ready = false;
    hz::AssetManager assetManager(catalog);
    assetManager.SetLoader(hz::AssetType::Scene, loader.GetLoader());
    const hz::AssetHandle parent = assetManager.Request(assetID(2));
    assetManager.Update();
    loader.jobs[1]->failed = true;
    loader.jobs[1]->ready = true;
    assetManager.Update();
    EXPECT_EQ(assetManager.GetState(parent), hz::AssetState::Failed);
    EXPECT_EQ(assetManager.GetError(parent), hz::AssetError::DependencyFailed);
    EXPECT_EQ(assetManager.GetScene(parent), nullptr);
    EXPECT_NE(std::string(assetManager.GetDiagnostic(parent)).find("dependency"), std::string::npos);
    EXPECT_EQ(assetManager.GetCount(), 2u);
    loader.jobs[2]->ready = true;
    assetManager.Update();
    EXPECT_EQ(assetManager.GetCount(), 1u);
    EXPECT_EQ(loader.destroyed, (std::vector<uint32_t>{ 1, 2 }));
    EXPECT_TRUE(assetManager.Release(parent));
    assetManager.Update();
    EXPECT_EQ(assetManager.GetCount(), 0u);
    loader.failBegin = 1;
    const hz::AssetHandle failed = assetManager.Request(assetID(1));
    assetManager.Update();
    EXPECT_EQ(assetManager.GetState(failed), hz::AssetState::Failed);
    EXPECT_EQ(assetManager.GetError(failed), hz::AssetError::LoadFailed);
}

TEST_F(AssetManagerTest, ShutdownDrainsPendingJobsAndIsIdempotent)
{
    hz::AssetCatalog catalog;
    Add(catalog, 1);
    Add(catalog, 2, { assetID(1) });
    TestAssetLoader loader;
    loader.ready = false;
    hz::AssetManager assetManager(catalog);
    assetManager.SetLoader(hz::AssetType::Scene, loader.GetLoader());
    assetManager.Request(assetID(2));
    assetManager.Update();
    assetManager.Shutdown();
    EXPECT_EQ(loader.requestsDestroyed, 2u);
    EXPECT_EQ(loader.destroyed.size(), 2u);
    EXPECT_EQ(assetManager.GetCount(), 0u);
    hz::AssetError error;
    EXPECT_FALSE(assetManager.Request(assetID(1), &error).IsValid());
    EXPECT_EQ(error, hz::AssetError::ShuttingDown);
    assetManager.Shutdown();
    EXPECT_EQ(loader.requestsDestroyed, 2u);
}

TEST_F(AssetManagerTest, TraversesAndRetiresDeepDependenciesIteratively)
{
    hz::AssetCatalog catalog;
    Add(catalog, 1);
    for (uint32_t i = 2; i <= 1024; ++i)
        Add(catalog, i, { assetID(i - 1) });
    TestAssetLoader  loader;
    hz::AssetManager assetManager(catalog);
    assetManager.SetLoader(hz::AssetType::Scene, loader.GetLoader());
    const hz::AssetHandle Handle = assetManager.Request(assetID(1024));
    assetManager.Update();
    EXPECT_EQ(assetManager.GetState(Handle), hz::AssetState::Ready);
    EXPECT_TRUE(assetManager.Release(Handle));
    assetManager.Update();
    EXPECT_EQ(assetManager.GetCount(), 0u);
    ASSERT_EQ(loader.destroyed.size(), 1024u);
    EXPECT_EQ(loader.destroyed.front(), 1024u);
    EXPECT_EQ(loader.destroyed.back(), 1u);
}

TEST_F(AssetManagerTest, UploadWaitsForDependenciesAndCancellationDrainsBeforeDestruction)
{
    hz::AssetCatalog catalog;
    Add(catalog, 1);
    Add(catalog, 2, { assetID(1) });
    TestAssetLoader  loader;
    hz::AssetManager assetManager(catalog);
    assetManager.SetLoader(hz::AssetType::Scene, loader.getGpuLoader());
    const hz::AssetHandle parent = assetManager.Request(assetID(2));
    assetManager.Update();
    EXPECT_EQ(loader.uploads, 1u);
    EXPECT_EQ(assetManager.GetState(parent), hz::AssetState::WaitingDependencies);
    const hz::AssetHandle child = assetManager.Find(assetID(1));
    EXPECT_EQ(assetManager.GetState(child), hz::AssetState::UploadingGPU);
    assetManager.Release(parent);
    assetManager.Update();
    EXPECT_EQ(assetManager.GetState(parent), hz::AssetState::Invalid);
    EXPECT_EQ(assetManager.GetState(child), hz::AssetState::Retiring);
    EXPECT_EQ(loader.destroyed, (std::vector<uint32_t>{ 2 }));
    loader.uploadReady = true;
    assetManager.Update();
    EXPECT_EQ(assetManager.GetCount(), 0u);
    EXPECT_EQ(loader.destroyed, (std::vector<uint32_t>{ 2, 1 }));
    EXPECT_EQ(loader.uploadWaits, 0u);
}

TEST_F(AssetManagerTest, UploadFailureAndShutdownUseTheSamePayloadCleanup)
{
    hz::AssetCatalog catalog;
    Add(catalog, 1);
    TestAssetLoader  loader;
    hz::AssetManager assetManager(catalog);
    assetManager.SetLoader(hz::AssetType::Scene, loader.getGpuLoader());
    const hz::AssetHandle Handle = assetManager.Request(assetID(1));
    assetManager.Update();
    loader.uploadReady = true;
    loader.uploadFailed = true;
    assetManager.Update();
    EXPECT_EQ(assetManager.GetState(Handle), hz::AssetState::Failed);
    EXPECT_EQ(assetManager.GetError(Handle), hz::AssetError::UploadFailed);
    EXPECT_EQ(loader.destroyed, (std::vector<uint32_t>{ 1 }));
    assetManager.Release(Handle);
    assetManager.Update();
    loader.uploadReady = false;
    assetManager.Request(assetID(1));
    assetManager.Update();
    assetManager.Shutdown();
    EXPECT_EQ(loader.uploadWaits, 1u);
    EXPECT_EQ(loader.destroyed, (std::vector<uint32_t>{ 1, 1 }));
}

TEST_F(AssetManagerTest, GpuTablesRebuildOnlyWhenTheReadySetChanges)
{
    hz::AssetCatalog catalog;
    Add(catalog, 1);
    Add(catalog, 2);
    TestAssetLoader  loader;
    hz::AssetManager assetManager(catalog);
    assetManager.SetLoader(hz::AssetType::Scene, loader.GetLoader());
    const hz::AssetHandle first = assetManager.Request(assetID(1));
    assetManager.Update();
    const uint64_t            revision = assetManager.GetGpuTables().revision;
    const MemoryTrackingStats before = GetMemoryTrackingStats();
    for (uint32_t i = 0; i < 100; ++i)
    {
        EXPECT_EQ(assetManager.GetGpuTables().revision, revision);
        assetManager.Update();
    }
    const MemoryTrackingStats after = GetMemoryTrackingStats();
    if (after.TrackingEnabled)
    {
        EXPECT_EQ(after.TotalAllocationCount, before.TotalAllocationCount);
        EXPECT_EQ(after.ReallocationCount, before.ReallocationCount);
    }
    assetManager.Release(first);
    const uint64_t released = assetManager.GetGpuTables().revision;
    EXPECT_NE(released, revision);
    // Without a render context nothing can still be using the payload.
    assetManager.Update();
    EXPECT_EQ(assetManager.GetState(first), hz::AssetState::Invalid);
    EXPECT_EQ(loader.destroyed, (std::vector<uint32_t>{ 1 }));
    const hz::AssetHandle second = assetManager.Request(assetID(2));
    EXPECT_EQ(assetManager.GetGpuTables().revision, released);
    assetManager.Update();
    EXPECT_EQ(assetManager.GetState(second), hz::AssetState::Ready);
    EXPECT_NE(assetManager.GetGpuTables().revision, released);
    assetManager.Shutdown();
    EXPECT_EQ(loader.destroyed, (std::vector<uint32_t>{ 1, 2 }));
}

TEST_F(SceneAssetLoaderTest, DecodesSceneAsynchronouslyAndPublishesTypedPayload)
{
    hz::SceneAsset scene;
    ASSERT_TRUE(scene.Parse(kEmptyScene));
    hz::AssetCatalog catalog;
    ASSERT_TRUE(catalog.AddScene(scene, "scene.sceneasset.json"));
    hz::Array<char> index;
    ASSERT_TRUE(catalog.Serialize(index));
    FileStream stream = {};
    ASSERT_TRUE(OpenStreamFromPath(DirectoryOtherFiles, "scene.assetcatalog.json", FileModeWrite, &stream));
    EXPECT_EQ(WriteToStream(&stream, index.GetData(), index.Size() - 1), index.Size() - 1);
    CloseStream(&stream);
    catalog.Clear();
    ASSERT_TRUE(catalog.Load(DirectoryOtherFiles, "scene.assetcatalog.json"));
    hz::SceneAssetLoader loader(DirectoryOtherFiles);
    hz::AssetManager     assetManager(catalog);
    assetManager.SetLoader(hz::AssetType::Scene, loader.GetLoader());
    const hz::AssetHandle Handle = assetManager.Request(scene.GetID());
    EXPECT_EQ(assetManager.GetScene(Handle), nullptr);
    Wait(assetManager, Handle);
    ASSERT_EQ(assetManager.GetState(Handle), hz::AssetState::Ready);
    ASSERT_NE(assetManager.GetScene(Handle), nullptr);
    EXPECT_EQ(assetManager.GetScene(Handle)->GetID(), scene.GetID());
    assetManager.Release(Handle);
    assetManager.Update();
    EXPECT_EQ(assetManager.GetScene(Handle), nullptr);
}

TEST_F(SceneAssetLoaderTest, RejectsMissingFileAndMismatchedIdentityAndDrainsOnShutdown)
{
    hz::AssetCatalog catalog;
    ASSERT_TRUE(catalog.Add({ .id = assetID(1), .pSourcePath = "scene.gltf", .location = { .pPath = "missing.sceneasset.json" } }));
    ASSERT_TRUE(catalog.Add({ .id = assetID(2), .pSourcePath = "scene.gltf", .location = { .pPath = "scene.sceneasset.json" } }));
    hz::SceneAssetLoader loader(DirectoryOtherFiles);
    hz::AssetManager     assetManager(catalog);
    assetManager.SetLoader(hz::AssetType::Scene, loader.GetLoader());
    const hz::AssetHandle missing = assetManager.Request(assetID(1));
    const hz::AssetHandle mismatched = assetManager.Request(assetID(2));
    Wait(assetManager, missing);
    Wait(assetManager, mismatched);
    EXPECT_EQ(assetManager.GetError(missing), hz::AssetError::LoadFailed);
    EXPECT_EQ(assetManager.GetError(mismatched), hz::AssetError::LoadFailed);
    assetManager.Release(mismatched);
    assetManager.Update();
    assetManager.Request(assetID(2));
    assetManager.Update();
    assetManager.Shutdown();
    EXPECT_EQ(assetManager.GetCount(), 0u);
}

TEST_F(SceneAssetLoaderTest, RejectsPayloadWithDifferentDependenciesFromCatalog)
{
    hz::SceneAsset scene;
    ASSERT_TRUE(scene.Parse(kEmptyScene));
    hz::AssetCatalog catalog;
    ASSERT_TRUE(catalog.Add({ .id = scene.GetID(),
                              .pSourcePath = "scene.gltf",
                              .location = { .pPath = "scene.sceneasset.json" },
                              .dependencies = { assetID(1) } }));
    ASSERT_TRUE(catalog.Add({ .id = assetID(1),
                              .Type = hz::AssetType::Mesh,
                              .pSourcePath = "scene.gltf",
                              .location = { .pPath = "scene.sceneasset.json", .elementIndex = 0 } }));
    hz::SceneAssetLoader loader(DirectoryOtherFiles);
    TestAssetLoader      meshLoader;
    hz::AssetManager     assetManager(catalog);
    assetManager.SetLoader(hz::AssetType::Scene, loader.GetLoader());
    assetManager.SetLoader(hz::AssetType::Mesh, meshLoader.GetLoader());
    const hz::AssetHandle Handle = assetManager.Request(scene.GetID());
    Wait(assetManager, Handle);
    EXPECT_EQ(assetManager.GetState(Handle), hz::AssetState::Failed);
    EXPECT_EQ(assetManager.GetScene(Handle), nullptr);
    EXPECT_EQ(meshLoader.destroyed, (std::vector<uint32_t>{ 1 }));
}

TEST_F(TextureAssetLiveTest, UploadsSamplesAndRetiresTextureAfterSubmission)
{
    writeTexture();
    hz::RenderContext context({ .pAppName = "TextureAssetTest", .enableGpuValidation = true });
    hz::AssetCatalog  catalog;
    ASSERT_TRUE(catalog.Add(
        { .id = assetID(1), .Type = hz::AssetType::Texture, .pSourcePath = "pixel.png", .location = { .pPath = "pixel.dds" } }));
    hz::TextureAssetLoader loader(context, DirectoryOtherFiles);
    hz::AssetManager       assetManager(catalog, &context);
    assetManager.SetLoader(hz::AssetType::Texture, loader.GetLoader());
    const hz::AssetHandle Handle = assetManager.Request(assetID(1));
    EXPECT_EQ(assetManager.Request(assetID(1)), Handle);
    assetManager.Release(Handle);
    Wait(assetManager, Handle);
    ASSERT_EQ(assetManager.GetState(Handle), hz::AssetState::Ready);
    const hz::GPUTexture& texture = assetManager.GetTexture(Handle)->GetTexture();
    ASSERT_TRUE(texture.IsValid());
    const char      shader[] = R"hlsl(
        cbuffer RootConstant0 { uint textureIndex; uint outputIndex; };
        [numthreads(1,1,1)] void CSMain() {
            Texture2D<float4> source = ResourceDescriptorHeap[textureIndex];
            RWStructuredBuffer<uint4> output = ResourceDescriptorHeap[outputIndex];
            output[0] = uint4(round(source.Load(int3(0,0,0)) * 255.0));
        }
    )hlsl";
    hz::GPUPipeline pipeline = context.CreateComputePipeline({ .shaderDesc = { .stages = { { .stage = SHADER_STAGE_COMP,
                                                                                             .pSource = shader,
                                                                                             .sourceSize = sizeof(shader) - 1,
                                                                                             .pEntryPoint = "CSMain" } } },
                                                               .Name = "TextureAsset.Readback" });
    ASSERT_TRUE(pipeline.IsValid());
    hz::GPUBuffer    output = context.CreateBuffer({ .Size = 16,
                                                     .ElementCount = 1,
                                                     .StructStride = 16,
                                                     .usage = RESOURCE_MEMORY_USAGE_GPU_ONLY,
                                                     .StartState = RESOURCE_STATE_UNORDERED_ACCESS,
                                                     .Descriptors = DESCRIPTOR_TYPE_RW_BUFFER });
    hz::GPUBuffer    readback = context.CreateBuffer({ .Size = 16,
                                                       .usage = RESOURCE_MEMORY_USAGE_GPU_TO_CPU,
                                                       .StartState = RESOURCE_STATE_COPY_DEST,
                                                       .Flags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT });
    hz::CommandList& commands = context.AcquireCommandList();
    commands.SetPipeline(pipeline);
    commands.SetRootConstant({ texture.GetSrvIndex(), output.GetUavIndex() });
    commands.Dispatch(1, 1, 1, { .sampledTextures = { &texture }, .storageBuffers = { &output } });
    commands.CopyBuffer(readback, 0, output, 0, 16);
    // The open command list references the texture, so its Release waits for that work.
    assetManager.Release(Handle);
    assetManager.Update();
    EXPECT_EQ(assetManager.GetState(Handle), hz::AssetState::Retiring);
    EXPECT_EQ(assetManager.GetCount(), 1u);
    const hz::SubmitHandle submission = context.Submit(commands);
    assetManager.Shutdown();
    EXPECT_TRUE(context.IsComplete(submission));
    EXPECT_EQ(assetManager.GetCount(), 0u);
    const uint32_t* pPixel = (const uint32_t*)readback.Get()->pCpuMappedAddress;
    ASSERT_NE(pPixel, nullptr);
    EXPECT_EQ(pPixel[0], 255u);
    EXPECT_EQ(pPixel[1], 128u);
    EXPECT_EQ(pPixel[2], 64u);
    EXPECT_EQ(pPixel[3], 255u);
}

TEST_F(TextureAssetLiveTest, CancelsUploadsAndReportsInvalidFiles)
{
    writeTexture();
    std::filesystem::copy_file(directory / "pixel.dds", directory / "truncated.dds");
    std::filesystem::resize_file(directory / "truncated.dds", 128);
    FileStream broken = {};
    ASSERT_TRUE(OpenStreamFromPath(DirectoryOtherFiles, "broken.dds", FileModeWrite, &broken));
    const uint32_t invalidHeader = 0;
    WriteToStream(&broken, &invalidHeader, sizeof(invalidHeader));
    CloseStream(&broken);
    hz::RenderContext context({ .pAppName = "TextureAssetCancelTest" });
    hz::AssetCatalog  catalog;
    ASSERT_TRUE(catalog.Add(
        { .id = assetID(1), .Type = hz::AssetType::Texture, .pSourcePath = "pixel.png", .location = { .pPath = "pixel.dds" } }));
    ASSERT_TRUE(catalog.Add(
        { .id = assetID(2), .Type = hz::AssetType::Texture, .pSourcePath = "pixel.png", .location = { .pPath = "pixel.png" } }));
    ASSERT_TRUE(catalog.Add(
        { .id = assetID(3), .Type = hz::AssetType::Texture, .pSourcePath = "pixel.png", .location = { .pPath = "missing.dds" } }));
    ASSERT_TRUE(catalog.Add(
        { .id = assetID(4), .Type = hz::AssetType::Texture, .pSourcePath = "pixel.png", .location = { .pPath = "truncated.dds" } }));
    ASSERT_TRUE(catalog.Add(
        { .id = assetID(5), .Type = hz::AssetType::Texture, .pSourcePath = "pixel.png", .location = { .pPath = "broken.dds" } }));
    hz::TextureAssetLoader loader(context, DirectoryOtherFiles);
    hz::AssetManager       assetManager(catalog);
    assetManager.SetLoader(hz::AssetType::Texture, loader.GetLoader());
    const hz::AssetHandle canceled = assetManager.Request(assetID(1));
    assetManager.Update();
    assetManager.Release(canceled);
    for (uint32_t i = 2; i <= 5; ++i)
    {
        const hz::AssetHandle invalid = assetManager.Request(assetID(i));
        Wait(assetManager, invalid);
        EXPECT_EQ(assetManager.GetError(invalid), hz::AssetError::UploadFailed);
        EXPECT_EQ(assetManager.GetTexture(invalid), nullptr);
    }
    assetManager.Shutdown();
    EXPECT_EQ(assetManager.GetCount(), 0u);
}

TEST_F(TypedAssetLiveTest, LoadsSharedDependencyGraphAndRetiresGeometryAfterGpuUse)
{
    writeTexture();
    writeScene();
    writeGeometry();
    hz::SceneAsset scene;
    ASSERT_TRUE(scene.Parse(kTypedScene));
    hz::AssetCatalog catalog;
    ASSERT_TRUE(catalog.AddScene(scene, "scene.sceneasset.json"));
    hz::RenderContext context({ .pAppName = "TypedAssetTest", .enableGpuValidation = true });
    hz::AssetLoaders  loaders(context, { .vertexLayout = kAssetTestLayout, .cookedDirectory = DirectoryOtherFiles });
    hz::AssetManager  assetManager(catalog, &context);
    loaders.RegisterLoaders(assetManager);
    const hz::AssetHandle root = assetManager.Request(scene.GetID());
    const hz::AssetHandle second = assetManager.Request(scene.GetMeshes().Data[1].id);
    Wait(assetManager, root);
    ASSERT_EQ(assetManager.GetState(root), hz::AssetState::Ready);
    EXPECT_EQ(assetManager.GetCount(), 5u);
    const hz::AssetHandle first = assetManager.Find(scene.GetMeshes().Data[0].id);
    const hz::MeshAsset*  pFirst = assetManager.GetMesh(first);
    const hz::MeshAsset*  pSecond = assetManager.GetMesh(second);
    ASSERT_NE(pFirst, nullptr);
    ASSERT_NE(pSecond, nullptr);
    EXPECT_EQ(&pFirst->GetIndexBuffer(), &pSecond->GetIndexBuffer());
    // The test layout binds positions to binding 2, so bindings 0 and 1 have no stream.
    EXPECT_EQ(&pFirst->GetVertexBuffer(2), &pSecond->GetVertexBuffer(2));
    EXPECT_EQ(pFirst->GetVertexBufferCount(), 3u);
    EXPECT_FALSE(pFirst->GetVertexBuffer(0).IsValid());
    EXPECT_EQ(pFirst->GetVertexStride(0), 0u);
    EXPECT_EQ(pFirst->GetVertexStride(2), 12u);
    EXPECT_EQ(pFirst->GetIndexType(), INDEX_TYPE_UINT16);
    EXPECT_EQ(pFirst->GetDraw(0).indexCount, 3u);
    const hz::MeshBounds& bounds = pFirst->GetBounds(0);
    EXPECT_FLOAT_EQ(bounds.minimum[0], -1);
    EXPECT_FLOAT_EQ(bounds.minimum[1], -5);
    EXPECT_FLOAT_EQ(bounds.minimum[2], -2);
    EXPECT_FLOAT_EQ(bounds.maximum[0], 4);
    EXPECT_FLOAT_EQ(bounds.maximum[1], 2);
    EXPECT_FLOAT_EQ(bounds.maximum[2], 6);
    const hz::MaterialAsset* pMaterial = assetManager.GetMaterial(assetManager.Find(scene.GetMaterials().Data[0].id));
    ASSERT_NE(pMaterial, nullptr);
    EXPECT_FLOAT_EQ(pMaterial->GetDescription().roughnessFactor, 0.8f);
    ASSERT_NE(assetManager.GetTexture(assetManager.Find(pMaterial->GetDescription().baseColorTexture)), nullptr);
    hz::GPUBuffer    readback = context.CreateBuffer({ .Size = 36,
                                                       .usage = RESOURCE_MEMORY_USAGE_GPU_TO_CPU,
                                                       .StartState = RESOURCE_STATE_COPY_DEST,
                                                       .Flags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT });
    hz::CommandList& commands = context.AcquireCommandList();
    commands.CopyBuffer(readback, 0, pFirst->GetVertexBuffer(2), 0, 36);
    assetManager.Release(root);
    assetManager.Release(second);
    assetManager.Update();
    EXPECT_EQ(assetManager.GetCount(), 5u);
    const hz::SubmitHandle submission = context.Submit(commands);
    assetManager.Shutdown();
    EXPECT_EQ(assetManager.GetCount(), 0u);
    EXPECT_TRUE(context.IsComplete(submission));
    const float* pVertices = (const float*)readback.Get()->pCpuMappedAddress;
    EXPECT_FLOAT_EQ(pVertices[0], -1);
    EXPECT_FLOAT_EQ(pVertices[4], -5);
    EXPECT_FLOAT_EQ(pVertices[8], -2);
}

TEST_F(TypedAssetLiveTest, MissingTextureLeavesMaterialSlotUnboundInsteadOfFailingScene)
{
    // No writeTexture(): pixel.dds is absent, so only the texture asset fails.
    writeScene();
    writeGeometry();
    hz::SceneAsset scene;
    ASSERT_TRUE(scene.Parse(kTypedScene));
    hz::AssetCatalog catalog;
    ASSERT_TRUE(catalog.AddScene(scene, "scene.sceneasset.json"));
    hz::RenderContext context({ .pAppName = "MissingTextureTest" });
    hz::AssetLoaders  loaders(context, { .vertexLayout = kAssetTestLayout, .cookedDirectory = DirectoryOtherFiles });
    hz::AssetManager  assetManager(catalog, &context);
    loaders.RegisterLoaders(assetManager);
    const hz::AssetHandle root = assetManager.Request(scene.GetID());
    Wait(assetManager, root);
    EXPECT_EQ(assetManager.GetState(root), hz::AssetState::Ready);
    const hz::SceneAssetMaterial& description = scene.GetMaterials().Data[0];
    EXPECT_EQ(assetManager.GetState(assetManager.Find(description.baseColorTexture)), hz::AssetState::Failed);
    const hz::AssetHandle material = assetManager.Find(description.id);
    ASSERT_EQ(assetManager.GetState(material), hz::AssetState::Ready);
    const hz::GpuAssetTables tables = assetManager.GetGpuTables();
    const hz::GpuMaterialData* pMapping = tables.GetMaterial({ .index = material.index, .generation = material.generation });
    ASSERT_NE(pMapping, nullptr);
    EXPECT_EQ(pMapping->BaseColorTexture, UINT32_MAX);
    EXPECT_FLOAT_EQ(pMapping->RoughnessFactor, 0.8f);
    assetManager.Release(root);
    assetManager.Shutdown();
    EXPECT_EQ(assetManager.GetCount(), 0u);
}

TEST_F(TypedAssetLiveTest, Loads32BitIndicesWithSmallVertexCounts)
{
    writeTexture();
    writeScene();
    writeGeometry(INDEX_TYPE_UINT32);
    hz::SceneAsset asset;
    ASSERT_TRUE(asset.Parse(kTypedScene));
    hz::AssetCatalog catalog;
    ASSERT_TRUE(catalog.AddScene(asset, "scene.sceneasset.json"));
    hz::RenderContext context({ .pAppName = "TypedAsset32BitIndexTest", .enableGpuValidation = true });
    hz::AssetLoaders  loaders(context, { .vertexLayout = kAssetTestLayout, .cookedDirectory = DirectoryOtherFiles });
    hz::AssetManager  assetManager(catalog, &context);
    loaders.RegisterLoaders(assetManager);
    const hz::AssetHandle Handle = assetManager.Request(asset.GetMeshes().Data[0].id);
    Wait(assetManager, Handle);
    ASSERT_EQ(assetManager.GetState(Handle), hz::AssetState::Ready);
    const hz::MeshAsset* mesh = assetManager.GetMesh(Handle);
    ASSERT_NE(mesh, nullptr);
    EXPECT_EQ(mesh->GetIndexType(), INDEX_TYPE_UINT32);
    const hz::MeshBounds& bounds = mesh->GetBounds(0);
    const float           minimum[] = { -1, -5, -2 }, maximum[] = { 4, 2, 6 };
    for (uint32_t axis = 0; axis < 3; ++axis)
    {
        EXPECT_FLOAT_EQ(bounds.minimum[axis], minimum[axis]);
        EXPECT_FLOAT_EQ(bounds.maximum[axis], maximum[axis]);
    }
    const hz::GpuMeshData* gpuMesh = assetManager.GetGpuTables().GetMesh({ .index = Handle.index, .generation = Handle.generation });
    ASSERT_NE(gpuMesh, nullptr);
    EXPECT_EQ(gpuMesh->indexStride, 4u);
    hz::GPUBuffer    readback = context.CreateBuffer({ .Size = 48,
                                                       .usage = RESOURCE_MEMORY_USAGE_GPU_TO_CPU,
                                                       .StartState = RESOURCE_STATE_COPY_DEST,
                                                       .Flags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT });
    hz::CommandList& commands = context.AcquireCommandList();
    commands.CopyBuffer(readback, 0, mesh->GetIndexBuffer(), 0, 12);
    commands.CopyBuffer(readback, 12, mesh->GetVertexBuffer(2), 0, 36);
    assetManager.Release(Handle);
    const hz::SubmitHandle submission = context.Submit(commands);
    assetManager.Shutdown();
    EXPECT_TRUE(context.IsComplete(submission));
    const uint32_t expectedIndices[] = { 0, 1, 2 };
    const float    expectedPositions[] = { -1, 2, 3, 4, -5, 6, 0, 1, -2 };
    const uint8_t* bytes = (const uint8_t*)readback.Get()->pCpuMappedAddress;
    EXPECT_EQ(memcmp(bytes, expectedIndices, sizeof(expectedIndices)), 0);
    EXPECT_EQ(memcmp(bytes + sizeof(expectedIndices), expectedPositions, sizeof(expectedPositions)), 0);
}

TEST_F(AssetManagerTest, WorldBindingsRetainSharedMeshesAndReleaseRemovedComponents)
{
    hz::AssetCatalog catalog;
    ASSERT_TRUE(
        catalog.Add({ .id = assetID(1), .Type = hz::AssetType::Mesh, .pSourcePath = "scene.gltf", .location = { .pPath = "scene.bin" } }));
    TestAssetLoader loader;
    loader.ready = false;
    hz::AssetManager assetManager(catalog);
    assetManager.SetLoader(hz::AssetType::Mesh, loader.GetLoader());
    hz::TypeRegistry types;
    ASSERT_TRUE(types.RegisterBuiltins());
    hz::World        world(types);
    const hz::Entity a = world.CreateEntity(), b = world.CreateEntity();
    hz::MeshRenderer renderer = { .mesh = assetID(1) };
    ASSERT_TRUE(world.SetComponent(a, hz::SceneTypes::meshRenderer, &renderer));
    ASSERT_TRUE(world.SetComponent(b, hz::SceneTypes::meshRenderer, &renderer));
    hz::WorldAssetBindings bindings(world, assetManager);
    bindings.Update();
    ASSERT_EQ(bindings.GetCount(), 2u);
    const hz::AssetHandle mesh = bindings.Get(a)->mesh;
    EXPECT_EQ(bindings.Get(b)->mesh, mesh);
    assetManager.Update();
    bindings.Update();
    EXPECT_EQ(bindings.Get(a)->state, hz::AssetBindingState::Pending);
    EXPECT_EQ(loader.begun, 1u);
    const MemoryTrackingStats before = GetMemoryTrackingStats();
    for (uint32_t i = 0; i < 100; ++i)
        bindings.Update();
    const MemoryTrackingStats after = GetMemoryTrackingStats();
    if (before.TrackingEnabled)
    {
        EXPECT_EQ(before.TotalAllocationCount, after.TotalAllocationCount);
        EXPECT_EQ(before.ReallocationCount, after.ReallocationCount);
    }
    ASSERT_TRUE(world.RemoveComponent(a, hz::SceneTypes::meshRenderer));
    bindings.Update();
    EXPECT_EQ(bindings.Get(a), nullptr);
    EXPECT_EQ(bindings.GetCount(), 1u);
    EXPECT_EQ(assetManager.GetState(mesh), hz::AssetState::LoadingCPU);
    ASSERT_TRUE(world.DestroyEntity(b));
    bindings.Update();
    EXPECT_EQ(bindings.GetCount(), 0u);
    EXPECT_EQ(assetManager.GetState(mesh), hz::AssetState::Retiring);
    loader.jobs[1]->ready = true;
    assetManager.Update();
    EXPECT_EQ(assetManager.GetCount(), 0u);
    EXPECT_EQ(loader.destroyed, (std::vector<uint32_t>{ 1 }));
}

TEST_F(AssetManagerTest, WorldBindingsSwapMeshesWithoutRetiringSharedAssets)
{
    hz::AssetCatalog catalog;
    for (uint32_t i = 1; i <= 2; ++i)
        ASSERT_TRUE(catalog.Add(
            { .id = assetID(i), .Type = hz::AssetType::Mesh, .pSourcePath = "scene.gltf", .location = { .pPath = "scene.bin" } }));
    TestAssetLoader loader;
    loader.ready = false;
    hz::AssetManager assetManager(catalog);
    assetManager.SetLoader(hz::AssetType::Mesh, loader.GetLoader());
    hz::TypeRegistry types;
    ASSERT_TRUE(types.RegisterBuiltins());
    hz::World        world(types);
    const hz::Entity a = world.CreateEntity(), b = world.CreateEntity();
    hz::MeshRenderer first = { .mesh = assetID(1) }, second = { .mesh = assetID(2) };
    ASSERT_TRUE(world.SetComponent(a, hz::SceneTypes::meshRenderer, &first));
    ASSERT_TRUE(world.SetComponent(b, hz::SceneTypes::meshRenderer, &second));
    hz::WorldAssetBindings bindings(world, assetManager);
    bindings.Update();
    const hz::AssetHandle meshA = bindings.Get(a)->mesh, meshB = bindings.Get(b)->mesh;
    assetManager.Update();
    ASSERT_TRUE(world.SetComponent(a, hz::SceneTypes::meshRenderer, &second));
    ASSERT_TRUE(world.SetComponent(b, hz::SceneTypes::meshRenderer, &first));
    bindings.Update();
    EXPECT_EQ(bindings.Get(a)->mesh, meshB);
    EXPECT_EQ(bindings.Get(b)->mesh, meshA);
    EXPECT_EQ(assetManager.GetState(meshA), hz::AssetState::LoadingCPU);
    EXPECT_EQ(assetManager.GetState(meshB), hz::AssetState::LoadingCPU);
    EXPECT_EQ(loader.begun, 2u);

    ASSERT_TRUE(world.DestroyEntity(a));
    bindings.Update();
    const hz::Entity replacement = world.CreateEntity();
    EXPECT_NE(replacement, a);
    ASSERT_TRUE(world.SetComponent(replacement, hz::SceneTypes::meshRenderer, &second));
    bindings.Update();
    EXPECT_EQ(bindings.Get(a), nullptr);
    EXPECT_EQ(bindings.Get(replacement)->state, hz::AssetBindingState::Pending);
    EXPECT_FALSE(bindings.Get(replacement)->mesh.IsValid());
    loader.jobs[2]->ready = true;
    assetManager.Update();
    bindings.Update();
    EXPECT_TRUE(bindings.Get(replacement)->mesh.IsValid());
    EXPECT_NE(bindings.Get(replacement)->mesh, meshB);
    bindings.Clear();
    EXPECT_EQ(bindings.GetCount(), 0u);
    EXPECT_EQ(assetManager.GetState(meshA), hz::AssetState::Retiring);
}

TEST_F(AssetManagerTest, RenderSceneKeepsPendingEntriesAndRejectsReusedGenerations)
{
    hz::AssetCatalog catalog;
    ASSERT_TRUE(
        catalog.Add({ .id = assetID(1), .Type = hz::AssetType::Mesh, .pSourcePath = "scene.gltf", .location = { .pPath = "scene.bin" } }));
    TestAssetLoader loader;
    loader.ready = false;
    hz::AssetManager assetManager(catalog);
    assetManager.SetLoader(hz::AssetType::Mesh, loader.GetLoader());
    hz::TypeRegistry types;
    ASSERT_TRUE(types.RegisterBuiltins());
    hz::World              world(types);
    hz::RenderScene        scene(world, assetManager);
    const hz::MeshRenderer renderer = { .mesh = assetID(1) };
    const hz::Entity       a = world.CreateEntity(), b = world.CreateEntity();
    ASSERT_TRUE(world.SetComponent(a, hz::SceneTypes::meshRenderer, &renderer));
    ASSERT_TRUE(world.SetComponent(b, hz::SceneTypes::meshRenderer, &renderer));
    scene.Update();
    assetManager.Update();
    const hz::RenderPrimitiveID first = scene.FindPrimitive(a), second = scene.FindPrimitive(b);
    ASSERT_TRUE(first.IsValid());
    ASSERT_TRUE(second.IsValid());
    EXPECT_EQ(scene.GetActivePrimitives().Count, 2u);
    EXPECT_EQ(scene.GetActiveInstances().Count, 2u);
    EXPECT_EQ(scene.GetActiveMeshBatches().Count, 0u);
    EXPECT_EQ(scene.GetPrimitive(first)->instanceCount, 0u);
    ASSERT_EQ(scene.GetChanges().instanceIndices.Count, 1u);
    EXPECT_EQ(scene.GetChanges().instanceIndices.Data[0].Count, 2u);
    for (uint32_t index : scene.GetInstanceIndices())
        EXPECT_EQ(index, UINT32_MAX);
    const hz::RenderInstanceID instance = scene.GetInstances().Data[first.index].id;
    const uint64_t             revision = scene.GetRevision();
    const MemoryTrackingStats  before = GetMemoryTrackingStats();
    for (uint32_t i = 0; i < 100; ++i)
        scene.Update();
    const MemoryTrackingStats after = GetMemoryTrackingStats();
    if (before.TrackingEnabled)
    {
        EXPECT_EQ(before.TotalAllocationCount, after.TotalAllocationCount);
        EXPECT_EQ(before.ReallocationCount, after.ReallocationCount);
    }
    EXPECT_EQ(scene.GetRevision(), revision);
    EXPECT_EQ(scene.GetChanges().primitives.ranges.Count, 0u);
    EXPECT_EQ(scene.GetChanges().instances.ranges.Count, 0u);
    ASSERT_TRUE(world.RemoveComponent(a, hz::SceneTypes::meshRenderer));
    scene.Update();
    EXPECT_EQ(scene.GetPrimitive(first), nullptr);
    EXPECT_EQ(scene.GetInstance(instance), nullptr);
    EXPECT_FALSE(scene.FindPrimitive(a).IsValid());
    EXPECT_EQ(scene.FindPrimitive(b), second);
    ASSERT_EQ(scene.GetChanges().primitives.removed.Count, 1u);
    EXPECT_EQ(scene.GetChanges().primitives.removed.Data[0], first.index);
    EXPECT_TRUE(scene.GetChanges().primitives.activeList);
    ASSERT_TRUE(world.SetComponent(a, hz::SceneTypes::meshRenderer, &renderer));
    scene.Update();
    const hz::RenderPrimitiveID replacement = scene.FindPrimitive(a);
    EXPECT_EQ(replacement.index, first.index);
    EXPECT_NE(replacement.generation, first.generation);
    EXPECT_EQ(scene.GetPrimitive(first), nullptr);
    EXPECT_EQ(scene.GetInstance(instance), nullptr);
    ASSERT_TRUE(world.DestroyEntity(b));
    scene.Update();
    EXPECT_EQ(scene.GetPrimitive(second), nullptr);
    EXPECT_EQ(scene.GetActivePrimitives().Count, 1u);
    EXPECT_EQ(scene.GetPrimitives().Count, 2u);
}

TEST_F(AssetManagerTest, RenderSceneMergesSparseTransformChangesWithoutAllocating)
{
    hz::AssetCatalog catalog;
    ASSERT_TRUE(
        catalog.Add({ .id = assetID(1), .Type = hz::AssetType::Mesh, .pSourcePath = "scene.gltf", .location = { .pPath = "scene.bin" } }));
    TestAssetLoader loader;
    loader.ready = false;
    hz::AssetManager assetManager(catalog);
    assetManager.SetLoader(hz::AssetType::Mesh, loader.GetLoader());
    hz::TypeRegistry types;
    ASSERT_TRUE(types.RegisterBuiltins());
    hz::World              world(types);
    hz::RenderScene        scene(world, assetManager);
    hz::Array<hz::Entity>  entities;
    const hz::MeshRenderer renderer = { .mesh = assetID(1) };
    for (uint32_t i = 0; i < 40; ++i)
    {
        const hz::Entity         entity = world.CreateEntity();
        const hz::LocalTransform transform;
        ASSERT_TRUE(world.SetComponent(entity, hz::SceneTypes::meshRenderer, &renderer));
        ASSERT_TRUE(world.SetComponent(entity, hz::SceneTypes::localTransform, &transform));
        entities.PushBack(entity);
    }
    scene.Update();
    assetManager.Update();
    scene.Update();
    const MemoryTrackingStats before = GetMemoryTrackingStats();
    for (hz::Entity entity : entities)
        if ((scene.FindPrimitive(entity).index % 2) == 0)
        {
            const hz::LocalTransform transform = { .translation = Vector3(2, 0, 0) };
            ASSERT_TRUE(world.SetComponent(entity, hz::SceneTypes::localTransform, &transform));
        }
    scene.Update();
    const MemoryTrackingStats after = GetMemoryTrackingStats();
    if (before.TrackingEnabled)
    {
        EXPECT_EQ(before.TotalAllocationCount, after.TotalAllocationCount);
        EXPECT_EQ(before.ReallocationCount, after.ReallocationCount);
    }
    ASSERT_EQ(scene.GetChanges().instances.ranges.Count, 20u);
    for (uint32_t i = 0; i < 20; ++i)
    {
        EXPECT_EQ(scene.GetChanges().instances.ranges.Data[i].first, i * 2);
        EXPECT_EQ(scene.GetChanges().instances.ranges.Data[i].Count, 1u);
    }
    EXPECT_FALSE(scene.GetChanges().instances.activeList);
    EXPECT_EQ(scene.GetChanges().primitives.ranges.Count, 0u);
    EXPECT_EQ(scene.GetChanges().instanceIndices.Count, 0u);
}

TEST_F(AssetManagerTest, RenderSceneExtractsOnlyChangedEntitiesAndFollowsParentMoves)
{
    hz::AssetCatalog catalog;
    ASSERT_TRUE(
        catalog.Add({ .id = assetID(1), .Type = hz::AssetType::Mesh, .pSourcePath = "scene.gltf", .location = { .pPath = "scene.bin" } }));
    TestAssetLoader loader;
    loader.ready = false;
    hz::AssetManager assetManager(catalog);
    assetManager.SetLoader(hz::AssetType::Mesh, loader.GetLoader());
    hz::TypeRegistry types;
    ASSERT_TRUE(types.RegisterBuiltins());
    hz::World              world(types);
    hz::RenderScene        scene(world, assetManager);
    const hz::MeshRenderer renderer = { .mesh = assetID(1) };
    const hz::Entity       parent = world.CreateEntity();
    const hz::Entity       child = world.CreateEntity();
    const hz::Entity       other = world.CreateEntity();
    for (hz::Entity entity : { parent, child, other })
    {
        const hz::LocalTransform transform;
        ASSERT_TRUE(world.SetComponent(entity, hz::SceneTypes::meshRenderer, &renderer));
        ASSERT_TRUE(world.SetComponent(entity, hz::SceneTypes::localTransform, &transform));
    }
    ASSERT_TRUE(world.SetParent(child, parent, false));
    scene.Update();
    assetManager.Update();
    scene.Update();
    scene.Update();
    EXPECT_EQ(scene.GetChanges().instances.ranges.Count, 0u);

    // Moving the parent moves the child's world transform without touching the child's components.
    const hz::LocalTransform moved = { .translation = Vector3(4, 0, 0) };
    ASSERT_TRUE(world.SetComponent(parent, hz::SceneTypes::localTransform, &moved));
    scene.Update();
    const hz::RenderInstance& childInstance = scene.GetInstances().Data[scene.FindPrimitive(child).index];
    EXPECT_FLOAT_EQ((float)childInstance.current.getElem(3, 0), 4);
    uint32_t changedInstances = 0;
    for (uint32_t i = 0; i < scene.GetChanges().instances.ranges.Count; ++i)
        changedInstances += scene.GetChanges().instances.ranges.Data[i].Count;
    EXPECT_EQ(changedInstances, 2u);
    EXPECT_FLOAT_EQ((float)scene.GetInstances().Data[scene.FindPrimitive(other).index].current.getElem(3, 0), 0);

    // Removing the binding entity and its whole subtree leaves only the unrelated entity.
    ASSERT_TRUE(world.DestroySubtree(parent));
    scene.Update();
    EXPECT_FALSE(scene.FindPrimitive(parent).IsValid());
    EXPECT_FALSE(scene.FindPrimitive(child).IsValid());
    EXPECT_TRUE(scene.FindPrimitive(other).IsValid());
    EXPECT_EQ(scene.GetActivePrimitives().Count, 1u);
}

TEST_F(AssetManagerTest, RenderScenePendingUploadsAccumulateAcrossUpdates)
{
    hz::AssetCatalog catalog;
    ASSERT_TRUE(
        catalog.Add({ .id = assetID(1), .Type = hz::AssetType::Mesh, .pSourcePath = "scene.gltf", .location = { .pPath = "scene.bin" } }));
    TestAssetLoader loader;
    loader.ready = false;
    hz::AssetManager assetManager(catalog);
    assetManager.SetLoader(hz::AssetType::Mesh, loader.GetLoader());
    hz::TypeRegistry types;
    ASSERT_TRUE(types.RegisterBuiltins());
    hz::World              world(types);
    hz::RenderScene        scene(world, assetManager);
    const hz::Entity       entity = world.CreateEntity();
    const hz::MeshRenderer renderer = { .mesh = assetID(1) };
    hz::LocalTransform     transform;
    ASSERT_TRUE(world.SetComponent(entity, hz::SceneTypes::meshRenderer, &renderer));
    ASSERT_TRUE(world.SetComponent(entity, hz::SceneTypes::localTransform, &transform));
    scene.Update();
    const hz::RenderSceneUploads initial = scene.GetPendingUploads();
    EXPECT_EQ(initial.primitives.Count, 1u);
    EXPECT_EQ(initial.instances.Count, 1u);
    EXPECT_EQ(initial.instanceIndices.Count, 1u);
    EXPECT_TRUE(initial.activePrimitives);
    EXPECT_TRUE(initial.activeInstances);
    EXPECT_FALSE(initial.activeMeshBatches);
    for (uint32_t i = 1; i <= 4; ++i)
    {
        transform.translation = Vector3((float)i, 0, 0);
        ASSERT_TRUE(world.SetComponent(entity, hz::SceneTypes::localTransform, &transform));
        scene.Update();
    }
    // GetChanges() covers only the latest Update; pending uploads keep everything not yet acknowledged.
    EXPECT_EQ(scene.GetChanges().primitives.ranges.Count, 0u);
    EXPECT_FALSE(scene.GetChanges().primitives.activeList);
    const hz::RenderSceneUploads moved = scene.GetPendingUploads();
    ASSERT_EQ(moved.instances.Count, 1u);
    EXPECT_EQ(moved.instances.Data[0].first, 0u);
    EXPECT_EQ(moved.instances.Data[0].Count, 1u);
    EXPECT_EQ(moved.primitives.Count, 1u);
    EXPECT_TRUE(moved.activePrimitives);
    ASSERT_TRUE(world.RemoveComponent(entity, hz::SceneTypes::meshRenderer));
    scene.Update();
    const hz::RenderSceneUploads removal = scene.GetPendingUploads();
    EXPECT_TRUE(removal.activePrimitives);
    EXPECT_TRUE(removal.activeInstances);
    EXPECT_FALSE(removal.activeMeshBatches);
    EXPECT_EQ(removal.primitives.Count, 1u);
    EXPECT_EQ(removal.instances.Count, 1u);
    EXPECT_EQ(removal.instanceIndices.Count, 1u);
}

TEST_F(AssetManagerTest, RenderSceneExtractsLightsAndCamerasWithStableIndicesAndNoFrameAllocation)
{
    hz::AssetCatalog catalog;
    hz::AssetManager assetManager(catalog);
    hz::TypeRegistry types;
    ASSERT_TRUE(types.RegisterBuiltins());
    hz::World       world(types);
    hz::RenderScene scene(world, assetManager);
    scene.Reserve(0, 0, 9, 9);
    const hz::Entity parent = world.CreateEntity();
    Matrix4          parentMatrix = Matrix4::translation(Vector3(10, 20, 30)) * Matrix4::scale(Vector3(2, 3, 4));
    parentMatrix.setElem(1, 0, 0.5f);
    ASSERT_TRUE(world.SetComponent(parent, hz::SceneTypes::localMatrix, &parentMatrix));
    hz::FixedArray<hz::Entity, 9> entities;
    hz::SpotLightComponent        light = { .InnerConeAngle = 10, .OuterConeAngle = 20 };
    light.Point.Local.Common.Base.Intensity = 5;
    light.Point.Local.Common.Base.LightColor = Vector3(0.1f, 0.2f, 0.3f);
    light.Point.Local.Common.Temperature = 3200;
    light.Point.Local.Common.bUseTemperature = true;
    light.Point.Local.IntensityUnits = hz::LightUnits::Candelas;
    hz::Camera                    lens = { .verticalFov = 0.8f, .nearPlane = 0.2f, .farPlane = 500 };
    hz::LocalTransform            transform = { .translation = Vector3(1, 2, 3) };
    for (uint32_t i = 0; i < 9; ++i)
    {
        entities[i] = world.CreatePersistentEntity({ .high = 2, .low = i + 1 });
        ASSERT_TRUE(world.SetComponent(entities[i], hz::SceneTypes::spotLight, &light));
        ASSERT_TRUE(world.SetComponent(entities[i], hz::SceneTypes::camera, &lens));
        ASSERT_TRUE(world.SetComponent(entities[i], hz::SceneTypes::localTransform, &transform));
        ASSERT_TRUE(world.SetParent(entities[i], parent, false));
    }
    scene.Update();
    EXPECT_EQ(scene.GetActiveLights().Count, 9u);
    EXPECT_EQ(scene.GetActiveCameras().Count, 9u);
    for (uint32_t i = 0; i < 9; ++i)
    {
        const hz::RenderLight* extracted = scene.GetLight(scene.FindLight(entities[i]));
        ASSERT_NE(extracted, nullptr);
        EXPECT_EQ(extracted->entityGuid, world.GetEntityGuid(entities[i]));
        EXPECT_EQ(extracted->Type, hz::LightType::Spot);
        EXPECT_EQ(extracted->ready, 1u);
        EXPECT_FLOAT_EQ(extracted->color[2], 0.3f);
        EXPECT_FLOAT_EQ(extracted->intensity, 5);
        EXPECT_FLOAT_EQ(extracted->InfluenceRadius, 10);
        EXPECT_FLOAT_EQ(extracted->innerConeAngle, 10.0f * (PI / 180));
        EXPECT_FLOAT_EQ(extracted->outerConeAngle, 20.0f * (PI / 180));
        EXPECT_FLOAT_EQ(extracted->colorTemperature, 3200);
        EXPECT_EQ(extracted->enableColorTemperature, extracted->Type != hz::LightType::Sky);
        EXPECT_FLOAT_EQ((float)extracted->transform.getElem(1, 0), 0.5f);
        EXPECT_FLOAT_EQ((float)extracted->transform.getElem(3, 0), 13);
        const hz::RenderCamera* camera = scene.GetCamera(scene.FindCamera(entities[i]));
        ASSERT_NE(camera, nullptr);
        EXPECT_EQ(camera->entityGuid, extracted->entityGuid);
        EXPECT_FLOAT_EQ((float)camera->current.getElem(3, 2), 42);
        EXPECT_FLOAT_EQ((float)camera->previous.getElem(3, 2), 42);
        EXPECT_FLOAT_EQ(camera->lens.verticalFov, 0.8f);
        EXPECT_FLOAT_EQ(camera->lens.nearPlane, 0.2f);
        EXPECT_FLOAT_EQ(camera->lens.farPlane, 500);
    }
    const uint64_t            initial = scene.GetRevision();
    const MemoryTrackingStats before = GetMemoryTrackingStats();
    for (uint32_t i = 0; i < 100; ++i)
        scene.Update();
    EXPECT_EQ(scene.GetRevision(), initial);
    // Change every other entity, then keep nonadjacent camera/light ranges separate.
    transform.translation = Vector3(2, 3, 4);
    lens.verticalFov = 0.6f;
    for (uint32_t i = 0; i < 9; i += 2)
    {
        ASSERT_TRUE(world.SetComponent(entities[i], hz::SceneTypes::localTransform, &transform));
        ASSERT_TRUE(world.SetComponent(entities[i], hz::SceneTypes::camera, &lens));
    }
    scene.Update();
    const hz::RenderSceneChanges changes = scene.GetChanges();
    EXPECT_GT(scene.GetRevision(), initial);
    EXPECT_EQ(changes.lights.ranges.Count, 5u);
    EXPECT_EQ(changes.cameras.ranges.Count, 5u);
    EXPECT_FALSE(changes.lights.activeList);
    EXPECT_FALSE(changes.cameras.activeList);
    const MemoryTrackingStats after = GetMemoryTrackingStats();
    if (before.TrackingEnabled)
    {
        EXPECT_EQ(before.TotalAllocationCount, after.TotalAllocationCount);
        EXPECT_EQ(before.ReallocationCount, after.ReallocationCount);
    }
    const hz::RenderLightID  oldLight = scene.FindLight(entities[0]);
    const hz::RenderCameraID oldCamera = scene.FindCamera(entities[0]);
    ASSERT_TRUE(world.RemoveComponent(entities[0], hz::SceneTypes::spotLight));
    ASSERT_TRUE(world.RemoveComponent(entities[0], hz::SceneTypes::camera));
    scene.Update();
    EXPECT_EQ(scene.GetLight(oldLight), nullptr);
    EXPECT_EQ(scene.GetCamera(oldCamera), nullptr);
    EXPECT_FALSE(scene.FindLight(entities[0]).IsValid());
    EXPECT_FALSE(scene.FindCamera(entities[0]).IsValid());
    EXPECT_TRUE(scene.GetChanges().lights.activeList);
    EXPECT_TRUE(scene.GetChanges().cameras.activeList);
    ASSERT_TRUE(world.SetComponent(entities[0], hz::SceneTypes::spotLight, &light));
    ASSERT_TRUE(world.SetComponent(entities[0], hz::SceneTypes::camera, &lens));
    scene.Update();
    EXPECT_EQ(scene.FindLight(entities[0]).index, oldLight.index);
    EXPECT_NE(scene.FindLight(entities[0]).generation, oldLight.generation);
    EXPECT_EQ(scene.FindCamera(entities[0]).index, oldCamera.index);
    EXPECT_NE(scene.FindCamera(entities[0]).generation, oldCamera.generation);
    const hz::RenderCamera* replacement = scene.GetCamera(scene.FindCamera(entities[0]));
    EXPECT_FLOAT_EQ(replacement->previousLens.verticalFov, 0.6f);
    EXPECT_FLOAT_EQ((float)replacement->previous.getElem(3, 0), 15.5f);
    ASSERT_TRUE(world.DestroySubtree(parent));
    scene.Update();
    EXPECT_EQ(scene.GetActiveLights().Count, 0u);
    EXPECT_EQ(scene.GetActiveCameras().Count, 0u);
}

TEST_F(AssetManagerTest, ConcreteLightsOnOneEntityHaveIndependentHandlesAndParameters)
{
    hz::TypeRegistry types;
    ASSERT_TRUE(types.RegisterBuiltins());
    hz::World        world(types);
    hz::AssetCatalog catalog;
    hz::AssetManager assets(catalog);
    hz::RenderScene  scene(world, assets);
    const hz::Entity entity = world.CreateEntity();
    for (const uint64_t type : hz::LightComponentTypes)
    {
        ASSERT_TRUE(world.AddComponent(entity, type));
    }
    scene.Update();
    ASSERT_EQ(scene.GetActiveLights().Count, 5u);
    const hz::RenderLightID point = scene.FindLight(entity, hz::LightType::Point);
    const hz::RenderLightID spot = scene.FindLight(entity, hz::LightType::Spot);
    const hz::RenderLightID rect = scene.FindLight(entity, hz::LightType::Rect);
    const hz::RenderLightID sun = scene.FindLight(entity, hz::LightType::Directional);
    const hz::RenderLightID sky = scene.FindLight(entity, hz::LightType::Sky);
    ASSERT_TRUE(point.IsValid() && spot.IsValid() && rect.IsValid() && sun.IsValid() && sky.IsValid());
    EXPECT_FLOAT_EQ(scene.GetLight(sun)->intensity, 10);
    EXPECT_FLOAT_EQ(scene.GetLight(sky)->intensity, 1);
    EXPECT_FLOAT_EQ(scene.GetLight(rect)->Width, 0.64f);
    EXPECT_FLOAT_EQ(scene.GetLight(spot)->outerConeAngle, 44.0f * (PI / 180));
    EXPECT_FLOAT_EQ(scene.GetLight(point)->InfluenceRadius, 10);
    hz::PointLightComponent bulb;
    bulb.Local.IntensityUnits = hz::LightUnits::Lumens;
    bulb.Local.Common.Base.Intensity = 4.0f * PI;
    ASSERT_TRUE(world.SetComponent(entity, hz::SceneTypes::pointLight, &bulb));
    hz::DirectionalLightComponent daylight;
    daylight.LightSourceAngle = 2.0f;
    daylight.ShadowSourceAngleFactor = 0.5f;
    ASSERT_TRUE(world.SetComponent(entity, hz::SceneTypes::directionalLight, &daylight));
    scene.Update();
    EXPECT_FLOAT_EQ(scene.GetLight(point)->intensity, 1.0f);
    EXPECT_FLOAT_EQ(scene.GetLight(sun)->ShadowConeRadius, 0.5f * sinf(PI / 180.0f));
    EXPECT_FLOAT_EQ(scene.GetLight(sky)->intensity, 1.0f);
    ASSERT_TRUE(world.RemoveComponent(entity, hz::SceneTypes::pointLight));
    scene.Update();
    EXPECT_EQ(scene.GetLight(point), nullptr);
    EXPECT_EQ(scene.FindLight(entity, hz::LightType::Sky), sky);
    EXPECT_EQ(scene.FindLight(entity, hz::LightType::Spot), spot);
    EXPECT_EQ(scene.GetActiveLights().Count, 4u);
}

TEST_F(AssetManagerTest, RenderSceneEmissionBindingsHandlePendingFailureAndLeaseHandoffs)
{
    hz::AssetCatalog catalog;
    ASSERT_TRUE(catalog.Add(
        { .id = assetID(1), .Type = hz::AssetType::Texture, .pSourcePath = "light.png", .location = { .pPath = "light.dds" } }));
    TestAssetLoader loader;
    loader.ready = false;
    hz::AssetManager assetManager(catalog);
    assetManager.SetLoader(hz::AssetType::Texture, loader.GetLoader());
    Add(catalog, 2);
    assetManager.SetLoader(hz::AssetType::Scene, loader.GetLoader());
    hz::TypeRegistry types;
    ASSERT_TRUE(types.RegisterBuiltins());
    hz::World        world(types);
    hz::RenderScene  scene(world, assetManager);
    const hz::Entity a = world.CreateEntity(), b = world.CreateEntity();
    const hz::SkyLightComponent sky = { .SourceType = hz::SkyLightSourceType::SpecifiedCubemap, .Cubemap = assetID(1) };
    hz::RectLightComponent      light = { .SourceTexture = assetID(1) };
    ASSERT_TRUE(world.SetComponent(a, hz::SceneTypes::skyLight, &sky));
    scene.Update();
    const hz::RenderLightID id = scene.FindLight(a);
    const hz::AssetHandle   Handle = assetManager.Find(light.SourceTexture);
    ASSERT_TRUE(Handle.IsValid());
    assetManager.Update();
    EXPECT_EQ(scene.GetLight(id)->ready, 0u);
    EXPECT_FALSE(scene.GetLight(id)->texture.IsValid());
    ASSERT_TRUE(world.AddComponent(b, hz::SceneTypes::pointLight));
    scene.Update();
    EXPECT_EQ(scene.GetLight(scene.FindLight(b))->ready, 1u);
    EXPECT_FALSE(scene.GetLight(scene.FindLight(b))->texture.IsValid());
    // Move the only lease from a to b in one Update, without retiring the texture.
    ASSERT_TRUE(world.RemoveComponent(a, hz::SceneTypes::skyLight));
    ASSERT_TRUE(world.RemoveComponent(b, hz::SceneTypes::pointLight));
    ASSERT_TRUE(world.SetComponent(b, hz::SceneTypes::rectLight, &light));
    scene.Update();
    assetManager.Update();
    EXPECT_EQ(assetManager.Find(light.SourceTexture), Handle);
    EXPECT_EQ(loader.begun, 1u);
    loader.jobs[1]->failed = true;
    loader.jobs[1]->ready = true;
    assetManager.Update();
    scene.Update();
    EXPECT_EQ(scene.GetLight(scene.FindLight(b))->ready, 0u);
    for (uint32_t i = 0; i < 20; ++i)
    {
        assetManager.Update();
        scene.Update();
    }
    EXPECT_EQ(loader.begun, 1u);
    light.SourceTexture = assetID(999);
    ASSERT_TRUE(world.SetComponent(b, hz::SceneTypes::rectLight, &light));
    scene.Update();
    assetManager.Update();
    EXPECT_FALSE(assetManager.Find(assetID(1)).IsValid());
    EXPECT_EQ(scene.GetLight(scene.FindLight(b))->ready, 0u);
    scene.Update();
    light.SourceTexture = {};
    ASSERT_TRUE(world.SetComponent(b, hz::SceneTypes::rectLight, &light));
    scene.Update();
    EXPECT_EQ(scene.GetLight(scene.FindLight(b))->ready, 1u);
    EXPECT_EQ(assetManager.GetCount(), 0u);
    // A Ready payload of another Type must never become a GPU texture reference.
    loader.ready = true;
    light.SourceTexture = assetID(2);
    ASSERT_TRUE(world.SetComponent(b, hz::SceneTypes::rectLight, &light));
    scene.Update();
    assetManager.Update();
    scene.Update();
    EXPECT_EQ(assetManager.GetState(assetManager.Find(light.SourceTexture)), hz::AssetState::Ready);
    EXPECT_EQ(scene.GetLight(scene.FindLight(b))->ready, 0u);
    EXPECT_FALSE(scene.GetLight(scene.FindLight(b))->texture.IsValid());
}

TEST_F(AssetManagerTest, WorldBindingsKeepFailedRequestsIdleUntilRebuilt)
{
    hz::AssetCatalog catalog;
    TestAssetLoader  loader;
    loader.ready = false;
    hz::AssetManager assetManager(catalog);
    assetManager.SetLoader(hz::AssetType::Mesh, loader.GetLoader());
    hz::TypeRegistry types;
    ASSERT_TRUE(types.RegisterBuiltins());
    hz::World              world(types);
    const hz::Entity       entity = world.CreateEntity();
    const hz::MeshRenderer renderer = { .mesh = assetID(1) };
    ASSERT_TRUE(world.SetComponent(entity, hz::SceneTypes::meshRenderer, &renderer));
    hz::WorldAssetBindings bindings(world, assetManager);
    bindings.Update();
    ASSERT_EQ(bindings.Get(entity)->state, hz::AssetBindingState::Failed);
    ASSERT_TRUE(
        catalog.Add({ .id = assetID(1), .Type = hz::AssetType::Mesh, .pSourcePath = "scene.gltf", .location = { .pPath = "scene.bin" } }));
    const MemoryTrackingStats before = GetMemoryTrackingStats();
    for (uint32_t i = 0; i < 100; ++i)
        bindings.Update();
    const MemoryTrackingStats after = GetMemoryTrackingStats();
    if (before.TrackingEnabled)
    {
        EXPECT_EQ(before.TotalAllocationCount, after.TotalAllocationCount);
        EXPECT_EQ(before.ReallocationCount, after.ReallocationCount);
    }
    EXPECT_EQ(assetManager.GetCount(), 0u);
    bindings.Clear();
    bindings.Update();
    EXPECT_EQ(bindings.Get(entity)->state, hz::AssetBindingState::Pending);
    EXPECT_TRUE(bindings.Get(entity)->mesh.IsValid());
    EXPECT_EQ(assetManager.GetCount(), 1u);
}

TEST_F(TypedAssetLiveTest, WorldBindingsResolveSubmeshMaterialsAndDeferReleasedPayloads)
{
    const std::string firstSubmesh = R"({"id":"8000000000000001", "draw":0, "material":"00000000-0000-4000-8000-000000000003"})";
    const std::string secondSubmesh = R"({"id":"8000000000000003", "draw":0, "material":"00000000-0000-4000-8000-000000000003"})";
    std::string       sceneText = kTypedScene;
    sceneText.replace(sceneText.find(firstSubmesh), firstSubmesh.size(), firstSubmesh + "," + secondSubmesh);
    std::string       overrideText = kTypedScene;
    const std::string originalID = "00000000-0000-4000-8000-000000000003";
    const std::string overrideID = "00000000-0000-4000-8000-000000000006";
    for (size_t offset = overrideText.find(originalID); offset != std::string::npos; offset = overrideText.find(originalID))
        overrideText.replace(offset, originalID.size(), overrideID);
    writeTexture();
    writeScene(sceneText.c_str());
    writeScene(overrideText.c_str(), "override.sceneasset.json");
    writeGeometry();
    hz::SceneAsset scene, overrideScene;
    ASSERT_TRUE(scene.Parse({ sceneText.data(), (uint32_t)sceneText.size() }));
    ASSERT_TRUE(overrideScene.Parse({ overrideText.data(), (uint32_t)overrideText.size() }));
    hz::AssetCatalog catalog;
    ASSERT_TRUE(catalog.AddScene(scene, "scene.sceneasset.json"));
    const hz::AssetID alternate = overrideScene.GetMaterials().Data[0].id;
    ASSERT_TRUE(catalog.Add({ .id = alternate,
                              .Type = hz::AssetType::Material,
                              .pSourcePath = "scene.gltf",
                              .location = { .pPath = "override.sceneasset.json", .elementIndex = 0 },
                              .dependencies = { scene.GetTextures().Data[0].id } }));
    hz::RenderContext context({ .pAppName = "WorldAssetBindingsTest", .enableGpuValidation = true });
    hz::AssetLoaders  loaders(context, { .vertexLayout = kAssetTestLayout, .cookedDirectory = DirectoryOtherFiles });
    hz::AssetManager  assetManager(catalog, &context);
    loaders.RegisterLoaders(assetManager);
    hz::TypeRegistry types;
    ASSERT_TRUE(types.RegisterBuiltins());
    hz::World        world(types);
    const hz::Entity a = world.CreateEntity(), b = world.CreateEntity();
    hz::MeshRenderer renderer = { .mesh = scene.GetMeshes().Data[0].id };
    ASSERT_TRUE(world.SetComponent(b, hz::SceneTypes::meshRenderer, &renderer));
    renderer.overrides.PushBack({ .submesh = UINT64_C(0x8000000000000003), .material = alternate });
    ASSERT_TRUE(world.SetComponent(a, hz::SceneTypes::meshRenderer, &renderer));
    hz::WorldAssetBindings bindings(world, assetManager);
    bindings.Update();
    const hz::AssetHandle mesh = bindings.Get(a)->mesh;
    EXPECT_EQ(bindings.Get(a)->state, hz::AssetBindingState::Pending);
    Wait(assetManager, mesh);
    bindings.Update();
    const hz::AssetHandle overrideMaterial = assetManager.Find(alternate);
    ASSERT_TRUE(overrideMaterial.IsValid());
    Wait(assetManager, overrideMaterial);
    bindings.Update();
    ASSERT_EQ(bindings.Get(a)->state, hz::AssetBindingState::Ready);
    ASSERT_EQ(bindings.Get(b)->state, hz::AssetBindingState::Ready);
    ASSERT_EQ(bindings.Get(a)->materials.Count, 2u);
    EXPECT_EQ(bindings.Get(a)->materials.Data[0].asset, scene.GetMaterials().Data[0].id);
    EXPECT_EQ(bindings.Get(a)->materials.Data[1].asset, alternate);
    EXPECT_EQ(bindings.Get(b)->materials.Data[1].asset, scene.GetMaterials().Data[0].id);
    EXPECT_EQ(bindings.Get(a)->materials.Data[1].Handle, overrideMaterial);

    const MemoryTrackingStats before = GetMemoryTrackingStats();
    for (uint32_t i = 0; i < 100; ++i)
        bindings.Update();
    const MemoryTrackingStats after = GetMemoryTrackingStats();
    if (before.TrackingEnabled)
    {
        EXPECT_EQ(before.TotalAllocationCount, after.TotalAllocationCount);
        EXPECT_EQ(before.ReallocationCount, after.ReallocationCount);
    }

    // An open command list may reference any ready payload, so released payloads stay alive until it completes.
    hz::CommandList& pending = context.AcquireCommandList();
    // Reverse authoring order; resolution follows stable SubmeshID, not override array position.
    renderer.overrides.PushBack({ .submesh = UINT64_C(0x8000000000000001), .material = alternate });
    ASSERT_TRUE(world.SetComponent(a, hz::SceneTypes::meshRenderer, &renderer));
    bindings.Update();
    ASSERT_EQ(bindings.Get(a)->state, hz::AssetBindingState::Ready);
    EXPECT_EQ(bindings.Get(a)->materials.Data[0].Handle, overrideMaterial);
    EXPECT_EQ(bindings.Get(a)->materials.Data[1].Handle, overrideMaterial);

    const hz::MeshRenderer pendingRenderer = { .mesh = scene.GetMeshes().Data[1].id };
    ASSERT_TRUE(world.SetComponent(a, hz::SceneTypes::meshRenderer, &pendingRenderer));
    bindings.Update();
    EXPECT_EQ(bindings.Get(a)->state, hz::AssetBindingState::Pending);
    EXPECT_EQ(bindings.Get(a)->materials.Count, 0u);
    assetManager.Update();
    EXPECT_EQ(assetManager.GetState(overrideMaterial), hz::AssetState::Retiring);

    renderer.overrides[0].submesh = 99;
    ASSERT_TRUE(world.SetComponent(a, hz::SceneTypes::meshRenderer, &renderer));
    bindings.Update();
    EXPECT_EQ(bindings.Get(a)->state, hz::AssetBindingState::Failed);
    renderer.overrides[0].submesh = renderer.overrides[1].submesh;
    ASSERT_TRUE(world.SetComponent(a, hz::SceneTypes::meshRenderer, &renderer));
    bindings.Update();
    EXPECT_EQ(bindings.Get(a)->state, hz::AssetBindingState::Failed);
    renderer.overrides.Clear();
    renderer.overrides.PushBack({ .submesh = UINT64_C(0x8000000000000001), .material = scene.GetTextures().Data[0].id });
    ASSERT_TRUE(world.SetComponent(a, hz::SceneTypes::meshRenderer, &renderer));
    bindings.Update();
    EXPECT_EQ(bindings.Get(a)->state, hz::AssetBindingState::Failed);

    ASSERT_TRUE(world.RemoveComponent(a, hz::SceneTypes::meshRenderer));
    ASSERT_TRUE(world.DestroyEntity(b));
    bindings.Update();
    assetManager.Update();
    EXPECT_EQ(bindings.GetCount(), 0u);
    EXPECT_EQ(assetManager.GetState(mesh), hz::AssetState::Retiring);
    EXPECT_EQ(assetManager.GetState(overrideMaterial), hz::AssetState::Retiring);
    context.Wait(context.Submit(pending));
    assetManager.Update();
    EXPECT_EQ(assetManager.GetCount(), 0u);
}

TEST_F(TypedAssetLiveTest, RenderSceneExtractsSubmeshesBoundsHistoryAndMaterialChanges)
{
    const std::string submesh = R"({"id":"8000000000000001", "draw":0, "material":"00000000-0000-4000-8000-000000000003"})";
    const std::string extra = R"({"id":"8000000000000003", "draw":0, "material":"00000000-0000-4000-8000-000000000003"})";
    std::string       text = kTypedScene;
    text.replace(text.find(submesh), submesh.size(), submesh + "," + extra);
    std::string       alternateText = kTypedScene;
    const std::string originalID = "00000000-0000-4000-8000-000000000003";
    const std::string alternateID = "00000000-0000-4000-8000-000000000006";
    for (size_t offset = alternateText.find(originalID); offset != std::string::npos; offset = alternateText.find(originalID))
        alternateText.replace(offset, originalID.size(), alternateID);
    writeTexture();
    writeGeometry();
    writeScene(text.c_str());
    writeScene(alternateText.c_str(), "alternate.sceneasset.json");
    hz::SceneAsset assets, alternateAssets;
    ASSERT_TRUE(assets.Parse({ text.data(), (uint32_t)text.size() }));
    ASSERT_TRUE(alternateAssets.Parse({ alternateText.data(), (uint32_t)alternateText.size() }));
    hz::AssetCatalog catalog;
    ASSERT_TRUE(catalog.AddScene(assets, "scene.sceneasset.json"));
    const hz::AssetID alternate = alternateAssets.GetMaterials().Data[0].id;
    ASSERT_TRUE(catalog.Add({ .id = alternate,
                              .Type = hz::AssetType::Material,
                              .pSourcePath = "scene.gltf",
                              .location = { .pPath = "alternate.sceneasset.json", .elementIndex = 0 },
                              .dependencies = { assets.GetTextures().Data[0].id } }));
    hz::RenderContext context({ .pAppName = "RenderSceneTest", .enableGpuValidation = true });
    hz::AssetLoaders  loaders(context, { .vertexLayout = kAssetTestLayout, .cookedDirectory = DirectoryOtherFiles });
    hz::AssetManager  assetManager(catalog);
    loaders.RegisterLoaders(assetManager);
    hz::TypeRegistry types;
    ASSERT_TRUE(types.RegisterBuiltins());
    hz::World        world(types);
    hz::RenderScene  scene(world, assetManager);
    const hz::Guid   identity = { .high = 2, .low = 1 };
    const hz::Entity a = world.CreatePersistentEntity(identity), b = world.CreateEntity();
    hz::MeshRenderer renderer = { .mesh = assets.GetMeshes().Data[0].id };
    ASSERT_TRUE(world.SetComponent(a, hz::SceneTypes::meshRenderer, &renderer));
    ASSERT_TRUE(world.SetComponent(b, hz::SceneTypes::meshRenderer, &renderer));
    Matrix4 local;
    local = Matrix4::scale(Vector3(-2, 3, 0.5f));
    local.setElem(1, 0, 1.0f); // Shear X by Y.
    local.setTranslation(Vector3(10, 20, 30));
    ASSERT_TRUE(world.SetComponent(a, hz::SceneTypes::localMatrix, &local));
    scene.Update();
    const hz::RenderPrimitiveID primitive = scene.FindPrimitive(a);
    Wait(assetManager, assetManager.Find(renderer.mesh));
    scene.Update();
    ASSERT_EQ(scene.GetPrimitive(primitive)->instanceCount, 1u);
    EXPECT_EQ(scene.FindPrimitive(a), primitive);
    ASSERT_EQ(scene.GetActiveMeshBatches().Count, 4u);
    ASSERT_EQ(scene.GetChanges().meshBatches.created.Count, 4u);
    const uint32_t             instanceIndex = scene.GetInstanceIndices().Data[scene.GetPrimitive(primitive)->instanceOffset];
    const hz::RenderInstanceID instanceID = scene.GetInstances().Data[instanceIndex].id;
    const hz::RenderInstance*  instance = scene.GetInstance(instanceID);
    ASSERT_NE(instance, nullptr);
    EXPECT_EQ(instance->entityGuid, identity);
    EXPECT_EQ(instance->primitive, primitive);
    EXPECT_FLOAT_EQ(instance->bounds.minimum[0], -3);
    EXPECT_FLOAT_EQ(instance->bounds.maximum[0], 14);
    EXPECT_FLOAT_EQ(instance->bounds.minimum[1], 5);
    EXPECT_FLOAT_EQ(instance->bounds.maximum[1], 26);
    EXPECT_FLOAT_EQ(instance->bounds.minimum[2], 29);
    EXPECT_FLOAT_EQ(instance->bounds.maximum[2], 33);
    hz::Array<hz::RenderMeshBatchID> meshBatchIDs;
    for (uint32_t index : scene.GetActiveMeshBatches())
        if (scene.GetMeshBatches().Data[index].primitive == primitive)
            meshBatchIDs.PushBack(scene.GetMeshBatches().Data[index].id);
    ASSERT_EQ(meshBatchIDs.Size(), 2u);
    EXPECT_EQ(scene.GetMeshBatch(meshBatchIDs[0])->submesh, UINT64_C(0x8000000000000001));
    EXPECT_EQ(scene.GetMeshBatch(meshBatchIDs[1])->submesh, UINT64_C(0x8000000000000003));
    const hz::GpuMaterialID original = scene.GetMeshBatch(meshBatchIDs[0])->material;

    scene.Update();
    const uint64_t            revision = scene.GetRevision();
    const MemoryTrackingStats before = GetMemoryTrackingStats();
    for (uint32_t i = 0; i < 100; ++i)
        scene.Update();
    const MemoryTrackingStats after = GetMemoryTrackingStats();
    if (before.TrackingEnabled)
    {
        EXPECT_EQ(before.TotalAllocationCount, after.TotalAllocationCount);
        EXPECT_EQ(before.ReallocationCount, after.ReallocationCount);
    }
    EXPECT_EQ(scene.GetRevision(), revision);
    EXPECT_EQ(scene.GetChanges().meshBatches.ranges.Count, 0u);
    local.setTranslation(Vector3(11, 20, 30));
    ASSERT_TRUE(world.SetComponent(a, hz::SceneTypes::localMatrix, &local));
    scene.Update();
    EXPECT_FLOAT_EQ((float)scene.GetInstance(instanceID)->current.getElem(3, 0), 11);
    EXPECT_FLOAT_EQ((float)scene.GetInstance(instanceID)->previous.getElem(3, 0), 11);
    EXPECT_EQ(scene.GetChanges().primitives.ranges.Count, 0u);
    EXPECT_EQ(scene.GetChanges().meshBatches.ranges.Count, 0u);
    ASSERT_EQ(scene.GetChanges().instances.ranges.Count, 1u);
    EXPECT_EQ(scene.GetChanges().instances.ranges.Data[0].first, instanceIndex);
    EXPECT_EQ(scene.GetChanges().instances.ranges.Data[0].Count, 1u);
    scene.Update();
    EXPECT_FLOAT_EQ((float)scene.GetInstance(instanceID)->previous.getElem(3, 0), 11);
    scene.MarkDirty(a);
    scene.Update();
    EXPECT_FLOAT_EQ((float)scene.GetInstance(instanceID)->previous.getElem(3, 0), 11);
    EXPECT_EQ(scene.GetChanges().instances.ranges.Count, 0u);

    renderer.overrides.PushBack({ .submesh = UINT64_C(0x8000000000000003), .material = alternate });
    ASSERT_TRUE(world.SetComponent(a, hz::SceneTypes::meshRenderer, &renderer));
    scene.Update();
    EXPECT_EQ(scene.GetPrimitive(primitive)->instanceCount, 0u);
    EXPECT_NE(scene.GetMeshBatch(meshBatchIDs[0]), nullptr);
    EXPECT_NE(scene.GetMeshBatch(meshBatchIDs[1]), nullptr);
    EXPECT_FALSE(scene.GetMeshBatch(meshBatchIDs[1])->material.IsValid());
    Wait(assetManager, assetManager.Find(alternate));
    scene.Update();
    EXPECT_EQ(scene.GetPrimitive(primitive)->instanceCount, 1u);
    EXPECT_EQ(scene.GetMeshBatch(meshBatchIDs[0])->material, original);
    const hz::AssetHandle alternateHandle = assetManager.Find(alternate);
    EXPECT_EQ(scene.GetMeshBatch(meshBatchIDs[1])->material,
              (hz::GpuMaterialID{ .index = alternateHandle.index, .generation = alternateHandle.generation }));
    EXPECT_EQ(scene.GetChanges().meshBatches.created.Count, 0u);
    renderer.overrides.Clear();
    ASSERT_TRUE(world.SetComponent(a, hz::SceneTypes::meshRenderer, &renderer));
    scene.Update();
    EXPECT_EQ(scene.GetMeshBatch(meshBatchIDs[1])->material, original);
    EXPECT_EQ(scene.GetChanges().primitives.ranges.Count, 0u);
    ASSERT_EQ(scene.GetChanges().meshBatches.ranges.Count, 1u);
    EXPECT_EQ(scene.GetChanges().meshBatches.ranges.Data[0].first, meshBatchIDs[1].index);
    EXPECT_EQ(scene.GetChanges().meshBatches.ranges.Data[0].Count, 1u);

    ASSERT_TRUE(world.DestroyEntity(a));
    scene.Update();
    EXPECT_EQ(scene.GetPrimitive(primitive), nullptr);
    EXPECT_EQ(scene.GetInstance(instanceID), nullptr);
    EXPECT_EQ(scene.GetMeshBatch(meshBatchIDs[0]), nullptr);
    EXPECT_EQ(scene.GetMeshBatch(meshBatchIDs[1]), nullptr);
    EXPECT_EQ(scene.GetInstanceIndices().Data[primitive.index], UINT32_MAX);
    EXPECT_EQ(scene.GetActiveMeshBatches().Count, 2u);
    const hz::RenderPrimitiveID survivor = scene.FindPrimitive(b);
    const uint32_t              survivingInstance = scene.GetInstanceIndices().Data[scene.GetPrimitive(survivor)->instanceOffset];
    EXPECT_EQ(scene.GetInstances().Data[survivingInstance].primitive, survivor);
    const hz::Entity replacement = world.CreatePersistentEntity(identity);
    ASSERT_TRUE(world.SetComponent(replacement, hz::SceneTypes::meshRenderer, &renderer));
    scene.Update();
    const hz::RenderPrimitiveID reused = scene.FindPrimitive(replacement);
    EXPECT_EQ(reused.index, primitive.index);
    EXPECT_NE(reused.generation, primitive.generation);
    EXPECT_EQ(scene.GetActiveMeshBatches().Count, 4u);
    for (hz::RenderMeshBatchID old : meshBatchIDs)
    {
        EXPECT_EQ(scene.GetMeshBatch(old), nullptr);
        EXPECT_NE(scene.GetMeshBatches().Data[old.index].id.generation, old.generation);
    }
}

struct GpuSceneCopy
{
    hz::GPUBuffer hz::GpuSceneBuffers::* table = nullptr;
    uint64_t                             srcOffset = 0;
    uint64_t                             dstOffset = 0;
    uint64_t                             Size = 0;
};

// Uploads one GpuScene frame, copies table bytes into the readback buffer and waits for the GPU.

static void renderGpuSceneFrame(hz::RenderContext& context, hz::GpuScene& gpu, const hz::GPUBuffer& readback, hz::Span<GpuSceneCopy> copies)
{
    ASSERT_TRUE(gpu.BeginFrame());
    hz::CommandList& commands = context.AcquireCommandList();
    gpu.Upload(commands);
    for (const GpuSceneCopy& copy : copies)
        commands.CopyBuffer(readback, copy.dstOffset, gpu.GetBuffers().*copy.table, copy.srcOffset, copy.Size);
    const hz::SubmitHandle submission = context.Submit(commands);
    gpu.EndFrame(submission);
    context.Wait(submission);
}

static float translationX(const Matrix4& matrix) { return (float)matrix.getElem(3, 0); }

TEST_F(TypedAssetLiveTest, GpuSceneUploadsChangesAndAdvancesHistoryOnlyForSubmittedFrames)
{
    writeTexture();
    writeScene();
    writeGeometry();
    hz::SceneAsset assets;
    ASSERT_TRUE(assets.Parse(kTypedScene));
    hz::AssetCatalog catalog;
    ASSERT_TRUE(catalog.AddScene(assets, "scene.sceneasset.json"));
    hz::RenderContext context({ .pAppName = "GpuSceneTest", .enableGpuValidation = true });
    hz::AssetLoaders  loaders(context, { .vertexLayout = kAssetTestLayout, .cookedDirectory = DirectoryOtherFiles });
    hz::AssetManager  assetManager(catalog, &context);
    loaders.RegisterLoaders(assetManager);
    hz::TypeRegistry types;
    ASSERT_TRUE(types.RegisterBuiltins());
    hz::World              world(types);
    hz::RenderScene        scene(world, assetManager);
    const hz::Entity       a = world.CreateEntity(), b = world.CreateEntity();
    const hz::MeshRenderer renderer = { .mesh = assets.GetMeshes().Data[0].id };
    hz::LocalTransform     transform = { .translation = Vector3(1, 0, 0) };
    ASSERT_TRUE(world.SetComponent(a, hz::SceneTypes::meshRenderer, &renderer));
    ASSERT_TRUE(world.SetComponent(b, hz::SceneTypes::meshRenderer, &renderer));
    ASSERT_TRUE(world.SetComponent(a, hz::SceneTypes::localTransform, &transform));
    scene.Update();
    const hz::AssetHandle meshHandle = assetManager.Find(renderer.mesh);
    Wait(assetManager, meshHandle);
    scene.Update();
    const hz::RenderPrimitiveID primitive = scene.FindPrimitive(a);
    const hz::GpuMeshID         meshID = scene.GetPrimitive(primitive)->mesh;
    const uint32_t              instanceIndex = scene.GetInstanceIndices().Data[scene.GetPrimitive(primitive)->instanceOffset];
    hz::RenderMeshBatchID       meshBatchID;
    for (uint32_t index : scene.GetActiveMeshBatches())
        if (scene.GetMeshBatches().Data[index].primitive == primitive)
            meshBatchID = scene.GetMeshBatches().Data[index].id;
    ASSERT_TRUE(meshBatchID.IsValid());
    const hz::GpuMaterialID materialID = scene.GetMeshBatch(meshBatchID)->material;
    const uint32_t textureSrv = assetManager.GetTexture(assetManager.Find(assets.GetTextures().Data[0].id))->GetTexture().GetSrvIndex();

    hz::GpuScene              gpu(scene, assetManager, context);
    hz::GPUBuffer             readback = context.CreateBuffer({ .Size = 1024,
                                                                .usage = RESOURCE_MEMORY_USAGE_GPU_TO_CPU,
                                                                .StartState = RESOURCE_STATE_COPY_DEST,
                                                                .Flags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT });
    const uint8_t*            pBytes = (const uint8_t*)readback.Get()->pCpuMappedAddress;
    const hz::RenderInstance& copiedInstance = *(const hz::RenderInstance*)pBytes;
    const hz::GpuMeshData&    copiedMesh = *(const hz::GpuMeshData*)(pBytes + 256);
    const GpuSceneCopy        instanceCopy = { .table = &hz::GpuSceneBuffers::instances,
                                               .srcOffset = sizeof(hz::RenderInstance) * instanceIndex,
                                               .Size = sizeof(hz::RenderInstance) };
    const GpuSceneCopy        meshCopy = { .table = &hz::GpuSceneBuffers::meshes,
                                           .srcOffset = sizeof(hz::GpuMeshData) * meshID.index,
                                           .dstOffset = 256,
                                           .Size = sizeof(hz::GpuMeshData) };
    const GpuSceneCopy        firstCopies[] = {
        instanceCopy,
        meshCopy,
        { .table = &hz::GpuSceneBuffers::materials,
                 .srcOffset = sizeof(hz::GpuMaterialData) * materialID.index,
                 .dstOffset = 512,
                 .Size = sizeof(hz::GpuMaterialData) },
    };
    renderGpuSceneFrame(context, gpu, readback, firstCopies);
    EXPECT_EQ(copiedInstance.id, scene.GetInstances().Data[instanceIndex].id);
    EXPECT_FLOAT_EQ(translationX(copiedInstance.current), 1);
    EXPECT_FLOAT_EQ(translationX(copiedInstance.previous), 1);
    EXPECT_EQ(copiedMesh.id, meshID);
    EXPECT_EQ(copiedMesh.indexStride, 2u);
    EXPECT_EQ(copiedMesh.submeshCount, 1u);
    EXPECT_EQ(copiedMesh.indexSrv, assetManager.GetMesh(meshHandle)->GetIndexBuffer().GetSrvIndex());
    const hz::GpuMaterialData& copiedMaterial = *(const hz::GpuMaterialData*)(pBytes + 512);
    EXPECT_EQ(copiedMaterial.Id, materialID);
    EXPECT_FLOAT_EQ(copiedMaterial.RoughnessFactor, 0.8f);
    EXPECT_EQ(copiedMaterial.BaseColorTexture, textureSrv);
    const hz::RenderSceneUploads acknowledged = scene.GetPendingUploads();
    EXPECT_EQ(acknowledged.primitives.Count + acknowledged.instances.Count + acknowledged.meshBatches.Count +
                  acknowledged.instanceIndices.Count,
              0u);
    EXPECT_FALSE(acknowledged.activePrimitives || acknowledged.activeInstances || acknowledged.activeMeshBatches);

    // Unchanged tables persist on the GPU without another Upload.
    transform.translation = Vector3(2, 0, 0);
    ASSERT_TRUE(world.SetComponent(a, hz::SceneTypes::localTransform, &transform));
    const GpuSceneCopy movedCopies[] = {
        instanceCopy,
        { .table = &hz::GpuSceneBuffers::textures,
          .srcOffset = sizeof(hz::GpuTextureData) * assetManager.Find(assets.GetTextures().Data[0].id).index,
          .dstOffset = 768,
          .Size = sizeof(hz::GpuTextureData) },
    };
    renderGpuSceneFrame(context, gpu, readback, movedCopies);
    EXPECT_FLOAT_EQ(translationX(copiedInstance.current), 2);
    EXPECT_FLOAT_EQ(translationX(copiedInstance.previous), 1);
    const hz::GpuTextureData& copiedTexture = *(const hz::GpuTextureData*)(pBytes + 768);
    EXPECT_TRUE(copiedTexture.id.IsValid());
    EXPECT_EQ(copiedTexture.srv, copiedMaterial.BaseColorTexture);
    EXPECT_EQ(copiedTexture.srv, textureSrv);

    // A cancelled frame keeps its changes pending and does not advance history.
    transform.translation = Vector3(3, 0, 0);
    ASSERT_TRUE(world.SetComponent(a, hz::SceneTypes::localTransform, &transform));
    ASSERT_TRUE(gpu.BeginFrame());
    gpu.CancelFrame();
    renderGpuSceneFrame(context, gpu, readback, { &instanceCopy, 1 });
    EXPECT_FLOAT_EQ(translationX(copiedInstance.current), 3);
    EXPECT_FLOAT_EQ(translationX(copiedInstance.previous), 2);
    renderGpuSceneFrame(context, gpu, readback, { &instanceCopy, 1 });
    EXPECT_FLOAT_EQ(translationX(copiedInstance.current), 3);
    EXPECT_FLOAT_EQ(translationX(copiedInstance.previous), 3);

    // Changes accumulate across updates until the next submitted frame.
    for (uint32_t i = 0; i < 12; ++i)
    {
        transform.translation = Vector3((float)i + 100, 0, 0);
        ASSERT_TRUE(world.SetComponent(a, hz::SceneTypes::localTransform, &transform));
        scene.Update();
    }
    renderGpuSceneFrame(context, gpu, readback, { &instanceCopy, 1 });
    EXPECT_FLOAT_EQ(translationX(copiedInstance.current), 111);
    EXPECT_FLOAT_EQ(translationX(copiedInstance.previous), 3);

    const MemoryTrackingStats before = GetMemoryTrackingStats();
    for (uint32_t i = 0; i < 30; ++i)
    {
        transform.translation = Vector3((float)i + 200, 0, 0);
        ASSERT_TRUE(world.SetComponent(a, hz::SceneTypes::localTransform, &transform));
        renderGpuSceneFrame(context, gpu, readback, { &instanceCopy, 1 });
        EXPECT_FLOAT_EQ(translationX(copiedInstance.current), (float)i + 200);
        EXPECT_FLOAT_EQ(translationX(copiedInstance.previous), i ? (float)i + 199 : 111.0f);
    }
    const MemoryTrackingStats after = GetMemoryTrackingStats();
    if (before.TrackingEnabled)
    {
        EXPECT_EQ(before.TotalAllocationCount, after.TotalAllocationCount);
        EXPECT_EQ(before.ReallocationCount, after.ReallocationCount);
    }

    // A teleport resets history for the next submitted frame.
    transform.translation = Vector3(300, 0, 0);
    ASSERT_TRUE(world.SetComponent(a, hz::SceneTypes::localTransform, &transform));
    scene.MarkDirty(a);
    renderGpuSceneFrame(context, gpu, readback, { &instanceCopy, 1 });
    EXPECT_FLOAT_EQ(translationX(copiedInstance.current), 300);
    EXPECT_FLOAT_EQ(translationX(copiedInstance.previous), 300);

    // Removal frees the slots; re-adding reuses them with new generations and fresh history.
    ASSERT_TRUE(world.RemoveComponent(a, hz::SceneTypes::meshRenderer));
    const GpuSceneCopy activeCopies[] = {
        { .table = &hz::GpuSceneBuffers::activePrimitives, .Size = sizeof(uint32_t) },
        { .table = &hz::GpuSceneBuffers::instanceIndices,
          .srcOffset = sizeof(uint32_t) * primitive.index,
          .dstOffset = sizeof(uint32_t),
          .Size = sizeof(uint32_t) },
    };
    renderGpuSceneFrame(context, gpu, readback, activeCopies);
    EXPECT_EQ(scene.GetActivePrimitives().Count, 1u);
    EXPECT_EQ(((const uint32_t*)pBytes)[0], scene.FindPrimitive(b).index);
    EXPECT_EQ(((const uint32_t*)pBytes)[1], UINT32_MAX);
    ASSERT_TRUE(world.SetComponent(a, hz::SceneTypes::meshRenderer, &renderer));
    renderGpuSceneFrame(context, gpu, readback, { &instanceCopy, 1 });
    const hz::RenderPrimitiveID reused = scene.FindPrimitive(a);
    EXPECT_EQ(reused.index, primitive.index);
    EXPECT_NE(reused.generation, primitive.generation);
    ASSERT_EQ(scene.GetInstanceIndices().Data[reused.index], instanceIndex);
    EXPECT_EQ(copiedInstance.id, scene.GetInstances().Data[instanceIndex].id);
    EXPECT_FLOAT_EQ(translationX(copiedInstance.current), 300);
    EXPECT_FLOAT_EQ(translationX(copiedInstance.previous), 300);

    // Released meshes leave the uploaded tables and retire once the GPU no longer uses them.
    ASSERT_TRUE(world.RemoveComponent(a, hz::SceneTypes::meshRenderer));
    ASSERT_TRUE(world.RemoveComponent(b, hz::SceneTypes::meshRenderer));
    renderGpuSceneFrame(context, gpu, readback, { &meshCopy, 1 });
    EXPECT_EQ(scene.GetActivePrimitives().Count, 0u);
    EXPECT_FALSE(copiedMesh.id.IsValid());
    assetManager.Update();
    EXPECT_EQ(assetManager.GetCount(), 0u);

    // Reloading uses a new slot generation; the old GPU mesh ID stays invalid.
    ASSERT_TRUE(world.SetComponent(a, hz::SceneTypes::meshRenderer, &renderer));
    scene.Update();
    const hz::AssetHandle reloaded = assetManager.Find(renderer.mesh);
    Wait(assetManager, reloaded);
    renderGpuSceneFrame(context, gpu, readback, { &meshCopy, 1 });
    EXPECT_NE(scene.GetPrimitive(scene.FindPrimitive(a))->mesh, meshID);
    EXPECT_NE(copiedMesh.id, meshID);
    EXPECT_EQ(scene.GetActiveMeshBatches().Count, 1u);
}

TEST_F(TypedAssetLiveTest, GpuSceneUploadsLightsAndCameraHistoryAndRetiresEmissionTextures)
{
    writeTexture();
    writeScene();
    writeGeometry();
    hz::SceneAsset assets;
    ASSERT_TRUE(assets.Parse(kTypedScene));
    hz::AssetCatalog catalog;
    ASSERT_TRUE(catalog.AddScene(assets, "scene.sceneasset.json"));
    hz::RenderContext context({ .pAppName = "GpuSceneLightTest", .enableGpuValidation = true });
    hz::AssetLoaders  loaders(context, { .vertexLayout = kAssetTestLayout, .cookedDirectory = DirectoryOtherFiles });
    hz::AssetManager  assetManager(catalog, &context);
    loaders.RegisterLoaders(assetManager);
    hz::TypeRegistry types;
    ASSERT_TRUE(types.RegisterBuiltins());
    hz::World              world(types);
    hz::RenderScene        scene(world, assetManager);
    const hz::Entity       entity = world.CreateEntity();
    hz::RectLightComponent light = { .SourceTexture = assets.GetTextures().Data[0].id };
    light.Local.Common.Base.Intensity = 4;
    light.Local.IntensityUnits = hz::LightUnits::Nits;
    hz::Camera             lens = { .verticalFov = 0.8f };
    hz::LocalTransform     transform = { .translation = Vector3(1, 0, 0) };
    const hz::MeshRenderer renderer = { .mesh = assets.GetMeshes().Data[0].id };
    ASSERT_TRUE(world.SetComponent(entity, hz::SceneTypes::rectLight, &light));
    ASSERT_TRUE(world.SetComponent(entity, hz::SceneTypes::camera, &lens));
    ASSERT_TRUE(world.SetComponent(entity, hz::SceneTypes::localTransform, &transform));
    scene.Update();
    const hz::RenderLightID  lightID = scene.FindLight(entity);
    const hz::RenderCameraID cameraID = scene.FindCamera(entity);
    const hz::AssetHandle    textureHandle = assetManager.Find(light.SourceTexture);
    EXPECT_EQ(scene.GetLight(lightID)->ready, 0u);
    Wait(assetManager, textureHandle);
    scene.Update();
    EXPECT_EQ(scene.GetLight(lightID)->ready, 1u); // A rect light is independent of the mesh on its entity.
    EXPECT_TRUE(scene.GetLight(lightID)->texture.IsValid());
    ASSERT_TRUE(world.SetComponent(entity, hz::SceneTypes::meshRenderer, &renderer));
    scene.Update();
    Wait(assetManager, assetManager.Find(renderer.mesh));
    scene.Update();
    ASSERT_EQ(scene.GetLight(lightID)->ready, 1u);

    hz::GpuScene gpu(scene, assetManager, context);
    gpu.Reserve(1, 1, 1, 1);
    hz::GPUBuffer           readback = context.CreateBuffer({ .Size = 1024,
                                                              .usage = RESOURCE_MEMORY_USAGE_GPU_TO_CPU,
                                                              .StartState = RESOURCE_STATE_COPY_DEST,
                                                              .Flags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT });
    const uint8_t*          pBytes = (const uint8_t*)readback.Get()->pCpuMappedAddress;
    const hz::RenderCamera& copiedCamera = *(const hz::RenderCamera*)pBytes;
    const hz::GpuLightData&  copiedLight = *(const hz::GpuLightData*)(pBytes + 512);
    const uint32_t*         pActive = (const uint32_t*)(pBytes + 1000);
    static_assert(sizeof(hz::RenderCamera) <= 512 && sizeof(hz::GpuLightData) <= 256);
    const GpuSceneCopy copies[] = {
        { .table = &hz::GpuSceneBuffers::cameras,
          .srcOffset = sizeof(hz::RenderCamera) * cameraID.index,
          .Size = sizeof(hz::RenderCamera) },
        { .table = &hz::GpuSceneBuffers::localLights,
          .srcOffset = sizeof(hz::GpuLightData) * lightID.index,
          .dstOffset = 512,
          .Size = sizeof(hz::GpuLightData) },
        { .table = &hz::GpuSceneBuffers::activeLocalLights, .dstOffset = 1000, .Size = sizeof(uint32_t) },
        { .table = &hz::GpuSceneBuffers::activeCameras, .dstOffset = 1004, .Size = sizeof(uint32_t) },
    };
    renderGpuSceneFrame(context, gpu, readback, copies);
    EXPECT_EQ(copiedCamera.id, cameraID);
    EXPECT_FLOAT_EQ(translationX(copiedCamera.current), 1);
    EXPECT_FLOAT_EQ(translationX(copiedCamera.previous), 1);
    EXPECT_FLOAT_EQ(copiedCamera.lens.verticalFov, 0.8f);
    EXPECT_EQ(copiedLight.Id, lightID);
    EXPECT_FLOAT_EQ((float)copiedLight.Color.getX(), 4);
    EXPECT_EQ(gpu.GetActiveLocalLightCount(), 1u);
    EXPECT_EQ(copiedLight.Texture, scene.GetLight(lightID)->texture);
    EXPECT_EQ(pActive[0], lightID.index);
    EXPECT_EQ(pActive[1], cameraID.index);

    // Full-loop and clustered upload share the same eligible lights as an Inspector edit takes effect.
    light.Local.Common.Base.bAffectsWorld = false;
    ASSERT_TRUE(world.SetComponent(entity, hz::SceneTypes::rectLight, &light));
    renderGpuSceneFrame(context, gpu, readback, {});
    EXPECT_EQ(gpu.GetActiveLocalLightCount(), 0u);
    light.Local.Common.Base.bAffectsWorld = true;
    light.Local.AttenuationRadius = 0;
    ASSERT_TRUE(world.SetComponent(entity, hz::SceneTypes::rectLight, &light));
    renderGpuSceneFrame(context, gpu, readback, {});
    EXPECT_EQ(gpu.GetActiveLocalLightCount(), 0u);
    light.Local.AttenuationRadius = 1000;
    ASSERT_TRUE(world.SetComponent(entity, hz::SceneTypes::rectLight, &light));
    // View-order entries follow the active scene list even when only the view changes.
    const hz::Entity secondLight = world.CreateEntity();
    ASSERT_TRUE(world.SetComponent(secondLight, hz::SceneTypes::localTransform, &transform));
    ASSERT_TRUE(world.AddComponent(secondLight, hz::SceneTypes::pointLight));
    ASSERT_TRUE(gpu.BeginFrame(true));
    const hz::Span<uint32_t> sceneOrder = scene.GetActiveLights();
    ASSERT_EQ(sceneOrder.Count, 2u);
    const uint32_t   viewOrder[] = { sceneOrder.Data[1], sceneOrder.Data[0] };
    hz::CommandList& viewCommands = context.AcquireCommandList();
    gpu.Upload(viewCommands, viewOrder);
    viewCommands.CopyBuffer(readback, 0, gpu.GetBuffers().activeLocalLights, 0, 4 * sizeof(uint32_t));
    const hz::RenderLightID pointID = scene.FindLight(secondLight);
    viewCommands.CopyBuffer(readback, 512, gpu.GetBuffers().localLights, sizeof(hz::GpuLightData) * pointID.index,
                            sizeof(hz::GpuLightData));
    const hz::SubmitHandle viewSubmission = context.Submit(viewCommands);
    gpu.EndFrame(viewSubmission);
    context.Wait(viewSubmission);
    const uint32_t* ids = reinterpret_cast<const uint32_t*>(pBytes);
    EXPECT_EQ(ids[0], viewOrder[1]);
    EXPECT_EQ(ids[1], viewOrder[0]);
    EXPECT_EQ(ids[2], viewOrder[0]);
    EXPECT_EQ(ids[3], viewOrder[1]);
    EXPECT_FLOAT_EQ(scene.GetLight(pointID)->InfluenceRadius, 10.0f);
    EXPECT_FLOAT_EQ((float)copiedLight.PositionAndInvRadius.getW(), 0.1f);
    const hz::LocalTransform movedPoint = { .translation = Vector3(3, 1, 0) };
    ASSERT_TRUE(world.SetComponent(secondLight, hz::SceneTypes::localTransform, &movedPoint));
    scene.Update();
    const hz::RenderSceneUploads moved = scene.GetPendingUploads();
    EXPECT_GT(moved.lights.Count, 0u);
    EXPECT_FALSE(moved.ActiveLocalLightIndices);
    ASSERT_TRUE(world.RemoveComponent(secondLight, hz::SceneTypes::pointLight));
    ASSERT_TRUE(world.AddComponent(secondLight, hz::SceneTypes::directionalLight));
    ASSERT_TRUE(gpu.BeginFrame());
    EXPECT_TRUE(scene.GetPendingUploads().ActiveLocalLightIndices);
    gpu.CancelFrame();
    const GpuSceneCopy localIndexCopy[] = {
        { .table = &hz::GpuSceneBuffers::activeLocalLights, .dstOffset = 1000, .Size = sizeof(uint32_t) },
    };
    renderGpuSceneFrame(context, gpu, readback, localIndexCopy);
    EXPECT_EQ(gpu.GetActiveLocalLightCount(), 1u);
    EXPECT_EQ(pActive[0], lightID.index);
    world.DestroyEntity(secondLight);

    transform.translation = Vector3(2, 0, 0);
    lens.verticalFov = 0.6f;
    light.Local.Common.Base.Intensity = 8;
    ASSERT_TRUE(world.SetComponent(entity, hz::SceneTypes::localTransform, &transform));
    ASSERT_TRUE(world.SetComponent(entity, hz::SceneTypes::camera, &lens));
    ASSERT_TRUE(world.SetComponent(entity, hz::SceneTypes::rectLight, &light));
    renderGpuSceneFrame(context, gpu, readback, copies);
    EXPECT_FLOAT_EQ(translationX(copiedCamera.current), 2);
    EXPECT_FLOAT_EQ(translationX(copiedCamera.previous), 1);
    EXPECT_FLOAT_EQ(copiedCamera.lens.verticalFov, 0.6f);
    EXPECT_FLOAT_EQ(copiedCamera.previousLens.verticalFov, 0.8f);
    EXPECT_FLOAT_EQ((float)copiedLight.Color.getX(), 8);
    EXPECT_FLOAT_EQ((float)copiedLight.PositionAndInvRadius.getX(), 2);
    renderGpuSceneFrame(context, gpu, readback, copies);
    EXPECT_FLOAT_EQ(translationX(copiedCamera.previous), 2);
    EXPECT_FLOAT_EQ(copiedCamera.previousLens.verticalFov, 0.6f);

    const MemoryTrackingStats before = GetMemoryTrackingStats();
    for (uint32_t i = 0; i < 30; ++i)
    {
        transform.translation = Vector3((float)i + 4, 0, 0);
        lens.verticalFov = 0.6f + 0.001f * i;
        ASSERT_TRUE(world.SetComponent(entity, hz::SceneTypes::localTransform, &transform));
        ASSERT_TRUE(world.SetComponent(entity, hz::SceneTypes::camera, &lens));
        renderGpuSceneFrame(context, gpu, readback, copies);
        EXPECT_FLOAT_EQ(translationX(copiedCamera.current), (float)i + 4);
        EXPECT_FLOAT_EQ((float)copiedLight.PositionAndInvRadius.getX(), (float)i + 4);
        EXPECT_FLOAT_EQ(translationX(copiedCamera.previous), i ? (float)i + 3 : 2.0f);
    }
    const MemoryTrackingStats after = GetMemoryTrackingStats();
    if (before.TrackingEnabled)
    {
        EXPECT_EQ(before.TotalAllocationCount, after.TotalAllocationCount);
        EXPECT_EQ(before.ReallocationCount, after.ReallocationCount);
    }

    // A camera cut resets history for the next submitted frame.
    transform.translation = Vector3(100, 0, 0);
    lens.verticalFov = 1;
    ASSERT_TRUE(world.SetComponent(entity, hz::SceneTypes::localTransform, &transform));
    ASSERT_TRUE(world.SetComponent(entity, hz::SceneTypes::camera, &lens));
    scene.MarkDirty(entity);
    renderGpuSceneFrame(context, gpu, readback, copies);
    EXPECT_FLOAT_EQ(translationX(copiedCamera.previous), 100);
    EXPECT_FLOAT_EQ(copiedCamera.previousLens.verticalFov, 1);

    // Removing the mesh preserves the independent light and its texture lease.
    ASSERT_TRUE(world.RemoveComponent(entity, hz::SceneTypes::meshRenderer));
    renderGpuSceneFrame(context, gpu, readback, copies);
    assetManager.Update();
    EXPECT_EQ(scene.FindLight(entity), lightID);
    EXPECT_EQ(gpu.GetActiveLocalLightCount(), 1u);
    EXPECT_EQ(assetManager.GetState(textureHandle), hz::AssetState::Ready);

    hz::SkyLightComponent sky = { .SourceType = hz::SkyLightSourceType::SpecifiedCubemap, .Cubemap = light.SourceTexture };
    ASSERT_TRUE(world.RemoveComponent(entity, hz::SceneTypes::rectLight));
    ASSERT_TRUE(world.SetComponent(entity, hz::SceneTypes::skyLight, &sky));
    scene.Update();
    const hz::RenderLightID skyId = scene.FindLight(entity, hz::LightType::Sky);
    ASSERT_TRUE(skyId.IsValid());
    GpuSceneCopy skyCopy = copies[1];
    skyCopy.srcOffset = sizeof(hz::GpuLightData) * skyId.index;
    renderGpuSceneFrame(context, gpu, readback, { &skyCopy, 1 });
    const hz::GpuTextureID textureID = copiedLight.Texture;
    ASSERT_TRUE(textureID.IsValid());
    ASSERT_TRUE(world.RemoveComponent(entity, hz::SceneTypes::skyLight));
    ASSERT_TRUE(world.RemoveComponent(entity, hz::SceneTypes::camera));
    const GpuSceneCopy textureCopy = { .table = &hz::GpuSceneBuffers::textures,
                                       .srcOffset = sizeof(hz::GpuTextureData) * textureID.index,
                                       .dstOffset = 768,
                                       .Size = sizeof(hz::GpuTextureData) };
    renderGpuSceneFrame(context, gpu, readback, { &textureCopy, 1 });
    EXPECT_EQ(scene.GetActiveLights().Count, 0u);
    EXPECT_EQ(scene.GetActiveCameras().Count, 0u);
    EXPECT_FALSE(((const hz::GpuTextureData*)(pBytes + 768))->id.IsValid());
    assetManager.Update();
    EXPECT_EQ(assetManager.GetCount(), 0u);

    sky.Cubemap = {};
    ASSERT_TRUE(world.SetComponent(entity, hz::SceneTypes::skyLight, &sky));
    ASSERT_TRUE(world.SetComponent(entity, hz::SceneTypes::camera, &lens));
    renderGpuSceneFrame(context, gpu, readback, copies);
    const hz::RenderCameraID reusedCamera = scene.FindCamera(entity);
    EXPECT_EQ(reusedCamera.index, cameraID.index);
    EXPECT_NE(reusedCamera.generation, cameraID.generation);
    EXPECT_EQ(scene.FindLight(entity).index, skyId.index);
    EXPECT_NE(scene.FindLight(entity).generation, skyId.generation);
    EXPECT_EQ(copiedCamera.id, reusedCamera);
    EXPECT_FLOAT_EQ(translationX(copiedCamera.previous), 100);
}

TEST_F(AssetManagerTest, GpuSceneUploadsSparseChangesForOneHundredThousandEntities)
{
    hz::AssetCatalog catalog;
    ASSERT_TRUE(
        catalog.Add({ .id = assetID(1), .Type = hz::AssetType::Mesh, .pSourcePath = "scene.gltf", .location = { .pPath = "scene.bin" } }));
    TestAssetLoader loader;
    loader.ready = false;
    hz::RenderContext context({ .pAppName = "GpuSceneStressTest" });
    hz::AssetManager  assetManager(catalog, &context);
    assetManager.SetLoader(hz::AssetType::Mesh, loader.GetLoader());
    hz::TypeRegistry types;
    ASSERT_TRUE(types.RegisterBuiltins());
    hz::World              world(types);
    hz::RenderScene        scene(world, assetManager);
    constexpr uint32_t     kEntityCount = 100000;
    constexpr uint32_t     kStride = 100;
    const hz::MeshRenderer renderer = { .mesh = assetID(1) };
    hz::Array<hz::Entity>  entities;
    entities.Reserve(kEntityCount);
    for (uint32_t i = 0; i < kEntityCount; ++i)
    {
        const hz::Entity         entity = world.CreateEntity();
        const hz::LocalTransform transform = { .translation = Vector3((float)i, 0, 0) };
        ASSERT_TRUE(world.SetComponent(entity, hz::SceneTypes::meshRenderer, &renderer));
        ASSERT_TRUE(world.SetComponent(entity, hz::SceneTypes::localTransform, &transform));
        entities.PushBack(entity);
    }
    hz::GpuScene gpu(scene, assetManager, context);
    gpu.Reserve(kEntityCount, kEntityCount);
    hz::GPUBuffer             readback = context.CreateBuffer({ .Size = sizeof(hz::RenderInstance),
                                                                .usage = RESOURCE_MEMORY_USAGE_GPU_TO_CPU,
                                                                .StartState = RESOURCE_STATE_COPY_DEST,
                                                                .Flags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT });
    const hz::RenderInstance& copied = *(const hz::RenderInstance*)readback.Get()->pCpuMappedAddress;
    const int64_t             fullStart = GetUSec(true);
    renderGpuSceneFrame(context, gpu, readback, {});
    const int64_t fullUSec = GetUSec(true) - fullStart;

    // Move every hundredth entity per frame: 1000 separate instance ranges.
    int64_t sparseUSec = 0;
    for (uint32_t frame = 0; frame < 10; ++frame)
    {
        for (uint32_t i = frame; i < kEntityCount; i += kStride)
        {
            const hz::LocalTransform transform = { .translation = Vector3((float)i, (float)frame + 1, 0) };
            ASSERT_TRUE(world.SetComponent(entities[i], hz::SceneTypes::localTransform, &transform));
        }
        // Primitive and instance slots are allocated together, so their indices match.
        const uint32_t     instance = scene.FindPrimitive(entities[frame]).index;
        const GpuSceneCopy copy = { .table = &hz::GpuSceneBuffers::instances,
                                    .srcOffset = sizeof(hz::RenderInstance) * instance,
                                    .Size = sizeof(hz::RenderInstance) };
        const int64_t      start = GetUSec(true);
        renderGpuSceneFrame(context, gpu, readback, { &copy, 1 });
        sparseUSec += GetUSec(true) - start;
        ASSERT_EQ(scene.GetInstances().Data[instance].primitive, scene.FindPrimitive(entities[frame]));
        EXPECT_FLOAT_EQ((float)copied.current.getElem(3, 1), (float)frame + 1);
        EXPECT_EQ(scene.GetChanges().instances.ranges.Count, kEntityCount / kStride);
    }
    // Telemetry only; timings vary across machines.
    printf("GpuScene %u entities: full Upload frame %.2f ms, 1%% sparse frame %.2f ms\n", kEntityCount, fullUSec / 1000.0,
           sparseUSec / 10000.0);
}

TEST_F(TypedAssetLiveTest, InstantiatesEditsSavesAndReloadsWorldWithGpuAssetRebinding)
{
    writeTexture();
    writeScene();
    writeGeometry();
    hz::SceneAsset assets;
    ASSERT_TRUE(assets.Parse(kTypedScene));
    hz::AssetCatalog catalog;
    ASSERT_TRUE(catalog.AddScene(assets, "scene.sceneasset.json"));
    hz::RenderContext context({ .pAppName = "WorldWorkflowTest", .enableGpuValidation = true });
    hz::AssetLoaders  loaders(context, { .vertexLayout = kAssetTestLayout, .cookedDirectory = DirectoryOtherFiles });
    hz::AssetManager  assetManager(catalog, &context);
    loaders.RegisterLoaders(assetManager);
    const hz::AssetHandle source = assetManager.Request(assets.GetID());
    Wait(assetManager, source);
    ASSERT_EQ(assetManager.GetState(source), hz::AssetState::Ready);
    hz::TypeRegistry types;
    ASSERT_TRUE(types.RegisterBuiltins());
    hz::UniquePtr<hz::World> world = hz::MakeUnique<hz::World>(types);
    const hz::Entity          root = assetManager.GetScene(source)->Instantiate(*world);
    hz::Entity                first, second;
    {
        hz::WorldQuery query(*world, { hz::SceneTypes::meshRenderer });
        struct Nodes
        {
            hz::Entity* first;
            hz::Entity* second;
            hz::AssetID mesh;
        } nodes = { &first, &second, assets.GetMeshes().Data[0].id };
        query.Each(
            [](hz::Entity entity, hz::Span<const void*> fields, void* user)
            {
                Nodes& nodes = *(Nodes*)user;
                *(((const hz::MeshRenderer*)fields.Data[0])->mesh == nodes.mesh ? nodes.first : nodes.second) = entity;
                return true;
            },
            &nodes);
    }
    ASSERT_TRUE(first.IsValid() && second.IsValid());
    Matrix4 transform = Matrix4::translation(Vector3(5, 0, 0));
    transform.setElem(1, 0, 0.25f);
    ASSERT_TRUE(world->SetComponent(root, hz::SceneTypes::localMatrix, &transform));
    ASSERT_TRUE(world->SetParent(first, second, false));
    const hz::Name name = { "Edited" };
    ASSERT_TRUE(world->SetComponent(first, hz::SceneTypes::name, &name));
    hz::MeshRenderer renderer = { .mesh = assets.GetMeshes().Data[0].id };
    renderer.overrides.PushBack(
        { .submesh = assets.GetMeshes().Data[0].submeshes.Data[0].id, .material = assets.GetMaterials().Data[0].id });
    ASSERT_TRUE(world->SetComponent(first, hz::SceneTypes::meshRenderer, &renderer));
    const hz::Guid   rootID = world->GetEntityGuid(root), firstID = world->GetEntityGuid(first), secondID = world->GetEntityGuid(second);
    hz::WorldArchive saved;
    ASSERT_TRUE(saved.Capture(*world));
    ASSERT_TRUE(saved.Save(DirectoryOtherFiles, "edited.world.json"));
    hz::Array<char> original;
    ASSERT_TRUE(saved.Serialize(original));
    hz::UniquePtr<hz::RenderScene> scene = hz::MakeUnique<hz::RenderScene>(*world, assetManager);
    scene->Update();
    ASSERT_EQ(scene->GetActiveMeshBatches().Count, 2u);
    ASSERT_TRUE(assetManager.Release(source));
    hz::UniquePtr<hz::GpuScene> gpu = hz::MakeUnique<hz::GpuScene>(*scene, assetManager, context);
    hz::GPUBuffer                readback = context.CreateBuffer({ .Size = sizeof(hz::RenderInstance) + sizeof(hz::GpuMaterialData) + 36,
                                                                   .usage = RESOURCE_MEMORY_USAGE_GPU_TO_CPU,
                                                                   .StartState = RESOURCE_STATE_COPY_DEST,
                                                                   .Flags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT });
    const hz::AssetHandle        oldMesh = assetManager.Find(renderer.mesh);
    for (uint32_t pass = 0; pass < 2; ++pass)
    {
        const hz::Entity entity = world->GetEntity(firstID);
        ASSERT_TRUE(entity.IsValid());
        EXPECT_EQ(world->GetParent(entity), world->GetEntity(secondID));
        EXPECT_EQ(world->GetParent(world->GetEntity(secondID)), world->GetEntity(rootID));
        EXPECT_STREQ(((const hz::Name*)world->GetComponent(entity, hz::SceneTypes::name))->value, "Edited");
        const hz::MeshRenderer* meshRenderer = (const hz::MeshRenderer*)world->GetComponent(entity, hz::SceneTypes::meshRenderer);
        ASSERT_NE(meshRenderer, nullptr);
        ASSERT_EQ(meshRenderer->overrides.Size(), 1u);
        EXPECT_EQ(meshRenderer->mesh, renderer.mesh);
        EXPECT_EQ(meshRenderer->overrides[0].submesh, renderer.overrides[0].submesh);
        EXPECT_EQ(meshRenderer->overrides[0].material, renderer.overrides[0].material);
        ASSERT_TRUE(gpu->BeginFrame());
        ASSERT_EQ(scene->GetActiveMeshBatches().Count, 2u);
        const hz::RenderPrimitiveID primitiveID = scene->FindPrimitive(entity);
        const hz::RenderPrimitive&  primitive = *scene->GetPrimitive(primitiveID);
        const uint32_t              instance = scene->GetInstanceIndices().Data[primitive.instanceOffset];
        const hz::RenderMeshBatch*  meshBatch = nullptr;
        for (uint32_t index : scene->GetActiveMeshBatches())
            if (scene->GetMeshBatches().Data[index].primitive == primitiveID)
                meshBatch = &scene->GetMeshBatches().Data[index];
        ASSERT_NE(meshBatch, nullptr);
        EXPECT_EQ(meshBatch->submesh, renderer.overrides[0].submesh);
        const hz::GpuMaterialData* material = gpu->GetAssetTables().GetMaterial(meshBatch->material);
        ASSERT_NE(material, nullptr);
        EXPECT_NE(material->BaseColorTexture, UINT32_MAX);
        const hz::MeshAsset* mesh = assetManager.GetMesh(assetManager.Find(renderer.mesh));
        ASSERT_NE(mesh, nullptr);
        hz::CommandList& commands = context.AcquireCommandList();
        gpu->Upload(commands);
        commands.CopyBuffer(readback, 0, gpu->GetBuffers().instances, instance * sizeof(hz::RenderInstance), sizeof(hz::RenderInstance));
        commands.CopyBuffer(readback, sizeof(hz::RenderInstance), gpu->GetBuffers().materials,
                            meshBatch->material.index * sizeof(hz::GpuMaterialData), sizeof(hz::GpuMaterialData));
        commands.CopyBuffer(readback, sizeof(hz::RenderInstance) + sizeof(hz::GpuMaterialData), mesh->GetVertexBuffer(2), 0, 36);
        const hz::SubmitHandle submission = context.Submit(commands);
        gpu->EndFrame(submission);
        // Destroying the GPU scene, bindings and World does not Wait; buffers and payloads retire after the frame.
        gpu = nullptr;
        scene = nullptr;
        world = nullptr;
        context.Wait(submission);
        assetManager.Update();
        EXPECT_EQ(assetManager.GetCount(), 0u);
        EXPECT_EQ(assetManager.GetMesh(oldMesh), nullptr);
        const hz::RenderInstance* copied = (const hz::RenderInstance*)readback.Get()->pCpuMappedAddress;
        EXPECT_EQ(copied->entityGuid, firstID);
        EXPECT_FLOAT_EQ((float)copied->current.getElem(3, 0), 6);
        EXPECT_FLOAT_EQ((float)copied->previous.getElem(3, 0), 6);
        EXPECT_FLOAT_EQ((float)copied->current.getElem(1, 0), 0.25f);
        const hz::GpuMaterialData* copiedMaterial = (const hz::GpuMaterialData*)((const uint8_t*)copied + sizeof(*copied));
        EXPECT_FLOAT_EQ(copiedMaterial->RoughnessFactor, 0.8f);
        EXPECT_FLOAT_EQ(copiedMaterial->BaseColorFactor[1], 0.5f);
        const float* vertices = (const float*)((const uint8_t*)copiedMaterial + sizeof(*copiedMaterial));
        EXPECT_FLOAT_EQ(vertices[0], -1);
        EXPECT_FLOAT_EQ(vertices[4], -5);
        if (pass == 1)
            break;
        hz::WorldArchive loaded;
        ASSERT_TRUE(loaded.Load(DirectoryOtherFiles, "edited.world.json"));
        ASSERT_TRUE(loaded.Instantiate(types, world));
        EXPECT_EQ(loaded.GetID(), saved.GetID());
        ASSERT_TRUE(loaded.Capture(*world));
        hz::Array<char> restored;
        ASSERT_TRUE(loaded.Serialize(restored));
        ASSERT_EQ(restored.Size(), original.Size());
        EXPECT_EQ(memcmp(restored.GetData(), original.GetData(), original.Size()), 0);
        scene = hz::MakeUnique<hz::RenderScene>(*world, assetManager);
        scene->Update();
        const hz::AssetHandle reloadedMesh = assetManager.Find(renderer.mesh);
        ASSERT_TRUE(reloadedMesh.IsValid());
        EXPECT_NE(reloadedMesh, oldMesh);
        Wait(assetManager, reloadedMesh);
        Wait(assetManager, assetManager.Find(assets.GetMeshes().Data[1].id));
        scene->Update();
        gpu = hz::MakeUnique<hz::GpuScene>(*scene, assetManager, context);
    }
    assetManager.Shutdown();
}

TEST_F(TypedAssetLiveTest, RejectsBrokenGeometryAndMismatchedSubassetsAndCancelsLoads)
{
    writeTexture();
    writeScene();
    writeGeometry();
    hz::SceneAsset scene;
    ASSERT_TRUE(scene.Parse(kTypedScene));
    hz::AssetCatalog catalog;
    ASSERT_TRUE(catalog.AddScene(scene, "scene.sceneasset.json"));
    hz::RenderContext context({ .pAppName = "TypedAssetFailureTest" });
    hz::AssetLoaders  loaders(context, { .vertexLayout = kAssetTestLayout, .cookedDirectory = DirectoryOtherFiles });

    // Every prefix must fail without leaking partially decoded blocks or publishing GPU buffers.
    const uintmax_t Size = std::filesystem::file_size(directory / "scene.bin");
    const uintmax_t header = sizeof(GEOMETRY_FILE_MAGIC_STR) + sizeof(GEOMETRY_FILE_VERSION) + sizeof(GEOMETRY_FILE_LAYOUT);
    const uintmax_t prefixes[] = { 0, 10, 14, header, header + 4, header + 4 + sizeof(Geometry), Size - 1 };
    for (uintmax_t prefix : prefixes)
    {
        writeGeometry();
        std::filesystem::resize_file(directory / "scene.bin", prefix);
        hz::AssetManager assetManager(catalog);
        loaders.RegisterLoaders(assetManager);
        const hz::AssetHandle mesh = assetManager.Request(scene.GetMeshes().Data[0].id);
        Wait(assetManager, mesh);
        EXPECT_EQ(assetManager.GetError(mesh), hz::AssetError::UploadFailed) << prefix;
        EXPECT_EQ(assetManager.GetMesh(mesh), nullptr);
    }
    // A file from another format version or struct layout is rejected instead of reinterpreted.
    for (uintmax_t offset : { sizeof(GEOMETRY_FILE_MAGIC_STR), sizeof(GEOMETRY_FILE_MAGIC_STR) + sizeof(GEOMETRY_FILE_VERSION) })
    {
        writeGeometry();
        std::fstream file(directory / "scene.bin", std::ios::in | std::ios::out | std::ios::binary);
        file.seekp((std::streamoff)offset);
        const uint32_t stale = 1;
        file.write((const char*)&stale, sizeof(stale));
        file.close();
        hz::AssetManager assetManager(catalog);
        loaders.RegisterLoaders(assetManager);
        const hz::AssetHandle mesh = assetManager.Request(scene.GetMeshes().Data[0].id);
        Wait(assetManager, mesh);
        EXPECT_EQ(assetManager.GetError(mesh), hz::AssetError::UploadFailed) << offset;
    }
    writeGeometry((IndexType)2);
    {
        hz::AssetManager assetManager(catalog);
        loaders.RegisterLoaders(assetManager);
        const hz::AssetHandle mesh = assetManager.Request(scene.GetMeshes().Data[0].id);
        Wait(assetManager, mesh);
        EXPECT_EQ(assetManager.GetError(mesh), hz::AssetError::UploadFailed);
        EXPECT_EQ(assetManager.GetMesh(mesh), nullptr);
    }
    std::filesystem::remove(directory / "scene.bin");
    {
        hz::AssetManager assetManager(catalog);
        loaders.RegisterLoaders(assetManager);
        const hz::AssetHandle mesh = assetManager.Request(scene.GetMeshes().Data[0].id);
        Wait(assetManager, mesh);
        EXPECT_EQ(assetManager.GetError(mesh), hz::AssetError::UploadFailed);
    }
    writeGeometry();
    hz::AssetCatalogEntry wrong = *catalog.Find(scene.GetMeshes().Data[0].id);
    wrong.location.elementIndex = 1;
    ASSERT_TRUE(catalog.Replace(wrong));
    {
        hz::AssetManager assetManager(catalog);
        loaders.RegisterLoaders(assetManager);
        const hz::AssetHandle mismatched = assetManager.Request(wrong.id);
        const hz::AssetHandle root = assetManager.Request(scene.GetID());
        Wait(assetManager, root);
        EXPECT_EQ(assetManager.GetError(root), hz::AssetError::DependencyFailed);
        EXPECT_EQ(assetManager.GetError(mismatched), hz::AssetError::LoadFailed);
    }
    wrong.location.elementIndex = 0;
    ASSERT_TRUE(catalog.Replace(wrong));
    for (uint32_t phase = 0; phase < 2; ++phase)
    {
        hz::AssetManager assetManager(catalog);
        loaders.RegisterLoaders(assetManager);
        const hz::AssetHandle root = assetManager.Request(scene.GetID());
        if (phase)
        {
            for (uint32_t i = 0; i < 5000; ++i)
            {
                assetManager.Update();
                if (assetManager.GetState(assetManager.Find(wrong.id)) == hz::AssetState::UploadingGPU)
                    break;
                ThreadSleep(1);
            }
            EXPECT_EQ(assetManager.GetState(assetManager.Find(wrong.id)), hz::AssetState::UploadingGPU);
        }
        else
            assetManager.Update();
        assetManager.Release(root);
        assetManager.Shutdown();
        EXPECT_EQ(assetManager.GetCount(), 0u);
    }
}
