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
    const hz::GPUBuffer& GetVertexBuffer() const { return vertices; }
    const hz::GPUBuffer& GetIndexBuffer() const { return indices; }

private:
    hz::GPUBuffer   vertices;
    hz::GPUBuffer   indices;
    hz::GPUPipeline pipeline;
};

class GBufferPass
{
public:
    GBufferPass(hz::RenderContext& context);
    bool                  Resize(hz::RenderContext& context, uint32_t Width, uint32_t Height);
    void                  execute(hz::CommandList& commands, const hz::GPUBuffer& vertices, const hz::GPUBuffer& indices,
                                  const hz::GPUBuffer* sceneUniforms[kSceneObjectCount], uint32_t Width, uint32_t Height) const;
    const hz::GPUTexture& getAlbedo() const { return albedo; }
    const hz::GPUTexture& getNormal() const { return normal; }
    const hz::GPUTexture& getDepth() const { return Depth; }

private:
    hz::GPUTexture  albedo;
    hz::GPUTexture  normal;
    hz::GPUTexture  Depth;
    hz::GPUPipeline pipeline;
};

class LightingPass
{
public:
    LightingPass(hz::RenderContext& context, hz::Format surfaceFormat);
    void execute(hz::CommandList& commands, const hz::GPUTexture& backbuffer, const hz::GPUTexture& albedo, const hz::GPUTexture& normal,
                 const hz::GPUTexture& Depth, uint32_t Width, uint32_t Height) const;

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
    pipeline = context.CreateComputePipeline({
        .shaderDesc = {
            .stages = {
                { .stage = SHADER_STAGE_COMP,
                  .pSource = kGeometryBuildShader,
                  .sourceSize = (uint32_t)(sizeof(kGeometryBuildShader) - 1),
                  .pEntryPoint = "CSMain",
                  .Name = "DeferredShadingGeometryBuildCS" },
            },
        },
        .Name = "DeferredShading.GeometryBuildPipeline",
    });
    ASSERT(pipeline.IsValid());

    vertices = context.CreateBuffer({
        .Size = sizeof(Vertex) * kGeneratedVertexCount,
        .ElementCount = kGeneratedVertexCount,
        .StructStride = sizeof(Vertex),
        .Name = "DeferredShading.GeneratedVertices",
        .usage = RESOURCE_MEMORY_USAGE_GPU_ONLY,
        .StartState = RESOURCE_STATE_UNORDERED_ACCESS,
        .Descriptors = DESCRIPTOR_TYPE_BUFFER | DESCRIPTOR_TYPE_RW_BUFFER | DESCRIPTOR_TYPE_VERTEX_BUFFER,
    });
    indices = context.CreateBuffer({
        .Size = sizeof(uint32_t) * kGeneratedIndexCount,
        .ElementCount = kGeneratedIndexCount,
        .StructStride = sizeof(uint32_t),
        .Name = "DeferredShading.GeneratedIndices",
        .usage = RESOURCE_MEMORY_USAGE_GPU_ONLY,
        .StartState = RESOURCE_STATE_UNORDERED_ACCESS,
        .Descriptors = DESCRIPTOR_TYPE_BUFFER | DESCRIPTOR_TYPE_RW_BUFFER | DESCRIPTOR_TYPE_INDEX_BUFFER,
    });
    ASSERT(vertices.IsValid() && indices.IsValid());
}

void GeometryBuildPass::execute(hz::CommandList& commands) const
{
    commands.BeginGpuTimestamp("Build Geometry");
    commands.SetPipeline(pipeline);
    commands.SetRootConstant({ vertices.GetUavIndex(), indices.GetUavIndex() });
    const hz::GPUBuffer* buffers[] = { &vertices, &indices };
    commands.Dispatch(1, 1, 1, { .storageBuffers = buffers });
    commands.EndGpuTimestamp();
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
    pipeline = context.CreateGraphicsPipeline({
        .shaderDesc = {
        .stages = {
            { .stage = SHADER_STAGE_VERT, .pSource = kGBufferShader,
              .sourceSize = (uint32_t)(sizeof(kGBufferShader) - 1), .pEntryPoint = "VSMain",
              .Name = "DeferredShadingGBufferVS" },
            { .stage = SHADER_STAGE_FRAG, .pSource = kGBufferShader,
              .sourceSize = (uint32_t)(sizeof(kGBufferShader) - 1), .pEntryPoint = "PSMain",
              .Name = "DeferredShadingGBufferPS" },
        },
    },
        .vertexLayout = {
            .bindings = { { .stride = sizeof(Vertex), .rate = VERTEX_BINDING_RATE_VERTEX } },
            .attribs = {
                { .semantic = SEMANTIC_POSITION, .Format = hz::Format::R32G32B32_SFLOAT, .binding = 0, .location = 0,
                  .offset = (uint32_t)offsetof(Vertex, position) },
                { .semantic = SEMANTIC_NORMAL, .Format = hz::Format::R32G32B32_SFLOAT, .binding = 0, .location = 1,
                  .offset = (uint32_t)offsetof(Vertex, normal) },
                { .semantic = SEMANTIC_COLOR, .Format = hz::Format::R32G32B32_SFLOAT, .binding = 0, .location = 2,
                  .offset = (uint32_t)offsetof(Vertex, color) },
            },
            .bindingCount = 1,
            .attribCount = 3,
        },
        .Depth = { .depthTest = true, .depthWrite = true, .depthFunc = CMP_LEQUAL },
        .colorTargets = { { .Format = hz::Format::R8G8B8A8_UNORM }, { .Format = hz::Format::R16G16B16A16_SFLOAT } },
        .depthStencilFormat = hz::Format::D32_SFLOAT,
        .Name = "DeferredShading.GBufferPipeline",
    });
    ASSERT(pipeline.IsValid());
}

bool GBufferPass::Resize(hz::RenderContext& context, uint32_t Width, uint32_t Height)
{
    hz::TextureDesc albedoDesc = {
        .Width = Width,
        .Height = Height,
        .Format = hz::Format::R8G8B8A8_UNORM,
        .StartState = RESOURCE_STATE_RENDER_TARGET,
        .Descriptors = DESCRIPTOR_TYPE_TEXTURE,
        .GetRenderTarget = true,
        .Name = "GBuffer.Albedo",
    };
    hz::TextureDesc normalDesc = albedoDesc;
    normalDesc.Format = hz::Format::R16G16B16A16_SFLOAT;
    normalDesc.Name = "GBuffer.Normal";
    hz::TextureDesc depthDesc = albedoDesc;
    depthDesc.Format = hz::Format::D32_SFLOAT;
    depthDesc.StartState = RESOURCE_STATE_DEPTH_WRITE;
    depthDesc.Name = "GBuffer.Depth";

    albedo = context.CreateTexture(albedoDesc);
    normal = context.CreateTexture(normalDesc);
    Depth = context.CreateTexture(depthDesc);
    if (!albedo.IsValid() || !normal.IsValid() || !Depth.IsValid())
    {
        return false;
    }
    return true;
}

void GBufferPass::execute(hz::CommandList& commands, const hz::GPUBuffer& vertices, const hz::GPUBuffer& indices,
                          const hz::GPUBuffer* sceneUniforms[kSceneObjectCount], uint32_t Width, uint32_t Height) const
{
    const ClearValue black = { .a = 1.0f };
    const ClearValue normalClear = { .r = 0.5f, .g = 0.5f, .b = 1.0f, .a = 1.0f };
    const ClearValue depthClear = { .Depth = 1.0f };
    commands.BeginGpuTimestamp("GBuffer");
    const hz::GPUBuffer* buffers[] = { &vertices, &indices };
    commands.BeginRendering(
        {
            .colorAttachments =  {
                { .pTexture = &albedo, .loadAction = LOAD_ACTION_CLEAR, .storeAction = STORE_ACTION_STORE, .ClearValue = black },
                { .pTexture = &normal, .loadAction = LOAD_ACTION_CLEAR, .storeAction = STORE_ACTION_STORE, .ClearValue = normalClear },
            } ,
            .depthAttachment = { .pTexture = &Depth,
                                 .loadAction = LOAD_ACTION_CLEAR,
                                 .storeAction = STORE_ACTION_STORE,
                                 .ClearValue = depthClear },
        },
        { .buffers = buffers });
    commands.SetViewport(0.0f, 0.0f, (float)Width, (float)Height);
    commands.SetScissor(0, 0, Width, Height);
    commands.SetPipeline(pipeline);
    commands.SetVertexBuffer(0, vertices, 0, sizeof(Vertex));
    commands.SetIndexBuffer(indices, 0, INDEX_TYPE_UINT32);
    commands.SetRootConstant({ sceneUniforms[0]->GetSrvIndex() });
    commands.DrawIndexed(kCubeIndexCount);
    commands.SetRootConstant({ sceneUniforms[1]->GetSrvIndex() });
    commands.DrawIndexed(kPlaneIndexCount, kPlaneFirstIndex, kPlaneFirstVertex);
    commands.EndRendering();
    commands.EndGpuTimestamp();
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
    float Depth = DepthTexture.Load(int3(pixel, 0));

    float3 lightDir = normalize(float3(0.45f, 0.85f, -0.25f));
    float ndotl = saturate(dot(normal, lightDir));
    float3 color = albedo.rgb * (0.16f + ndotl * 0.84f);
    color += saturate(1.0f - Depth) * 0.04f;
    float geometryMask = saturate(dot(albedo.rgb, float3(32.0f, 32.0f, 32.0f)));
    color = lerp(float3(0.02f, 0.025f, 0.03f), color, geometryMask);
    return float4(color, 1.0f);
}
)";

LightingPass::LightingPass(hz::RenderContext& context, hz::Format surfaceFormat)
{
    pipeline = context.CreateGraphicsPipeline({
        .shaderDesc = {
        .stages = {
            { .stage = SHADER_STAGE_VERT, .pSource = kLightingShader,
              .sourceSize = (uint32_t)(sizeof(kLightingShader) - 1), .pEntryPoint = "VSMain",
              .Name = "DeferredShadingLightingVS" },
            { .stage = SHADER_STAGE_FRAG, .pSource = kLightingShader,
              .sourceSize = (uint32_t)(sizeof(kLightingShader) - 1), .pEntryPoint = "PSMain",
              .Name = "DeferredShadingLightingPS" },
        },
    },
        .colorTargets = { { .Format = surfaceFormat } },
        .Name = "DeferredShading.LightingPipeline",
    });
    ASSERT(pipeline.IsValid());
}

void LightingPass::execute(hz::CommandList& commands, const hz::GPUTexture& backbuffer, const hz::GPUTexture& albedo,
                           const hz::GPUTexture& normal, const hz::GPUTexture& Depth, uint32_t Width, uint32_t Height) const
{
    const ClearValue Clear = { .r = 0.02f, .g = 0.025f, .b = 0.03f, .a = 1.0f };
    commands.BeginGpuTimestamp("Lighting");
    const hz::GPUTexture* sampledTextures[] = { &albedo, &normal, &Depth };
    commands.BeginRendering(
        {
            .colorAttachments = { {
                .pTexture = &backbuffer,
                .loadAction = LOAD_ACTION_CLEAR,
                .storeAction = STORE_ACTION_STORE,
                .ClearValue = Clear,
            } },
        },
        { .sampledTextures = sampledTextures });
    commands.SetViewport(0.0f, 0.0f, (float)Width, (float)Height);
    commands.SetScissor(0, 0, Width, Height);
    commands.SetPipeline(pipeline);
    commands.SetRootConstant({ { albedo.GetSrvIndex(), normal.GetSrvIndex(), Depth.GetSrvIndex() } });
    commands.Draw(3);
    commands.EndRendering();
    commands.EndGpuTimestamp();
}

constexpr uint32_t   kCpuProfileColor = 0x88CC44;
constexpr hz::Format kSurfaceFormat = hz::Format::B8G8R8A8_SRGB;

class DeferredShadingApp final: public IApp
{
public:
    DeferredShadingApp()
    {
        settings.Width = 1280;
        settings.Height = 720;
        settings.vSyncEnabled = true;
        settings.showPlatformUI = false;
    }

    bool Init() override
    {
        PROFILER_SET_CPU_SCOPE("DeferredShading", "Init", kCpuProfileColor);
        hz::ContextDesc contextDesc = {
            .pAppName = GetName(),
            .windowHandle = pWindow->Handle,
            .Width = 0,
            .Height = 0,
            .imageCount = 2,
            .colorFormat = kSurfaceFormat,
            .colorSpace = COLOR_SPACE_SDR_SRGB,
            .enableVSync = settings.vSyncEnabled,
            .enableGpuValidation = true,
            .enableGpuProfiler = true,
        };
        context = std::MakeUnique<hz::RenderContext>(contextDesc);
        geometryBuildPass = std::MakeUnique<GeometryBuildPass>(*context);
        gBufferPass = std::MakeUnique<GBufferPass>(*context);
        lightingPass = std::MakeUnique<LightingPass>(*context, hz::Format::R8G8B8A8_SRGB);

        createUniformBuffers();
        return true;
    }

    void Exit() override
    {
        PROFILER_SET_CPU_SCOPE("DeferredShading", "Exit", kCpuProfileColor);
        context->WaitIdle();
        lightingPass.Reset();
        gBufferPass.Reset();
        geometryBuildPass.Reset();
        for (hz::GPUBuffer& buffer : sceneUniformBuffers)
            buffer = {};
        context.Reset();
    }

    bool Load(ReloadDesc* pReloadDesc) override
    {
        PROFILER_SET_CPU_SCOPE("DeferredShading", "Load", kCpuProfileColor);
        if (!(pReloadDesc->Type & (RELOAD_TYPE_RESIZE | RELOAD_TYPE_RENDERTARGET)))
        {
            return true;
        }
        const uint32_t Width = (uint32_t)settings.Width;
        const uint32_t Height = (uint32_t)settings.Height;
        if (!context->Resize(Width, Height))
        {
            return false;
        }
        if (!gBufferPass->Resize(*context, Width, Height))
        {
            return false;
        }
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

        if (context->IsSuspended())
        {
            return;
        }
        hz::CommandList& commands = context->AcquireCommandList();
        updateSceneUniforms(commands);

        const hz::GPUTexture& backbuffer = context->GetCurrentBackbuffer();
        const hz::GPUBuffer*  sceneUniforms[kSceneObjectCount] = {
            &sceneUniformBuffers[0],
            &sceneUniformBuffers[1],
        };
        geometryBuildPass->execute(commands);
        gBufferPass->execute(commands, geometryBuildPass->GetVertexBuffer(), geometryBuildPass->GetIndexBuffer(), sceneUniforms,
                             context->GetWidth(), context->GetHeight());
        lightingPass->execute(commands, backbuffer, gBufferPass->getAlbedo(), gBufferPass->getNormal(), gBufferPass->getDepth(),
                              context->GetWidth(), context->GetHeight());
        context->Submit(commands, &backbuffer);
    }

    const char* GetName() override { return "DeferredShading"; }

private:
    void createUniformBuffers()
    {
        for (uint32_t object = 0; object < kSceneObjectCount; ++object)
        {
            sceneUniformBuffers[object] = context->CreateBuffer({
                .Size = sizeof(SceneUniforms),
                .ElementCount = 1,
                .StructStride = sizeof(SceneUniforms),
                .Name = object == 0 ? "DeferredShading.CubeUniforms" : "DeferredShading.FloorUniforms",
                .usage = RESOURCE_MEMORY_USAGE_GPU_ONLY,
                .StartState = RESOURCE_STATE_SHADER_RESOURCE,
                .Descriptors = DESCRIPTOR_TYPE_BUFFER,
            });
            ASSERT(sceneUniformBuffers[object].IsValid());
        }
    }

    void updateSceneUniforms(hz::CommandList& commands)
    {
        const float   aspectInverse = (float)settings.Height / (float)settings.Width;
        const Matrix4 view = Matrix4::lookAtLH(Point3(3.5f, 3.0f, -6.0f), Point3(0.0f, 0.7f, 0.0f), Vector3(0.0f, 1.0f, 0.0f));
        const float   verticalFov = 60.0f * 3.1415926535f / 180.0f;
        const float   horizontalFov = 2.0f * std::atan(std::tan(verticalFov * 0.5f) / aspectInverse);
        const Matrix4 projection = Matrix4::perspectiveLH(horizontalFov, aspectInverse, 0.1f, 100.0f);
        const Matrix4 viewProjection = projection * view;

        SceneUniforms cube = {};
        cube.world = Matrix4::translation(Vector3(0.0f, 1.15f, 0.0f)) * Matrix4::rotationY(elapsedTime) *
                     Matrix4::scale(Vector3(0.85f, 0.85f, 0.85f));
        cube.worldViewProjection = viewProjection * cube.world;
        commands.UpdateBuffer(sceneUniformBuffers[0], 0, &cube, sizeof(cube));

        SceneUniforms floor = {};
        floor.world = Matrix4::identity();
        floor.worldViewProjection = viewProjection * floor.world;
        commands.UpdateBuffer(sceneUniformBuffers[1], 0, &floor, sizeof(floor));
    }

    std::UniquePtr<hz::RenderContext>  context;
    hz::GPUBuffer                      sceneUniformBuffers[kSceneObjectCount];
    std::UniquePtr<GeometryBuildPass>  geometryBuildPass;
    std::UniquePtr<GBufferPass>        gBufferPass;
    std::UniquePtr<LightingPass>       lightingPass;
    float                              elapsedTime = 0.0f;
};

DEFINE_APPLICATION_MAIN(DeferredShadingApp)
