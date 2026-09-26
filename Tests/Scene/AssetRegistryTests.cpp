#include <gtest/gtest.h>
#include <filesystem>
#include <vector>

#include "Scene/AssetRegistry.h"
#include "Scene/SceneAssetLoader.h"
#include "Scene/SceneAsset.h"
#include "Scene/TextureAsset.h"
#include "Scene/AssetLoaders.h"
#include "Scene/MeshAsset.h"
#include "Scene/MaterialAsset.h"
#include "Scene/WorldAssetBindings.h"
#include "Resources/IResourceLoader.h"
#include "Graphics/RenderContext.h"
#include "Core/IThread.h"
#include "Core/IContainer.h"
#define IMEMORY_FROM_HEADER
#include "Core/IMemory.h"

static hz::AssetID assetID(uint32_t value) { return { .high = 1, .low = value }; }

class TestAssetLoader
{
public:
    struct Payload
    {
        uint32_t id;
    };
    struct Job
    {
        Payload* pPayload;
        bool     ready;
        bool     failed = false;
    };

    hz::AssetLoader getLoader()
    {
        return { .pUserData = this, .pBegin = begin, .pPoll = poll, .pDestroyRequest = destroyRequest, .pDestroyPayload = destroyPayload };
    }

    hz::AssetLoader getGpuLoader()
    {
        hz::AssetLoader loader = getLoader();
        loader.pUpload = upload;
        loader.pPollUpload = pollUpload;
        loader.pWaitUpload = waitUpload;
        return loader;
    }
    static void upload(void*, const hz::AssetRegistry& registry, hz::Span<hz::AssetHandle> dependencies, void* pUser)
    {
        ++((TestAssetLoader*)pUser)->uploads;
        for (hz::AssetHandle dependency : dependencies)
            EXPECT_EQ(registry.getState(dependency), hz::AssetState::Ready);
    }
    static hz::AssetLoadStatus pollUpload(void*, void* pUser)
    {
        TestAssetLoader& loader = *(TestAssetLoader*)pUser;
        if (!loader.uploadReady)
            return hz::AssetLoadStatus::Pending;
        return loader.uploadFailed ? hz::AssetLoadStatus::Failed : hz::AssetLoadStatus::Succeeded;
    }
    static void waitUpload(void*, void* pUser) { ++((TestAssetLoader*)pUser)->uploadWaits; }

    static void* begin(const hz::AssetCatalogEntry& entry, hz::AssetHandle, void* pUser)
    {
        TestAssetLoader& loader = *(TestAssetLoader*)pUser;
        const uint32_t   id = (uint32_t)entry.id.low;
        ++loader.begun;
        if (loader.failBegin == id)
            return nullptr;
        Payload* pPayload = tf_new(Payload);
        pPayload->id = id;
        Job* pJob = tf_new(Job);
        *pJob = { .pPayload = pPayload, .ready = loader.ready };
        loader.jobs[id] = pJob;
        return pJob;
    }

    static hz::AssetLoadResult poll(void* pRequest, void*)
    {
        Job& job = *(Job*)pRequest;
        if (!job.ready)
            return {};
        const hz::AssetLoadResult result = { .status = job.failed ? hz::AssetLoadStatus::Failed : hz::AssetLoadStatus::Succeeded,
                                             .pPayload = job.pPayload };
        job.pPayload = nullptr;
        return result;
    }

    static void destroyRequest(void* pRequest, void* pUser)
    {
        TestAssetLoader& loader = *(TestAssetLoader*)pUser;
        Job*             pJob = (Job*)pRequest;
        ++loader.requestsDestroyed;
        if (pJob->pPayload)
            destroyPayload(pJob->pPayload, pUser);
        tf_delete(pJob);
    }

    static void destroyPayload(void* pPayload, void* pUser)
    {
        TestAssetLoader& loader = *(TestAssetLoader*)pUser;
        Payload*         pValue = (Payload*)pPayload;
        loader.destroyed.push_back(pValue->id);
        tf_delete(pValue);
    }

    Job*                  jobs[2048] = {};
    std::vector<uint32_t> destroyed;
    uint32_t              begun = 0;
    uint32_t              requestsDestroyed = 0;
    uint32_t              failBegin = 0;
    bool                  ready = true;
    uint32_t              uploads = 0;
    uint32_t              uploadWaits = 0;
    bool                  uploadReady = false;
    bool                  uploadFailed = false;
};

class AssetRegistryTest: public ::testing::Test
{
protected:
    void SetUp() override
    {
        ASSERT_TRUE(initMemAlloc(nullptr));
        initialStats = memGetTrackingStats();
    }
    void TearDown() override
    {
        const MemoryTrackingStats stats = memGetTrackingStats();
        if (stats.trackingEnabled)
        {
            EXPECT_EQ(stats.liveAllocationCount, initialStats.liveAllocationCount);
            EXPECT_EQ(stats.liveRequestedBytes, initialStats.liveRequestedBytes);
        }
        exitMemAlloc();
    }
    static void add(hz::AssetCatalog& catalog, uint32_t id, hz::Span<hz::AssetID> dependencies = {})
    {
        ASSERT_TRUE(catalog.add({ .id = assetID(id),
                                  .pSourcePath = "test.gltf",
                                  .location = { .pPath = "test.sceneasset.json" },
                                  .dependencies = dependencies }));
    }
    MemoryTrackingStats initialStats = {};
};

TEST_F(AssetRegistryTest, DeduplicatesRequestsAndRejectsStaleHandlesAfterReuse)
{
    hz::AssetCatalog catalog;
    add(catalog, 1);
    TestAssetLoader   loader;
    hz::AssetRegistry registry(catalog);
    registry.setLoader(hz::AssetType::Scene, loader.getLoader());
    const hz::AssetHandle first = registry.request(assetID(1));
    ASSERT_TRUE(first.isValid());
    EXPECT_EQ(registry.request(assetID(1)), first);
    EXPECT_EQ(registry.getState(first), hz::AssetState::Queued);
    registry.update();
    EXPECT_EQ(registry.getState(first), hz::AssetState::Ready);
    EXPECT_EQ(loader.begun, 1u);
    EXPECT_TRUE(registry.release(first));
    registry.update();
    EXPECT_EQ(registry.getState(first), hz::AssetState::Ready);
    EXPECT_TRUE(registry.release(first));
    registry.update();
    EXPECT_EQ(registry.getCount(), 0u);
    const hz::AssetHandle second = registry.request(assetID(1));
    EXPECT_EQ(second.index, first.index);
    EXPECT_NE(second.generation, first.generation);
    EXPECT_FALSE(registry.release(first));
    EXPECT_EQ(registry.getState(first), hz::AssetState::Invalid);
    registry.update();
    registry.shutdown();
    EXPECT_EQ(loader.destroyed.size(), 2u);
}

TEST_F(AssetRegistryTest, SharesDependenciesAndDestroysParentsBeforeChildren)
{
    hz::AssetCatalog catalog;
    add(catalog, 1);
    add(catalog, 2, { assetID(1), assetID(1) });
    add(catalog, 3, { assetID(1) });
    EXPECT_EQ(catalog.find(assetID(2))->dependencies.count, 1u);
    TestAssetLoader   loader;
    hz::AssetRegistry registry(catalog);
    registry.setLoader(hz::AssetType::Scene, loader.getLoader());
    const hz::AssetHandle a = registry.request(assetID(2));
    const hz::AssetHandle b = registry.request(assetID(3));
    registry.update();
    EXPECT_EQ(loader.begun, 3u);
    EXPECT_TRUE(registry.release(a));
    registry.update();
    EXPECT_EQ(registry.getCount(), 2u);
    EXPECT_EQ(registry.getState(b), hz::AssetState::Ready);
    EXPECT_EQ(loader.destroyed, (std::vector<uint32_t>{ 2 }));
    registry.shutdown();
    EXPECT_EQ(loader.destroyed, (std::vector<uint32_t>{ 2, 3, 1 }));
}

TEST_F(AssetRegistryTest, RejectsInvalidDependencyGraphsWithoutChangingExistingReferences)
{
    hz::AssetCatalog catalog;
    add(catalog, 1);
    add(catalog, 2, { assetID(1), assetID(99) });
    add(catalog, 3, { assetID(4) });
    add(catalog, 4, { assetID(3) });
    ASSERT_TRUE(
        catalog.add({ .id = assetID(5), .type = hz::AssetType::Texture, .pSourcePath = "test.png", .location = { .pPath = "test.dds" } }));
    TestAssetLoader   loader;
    hz::AssetRegistry registry(catalog);
    registry.setLoader(hz::AssetType::Scene, loader.getLoader());
    const hz::AssetHandle existing = registry.request(assetID(1));
    hz::AssetError        error = hz::AssetError::None;
    EXPECT_FALSE(registry.request(assetID(2), &error).isValid());
    EXPECT_EQ(error, hz::AssetError::UnknownAsset);
    EXPECT_FALSE(registry.request(assetID(3), &error).isValid());
    EXPECT_EQ(error, hz::AssetError::DependencyCycle);
    EXPECT_FALSE(registry.request(assetID(5), &error).isValid());
    EXPECT_EQ(error, hz::AssetError::MissingLoader);
    EXPECT_EQ(registry.getCount(), 1u);
    EXPECT_TRUE(registry.release(existing));
    registry.update();
    EXPECT_EQ(registry.getCount(), 0u);
    EXPECT_EQ(loader.begun, 0u);
}

TEST_F(AssetRegistryTest, CancellationDrainsLateResultsWithoutPublishingOrReusingTheSlot)
{
    hz::AssetCatalog catalog;
    add(catalog, 1);
    TestAssetLoader loader;
    loader.ready = false;
    hz::AssetRegistry registry(catalog);
    registry.setLoader(hz::AssetType::Scene, loader.getLoader());
    const hz::AssetHandle handle = registry.request(assetID(1));
    registry.update();
    ASSERT_EQ(registry.getState(handle), hz::AssetState::LoadingCPU);
    EXPECT_TRUE(registry.release(handle));
    registry.update();
    EXPECT_EQ(registry.getState(handle), hz::AssetState::Retiring);
    EXPECT_EQ(registry.getScene(handle), nullptr);
    hz::AssetError error;
    EXPECT_FALSE(registry.request(assetID(1), &error).isValid());
    EXPECT_EQ(error, hz::AssetError::Retiring);
    EXPECT_EQ(loader.requestsDestroyed, 0u);
    loader.jobs[1]->ready = true;
    registry.update();
    EXPECT_EQ(registry.getState(handle), hz::AssetState::Invalid);
    EXPECT_EQ(loader.requestsDestroyed, 1u);
    EXPECT_EQ(loader.destroyed, (std::vector<uint32_t>{ 1 }));
}

TEST_F(AssetRegistryTest, WaitsForDependenciesBeforePublishingDecodedPayload)
{
    hz::AssetCatalog catalog;
    add(catalog, 1);
    add(catalog, 2, { assetID(1) });
    TestAssetLoader loader;
    loader.ready = false;
    hz::AssetRegistry registry(catalog);
    registry.setLoader(hz::AssetType::Scene, loader.getLoader());
    const hz::AssetHandle handle = registry.request(assetID(2));
    registry.update();
    loader.jobs[2]->ready = true;
    registry.update();
    EXPECT_EQ(registry.getState(handle), hz::AssetState::WaitingDependencies);
    EXPECT_EQ(registry.getScene(handle), nullptr);
    loader.jobs[1]->ready = true;
    registry.update();
    EXPECT_EQ(registry.getState(handle), hz::AssetState::Ready);
    registry.shutdown();
    EXPECT_EQ(loader.destroyed, (std::vector<uint32_t>{ 2, 1 }));
}

TEST_F(AssetRegistryTest, DependencyFailureDrainsPendingParentAndReleasesPartialPayloads)
{
    hz::AssetCatalog catalog;
    add(catalog, 1);
    add(catalog, 2, { assetID(1) });
    TestAssetLoader loader;
    loader.ready = false;
    hz::AssetRegistry registry(catalog);
    registry.setLoader(hz::AssetType::Scene, loader.getLoader());
    const hz::AssetHandle parent = registry.request(assetID(2));
    registry.update();
    loader.jobs[1]->failed = true;
    loader.jobs[1]->ready = true;
    registry.update();
    EXPECT_EQ(registry.getState(parent), hz::AssetState::Failed);
    EXPECT_EQ(registry.getError(parent), hz::AssetError::DependencyFailed);
    EXPECT_EQ(registry.getScene(parent), nullptr);
    EXPECT_EQ(registry.getCount(), 2u);
    loader.jobs[2]->ready = true;
    registry.update();
    EXPECT_EQ(registry.getCount(), 1u);
    EXPECT_EQ(loader.destroyed, (std::vector<uint32_t>{ 1, 2 }));
    EXPECT_TRUE(registry.release(parent));
    registry.update();
    EXPECT_EQ(registry.getCount(), 0u);
    loader.failBegin = 1;
    const hz::AssetHandle failed = registry.request(assetID(1));
    registry.update();
    EXPECT_EQ(registry.getState(failed), hz::AssetState::Failed);
    EXPECT_EQ(registry.getError(failed), hz::AssetError::LoadFailed);
}

TEST_F(AssetRegistryTest, ShutdownDrainsPendingJobsAndIsIdempotent)
{
    hz::AssetCatalog catalog;
    add(catalog, 1);
    add(catalog, 2, { assetID(1) });
    TestAssetLoader loader;
    loader.ready = false;
    hz::AssetRegistry registry(catalog);
    registry.setLoader(hz::AssetType::Scene, loader.getLoader());
    registry.request(assetID(2));
    registry.update();
    registry.shutdown();
    EXPECT_EQ(loader.requestsDestroyed, 2u);
    EXPECT_EQ(loader.destroyed.size(), 2u);
    EXPECT_EQ(registry.getCount(), 0u);
    hz::AssetError error;
    EXPECT_FALSE(registry.request(assetID(1), &error).isValid());
    EXPECT_EQ(error, hz::AssetError::ShuttingDown);
    registry.shutdown();
    EXPECT_EQ(loader.requestsDestroyed, 2u);
}

TEST_F(AssetRegistryTest, TraversesAndRetiresDeepDependenciesIteratively)
{
    hz::AssetCatalog catalog;
    add(catalog, 1);
    for (uint32_t i = 2; i <= 1024; ++i)
        add(catalog, i, { assetID(i - 1) });
    TestAssetLoader   loader;
    hz::AssetRegistry registry(catalog);
    registry.setLoader(hz::AssetType::Scene, loader.getLoader());
    const hz::AssetHandle handle = registry.request(assetID(1024));
    registry.update();
    EXPECT_EQ(registry.getState(handle), hz::AssetState::Ready);
    EXPECT_TRUE(registry.release(handle));
    registry.update();
    EXPECT_EQ(registry.getCount(), 0u);
    ASSERT_EQ(loader.destroyed.size(), 1024u);
    EXPECT_EQ(loader.destroyed.front(), 1024u);
    EXPECT_EQ(loader.destroyed.back(), 1u);
}

TEST_F(AssetRegistryTest, UploadWaitsForDependenciesAndCancellationDrainsBeforeDestruction)
{
    hz::AssetCatalog catalog;
    add(catalog, 1);
    add(catalog, 2, { assetID(1) });
    TestAssetLoader   loader;
    hz::AssetRegistry registry(catalog);
    registry.setLoader(hz::AssetType::Scene, loader.getGpuLoader());
    const hz::AssetHandle parent = registry.request(assetID(2));
    registry.update();
    EXPECT_EQ(loader.uploads, 1u);
    EXPECT_EQ(registry.getState(parent), hz::AssetState::WaitingDependencies);
    const hz::AssetHandle child = registry.find(assetID(1));
    EXPECT_EQ(registry.getState(child), hz::AssetState::UploadingGPU);
    registry.release(parent);
    registry.update();
    EXPECT_EQ(registry.getState(parent), hz::AssetState::Invalid);
    EXPECT_EQ(registry.getState(child), hz::AssetState::Retiring);
    EXPECT_EQ(loader.destroyed, (std::vector<uint32_t>{ 2 }));
    loader.uploadReady = true;
    registry.update();
    EXPECT_EQ(registry.getCount(), 0u);
    EXPECT_EQ(loader.destroyed, (std::vector<uint32_t>{ 2, 1 }));
    EXPECT_EQ(loader.uploadWaits, 0u);
}

TEST_F(AssetRegistryTest, UploadFailureAndShutdownUseTheSamePayloadCleanup)
{
    hz::AssetCatalog catalog;
    add(catalog, 1);
    TestAssetLoader   loader;
    hz::AssetRegistry registry(catalog);
    registry.setLoader(hz::AssetType::Scene, loader.getGpuLoader());
    const hz::AssetHandle handle = registry.request(assetID(1));
    registry.update();
    loader.uploadReady = true;
    loader.uploadFailed = true;
    registry.update();
    EXPECT_EQ(registry.getState(handle), hz::AssetState::Failed);
    EXPECT_EQ(registry.getError(handle), hz::AssetError::UploadFailed);
    EXPECT_EQ(loader.destroyed, (std::vector<uint32_t>{ 1 }));
    registry.release(handle);
    registry.update();
    loader.uploadReady = false;
    registry.request(assetID(1));
    registry.update();
    registry.shutdown();
    EXPECT_EQ(loader.uploadWaits, 1u);
    EXPECT_EQ(loader.destroyed, (std::vector<uint32_t>{ 1, 1 }));
}

TEST_F(AssetRegistryTest, SnapshotsPinPayloadsAndKeepTheirOriginalMembership)
{
    hz::AssetCatalog catalog;
    add(catalog, 1);
    add(catalog, 2);
    TestAssetLoader   loader;
    hz::AssetRegistry registry(catalog);
    registry.setLoader(hz::AssetType::Scene, loader.getLoader());
    const hz::AssetHandle first = registry.request(assetID(1));
    registry.update();
    hz::AssetSnapshot     old = registry.captureSnapshot();
    const hz::SceneAsset* pFirst = old.getScene(first);
    ASSERT_NE(pFirst, nullptr);
    const MemoryTrackingStats before = memGetTrackingStats();
    for (uint32_t i = 0; i < 100; ++i)
    {
        hz::AssetSnapshot frame = registry.captureSnapshot();
        registry.update();
    }
    const MemoryTrackingStats after = memGetTrackingStats();
    if (after.trackingEnabled)
    {
        EXPECT_EQ(after.totalAllocationCount, before.totalAllocationCount);
        EXPECT_EQ(after.reallocationCount, before.reallocationCount);
    }
    registry.release(first);
    registry.update();
    EXPECT_EQ(registry.getState(first), hz::AssetState::Retiring);
    EXPECT_EQ(registry.getScene(first), nullptr);
    EXPECT_EQ(old.getScene(first), pFirst);
    const hz::AssetHandle second = registry.request(assetID(2));
    registry.update();
    hz::AssetSnapshot current = registry.captureSnapshot();
    EXPECT_EQ(current.getScene(first), nullptr);
    EXPECT_EQ(old.getScene(second), nullptr);
    EXPECT_NE(current.getScene(second), nullptr);
    old = {};
    registry.update();
    EXPECT_EQ(registry.getState(first), hz::AssetState::Invalid);
    EXPECT_EQ(loader.destroyed, (std::vector<uint32_t>{ 1 }));
    current = {};
    registry.shutdown();
    EXPECT_EQ(loader.destroyed, (std::vector<uint32_t>{ 1, 2 }));
}

static const char kEmptyScene[] = R"json({
    "format":"Horizon.SceneAsset", "version":1,
    "id":"00000000-0000-4000-8000-000000000001",
    "source":"scene.gltf", "contentHash":"fnv1a64:0123456789abcdef", "identityHash":"0123456789abcdef",
    "nodes":[], "meshes":[], "materials":[], "textures":[]
})json";

class SceneAssetLoaderTest: public AssetRegistryTest
{
protected:
    void SetUp() override
    {
        AssetRegistryTest::SetUp();
        FileSystemInitDesc desc = { .pAppName = "SceneAssetLoaderTest" };
        ASSERT_TRUE(initFileSystem(&desc));
        char id[hz::kIDStringCapacity];
        hz::AssetID::create().toString(id);
        directory = std::filesystem::temp_directory_path() / id;
        std::filesystem::create_directory(directory);
        fileSystem = *pSystemFileIO;
        fileSystem.GetResourceMount = nullptr;
        fsSetPathForResourceDir(&fileSystem, RM_PROJECT, RD_OTHER_FILES, directory.string().c_str());
        FileStream stream = {};
        ASSERT_TRUE(fsOpenStreamFromPath(RD_OTHER_FILES, "scene.sceneasset.json", FM_WRITE, &stream));
        EXPECT_EQ(fsWriteToStream(&stream, kEmptyScene, sizeof(kEmptyScene) - 1), sizeof(kEmptyScene) - 1);
        fsCloseStream(&stream);
    }
    void TearDown() override
    {
        exitFileSystem();
        std::filesystem::remove_all(directory);
        AssetRegistryTest::TearDown();
    }
    static void wait(hz::AssetRegistry& registry, hz::AssetHandle handle)
    {
        for (uint32_t i = 0; i < 5000; ++i)
        {
            registry.update();
            if (registry.getState(handle) == hz::AssetState::Ready || registry.getState(handle) == hz::AssetState::Failed)
                return;
            threadSleep(1);
        }
        FAIL() << "SceneAsset load timed out";
    }
    std::filesystem::path directory;
    IFileSystem           fileSystem = {};
};

TEST_F(SceneAssetLoaderTest, DecodesSceneAsynchronouslyAndPublishesTypedPayload)
{
    hz::SceneAsset scene;
    ASSERT_TRUE(scene.parse(kEmptyScene));
    hz::AssetCatalog catalog;
    ASSERT_TRUE(catalog.addScene(scene, "scene.sceneasset.json"));
    hz::Array<char> index;
    ASSERT_TRUE(catalog.serialize(index));
    FileStream stream = {};
    ASSERT_TRUE(fsOpenStreamFromPath(RD_OTHER_FILES, "scene.assetcatalog.json", FM_WRITE, &stream));
    EXPECT_EQ(fsWriteToStream(&stream, index.data(), index.size() - 1), index.size() - 1);
    fsCloseStream(&stream);
    catalog.clear();
    ASSERT_TRUE(catalog.load(RD_OTHER_FILES, "scene.assetcatalog.json"));
    hz::SceneAssetLoader loader(RD_OTHER_FILES);
    hz::AssetRegistry    registry(catalog);
    registry.setLoader(hz::AssetType::Scene, loader.getLoader());
    const hz::AssetHandle handle = registry.request(scene.getID());
    EXPECT_EQ(registry.getScene(handle), nullptr);
    wait(registry, handle);
    ASSERT_EQ(registry.getState(handle), hz::AssetState::Ready);
    ASSERT_NE(registry.getScene(handle), nullptr);
    EXPECT_EQ(registry.getScene(handle)->getID(), scene.getID());
    registry.release(handle);
    registry.update();
    EXPECT_EQ(registry.getScene(handle), nullptr);
}

TEST_F(SceneAssetLoaderTest, RejectsMissingFileAndMismatchedIdentityAndDrainsOnShutdown)
{
    hz::AssetCatalog catalog;
    ASSERT_TRUE(catalog.add({ .id = assetID(1), .pSourcePath = "scene.gltf", .location = { .pPath = "missing.sceneasset.json" } }));
    ASSERT_TRUE(catalog.add({ .id = assetID(2), .pSourcePath = "scene.gltf", .location = { .pPath = "scene.sceneasset.json" } }));
    hz::SceneAssetLoader loader(RD_OTHER_FILES);
    hz::AssetRegistry    registry(catalog);
    registry.setLoader(hz::AssetType::Scene, loader.getLoader());
    const hz::AssetHandle missing = registry.request(assetID(1));
    const hz::AssetHandle mismatched = registry.request(assetID(2));
    wait(registry, missing);
    wait(registry, mismatched);
    EXPECT_EQ(registry.getError(missing), hz::AssetError::LoadFailed);
    EXPECT_EQ(registry.getError(mismatched), hz::AssetError::LoadFailed);
    registry.release(mismatched);
    registry.update();
    registry.request(assetID(2));
    registry.update();
    registry.shutdown();
    EXPECT_EQ(registry.getCount(), 0u);
}

TEST_F(SceneAssetLoaderTest, RejectsPayloadWithDifferentDependenciesFromCatalog)
{
    hz::SceneAsset scene;
    ASSERT_TRUE(scene.parse(kEmptyScene));
    hz::AssetCatalog catalog;
    ASSERT_TRUE(catalog.add({ .id = scene.getID(),
                              .pSourcePath = "scene.gltf",
                              .location = { .pPath = "scene.sceneasset.json" },
                              .dependencies = { assetID(1) } }));
    ASSERT_TRUE(catalog.add({ .id = assetID(1),
                              .type = hz::AssetType::Mesh,
                              .pSourcePath = "scene.gltf",
                              .location = { .pPath = "scene.sceneasset.json", .elementIndex = 0 } }));
    hz::SceneAssetLoader loader(RD_OTHER_FILES);
    TestAssetLoader      meshLoader;
    hz::AssetRegistry    registry(catalog);
    registry.setLoader(hz::AssetType::Scene, loader.getLoader());
    registry.setLoader(hz::AssetType::Mesh, meshLoader.getLoader());
    const hz::AssetHandle handle = registry.request(scene.getID());
    wait(registry, handle);
    EXPECT_EQ(registry.getState(handle), hz::AssetState::Failed);
    EXPECT_EQ(registry.getScene(handle), nullptr);
    EXPECT_EQ(meshLoader.destroyed, (std::vector<uint32_t>{ 1 }));
}

class TextureAssetLiveTest: public SceneAssetLoaderTest
{
protected:
    void writeTexture()
    {
        uint32_t dds[33] = {};
        dds[0] = 0x20534444;
        dds[1] = 124;
        dds[2] = 0x100f;
        dds[3] = dds[4] = 1;
        dds[5] = 4;
        dds[19] = 32;
        dds[20] = 0x41;
        dds[22] = 32;
        dds[23] = 0xff;
        dds[24] = 0xff00;
        dds[25] = 0xff0000;
        dds[26] = 0xff000000;
        dds[27] = 0x1000;
        dds[32] = 0xff4080ff;
        FileStream stream = {};
        ASSERT_TRUE(fsOpenStreamFromPath(RD_OTHER_FILES, "pixel.dds", FM_WRITE, &stream));
        EXPECT_EQ(fsWriteToStream(&stream, dds, sizeof(dds)), sizeof(dds));
        fsCloseStream(&stream);
    }
};

TEST_F(TextureAssetLiveTest, UploadsSamplesAndRetiresTextureAfterSubmission)
{
    writeTexture();
    hz::RenderContext context({ .pAppName = "TextureAssetTest", .enableGpuValidation = true });
    hz::AssetCatalog  catalog;
    ASSERT_TRUE(catalog.add(
        { .id = assetID(1), .type = hz::AssetType::Texture, .pSourcePath = "pixel.png", .location = { .pPath = "pixel.dds" } }));
    hz::TextureAssetLoader loader(context, RD_OTHER_FILES);
    hz::AssetRegistry      registry(catalog);
    registry.setLoader(hz::AssetType::Texture, loader.getLoader());
    const hz::AssetHandle handle = registry.request(assetID(1));
    EXPECT_EQ(registry.request(assetID(1)), handle);
    registry.release(handle);
    wait(registry, handle);
    ASSERT_EQ(registry.getState(handle), hz::AssetState::Ready);
    hz::AssetSnapshot     snapshot = registry.captureSnapshot();
    const hz::GPUTexture& texture = snapshot.getTexture(handle)->getTexture();
    ASSERT_TRUE(texture.isValid());
    const char      shader[] = R"hlsl(
        cbuffer RootConstant0 { uint textureIndex; uint outputIndex; };
        [numthreads(1,1,1)] void CSMain() {
            Texture2D<float4> source = ResourceDescriptorHeap[textureIndex];
            RWStructuredBuffer<uint4> output = ResourceDescriptorHeap[outputIndex];
            output[0] = uint4(round(source.Load(int3(0,0,0)) * 255.0));
        }
    )hlsl";
    hz::GPUPipeline pipeline = context.createComputePipeline({ .shaderDesc = { .stages = { { .stage = SHADER_STAGE_COMP,
                                                                                             .pSource = shader,
                                                                                             .sourceSize = sizeof(shader) - 1,
                                                                                             .pEntryPoint = "CSMain" } } },
                                                               .pName = "TextureAsset.Readback" });
    ASSERT_TRUE(pipeline.isValid());
    hz::GPUBuffer    output = context.createBuffer({ .size = 16,
                                                     .elementCount = 1,
                                                     .structStride = 16,
                                                     .usage = RESOURCE_MEMORY_USAGE_GPU_ONLY,
                                                     .startState = RESOURCE_STATE_UNORDERED_ACCESS,
                                                     .descriptors = DESCRIPTOR_TYPE_RW_BUFFER });
    hz::GPUBuffer    readback = context.createBuffer({ .size = 16,
                                                       .usage = RESOURCE_MEMORY_USAGE_GPU_TO_CPU,
                                                       .startState = RESOURCE_STATE_COPY_DEST,
                                                       .flags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT });
    hz::CommandList& commands = context.acquireCommandList();
    commands.setPipeline(pipeline);
    commands.setRootConstant({ texture.getSrvIndex(), output.getUavIndex() });
    commands.dispatch(1, 1, 1, { .sampledTextures = { &texture }, .storageBuffers = { &output } });
    commands.copyBuffer(readback, 0, output, 0, 16);
    const hz::SubmitHandle submission = context.submit(commands);
    registry.release(handle);
    registry.retireSnapshot(std::move(snapshot), context, submission);
    EXPECT_FALSE(snapshot.isValid());
    EXPECT_EQ(registry.getState(handle), hz::AssetState::Retiring);
    registry.shutdown();
    EXPECT_TRUE(context.isComplete(submission));
    EXPECT_EQ(registry.getCount(), 0u);
    const uint32_t* pPixel = (const uint32_t*)readback.get()->pCpuMappedAddress;
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
    ASSERT_TRUE(fsOpenStreamFromPath(RD_OTHER_FILES, "broken.dds", FM_WRITE, &broken));
    const uint32_t invalidHeader = 0;
    fsWriteToStream(&broken, &invalidHeader, sizeof(invalidHeader));
    fsCloseStream(&broken);
    hz::RenderContext context({ .pAppName = "TextureAssetCancelTest" });
    hz::AssetCatalog  catalog;
    ASSERT_TRUE(catalog.add(
        { .id = assetID(1), .type = hz::AssetType::Texture, .pSourcePath = "pixel.png", .location = { .pPath = "pixel.dds" } }));
    ASSERT_TRUE(catalog.add(
        { .id = assetID(2), .type = hz::AssetType::Texture, .pSourcePath = "pixel.png", .location = { .pPath = "pixel.png" } }));
    ASSERT_TRUE(catalog.add(
        { .id = assetID(3), .type = hz::AssetType::Texture, .pSourcePath = "pixel.png", .location = { .pPath = "missing.dds" } }));
    ASSERT_TRUE(catalog.add(
        { .id = assetID(4), .type = hz::AssetType::Texture, .pSourcePath = "pixel.png", .location = { .pPath = "truncated.dds" } }));
    ASSERT_TRUE(catalog.add(
        { .id = assetID(5), .type = hz::AssetType::Texture, .pSourcePath = "pixel.png", .location = { .pPath = "broken.dds" } }));
    hz::TextureAssetLoader loader(context, RD_OTHER_FILES);
    hz::AssetRegistry      registry(catalog);
    registry.setLoader(hz::AssetType::Texture, loader.getLoader());
    const hz::AssetHandle canceled = registry.request(assetID(1));
    registry.update();
    registry.release(canceled);
    for (uint32_t i = 2; i <= 5; ++i)
    {
        const hz::AssetHandle invalid = registry.request(assetID(i));
        wait(registry, invalid);
        EXPECT_EQ(registry.getError(invalid), hz::AssetError::UploadFailed);
        EXPECT_EQ(registry.getTexture(invalid), nullptr);
    }
    registry.shutdown();
    EXPECT_EQ(registry.getCount(), 0u);
}

static const char kTypedScene[] = R"json({
    "format":"Horizon.SceneAsset", "version":1,
    "id":"00000000-0000-4000-8000-000000000001",
    "source":"scene.gltf", "contentHash":"fnv1a64:0123456789abcdef", "identityHash":"0123456789abcdef",
    "textures":[{"id":"00000000-0000-4000-8000-000000000002", "path":"pixel.dds", "source":"pixel.png", "srgb":false, "cooked":true}],
    "materials":[{"id":"00000000-0000-4000-8000-000000000003", "name":"Shared",
        "baseColorTexture":"00000000-0000-4000-8000-000000000002", "normalTexture":null,
        "metallicRoughnessTexture":null, "emissiveTexture":null,
        "baseColorFactor":[1,0.5,0.25,1], "metallicFactor":0.2, "roughnessFactor":0.8,
        "emissiveFactor":[0,0,0], "alphaMode":1, "alphaCutoff":0.5, "doubleSided":true}],
    "meshes":[
        {"id":"00000000-0000-4000-8000-000000000004", "name":"First", "geometry":"scene.bin",
         "submeshes":[{"id":"8000000000000001", "draw":0, "material":"00000000-0000-4000-8000-000000000003"}]},
        {"id":"00000000-0000-4000-8000-000000000005", "name":"Second", "geometry":"scene.bin",
         "submeshes":[{"id":"8000000000000002", "draw":0, "material":"00000000-0000-4000-8000-000000000003"}]}
    ],
    "nodes":[
        {"name":"First", "parent":-1, "mesh":"00000000-0000-4000-8000-000000000004",
         "translation":[0,0,0], "rotation":[0,0,0,1], "scale":[1,1,1]},
        {"name":"Second", "parent":-1, "mesh":"00000000-0000-4000-8000-000000000005",
         "translation":[1,0,0], "rotation":[0,0,0,1], "scale":[1,1,1]}
    ]
})json";

static const VertexLayout kAssetTestLayout = {
    .attribs = { { .semantic = SEMANTIC_POSITION, .format = hz::Format::R32G32B32_SFLOAT, .binding = 2 } },
    .attribCount = 1,
};

class TypedAssetLiveTest: public TextureAssetLiveTest
{
protected:
    void writeScene(const char* text = kTypedScene, const char* path = "scene.sceneasset.json")
    {
        FileStream stream = {};
        ASSERT_TRUE(fsOpenStreamFromPath(RD_OTHER_FILES, path, FM_WRITE, &stream));
        EXPECT_EQ(fsWriteToStream(&stream, text, strlen(text)), strlen(text));
        fsCloseStream(&stream);
    }
    void writeGeometry(IndexType indexType = INDEX_TYPE_UINT16)
    {
        Geometry geometry = {};
        geometry.drawArgCount = 1;
        geometry.indexCount = 3;
        geometry.vertexCount = 3;
        geometry.indexType = indexType;
        const IndirectDrawIndexArguments draw = { .indexCount = 3, .instanceCount = 1 };
        const GeometryData               data = {};
        GeometryData::ShadowData         shadow = {};
        shadow.vertexStrides[SEMANTIC_POSITION] = 12;
        shadow.attributeCount[SEMANTIC_POSITION] = 3;
        const uint16_t indices[] = { 0, 1, 2 };
        const float    positions[] = { -1, 2, 3, 4, -5, 6, 0, 1, -2 };
        const uint32_t geometrySize = sizeof(geometry) + sizeof(draw);
        const uint32_t dataSize = sizeof(data);
        const uint32_t shadowSize = sizeof(shadow) + sizeof(indices) + sizeof(positions);
        FileStream     stream = {};
        ASSERT_TRUE(fsOpenStreamFromPath(RD_OTHER_FILES, "scene.bin", FM_WRITE, &stream));
        fsWriteToStream(&stream, GEOMETRY_FILE_MAGIC_STR, sizeof(GEOMETRY_FILE_MAGIC_STR));
        fsWriteToStream(&stream, &geometrySize, sizeof(geometrySize));
        fsWriteToStream(&stream, &geometry, sizeof(geometry));
        fsWriteToStream(&stream, &draw, sizeof(draw));
        fsWriteToStream(&stream, &dataSize, sizeof(dataSize));
        fsWriteToStream(&stream, &data, sizeof(data));
        fsWriteToStream(&stream, &shadowSize, sizeof(shadowSize));
        fsWriteToStream(&stream, &shadow, sizeof(shadow));
        fsWriteToStream(&stream, indices, sizeof(indices));
        fsWriteToStream(&stream, positions, sizeof(positions));
        fsCloseStream(&stream);
    }
};

TEST_F(TypedAssetLiveTest, LoadsSharedDependencyGraphAndRetiresGeometryAfterGpuUse)
{
    writeTexture();
    writeScene();
    writeGeometry();
    hz::SceneAsset scene;
    ASSERT_TRUE(scene.parse(kTypedScene));
    hz::AssetCatalog catalog;
    ASSERT_TRUE(catalog.addScene(scene, "scene.sceneasset.json"));
    hz::RenderContext context({ .pAppName = "TypedAssetTest", .enableGpuValidation = true });
    hz::AssetLoaders  loaders(context, { .vertexLayout = kAssetTestLayout, .cookedDirectory = RD_OTHER_FILES });
    hz::AssetRegistry registry(catalog);
    loaders.registerLoaders(registry);
    const hz::AssetHandle root = registry.request(scene.getID());
    const hz::AssetHandle second = registry.request(scene.getMeshes().pData[1].id);
    wait(registry, root);
    ASSERT_EQ(registry.getState(root), hz::AssetState::Ready);
    EXPECT_EQ(registry.getCount(), 5u);
    const hz::AssetHandle first = registry.find(scene.getMeshes().pData[0].id);
    const hz::MeshAsset*  pFirst = registry.getMesh(first);
    const hz::MeshAsset*  pSecond = registry.getMesh(second);
    ASSERT_NE(pFirst, nullptr);
    ASSERT_NE(pSecond, nullptr);
    EXPECT_EQ(&pFirst->getIndexBuffer(), &pSecond->getIndexBuffer());
    EXPECT_EQ(&pFirst->getVertexBuffer(0), &pSecond->getVertexBuffer(0));
    EXPECT_EQ(pFirst->getVertexBufferCount(), 1u);
    EXPECT_EQ(pFirst->getVertexStride(0), 12u);
    EXPECT_EQ(pFirst->getIndexType(), INDEX_TYPE_UINT16);
    EXPECT_EQ(pFirst->getDraw(0).indexCount, 3u);
    const hz::MeshBounds& bounds = pFirst->getBounds(0);
    EXPECT_FLOAT_EQ(bounds.minimum[0], -1);
    EXPECT_FLOAT_EQ(bounds.minimum[1], -5);
    EXPECT_FLOAT_EQ(bounds.minimum[2], -2);
    EXPECT_FLOAT_EQ(bounds.maximum[0], 4);
    EXPECT_FLOAT_EQ(bounds.maximum[1], 2);
    EXPECT_FLOAT_EQ(bounds.maximum[2], 6);
    const hz::MaterialAsset* pMaterial = registry.getMaterial(registry.find(scene.getMaterials().pData[0].id));
    ASSERT_NE(pMaterial, nullptr);
    EXPECT_FLOAT_EQ(pMaterial->getDescription().roughnessFactor, 0.8f);
    ASSERT_NE(registry.getTexture(registry.find(pMaterial->getDescription().baseColorTexture)), nullptr);
    hz::AssetSnapshot snapshot = registry.captureSnapshot();
    hz::GPUBuffer     readback = context.createBuffer({ .size = 36,
                                                        .usage = RESOURCE_MEMORY_USAGE_GPU_TO_CPU,
                                                        .startState = RESOURCE_STATE_COPY_DEST,
                                                        .flags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT });
    hz::CommandList&  commands = context.acquireCommandList();
    commands.copyBuffer(readback, 0, pFirst->getVertexBuffer(0), 0, 36);
    const hz::SubmitHandle submission = context.submit(commands);
    registry.release(root);
    registry.release(second);
    EXPECT_EQ(snapshot.getMesh(first), pFirst);
    registry.retireSnapshot(std::move(snapshot), context, submission);
    registry.shutdown();
    EXPECT_EQ(registry.getCount(), 0u);
    EXPECT_TRUE(context.isComplete(submission));
    const float* pVertices = (const float*)readback.get()->pCpuMappedAddress;
    EXPECT_FLOAT_EQ(pVertices[0], -1);
    EXPECT_FLOAT_EQ(pVertices[4], -5);
    EXPECT_FLOAT_EQ(pVertices[8], -2);
}

TEST_F(AssetRegistryTest, WorldBindingsRetainSharedMeshesAndReleaseRemovedComponents)
{
    hz::AssetCatalog catalog;
    ASSERT_TRUE(
        catalog.add({ .id = assetID(1), .type = hz::AssetType::Mesh, .pSourcePath = "scene.gltf", .location = { .pPath = "scene.bin" } }));
    TestAssetLoader loader;
    loader.ready = false;
    hz::AssetRegistry registry(catalog);
    registry.setLoader(hz::AssetType::Mesh, loader.getLoader());
    hz::TypeRegistry types;
    ASSERT_TRUE(types.registerBuiltins());
    hz::World        world(types);
    const hz::Entity a = world.createEntity(), b = world.createEntity();
    hz::MeshRenderer renderer = { .mesh = assetID(1) };
    ASSERT_TRUE(world.setComponent(a, hz::SceneTypes::meshRenderer, &renderer));
    ASSERT_TRUE(world.setComponent(b, hz::SceneTypes::meshRenderer, &renderer));
    hz::WorldAssetBindings bindings(world, registry);
    bindings.update();
    ASSERT_EQ(bindings.getCount(), 2u);
    const hz::AssetHandle mesh = bindings.get(a)->mesh;
    EXPECT_EQ(bindings.get(b)->mesh, mesh);
    registry.update();
    bindings.update();
    EXPECT_EQ(bindings.get(a)->state, hz::AssetBindingState::Pending);
    EXPECT_EQ(loader.begun, 1u);
    const MemoryTrackingStats before = memGetTrackingStats();
    for (uint32_t i = 0; i < 100; ++i)
        bindings.update();
    const MemoryTrackingStats after = memGetTrackingStats();
    if (before.trackingEnabled)
    {
        EXPECT_EQ(before.totalAllocationCount, after.totalAllocationCount);
        EXPECT_EQ(before.reallocationCount, after.reallocationCount);
    }
    ASSERT_TRUE(world.removeComponent(a, hz::SceneTypes::meshRenderer));
    bindings.update();
    EXPECT_EQ(bindings.get(a), nullptr);
    EXPECT_EQ(bindings.getCount(), 1u);
    EXPECT_EQ(registry.getState(mesh), hz::AssetState::LoadingCPU);
    ASSERT_TRUE(world.destroyEntity(b));
    bindings.update();
    EXPECT_EQ(bindings.getCount(), 0u);
    EXPECT_EQ(registry.getState(mesh), hz::AssetState::Retiring);
    loader.jobs[1]->ready = true;
    registry.update();
    EXPECT_EQ(registry.getCount(), 0u);
    EXPECT_EQ(loader.destroyed, (std::vector<uint32_t>{ 1 }));
}

TEST_F(AssetRegistryTest, WorldBindingsSwapMeshesWithoutRetiringSharedAssets)
{
    hz::AssetCatalog catalog;
    for (uint32_t i = 1; i <= 2; ++i)
        ASSERT_TRUE(catalog.add(
            { .id = assetID(i), .type = hz::AssetType::Mesh, .pSourcePath = "scene.gltf", .location = { .pPath = "scene.bin" } }));
    TestAssetLoader loader;
    loader.ready = false;
    hz::AssetRegistry registry(catalog);
    registry.setLoader(hz::AssetType::Mesh, loader.getLoader());
    hz::TypeRegistry types;
    ASSERT_TRUE(types.registerBuiltins());
    hz::World        world(types);
    const hz::Entity a = world.createEntity(), b = world.createEntity();
    hz::MeshRenderer first = { .mesh = assetID(1) }, second = { .mesh = assetID(2) };
    ASSERT_TRUE(world.setComponent(a, hz::SceneTypes::meshRenderer, &first));
    ASSERT_TRUE(world.setComponent(b, hz::SceneTypes::meshRenderer, &second));
    hz::WorldAssetBindings bindings(world, registry);
    bindings.update();
    const hz::AssetHandle meshA = bindings.get(a)->mesh, meshB = bindings.get(b)->mesh;
    registry.update();
    ASSERT_TRUE(world.setComponent(a, hz::SceneTypes::meshRenderer, &second));
    ASSERT_TRUE(world.setComponent(b, hz::SceneTypes::meshRenderer, &first));
    bindings.update();
    EXPECT_EQ(bindings.get(a)->mesh, meshB);
    EXPECT_EQ(bindings.get(b)->mesh, meshA);
    EXPECT_EQ(registry.getState(meshA), hz::AssetState::LoadingCPU);
    EXPECT_EQ(registry.getState(meshB), hz::AssetState::LoadingCPU);
    EXPECT_EQ(loader.begun, 2u);

    ASSERT_TRUE(world.destroyEntity(a));
    bindings.update();
    const hz::Entity replacement = world.createEntity();
    EXPECT_NE(replacement, a);
    ASSERT_TRUE(world.setComponent(replacement, hz::SceneTypes::meshRenderer, &second));
    bindings.update();
    EXPECT_EQ(bindings.get(a), nullptr);
    EXPECT_EQ(bindings.get(replacement)->state, hz::AssetBindingState::Pending);
    EXPECT_FALSE(bindings.get(replacement)->mesh.isValid());
    loader.jobs[2]->ready = true;
    registry.update();
    bindings.update();
    EXPECT_TRUE(bindings.get(replacement)->mesh.isValid());
    EXPECT_NE(bindings.get(replacement)->mesh, meshB);
    bindings.clear();
    EXPECT_EQ(bindings.getCount(), 0u);
    EXPECT_EQ(registry.getState(meshA), hz::AssetState::Retiring);
}

TEST_F(AssetRegistryTest, WorldBindingsKeepFailedRequestsIdleUntilRebuilt)
{
    hz::AssetCatalog catalog;
    TestAssetLoader  loader;
    loader.ready = false;
    hz::AssetRegistry registry(catalog);
    registry.setLoader(hz::AssetType::Mesh, loader.getLoader());
    hz::TypeRegistry types;
    ASSERT_TRUE(types.registerBuiltins());
    hz::World              world(types);
    const hz::Entity       entity = world.createEntity();
    const hz::MeshRenderer renderer = { .mesh = assetID(1) };
    ASSERT_TRUE(world.setComponent(entity, hz::SceneTypes::meshRenderer, &renderer));
    hz::WorldAssetBindings bindings(world, registry);
    bindings.update();
    ASSERT_EQ(bindings.get(entity)->state, hz::AssetBindingState::Failed);
    ASSERT_TRUE(
        catalog.add({ .id = assetID(1), .type = hz::AssetType::Mesh, .pSourcePath = "scene.gltf", .location = { .pPath = "scene.bin" } }));
    const MemoryTrackingStats before = memGetTrackingStats();
    for (uint32_t i = 0; i < 100; ++i)
        bindings.update();
    const MemoryTrackingStats after = memGetTrackingStats();
    if (before.trackingEnabled)
    {
        EXPECT_EQ(before.totalAllocationCount, after.totalAllocationCount);
        EXPECT_EQ(before.reallocationCount, after.reallocationCount);
    }
    EXPECT_EQ(registry.getCount(), 0u);
    bindings.clear();
    bindings.update();
    EXPECT_EQ(bindings.get(entity)->state, hz::AssetBindingState::Pending);
    EXPECT_TRUE(bindings.get(entity)->mesh.isValid());
    EXPECT_EQ(registry.getCount(), 1u);
}

TEST_F(TypedAssetLiveTest, WorldBindingsResolveSubmeshMaterialsAndKeepSnapshotsAlive)
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
    ASSERT_TRUE(scene.parse({ sceneText.data(), (uint32_t)sceneText.size() }));
    ASSERT_TRUE(overrideScene.parse({ overrideText.data(), (uint32_t)overrideText.size() }));
    hz::AssetCatalog catalog;
    ASSERT_TRUE(catalog.addScene(scene, "scene.sceneasset.json"));
    const hz::AssetID alternate = overrideScene.getMaterials().pData[0].id;
    ASSERT_TRUE(catalog.add({ .id = alternate,
                              .type = hz::AssetType::Material,
                              .pSourcePath = "scene.gltf",
                              .location = { .pPath = "override.sceneasset.json", .elementIndex = 0 },
                              .dependencies = { scene.getTextures().pData[0].id } }));
    hz::RenderContext context({ .pAppName = "WorldAssetBindingsTest", .enableGpuValidation = true });
    hz::AssetLoaders  loaders(context, { .vertexLayout = kAssetTestLayout, .cookedDirectory = RD_OTHER_FILES });
    hz::AssetRegistry registry(catalog);
    loaders.registerLoaders(registry);
    hz::TypeRegistry types;
    ASSERT_TRUE(types.registerBuiltins());
    hz::World        world(types);
    const hz::Entity a = world.createEntity(), b = world.createEntity();
    hz::MeshRenderer renderer = { .mesh = scene.getMeshes().pData[0].id };
    ASSERT_TRUE(world.setComponent(b, hz::SceneTypes::meshRenderer, &renderer));
    renderer.overrides.pushBack({ .submesh = UINT64_C(0x8000000000000003), .material = alternate });
    ASSERT_TRUE(world.setComponent(a, hz::SceneTypes::meshRenderer, &renderer));
    hz::WorldAssetBindings bindings(world, registry);
    bindings.update();
    const hz::AssetHandle mesh = bindings.get(a)->mesh;
    EXPECT_EQ(bindings.get(a)->state, hz::AssetBindingState::Pending);
    wait(registry, mesh);
    bindings.update();
    const hz::AssetHandle overrideMaterial = registry.find(alternate);
    ASSERT_TRUE(overrideMaterial.isValid());
    wait(registry, overrideMaterial);
    bindings.update();
    ASSERT_EQ(bindings.get(a)->state, hz::AssetBindingState::Ready);
    ASSERT_EQ(bindings.get(b)->state, hz::AssetBindingState::Ready);
    ASSERT_EQ(bindings.get(a)->materials.count, 2u);
    EXPECT_EQ(bindings.get(a)->materials.pData[0].asset, scene.getMaterials().pData[0].id);
    EXPECT_EQ(bindings.get(a)->materials.pData[1].asset, alternate);
    EXPECT_EQ(bindings.get(b)->materials.pData[1].asset, scene.getMaterials().pData[0].id);
    EXPECT_EQ(bindings.get(a)->materials.pData[1].handle, overrideMaterial);

    const MemoryTrackingStats before = memGetTrackingStats();
    for (uint32_t i = 0; i < 100; ++i)
        bindings.update();
    const MemoryTrackingStats after = memGetTrackingStats();
    if (before.trackingEnabled)
    {
        EXPECT_EQ(before.totalAllocationCount, after.totalAllocationCount);
        EXPECT_EQ(before.reallocationCount, after.reallocationCount);
    }

    hz::AssetSnapshot snapshot = registry.captureSnapshot();
    // Reverse authoring order; resolution follows stable SubmeshID, not override array position.
    renderer.overrides.pushBack({ .submesh = UINT64_C(0x8000000000000001), .material = alternate });
    ASSERT_TRUE(world.setComponent(a, hz::SceneTypes::meshRenderer, &renderer));
    bindings.update();
    ASSERT_EQ(bindings.get(a)->state, hz::AssetBindingState::Ready);
    EXPECT_EQ(bindings.get(a)->materials.pData[0].handle, overrideMaterial);
    EXPECT_EQ(bindings.get(a)->materials.pData[1].handle, overrideMaterial);

    const hz::MeshRenderer pendingRenderer = { .mesh = scene.getMeshes().pData[1].id };
    ASSERT_TRUE(world.setComponent(a, hz::SceneTypes::meshRenderer, &pendingRenderer));
    bindings.update();
    EXPECT_EQ(bindings.get(a)->state, hz::AssetBindingState::Pending);
    EXPECT_EQ(bindings.get(a)->materials.count, 0u);
    EXPECT_EQ(registry.getState(overrideMaterial), hz::AssetState::Retiring);
    EXPECT_NE(snapshot.getMaterial(overrideMaterial), nullptr);

    renderer.overrides[0].submesh = 99;
    ASSERT_TRUE(world.setComponent(a, hz::SceneTypes::meshRenderer, &renderer));
    bindings.update();
    EXPECT_EQ(bindings.get(a)->state, hz::AssetBindingState::Failed);
    renderer.overrides[0].submesh = renderer.overrides[1].submesh;
    ASSERT_TRUE(world.setComponent(a, hz::SceneTypes::meshRenderer, &renderer));
    bindings.update();
    EXPECT_EQ(bindings.get(a)->state, hz::AssetBindingState::Failed);
    renderer.overrides.clear();
    renderer.overrides.pushBack({ .submesh = UINT64_C(0x8000000000000001), .material = scene.getTextures().pData[0].id });
    ASSERT_TRUE(world.setComponent(a, hz::SceneTypes::meshRenderer, &renderer));
    bindings.update();
    EXPECT_EQ(bindings.get(a)->state, hz::AssetBindingState::Failed);

    ASSERT_TRUE(world.removeComponent(a, hz::SceneTypes::meshRenderer));
    ASSERT_TRUE(world.destroyEntity(b));
    bindings.update();
    registry.update();
    EXPECT_EQ(bindings.getCount(), 0u);
    EXPECT_EQ(registry.getState(mesh), hz::AssetState::Retiring);
    EXPECT_NE(snapshot.getMesh(mesh), nullptr);
    EXPECT_NE(snapshot.getMaterial(overrideMaterial), nullptr);
    snapshot = {};
    registry.update();
    EXPECT_EQ(registry.getCount(), 0u);
}

TEST_F(TypedAssetLiveTest, RejectsBrokenGeometryAndMismatchedSubassetsAndCancelsLoads)
{
    writeTexture();
    writeScene();
    writeGeometry();
    hz::SceneAsset scene;
    ASSERT_TRUE(scene.parse(kTypedScene));
    hz::AssetCatalog catalog;
    ASSERT_TRUE(catalog.addScene(scene, "scene.sceneasset.json"));
    hz::RenderContext context({ .pAppName = "TypedAssetFailureTest" });
    hz::AssetLoaders  loaders(context, { .vertexLayout = kAssetTestLayout, .cookedDirectory = RD_OTHER_FILES });

    // Every prefix must fail without leaking partially decoded blocks or publishing GPU buffers.
    const uintmax_t size = std::filesystem::file_size(directory / "scene.bin");
    const uintmax_t prefixes[] = { 0, 10, 14, 14 + sizeof(Geometry), size - 1 };
    for (uintmax_t prefix : prefixes)
    {
        writeGeometry();
        std::filesystem::resize_file(directory / "scene.bin", prefix);
        hz::AssetRegistry registry(catalog);
        loaders.registerLoaders(registry);
        const hz::AssetHandle mesh = registry.request(scene.getMeshes().pData[0].id);
        wait(registry, mesh);
        EXPECT_EQ(registry.getError(mesh), hz::AssetError::UploadFailed) << prefix;
        EXPECT_EQ(registry.getMesh(mesh), nullptr);
    }
    writeGeometry(INDEX_TYPE_UINT32);
    {
        hz::AssetRegistry registry(catalog);
        loaders.registerLoaders(registry);
        const hz::AssetHandle mesh = registry.request(scene.getMeshes().pData[0].id);
        wait(registry, mesh);
        EXPECT_EQ(registry.getError(mesh), hz::AssetError::UploadFailed);
        EXPECT_EQ(registry.getMesh(mesh), nullptr);
    }
    std::filesystem::remove(directory / "scene.bin");
    {
        hz::AssetRegistry registry(catalog);
        loaders.registerLoaders(registry);
        const hz::AssetHandle mesh = registry.request(scene.getMeshes().pData[0].id);
        wait(registry, mesh);
        EXPECT_EQ(registry.getError(mesh), hz::AssetError::UploadFailed);
    }
    writeGeometry();
    hz::AssetCatalogEntry wrong = *catalog.find(scene.getMeshes().pData[0].id);
    wrong.location.elementIndex = 1;
    ASSERT_TRUE(catalog.replace(wrong));
    {
        hz::AssetRegistry registry(catalog);
        loaders.registerLoaders(registry);
        const hz::AssetHandle mismatched = registry.request(wrong.id);
        const hz::AssetHandle root = registry.request(scene.getID());
        wait(registry, root);
        EXPECT_EQ(registry.getError(root), hz::AssetError::DependencyFailed);
        EXPECT_EQ(registry.getError(mismatched), hz::AssetError::LoadFailed);
    }
    wrong.location.elementIndex = 0;
    ASSERT_TRUE(catalog.replace(wrong));
    for (uint32_t phase = 0; phase < 2; ++phase)
    {
        hz::AssetRegistry registry(catalog);
        loaders.registerLoaders(registry);
        const hz::AssetHandle root = registry.request(scene.getID());
        if (phase)
        {
            for (uint32_t i = 0; i < 5000; ++i)
            {
                registry.update();
                if (registry.getState(registry.find(wrong.id)) == hz::AssetState::UploadingGPU)
                    break;
                threadSleep(1);
            }
            EXPECT_EQ(registry.getState(registry.find(wrong.id)), hz::AssetState::UploadingGPU);
        }
        else
            registry.update();
        registry.release(root);
        registry.shutdown();
        EXPECT_EQ(registry.getCount(), 0u);
    }
}
