#pragma once
#include <math.h>
#include <stdlib.h>
#include <string.h>

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
#include "Core/IMemory.h"


constexpr const char* kSceneDirectory = "Assets/Bistro";
constexpr const char* kSceneSource = "BistroExterior.gltf";
constexpr TinyImageFormat kSurfaceFormat = TinyImageFormat_B8G8R8A8_SRGB;
constexpr TinyImageFormat kDepthFormat = TinyImageFormat_D24_UNORM_S8_UINT;
constexpr TinyImageFormat kGBufferFormats[] = {
    TinyImageFormat_R10G10B10A2_UNORM,
    TinyImageFormat_R10G10B10A2_UNORM,
    TinyImageFormat_R8G8B8A8_UNORM,
    TinyImageFormat_R8G8B8A8_UNORM,
};
constexpr const char* kGBufferNames[] = {
    "Renderer.GBuffer.Emissive",
    "Renderer.GBuffer.NormalMaterial",
    "Renderer.GBuffer.BaseColorMetallic",
    "Renderer.GBuffer.MotionMaterialId",
};
constexpr uint32_t kGBufferCount = TF_ARRAY_COUNT(kGBufferFormats);
const VertexLayout kSceneVertexLayout = {
    .mBindings = { { .mStride = 12 }, { .mStride = 4 }, { .mStride = 4 } },
    .mAttribs = {
        { .mSemantic = SEMANTIC_POSITION, .mFormat = TinyImageFormat_R32G32B32_SFLOAT, .mBinding = 0, .mLocation = 0 },
        { .mSemantic = SEMANTIC_NORMAL, .mFormat = TinyImageFormat_R32_UINT, .mBinding = 1, .mLocation = 1 },
        { .mSemantic = SEMANTIC_TEXCOORD0, .mFormat = TinyImageFormat_R32_UINT, .mBinding = 2, .mLocation = 2 },
    },
    .mBindingCount = 3, .mAttribCount = 3,
};

struct DrawData
{
    Matrix4  world;
    Matrix4  normal;
    uint32_t material;
    float    alphaCutoff;
    uint32_t padding[2];
};
struct FrameData
{
    Matrix4 viewProjection;
    Matrix4 previousViewProjection;
    Matrix4 inverseViewProjection;
    Vector4 eye;
};

int compareMaterials(const void* left, const void* right)
{
    const uint32_t a = ((const SceneAssetInstance*)left)->mMaterialIndex;
    const uint32_t b = ((const SceneAssetInstance*)right)->mMaterialIndex;
    return (a > b) - (a < b);
}

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
        if (!(initSceneAsset() && initRenderResources() && initPipelines()))
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
        pInstances = (SceneAssetInstance*)tf_malloc(mInstanceCount * sizeof(SceneAssetInstance));
        memcpy(pInstances, header + 1, mInstanceCount * sizeof(SceneAssetInstance));
        qsort(pInstances, mInstanceCount, sizeof(SceneAssetInstance), compareMaterials);

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

    bool initRenderResources()
    {
        const SceneGeometry& geometry = getGeometry();
        DrawData* draws = (DrawData*)tf_calloc(mInstanceCount, sizeof(DrawData));
        for (uint32_t i = 0; i < mInstanceCount; ++i)
        {
            if (pInstances[i].mDrawIndex >= geometry.mDrawArgCount ||
                pInstances[i].mMaterialIndex >= pScenes->getMaterialCount(mScene))
            {
                tf_free(draws);
                LOGF(eERROR, "Scene instance has an invalid draw or material index");
                return false;
            }
            memcpy(&draws[i].world, pInstances[i].mWorld, sizeof(Matrix4));
            draws[i].normal = transpose(inverse(draws[i].world));
            draws[i].material = pInstances[i].mMaterialIndex;
            draws[i].alphaCutoff = pInstances[i].mAlphaCutoff;
        }
        const hz::BufferDesc drawDesc = {
            .size = mInstanceCount * sizeof(DrawData),
            .elementCount = mInstanceCount,
            .structStride = sizeof(DrawData),
            .pName = "Renderer.Instances",
            .pInitialData = draws,
            .initialDataSize = mInstanceCount * sizeof(DrawData),
            .usage = RESOURCE_MEMORY_USAGE_GPU_ONLY,
            .startState = RESOURCE_STATE_SHADER_RESOURCE,
            .descriptors = DESCRIPTOR_TYPE_BUFFER,
        };
        mDraws = pContext->createBuffer(drawDesc);
        tf_free(draws);
        if (!mDraws)
        {
            LOGF(eERROR, "Failed to create Renderer.Instances");
            return false;
        }

        const hz::BufferDesc frameDesc = {
            .size = sizeof(FrameData),
            .elementCount = 1,
            .structStride = sizeof(FrameData),
            .pName = "Renderer.Frame",
            .usage = RESOURCE_MEMORY_USAGE_GPU_ONLY,
            .startState = RESOURCE_STATE_SHADER_RESOURCE,
            .descriptors = DESCRIPTOR_TYPE_BUFFER,
        };
        mFrame = pContext->createBuffer(frameDesc);
        if (!mFrame)
        {
            LOGF(eERROR, "Failed to create Renderer.Frame");
            return false;
        }

        const SamplerDesc samplerDesc = {
            .mMinFilter = FILTER_LINEAR,
            .mMagFilter = FILTER_LINEAR,
            .mMipMapMode = MIPMAP_MODE_LINEAR,
            .mAddressU = ADDRESS_MODE_REPEAT,
            .mAddressV = ADDRESS_MODE_REPEAT,
            .mAddressW = ADDRESS_MODE_REPEAT,
        };
        mSampler = pContext->createSampler(samplerDesc);
        if (!mSampler)
        {
            LOGF(eERROR, "Failed to create Renderer.SurfaceSampler");
            return false;
        }
        return true;
    }

    bool initPipelines()
    {
        const hz::ShaderDesc geometryShaderDesc = {
            .stages = {
                { .stage = SHADER_STAGE_VERT, .pEntryPoint = "VSMain", .pName = "Renderer.GeometryVS" },
                { .stage = SHADER_STAGE_FRAG, .pEntryPoint = "PSMain", .pName = "Renderer.GeometryPS" },
            },
            .stageCount = 2,
            .sourceDirectory = RD_SHADER_SOURCES,
            .pFileName = "Geometry.hlsl",
        };
        const hz::ShaderDesc lightingShaderDesc = {
            .stages = {
                { .stage = SHADER_STAGE_VERT, .pEntryPoint = "VSMain", .pName = "Renderer.LightingVS" },
                { .stage = SHADER_STAGE_FRAG, .pEntryPoint = "PSMain", .pName = "Renderer.LightingPS" },
            },
            .stageCount = 2,
            .sourceDirectory = RD_SHADER_SOURCES,
            .pFileName = "Lighting.hlsl",
        };
        mGeometryShader = pContext->createShader(geometryShaderDesc);
        if (!mGeometryShader)
        {
            LOGF(eERROR, "Failed to create Geometry shader");
            return false;
        }
        mLightingShader = pContext->createShader(lightingShaderDesc);
        if (!mLightingShader)
        {
            LOGF(eERROR, "Failed to create Lighting shader");
            return false;
        }

        const hz::GraphicsPipelineDesc geometryPipelineDesc = {
            .pShader = &mGeometryShader, .vertexLayout = kSceneVertexLayout,
            .rasterizer = { .mCullMode = CULL_MODE_NONE, .mFillMode = FILL_MODE_SOLID },
            .depth = { .mDepthTest = true, .mDepthWrite = true, .mDepthFunc = CMP_LEQUAL },
            .blend = {
                .mSrcFactors = { BC_ONE }, .mDstFactors = { BC_ZERO }, .mSrcAlphaFactors = { BC_ONE }, .mDstAlphaFactors = { BC_ZERO },
                .mBlendModes = { BM_ADD }, .mBlendAlphaModes = { BM_ADD }, .mColorWriteMasks = { COLOR_MASK_ALL },
                .mRenderTargetMask = (BlendStateTargets)(BLEND_STATE_TARGET_0 | BLEND_STATE_TARGET_1 |
                                                         BLEND_STATE_TARGET_2 | BLEND_STATE_TARGET_3),
            },
            .colorFormats = { kGBufferFormats[0], kGBufferFormats[1], kGBufferFormats[2], kGBufferFormats[3] },
            .renderTargetCount = kGBufferCount, .depthStencilFormat = kDepthFormat,
            .topology = PRIMITIVE_TOPO_TRI_LIST, .sampleCount = SAMPLE_COUNT_1, .pName = "Renderer.GeometryPipeline",
        };
        const hz::GraphicsPipelineDesc lightingPipelineDesc = {
            .pShader = &mLightingShader,
            .rasterizer = { .mCullMode = CULL_MODE_NONE, .mFillMode = FILL_MODE_SOLID },
            .depth = { .mDepthFunc = CMP_ALWAYS },
            .blend = {
                .mSrcFactors = { BC_ONE }, .mDstFactors = { BC_ZERO }, .mSrcAlphaFactors = { BC_ONE }, .mDstAlphaFactors = { BC_ZERO },
                .mBlendModes = { BM_ADD }, .mBlendAlphaModes = { BM_ADD }, .mColorWriteMasks = { COLOR_MASK_ALL },
                .mRenderTargetMask = BLEND_STATE_TARGET_0,
            },
            .colorFormats = { kSurfaceFormat }, .renderTargetCount = 1,
            .topology = PRIMITIVE_TOPO_TRI_LIST, .sampleCount = SAMPLE_COUNT_1, .pName = "Renderer.LightingPipeline",
        };
        mGeometryPipeline = pContext->createGraphicsPipeline(geometryPipelineDesc);
        if (!mGeometryPipeline)
        {
            LOGF(eERROR, "Failed to create Geometry pipeline");
            return false;
        }
        mLightingPipeline = pContext->createGraphicsPipeline(lightingPipelineDesc);
        if (!mLightingPipeline)
        {
            LOGF(eERROR, "Failed to create Lighting pipeline");
            return false;
        }
        return true;
    }

public:
    void Exit() override
    {
        pCamera = nullptr;
        if (pContext && *pContext)
            pContext->waitIdle();
        mLightingPipeline = {};
        mGeometryPipeline = {};
        mLightingShader = {};
        mGeometryShader = {};
        mSampler = {};
        for (hz::GPUTexture& target : mGBuffer)
            target = {};
        mDepth = {};
        mDraws = {};
        mFrame = {};
        pScenes = nullptr;
        if (pGeometryData)
            removeResource(pGeometryData);
        pGeometryData = nullptr;
        tf_free(pInstances);
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
        if (!width || !height)
            return true;
        hz::TextureDesc targetDesc = {
            .width = width,
            .height = height,
            .depth = 1,
            .arraySize = 1,
            .mipLevels = 1,
            .sampleCount = SAMPLE_COUNT_1,
            .startState = RESOURCE_STATE_RENDER_TARGET,
            .descriptors = DESCRIPTOR_TYPE_TEXTURE,
            .flags = TEXTURE_CREATION_FLAG_FORCE_2D,
            .renderTarget = true,
        };
        for (uint32_t i = 0; i < kGBufferCount; ++i)
        {
            targetDesc.format = kGBufferFormats[i];
            targetDesc.pName = kGBufferNames[i];
            mGBuffer[i] = pContext->createTexture(targetDesc);
            if (!mGBuffer[i])
            {
                LOGF(eERROR, "Failed to create %s", kGBufferNames[i]);
                return false;
            }
        }
        targetDesc.format = kDepthFormat;
        targetDesc.startState = RESOURCE_STATE_DEPTH_WRITE;
        targetDesc.pName = "Renderer.Depth";
        const hz::TextureDesc depthDesc = targetDesc;
        mDepth = pContext->createTexture(depthDesc);
        mHasPreviousViewProjection = false;
        if (!mDepth)
        {
            LOGF(eERROR, "Failed to create Renderer.Depth");
            return false;
        }
        return true;
    }
    void Unload(ReloadDesc*) override
    {
        for (hz::GPUTexture& target : mGBuffer)
            target = {};
        mDepth = {};
        mHasPreviousViewProjection = false;
    }
    void Update(float deltaTime) override
    {
        pCamera->update(deltaTime, (uint32_t)mSettings.mWidth, (uint32_t)mSettings.mHeight, mSettings.mFocused);
    }
    const char* GetName() override { return "Renderer"; }

    void Draw() override
    {
        if (pContext->isSuspended())
            return;
        const SceneGeometry& geometry = getGeometry();
        hz::CommandList& commands = pContext->acquireCommandList();
        const float      aspectInverse = (float)pContext->getHeight() / (float)pContext->getWidth();
        const float      horizontalFov = 2.0f * atanf(tanf(mVerticalFov * 0.5f) / aspectInverse);
        const Matrix4    viewProjection = Matrix4::perspectiveRH(horizontalFov, aspectInverse, 0.1f, 1000.0f) *
                                       Matrix4::scale(Vector3(-1.0f, 1.0f, -1.0f)) * pCamera->getViewMatrix();
        if (!mHasPreviousViewProjection)
            mPreviousViewProjection = viewProjection;
        const FrameData  frame = {
            .viewProjection = viewProjection,
            .previousViewProjection = mPreviousViewProjection,
            .inverseViewProjection = inverse(viewProjection),
            .eye = Vector4(pCamera->getPosition(), 1.0f),
        };
        commands.updateBuffer(mFrame, 0, &frame, sizeof(frame));

        const ClearValue black = {};
        const hz::RenderPassDesc geometryPass = {
            .colorAttachments = {
                { .pTexture = &mGBuffer[0], .loadAction = LOAD_ACTION_CLEAR, .storeAction = STORE_ACTION_STORE, .clearValue = black },
                { .pTexture = &mGBuffer[1], .loadAction = LOAD_ACTION_CLEAR, .storeAction = STORE_ACTION_STORE, .clearValue = black },
                { .pTexture = &mGBuffer[2], .loadAction = LOAD_ACTION_CLEAR, .storeAction = STORE_ACTION_STORE, .clearValue = black },
                { .pTexture = &mGBuffer[3], .loadAction = LOAD_ACTION_CLEAR, .storeAction = STORE_ACTION_STORE, .clearValue = black },
            },
            .colorAttachmentCount = kGBufferCount,
            .depthAttachment = { .pTexture = &mDepth,
                                 .loadAction = LOAD_ACTION_CLEAR,
                                 .storeAction = STORE_ACTION_STORE,
                                 .clearValue = { .depth = 1.0f } },
        };
        commands.beginGpuTimestamp("Geometry");
        commands.beginRendering(geometryPass);
        commands.setViewport(0, 0, (float)pContext->getWidth(), (float)pContext->getHeight());
        commands.setScissor(0, 0, pContext->getWidth(), pContext->getHeight());
        commands.setPipeline(mGeometryPipeline);
        commands.bindBuffer("Frame", mFrame);
        commands.bindBuffer("Draws", mDraws);
        commands.bindBuffer("Materials", *pScenes->getMaterialBuffer(mScene));
        commands.bindSampler("SurfaceSampler", mSampler);
        for (uint32_t i = 0; i < geometry.mVertexBufferCount; ++i)
        {
            commands.setVertexBuffer(i, geometry.mVertexBuffers[i], 0, geometry.mVertexStrides[i]);
        }
        commands.setIndexBuffer(geometry.mIndexBuffer, 0, geometry.mIndexType);
        const SceneAssetGpuMaterial* gpuMaterials = pScenes->getGpuMaterials(mScene);
        uint32_t                     previousMaterial = UINT32_MAX;
        for (uint32_t i = 0; i < mInstanceCount; ++i)
        {
            const SceneAssetInstance& instance = pInstances[i];
            if (instance.mMaterialIndex != previousMaterial)
            {
                const SceneAssetGpuMaterial& material = gpuMaterials[instance.mMaterialIndex];
                const uint32_t textureIndices[] = { material.mBaseColorTexture, material.mNormalTexture, material.mMetallicRoughnessTexture,
                                                    material.mEmissiveTexture };
                const char*    names[] = { "BaseColor", "NormalMap", "MetallicRoughness", "Emissive" };
                for (uint32_t t = 0; t < 4; ++t)
                {
                    // Missing maps are not sampled by the shader, but still need a valid descriptor.
                    const uint32_t       textureIndex = textureIndices[t] == UINT32_MAX ? 0 : textureIndices[t];
                    commands.bindTexture(names[t], *pScenes->getTexture(mScene, textureIndex));
                }
                previousMaterial = instance.mMaterialIndex;
            }
            commands.setPushConstants(0, &i, sizeof(i));
            const IndirectDrawIndexArguments& draw = geometry.pDrawArgs[instance.mDrawIndex];
            commands.drawIndexed(draw.mIndexCount, draw.mStartIndex, draw.mVertexOffset);
        }
        commands.endRendering();
        commands.endGpuTimestamp();

        const hz::GPUTexture& backbuffer = pContext->getCurrentBackbuffer();
        const hz::RenderPassDesc lightingPass = {
            .colorAttachments = { { .pTexture = &backbuffer,
                                    .loadAction = LOAD_ACTION_CLEAR,
                                    .storeAction = STORE_ACTION_STORE,
                                    .clearValue = { .r = 0.02f, .g = 0.035f, .b = 0.055f, .a = 1.0f } } },
            .colorAttachmentCount = 1,
        };
        const hz::GPUTexture* sampledTextures[] = { &mGBuffer[0], &mGBuffer[1], &mGBuffer[2], &mDepth };
        commands.beginGpuTimestamp("Lighting");
        commands.beginRendering(lightingPass, { .sampledTextures = sampledTextures });
        commands.setViewport(0, 0, (float)pContext->getWidth(), (float)pContext->getHeight());
        commands.setScissor(0, 0, pContext->getWidth(), pContext->getHeight());
        commands.setPipeline(mLightingPipeline);
        commands.bindTexture("EmissiveBuffer", mGBuffer[0]);
        commands.bindTexture("NormalMaterialBuffer", mGBuffer[1]);
        commands.bindTexture("BaseColorMetallicBuffer", mGBuffer[2]);
        commands.bindTexture("DepthBuffer", mDepth);
        commands.bindBuffer("Frame", mFrame);
        commands.draw(3);
        commands.endRendering();
        commands.endGpuTimestamp();

        pContext->submit(commands, &backbuffer);
        mPreviousViewProjection = viewProjection;
        mHasPreviousViewProjection = true;
    }

private:
    const SceneGeometry& getGeometry() const
    {
        const SceneGeometry* geometry = pScenes->getGeometry(mScene);
        ASSERT(geometry);
        return *geometry;
    }

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
    hz::unique_ptr<hz::RenderContext> pContext;
    hz::unique_ptr<SceneManager> pScenes;
    SceneAssetHandle    mScene = {};
    GeometryData*       pGeometryData = nullptr;
    SceneAssetInstance* pInstances = nullptr;
    uint32_t            mInstanceCount = 0;
    hz::GPUBuffer       mFrame;
    hz::GPUBuffer       mDraws;
    hz::GPUTexture      mGBuffer[kGBufferCount];
    hz::GPUTexture      mDepth;
    hz::GPUSampler      mSampler;
    hz::GPUShader       mGeometryShader;
    hz::GPUShader       mLightingShader;
    hz::GPUPipeline     mGeometryPipeline;
    hz::GPUPipeline     mLightingPipeline;
    Matrix4             mPreviousViewProjection;
    bool                mHasPreviousViewProjection = false;
    Point3              mEye;
    Point3              mTarget;
    float                mVerticalFov = PI / 4.0f;
};
