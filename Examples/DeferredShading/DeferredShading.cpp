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
    const hz::GPUBuffer& getVertexBuffer() const { return vertices; }
    const hz::GPUBuffer& getIndexBuffer() const { return indices; }

private:
    hz::GPUBuffer   vertices;
    hz::GPUBuffer   indices;
    hz::GPUPipeline pipeline;
};

class GBufferPass
{
public:
    GBufferPass(hz::RenderContext& context);
    bool                  resize(hz::RenderContext& context, uint32_t width, uint32_t height);
    void                  execute(hz::CommandList& commands, const hz::GPUBuffer& vertices, const hz::GPUBuffer& indices,
                                  const hz::GPUBuffer* sceneUniforms[kSceneObjectCount], uint32_t width, uint32_t height) const;
    const hz::GPUTexture& getAlbedo() const { return albedo; }
    const hz::GPUTexture& getNormal() const { return normal; }
    const hz::GPUTexture& getDepth() const { return depth; }

private:
    hz::GPUTexture  albedo;
    hz::GPUTexture  normal;
    hz::GPUTexture  depth;
    hz::GPUPipeline pipeline;
};

class LightingPass
{
public:
    LightingPass(hz::RenderContext& context, hz::Format surfaceFormat);
    void execute(hz::CommandList& commands, const hz::GPUTexture& backbuffer, const hz::GPUTexture& albedo, const hz::GPUTexture& normal,
                 const hz::GPUTexture& depth, uint32_t width, uint32_t height) const;

private:
    hz::GPUPipeline pipeline;
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

cbuffer RootConstant0 { uint verticesIndex; uint indicesIndex; };

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
    RWStructuredBuffer<GeneratedVertex> GeneratedVertices = ResourceDescriptorHeap[verticesIndex];
    RWStructuredBuffer<uint> GeneratedIndices = ResourceDescriptorHeap[indicesIndex];
    const uint id = dispatchThreadId.x;
    if (id < 28)
        GeneratedVertices[id] = MakeVertex(id);
    if (id < 42)
        GeneratedIndices[id] = MakeIndex(id);
}
)";

GeometryBuildPass::GeometryBuildPass(hz::RenderContext& context)
{
    pipeline = context.createComputePipeline({
        .shaderDesc = {
            .stages = {
                { .stage = SHADER_STAGE_COMP,
                  .pSource = kGeometryBuildShader,
                  .sourceSize = (uint32_t)(sizeof(kGeometryBuildShader) - 1),
                  .pEntryPoint = "CSMain",
                  .pName = "DeferredShadingGeometryBuildCS" },
            },
        },
        .pName = "DeferredShading.GeometryBuildPipeline",
    });
    ASSERT(pipeline.isValid());

    vertices = context.createBuffer({
        .size = sizeof(Vertex) * kGeneratedVertexCount,
        .elementCount = kGeneratedVertexCount,
        .structStride = sizeof(Vertex),
        .pName = "DeferredShading.GeneratedVertices",
        .usage = RESOURCE_MEMORY_USAGE_GPU_ONLY,
        .startState = RESOURCE_STATE_UNORDERED_ACCESS,
        .descriptors = DESCRIPTOR_TYPE_BUFFER | DESCRIPTOR_TYPE_RW_BUFFER | DESCRIPTOR_TYPE_VERTEX_BUFFER,
    });
    indices = context.createBuffer({
        .size = sizeof(uint32_t) * kGeneratedIndexCount,
        .elementCount = kGeneratedIndexCount,
        .structStride = sizeof(uint32_t),
        .pName = "DeferredShading.GeneratedIndices",
        .usage = RESOURCE_MEMORY_USAGE_GPU_ONLY,
        .startState = RESOURCE_STATE_UNORDERED_ACCESS,
        .descriptors = DESCRIPTOR_TYPE_BUFFER | DESCRIPTOR_TYPE_RW_BUFFER | DESCRIPTOR_TYPE_INDEX_BUFFER,
    });
    ASSERT(vertices.isValid() && indices.isValid());
}

void GeometryBuildPass::execute(hz::CommandList& commands) const
{
    commands.beginGpuTimestamp("Build Geometry");
    commands.setPipeline(pipeline);
    const uint32_t resourceIndices[] = { vertices.getUavIndex(), indices.getUavIndex() };
    commands.setPushConstants(0, resourceIndices, sizeof(resourceIndices));
    const hz::GPUBuffer* buffers[] = { &vertices, &indices };
    commands.dispatch(1, 1, 1, { .storageBuffers = buffers });
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

cbuffer RootConstant0 { uint sceneIndex; };

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
    StructuredBuffer<SceneUniforms> SceneUniformBuffer = ResourceDescriptorHeap[sceneIndex];
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
    pipeline = context.createGraphicsPipeline({
        .shaderDesc = {
        .stages = {
            { .stage = SHADER_STAGE_VERT, .pSource = kGBufferShader,
              .sourceSize = (uint32_t)(sizeof(kGBufferShader) - 1), .pEntryPoint = "VSMain",
              .pName = "DeferredShadingGBufferVS" },
            { .stage = SHADER_STAGE_FRAG, .pSource = kGBufferShader,
              .sourceSize = (uint32_t)(sizeof(kGBufferShader) - 1), .pEntryPoint = "PSMain",
              .pName = "DeferredShadingGBufferPS" },
        },
    },
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
        .colorTargets = { { .format = hz::Format::R8G8B8A8_UNORM }, { .format = hz::Format::R16G16B16A16_SFLOAT } },
        .depthStencilFormat = hz::Format::D32_SFLOAT,
        .pName = "DeferredShading.GBufferPipeline",
    });
    ASSERT(pipeline.isValid());
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

    albedo = context.createTexture(albedoDesc);
    normal = context.createTexture(normalDesc);
    depth = context.createTexture(depthDesc);
    if (!albedo.isValid() || !normal.isValid() || !depth.isValid())
        return false;
    return true;
}

void GBufferPass::execute(hz::CommandList& commands, const hz::GPUBuffer& vertices, const hz::GPUBuffer& indices,
                          const hz::GPUBuffer* sceneUniforms[kSceneObjectCount], uint32_t width, uint32_t height) const
{
    const ClearValue black = { .a = 1.0f };
    const ClearValue normalClear = { .r = 0.5f, .g = 0.5f, .b = 1.0f, .a = 1.0f };
    const ClearValue depthClear = { .depth = 1.0f };
    commands.beginGpuTimestamp("GBuffer");
    const hz::GPUBuffer* buffers[] = { &vertices, &indices };
    commands.beginRendering(
        {
            .colorAttachments =  {
                { .pTexture = &albedo, .loadAction = LOAD_ACTION_CLEAR, .storeAction = STORE_ACTION_STORE, .clearValue = black },
                { .pTexture = &normal, .loadAction = LOAD_ACTION_CLEAR, .storeAction = STORE_ACTION_STORE, .clearValue = normalClear },
            } ,
            .depthAttachment = { .pTexture = &depth,
                                 .loadAction = LOAD_ACTION_CLEAR,
                                 .storeAction = STORE_ACTION_STORE,
                                 .clearValue = depthClear },
        },
        { .buffers = buffers });
    commands.setViewport(0.0f, 0.0f, (float)width, (float)height);
    commands.setScissor(0, 0, width, height);
    commands.setPipeline(pipeline);
    commands.setVertexBuffer(0, vertices, 0, sizeof(Vertex));
    commands.setIndexBuffer(indices, 0, INDEX_TYPE_UINT32);
    const uint32_t sceneIndex0 = sceneUniforms[0]->getSrvIndex();
    commands.setPushConstants(0, &sceneIndex0, sizeof(sceneIndex0));
    commands.drawIndexed(kCubeIndexCount);
    const uint32_t sceneIndex1 = sceneUniforms[1]->getSrvIndex();
    commands.setPushConstants(0, &sceneIndex1, sizeof(sceneIndex1));
    commands.drawIndexed(kPlaneIndexCount, kPlaneFirstIndex, kPlaneFirstVertex);
    commands.endRendering();
    commands.endGpuTimestamp();
}

constexpr char kLightingShader[] = R"(
cbuffer RootConstant0 { uint albedoIndex; uint normalIndex; uint depthIndex; };

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
    Texture2D<float4> AlbedoTexture = ResourceDescriptorHeap[albedoIndex];
    Texture2D<float4> NormalTexture = ResourceDescriptorHeap[normalIndex];
    Texture2D<float> DepthTexture = ResourceDescriptorHeap[depthIndex];
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
    pipeline = context.createGraphicsPipeline({
        .shaderDesc = {
        .stages = {
            { .stage = SHADER_STAGE_VERT, .pSource = kLightingShader,
              .sourceSize = (uint32_t)(sizeof(kLightingShader) - 1), .pEntryPoint = "VSMain",
              .pName = "DeferredShadingLightingVS" },
            { .stage = SHADER_STAGE_FRAG, .pSource = kLightingShader,
              .sourceSize = (uint32_t)(sizeof(kLightingShader) - 1), .pEntryPoint = "PSMain",
              .pName = "DeferredShadingLightingPS" },
        },
    },
        .colorTargets = { { .format = surfaceFormat } },
        .pName = "DeferredShading.LightingPipeline",
    });
    ASSERT(pipeline.isValid());
}

void LightingPass::execute(hz::CommandList& commands, const hz::GPUTexture& backbuffer, const hz::GPUTexture& albedo,
                           const hz::GPUTexture& normal, const hz::GPUTexture& depth, uint32_t width, uint32_t height) const
{
    const ClearValue clear = { .r = 0.02f, .g = 0.025f, .b = 0.03f, .a = 1.0f };
    commands.beginGpuTimestamp("Lighting");
    const hz::GPUTexture* sampledTextures[] = { &albedo, &normal, &depth };
    commands.beginRendering(
        {
            .colorAttachments = { {
                .pTexture = &backbuffer,
                .loadAction = LOAD_ACTION_CLEAR,
                .storeAction = STORE_ACTION_STORE,
                .clearValue = clear,
            } },
        },
        { .sampledTextures = sampledTextures });
    commands.setViewport(0.0f, 0.0f, (float)width, (float)height);
    commands.setScissor(0, 0, width, height);
    commands.setPipeline(pipeline);
    const uint32_t resourceIndices[] = { albedo.getSrvIndex(), normal.getSrvIndex(), depth.getSrvIndex() };
    commands.setPushConstants(0, resourceIndices, sizeof(resourceIndices));
    commands.draw(3);
    commands.endRendering();
    commands.endGpuTimestamp();
}

constexpr uint32_t   kCpuProfileColor = 0x88CC44;
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
        elapsedTime += deltaTime;
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
        cube.world = Matrix4::translation(Vector3(0.0f, 1.15f, 0.0f)) * Matrix4::rotationY(elapsedTime) *
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
    float                              elapsedTime = 0.0f;
};

DEFINE_APPLICATION_MAIN(DeferredShadingApp)
