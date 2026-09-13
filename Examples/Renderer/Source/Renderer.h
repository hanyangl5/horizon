#pragma once
#include "Application/IApp.h"
#include "Application/IFreeCameraController.h"
#include "Core/ILog.h"
#include "Core/IMath.h"
#include "Core/IToolFileSystem.h"
#include "Core/IUniquePtr.h"
#include "Graphics/RenderContext.h"
#include "Scene/ISceneManager.h"
#include "Scene/SceneGeometry.h"
#if defined(HORIZON_RENDERER_ASSET_COOKING)
#include "AssetPipeline/IAssetPipeline.h"
#endif

#include "RenderPasses.h"

constexpr const char*     kSceneDirectory = "Assets/Bistro";
constexpr const char*     kSceneSource = "BistroExterior.gltf";
constexpr TinyImageFormat kSurfaceFormat = TinyImageFormat_B8G8R8A8_SRGB;

const VertexLayout kSceneVertexLayout = {
    .mBindings = { { .mStride = 12 }, { .mStride = 4 }, { .mStride = 4 } },
    .mAttribs = {
        { .mSemantic = SEMANTIC_POSITION, .mFormat = TinyImageFormat_R32G32B32_SFLOAT, .mBinding = 0, .mLocation = 0 },
        { .mSemantic = SEMANTIC_NORMAL, .mFormat = TinyImageFormat_R32_UINT, .mBinding = 1, .mLocation = 1 },
        { .mSemantic = SEMANTIC_TEXCOORD0, .mFormat = TinyImageFormat_R32_UINT, .mBinding = 2, .mLocation = 2 },
    },
    .mBindingCount = 3, .mAttribCount = 3,
};

class RendererApp final: public IApp
{
public:
    RendererApp()
    {
        mSettings.mWidth = 1920;
        mSettings.mHeight = 1080;
        mSettings.mVSyncEnabled = true;
        mSettings.mShowPlatformUI = false;
    }

    bool Init() override
    {
        configureResourceDirectories();
        initRenderContext();
        if (!(initSceneAsset() && initRenderPasses()))
        {
            Exit();
            return false;
        }
        initCamera();
        return true;
    }

private:
    void configureResourceDirectories()
    {
        fsSetPathForResourceDir(pSystemFileIO, RM_SAVE_0, RD_MESHES, "RendererAssets");
        fsSetPathForResourceDir(pSystemFileIO, RM_SAVE_0, RD_OTHER_FILES, "RendererAssets");
        fsSetPathForResourceDir(pSystemFileIO, RM_PROJECT, RD_TEXTURES, kSceneDirectory);
        fsSetPathForResourceDir(pSystemFileIO, RM_PROJECT, RD_SHADER_SOURCES, "Examples/Renderer/Shaders");
    }

    void initRenderContext()
    {
        const hz::ContextDesc desc = {
            .pAppName = GetName(),
            .windowHandle = pWindow->handle,
            .imageCount = 2,
            .colorFormat = kSurfaceFormat,
            .colorSpace = COLOR_SPACE_SDR_SRGB,
            .enableVSync = mSettings.mVSyncEnabled,
            .enableGpuValidation = true,
        };
        pContext = hz::make_unique<hz::RenderContext>(desc);
    }

    bool initSceneAsset()
    {
        const SceneManagerDesc sceneDesc = {
            .mCapacity = 1,
            .pContext = pContext.get(),
#if defined(HORIZON_RENDERER_ASSET_COOKING)
            .pEnsureGltfCooked = ensureSceneGltfCooked,
#endif
        };
        pScenes = hz::make_unique<SceneManager>(sceneDesc);

        const GeometryLoadDesc geometryDesc = { .ppGeometryData = &pGeometryData, .pVertexLayout = &kSceneVertexLayout };
        SceneAssetError        error = {};
        mScene = pScenes->requestFromGltf(RD_TEXTURES, kSceneSource, RD_OTHER_FILES, &geometryDesc, &error);
        if (!isSceneAssetHandleValid(mScene))
        {
            LOGF(eERROR, "Failed to request scene '%s': %s", kSceneSource, error.mMessage);
            return false;
        }

        waitForAllResourceLoads();
        pScenes->update();
        if (pScenes->getStatus(mScene) != SCENE_ASSET_STATUS_READY)
        {
            LOGF(eERROR, "Scene '%s' did not become ready", kSceneSource);
            return false;
        }
        if (!pGeometryData)
        {
            LOGF(eERROR, "Scene '%s' contains no geometry data", kSceneSource);
            return false;
        }

        const SceneGeometry* geometry = pScenes->getGeometry(mScene);
        if (!geometry)
        {
            LOGF(eERROR, "Scene '%s' contains no GPU geometry", kSceneSource);
            return false;
        }
        if (!pGeometryData->pUserData || pGeometryData->mUserDataSize < sizeof(SceneAssetGeometryHeader))
        {
            LOGF(eERROR, "Scene '%s' contains no valid scene metadata", kSceneSource);
            return false;
        }

        const SceneAssetGeometryHeader* header = (const SceneAssetGeometryHeader*)pGeometryData->pUserData;
        if (header->mMagic != SCENE_ASSET_GEOMETRY_MAGIC || !header->mInstanceCount ||
            sizeof(*header) + (uint64_t)header->mInstanceCount * sizeof(SceneAssetInstance) > pGeometryData->mUserDataSize)
        {
            LOGF(eERROR, "Scene '%s' contains invalid scene metadata", kSceneSource);
            return false;
        }

        mInstanceCount = header->mInstanceCount;
        pInstances = (const SceneAssetInstance*)(header + 1);

        const float cx = (header->mBoundsMin[0] + header->mBoundsMax[0]) * 0.5f;
        const float cz = (header->mBoundsMin[2] + header->mBoundsMax[2]) * 0.5f;
        mTarget = Point3(cx, header->mBoundsMin[1] + 4.0f, cz);
        mEye = Point3(cx + 18.0f, header->mBoundsMin[1] + 16.0f, cz - 32.0f);
        if (header->mHasCamera)
        {
            const float* camera = header->mCameraWorld;
            mEye = Point3(camera[12], camera[13], camera[14]);
            mTarget = mEye - Vector3(camera[8], camera[9], camera[10]);
            mVerticalFov = header->mCameraYFov;
        }

        LOGF(eINFO, "SceneAsset ready: %u instances, %u draws, %u triangles, %u materials, %u textures", mInstanceCount,
             geometry->mDrawArgCount, geometry->mIndexCount / 3, pScenes->getMaterialCount(mScene), pScenes->getTextureCount(mScene));
        LOGF(eINFO, "Scene bounds: (%f, %f, %f) - (%f, %f, %f)", header->mBoundsMin[0], header->mBoundsMin[1], header->mBoundsMin[2],
             header->mBoundsMax[0], header->mBoundsMax[1], header->mBoundsMax[2]);
        return true;
    }

    bool initRenderPasses()
    {
        const RenderPassesDesc desc = {
            .pContext = pContext.get(),
            .pScenes = pScenes.get(),
            .scene = mScene,
            .pInstances = pInstances,
            .instanceCount = mInstanceCount,
            .pVertexLayout = &kSceneVertexLayout,
            .surfaceFormat = kSurfaceFormat,
            .verticalFov = mVerticalFov,
        };
        pRenderPasses = hz::make_unique<RenderPasses>(desc);
        ASSERT(pRenderPasses);
        return true;
    }

public:
    void Exit() override
    {
        pCamera = nullptr;
        if (pContext && pContext->isValid())
            pContext->waitIdle();
        pRenderPasses = nullptr;
        pScenes = nullptr;
        if (pGeometryData)
            removeResource(pGeometryData);
        pGeometryData = nullptr;
        pInstances = nullptr;
        pContext = nullptr;
    }

    bool Load(ReloadDesc* reload) override
    {
        if (!(reload->mType & (RELOAD_TYPE_RESIZE | RELOAD_TYPE_RENDERTARGET)))
            return true;
        const uint32_t width = (uint32_t)mSettings.mWidth, height = (uint32_t)mSettings.mHeight;
        if (!pContext->resize(width, height))
        {
            LOGF(eERROR, "Failed to resize RenderContext to %ux%u", width, height);
            return false;
        }
        return pRenderPasses->load(width, height);
    }
    void Unload(ReloadDesc*) override { pRenderPasses->unload(); }
    void Update(float deltaTime) override
    {
        pCamera->update(deltaTime, (uint32_t)mSettings.mWidth, (uint32_t)mSettings.mHeight, mSettings.mFocused);
        pRenderPasses->update(*pCamera);
    }
    const char* GetName() override { return "Renderer"; }

    void Draw() override { pRenderPasses->execute(); }

private:
    void initCamera()
    {
        const FreeCameraControllerDesc desc = {
            .pWindow = pWindow,
            .position = Vector3(mEye),
            .lookAt = Vector3(mTarget),
            .motion = { .maxSpeed = 8.0f, .acceleration = 40.0f, .braking = 60.0f },
            .boostMultiplier = 4.0f,
        };
        pCamera = hz::make_unique<FreeCameraController>(desc);
        LOGF(eINFO, "Camera: WASD move, Q/E down/up, hold RMB to look, left Shift boost, R reset, Esc release mouse");
    }

    hz::unique_ptr<FreeCameraController> pCamera;
    hz::unique_ptr<hz::RenderContext>    pContext;
    hz::unique_ptr<SceneManager>         pScenes;
    hz::unique_ptr<RenderPasses>         pRenderPasses;
    SceneAssetHandle                     mScene = {};
    GeometryData*                        pGeometryData = nullptr;
    const SceneAssetInstance*            pInstances = nullptr;
    uint32_t                             mInstanceCount = 0;
    Point3                               mEye;
    Point3                               mTarget;
    float                                mVerticalFov = PI / 4.0f;
};
