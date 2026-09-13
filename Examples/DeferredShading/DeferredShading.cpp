#include <cmath>
#include <cstdint>
#include <memory>

#include <cstddef>

#include "Application/IApp.h"
#include "Core/ILog.h"
#include "Core/IMath.h"
#include "Graphics/RenderContext.h"
#include "Profiler/IProfiler.h"

constexpr uint32_t kSceneObjectCount = 2;

struct Vertex
{
    Vector3 position;
    Vector3 normal;
    Vector3 color;
};

struct SceneUniforms
{
    Matrix4 worldViewProjection;
    Matrix4 world;
};

class GeometryBuildPass
{
public:
    GeometryBuildPass(hz::RenderContext& context);
    void                 execute(hz::CommandList& commands) const;
    const hz::GPUBuffer& getVertexBuffer() const { return mVertices; }
    const hz::GPUBuffer& getIndexBuffer() const { return mIndices; }

private:
    hz::GPUBuffer   mVertices;
    hz::GPUBuffer   mIndices;
    hz::GPUShader   mShader;
    hz::GPUPipeline mPipeline;
};

class GBufferPass
{
public:
    GBufferPass(hz::RenderContext& context);
    bool                  resize(hz::RenderContext& context, uint32_t width, uint32_t height);
    void                  execute(hz::CommandList& commands, const hz::GPUBuffer& vertices, const hz::GPUBuffer& indices,
                                  const hz::GPUBuffer* sceneUniforms[kSceneObjectCount], uint32_t width, uint32_t height) const;
    const hz::GPUTexture& getAlbedo() const { return mAlbedo; }
    const hz::GPUTexture& getNormal() const { return mNormal; }
    const hz::GPUTexture& getDepth() const { return mDepth; }

private:
    hz::GPUTexture  mAlbedo;
    hz::GPUTexture  mNormal;
    hz::GPUTexture  mDepth;
    hz::GPUShader   mShader;
    hz::GPUPipeline mPipeline;
};

class LightingPass
{
public:
    LightingPass(hz::RenderContext& context, hz::Format surfaceFormat);
    void execute(hz::CommandList& commands, const hz::GPUTexture& backbuffer, const hz::GPUTexture& albedo, const hz::GPUTexture& normal,
                 const hz::GPUTexture& depth, uint32_t width, uint32_t height) const;

private:
    hz::GPUShader   mShader;
    hz::GPUPipeline mPipeline;
};

constexpr uint32_t kGeneratedVertexCount = 28;
constexpr uint32_t kGeneratedIndexCount = 42;

constexpr char kGeometryBuildShader[] = R"(
struct GeneratedVertex
{
    float4 Position;
    float4 Normal;
    float4 Color;
};

struct GeneratedVertexData
{
    float3 Position;
    float3 Normal;
    float3 Color;
};

RWStructuredBuffer<GeneratedVertex> GeneratedVertices : register(u0);
RWStructuredBuffer<uint> GeneratedIndices : register(u1);

GeneratedVertex PackVertex(GeneratedVertexData source)
{
    GeneratedVertex v;
    v.Position = float4(source.Position, 1.0f);
    v.Normal = float4(source.Normal, 0.0f);
    v.Color = float4(source.Color, 1.0f);
    return v;
}

GeneratedVertex MakeVertex(uint id)
{
    GeneratedVertexData v;
    v.Position = float3(0.0f, 0.0f, 0.0f);
    v.Normal = float3(0.0f, 1.0f, 0.0f);
    v.Color = float3(1.0f, 1.0f, 1.0f);

    switch (id)
    {
    case 0: v.Position = float3(-1.0f, -1.0f, 1.0f); v.Normal = float3(0.0f, 0.0f, 1.0f); v.Color = float3(0.85f, 0.18f, 0.12f); break;
    case 1: v.Position = float3(1.0f, -1.0f, 1.0f); v.Normal = float3(0.0f, 0.0f, 1.0f); v.Color = float3(0.85f, 0.18f, 0.12f); break;
    case 2: v.Position = float3(1.0f, 1.0f, 1.0f); v.Normal = float3(0.0f, 0.0f, 1.0f); v.Color = float3(0.85f, 0.18f, 0.12f); break;
    case 3: v.Position = float3(-1.0f, 1.0f, 1.0f); v.Normal = float3(0.0f, 0.0f, 1.0f); v.Color = float3(0.85f, 0.18f, 0.12f); break;
    case 4: v.Position = float3(1.0f, -1.0f, -1.0f); v.Normal = float3(0.0f, 0.0f, -1.0f); v.Color = float3(0.12f, 0.38f, 0.85f); break;
    case 5: v.Position = float3(-1.0f, -1.0f, -1.0f); v.Normal = float3(0.0f, 0.0f, -1.0f); v.Color = float3(0.12f, 0.38f, 0.85f); break;
    case 6: v.Position = float3(-1.0f, 1.0f, -1.0f); v.Normal = float3(0.0f, 0.0f, -1.0f); v.Color = float3(0.12f, 0.38f, 0.85f); break;
    case 7: v.Position = float3(1.0f, 1.0f, -1.0f); v.Normal = float3(0.0f, 0.0f, -1.0f); v.Color = float3(0.12f, 0.38f, 0.85f); break;
    case 8: v.Position = float3(-1.0f, -1.0f, -1.0f); v.Normal = float3(-1.0f, 0.0f, 0.0f); v.Color = float3(0.18f, 0.72f, 0.36f); break;
    case 9: v.Position = float3(-1.0f, -1.0f, 1.0f); v.Normal = float3(-1.0f, 0.0f, 0.0f); v.Color = float3(0.18f, 0.72f, 0.36f); break;
    case 10: v.Position = float3(-1.0f, 1.0f, 1.0f); v.Normal = float3(-1.0f, 0.0f, 0.0f); v.Color = float3(0.18f, 0.72f, 0.36f); break;
    case 11: v.Position = float3(-1.0f, 1.0f, -1.0f); v.Normal = float3(-1.0f, 0.0f, 0.0f); v.Color = float3(0.18f, 0.72f, 0.36f); break;
    case 12: v.Position = float3(1.0f, -1.0f, 1.0f); v.Normal = float3(1.0f, 0.0f, 0.0f); v.Color = float3(0.92f, 0.66f, 0.20f); break;
    case 13: v.Position = float3(1.0f, -1.0f, -1.0f); v.Normal = float3(1.0f, 0.0f, 0.0f); v.Color = float3(0.92f, 0.66f, 0.20f); break;
    case 14: v.Position = float3(1.0f, 1.0f, -1.0f); v.Normal = float3(1.0f, 0.0f, 0.0f); v.Color = float3(0.92f, 0.66f, 0.20f); break;
    case 15: v.Position = float3(1.0f, 1.0f, 1.0f); v.Normal = float3(1.0f, 0.0f, 0.0f); v.Color = float3(0.92f, 0.66f, 0.20f); break;
    case 16: v.Position = float3(-1.0f, 1.0f, 1.0f); v.Normal = float3(0.0f, 1.0f, 0.0f); v.Color = float3(0.75f, 0.28f, 0.88f); break;
    case 17: v.Position = float3(1.0f, 1.0f, 1.0f); v.Normal = float3(0.0f, 1.0f, 0.0f); v.Color = float3(0.75f, 0.28f, 0.88f); break;
    case 18: v.Position = float3(1.0f, 1.0f, -1.0f); v.Normal = float3(0.0f, 1.0f, 0.0f); v.Color = float3(0.75f, 0.28f, 0.88f); break;
    case 19: v.Position = float3(-1.0f, 1.0f, -1.0f); v.Normal = float3(0.0f, 1.0f, 0.0f); v.Color = float3(0.75f, 0.28f, 0.88f); break;
    case 20: v.Position = float3(-1.0f, -1.0f, -1.0f); v.Normal = float3(0.0f, -1.0f, 0.0f); v.Color = float3(0.20f, 0.68f, 0.78f); break;
    case 21: v.Position = float3(1.0f, -1.0f, -1.0f); v.Normal = float3(0.0f, -1.0f, 0.0f); v.Color = float3(0.20f, 0.68f, 0.78f); break;
    case 22: v.Position = float3(1.0f, -1.0f, 1.0f); v.Normal = float3(0.0f, -1.0f, 0.0f); v.Color = float3(0.20f, 0.68f, 0.78f); break;
    case 23: v.Position = float3(-1.0f, -1.0f, 1.0f); v.Normal = float3(0.0f, -1.0f, 0.0f); v.Color = float3(0.20f, 0.68f, 0.78f); break;
    case 24: v.Position = float3(-5.0f, 0.0f, -5.0f); v.Normal = float3(0.0f, 1.0f, 0.0f); v.Color = float3(0.42f, 0.48f, 0.50f); break;
    case 25: v.Position = float3(5.0f, 0.0f, -5.0f); v.Normal = float3(0.0f, 1.0f, 0.0f); v.Color = float3(0.42f, 0.48f, 0.50f); break;
    case 26: v.Position = float3(5.0f, 0.0f, 5.0f); v.Normal = float3(0.0f, 1.0f, 0.0f); v.Color = float3(0.42f, 0.48f, 0.50f); break;
    case 27: v.Position = float3(-5.0f, 0.0f, 5.0f); v.Normal = float3(0.0f, 1.0f, 0.0f); v.Color = float3(0.42f, 0.48f, 0.50f); break;
    }

    return PackVertex(v);
}

uint MakeIndex(uint id)
{
    switch (id)
    {
    case 0: return 0; case 1: return 1; case 2: return 2; case 3: return 0; case 4: return 2; case 5: return 3;
    case 6: return 4; case 7: return 5; case 8: return 6; case 9: return 4; case 10: return 6; case 11: return 7;
    case 12: return 8; case 13: return 9; case 14: return 10; case 15: return 8; case 16: return 10; case 17: return 11;
    case 18: return 12; case 19: return 13; case 20: return 14; case 21: return 12; case 22: return 14; case 23: return 15;
    case 24: return 16; case 25: return 17; case 26: return 18; case 27: return 16; case 28: return 18; case 29: return 19;
    case 30: return 20; case 31: return 21; case 32: return 22; case 33: return 20; case 34: return 22; case 35: return 23;
    case 36: return 0; case 37: return 1; case 38: return 2; case 39: return 0; case 40: return 2; case 41: return 3;
    default: return 0;
    }
}

[numthreads(64, 1, 1)]
void CSMain(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    const uint id = dispatchThreadId.x;
    if (id < 28)
        GeneratedVertices[id] = MakeVertex(id);
    if (id < 42)
        GeneratedIndices[id] = MakeIndex(id);
}
)";

GeometryBuildPass::GeometryBuildPass(hz::RenderContext& context)
{
    mShader = context.createShader({
        .stages = { { .stage = SHADER_STAGE_COMP,
                      .pSource = kGeometryBuildShader,
                      .sourceSize = (uint32_t)(sizeof(kGeometryBuildShader) - 1),
                      .pEntryPoint = "CSMain",
                      .pName = "DeferredShadingGeometryBuildCS" } },
        .stageCount = 1,
    });
    ASSERT(mShader.isValid());
    mPipeline = context.createComputePipeline({ .pShader = &mShader, .pName = "DeferredShading.GeometryBuildPipeline" });
    ASSERT(mPipeline.isValid());

    mVertices = context.createBuffer({
        .size = sizeof(Vertex) * kGeneratedVertexCount,
        .elementCount = kGeneratedVertexCount,
        .structStride = sizeof(Vertex),
        .pName = "DeferredShading.GeneratedVertices",
        .usage = RESOURCE_MEMORY_USAGE_GPU_ONLY,
        .startState = RESOURCE_STATE_UNORDERED_ACCESS,
        .descriptors = DESCRIPTOR_TYPE_BUFFER | DESCRIPTOR_TYPE_RW_BUFFER | DESCRIPTOR_TYPE_VERTEX_BUFFER,
    });
    mIndices = context.createBuffer({
        .size = sizeof(uint32_t) * kGeneratedIndexCount,
        .elementCount = kGeneratedIndexCount,
        .structStride = sizeof(uint32_t),
        .pName = "DeferredShading.GeneratedIndices",
        .usage = RESOURCE_MEMORY_USAGE_GPU_ONLY,
        .startState = RESOURCE_STATE_UNORDERED_ACCESS,
        .descriptors = DESCRIPTOR_TYPE_BUFFER | DESCRIPTOR_TYPE_RW_BUFFER | DESCRIPTOR_TYPE_INDEX_BUFFER,
    });
    ASSERT(mVertices.isValid() && mIndices.isValid());
}

void GeometryBuildPass::execute(hz::CommandList& commands) const
{
    commands.beginGpuTimestamp("Build Geometry");
    commands.setPipeline(mPipeline);
    commands.bindBuffer("GeneratedVertices", mVertices);
    commands.bindBuffer("GeneratedIndices", mIndices);
    const hz::GPUBuffer* buffers[] = { &mVertices, &mIndices };
    commands.dispatch(1, 1, 1, { .buffers = buffers });
    commands.endGpuTimestamp();
}

constexpr uint32_t kCubeIndexCount = 36;
constexpr uint32_t kPlaneIndexCount = 6;
constexpr uint32_t kPlaneFirstIndex = 36;
constexpr uint32_t kPlaneFirstVertex = 24;

constexpr char kGBufferShader[] = R"(
#pragma pack_matrix(column_major)

struct SceneUniforms
{
    float4x4 WorldViewProjection;
    float4x4 World;
};

StructuredBuffer<SceneUniforms> SceneUniformBuffer : register(t0);

struct VSInput
{
    float3 Position : POSITION;
    float3 Normal : NORMAL;
    float3 Color : COLOR;
};

struct VSOutput
{
    float4 Position : SV_Position;
    float3 Normal : NORMAL;
    float3 Color : COLOR;
};

VSOutput VSMain(VSInput input)
{
    SceneUniforms scene = SceneUniformBuffer[0];
    VSOutput output;
    output.Position = mul(scene.WorldViewProjection, float4(input.Position, 1.0f));
    output.Normal = normalize(mul(scene.World, float4(input.Normal, 0.0f)).xyz);
    output.Color = input.Color;
    return output;
}

struct GBufferOutput
{
    float4 Albedo : SV_Target0;
    float4 Normal : SV_Target1;
};

GBufferOutput PSMain(VSOutput input)
{
    GBufferOutput output;
    output.Albedo = float4(input.Color, 1.0f);
    output.Normal = float4(normalize(input.Normal) * 0.5f + 0.5f, 1.0f);
    return output;
}
)";

GBufferPass::GBufferPass(hz::RenderContext& context)
{
    mShader = context.createShader({
        .stages = {
            { .stage = SHADER_STAGE_VERT, .pSource = kGBufferShader,
              .sourceSize = (uint32_t)(sizeof(kGBufferShader) - 1), .pEntryPoint = "VSMain",
              .pName = "DeferredShadingGBufferVS" },
            { .stage = SHADER_STAGE_FRAG, .pSource = kGBufferShader,
              .sourceSize = (uint32_t)(sizeof(kGBufferShader) - 1), .pEntryPoint = "PSMain",
              .pName = "DeferredShadingGBufferPS" },
        },
        .stageCount = 2,
    });
    ASSERT(mShader.isValid());
    mPipeline = context.createGraphicsPipeline({
        .pShader = &mShader,
        .vertexLayout = {
            .bindings = { { .stride = sizeof(Vertex), .rate = VERTEX_BINDING_RATE_VERTEX } },
            .attribs = {
                { .semantic = SEMANTIC_POSITION, .format = hz::Format::R32G32B32_SFLOAT, .binding = 0, .location = 0,
                  .offset = (uint32_t)offsetof(Vertex, position) },
                { .semantic = SEMANTIC_NORMAL, .format = hz::Format::R32G32B32_SFLOAT, .binding = 0, .location = 1,
                  .offset = (uint32_t)offsetof(Vertex, normal) },
                { .semantic = SEMANTIC_COLOR, .format = hz::Format::R32G32B32_SFLOAT, .binding = 0, .location = 2,
                  .offset = (uint32_t)offsetof(Vertex, color) },
            },
            .bindingCount = 1,
            .attribCount = 3,
        },
        .depth = { .depthTest = true, .depthWrite = true, .depthFunc = CMP_LEQUAL },
        .colorFormats = { hz::Format::R8G8B8A8_UNORM, hz::Format::R16G16B16A16_SFLOAT },
        .renderTargetCount = 2,
        .depthStencilFormat = hz::Format::D32_SFLOAT,
        .pName = "DeferredShading.GBufferPipeline",
    });
    ASSERT(mPipeline.isValid());
}

bool GBufferPass::resize(hz::RenderContext& context, uint32_t width, uint32_t height)
{
    hz::TextureDesc albedoDesc = {
        .width = width,
        .height = height,
        .format = hz::Format::R8G8B8A8_UNORM,
        .startState = RESOURCE_STATE_RENDER_TARGET,
        .descriptors = DESCRIPTOR_TYPE_TEXTURE,
        .renderTarget = true,
        .pName = "GBuffer.Albedo",
    };
    hz::TextureDesc normalDesc = albedoDesc;
    normalDesc.format = hz::Format::R16G16B16A16_SFLOAT;
    normalDesc.pName = "GBuffer.Normal";
    hz::TextureDesc depthDesc = albedoDesc;
    depthDesc.format = hz::Format::D32_SFLOAT;
    depthDesc.startState = RESOURCE_STATE_DEPTH_WRITE;
    depthDesc.pName = "GBuffer.Depth";

    mAlbedo = context.createTexture(albedoDesc);
    mNormal = context.createTexture(normalDesc);
    mDepth = context.createTexture(depthDesc);
    if (!mAlbedo.isValid() || !mNormal.isValid() || !mDepth.isValid())
        return false;
    return true;
}

void GBufferPass::execute(hz::CommandList& commands, const hz::GPUBuffer& vertices, const hz::GPUBuffer& indices,
                          const hz::GPUBuffer* sceneUniforms[kSceneObjectCount], uint32_t width, uint32_t height) const
{
    const ClearValue black = { .a = 1.0f };
    const ClearValue normal = { .r = 0.5f, .g = 0.5f, .b = 1.0f, .a = 1.0f };
    const ClearValue depth = { .depth = 1.0f };
    hz::RenderPassDesc pass = {
        .colorAttachments = {
            { .pTexture = &mAlbedo, .loadAction = LOAD_ACTION_CLEAR, .storeAction = STORE_ACTION_STORE,
              .clearValue = black },
            { .pTexture = &mNormal, .loadAction = LOAD_ACTION_CLEAR, .storeAction = STORE_ACTION_STORE,
              .clearValue = normal },
        },
        .colorAttachmentCount = 2,
        .depthAttachment = { .pTexture = &mDepth, .loadAction = LOAD_ACTION_CLEAR,
                              .storeAction = STORE_ACTION_STORE, .clearValue = depth },
    };
    commands.beginGpuTimestamp("GBuffer");
    const hz::GPUBuffer* buffers[] = { &vertices, &indices };
    commands.beginRendering(pass, { .buffers = buffers });
    commands.setViewport(0.0f, 0.0f, (float)width, (float)height);
    commands.setScissor(0, 0, width, height);
    commands.setPipeline(mPipeline);
    commands.setVertexBuffer(0, vertices, 0, sizeof(Vertex));
    commands.setIndexBuffer(indices, 0, INDEX_TYPE_UINT32);
    commands.bindBuffer("SceneUniformBuffer", *sceneUniforms[0]);
    commands.drawIndexed(kCubeIndexCount);
    commands.bindBuffer("SceneUniformBuffer", *sceneUniforms[1]);
    commands.drawIndexed(kPlaneIndexCount, kPlaneFirstIndex, kPlaneFirstVertex);
    commands.endRendering();
    commands.endGpuTimestamp();
}

constexpr char kLightingShader[] = R"(
Texture2D<float4> AlbedoTexture : register(t0);
Texture2D<float4> NormalTexture : register(t1);
Texture2D<float> DepthTexture : register(t2);

struct VSOutput
{
    float4 Position : SV_Position;
};

VSOutput VSMain(uint vertexId : SV_VertexID)
{
    float2 positions[3] = {
        float2(-1.0f, -1.0f),
        float2(-1.0f, 3.0f),
        float2(3.0f, -1.0f),
    };
    VSOutput output;
    output.Position = float4(positions[vertexId], 0.0f, 1.0f);
    return output;
}

float4 PSMain(VSOutput input) : SV_Target0
{
    int2 pixel = int2(input.Position.xy);
    float4 albedo = AlbedoTexture.Load(int3(pixel, 0));
    float3 normal = normalize(NormalTexture.Load(int3(pixel, 0)).xyz * 2.0f - 1.0f);
    float depth = DepthTexture.Load(int3(pixel, 0));

    float3 lightDir = normalize(float3(0.45f, 0.85f, -0.25f));
    float ndotl = saturate(dot(normal, lightDir));
    float3 color = albedo.rgb * (0.16f + ndotl * 0.84f);
    color += saturate(1.0f - depth) * 0.04f;
    float geometryMask = saturate(dot(albedo.rgb, float3(32.0f, 32.0f, 32.0f)));
    color = lerp(float3(0.02f, 0.025f, 0.03f), color, geometryMask);
    return float4(color, 1.0f);
}
)";

LightingPass::LightingPass(hz::RenderContext& context, hz::Format surfaceFormat)
{
    mShader = context.createShader({
        .stages = {
            { .stage = SHADER_STAGE_VERT, .pSource = kLightingShader,
              .sourceSize = (uint32_t)(sizeof(kLightingShader) - 1), .pEntryPoint = "VSMain",
              .pName = "DeferredShadingLightingVS" },
            { .stage = SHADER_STAGE_FRAG, .pSource = kLightingShader,
              .sourceSize = (uint32_t)(sizeof(kLightingShader) - 1), .pEntryPoint = "PSMain",
              .pName = "DeferredShadingLightingPS" },
        },
        .stageCount = 2,
    });
    ASSERT(mShader.isValid());
    mPipeline = context.createGraphicsPipeline({
        .pShader = &mShader,
        .colorFormats = { surfaceFormat },
        .renderTargetCount = 1,
        .pName = "DeferredShading.LightingPipeline",
    });
    ASSERT(mPipeline.isValid());
}

void LightingPass::execute(hz::CommandList& commands, const hz::GPUTexture& backbuffer, const hz::GPUTexture& albedo,
                           const hz::GPUTexture& normal, const hz::GPUTexture& depth, uint32_t width, uint32_t height) const
{
    const ClearValue   clear = { .r = 0.02f, .g = 0.025f, .b = 0.03f, .a = 1.0f };
    hz::RenderPassDesc pass = {
        .colorAttachments = { {
            .pTexture = &backbuffer,
            .loadAction = LOAD_ACTION_CLEAR,
            .storeAction = STORE_ACTION_STORE,
            .clearValue = clear,
        } },
        .colorAttachmentCount = 1,
    };
    commands.beginGpuTimestamp("Lighting");
    const hz::GPUTexture* sampledTextures[] = { &albedo, &normal, &depth };
    commands.beginRendering(pass, { .sampledTextures = sampledTextures });
    commands.setViewport(0.0f, 0.0f, (float)width, (float)height);
    commands.setScissor(0, 0, width, height);
    commands.setPipeline(mPipeline);
    commands.bindTexture("AlbedoTexture", albedo);
    commands.bindTexture("NormalTexture", normal);
    commands.bindTexture("DepthTexture", depth);
    commands.draw(3);
    commands.endRendering();
    commands.endGpuTimestamp();
}

constexpr uint32_t        kCpuProfileColor = 0x88CC44;
constexpr hz::Format kSurfaceFormat = hz::Format::B8G8R8A8_SRGB;

class DeferredShadingApp final: public IApp
{
public:
    DeferredShadingApp()
    {
        settings.width = 1280;
        settings.height = 720;
        settings.vSyncEnabled = true;
        settings.showPlatformUI = false;
    }

    bool Init() override
    {
        PROFILER_SET_CPU_SCOPE("DeferredShading", "Init", kCpuProfileColor);
        hz::ContextDesc contextDesc = {
            .pAppName = GetName(),
            .windowHandle = pWindow->handle,
            .width = 0,
            .height = 0,
            .imageCount = 2,
            .colorFormat = kSurfaceFormat,
            .colorSpace = COLOR_SPACE_SDR_SRGB,
            .enableVSync = settings.vSyncEnabled,
            .enableGpuValidation = true,
            .enableGpuProfiler = true,
        };
        context = std::make_unique<hz::RenderContext>(contextDesc);
        geometryBuildPass = std::make_unique<GeometryBuildPass>(*context);
        gBufferPass = std::make_unique<GBufferPass>(*context);
        lightingPass = std::make_unique<LightingPass>(*context, hz::Format::R8G8B8A8_SRGB);

        createUniformBuffers();
        return true;
    }

    void Exit() override
    {
        PROFILER_SET_CPU_SCOPE("DeferredShading", "Exit", kCpuProfileColor);
        context->waitIdle();
        lightingPass.reset();
        gBufferPass.reset();
        geometryBuildPass.reset();
        for (hz::GPUBuffer& buffer : sceneUniformBuffers)
            buffer = {};
        context.reset();
    }

    bool Load(ReloadDesc* pReloadDesc) override
    {
        PROFILER_SET_CPU_SCOPE("DeferredShading", "Load", kCpuProfileColor);
        if (!(pReloadDesc->type & (RELOAD_TYPE_RESIZE | RELOAD_TYPE_RENDERTARGET)))
            return true;
        const uint32_t width = (uint32_t)settings.width;
        const uint32_t height = (uint32_t)settings.height;
        if (!context->resize(width, height))
            return false;
        if (!gBufferPass->resize(*context, width, height))
            return false;
        return true;
    }

    void Unload(ReloadDesc*) override {}

    void Update(float deltaTime) override
    {
        PROFILER_SET_CPU_SCOPE("DeferredShading", "Update", kCpuProfileColor);
        mElapsedTime += deltaTime;
    }

    void Draw() override
    {
        PROFILER_SET_CPU_SCOPE("DeferredShading", "Draw", kCpuProfileColor);

        if (context->isSuspended())
            return;
        hz::CommandList& commands = context->acquireCommandList();
        updateSceneUniforms(commands);

        const hz::GPUTexture& backbuffer = context->getCurrentBackbuffer();
        const hz::GPUBuffer*  sceneUniforms[kSceneObjectCount] = {
            &sceneUniformBuffers[0],
            &sceneUniformBuffers[1],
        };
        geometryBuildPass->execute(commands);
        gBufferPass->execute(commands, geometryBuildPass->getVertexBuffer(), geometryBuildPass->getIndexBuffer(), sceneUniforms,
                             context->getWidth(), context->getHeight());
        lightingPass->execute(commands, backbuffer, gBufferPass->getAlbedo(), gBufferPass->getNormal(), gBufferPass->getDepth(),
                              context->getWidth(), context->getHeight());
        context->submit(commands, &backbuffer);
    }

    const char* GetName() override { return "DeferredShading"; }

private:
    void createUniformBuffers()
    {
        for (uint32_t object = 0; object < kSceneObjectCount; ++object)
        {
            sceneUniformBuffers[object] = context->createBuffer({
                .size = sizeof(SceneUniforms),
                .elementCount = 1,
                .structStride = sizeof(SceneUniforms),
                .pName = object == 0 ? "DeferredShading.CubeUniforms" : "DeferredShading.FloorUniforms",
                .usage = RESOURCE_MEMORY_USAGE_GPU_ONLY,
                .startState = RESOURCE_STATE_SHADER_RESOURCE,
                .descriptors = DESCRIPTOR_TYPE_BUFFER,
            });
            ASSERT(sceneUniformBuffers[object].isValid());
        }
    }

    void updateSceneUniforms(hz::CommandList& commands)
    {
        const float   aspectInverse = (float)settings.height / (float)settings.width;
        const Matrix4 view = Matrix4::lookAtLH(Point3(3.5f, 3.0f, -6.0f), Point3(0.0f, 0.7f, 0.0f), Vector3(0.0f, 1.0f, 0.0f));
        const float   verticalFov = 60.0f * 3.1415926535f / 180.0f;
        const float   horizontalFov = 2.0f * std::atan(std::tan(verticalFov * 0.5f) / aspectInverse);
        const Matrix4 projection = Matrix4::perspectiveLH(horizontalFov, aspectInverse, 0.1f, 100.0f);
        const Matrix4 viewProjection = projection * view;

        SceneUniforms cube = {};
        cube.world = Matrix4::translation(Vector3(0.0f, 1.15f, 0.0f)) * Matrix4::rotationY(mElapsedTime) *
                     Matrix4::scale(Vector3(0.85f, 0.85f, 0.85f));
        cube.worldViewProjection = viewProjection * cube.world;
        commands.updateBuffer(sceneUniformBuffers[0], 0, &cube, sizeof(cube));

        SceneUniforms floor = {};
        floor.world = Matrix4::identity();
        floor.worldViewProjection = viewProjection * floor.world;
        commands.updateBuffer(sceneUniformBuffers[1], 0, &floor, sizeof(floor));
    }

    std::unique_ptr<hz::RenderContext> context;
    hz::GPUBuffer                      sceneUniformBuffers[kSceneObjectCount];
    std::unique_ptr<GeometryBuildPass> geometryBuildPass;
    std::unique_ptr<GBufferPass>       gBufferPass;
    std::unique_ptr<LightingPass>      lightingPass;
    float                              mElapsedTime = 0.0f;
};

DEFINE_APPLICATION_MAIN(DeferredShadingApp)
