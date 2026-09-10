#include <gtest/gtest.h>

#include <stdlib.h>
#include <string.h>

#include "Core/ILog.h"
#define IMEMORY_FROM_HEADER
#include "Core/IMemory.h"
#include "Scene/ISceneManager.h"

namespace
{
struct RuntimeServices
{
    bool             mMemoryInitialized = false;
    bool             mFileSystemInitialized = false;
    bool             mResourceLoaderInitialized = false;
    bool             mInteractiveModeDisabled = false;
    RendererContext* pRendererContext = nullptr;
    Renderer*        pRenderer = nullptr;

    ~RuntimeServices()
    {
        if (mResourceLoaderInitialized)
            exitResourceLoaderInterface(pRenderer);
        if (pRenderer)
            exitRenderer(pRenderer);
        if (pRendererContext)
            exitRendererContext(pRendererContext);
        if (mFileSystemInitialized)
            exitFileSystem();
        if (mInteractiveModeDisabled)
            _EnableInteractiveMode(true);
        if (mMemoryInitialized)
            exitMemAlloc();
    }
};

bool configureCookedSceneFileSystem(RuntimeServices* pServices, const char* pCookedRoot)
{
    FileSystemInitDesc fileSystemDesc = {
        .pAppName = "AssetPipelineContractTests",
    };
    fileSystemDesc.pResourceMounts[RM_PROJECT] = pCookedRoot;
    if (!initFileSystem(&fileSystemDesc))
        return false;
    pServices->mFileSystemInitialized = true;
    fsSetPathForResourceDir(pSystemFileIO, RM_PROJECT, RD_OTHER_FILES, "");
    fsSetPathForResourceDir(pSystemFileIO, RM_PROJECT, RD_MESHES, "");
    return true;
}
} // namespace

TEST(SceneAssetCookerContractTest, CookedFixtureParsesAndUsesGeometryTfPayload)
{
    const char* pCookedRoot = getenv("HORIZON_COOKED_SCENE_ROOT");
    if (!pCookedRoot || !pCookedRoot[0])
        GTEST_SKIP() << "HORIZON_COOKED_SCENE_ROOT is only set by the cooker contract fixture";

    RuntimeServices services;
    ASSERT_TRUE(initMemAlloc(nullptr));
    services.mMemoryInitialized = true;

    ASSERT_TRUE(configureCookedSceneFileSystem(&services, pCookedRoot));

    SceneAssetManifest manifest = {};
    SceneAssetError    error = {};
    ASSERT_TRUE(loadSceneAssetManifest(RD_OTHER_FILES, "triangle.scene.json", &manifest, &error)) << error.mMessage;
    EXPECT_EQ(manifest.mVersion, SCENE_ASSET_MANIFEST_VERSION);
    EXPECT_EQ(strncmp(manifest.mContentHash, "fnv1a64:", 8), 0);
    ASSERT_GE(manifest.mDependencyCount, 1u);
    EXPECT_STREQ(manifest.mDependencies[0], "triangle.gltf");
    EXPECT_STREQ(manifest.mDefaultScene, "triangle");
    EXPECT_STREQ(manifest.mGeometry, "triangle.bin");

    FileStream geometryStream = {};
    ASSERT_TRUE(fsOpenStreamFromPath(RD_OTHER_FILES, manifest.mGeometry, FM_READ, &geometryStream));
    char magic[sizeof(GEOMETRY_FILE_MAGIC_STR)] = {};
    EXPECT_EQ(fsReadFromStream(&geometryStream, magic, sizeof(magic)), sizeof(magic));
    EXPECT_EQ(memcmp(magic, GEOMETRY_FILE_MAGIC_STR, sizeof(magic)), 0);
    EXPECT_TRUE(fsCloseStream(&geometryStream));
}

TEST(SceneAssetCookerContractTest, LoadsCookedGeometryThroughResourceLoader)
{
    const char* pCookedRoot = getenv("HORIZON_COOKED_SCENE_ROOT");
    if (!pCookedRoot || !pCookedRoot[0])
        GTEST_SKIP() << "HORIZON_COOKED_SCENE_ROOT is only set by the cooker contract fixture";

    RuntimeServices services;
    ASSERT_TRUE(initMemAlloc(nullptr));
    services.mMemoryInitialized = true;
    _EnableInteractiveMode(false);
    services.mInteractiveModeDisabled = true;
    ASSERT_TRUE(configureCookedSceneFileSystem(&services, pCookedRoot));

    RendererContextDesc contextDesc = {};
    initRendererContext("SceneAssetCookerContractTest", &contextDesc, &services.pRendererContext);
    if (!services.pRendererContext)
        GTEST_SKIP() << "No supported renderer context is available for the live GeometryTF load";

    RendererDesc rendererDesc = { .pContext = services.pRendererContext };
    initRenderer("SceneAssetCookerContractTest", &rendererDesc, &services.pRenderer);
    if (!services.pRenderer)
        GTEST_SKIP() << "No supported renderer is available for the live GeometryTF load";

    ResourceLoaderDesc loaderDesc = gDefaultResourceLoaderDesc;
    loaderDesc.mSingleThreaded = true;
    initResourceLoaderInterface(services.pRenderer, &loaderDesc);
    services.mResourceLoaderInitialized = true;

    VertexLayout vertexLayout = {};
    vertexLayout.mAttribCount = 6;
    vertexLayout.mAttribs[0] = { .mSemantic = SEMANTIC_POSITION, .mFormat = TinyImageFormat_R32G32B32_SFLOAT, .mBinding = 0 };
    vertexLayout.mAttribs[1] = { .mSemantic = SEMANTIC_NORMAL, .mFormat = TinyImageFormat_R32_UINT, .mBinding = 1 };
    vertexLayout.mAttribs[2] = { .mSemantic = SEMANTIC_TANGENT, .mFormat = TinyImageFormat_R32_UINT, .mBinding = 2 };
    vertexLayout.mAttribs[3] = { .mSemantic = SEMANTIC_TEXCOORD0, .mFormat = TinyImageFormat_R32_UINT, .mBinding = 3 };
    vertexLayout.mAttribs[4] = { .mSemantic = SEMANTIC_JOINTS, .mFormat = TinyImageFormat_R16G16B16A16_UINT, .mBinding = 4 };
    vertexLayout.mAttribs[5] = { .mSemantic = SEMANTIC_WEIGHTS, .mFormat = TinyImageFormat_R32G32B32A32_SFLOAT, .mBinding = 5 };

    Geometry*        pGeometry = nullptr;
    GeometryData*    pGeometryData = nullptr;
    GeometryLoadDesc loadDesc = {
        .ppGeometry = &pGeometry,
        .ppGeometryData = &pGeometryData,
        .pFileName = "triangle.bin",
        .pVertexLayout = &vertexLayout,
    };
    SyncToken token = {};
    addResource(&loadDesc, &token);
    waitForToken(&token);

    ASSERT_NE(pGeometry, nullptr);
    EXPECT_EQ(pGeometry->mVertexCount, 3u);
    EXPECT_EQ(pGeometry->mIndexCount, 3u);
    EXPECT_EQ(pGeometry->mDrawArgCount, 1u);
    ASSERT_NE(pGeometryData, nullptr);
    ASSERT_NE(pGeometryData->pUserData, nullptr);
    ASSERT_EQ(pGeometryData->mUserDataSize, sizeof(SceneAssetGeometryHeader) + sizeof(SceneAssetInstance));
    const SceneAssetGeometryHeader* header = (const SceneAssetGeometryHeader*)pGeometryData->pUserData;
    EXPECT_EQ(header->mMagic, SCENE_ASSET_GEOMETRY_MAGIC);
    EXPECT_EQ(header->mInstanceCount, 1u); // The mesh node outside the selected scene must be excluded.
    EXPECT_EQ(header->mHasCamera, 1u);
    EXPECT_FLOAT_EQ(header->mCameraYFov, 0.9f);
    EXPECT_FLOAT_EQ(header->mCameraWorld[12], 2.0f);
    EXPECT_FLOAT_EQ(header->mCameraWorld[13], 3.0f);
    EXPECT_FLOAT_EQ(header->mCameraWorld[14], 9.0f);
    const SceneAssetInstance* instance = (const SceneAssetInstance*)(header + 1);
    EXPECT_EQ(instance->mDrawIndex, 0u);
    EXPECT_EQ(instance->mMaterialIndex, 0u);
    EXPECT_FLOAT_EQ(instance->mAlphaCutoff, 0.35f);
    EXPECT_FLOAT_EQ(instance->mWorld[0], 2.0f);
    EXPECT_FLOAT_EQ(instance->mWorld[5], 2.0f);
    EXPECT_FLOAT_EQ(instance->mWorld[10], 2.0f);
    for (uint32_t axis = 0; axis < 3; ++axis)
    {
        EXPECT_FLOAT_EQ(instance->mWorld[12 + axis], (float)(axis + 2));
        EXPECT_FLOAT_EQ(header->mBoundsMin[axis], (float)(axis + 2));
    }
    EXPECT_FLOAT_EQ(header->mBoundsMax[0], 4.0f);
    EXPECT_FLOAT_EQ(header->mBoundsMax[1], 5.0f);
    EXPECT_FLOAT_EQ(header->mBoundsMax[2], 4.0f);
    removeResource(pGeometryData);
    removeResource(pGeometry);
}
