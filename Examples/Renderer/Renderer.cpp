#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "Application/IApp.h"
#include "Application/ICameraController.h"
#include "Platform/IInput.h"
#include "Core/ILog.h"
#include "Core/IMath.h"
#include "Core/IToolFileSystem.h"
#include "Graphics/RenderContext.h"
#include "Scene/ISceneManager.h"
#include "Scene/SceneGeometry.h"
#if defined(HORIZON_RENDERER_ASSET_COOKING)
#include "AssetPipeline/IAssetPipeline.h"
#endif
#include "Core/IMemory.h"

namespace
{
constexpr const char* kSceneSource = "D:/Codes/models/Bistro_v5_2/BistroExterior.gltf";
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
    Vector4 eye;
};

int compareMaterials(const void* left, const void* right)
{
    const uint32_t a = ((const SceneAssetInstance*)left)->mMaterialIndex;
    const uint32_t b = ((const SceneAssetInstance*)right)->mMaterialIndex;
    return (a > b) - (a < b);
}

constexpr char kShader[] = R"(
struct FrameData { float4x4 viewProjection; float4 eye; };
struct DrawData { float4x4 world; float4x4 normal; uint material; float alphaCutoff; uint2 padding; };
struct Material {
    uint baseTexture; uint normalTexture; uint metallicRoughnessTexture; uint emissiveTexture;
    float4 baseColor; float3 emissive; float metallic; float roughness; float3 padding;
};
StructuredBuffer<FrameData> Frame : register(t0);
StructuredBuffer<DrawData> Draws : register(t1);
StructuredBuffer<Material> Materials : register(t2);
Texture2D BaseColor : register(t3);
Texture2D NormalMap : register(t4);
Texture2D MetallicRoughness : register(t5);
Texture2D Emissive : register(t6);
SamplerState SurfaceSampler : register(s0);
cbuffer RootConstant0 : register(b0) { uint drawId; };
struct VSInput { float3 position : POSITION; uint normal : NORMAL; uint uv : TEXCOORD0; };
struct Varyings {
    float4 position : SV_Position; float3 worldPosition : TEXCOORD0;
    float3 normal : TEXCOORD1; float2 uv : TEXCOORD2;
};
float3 decodeNormal(uint packed) {
    float2 e = float2(packed & 65535, packed >> 16) / 65535.0 * 2.0 - 1.0;
    float3 n = float3(e, 1.0 - abs(e.x) - abs(e.y));
    if (n.z < 0) n.xy = (1.0 - abs(n.yx)) * float2(n.x >= 0 ? 1 : -1, n.y >= 0 ? 1 : -1);
    return normalize(n);
}
Varyings VSMain(VSInput input) {
    DrawData d = Draws[drawId];
    Varyings o;
    float4 world = mul(d.world, float4(input.position, 1));
    o.position = mul(Frame[0].viewProjection, world);
    o.worldPosition = world.xyz;
    o.normal = normalize(mul((float3x3)d.normal, decodeNormal(input.normal)));
    o.uv = float2(f16tof32(input.uv & 65535), f16tof32(input.uv >> 16));
    return o;
}
float4 PSMain(Varyings input, bool frontFace : SV_IsFrontFace) : SV_Target0 {
    DrawData d = Draws[drawId];
    Material m = Materials[d.material];
    float4 base = m.baseColor;
    if (m.baseTexture != 0xffffffff) base *= BaseColor.Sample(SurfaceSampler, input.uv);
    clip(base.a - d.alphaCutoff);
    float3 N = normalize(input.normal) * (frontFace ? 1.0 : -1.0);
    if (m.normalTexture != 0xffffffff) {
        float3 q1 = ddx(input.worldPosition), q2 = ddy(input.worldPosition);
        float2 st1 = ddx(input.uv), st2 = ddy(input.uv);
        float3 T = q1 * st2.y - q2 * st1.y;
        float3 B = q2 * st1.x - q1 * st2.x;
        float invLength = rsqrt(max(max(dot(T,T), dot(B,B)), 1e-10));
        float2 normalXY = NormalMap.Sample(SurfaceSampler, input.uv).xy * 2.0 - 1.0;
        float3 mapped = float3(normalXY, sqrt(saturate(1.0-dot(normalXY, normalXY))));
        N = normalize(T * invLength * mapped.x + B * invLength * mapped.y + N * mapped.z);
    }
    float roughness = m.roughness, metallic = m.metallic;
    if (m.metallicRoughnessTexture != 0xffffffff) {
        float4 mr = MetallicRoughness.Sample(SurfaceSampler, input.uv);
        roughness *= mr.g; metallic *= mr.b;
    }
    roughness = clamp(roughness, 0.08, 1.0);
    float3 L = normalize(float3(-0.4, 0.8, 0.3));
    float3 V = normalize(Frame[0].eye.xyz - input.worldPosition);
    float3 H = normalize(L + V);
    float nl = saturate(dot(N,L)), nv = max(saturate(dot(N,V)), 0.001);
    float nh = saturate(dot(N,H)), vh = saturate(dot(V,H));
    float a = roughness * roughness, a2 = a*a;
    float denom = nh*nh*(a2-1.0)+1.0;
    float D = a2 / max(3.14159265*denom*denom, 1e-5);
    float k = (roughness+1.0)*(roughness+1.0)/8.0;
    float G = nv/(nv*(1-k)+k) * nl/(nl*(1-k)+k);
    float3 F = lerp(0.04.xxx, base.rgb, metallic);
    F += (1.0-F)*pow(1.0-vh, 5.0);
    float3 color = ((1-F)*(1-metallic)*base.rgb/3.14159265 + D*G*F/max(4*nv*nl, 0.001))*nl*3.0;
    color += base.rgb * lerp(float3(0.10,0.09,0.08), float3(0.28,0.34,0.42), N.y*0.5+0.5);
    float3 emission = m.emissive;
    if (m.emissiveTexture != 0xffffffff) emission *= Emissive.Sample(SurfaceSampler, input.uv).rgb;
    color += emission;
    color = color / (1.0 + color);
    return float4(color, 1);
}
)";
} // namespace

class RendererApp final: public IApp
{
public:
    RendererApp()
    {
        mSettings.mWidth = 1280;
        mSettings.mHeight = 720;
        mSettings.mVSyncEnabled = true;
        mSettings.mShowPlatformUI = false;
    }

    bool Init() override
    {
        if (initScene() && initCamera())
            return true;
        Exit();
        return false;
    }

    bool initScene()
    {
        char textureDirectory[FS_MAX_PATH] = {};
        char sceneName[FS_MAX_PATH] = {};
        char sourceFile[FS_MAX_PATH] = {};
        fsGetParentPath(kSceneSource, textureDirectory);
        fsGetPathFileName(kSceneSource, sceneName);
        fsAppendPathExtension(sceneName, "gltf", sourceFile);
        mSceneFileSystem = *pSystemFileIO;
        mSceneFileSystem.GetResourceMount = nullptr;
        fsSetPathForResourceDir(&mSceneFileSystem, RM_CONTENT, RD_MESHES, HORIZON_RENDERER_ASSET_DIR);
        fsSetPathForResourceDir(&mSceneFileSystem, RM_CONTENT, RD_OTHER_FILES, HORIZON_RENDERER_ASSET_DIR);
        fsSetPathForResourceDir(&mSceneFileSystem, RM_CONTENT, RD_TEXTURES, textureDirectory);
        const hz::ContextDesc desc = {
            .pAppName = GetName(),
            .windowHandle = pWindow->handle,
            .imageCount = 2,
            .colorFormat = kSurfaceFormat,
            .colorSpace = COLOR_SPACE_SDR_SRGB,
            .enableVSync = mSettings.mVSyncEnabled,
            .enableGpuValidation = true,
        };
        pContext = tf_new(hz::RenderContext);
        if (!pContext->init(desc))
            return false;
        const SceneManagerDesc sceneDesc = {
            .mCapacity = 1,
            .pContext = pContext,
#if defined(HORIZON_RENDERER_ASSET_COOKING)
            .pEnsureGltfCooked = ensureSceneGltfCooked,
#endif
        };
        if (!initSceneManager(&sceneDesc, &pScenes))
            return false;
        const GeometryLoadDesc geometryDesc = { .ppGeometryData = &pGeometryData, .pVertexLayout = &kSceneVertexLayout };
        SceneAssetError        error = {};
        mScene = requestSceneAssetFromGltf(pScenes, RD_TEXTURES, sourceFile, RD_OTHER_FILES, &geometryDesc, &error);
        if (!isSceneAssetHandleValid(mScene))
        {
            LOGF(eERROR, "SceneAsset load failed: %s (source: %s)", error.mMessage, kSceneSource);
            return false;
        }
        waitForAllResourceLoads();
        updateSceneManager(pScenes);
        if (getSceneAssetStatus(pScenes, mScene) != SCENE_ASSET_STATUS_READY || !pGeometryData)
            return false;
        pGeometry = getSceneAssetGeometry(pScenes, mScene);
        if (!pGeometryData->pUserData || pGeometryData->mUserDataSize < sizeof(SceneAssetGeometryHeader))
            return false;
        const SceneAssetGeometryHeader* header = (const SceneAssetGeometryHeader*)pGeometryData->pUserData;
        if (header->mMagic != SCENE_ASSET_GEOMETRY_MAGIC || !header->mInstanceCount ||
            sizeof(*header) + (uint64_t)header->mInstanceCount * sizeof(SceneAssetInstance) > pGeometryData->mUserDataSize)
            return false;
        mInstanceCount = header->mInstanceCount;
        pInstances = (SceneAssetInstance*)tf_malloc(mInstanceCount * sizeof(SceneAssetInstance));
        memcpy(pInstances, header + 1, mInstanceCount * sizeof(SceneAssetInstance));
        qsort(pInstances, mInstanceCount, sizeof(SceneAssetInstance), compareMaterials);
        DrawData* draws = (DrawData*)tf_calloc(mInstanceCount, sizeof(DrawData));
        for (uint32_t i = 0; i < mInstanceCount; ++i)
        {
            if (pInstances[i].mDrawIndex >= pGeometry->mDrawArgCount ||
                pInstances[i].mMaterialIndex >= getSceneAssetMaterialCount(pScenes, mScene))
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
        const hz::ShaderDesc shaderDesc = {
            .stages = {
                { .stage = SHADER_STAGE_VERT, .pSource = kShader, .sourceSize = sizeof(kShader)-1, .pEntryPoint = "VSMain", .pName = "Renderer.SceneVS" },
                { .stage = SHADER_STAGE_FRAG, .pSource = kShader, .sourceSize = sizeof(kShader)-1, .pEntryPoint = "PSMain", .pName = "Renderer.ScenePS" },
            }, .stageCount = 2,
        };
        mShader = pContext->createShader(shaderDesc);
        const hz::GraphicsPipelineDesc pipelineDesc = {
            .pShader = &mShader, .vertexLayout = kSceneVertexLayout,
            .rasterizer = { .mCullMode = CULL_MODE_NONE, .mFillMode = FILL_MODE_SOLID },
            .depth = { .mDepthTest = true, .mDepthWrite = true, .mDepthFunc = CMP_LEQUAL },
            .blend = {
                .mSrcFactors = { BC_ONE }, .mDstFactors = { BC_ZERO }, .mSrcAlphaFactors = { BC_ONE }, .mDstAlphaFactors = { BC_ZERO },
                .mBlendModes = { BM_ADD }, .mBlendAlphaModes = { BM_ADD }, .mColorWriteMasks = { COLOR_MASK_ALL }, .mRenderTargetMask = BLEND_STATE_TARGET_0,
            },
            .colorFormats = { kSurfaceFormat }, .renderTargetCount = 1, .depthStencilFormat = TinyImageFormat_D32_SFLOAT,
            .topology = PRIMITIVE_TOPO_TRI_LIST, .sampleCount = SAMPLE_COUNT_1, .pName = "Renderer.ScenePipeline",
        };
        mPipeline = pContext->createGraphicsPipeline(pipelineDesc);
        const SamplerDesc samplerDesc = {
            .mMinFilter = FILTER_LINEAR,
            .mMagFilter = FILTER_LINEAR,
            .mMipMapMode = MIPMAP_MODE_LINEAR,
            .mAddressU = ADDRESS_MODE_REPEAT,
            .mAddressV = ADDRESS_MODE_REPEAT,
            .mAddressW = ADDRESS_MODE_REPEAT,
        };
        mSampler = pContext->createSampler(samplerDesc);
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
             pGeometry->mDrawArgCount, pGeometry->mIndexCount / 3, getSceneAssetMaterialCount(pScenes, mScene),
             getSceneAssetTextureCount(pScenes, mScene));
        LOGF(eINFO, "Scene bounds: (%f, %f, %f) - (%f, %f, %f)", header->mBoundsMin[0], header->mBoundsMin[1], header->mBoundsMin[2],
             header->mBoundsMax[0], header->mBoundsMax[1], header->mBoundsMax[2]);
        return true;
    }

    void Exit() override
    {
        if (mInputInitialized)
        {
            setEnableCaptureInput(false);
            exitInputSystem();
            mInputInitialized = false;
        }
        if (pCamera)
        {
            exitCameraController(pCamera);
            pCamera = nullptr;
        }
        if (pContext && *pContext)
            pContext->waitIdle();
        mPipeline = {};
        mShader = {};
        mSampler = {};
        mDepth = {};
        mDraws = {};
        mFrame = {};
        if (pScenes)
        {
            releaseSceneAsset(pScenes, mScene);
            exitSceneManager(pScenes);
            pScenes = nullptr;
        }
        if (pGeometryData)
            removeResource(pGeometryData);
        pGeometryData = nullptr;
        tf_free(pInstances);
        pInstances = nullptr;
        tf_delete(pContext);
        pContext = nullptr;
    }

    bool Load(ReloadDesc* reload) override
    {
        if (!(reload->mType & (RELOAD_TYPE_RESIZE | RELOAD_TYPE_RENDERTARGET)))
            return true;
        const uint32_t width = (uint32_t)mSettings.mWidth, height = (uint32_t)mSettings.mHeight;
        if (!pContext->resize(width, height))
            return false;
        if (!width || !height)
            return true;
        const hz::TextureDesc depthDesc = {
            .width = width,
            .height = height,
            .depth = 1,
            .arraySize = 1,
            .mipLevels = 1,
            .sampleCount = SAMPLE_COUNT_1,
            .format = TinyImageFormat_D32_SFLOAT,
            .startState = RESOURCE_STATE_DEPTH_WRITE,
            .renderTarget = true,
            .pName = "Renderer.Depth",
        };
        mDepth = pContext->createTexture(depthDesc);
        return (bool)mDepth;
    }
    void Unload(ReloadDesc*) override {}
    void Update(float deltaTime) override
    {
        mLook = float2(0.0f);
        updateInputSystem(deltaTime, (uint32_t)mSettings.mWidth, (uint32_t)mSettings.mHeight);
        if (!mSettings.mFocused)
        {
            setEnableCaptureInput(false);
            mCaptured = false;
            mBoost = false;
            mMovement = float2(0.0f);
            mVertical = 0.0f;
            mLook = float2(0.0f);
            pCamera->moveTo(pCamera->getViewPosition());
        }
        CameraMotionParameters motion;
        motion.maxSpeed = 8.0f;
        motion.acceleration = 40.0f;
        motion.braking = 60.0f;
        motion.movementSpeed = mBoost ? 4.0f : 1.0f;
        pCamera->setMotionParameters(motion);
        // The controller uses +Z forward; the scene uses a right-handed view.
        pCamera->onMove(float2(-mMovement.x, mMovement.y));
        pCamera->onMoveY(mVertical);
        pCamera->onRotate(float2(-mLook.x, mLook.y));
        pCamera->update(TF_MIN(deltaTime, 0.1f));
        vec2 rotation = pCamera->getRotationXY();
        rotation.setX(TF_MAX(-1.553343f, TF_MIN(1.553343f, (float)rotation.getX())));
        pCamera->setViewRotationXY(rotation);
    }
    const char* GetName() override { return "Renderer"; }

    void Draw() override
    {
        if (pContext->isSuspended())
            return;
        hz::CommandList& commands = pContext->acquireCommandList();
        const float      aspectInverse = (float)pContext->getHeight() / (float)pContext->getWidth();
        const float      horizontalFov = 2.0f * atanf(tanf(mVerticalFov * 0.5f) / aspectInverse);
        const FrameData  frame = {
             .viewProjection = Matrix4::perspectiveRH(horizontalFov, aspectInverse, 0.1f, 1000.0f) *
                              Matrix4::scale(Vector3(-1.0f, 1.0f, -1.0f)) * pCamera->getViewMatrix(),
             .eye = Vector4(pCamera->getViewPosition(), 1.0f),
        };
        commands.updateBuffer(mFrame, 0, &frame, sizeof(frame));
        const hz::GPUTexture&    backbuffer = pContext->getCurrentBackbuffer();
        const hz::RenderPassDesc pass = {
            .colorAttachments = { { .pTexture = &backbuffer,
                                    .loadAction = LOAD_ACTION_CLEAR,
                                    .storeAction = STORE_ACTION_STORE,
                                    .clearValue = { .r = 0.16f, .g = 0.24f, .b = 0.34f, .a = 1.0f } } },
            .colorAttachmentCount = 1,
            .depthAttachment = { .pTexture = &mDepth,
                                 .loadAction = LOAD_ACTION_CLEAR,
                                 .storeAction = STORE_ACTION_DONTCARE,
                                 .clearValue = { .depth = 1.0f } },
        };
        commands.beginRendering(pass);
        commands.setViewport(0, 0, (float)pContext->getWidth(), (float)pContext->getHeight());
        commands.setScissor(0, 0, pContext->getWidth(), pContext->getHeight());
        commands.setPipeline(mPipeline);
        commands.bindBuffer("Frame", mFrame);
        commands.bindBuffer("Draws", mDraws);
        commands.bindBuffer("Materials", *getSceneAssetMaterialBuffer(pScenes, mScene));
        commands.bindSampler("SurfaceSampler", mSampler);
        for (uint32_t i = 0; i < pGeometry->mVertexBufferCount; ++i)
        {
            commands.setVertexBuffer(i, pGeometry->mVertexBuffers[i], 0, pGeometry->mVertexStrides[i]);
        }
        commands.setIndexBuffer(pGeometry->mIndexBuffer, 0, pGeometry->mIndexType);
        const SceneAssetGpuMaterial* gpuMaterials = getSceneAssetGpuMaterials(pScenes, mScene);
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
                    commands.bindTexture(names[t], *getSceneAssetTexture(pScenes, mScene, textureIndex));
                }
                previousMaterial = instance.mMaterialIndex;
            }
            commands.setPushConstants(0, &i, sizeof(i));
            const IndirectDrawIndexArguments& draw = pGeometry->pDrawArgs[instance.mDrawIndex];
            commands.drawIndexed(draw.mIndexCount, draw.mStartIndex, draw.mVertexOffset);
        }
        commands.endRendering();
        pContext->submit(commands, &backbuffer);
    }

private:
    enum CameraAction : uint32_t
    {
        Move,
        MoveVertical,
        Look,
        Capture,
        Boost,
        Reset,
        Release
    };

    bool initCamera()
    {
        pCamera = initFpsCameraController(Vector3(mEye), Vector3(mTarget));
        InputSystemDesc inputDesc = { .pWindow = pWindow };
        if (!initInputSystem(&inputDesc))
        {
            exitInputSystem();
            return false;
        }
        mInputInitialized = true;
        ActionMappingDesc mappings[] = {
            { .mActionMappingType = INPUT_ACTION_MAPPING_COMPOSITE,
              .mActionMappingDeviceTarget = INPUT_ACTION_MAPPING_TARGET_KEYBOARD,
              .mActionId = Move,
              .mDeviceButtons = { KEYBOARD_BUTTON_D, KEYBOARD_BUTTON_A, KEYBOARD_BUTTON_W, KEYBOARD_BUTTON_S } },
            { .mActionMappingType = INPUT_ACTION_MAPPING_COMPOSITE,
              .mActionMappingDeviceTarget = INPUT_ACTION_MAPPING_TARGET_KEYBOARD,
              .mActionId = MoveVertical,
              .mDeviceButtons = { KEYBOARD_BUTTON_E, KEYBOARD_BUTTON_Q },
              .mCompositeUseSingleAxis = true },
            { .mActionMappingDeviceTarget = INPUT_ACTION_MAPPING_TARGET_MOUSE,
              .mActionId = Look,
              .mDeviceButtons = { MOUSE_BUTTON_AXIS_X },
              .mNumAxis = 2,
              .mScale = 0.002f,
              .mScaleByDT = true },
            { .mActionMappingDeviceTarget = INPUT_ACTION_MAPPING_TARGET_MOUSE,
              .mActionId = Capture,
              .mDeviceButtons = { MOUSE_BUTTON_RIGHT } },
            { .mActionMappingDeviceTarget = INPUT_ACTION_MAPPING_TARGET_KEYBOARD,
              .mActionId = Boost,
              .mDeviceButtons = { KEYBOARD_BUTTON_SHIFT_L } },
            { .mActionMappingDeviceTarget = INPUT_ACTION_MAPPING_TARGET_KEYBOARD,
              .mActionId = Reset,
              .mDeviceButtons = { KEYBOARD_BUTTON_R } },
            { .mActionMappingDeviceTarget = INPUT_ACTION_MAPPING_TARGET_KEYBOARD,
              .mActionId = Release,
              .mDeviceButtons = { KEYBOARD_BUTTON_ESCAPE } },
        };
        addActionMappings(mappings, TF_ARRAY_COUNT(mappings), INPUT_ACTION_MAPPING_TARGET_ALL);
        for (uint32_t i = 0; i < TF_ARRAY_COUNT(mappings); ++i)
        {
            const InputActionDesc action = { .mActionId = mappings[i].mActionId, .pFunction = onCameraInput, .pUserData = this };
            addInputAction(&action);
        }
        LOGF(eINFO, "Camera: WASD move, Q/E down/up, hold RMB to look, left Shift boost, R reset, Esc release mouse");
        return true;
    }

    static bool onCameraInput(InputActionContext* input)
    {
        RendererApp* app = (RendererApp*)input->pUserData;
        if (!app->mSettings.mFocused)
            return true;
        const bool ended = input->mPhase == INPUT_ACTION_PHASE_CANCELED || input->mPhase == INPUT_ACTION_PHASE_ENDED;
        switch (input->mActionId)
        {
        case Move:
            app->mMovement = ended ? float2(0.0f) : input->mFloat2;
            break;
        case MoveVertical:
            app->mVertical = ended ? 0.0f : input->mFloat;
            break;
        case Look:
            if (app->mCaptured && !ended)
                app->mLook = input->mFloat2;
            break;
        case Capture:
            app->mCaptured = !ended && input->mBool;
            setEnableCaptureInput(app->mCaptured);
            break;
        case Boost:
            app->mBoost = !ended && input->mBool;
            break;
        case Reset:
            if (input->mPhase == INPUT_ACTION_PHASE_STARTED)
            {
                app->pCamera->resetView();
                app->mMovement = float2(0.0f);
                app->mVertical = 0.0f;
                app->mLook = float2(0.0f);
            }
            break;
        case Release:
            if (input->mBool)
            {
                app->mCaptured = false;
                setEnableCaptureInput(false);
            }
            break;
        }
        return true;
    }

    ICameraController*  pCamera = nullptr;
    bool                mInputInitialized = false;
    bool                mCaptured = false;
    bool                mBoost = false;
    float2              mMovement = float2(0.0f);
    float2              mLook = float2(0.0f);
    float               mVertical = 0.0f;
    IFileSystem         mSceneFileSystem = {};
    hz::RenderContext*  pContext = nullptr;
    SceneManager*   pScenes = nullptr;
    SceneAssetHandle    mScene = {};
    const SceneGeometry* pGeometry = nullptr;
    GeometryData*       pGeometryData = nullptr;
    SceneAssetInstance* pInstances = nullptr;
    uint32_t            mInstanceCount = 0;
    hz::GPUBuffer       mFrame;
    hz::GPUBuffer       mDraws;
    hz::GPUTexture      mDepth;
    hz::GPUSampler      mSampler;
    hz::GPUShader       mShader;
    hz::GPUPipeline     mPipeline;
    Point3              mEye;
    Point3              mTarget;
    float               mVerticalFov = 1.04719755f;
};

DEFINE_APPLICATION_MAIN(RendererApp)
